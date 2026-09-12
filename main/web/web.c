/* ------------------------------------------------------------------------
 * Server di configurazione — smistamento e regole di accesso.
 *
 * Un gestore solo, non uno per percorso: le regole di accesso sono le
 * stesse per tutti e conviene che stiano scritte in un posto, dove si
 * leggono tutte insieme, invece che ripetute in ogni gestore — dove prima o
 * poi una si dimentica.
 * --------------------------------------------------------------------- */
#include "web.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "aggiornamento.h"
#include "config.h"
#include "registro.h"
#include "ripristino.h"
#include "ha.h"
#include "dati.h"
#include "entita.h"
#include "pagina.h"
#include "profile.h"
#include "segreti.h"
#include "sistema.h"
#include "translations.h"
#include "versione.h"

static void (*su_cambio)(void);
static void (*on_texts)(void);

void web_quando_cambia(void (*cb)(void)) { su_cambio = cb; }
void web_when_texts_change(void (*cb)(void)) { on_texts = cb; }

static void texts_changed(void)
{
    if (on_texts) on_texts();
    else if (su_cambio) su_cambio();
}

/* --- tempo e sblocco ---------------------------------------------------- */

static uint32_t adesso_ms;
static uint32_t sblocco_fino_ms;
static bool     sbloccato;

/* --- la porta tenuta aperta per il montaggio ----------------------------
 *
 * Il blocco dei quindici minuti e giusto su un pannello finito e d'intralcio
 * su uno che si sta ancora montando: si tocca il vetro, si salva, si tocca
 * di nuovo, dieci volte in un pomeriggio.
 *
 * Sta in configurazione e non e una costante, cosi rimetterlo e una spunta.
 * Il valore di riposo e **chiuso**: un firmware che nasce spalancato e
 * esattamente la cosa che si dimentica accesa.
 *
 * Si legge a ogni giro invece di essere presa una volta all'avvio, cosi
 * spegnerla dalla pagina chiude la porta subito — compresa quella da cui e
 * arrivato il comando, che e il comportamento giusto. */
bool web_sempre_aperta(void)
{
    return cfg_vero("diagnostics/config_always_open", false);
}

void web_tempo(uint32_t ms)
{
    adesso_ms = ms;
    /* La finestra si chiude da sola. Non c'e un modo per rinnovarla senza
       tornare davanti al pannello, ed e il punto — salvo quando qualcuno ha
       chiesto esplicitamente di tenerla aperta. */
    if (web_sempre_aperta()) return;
    if (sbloccato && (int32_t)(adesso_ms - sblocco_fino_ms) >= 0)
        web_richiudi();
}

void web_sblocca(void)
{
    sbloccato = true;
    sblocco_fino_ms = adesso_ms + WEB_SBLOCCO_MS;
}

void web_richiudi(void) { sbloccato = false; sblocco_fino_ms = 0; }

bool web_sbloccato(void) { return sbloccato || web_sempre_aperta(); }

uint32_t web_sblocco_resta_ms(void)
{
    /* Zero vuol dire «non c'e un conto alla rovescia», e con la porta tenuta
       aperta non ce n'e: chi legge questo numero deve chiedere prima
       web_sempre_aperta(), se no mostra «si richiude fra 0:00» a una porta
       che non si richiude. */
    if (web_sempre_aperta()) return 0;
    if (!sbloccato) return 0;
    const int32_t resta = (int32_t)(sblocco_fino_ms - adesso_ms);
    return resta > 0 ? (uint32_t)resta : 0;
}

/* --- limite di frequenza ------------------------------------------------
 *
 * Una richiesta al secondo per chiamante sui due percorsi sempre attivi.
 * Non e una difesa da un attacco — chi vuole insistere insiste — ma da un
 * incidente: un'integrazione che interroga in ciclo stretto scalderebbe un
 * apparecchio passivamente raffreddato dietro una placca.
 *
 * Sedici chiamanti bastano: e una rete di casa. Il diciassettesimo prende
 * il posto del piu vecchio, che e meglio di rifiutarlo.
 */
#define CHIAMANTI_MAX 16

static struct {
    char     chi[46];          /* ci sta anche un IPv6 testuale */
    uint32_t ultimo_ms;
    bool     usato;
} chiamanti[CHIAMANTI_MAX];

static bool troppo_spesso(const char *chi)
{
    if (!chi || !*chi) return false;

    int libero = -1, piu_vecchio = 0;
    for (int n = 0; n < CHIAMANTI_MAX; n++) {
        if (!chiamanti[n].usato) { if (libero < 0) libero = n; continue; }
        if (strcmp(chiamanti[n].chi, chi) == 0) {
            const uint32_t da = adesso_ms - chiamanti[n].ultimo_ms;
            if (da < WEB_INTERVALLO_MIN_MS) return true;
            chiamanti[n].ultimo_ms = adesso_ms;
            return false;
        }
        if (chiamanti[n].ultimo_ms < chiamanti[piu_vecchio].ultimo_ms)
            piu_vecchio = n;
    }

    const int dove = libero >= 0 ? libero : piu_vecchio;
    snprintf(chiamanti[dove].chi, sizeof chiamanti[dove].chi, "%s", chi);
    chiamanti[dove].ultimo_ms = adesso_ms;
    chiamanti[dove].usato = true;
    return false;
}

/* --- risposte ----------------------------------------------------------- */

static void testo(risposta_t *s, int codice, const char *tipo, const char *c)
{
    s->codice = codice;
    s->tipo = tipo;
    s->corpo = c;
    s->corpo_n = 0;
    s->libera = NULL;
}

/* Il corpo lo ha allocato cJSON: si dice **come** liberarlo, cosi il
   trasporto non deve sapere che qui dentro c'e cJSON. */
static void libera_cjson(void *c) { cJSON_free(c); }

static void json_proprio(risposta_t *s, int codice, char *c)
{
    if (!c) { testo(s, 500, "application/json", "{\"error\":\"memory\"}"); return; }
    s->codice = codice;
    s->tipo = "application/json";
    s->corpo = c;
    s->corpo_n = 0;
    s->libera = libera_cjson;
}

/* Quello che si risponde a chi non deve sapere che qui c'e qualcosa. Un
   403 direbbe "c'e, ma non puoi"; un 404 non dice niente. */
static void non_trovato(risposta_t *s)
{
    testo(s, 404, "text/plain", "Not Found");
}

/* --- percorsi sempre attivi --------------------------------------------- */

static void api_status(risposta_t *s)
{
    const contatori_t c = dati_contatori();

    cJSON *o = cJSON_CreateObject();
    if (!o) { json_proprio(s, 500, NULL); return; }

    /* Niente SSID: 10-diagnostica.md §1 lo mette accanto a token e password
       fra le cose che non escono dal pannello, e questo percorso risponde a
       chiunque sia sulla rete. Il livello del segnale si', che non dice a
       quale rete si e attaccati. */
    cJSON_AddStringToObject(o, "panel", cfg_testo("system/panel_name", "panel"));
    cJSON_AddStringToObject(o, "profile", PRF->chiave);
    cJSON_AddStringToObject(o, "firmware", PANNELLO_VERSIONE);
    /* La revisione a parte: e la sola cosa con cui si ritrova il codice
       esatto che sta girando su quel muro, e un numero di versione da solo
       non basta a trovarlo. */
    cJSON_AddStringToObject(o, "revision", PANNELLO_REVISIONE);
    /* Quando e stata compilata: la revisione dice **quale** codice,
       questa dice **quando** e finito su questo pannello. */
    cJSON_AddStringToObject(o, "built", PANNELLO_COMPILATO);
    /* Cosi la pagina puo dirlo a chi la guarda: una porta lasciata
       aperta si dimentica, e questo e il posto da cui si ricorda. */
    cJSON_AddBoolToObject(o, "config_always_open",
                          web_sempre_aperta());
    cJSON_AddNumberToObject(o, "uptime_s", c.accensione_s);
    cJSON_AddNumberToObject(o, "restarts", c.riavvii);
    cJSON_AddStringToObject(o, "last_restart_reason",
                            c.motivo_ultimo_riavvio ? c.motivo_ultimo_riavvio : "");
    cJSON_AddNumberToObject(o, "heap_free", c.heap_libero);
    cJSON_AddNumberToObject(o, "heap_min", c.heap_minimo);
    cJSON_AddNumberToObject(o, "psram_free", c.psram_libera);
    cJSON_AddNumberToObject(o, "fps", c.fps);
    cJSON_AddNumberToObject(o, "wifi_rssi", c.wifi_rssi);
    cJSON_AddStringToObject(o, "ha_state", c.ha_stato ? c.ha_stato : "");
    cJSON_AddNumberToObject(o, "ha_last_data_s", c.ha_ultimo_dato_s);
    cJSON_AddNumberToObject(o, "commands_sent", c.comandi_inviati);
    cJSON_AddNumberToObject(o, "commands_failed", c.comandi_falliti);
    cJSON_AddNumberToObject(o, "recent_errors", c.errori_recenti);
    /* The trial of a new Wi-Fi network. The state only, never the network's
       name: this path answers anyone on the network. */
    uint32_t fra_s = 0;
    cJSON_AddStringToObject(o, "network_trial", sistema_rete_prova(&fra_s));
    cJSON_AddNumberToObject(o, "network_trial_in_s", fra_s);

    char *testo_json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    json_proprio(s, 200, testo_json);
}

static const char *NOMI_LIVELLO[] = {
    [LOG_ERRORE] = "error", [LOG_AVVISO] = "warning",
    [LOG_INFO] = "info",     [LOG_DEBUG] = "debug",
};

/* Il valore di un parametro nella query, dentro `fuori`. Falso se non c'e. */
static bool parametro(const char *query, const char *nome, char *fuori,
                      size_t max)
{
    if (!query) return false;
    const size_t l = strlen(nome);
    for (const char *p = query; p && *p; ) {
        if (strncmp(p, nome, l) == 0 && p[l] == '=') {
            p += l + 1;
            size_t n = 0;
            while (p[n] && p[n] != '&' && n < max - 1) n++;
            memcpy(fuori, p, n);
            fuori[n] = 0;
            return true;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return false;
}

static void api_log(const richiesta_t *r, risposta_t *s)
{
    char v[16];
    int soglia = LOG_DEBUG;      /* tutto, se non si chiede altro */
    if (parametro(r->query, "level", v, sizeof v))
        for (int n = 0; n < 4; n++)
            if (strcmp(v, NOMI_LIVELLO[n]) == 0) soglia = n;

    int quante = dati_log();
    if (parametro(r->query, "n", v, sizeof v)) {
        const int chieste = atoi(v);
        if (chieste > 0 && chieste < quante) quante = chieste;
    }

    cJSON *o = cJSON_CreateObject();
    cJSON *righe = o ? cJSON_AddArrayToObject(o, "lines") : NULL;
    if (!righe) { cJSON_Delete(o); json_proprio(s, 500, NULL); return; }

    /* Dalla piu recente all'indietro: chi guarda un registro cerca cosa e
       successo poco fa, non com'e cominciata la giornata. */
    int messe = 0;
    for (int n = dati_log() - 1; n >= 0 && messe < quante; n--) {
        const riga_log_t *l = dati_log_riga(n);
        if (!l || (int)l->livello > soglia) continue;
        cJSON *e = cJSON_CreateObject();
        if (!e) break;
        cJSON_AddStringToObject(e, "time", l->ora ? l->ora : "");
        cJSON_AddStringToObject(e, "level", NOMI_LIVELLO[l->livello]);
        cJSON_AddStringToObject(e, "source", l->sorgente ? l->sorgente : "");
        cJSON_AddStringToObject(e, "message", l->messaggio ? l->messaggio : "");
        cJSON_AddItemToArray(righe, e);
        messe++;
    }

    char *testo_json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    json_proprio(s, 200, testo_json);
}

/* --- percorsi che vogliono lo sblocco ----------------------------------- */

static void api_config_get(risposta_t *s)
{
    json_proprio(s, 200, cfg_esporta(true));
}

static void api_config_post(const richiesta_t *r, risposta_t *s)
{
    /* §2.1: la pagina manda il documento **intero**, non le differenze. Se
       fosse una differenza servirebbe fondere, e fondere vuol dire avere
       due idee di cosa sia il documento corrente. */
    if (cfg_sostituisci(r->corpo, r->corpo_n) && cfg_salva()) {
        /* §2: nomi, zone, entita e ordine delle sezioni si applicano
           subito. Chiedere un riavvio per rinominare una luce sarebbe
           dieci secondi di schermo nero per una parola. */
        if (su_cambio) su_cambio();
        testo(s, 200, "application/json", "{\"result\":\"saved\"}");
        return;
    }

    /* §2.2: quali campi, e il file non e stato toccato. Un "422" e basta
       obbligherebbe a indovinare. */
    cJSON *o = cJSON_CreateObject();
    cJSON *campi = o ? cJSON_AddArrayToObject(o, "fields") : NULL;
    if (!campi) { cJSON_Delete(o); json_proprio(s, 500, NULL); return; }

    /* Errori e avvisi in due elenchi distinti, e non e pedanteria: un
       avviso — "campo ignorato su questo profilo, ma conservato" — non e un
       motivo di rifiuto, e mescolarlo agli errori farebbe segnare in rosso
       nella pagina un campo che va benissimo. */
    cJSON *avvisi = cJSON_AddArrayToObject(o, "warnings");
    for (int n = 0; n < cfg_errori(); n++) {
        const errore_cfg_t *e = cfg_errore(n);
        cJSON *v = cJSON_CreateObject();
        if (!v) break;
        cJSON_AddStringToObject(v, "field", e->campo);
        cJSON_AddStringToObject(v, "reason", e->motivo);
        cJSON_AddItemToArray(e->gravita == CFG_ERRORE ? campi : avvisi, v);
    }
    cJSON_AddBoolToObject(o, "file_changed", false);

    char *testo_json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    json_proprio(s, 422, testo_json);
}

static void api_secrets_post(const richiesta_t *r, risposta_t *s)
{
    cJSON *o = cJSON_Parse(r->corpo);
    if (!cJSON_IsObject(o)) {
        cJSON_Delete(o);
        testo(s, 400, "application/json", "{\"error\":\"json\"}");
        return;
    }

    cJSON *fuori = cJSON_CreateObject();
    if (!fuori) { cJSON_Delete(o); json_proprio(s, 500, NULL); return; }

    bool scritto = false;
    for (cJSON *c = o->child; c; c = c->next) {
        const segreto_t seg = segreto_da_nome(c->string);
        /* Nome fuori dall'elenco: si dice che non e stato scritto e si va
           avanti. Un nome arbitrario dalla rete non diventa una chiave. */
        if (seg == SEG_QUANTI || !cJSON_IsString(c)) {
            cJSON_AddFalseToObject(fuori, c->string ? c->string : "?");
            continue;
        }
        segreti_scrivi(seg, c->valuestring);
        scritto = true;
        /* Si risponde se **c'e**, non cosa c'e: e tutto quello che la
           pagina web puo sapere di un segreto, anche di uno appena
           scritto da lei. */
        cJSON_AddBoolToObject(fuori, c->string, segreti_impostato(seg));
    }
    cJSON_Delete(o);

    /* A new secret is to be used, not only kept. The page said "secret
       updated" and the connection to Home Assistant went on with the old
       token — or stayed on "token refused", which by design does not clear
       by itself — until someone saved the configuration for another
       reason. The same gesture as a save: it is noted, and the main loop
       retries Home Assistant and redraws (the shared networks' passwords
       end up in the QR codes). */
    if (scritto && su_cambio) su_cambio();

    char *testo_json = cJSON_PrintUnformatted(fuori);
    cJSON_Delete(fuori);
    json_proprio(s, 200, testo_json);
}

/* Lo stato dei segreti senza scriverne nessuno: la pagina deve poter
   mostrare "impostato" accanto a un campo lasciato vuoto. */
static void api_secrets_get(risposta_t *s)
{
    cJSON *o = cJSON_CreateObject();
    if (!o) { json_proprio(s, 500, NULL); return; }
    for (int n = 0; n < SEG_QUANTI; n++)
        cJSON_AddBoolToObject(o, segreto_nome((segreto_t)n),
                              segreti_impostato((segreto_t)n));
    char *testo_json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    json_proprio(s, 200, testo_json);
}

/* L'elenco delle entita che Home Assistant conosce, per i menu della
 * pagina. Rispondeva sempre "nessun collegamento", anche a collegamento
 * attivo: era rimasto dalla Fase 2, quando Home Assistant non c'era.
 *
 * La prima domanda quasi sempre torna a mani vuote con `stato: "in
 * arrivo"`, e non e un difetto: chiedere l'elenco vuol dire un giro sul
 * WebSocket, e questo server risponde dal ciclo principale senza
 * aspettare nessuno. La pagina richiede fra un secondo. Chiedere solo
 * quando qualcuno domanda evita di tenere ventimila byte in memoria sui
 * mesi in cui la pagina non si apre. */
/* Le stanze che il robot conosce, per la pagina di configurazione. Stessa
   forma di api_entities() e stessa ragione: chi configura sceglie da un
   elenco vero invece di copiare numeri da un'altra applicazione — che per
   le stanze del robot **non sono gli stessi numeri**, e l'abbiamo scoperto
   guardando il robot uscire dalla base e rientrare senza pulire. */
static void api_stanze_robot(risposta_t *s)
{
    if (ha_stanze_no()) {
        testo(s, 200, "application/json",
              "{\"rooms\":[],\"state\":\"the robot cannot list them\"}");
        return;
    }

    const char *e = ha_stanze();
    if (!e) {
        ha_stanze_chiedi(cfg_testo("robot/entity", ""));
        testo(s, 200, "application/json",
              "{\"rooms\":[],\"state\":\"on its way\"}");
        return;
    }

    s->codice = 200;
    s->tipo = "application/json";
    s->corpo = e;
    s->corpo_n = strlen(e);
    s->libera = NULL;
}

static void api_entities(risposta_t *s)
{
    if (ha_stato() != HA_PRONTO) {
        testo(s, 200, "application/json",
              "{\"entities\":[],\"state\":\"not connected to Home Assistant\"}");
        return;
    }

    if (ha_elenco_no()) {
        testo(s, 200, "application/json",
              "{\"entities\":[],\"state\":\"Home Assistant cannot list them\"}");
        return;
    }

    const char *e = ha_elenco();
    if (!e) {
        ha_elenco_chiedi();
        testo(s, 200, "application/json",
              "{\"entities\":[],\"state\":\"on its way\"}");
        return;
    }

    /* Arriva gia scritto da ha.c, parentesi comprese, e si manda com'e:
       ricomporlo qui vorrebbe dire una seconda copia di qualche decina di
       kilobyte, e su questo chip la seconda copia e quella che non ci
       sta.

       Non si libera: il testo e di ha.c, che lo tiene finche il
       collegamento dura. Regge perche i due girano nello stesso ciclo, uno
       dopo l'altro: la risposta esce dentro http_gira(), e ha_gira() — che
       e l'unico a poterlo buttare, quando il collegamento cade — non corre
       nel frattempo. Il giorno che il server web avesse un compito suo,
       questa riga andrebbe rifatta con una copia. */
    testo(s, 200, "application/json", e);
}

/* Vera quando qualcuno ha chiesto di riavviare e la risposta e gia
   partita. La legge il ciclo principale. */
static bool riavviare;

bool web_riavvio_chiesto(void) { return riavviare; }

static void api_reboot(risposta_t *s)
{
    testo(s, 200, "application/json", "{\"result\":\"restarting\"}");
    /* Il riavvio vero lo fa il ciclo principale dopo che questa risposta e
       uscita: rispondere dopo essersi riavviati non e possibile.

       Qui c'era **solo** il commento. La risposta diceva "riavvio" e non si
       riavviava niente: la pagina web mostrava la conferma, il pannello
       restava dov'era, e chi guardava concludeva che il riavvio non serve a
       niente invece che non essere avvenuto. */
    riavviare = true;
}

/* --- aggiornamento del firmware ----------------------------------------
 *
 * L'unico percorso a flusso del server: due megabyte non si tengono in
 * memoria mentre li si scrive in flash. Vedi http.h per il perche non e un
 * meccanismo generale.
 *
 * Sta dietro allo sblocco come tutto il resto — bisogna essere stati davanti
 * al pannello negli ultimi quindici minuti — e la verifica che conta non e
 * questa: e la firma, che ESP-IDF controlla alla chiusura, su tutta
 * l'immagine, prima che la partizione di avvio cambi.
 */
static bool ota_esito;
static char ota_errore[96];

static bool ota_apri(size_t dichiarati)
{
    ota_errore[0] = 0;

    /* Il pannello chiuso non si aggiorna. Il controllo e qui e non nel
       dispatcher perche il percorso a flusso lo scavalca: e proprio il
       genere di scorciatoia che lascia una porta aperta senza che nessuno
       se ne accorga. */
    if (!web_sbloccato()) {
        snprintf(ota_errore, sizeof ota_errore,
                 "configurazione chiusa: sbloccala dal pannello");
        return false;
    }
    if (!agg_apri(dichiarati)) {
        snprintf(ota_errore, sizeof ota_errore, "%s", agg_motivo());
        return false;
    }
    return true;
}

static bool ota_pezzo(const void *dati, size_t n)
{
    if (agg_scrivi(dati, n)) return true;
    snprintf(ota_errore, sizeof ota_errore, "%s", agg_motivo());
    return false;
}

static bool ota_chiudi(bool completo)
{
    ota_esito = agg_chiudi(completo);
    if (!ota_esito && !ota_errore[0])
        snprintf(ota_errore, sizeof ota_errore, "%s", agg_motivo());
    return ota_esito;
}

static void ota_risposta(bool esito, risposta_t *s)
{
    static char corpo[192];
    if (esito) {
        snprintf(corpo, sizeof corpo,
                 "{\"result\":\"installed\",\"bytes\":%u,\"restart\":true}",
                 (unsigned)agg_scritti());
        testo(s, 200, "application/json", corpo);
        /* Il riavvio lo fa il ciclo principale dopo che questa risposta e
           uscita: chi ha caricato deve vedere la conferma, non un errore di
           rete. */
        riavviare = true;
        return;
    }
    /* Il motivo torna indietro per intero. "Aggiornamento fallito" e basta
       manderebbe a riprovare lo stesso file: sapere che e la firma, o che
       l'immagine e piu grande della partizione, dice cosa cambiare. */
    snprintf(corpo, sizeof corpo, "{\"result\":\"failed\",\"error\":\"%s\"}",
             ota_errore[0] ? ota_errore : "motivo non riportato");
    testo(s, 400, "application/json", corpo);
}

/* Vedi web.h: fa percorrere all'immagine la stessa catena che percorre
   arrivando dal socket, pezzo per pezzo. */
bool web_prova_ota(const void *dati, size_t n, size_t dichiarati)
{
    if (!ota_apri(dichiarati)) return false;

    /* Pezzi piccoli di proposito: e cosi che arrivano davvero, e un difetto
       nel rimetterli insieme non si vede mai su un blocco solo. */
    const size_t PEZZO = 512;
    bool bene = true;
    for (size_t f = 0; f < n && bene; f += PEZZO) {
        const size_t q = (n - f < PEZZO) ? n - f : PEZZO;
        bene = ota_pezzo((const char *)dati + f, q);
    }
    const bool esito = ota_chiudi(bene && (dichiarati ? n >= dichiarati : true));

    /* Anche la risposta, e non per completezza: e li che si decide il
       riavvio, ed e li che il motivo del rifiuto torna indietro. Fermarsi
       prima vorrebbe dire provare meta catena e chiamarla catena. */
    risposta_t s = {0};
    ota_risposta(esito, &s);
    return esito;
}

static const http_flusso_t OTA = {
    .percorso = "/api/ota",
    .apri     = ota_apri,
    .pezzo    = ota_pezzo,
    .chiudi   = ota_chiudi,
    .risposta = ota_risposta,
};

/* --- ripristino di fabbrica --------------------------------------------
 *
 * Sta dietro allo sblocco come tutto il resto: la pagina risponde soltanto
 * dopo che qualcuno ha toccato "Consenti configurazione" sul vetro. Per
 * un'azione che cancella le credenziali e la barriera giusta — bisogna
 * essere stati in casa, davanti al muro, negli ultimi quindici minuti.
 *
 * La conferma la chiede la pagina, non questo codice: un `POST` arrivato
 * fin qui e gia una decisione presa, e chiedere due volte dalla stessa
 * parte del filo non aggiunge niente. */
static void api_ripristino(risposta_t *s)
{
    if (!ripristino_esegui()) {
        /* Non si riavvia. Un pannello che riparte credendo di aver
           dimenticato qualcosa che invece ha ancora e peggio di uno che
           dice di non esserci riuscito. */
        testo(s, 500, "application/json",
              "{\"result\":\"failed\",\"error\":"
              "\"la configurazione o i segreti sono ancora sul pannello\"}");
        return;
    }
    testo(s, 200, "application/json",
          "{\"result\":\"reset\",\"restart\":true}");
    riavviare = true;
}

/* --- translations ---------------------------------------------------------
 *
 * The texts of the panel and of this page, edited from the page. See
 * translations.h for the file and the checks. `lang` chooses the language;
 * without it, the panel's own. */

/* True when the caller named the language. */
static bool texts_lang(const richiesta_t *r, char *code, size_t n)
{
    if (parametro(r->query, "lang", code, n) && code[0]) return true;
    snprintf(code, n, "%s", cfg_testo("system/language", "en"));
    return false;
}

static void no_language(risposta_t *s)
{
    testo(s, 404, "application/json", "{\"error\":\"no such language\"}");
}

static void api_texts_get(const richiesta_t *r, risposta_t *s)
{
    char code[8], edit[4] = "";
    const bool asked = texts_lang(r, code, sizeof code);
    parametro(r->query, "edit", edit, sizeof edit);
    char *(*get)(const char *) = strcmp(edit, "1") == 0 ? translations_editor
                                                        : translations_page;
    char *out = get(code);
    /* The panel's language may be one this firmware does not have — a
       configuration from a newer one. The page still wants to show itself. */
    if (!out && !asked) out = get("en");
    if (!out) { no_language(s); return; }
    json_proprio(s, 200, out);
}

static void api_texts_export(const richiesta_t *r, risposta_t *s)
{
    char code[8];
    texts_lang(r, code, sizeof code);
    char *out = translations_export(code);
    if (!out) { no_language(s); return; }
    json_proprio(s, 200, out);
}

static void api_texts_post(const richiesta_t *r, risposta_t *s)
{
    char code[8];
    texts_lang(r, code, sizeof code);
    if (translations_save(code, r->corpo, r->corpo_n)) {
        texts_changed();
        testo(s, 200, "application/json", "{\"result\":\"saved\"}");
        return;
    }
    /* The same shape as the configuration's 422: which texts, and why. */
    cJSON *o = cJSON_CreateObject();
    cJSON *campi = o ? cJSON_AddArrayToObject(o, "fields") : NULL;
    if (!campi) { cJSON_Delete(o); json_proprio(s, 500, NULL); return; }
    for (int n = 0; n < translations_errors(); n++) {
        const translation_error_t *e = translations_error(n);
        cJSON *c = cJSON_CreateObject();
        if (!c) break;
        cJSON_AddStringToObject(c, "field", e->field);
        cJSON_AddStringToObject(c, "reason", e->reason);
        cJSON_AddItemToArray(campi, c);
    }
    cJSON_AddArrayToObject(o, "warnings");
    cJSON_AddFalseToObject(o, "file_changed");
    json_proprio(s, 422, cJSON_PrintUnformatted(o));
    cJSON_Delete(o);
}

static void api_texts_reset(const richiesta_t *r, risposta_t *s)
{
    char code[8];
    texts_lang(r, code, sizeof code);
    if (!translations_reset(code)) { no_language(s); return; }
    texts_changed();
    testo(s, 200, "application/json", "{\"result\":\"reset\"}");
}

static void api_config_export(risposta_t *s)
{
    /* Con i segreti oscurati come tutto il resto: questo file finisce su
       una chiavetta o in una cartella di backup. */
    json_proprio(s, 200, cfg_esporta(true));
}

/* --- smistamento -------------------------------------------------------- */

static bool uguale(const char *a, const char *b) { return strcmp(a, b) == 0; }

#ifdef PANNELLO_BANCO_ROBOT
/* --- banco di prova del robot ------------------------------------------
 *
 * **Temporaneo.** Manda al robot un comando di pulizia per stanze e risponde
 * con il JSON esatto che ha spedito, cosi la forma dei parametri si prova
 * invece di dedurla — cosa che finora e andata male due volte: prima il
 * comando arrivava con la lista vuota, e il robot usciva dalla base e
 * rientrava senza pulire.
 *
 *     /api/prova/robot?stanze=17            manda, forma da configurazione
 *     /api/prova/robot?stanze=17,18         piu stanze
 *     /api/prova/robot?stanze=17&manda=0    mostra il JSON e non spedisce
 *
 * Vive alla stessa condizione di /api/status: la spunta degli endpoint di
 * diagnostica. Va tolto quando la pulizia funziona — e un comando fisico
 * raggiungibile da chiunque sia sulla rete di casa, e va bene per il tempo
 * di una prova concordata, non per sempre. */
static void api_prova_robot(const richiesta_t *r, risposta_t *s)
{
    if (troppo_spesso(r->chiamante)) {
        testo(s, 429, "application/json", "{\"error\":\"too many requests\"}");
        return;
    }

    int numeri[8];
    int quanti = 0;

    const char *p = strstr(r->query ? r->query : "", "stanze=");
    if (p) {
        p += 7;
        while (quanti < 8 && *p >= '0' && *p <= '9') {
            int v = 0;
            while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
            numeri[quanti++] = v;
            if (*p == ',' || *p == '%') { while (*p && *p != ',') p++; if (*p) p++; }
            else break;
        }
    }

    /* Un comando qualsiasi al robot, e la risposta che torna indietro:
       serve a chiedergli `get_room_mapping`, cioe la corrispondenza fra i
       numeri della sua mappa e i segmenti che accetta. */
    const char *cmd = strstr(r->query ? r->query : "", "command=");
    if (cmd) {
        char nome[48] = "";
        const char *q = cmd + 8;
        size_t i = 0;
        while (*q && *q != '&' && i + 1 < sizeof nome) nome[i++] = *q++;
        nome[i] = 0;

        /* Con ?servizio=dominio.nome si chiama un servizio qualsiasi
           chiedendo la risposta: serve a roborock.get_maps, che la mappa
           delle stanze la sa e la restituisce. Senza, si manda il comando
           al robot come prima. */
        bool andato;
        const char *srv = strstr(r->query ? r->query : "", "service=");
        if (srv) {
            char pieno[64] = "";
            const char *z = srv + 9;
            size_t k = 0;
            while (*z && *z != '&' && k + 1 < sizeof pieno) pieno[k++] = *z++;
            pieno[k] = 0;
            char *punto = strchr(pieno, '.');
            if (punto) {
                *punto = 0;
                andato = ha_chiama_con_risposta(pieno, punto + 1,
                                                cfg_testo("robot/entity", ""), NULL);
            } else andato = false;
        } else {
            char dati[96];
            snprintf(dati, sizeof dati, "\"command\":\"%s\"", nome);
            andato = dati_robot_manda_json(dati);
        }

        static char corpo[900];
        snprintf(corpo, sizeof corpo,
                 "{\"command\":\"%s\",\"sent\":%s,\"last_response\":%s}",
                 nome, andato ? "true" : "false",
                 ha_ultima_risposta()[0] ? ha_ultima_risposta() : "null");
        testo(s, 200, "application/json", corpo);
        return;
    }

    /* Le stanze come le ha capite il pannello, per verificare che la
       lettura della mappa funzioni senza dover sbloccare la pagina. */
    if (strstr(r->query ? r->query : "", "robot_rooms=1")) {
        if (!ha_stanze()) ha_stanze_chiedi(cfg_testo("robot/entity", ""));
        static char corpo[900];
        snprintf(corpo, sizeof corpo, "{\"read\":%s,\"refused\":%s}",
                 ha_stanze() ? ha_stanze() : "null",
                 ha_stanze_no() ? "true" : "false");
        testo(s, 200, "application/json", corpo);
        return;
    }

    /* Solo la risposta, senza mandare niente: si chiama dopo il comando,
       quando Home Assistant ha avuto il tempo di rispondere. */
    if (strstr(r->query ? r->query : "", "response=1")) {
        static char corpo[900];
        snprintf(corpo, sizeof corpo, "{\"last_response\":%s}",
                 ha_ultima_risposta()[0] ? ha_ultima_risposta() : "null");
        testo(s, 200, "application/json", corpo);
        return;
    }

    /* Un attributo qualsiasi dell'entita del robot, per guardare cosa
       manda davvero Home Assistant senza doverlo chiedere a chi e in casa.
       Serve a trovare gli identificativi veri dei segmenti, che non sempre
       sono i numeri che l'applicazione mostra sulla mappa. */
    const char *att = strstr(r->query ? r->query : "", "attribute=");
    if (att) {
        char nome[48] = "";
        const char *q = att + 10;
        size_t i = 0;
        while (*q && *q != '&' && i + 1 < sizeof nome) nome[i++] = *q++;
        nome[i] = 0;

        char *j = ent_attributo_json(cfg_testo("robot/entity", ""), nome);
        static char corpo[900];
        snprintf(corpo, sizeof corpo, "{\"%s\":%s}", nome, j ? j : "null");
        if (j) free(j);
        testo(s, 200, "application/json", corpo);
        return;
    }

    /* Senza parametri non e un errore: e la domanda «quali stanze
       conosci?». Chi prova da un'altra stanza della casa non ha il
       documento sotto mano, e i numeri delle stanze sono la prima cosa che
       gli serve. */
    if (!quanti) {
        static char elenco[512];
        int n = snprintf(elenco, sizeof elenco, "{\"rooms\":[");
        for (int i = 0; i < dati_robot_stanze() && (size_t)n < sizeof elenco; i++) {
            const robot_stanza_t *st = dati_robot_stanza(i);
            if (!st) continue;
            n += snprintf(elenco + n, sizeof elenco - (size_t)n,
                          "%s{\"number\":%d,\"name\":\"%s\"}",
                          i ? "," : "", st->numero, st->nome ? st->nome : "");
        }
        /* Il controllo prima della chiusura, e non e pedanteria: snprintf
           torna la lunghezza che **avrebbe** scritto, non quella scritta.
           Con abbastanza stanze dai nomi lunghi `n` supera il buffer, e
           allora `sizeof elenco - n` non diventa negativo — e un size_t,
           diventa enorme — e questa riga scriverebbe fuori. Oggi non
           succede con nove stanze; succederebbe con sedici. */
        if ((size_t)n >= sizeof elenco) n = (int)sizeof elenco - 3;
        snprintf(elenco + n, sizeof elenco - (size_t)n, "]}");
        testo(s, 200, "application/json", elenco);
        return;
    }

    const bool manda = !strstr(r->query ? r->query : "",
                               "send=0");

    /* La forma si puo imporre dalla richiesta: ?forma=elenco oppure
       ?forma=segmenti. Senza, vale quella della configurazione. */
    char forma[16] = "";
    const char *f = strstr(r->query ? r->query : "", "format=");
    if (f) {
        f += 6;
        size_t i = 0;
        while (*f && *f != '&' && i + 1 < sizeof forma) forma[i++] = *f++;
        forma[i] = 0;
    }

    char json[320] = "";
    dati_robot_comando_stanze_json(numeri, quanti, forma, json, sizeof json);

    bool andato = false;
    if (manda) andato = dati_robot_manda_json(json);

    /* Lo stato del robot torna nella stessa risposta: chi prova da lontano
       deve poter vedere cosa succede senza avere il pannello davanti, e
       ripetendo la chiamata con manda=0 si guarda il robot muoversi. */
    const robot_t rb = dati_robot();
    static const char *const PAROLA[] = {
        "unknown", "idle", "cleaning", "returning", "docked", "paused",
        "error",
    };

    static char corpo[640];
    snprintf(corpo, sizeof corpo,
             "{\"rooms\":%d,\"sent\":%s,\"accepted\":%s,"
             "\"state\":\"%s\",\"detail\":\"%s\",\"battery\":%d,"
             "\"service_data\":{%s}}",
             quanti, manda ? "true" : "false", andato ? "true" : "false",
             PAROLA[rb.stato < 7 ? rb.stato : 0],
             rb.dettaglio ? rb.dettaglio : "",
             rb.batteria_c_e ? rb.batteria : -1, json);
    testo(s, 200, "application/json", corpo);
}
#endif


void web_servi(const richiesta_t *r, risposta_t *s)
{
    if (!r || !s) return;

    const bool sempre_attivi = cfg_vero("diagnostics/status_endpoint", true);

    /* I due percorsi in sola lettura, prima di tutto: rispondono anche a
       server chiuso, ed e la sola deroga. */
    if (r->metodo == HTTP_GET && sempre_attivi &&
        (uguale(r->percorso, "/api/status") || uguale(r->percorso, "/api/log"))) {
        if (troppo_spesso(r->chiamante)) {
            testo(s, 429, "application/json", "{\"error\":\"too many requests\"}");
            return;
        }
        if (uguale(r->percorso, "/api/status")) api_status(s);
        else                                    api_log(r, s);
        return;
    }

#ifdef PANNELLO_BANCO_ROBOT
    /* --- il banco di prova del robot, temporaneo -------------------------
     *
     * Vedi api_prova_robot(): serve a provare la forma dei parametri della
     * pulizia per stanze da un'altra stanza della casa, o da fuori. Si toglie
     * quando quella pulizia funziona. */
    if (r->metodo == HTTP_GET && sempre_attivi &&
        uguale(r->percorso, "/api/test/robot")) {
        api_prova_robot(r, s);
        return;
    }
#endif

    /* Tutto il resto non esiste, finche non lo si sblocca dal muro. */
    if (!web_sbloccato()) { non_trovato(s); return; }

    if (r->metodo == HTTP_GET) {
        if (uguale(r->percorso, "/") || uguale(r->percorso, "/index.html")) {
            s->codice = 200;
            s->tipo = "text/html; charset=utf-8";
            s->corpo = PAGINA_HTML;
            s->corpo_n = PAGINA_HTML_N;
            s->libera = NULL;
            return;
        }
        if (uguale(r->percorso, "/api/schema")) {
            s->codice = 200;
            s->tipo = "application/json";
            s->corpo = PAGINA_SCHEMA;
            s->corpo_n = PAGINA_SCHEMA_N;
            s->libera = NULL;
            return;
        }
        if (uguale(r->percorso, "/api/config"))        { api_config_get(s); return; }
        if (uguale(r->percorso, "/api/config/export")) { api_config_export(s); return; }
        if (uguale(r->percorso, "/api/secrets"))       { api_secrets_get(s); return; }
        if (uguale(r->percorso, "/api/entities"))      { api_entities(s); return; }
        if (uguale(r->percorso, "/api/robot-rooms")) { api_stanze_robot(s); return; }
        if (uguale(r->percorso, "/api/texts"))        { api_texts_get(r, s); return; }
        if (uguale(r->percorso, "/api/texts/export")) { api_texts_export(r, s); return; }
    }

    if (r->metodo == HTTP_POST) {
        if (uguale(r->percorso, "/api/config"))  { api_config_post(r, s); return; }
        if (uguale(r->percorso, "/api/secrets")) { api_secrets_post(r, s); return; }
        if (uguale(r->percorso, "/api/reboot"))  { api_reboot(s); return; }
        if (uguale(r->percorso, "/api/reset")) { api_ripristino(s); return; }
        if (uguale(r->percorso, "/api/texts"))       { api_texts_post(r, s); return; }
        if (uguale(r->percorso, "/api/texts/reset")) { api_texts_reset(r, s); return; }
    }

    non_trovato(s);
}

/* --- ciclo di vita ------------------------------------------------------ */

bool web_avvia(int porta)
{
    /* Il percorso a flusso si registra qui e non altrove: e il server a
       doverlo sapere prima di leggere la prima richiesta, e web.c e l'unico
       che sa quale percorso e. */
    http_flusso(&OTA);

    /* Un avviso all'avvio, e non un'informazione: e una barriera spenta di
       proposito, e l'unico modo perche non si dimentichi accesa e che
       compaia ogni volta che il pannello riparte, nel posto dove si va a
       guardare quando qualcosa non torna. */
    if (web_sempre_aperta())
        registro_aggiungi(LOG_AVVISO, "web",
                          "configurazione sempre aperta: nessuna finestra, "
                          "nessuno sblocco dal vetro");

    return http_avvia(porta, web_servi);
}

void web_ferma(void) { http_ferma(); }
