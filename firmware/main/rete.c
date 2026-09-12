/* ------------------------------------------------------------------------
 * Wi-Fi — attuazione su ESP-IDF.
 * --------------------------------------------------------------------- */
#include "rete.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_netif_sntp.h"
#include "mdns.h"

#include "config.h"
#include "i18n.h"
#include "segreti.h"

static const char *TAG = "rete";

static rete_stato_t   stato;
static uint32_t       tentativi;
static char           indirizzo[16];
static char           descrizione[64];
static TimerHandle_t  riprova;
static bool           acceso;

/* The NTP servers, copied. See copia_server_ntp() for why a copy. */
static char           ntp[CONFIG_LWIP_SNTP_MAX_SERVERS][64];
static int            ntp_quanti;
static bool           sntp_avviato;

/* The time is asked for at the first address, but not from the event
   handler: rete_gira() asks, from the graphics task, which is also the one
   that restarts it when the servers change. Two tasks on the same server
   list would be a race. */
static volatile bool     ora_da_avviare;
/* How many times an address was taken. The new network's trial watches it
   change: the only sure sign that the new credentials work. */
static volatile uint32_t indirizzi_presi;

/* The scan method last given to the radio — that is, whether the network
   is hidden. Remembered here because esp_wifi_get_config() does not return
   it as it was: on the P4 it goes through the co-processor and comes back
   different, and comparing it started the new network's trial at every
   save, even of an identical configuration (seen on the real panel). Name
   and password do come back exact. */
static wifi_scan_method_t metodo_in_uso;

/* --- l'attesa fra un tentativo e l'altro --------------------------------
 *
 * Raddoppia fino a un minuto e li si ferma. Il tetto non e prudenza: un
 * pannello al muro deve riattaccarsi da solo quando l'access point si riavvia
 * alle tre di notte, e un'attesa che cresce all'infinito vorrebbe dire
 * trovarlo scollegato la mattina dopo. Un minuto e abbastanza lungo da non
 * tempestare la radio e abbastanza corto da non far notare l'assenza. */
static uint32_t attesa_ms(void)
{
    uint32_t s = 2;
    for (uint32_t i = 1; i < tentativi && s < 60; i++) s *= 2;
    return (s > 60 ? 60 : s) * 1000;
}

static void aggiorna_descrizione(void)
{
    switch (stato) {
    case RETE_SPENTA:
        snprintf(descrizione, sizeof descrizione, "%s", tr(TX_SETTINGS_NO_NETWORK));
        break;
    case RETE_SENZA_CHIAVE:
        snprintf(descrizione, sizeof descrizione, "%s", tr(TX_NET_NO_PASSWORD));
        break;
    case RETE_IN_CORSO:
        snprintf(descrizione, sizeof descrizione, tr(TX_NET_IN_PROGRESS),
                 (unsigned long)tentativi);
        break;
    case RETE_CONNESSA:
        snprintf(descrizione, sizeof descrizione, tr(TX_NET_CONNECTED),
                 indirizzo, rete_potenza());
        break;
    case RETE_RIFIUTATA:
        snprintf(descrizione, sizeof descrizione, "%s", tr(TX_NET_REFUSED));
        break;
    case RETE_ASSENTE:
        snprintf(descrizione, sizeof descrizione, "%s", tr(TX_NET_NOT_FOUND));
        break;
    }
}

/* Definita piu sotto, accanto a mDNS: qui serve solo il nome, perche a
   chiamarla e il gestore degli eventi, che viene prima. */
static void avvia_ora(void);

static void su_riprova(TimerHandle_t t)
{
    (void)t;
    esp_wifi_connect();
}

static void programma_riprova(void)
{
    const uint32_t ms = attesa_ms();
    xTimerChangePeriod(riprova, pdMS_TO_TICKS(ms), 0);
    xTimerStart(riprova, 0);
}

/* --- eventi -------------------------------------------------------------
 *
 * Il registro dice **cosa** succede e non **a chi**: ne l'SSID ne la
 * password compaiono mai, che e la regola di 10-diagnostica.md. Il motivo
 * del rifiuto invece si stampa, perche e l'unica cosa che distingue una
 * password sbagliata da un access point spento — e senza quella distinzione
 * chi ripara prova a caso. */
/* Vero quando la radio e stata avviata **con** un nome e una chiave da
   provare. Falso quando e accesa solo per farsi guardare intorno dal primo
   avvio: li non c'e niente a cui connettersi, e chiederlo lo stesso
   produrrebbe un giro di cadute e ritentativi su una rete che non esiste. */
static bool con_credenziali;

static void su_evento(void *arg, esp_event_base_t base, int32_t id, void *dato)
{
    (void)arg;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (con_credenziali) esp_wifi_connect();
        return;
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = dato;
        indirizzo[0] = 0;
        tentativi++;

        switch (d->reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            stato = RETE_RIFIUTATA;
            break;
        case WIFI_REASON_NO_AP_FOUND:
            stato = RETE_ASSENTE;
            break;
        default:
            stato = RETE_IN_CORSO;
            break;
        }
        aggiorna_descrizione();

        /* Anche da rifiutata si riprova, con calma. Una password sbagliata
           non si corregge da sola, ma un access point che rifiuta perche sta
           ancora avviandosi si: distinguerli da qui e impossibile, e
           rinunciare per sempre e l'unico dei due errori che richiede una
           mano umana per uscirne. */
        ESP_LOGW(TAG, "caduta - motivo %d - tentativo %lu - riprovo fra %lu s",
                 d->reason, (unsigned long)tentativi,
                 (unsigned long)(attesa_ms() / 1000));
        programma_riprova();
        return;
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = dato;
        snprintf(indirizzo, sizeof indirizzo, IPSTR, IP2STR(&e->ip_info.ip));
        stato = RETE_CONNESSA;
        tentativi = 0;
        aggiorna_descrizione();
        ESP_LOGI(TAG, "connessa - %s", indirizzo);

        indirizzi_presi++;

        /* L'ora si chiede appena si ha un indirizzo, e una volta sola: se il
           collegamento cade e torna, il servizio e ancora li e riprende da
           solo. rete_gira() asks for it: see ora_da_avviare. */
        if (!sntp_avviato) ora_da_avviare = true;
    }
}

/* --- la password, per il tempo di una chiamata --------------------------- */

static void metti_chiave(const char *valore, void *dato)
{
    wifi_config_t *c = dato;
    strlcpy((char *)c->sta.password, valore, sizeof c->sta.password);
}

/* The configuration's credentials, without touching the state. False if
   the name or the password is missing, and `manca` says which. */
static bool leggi_credenziali(wifi_config_t *c, rete_stato_t *manca)
{
    memset(c, 0, sizeof *c);

    const char *ssid = cfg_testo("system/network/ssid", "");
    if (!ssid[0]) { *manca = RETE_SPENTA; return false; }
    strlcpy((char *)c->sta.ssid, ssid, sizeof c->sta.ssid);

    if (!segreti_usa(SEG_WIFI_PASSWORD, metti_chiave, c)) {
        *manca = RETE_SENZA_CHIAVE;
        return false;
    }

    /* Una rete nascosta non risponde alla ricerca veloce: bisogna guardare
       tutti i canali. Costa qualche secondo in piu al primo tentativo, e
       solo a chi ha davvero una rete nascosta. */
    if (cfg_vero("system/network/hidden", false))
        c->sta.scan_method = WIFI_ALL_CHANNEL_SCAN;

    return true;
}

/* Falso se manca il nome o la password: sono due assenze diverse e lo stato
   le distingue, perche a chi guarda lo schermo servono due risposte diverse. */
static bool prepara(wifi_config_t *c)
{
    rete_stato_t manca = RETE_SPENTA;
    if (leggi_credenziali(c, &manca)) return true;
    stato = manca;
    return false;
}

/* --- avvio --------------------------------------------------------------- */

static void avvia_mdns(void)
{
    if (mdns_init() != ESP_OK) {
        ESP_LOGW(TAG, "mDNS non avviato");
        return;
    }
    /* Un nome solo per tutti i pannelli sarebbe un conflitto appena ne
       appendi il secondo. Quello di 11-collaudo.md resta il nome
       predefinito, e chi ha due pannelli cambia nome_pannello.

       La traduzione da nome leggibile a nome host sta in cfg_nome_host(),
       perche' non la usa solo mDNS: la stessa stringa finisce sul vetro
       nelle impostazioni e al primo avvio, e per un po' quelle due
       schermate hanno scritto `pannello.local` a prescindere. */
    const char *nome = cfg_testo("system/panel_name", "pannello");
    char host[32];
    cfg_nome_host(host, sizeof host);

    mdns_hostname_set(host);
    mdns_instance_name_set(nome);
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS - %s.local", host);
}

/* --- l'ora dalla rete ---------------------------------------------------
 *
 * Il pannello all'accensione crede di essere nel 1970 e ci resta finche
 * qualcuno non lo corregge: non ha una batteria per l'orologio, ed e una
 * scelta ragionevole su un apparecchio sempre attaccato alla corrente e
 * sempre in rete.
 *
 * I server vengono da `sistema/ntp` — al massimo tre, dice il contratto — e
 * se non ce ne sono si usa il gruppo pubblico italiano. Un pannello che non
 * sa l'ora perche nessuno gli ha scritto un indirizzo sarebbe un modo
 * pedante di essere inutili.
 *
 * Il fuso si applica **prima** di chiedere l'ora, non dopo: cosi il primo
 * aggiornamento e gia quello giusto, e nessuno vede l'orologio saltare di
 * un'ora due secondi dopo essersi acceso. app_main() does it, as soon as
 * the configuration is read.
 *
 * The servers are copied when the radio starts, from the task that read
 * the configuration, and not when the address arrives.
 *
 * --- why a copy -----------------------------------------------------------
 *
 * sntp_setservername() **keeps the pointer**, not the text: lwIP reads it
 * again at every synchronisation, once an hour. It was given the text
 * inside the configuration document, with a comment saying "safe as long
 * as nobody reloads the configuration" — but the web page reloads it at
 * every save: cfg_sostituisci() throws the old document away, and its
 * texts with it. From the first save on, SNTP looked for a server whose
 * name was in memory already given back.
 *
 * --- why now ----------------------------------------------------------------
 *
 * avvia_ora() is called by the radio's event handler, which is a task of
 * its own: reading the configuration from there meant reading it while the
 * graphics task could be replacing it. */
static int leggi_server_ntp(char dove[][64])
{
    int quanti = cfg_quanti("system/ntp");
    if (quanti > CONFIG_LWIP_SNTP_MAX_SERVERS)
        quanti = CONFIG_LWIP_SNTP_MAX_SERVERS;

    memset(dove, 0, sizeof ntp);
    int messi = 0;
    for (int n = 0; n < quanti; n++) {
        char percorso[32];
        snprintf(percorso, sizeof percorso, "system/ntp/%d", n);
        const char *s = cfg_testo(percorso, NULL);
        if (s && *s) strlcpy(dove[messi++], s, 64);
    }
    return messi;
}

static void copia_server_ntp(void) { ntp_quanti = leggi_server_ntp(ntp); }

static void avvia_ora(void)
{
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("it.pool.ntp.org");

    const int messi = ntp_quanti;
    for (int n = 0; n < messi; n++) cfg.servers[n] = ntp[n];
    if (messi > 0) cfg.num_of_servers = (size_t)messi;

    if (esp_netif_sntp_init(&cfg) != ESP_OK) {
        ESP_LOGW(TAG, "NTP non avviato: l'ora restera quella dell'accensione");
        return;
    }
    sntp_avviato = true;
    /* Distinguere i due casi: "un server" e "nessuno in configurazione,
       uso il pubblico" si somigliano nel numero e non nel significato. */
    if (messi > 0) ESP_LOGI(TAG, "NTP avviato su %d server da configurazione", messi);
    else           ESP_LOGW(TAG, "nessun server NTP in configurazione: uso it.pool.ntp.org");
}

bool rete_radio_accesa(void) { return acceso; }

bool rete_avvia(void)
{
    if (acceso) return true;

    if (esp_netif_init() != ESP_OK) return false;
    if (esp_event_loop_create_default() != ESP_OK) return false;
    esp_netif_create_default_wifi_sta();

    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK) {
        ESP_LOGE(TAG, "radio non avviata");
        return false;
    }
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        su_evento, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        su_evento, NULL, NULL);

    /* La configurazione la tiene NVS, che gia c'e: senza questo, ogni avvio
       riscrive la stessa cosa in flash per niente. */
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);

    riprova = xTimerCreate("rete", pdMS_TO_TICKS(2000), pdFALSE, NULL,
                           su_riprova);
    if (!riprova) return false;

    acceso = true;
    copia_server_ntp();
    avvia_mdns();
    rete_riprova();
    return true;
}

void rete_riprova(void)
{
    if (!acceso) return;

    xTimerStop(riprova, 0);
    esp_wifi_disconnect();
    esp_wifi_stop();

    wifi_config_t c;
    if (!prepara(&c)) {
        aggiorna_descrizione();
        ESP_LOGW(TAG, "%s", descrizione);
        /* La copia in chiaro non deve sopravvivere nemmeno al fallimento. */
        memset(&c, 0, sizeof c);

        /* --- e pero la radio si accende lo stesso -----------------------
         *
         * Qui prima si tornava indietro, e sembrava la cosa ragionevole:
         * senza nome e senza chiave non c'e niente a cui connettersi, e
         * accendere una radio per non usarla e spreco.
         *
         * Era un ragionamento circolare, e teneva fermo il pannello proprio
         * dove serviva muoversi. Il primo avvio chiede **quale rete**, e per
         * chiederlo deve guardarsi intorno; guardarsi intorno e
         * esp_wifi_scan_start(), che vuole la radio gia avviata. Senza
         * questa riga la schermata mostrava un elenco vuoto per sempre, e
         * l'unico modo di dire una rete al pannello era la pagina web — che
         * arriva via Wi-Fi. Nessuna strada.
         *
         * Accesa e non connessa: esp_wifi_connect() non si chiama, quindi
         * non c'e nessun tentativo con credenziali che non abbiamo, e lo
         * stato resta quello che descrive la mancanza. */
        con_credenziali = false;
        esp_wifi_start();
        return;
    }

    con_credenziali = true;
    tentativi = 1;
    stato = RETE_IN_CORSO;
    aggiorna_descrizione();

    esp_wifi_set_config(WIFI_IF_STA, &c);
    metodo_in_uso = c.sta.scan_method;
    /* Cancellata subito: da qui in poi la password sta solo dentro il
       driver, dove serve, e non piu su questa pila. */
    memset(&c, 0, sizeof c);

    esp_wifi_start();
    ESP_LOGI(TAG, "provo a connettermi");
}

rete_stato_t rete_stato(void)      { return stato; }
uint32_t     rete_tentativi(void)  { return tentativi; }
const char  *rete_indirizzo(void)  { return indirizzo; }

int8_t rete_potenza(void)
{
    if (stato != RETE_CONNESSA) return 0;
    wifi_ap_record_t ap;
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
}

const char *rete_descrizione(void)
{
    /* La potenza cambia di continuo: se e connessa la riga si rifa adesso. */
    if (stato == RETE_CONNESSA) aggiorna_descrizione();
    return descrizione;
}

/* --- the NTP servers, while running ---------------------------------------
 *
 * They were read once, when the radio started, and changing them from the
 * page did nothing until a restart. Nothing is at risk here: if the list
 * changed, the service is stopped and started again with the new one. If
 * it had not started yet — no address, yet — it will start by itself with
 * the right list. */
void rete_ntp_rileggi(void)
{
    /* The new list in a separate copy: the real one is being read by SNTP —
       it keeps the pointers — and is touched only with the service stopped. */
    char nuovi[CONFIG_LWIP_SNTP_MAX_SERVERS][64];
    const int quanti = leggi_server_ntp(nuovi);
    if (quanti == ntp_quanti && memcmp(nuovi, ntp, sizeof ntp) == 0) return;

    const bool era_avviato = sntp_avviato;
    if (era_avviato) {
        esp_netif_sntp_deinit();
        sntp_avviato = false;
    }
    memcpy(ntp, nuovi, sizeof ntp);
    ntp_quanti = quanti;
    if (era_avviato) avvia_ora();
}

/* --- the new network, tried with a safety net -------------------------------
 *
 * Wi-Fi name and password applied only at the restart. Applying them at
 * once has a risk the other fields do not have: a wrong password takes the
 * panel off the network, and a wall panel with no network is reached only
 * with the serial cable.
 *
 * So the new network is **tried**, and the credentials that worked stay in
 * memory until an address has been seen arriving with the new ones. If it
 * does not arrive within a minute, the old ones come back — in the radio
 * **and** in the store, so that the next restart does not fall into the
 * same hole — and the log and the page say so.
 *
 * The trial does not start at once: name and password come from the page
 * in two different requests, with the time to type them in between. It
 * starts thirty seconds after the last change, and every change restarts
 * the count. It also starts after the answer to the save has left, which
 * switching the radio at that same instant would not guarantee.
 *
 * All from the graphics task, which owns configuration and secrets:
 * rete_ricontrolla() after a save, rete_gira() at every loop. */
typedef enum { PROVA_NIENTE, PROVA_IN_ATTESA, PROVA_IN_CORSO } prova_t;

#define PROVA_QUIETE_MS  30000u
#define PROVA_MAX_MS     60000u

static prova_t       prova;
static uint32_t      prova_da_ms;
static uint32_t      adesso_rete;
static uint32_t      indirizzi_al_via;
static wifi_config_t paracadute;       /* the credentials that worked      */
static bool          ricontrolla_dopo; /* a change during the trial        */
static bool          tornata;          /* the last trial did not work      */

static bool stesse_credenziali(const wifi_config_t *a, const wifi_config_t *b)
{
    return strncmp((const char *)a->sta.ssid, (const char *)b->sta.ssid,
                   sizeof a->sta.ssid) == 0 &&
           strncmp((const char *)a->sta.password, (const char *)b->sta.password,
                   sizeof a->sta.password) == 0 &&
           a->sta.scan_method == b->sta.scan_method;
}

/* Puts `c` in the radio and reconnects. The password stays only in the
   driver: the caller erases its copy. */
static void metti_in_radio(const wifi_config_t *c)
{
    xTimerStop(riprova, 0);
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, (wifi_config_t *)c);
    metodo_in_uso = c->sta.scan_method;
    tentativi = 1;
    stato = RETE_IN_CORSO;
    aggiorna_descrizione();
    esp_wifi_connect();
}

bool rete_in_prova(void) { return prova != PROVA_NIENTE; }

void rete_ricontrolla(bool subito)
{
    if (!acceso) return;
    if (prova == PROVA_IN_CORSO) { ricontrolla_dopo = true; return; }

    wifi_config_t voluta;
    rete_stato_t manca;
    if (!leggi_credenziali(&voluta, &manca)) {
        /* Name or password removed: the working connection is not touched,
           and a waiting trial has nothing left to try. */
        memset(&voluta, 0, sizeof voluta);
        prova = PROVA_NIENTE;
        memset(&paracadute, 0, sizeof paracadute);
        return;
    }

    wifi_config_t attuale;
    /* Without the credentials in use there is no safety net: better apply
       at the restart, as before, than try with nothing to fall back on. */
    if (esp_wifi_get_config(WIFI_IF_STA, &attuale) != ESP_OK ||
        !attuale.sta.ssid[0]) {
        memset(&attuale, 0, sizeof attuale);
        memset(&voluta, 0, sizeof voluta);
        ESP_LOGW(TAG, "credentials in use not readable: the new network "
                      "applies at the restart");
        return;
    }
    /* The scan method as this file gave it, not as the radio returns it:
       see metodo_in_uso. */
    attuale.sta.scan_method = metodo_in_uso;

    const bool uguali = stesse_credenziali(&voluta, &attuale);
    if (!uguali) {
        /* Which field, never the value: the way to see from the log whether
           the trial started for a real change. */
        ESP_LOGI(TAG, "Wi-Fi credentials changed:%s%s%s",
                 strncmp((const char *)voluta.sta.ssid,
                         (const char *)attuale.sta.ssid,
                         sizeof voluta.sta.ssid) ? " name" : "",
                 strncmp((const char *)voluta.sta.password,
                         (const char *)attuale.sta.password,
                         sizeof voluta.sta.password) ? " password" : "",
                 voluta.sta.scan_method != attuale.sta.scan_method
                     ? " hidden network" : "");
    }
    memset(&voluta, 0, sizeof voluta);
    if (uguali) {
        /* Nothing new — or back to the old ones before the trial started. */
        memset(&attuale, 0, sizeof attuale);
        if (prova == PROVA_IN_ATTESA) {
            prova = PROVA_NIENTE;
            memset(&paracadute, 0, sizeof paracadute);
        }
        return;
    }

    /* Without a working network there is nothing to go back to: the new
       one is simply tried, as the console does. */
    if (stato != RETE_CONNESSA && prova == PROVA_NIENTE) {
        memset(&attuale, 0, sizeof attuale);
        rete_riprova();
        return;
    }

    if (prova == PROVA_NIENTE) paracadute = attuale;
    memset(&attuale, 0, sizeof attuale);
    prova = PROVA_IN_ATTESA;
    prova_da_ms = subito ? adesso_rete - PROVA_QUIETE_MS : adesso_rete;
    ESP_LOGI(TAG, "new Wi-Fi network in the configuration: trying it in %u s",
             (unsigned)(PROVA_QUIETE_MS / 1000));
}

/* The trial did not work: back to the old credentials, in the radio and in
   the store. The network name never reaches the log (10-diagnostica.md §1). */
static void torna_indietro(void)
{
    char ssid[sizeof paracadute.sta.ssid + 1];
    char chiave[sizeof paracadute.sta.password + 1];
    memcpy(ssid, paracadute.sta.ssid, sizeof paracadute.sta.ssid);
    ssid[sizeof ssid - 1] = 0;
    memcpy(chiave, paracadute.sta.password, sizeof paracadute.sta.password);
    chiave[sizeof chiave - 1] = 0;
    const bool nascosta = paracadute.sta.scan_method == WIFI_ALL_CHANNEL_SCAN;

    metti_in_radio(&paracadute);

    cfg_imposta_testo("system/network/ssid", ssid);
    cfg_imposta_vero("system/network/hidden", nascosta);
    cfg_salva();
    segreti_scrivi(SEG_WIFI_PASSWORD, chiave);

    memset(chiave, 0, sizeof chiave);
    memset(ssid, 0, sizeof ssid);
    memset(&paracadute, 0, sizeof paracadute);

    prova = PROVA_NIENTE;
    ricontrolla_dopo = false;
    tornata = true;
    ESP_LOGW(TAG, "the new Wi-Fi network did not answer within %u s: back to "
                  "the previous one, name and password included",
             (unsigned)(PROVA_MAX_MS / 1000));
}

bool rete_gira(uint32_t adesso)
{
    adesso_rete = adesso;

    if (ora_da_avviare) {
        ora_da_avviare = false;
        if (!sntp_avviato) avvia_ora();
    }

    switch (prova) {
    case PROVA_NIENTE:
        return false;

    case PROVA_IN_ATTESA: {
        if (adesso - prova_da_ms < PROVA_QUIETE_MS) return false;

        wifi_config_t nuova;
        rete_stato_t manca;
        if (!leggi_credenziali(&nuova, &manca)) {
            memset(&nuova, 0, sizeof nuova);
            memset(&paracadute, 0, sizeof paracadute);
            prova = PROVA_NIENTE;
            return false;
        }
        ESP_LOGI(TAG, "trying the new Wi-Fi network: if it does not answer "
                      "within %u s, back to the previous one",
                 (unsigned)(PROVA_MAX_MS / 1000));
        indirizzi_al_via = indirizzi_presi;
        metti_in_radio(&nuova);
        memset(&nuova, 0, sizeof nuova);
        prova = PROVA_IN_CORSO;
        prova_da_ms = adesso;
        return false;
    }

    case PROVA_IN_CORSO:
        if (indirizzi_presi != indirizzi_al_via) {
            ESP_LOGI(TAG, "the new Wi-Fi network works");
            memset(&paracadute, 0, sizeof paracadute);
            prova = PROVA_NIENTE;
            tornata = false;
            if (ricontrolla_dopo) {
                ricontrolla_dopo = false;
                rete_ricontrolla(false);
            }
            return false;
        }
        if (adesso - prova_da_ms < PROVA_MAX_MS) return false;
        torna_indietro();
        return true;    /* the configuration changed: read it again */
    }
    return false;
}

const char *rete_prova_stato(uint32_t *fra_s)
{
    if (fra_s) *fra_s = 0;
    switch (prova) {
    case PROVA_IN_ATTESA: {
        const uint32_t passati = adesso_rete - prova_da_ms;
        if (fra_s && passati < PROVA_QUIETE_MS)
            *fra_s = (PROVA_QUIETE_MS - passati + 999) / 1000;
        return "waiting";
    }
    case PROVA_IN_CORSO:
        return "trying";
    default:
        return tornata ? "reverted" : "";
    }
}
