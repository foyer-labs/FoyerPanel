/* ------------------------------------------------------------------------
 * Le informazioni di sistema, sul pannello.
 * --------------------------------------------------------------------- */
#include "sistema.h"

#include "esp_wifi.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "rete.h"

static const char *TAG = "sistema";

rete_info_t sistema_rete(void)
{
    return (rete_info_t){
        .connessa    = rete_stato() == RETE_CONNESSA,
        .indirizzo   = rete_indirizzo(),
        .potenza_dbm = rete_potenza(),
        .descrizione = rete_descrizione(),
    };
}

bool sistema_radio_pronta(void) { return rete_radio_accesa(); }

const char *sistema_rete_prova(uint32_t *fra_s) { return rete_prova_stato(fra_s); }
bool        sistema_rete_in_prova(void) { return rete_in_prova(); }
void        sistema_rete_applica(void) { rete_ricontrolla(true); }
void        sistema_rete_riprova(void) { rete_riprova(); }

/* --- perche si e riacceso ----------------------------------------------
 *
 * 10-diagnostica.md lo chiama "il dato piu prezioso", e ha ragione: un
 * pannello che si e riavviato da solo non lo dice a nessuno, e la
 * differenza fra un guasto di alimentazione, un watchdog e una pila finita
 * e la differenza fra tre indagini diverse. Qui il chip lo sa, e basta
 * chiederglielo. */
static const char *perche_riacceso(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "accensione";
    case ESP_RST_SW:       return "riavvio richiesto";
    case ESP_RST_PANIC:    return "errore del programma";
    case ESP_RST_INT_WDT:  return "watchdog di interruzione";
    case ESP_RST_TASK_WDT: return "watchdog di compito";
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_BROWNOUT: return "tensione insufficiente";
    case ESP_RST_DEEPSLEEP:return "risveglio";
    case ESP_RST_EXT:      return "reset esterno";
    default:               return "sconosciuto";
    }
}

static uint16_t riavvii;
static const char *motivo = "sconosciuto";

void sistema_avvia(void)
{
    motivo = perche_riacceso();

    /* Il conteggio sta in NVS perche deve sopravvivere proprio alla cosa che
       conta. Uno spazio dei nomi suo: mescolarlo ai segreti vorrebbe dire
       che azzerare le credenziali azzera anche la storia dei riavvii, e
       sono due gesti diversi. */
    nvs_handle_t h;
    if (nvs_open("sistema", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_get_u16(h, "riavvii", &riavvii);
    riavvii++;
    nvs_set_u16(h, "riavvii", riavvii);
    nvs_commit(h);
    nvs_close(h);
}

/* --- i fotogrammi -------------------------------------------------------
 *
 * Si contano quelli **consegnati allo schermo**, non i giri del ciclo:
 * quando non cambia niente non si disegna, e contare i giri darebbe
 * ventiquattro fotogrammi al secondo su un'immagine ferma da un'ora.
 *
 * La finestra e un secondo, e il valore mostrato e quello della finestra
 * appena chiusa: gli fps sono per definizione una media, e una media su
 * meno di un secondo dice piu del rumore che del segnale. */
static uint32_t contati, finestra_ms;
static uint8_t  ultimi_fps;

void sistema_fotogramma(void)
{
    contati++;
    const uint32_t ora = (uint32_t)(esp_timer_get_time() / 1000);
    if (ora - finestra_ms < 1000) return;

    ultimi_fps = contati > 255 ? 255 : (uint8_t)contati;
    contati = 0;
    finestra_ms = ora;
}

sistema_info_t sistema_stato(void)
{
    return (sistema_info_t){
        .accensione_s = (uint32_t)(esp_timer_get_time() / 1000000),
        .riavvii      = riavvii,
        .motivo       = motivo,
        .heap_libero  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        /* Il minimo storico e il numero che conta: e il primo sintomo di una
           perdita di memoria, e si vede giorni prima del riavvio. */
        .heap_minimo  = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
        .psram_libera = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        .fps          = ultimi_fps,
    };
}

void sistema_riavvia(void) { esp_restart(); }

/* --- le reti che si vedono da qui ---------------------------------------
 *
 * Un giro di scansione con la radio gia avviata: esp_wifi_scan_start() in
 * modo asincrono, e il risultato si raccoglie quando l'evento dice che ha
 * finito. Sul p4 questo passa da esp_wifi_remote e quindi dal C6, ma da
 * qui non si vede: e la stessa chiamata.
 *
 * **Non si aspetta dentro questa funzione.** La radio, mentre gira i
 * canali, non e connessa e non risponde: bloccare qui vorrebbe dire
 * un'interfaccia ferma per due secondi buoni, e sul primo avvio — che e
 * l'unico posto da cui si chiama — quella sarebbe la prima impressione che
 * il pannello fa. Si chiede, si torna a guardare.
 *
 * Le reti senza nome si scartano: un SSID nascosto arriva come stringa
 * vuota, e una riga vuota in un elenco da cui bisogna scegliere e peggio
 * di una riga in meno. Chi ha una rete nascosta la scrive a mano — e per
 * quello serve comunque la tastiera, che c'e.
 */
#define SCAN_MAX 20

static wifi_ap_record_t trovate[SCAN_MAX];
static uint16_t         quante_trovate;
static bool             in_corso;

/* --- come si sa che il giro e finito ------------------------------------
 *
 * Lo dice la radio con un evento, e non si deduce da una chiamata che
 * fallisce.
 *
 * Qui prima si deduceva, e la deduzione era scritta nel commento: «
 * esp_wifi_scan_get_ap_num() risponde solo a giro finito, finche e in corso
 * torna un errore, ed e cosi che si sa che non e ora ». Sul silicio dove la
 * radio e a bordo e vero. Sul p4 la chiamata attraversa l'RPC verso il C6, e
 * quella **risponde subito**, con le reti sentite fino a quel momento.
 *
 * Il risultato era una schermata con una rete sola, e ogni avvio una
 * diversa: si guardava trecento millisecondi dopo l'inizio del giro, si
 * trovava il conto a uno, lo si prendeva per definitivo e si smetteva di
 * guardare. Non era il trasporto dei risultati a perdere le altre — quelle
 * non erano ancora arrivate.
 *
 * L'evento invece vuol dire una cosa sola, su tutti e due i silici. */
static volatile bool giro_finito;
/* L'avviso del ripiego si dice una volta: e una proprieta del
   co-processore, non di questo giro, e ripeterla a ogni scansione
   riempirebbe il registro di una notizia gia data. */
static bool ripiego_detto;
static bool ascolto_acceso;

static void su_giro_finito(void *arg, esp_event_base_t base, int32_t id,
                           void *dato)
{
    (void)arg; (void)base; (void)id; (void)dato;
    giro_finito = true;
}

bool sistema_reti_cerca(void)
{
    if (in_corso) return true;      /* un giro alla volta */

    /* Alla prima richiesta: il ciclo degli eventi c'e gia, l'ha creato
       rete_avvia(), e senza radio non si arriverebbe comunque qui. */
    if (!ascolto_acceso) {
        if (esp_event_handler_instance_register(WIFI_EVENT,
                WIFI_EVENT_SCAN_DONE, su_giro_finito, NULL, NULL) != ESP_OK)
            return false;
        ascolto_acceso = true;
    }

    /* --- si chiede il giro **senza dire come farlo** ---------------------
     *
     * Il parametro e nullo, e non e pigrizia: e il risultato di due misure.
     *
     * Prima c'era una configurazione con i campi a zero, poi una con i tempi
     * di ascolto scritti a mano. In tutti e due i casi il C6 tornava con
     * **una** rete sola — quella dell'inverter fotovoltaico, cosi vicina da
     * farsi sentire comunque — mentre di reti in giro ce ne sono decine. Il
     * conteggio del chip diceva uno, quindi non si perdevano nel trasporto
     * dei risultati: non le vedeva proprio.
     *
     * La configurazione attraversa l'RPC campo per campo, e il firmware che
     * gira sul C6 e di una generazione precedente a quella che compiliamo
     * qui — si presenta come 0.0.0, cioe non dichiara nemmeno la sua
     * versione. Quando due parti si scambiano una struttura con schemi
     * diversi, cio che si scrive in un campo arriva in un altro: un tempo di
     * ascolto letto come numero di canale spiega esattamente una rete sola.
     *
     * Con NULL non si manda niente da interpretare male. Il chip usa i suoi
     * valori di riposo, che sono quelli con cui il firmware di fabbrica
     * faceva le sue scansioni su questa stessa scheda.
     *
     * Da rivedere il giorno che il C6 avra un firmware allineato: allora una
     * configurazione esplicita tornera a voler dire quello che dice. */
    if (esp_wifi_scan_start(NULL, false) != ESP_OK) return false;

    in_corso = true;
    giro_finito = false;
    quante_trovate = 0;
    return true;
}

bool sistema_reti_pronte(void)
{
    if (!in_corso) return quante_trovate > 0;

    /* Vedi su_giro_finito: si aspetta l'evento, non il fallimento di una
       chiamata. */
    if (!giro_finito) return false;

    uint16_t n = 0;
    if (esp_wifi_scan_get_ap_num(&n) != ESP_OK) return false;

    /* --- come si prendono i risultati, e perche' in due modi -----------
     *
     * Sul p4 questa lettura attraversa l'RPC verso il C6, e nessuna delle
     * due chiamate di ESP-IDF si comporta li come sul silicio dove la radio
     * e a bordo. Quindi si provano tutte e due, in ordine, e si tiene la
     * prima che porta qualcosa.
     *
     * **Una per volta** (esp_wifi_scan_get_ap_record): il chip tiene una
     * coda e ne cede una a ogni chiamata. E la piu pulita — copia diritta,
     * nessun numero da riportare indietro — e finche' fallisce subito non
     * consuma niente, quindi provarla per prima non costa la seconda.
     *
     * **Tutte insieme** (esp_wifi_scan_get_ap_records): funziona a meta.
     * Non aggiorna il numero, che e un parametro di entrata e uscita, e
     * lascia zeri nei record che non riempie. Percio il limite lo da il
     * conteggio, non la capienza del vettore, e i record senza nome si
     * scartano: sono quelli mai riempiti, e sul vetro comparivano come
     * righe vuote «aperte» che facevano anche lampeggiare la schermata.
     *
     * Le reti senza nome si scartano comunque, in tutti e due i rami: un
     * SSID nascosto arriva come stringa vuota, e una riga vuota in un
     * elenco da cui bisogna scegliere e peggio di una riga in meno. */
    const uint16_t da_prendere = n < SCAN_MAX ? n : SCAN_MAX;
    uint16_t tenute = 0;

    for (uint16_t i = 0; i < da_prendere; i++) {
        wifi_ap_record_t a = { 0 };
        const esp_err_t e = esp_wifi_scan_get_ap_record(&a);
        if (e != ESP_OK) {
            if (i == 0 && !ripiego_detto) {
                ripiego_detto = true;
                ESP_LOGW(TAG, "una per volta non va (%s): provo all'ingrosso",
                         esp_err_to_name(e));
            }
            break;
        }
        if (a.ssid[0] == 0) continue;
        trovate[tenute++] = a;
    }

    if (tenute == 0 && da_prendere > 0) {
        uint16_t quante = da_prendere;
        if (esp_wifi_scan_get_ap_records(&quante, trovate) == ESP_OK) {
            /* `quante` non e affidabile: vedi sopra. Si guarda il conteggio
               e si buttano i record che nessuno ha riempito. */
            for (uint16_t i = 0; i < da_prendere; i++) {
                if (trovate[i].ssid[0] == 0) continue;
                if (tenute != i) trovate[tenute] = trovate[i];
                tenute++;
            }
        }
    }

    quante_trovate = tenute;
    /* Si dice **quante**, non quali: 12-sicurezza.md §4 tiene i nomi delle
       reti fuori dal registro diagnostico, che si esporta e si manda a chi
       aiuta. Un conto basta a distinguere «la radio non sente» da «le reti
       si perdono per strada», che e l'unica domanda a cui questa riga deve
       rispondere. */
    ESP_LOGI(TAG, "reti trovate: %u, con un nome %u",
             (unsigned)n, (unsigned)tenute);

    in_corso = false;
    return true;
}

int sistema_reti_quante(void)
{
    return in_corso ? 0 : (int)quante_trovate;
}

rete_trovata_t sistema_rete_trovata(int n)
{
    if (in_corso || n < 0 || n >= (int)quante_trovate)
        return (rete_trovata_t){ "", 0, false };

    const wifi_ap_record_t *a = &trovate[n];
    return (rete_trovata_t){
        .ssid     = (const char *)a->ssid,
        .dbm      = a->rssi,
        .protetta = a->authmode != WIFI_AUTH_OPEN,
    };
}
