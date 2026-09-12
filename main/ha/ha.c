/* ------------------------------------------------------------------------
 * Home Assistant — protocollo, riconnessione, comandi.
 * --------------------------------------------------------------------- */
#include "ha.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "cJSON.h"
#include "config.h"
#include "orologio.h"
#include "registro.h"
#include "entita.h"
#include "segreti.h"
#include "ws.h"

/* Gli identificatori dei messaggi partono da 1 e non si riusano mai dentro
   lo stesso collegamento: lo impone Home Assistant, e riaprendo si
   riparte da capo perche il collegamento e nuovo. */
#define ID_SOTTOSCRIZIONE 1
#define ID_FOTOGRAFIA     2
#define ID_FINE_FOTOGRAFIA 3
#define ID_PRIMO_COMANDO  10

static ha_stato_t stato = HA_SPENTO;
static char       motivo[128] = "";
static uint32_t   prossimo_tentativo_ms;
static uint32_t   ultimo_dato_ms;
/* L'elenco delle entita per i menu della pagina, e a che punto siamo:
   `elenco` e l'array JSON gia pronto, gli altri due dicono se e in arrivo
   o se Home Assistant ha detto di no. Qui in cima e non accanto alle
   funzioni che li usano perche il ripristino del collegamento, che li
   azzera, sta prima. */
static char *elenco;   /* il documento gia pronto per la pagina */
static bool  elenco_chiesto;
static bool  elenco_rifiutato;

/* Quando abbiamo mandato l'ultimo ping. Separato da ultimo_dato_ms di
   proposito: uno misura il nostro respiro, l'altro il loro. */
static uint32_t   ultimo_ping_ms;
static uint32_t   id_comando = ID_PRIMO_COMANDO;

/* --- gli id delle due richieste con risposta ---------------------------
 *
 * Erano due costanti, 5 e 6, e questo bastava a romperle: Home Assistant
 * pretende id **crescenti** su un collegamento, e i comandi partono da
 * ID_PRIMO_COMANDO. Chiedere l'elenco o le stanze dopo un comando qualsiasi
 * significava mandare un id piu basso, che HA respinge — e la richiesta non
 * si riprova, perche' `chiesto` resta vero. Il menu delle stanze spariva
 * per il resto del collegamento, senza un errore da nessuna parte.
 *
 * Adesso l'id lo danno gli stessi comandi, e si tiene da parte per
 * riconoscere la risposta. Zero vuol dire «non ancora chiesto». */
static uint32_t id_elenco, id_stanze, id_storico;

/* --- la curva della giornata -------------------------------------------
 *
 * Ventiquattro caselle, una per ora locale, in unita del sensore. -1 vuol
 * dire «di quell'ora non si sa niente»: e diverso da zero, e chi disegna
 * deve poterle distinguere — un impianto fermo e una casella vuota si
 * assomigliano solo finche non si prova a leggere il grafico alle sei del
 * mattino. */
#define STORICO_ORE 24
static int32_t storico[STORICO_ORE];
static bool    storico_chiesto, storico_pronto, storico_vuoto;
/* La mappa delle stanze del robot, chiesta una volta sola. */
static char      *stanze;
static bool       stanze_chieste, stanze_rifiutate;
/* Vedi il ramo "result": temporaneo, per il banco del robot. */
#ifdef PANNELLO_BANCO_ROBOT
static char       ultima_risposta[768];
#endif
static uint32_t   inviati, falliti;
static uint32_t   adesso;

/* Quanto si aspetta prima di riprovare. Dalla configurazione, perche una
   casa con Home Assistant su un Raspberry lento vuole piu respiro di una
   con un mini PC. */
static uint32_t attesa_riprova_ms(void)
{
    const int32_t s = cfg_intero("home_assistant/retry_s", 15);
    return (uint32_t)(s > 0 ? s : 15) * 1000u;
}

ha_stato_t  ha_stato(void)  { return stato; }
const char *ha_motivo(void) { return motivo; }

const char *ha_stato_nome(ha_stato_t s)
{
    switch (s) {
    case HA_SPENTO:          return "off";
    case HA_CONNETTO:        return "connecting";
    case HA_AUTENTICO:       return "authenticating";
    case HA_ALLINEO:         return "syncing";
    case HA_PRONTO:          return "connected";
    case HA_CADUTO:          return "dropped";
    case HA_TOKEN_RIFIUTATO: return "token_refused";
    }
    return "";
}

uint32_t ha_comandi_inviati(void) { return inviati; }

uint32_t ha_eta_ultimo_dato(void) { return ha_eta_ultimo_dato_ms(adesso); }
uint32_t ha_comandi_falliti(void) { return falliti; }

uint32_t ha_eta_ultimo_dato_ms(uint32_t adesso_ms)
{
    /* Mai arrivato niente: eta infinita, non zero. Zero vorrebbe dire
       "appena aggiornato", che e l'opposto. */
    return ultimo_dato_ms ? adesso_ms - ultimo_dato_ms : UINT32_MAX;
}

static void cade(const char *perche)
{
    snprintf(motivo, sizeof motivo, "%s", perche ? perche : "");
    ws_chiudi();
    /* I valori si dimenticano ma l'elenco no: non si sa piu com'e la casa,
       si sa ancora cosa chiedere. Tenerli sarebbe peggio che perderli —
       il pannello mostrerebbe uno stato vecchio come se fosse di adesso. */
    ent_dimentica_valori();
    stato = HA_CADUTO;
    prossimo_tentativo_ms = adesso + attesa_riprova_ms();
}

/* --- apertura ----------------------------------------------------------- */

static void apri(void)
{
    const char *host = cfg_testo("home_assistant/host", "");
    const int porta = cfg_intero("home_assistant/port", 8123);
    const char *percorso = cfg_testo("home_assistant/ws_path",
                                     "/api/websocket");

    if (!*host) {
        snprintf(motivo, sizeof motivo,
                 "nessun indirizzo di Home Assistant in configurazione");
        stato = HA_SPENTO;
        return;
    }
    if (!segreti_impostato(SEG_HA_TOKEN)) {
        /* Senza token non si prova nemmeno: un tentativo che fallira di
           sicuro riempie il registro e non insegna niente. */
        snprintf(motivo, sizeof motivo,
                 "manca il token: scrivilo dalla pagina di configurazione");
        stato = HA_SPENTO;
        return;
    }

    /* `tls` sta nel contratto da sempre e nessuno lo leggeva: 04-config
       .example.json ce l'ha, e il trasporto lo ignorava. Un campo di
       configurazione che non fa niente e peggio di un campo che manca —
       chi lo mette a vero si aspetta che serva a qualcosa. */
    const bool cifrato = cfg_vero("home_assistant/tls", false);

    if (!ws_apri(host, porta, percorso, cifrato)) {
        cade(ws_motivo());
        return;
    }
    stato = HA_CONNETTO;
    motivo[0] = 0;
}

void ha_avvia(void)
{
    ent_ricostruisci_elenco();
    id_comando = ID_PRIMO_COMANDO;
    if (stanze) { free(stanze); stanze = NULL; }
    stanze_chieste = stanze_rifiutate = false;
    /* Con il collegamento se ne vanno anche gli id: su una connessione
       nuova il contatore riparte, e un id tenuto da prima riconoscerebbe la
       risposta sbagliata. */
    id_stanze = id_elenco = id_storico = 0;
    storico_chiesto = storico_pronto = storico_vuoto = false;
    ultimo_dato_ms = 0;
    ultimo_ping_ms = 0;
    /* L'elenco si butta: a un collegamento nuovo la casa puo avere entita
       diverse, e un menu che offre quelle di ieri e peggio di un menu che
       non c'e. Si richiedera quando qualcuno riaprira la pagina. */
    free(elenco);
    elenco = NULL;
    elenco_chiesto = false;
    elenco_rifiutato = false;
    apri();
}

void ha_ferma(void)
{
    ws_chiudi();
    ent_dimentica_valori();
    stato = HA_SPENTO;
    motivo[0] = 0;
}

void ha_riprova(void)
{
    /* L'unico modo per uscire da HA_TOKEN_RIFIUTATO, e deve essere un
       gesto: qualcuno ha salvato una configurazione o un token nuovo. */
    ws_chiudi();
    stato = HA_SPENTO;
    prossimo_tentativo_ms = 0;
    ha_avvia();
}

/* --- messaggi in uscita -------------------------------------------------- */

/* Il token si prende in prestito e il messaggio si compone qui dentro:
   fuori di qui non ne resta una copia. E l'unico punto del firmware in cui
   il token passa, e va tenuto piccolo e senza rami.

   Funzione a se e non annidata: una funzione annidata usata come puntatore
   vuole una pila eseguibile, e su un ESP32 e una richiesta che non si fa. */
static void componi_auth(const char *token, void *dato)
{
    (void)dato;
    cJSON *o = cJSON_CreateObject();
    if (!o) return;
    cJSON_AddStringToObject(o, "type", "auth");
    cJSON_AddStringToObject(o, "access_token", token);
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!s) return;

    ws_manda(s);

    /* La copia in chiaro non resta nell'heap in attesa di essere
       riassegnata a qualcos'altro che poi finisce in un registro. */
    volatile char *p = s;
    for (size_t n = 0; s[n]; n++) p[n] = 0;
    cJSON_free(s);
}

static void manda_token(void)
{
    if (!segreti_usa(SEG_HA_TOKEN, componi_auth, NULL))
        cade("token non leggibile");
}

/* --- la fotografia iniziale ---------------------------------------------
 *
 * Prima si chiedeva `get_states`, che risponde con **tutte** le entita della
 * casa e tutti i loro attributi. Sull'impianto di casa sono 712 kB in un
 * frame solo: non e questione di alzare il tetto del buffer, perche quel
 * testo diventa poi un albero cJSON da qualche megabyte e una pausa di
 * analisi che si sente sotto il dito — il compito grafico e lo stesso che
 * legge il tocco.
 *
 * Il pannello pero non usa la casa intera: usa le entita nominate in
 * config.json, che il magazzino conosce gia una per una. `subscribe_entities`
 * accetta quell'elenco e risponde con quelle sole. E il comando che usa
 * anche l'interfaccia web di Home Assistant, per la stessa ragione.
 *
 * Presa la fotografia ci si **disiscrive**: gli aggiornamenti continuano ad
 * arrivare da `state_changed`, che porta lo stato nuovo completo di tutti
 * gli attributi. Le differenze di `subscribe_entities` sono invece parziali
 * — mandano solo gli attributi cambiati e l'elenco di quelli tolti — e
 * fonderle nel magazzino vorrebbe dire tenere due strade per la stessa cosa.
 * Una sola strada per gli aggiornamenti, e questa serve solo a cominciare.
 */
static void chiedi_fotografia(void)
{
    char msg[128];
    snprintf(msg, sizeof msg,
             "{\"id\":%d,\"type\":\"subscribe_events\","
             "\"event_type\":\"state_changed\"}", ID_SOTTOSCRIZIONE);
    ws_manda(msg);

    const int quante = ent_seguite();
    if (quante <= 0) {
        /* Nessuna entita da seguire: non c'e niente da fotografare, e
           chiedere una fotografia vuota sarebbe un giro a vuoto. E il caso
           di un pannello appena montato, non un guasto. */
        stato = HA_PRONTO;
        return;
    }

    /* L'elenco puo essere lungo: un identificatore sta in una sessantina di
       caratteri e le entita di una casa arredata sono qualche centinaio.
       Si misura invece di indovinare. */
    size_t serve = 96;
    for (int n = 0; n < quante; n++)
        serve += strlen(ent_seguita(n)) + 4;

    char *richiesta = malloc(serve);
    if (!richiesta) { cade("memoria non disponibile"); return; }

    /* --- perche c'e un tetto a ogni passo -------------------------------
     *
     * snprintf() torna quanto **avrebbe** scritto, non quanto ha scritto. Se
     * la stima di `serve` fosse corta anche di un byte, `k` supererebbe
     * `serve`, e `serve - (size_t)k` — che e senza segno — diventerebbe un
     * numero enorme: da quel momento si scriverebbe fuori dal buffer con un
     * limite che dice "quattro miliardi".
     *
     * La stima qui sopra e giusta, e il difetto non si e mai visto. Ma
     * dipende dal fatto che resti giusta mentre il formato cambia, e non c'e
     * niente che lo imponga: basta aggiungere un campo alla richiesta e
     * dimenticare di aggiornare il 96. Il tetto costa due righe e toglie la
     * dipendenza. */
    size_t k = 0;
    #define AGGIUNGI(...) do {                    \
        if (k < serve) {                        \
            const int _n = snprintf(richiesta + k, serve - k, __VA_ARGS__); \
            k += (_n < 0) ? 0 : ((size_t)_n > serve - k ? serve - k  \
                                                        : (size_t)_n); \
        }                                      \
    } while (0)

    AGGIUNGI("{\"id\":%d,\"type\":\"subscribe_entities\","
             "\"entity_ids\":[", ID_FOTOGRAFIA);
    for (int n = 0; n < quante; n++)
        AGGIUNGI("%s\"%s\"", n ? "," : "", ent_seguita(n));
    AGGIUNGI("]}");
    #undef AGGIUNGI

    /* Se il tetto e scattato la richiesta e monca, e una richiesta JSON
       monca fa chiudere il socket dall'altra parte — che si presenta come
       una caduta di rete e manda a cercare dalla parte sbagliata. Meglio
       dirlo qui. */
    if (k >= serve - 1) {
        /* cade() porta il motivo fino in `stato` e nella diagnostica: una
           richiesta JSON monca farebbe chiudere il socket dall'altra parte,
           e si presenterebbe come una caduta di rete — mandando a cercare
           dalla parte sbagliata. */
        free(richiesta);
        cade("troppe entita per una sola richiesta");
        return;
    }

    ws_manda(richiesta);
    free(richiesta);
    stato = HA_ALLINEO;
}

/* Presa la fotografia, la sottoscrizione filtrata ha finito il suo mestiere:
   restare iscritti vorrebbe dire ricevere per sempre differenze che non si
   guardano, visto che gli aggiornamenti li porta gia state_changed. */
static void basta_fotografia(void)
{
    char msg[96];
    snprintf(msg, sizeof msg,
             "{\"id\":%d,\"type\":\"unsubscribe_events\","
             "\"subscription\":%d}", ID_FINE_FOTOGRAFIA, ID_FOTOGRAFIA);
    ws_manda(msg);
}

/* La fotografia di subscribe_entities: un oggetto con dentro un campo per
   entita. La forma e piu stretta di quella di get_states — `s` per lo
   stato, `a` per gli attributi — perche e pensata per passare su una rete
   e non per essere letta da un umano. */
static void prendi_fotografia(const cJSON *aggiunte)
{
    for (const cJSON *e = cJSON_IsObject(aggiunte) ? aggiunte->child : NULL;
         e; e = e->next) {
        if (!e->string) continue;

        const cJSON *st = cJSON_GetObjectItemCaseSensitive(e, "s");
        if (!cJSON_IsString(st)) continue;

        const cJSON *attr = cJSON_GetObjectItemCaseSensitive(e, "a");
        char *testo = attr ? cJSON_PrintUnformatted(attr) : NULL;
        ent_aggiorna(e->string, st->valuestring, testo, adesso);
        if (testo) cJSON_free(testo);
    }
}


/* --- l'elenco delle entita che Home Assistant ha ------------------------
 *
 * Serve a una cosa sola: i menu a tendina della pagina di configurazione.
 * Battere "sensor.inverter_grid_active_power" a mano e esattamente dove
 * si sbaglia, e l'errore che ne segue e muto — il pannello segue
 * un'entita che non esiste e mostra un trattino, senza dire perche.
 *
 * --- il registro non basta, e si e visto configurando ------------------
 *
 * Si chiedeva `config/entity_registry/list_for_display`, cioe il **registro**
 * delle entita. Il registro pero non e l'elenco di cio che esiste: e
 * l'elenco di cio che ha un `unique_id`. Un'entita scritta in YAML — un
 * `climate` da template, un'automazione senza `id:` — funziona benissimo e
 * nel registro non c'e.
 *
 * Sull'impianto di casa la differenza e cosi: 133 automazioni in
 * `automations.yaml`, 40 nel registro (quelle con un `id:` esplicito, piu
 * quelle create dall'editor); delle sedici `climate.` in configurazione, il
 * registro ne conosce cinque. Chi compilava la pagina si trovava davanti a
 * una tendina che non aveva quello che cercava, e batteva a mano — che e
 * esattamente il gesto che questa tendina esiste per togliere.
 *
 * --- e nemmeno get_states, che qui e gia stato provato -----------------
 *
 * La risposta a `get_states` contiene tutto, ed e la ragione per cui non si
 * puo usare: **712 kB in un frame solo** su questo impianto, misurati, che
 * diventano un albero cJSON da qualche megabyte e una pausa di analisi che
 * si sente sotto il dito. E la stessa misura che ha fatto abbandonare
 * get_states per la fotografia iniziale — vedi chiedi_fotografia().
 *
 * --- si chiede a Home Assistant di fare il lavoro ----------------------
 *
 * `render_template` con un modello che mappa `states` sui soli
 * `entity_id`. Il conto lo fa il server, e quello che viaggia sono gli
 * identificatori e basta: qualche decina di kilobyte invece di settecento,
 * e nessun attributo da attraversare.
 *
 * **Ci si disiscrive appena arriva la risposta.** Un modello che nomina
 * `states` dichiara di dipendere da tutte le entita, quindi Home Assistant
 * lo ricalcola a ogni cambiamento in casa: tenerlo aperto vorrebbe dire un
 * elenco intero a ogni lampadina accesa. E lo stesso gesto della fotografia
 * iniziale, per la stessa ragione.
 *
 * Si chiede **quando qualcuno lo domanda**, non al collegamento: un
 * pannello a muro sta acceso mesi senza che nessuno apra la pagina, e
 * ventimila byte in PSRAM tenuti per un menu mai aperto sono ventimila
 * byte sprecati per mesi. */
/* La mappa delle stanze del robot: stessa forma dell'elenco delle entita,
   altro identificativo. Vedi ha_stanze_chiedi(). */



/* --- la mappa delle stanze del robot ------------------------------------
 *
 * `roborock.get_maps` risponde con i piani e, dentro ognuno, le stanze come
 * coppie numero-nome. Sono i numeri dei **segmenti**, quelli che il robot
 * accetta davvero — e non quelli che la sua applicazione mostra sulla mappa,
 * che sono un'altra cosa e hanno gia fatto perdere una serata.
 *
 * Si chiede con `return_response`, che e la sola strada per vedere cosa
 * risponde un servizio: senza, torna solo "riuscito".
 *
 * Su domanda e non al collegamento: serve alla pagina di configurazione, e
 * un pannello a muro sta acceso mesi senza che nessuno la apra. */
/* --- la curva della giornata --------------------------------------------
 *
 * `recorder/statistics_during_period` invece di
 * `history/history_during_period`, e non e un dettaglio: lo storico grezzo
 * di un sensore di potenza che cambia ogni pochi secondi sono migliaia di
 * righe per una giornata, le statistiche orarie sono ventiquattro. Su un
 * chip che tiene un messaggio intero in un buffer solo, e la differenza fra
 * una cosa che funziona e una che funziona finche la casa e piccola.
 *
 * Il prezzo e un requisito: il sensore deve avere `state_class` in Home
 * Assistant, se no il recorder non ne tiene statistiche e la risposta torna
 * vuota. Vuota si distingue da non-ancora-chiesta, e si dice.
 *
 * Da mezzanotte **locale**, non «le ultime ventiquattro ore»: il grafico si
 * legge come la giornata di oggi, e alle nove del mattino una finestra
 * mobile mostrerebbe meta della notte di ieri.
 */
static void prendi_storico(const cJSON *risultato)
{
    for (int n = 0; n < STORICO_ORE; n++) storico[n] = -1;

    /* Il risultato e un oggetto con dentro una chiave per entita chiesta;
       ne abbiamo chiesta una sola, quindi si prende la prima invece di
       ricostruirne il nome. */
    const cJSON *serie = NULL;
    if (cJSON_IsObject(risultato)) serie = risultato->child;
    if (!cJSON_IsArray(serie)) {
        /* Un `result` vuoto e la risposta piu probabile, e va detta qui e non
           in fondo alla funzione: il ritorno anticipato saltava l'avviso, e
           dal muro «nessuna curva» e «nessun messaggio» sembravano un
           guasto del pannello invece di un sensore senza state_class. */
        storico_vuoto = storico_pronto = true;

        /* Due vuoti diversi, e dirli uguali manda a cercare dalla parte
           sbagliata. Il recorder tiene statistiche solo per le ore
           **concluse**: da mezzanotte alle una non ce n'e ancora nessuna, e
           una risposta vuota li dentro e la cosa giusta. Fuori da quella
           fascia, un vuoto vuol dire che quel sensore non ha state_class e
           il recorder non lo sta contando affatto. */
        const int m = orologio_minuti();
        registro_aggiungi(LOG_AVVISO, "energia",
                          (m >= 0 && m < 60)
                              ? "curva vuota: la prima ora del giorno non e "
                                "ancora conclusa, ricompare dopo l'una"
                              : "nessuna statistica oraria: il sensore ha "
                                "state_class in Home Assistant?");
        return;
    }

    int riempite = 0;
    const cJSON *v = NULL;
    cJSON_ArrayForEach(v, serie) {
        const cJSON *inizio = cJSON_GetObjectItemCaseSensitive(v, "start");
        const cJSON *media  = cJSON_GetObjectItemCaseSensitive(v, "mean");
        if (!cJSON_IsNumber(inizio) || !cJSON_IsNumber(media)) continue;

        const int ora = orologio_ora_di((long long)inizio->valuedouble);
        if (ora < 0 || ora >= STORICO_ORE) continue;

        /* Negativo a zero: un impianto non produce meno di niente, e un
           valore sotto zero qui verrebbe da un sensore che misura altro. */
        const double x = media->valuedouble;
        storico[ora] = x > 0 ? (int32_t)x : 0;
        riempite++;
    }

    storico_vuoto = riempite == 0;
    storico_pronto = true;

    /* Detto una volta sola, e detto in tutti e due i casi: «zero ore» e la
       risposta piu probabile quando il sensore non ha state_class, e senza
       questa riga dal muro non c'e modo di distinguerla da una rete lenta. */
    if (storico_vuoto)
        registro_aggiungi(LOG_AVVISO, "energia",
                          "statistiche orarie vuote: il sensore ha state_class?");
    else
        registro_aggiungi(LOG_INFO, "energia",
                          "curva della giornata dal recorder");
}

void ha_storico_chiedi(const char *entita)
{
    if (storico_chiesto || stato != HA_PRONTO || !entita || !*entita) return;

    char da[40];
    if (!orologio_mezzanotte_iso(da, sizeof da)) return;   /* ora non credibile */

    const uint32_t mio = id_comando++;
    char msg[256];
    snprintf(msg, sizeof msg,
        "{\"id\":%" PRIu32 ",\"type\":\"recorder/statistics_during_period\","
        "\"start_time\":\"%s\",\"statistic_ids\":[\"%s\"],"
        "\"period\":\"hour\",\"types\":[\"mean\"]}", mio, da, entita);
    if (ws_manda(msg)) { storico_chiesto = true; id_storico = mio; }
}

bool ha_storico_c_e(void) { return storico_pronto && !storico_vuoto; }
bool ha_storico_no(void)  { return storico_pronto && storico_vuoto; }

int32_t ha_storico_ora(int ora)
{
    if (!storico_pronto || ora < 0 || ora >= STORICO_ORE) return -1;
    return storico[ora];
}

void ha_storico_scade(void)
{
    /* Si riapre la porta alla prossima richiesta, ma **non** si butta quello
       che c'e: fra la scadenza e la risposta nuova il grafico continua a
       mostrare la curva di prima, che e vecchia di un quarto d'ora e non
       sbagliata. Azzerarla farebbe lampeggiare un grafico vuoto a ogni
       rinfresco. */
    storico_chiesto = false;
}

void ha_stanze_chiedi(const char *entita)
{
    if (stanze || stanze_chieste || stato != HA_PRONTO || !entita || !*entita)
        return;

    const uint32_t mio = id_comando++;
    char msg[256];
    snprintf(msg, sizeof msg,
        "{\"id\":%" PRIu32 ",\"type\":\"call_service\",\"domain\":\"roborock\","
        "\"service\":\"get_maps\",\"return_response\":true,"
        "\"target\":{\"entity_id\":\"%s\"}}", mio, entita);
    if (ws_manda(msg)) { stanze_chieste = true; id_stanze = mio; }
}

const char *ha_stanze(void) { return stanze; }
bool        ha_stanze_no(void) { return stanze_rifiutate; }

/* Da {"response":{"vacuum.x":{"maps":[{"rooms":{"20":"Cucina",...}}]}}}
   a [{"number":20,"name":"Cucina"},...] — la forma che la pagina si aspetta.
   Si tengono le stanze di **tutti** i piani: una casa su due livelli ha due
   mappe, e scartare la seconda vorrebbe dire non poter comandare di sopra. */
static void prendi_stanze(const cJSON *risultato)
{
    const cJSON *risp = cJSON_GetObjectItemCaseSensitive(risultato, "response");
    if (!cJSON_IsObject(risp)) { stanze_rifiutate = true; return; }

    cJSON *fuori = cJSON_CreateArray();
    if (!fuori) return;

    const cJSON *per_entita = NULL;
    cJSON_ArrayForEach(per_entita, risp) {
        const cJSON *mappe = cJSON_GetObjectItemCaseSensitive(per_entita, "maps");
        const cJSON *mappa = NULL;
        cJSON_ArrayForEach(mappa, mappe) {
            const cJSON *stanze_j =
                cJSON_GetObjectItemCaseSensitive(mappa, "rooms");
            const cJSON *st = NULL;
            cJSON_ArrayForEach(st, stanze_j) {
                if (!st->string || !cJSON_IsString(st)) continue;
                cJSON *v = cJSON_CreateObject();
                if (!v) continue;
                cJSON_AddNumberToObject(v, "number", atoi(st->string));
                cJSON_AddStringToObject(v, "name", st->valuestring);
                cJSON_AddItemToArray(fuori, v);
            }
        }
    }

    if (cJSON_GetArraySize(fuori) == 0) { cJSON_Delete(fuori); stanze_rifiutate = true; return; }

    cJSON *involucro = cJSON_CreateObject();
    if (involucro) {
        cJSON_AddItemToObject(involucro, "rooms", fuori);
        stanze = cJSON_PrintUnformatted(involucro);
        cJSON_Delete(involucro);
    } else {
        cJSON_Delete(fuori);
    }
}

void ha_elenco_chiedi(void)
{
    if (elenco || elenco_chiesto || stato != HA_PRONTO) return;

    const uint32_t mio = id_comando++;
    char msg[160];
    snprintf(msg, sizeof msg,
             "{\"id\":%" PRIu32 ",\"type\":\"render_template\","
             "\"template\":\"{{ states | map(attribute='entity_id') | join(',') }}\","
             "\"report_errors\":false}",
             mio);
    if (ws_manda(msg)) { elenco_chiesto = true; id_elenco = mio; }
}

/* Chiude la sottoscrizione del modello. Va fatto appena la risposta e
   arrivata: da li in poi ogni cambiamento in casa ne farebbe arrivare
   un'altra, identica e inutile. */
static void basta_elenco(void)
{
    if (!id_elenco) return;
    char msg[96];
    snprintf(msg, sizeof msg,
             "{\"id\":%" PRIu32 ",\"type\":\"unsubscribe_events\","
             "\"subscription\":%" PRIu32 "}", id_comando++, id_elenco);
    ws_manda(msg);
    id_elenco = 0;
}

const char *ha_elenco(void)   { return elenco; }
bool        ha_elenco_no(void) { return elenco_rifiutato; }

/* Da `"light.x,light.y,..."` a `{"entita":["light.x","light.y",...]}`.
 *
 * Il modello risponde con **una riga sola**, gli identificatori separati da
 * virgole. Il join si fa dire dal server invece di mandare un array: cosi
 * quello che arriva e' una stringa e basta, e non dipende da come Home
 * Assistant decide di interpretare il risultato di un modello — che a
 * seconda della versione puo tornare come testo o come elenco gia
 * convertito. Una forma sola, un percorso solo.
 *
 * Si riscrive invece di tenere l'albero cJSON: l'albero di duemila entita
 * sono decine di migliaia di nodi da sessanta byte, l'array di testo e
 * qualche decina di kilobyte e si manda alla pagina cosi com'e, senza
 * riserializzare niente a ogni richiesta. */
/* When the answer cannot be used, say no instead of keeping quiet.
   Quiet, `elenco_chiesto` stayed true and `elenco` empty: the page got
   "on its way" at every question for as long as the connection lasted,
   and never asked again. */
static void elenco_inservibile(const char *perche)
{
    elenco_rifiutato = true;
    registro_aggiungi(LOG_AVVISO, "ha", perche);
}

static void prendi_elenco(const cJSON *risultato)
{
    if (!cJSON_IsString(risultato) || !risultato->valuestring) {
        elenco_inservibile("entity list in an unexpected shape");
        return;
    }

    const char *s = risultato->valuestring;
    const size_t lung = strlen(s);
    if (!lung) { elenco_inservibile("Home Assistant has no entities to list"); return; }

    /* Si scrive il **documento intero** che la pagina ricevera, parentesi
       comprese, e non solo l'array. Non e pigrizia: il server web manda un
       corpo solo, e per aggiungere `{"entita": ... }` attorno dovrebbe
       farne una copia. Su questo chip la seconda copia di qualche decina
       di kilobyte e quella che non ci sta. */
    static const char TESTA[] = "{\"entities\":[";
    static const char CODA[]  = "]}";

    /* Quante ne sono: le virgole piu uno. Serve a sapere quanti apici
       aggiungere, e a chiedere il mucchio una volta sola. */
    size_t quante = 1;
    for (size_t n = 0; n < lung; n++) if (s[n] == ',') quante++;

    const size_t serve = sizeof TESTA + sizeof CODA + lung + quante * 2;
    char *b = malloc(serve);
    if (!b) { elenco_inservibile("not enough memory for the entity list"); return; }

    size_t k = strlen(TESTA);
    memcpy(b, TESTA, k);

    /* Una passata sola, copiando fra apici quello che sta fra due virgole.
       Gli identificatori di Home Assistant non hanno apici ne barre
       rovesciate — sono minuscole, cifre, punto e trattino basso — quindi
       non c'e niente da proteggere: se un giorno ne comparisse uno strano,
       il salto qui sotto lo lascia fuori invece di scrivere JSON rotto. */
    const char *p = s;
    bool primo = true;
    while (p < s + lung) {
        const char *virgola = strchr(p, ',');
        const size_t n = virgola ? (size_t)(virgola - p) : (size_t)(s + lung - p);

        bool pulito = n > 0;
        for (size_t q = 0; q < n && pulito; q++)
            if (p[q] == '"' || p[q] == '\\') pulito = false;

        if (pulito) {
            if (!primo) b[k++] = ',';
            primo = false;
            b[k++] = '"';
            memcpy(b + k, p, n);
            k += n;
            b[k++] = '"';
        }

        if (!virgola) break;
        p = virgola + 1;
    }

    memcpy(b + k, CODA, sizeof CODA);   /* porta il terminatore con se */

    free(elenco);
    elenco = b;
}

/* Come ha_chiama(), ma chiede a Home Assistant di **riportare indietro** la
   risposta del servizio. Serve ai servizi che ne hanno una — roborock.get_maps
   e la mappa delle stanze — e non a quelli che eseguono e basta.

   **Temporaneo**, con il banco di prova del robot: si toglie con lui. */
#ifdef PANNELLO_BANCO_ROBOT
bool ha_chiama_con_risposta(const char *dominio, const char *servizio,
                            const char *entita, const char *dati_json)
{
    if (stato != HA_PRONTO || !dominio || !servizio || !entita) return false;

    static char msg[512];
    const int n = snprintf(msg, sizeof msg,
        "{\"id\":%" PRIu32 ",\"type\":\"call_service\","
        "\"domain\":\"%s\",\"service\":\"%s\","
        "\"return_response\":true,"
        "\"target\":{\"entity_id\":\"%s\"}%s%s%s}",
        id_comando++, dominio, servizio, entita,
        dati_json && *dati_json ? ",\"service_data\":{" : "",
        dati_json && *dati_json ? dati_json : "",
        dati_json && *dati_json ? "}" : "");

    if (n < 0 || (size_t)n >= sizeof msg) return false;
    return ws_manda(msg);
}
#endif

bool ha_chiama(const char *dominio, const char *servizio, const char *entita,
               const char *dati_json)
{
    /* La risposta immediata quando non si e collegati e il criterio di
       11-collaudo.md: un comando mandato mentre il collegamento cade deve
       produrre "non riuscito", non un'attesa che non finisce. */
    if (stato != HA_PRONTO || !dominio || !servizio || !entita) {
        falliti++;
        return false;
    }

    static char msg[512];
    const int n = snprintf(msg, sizeof msg,
        "{\"id\":%" PRIu32 ",\"type\":\"call_service\","
        "\"domain\":\"%s\",\"service\":\"%s\","
        "\"target\":{\"entity_id\":\"%s\"}%s%s%s}",
        id_comando++, dominio, servizio, entita,
        dati_json && *dati_json ? ",\"service_data\":{" : "",
        dati_json && *dati_json ? dati_json : "",
        dati_json && *dati_json ? "}" : "");

    if (n < 0 || (size_t)n >= sizeof msg) { falliti++; return false; }
    if (!ws_manda(msg)) { falliti++; return false; }
    inviati++;
    return true;
}

/* --- messaggi in entrata ------------------------------------------------- */

/* Uno stato come lo manda Home Assistant: entity_id, state, attributes. */
static void prendi_stato(const cJSON *s)
{
    if (!cJSON_IsObject(s)) return;
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(s, "entity_id");
    const cJSON *st = cJSON_GetObjectItemCaseSensitive(s, "state");
    if (!cJSON_IsString(id) || !cJSON_IsString(st)) return;

    /* Gli attributi si serializzano solo per le entita che seguiamo: farlo
       per tutte vorrebbe dire stampare qualche migliaio di oggetti JSON a
       ogni fotografia, e buttarli via subito dopo. */
    if (!ent_seguita_e(id->valuestring)) return;

    const cJSON *attr = cJSON_GetObjectItemCaseSensitive(s, "attributes");
    char *testo = attr ? cJSON_PrintUnformatted(attr) : NULL;
    ent_aggiorna(id->valuestring, st->valuestring, testo, adesso);
    if (testo) cJSON_free(testo);
}

void ha_messaggio(const char *json, uint32_t adesso_ms)
{
    adesso = adesso_ms;
    cJSON *m = cJSON_Parse(json);
    if (!cJSON_IsObject(m)) { cJSON_Delete(m); return; }

    const cJSON *tipo = cJSON_GetObjectItemCaseSensitive(m, "type");
    const char *t = cJSON_IsString(tipo) ? tipo->valuestring : "";

    /* Qualunque cosa arrivi conta come segno di vita: anche un result che
       non ci interessa dice che il collegamento respira. */
    ultimo_dato_ms = adesso_ms;

    if (strcmp(t, "auth_required") == 0) {
        stato = HA_AUTENTICO;
        manda_token();
    } else if (strcmp(t, "auth_ok") == 0) {
        motivo[0] = 0;
        chiedi_fotografia();
    } else if (strcmp(t, "auth_invalid") == 0) {
        /* Qui ci si ferma, e non e pigrizia: riprovare ogni quindici
           secondi riempirebbe il registro di Home Assistant di tentativi
           falliti senza mai dire all'utente cosa c'e che non va. */
        const cJSON *msg = cJSON_GetObjectItemCaseSensitive(m, "message");
        snprintf(motivo, sizeof motivo,
                 "token rifiutato da Home Assistant%s%s",
                 cJSON_IsString(msg) ? ": " : "",
                 cJSON_IsString(msg) ? msg->valuestring : "");
        ws_chiudi();
        ent_dimentica_valori();
        stato = HA_TOKEN_RIFIUTATO;
    } else if (strcmp(t, "result") == 0) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(m, "id");
        const cJSON *ok = cJSON_GetObjectItemCaseSensitive(m, "success");
        const bool riuscito = cJSON_IsBool(ok) ? cJSON_IsTrue(ok) : true;

#ifdef PANNELLO_BANCO_ROBOT
        /* --- l'ultima risposta a un comando, per poterla guardare --------
         *
         * Un `call_service` torna indietro con l'esito e, per i servizi che
         * ne hanno una, con la risposta del dispositivo. Di solito la si
         * butta: ha_chiama() sa solo se il messaggio e partito.
         *
         * Serve al banco, per chiedere al robot la sua mappa delle stanze
         * invece di indovinarne i numeri mandandolo in giro per casa. Sta
         * dietro la stessa -D perche costa: stampa il JSON intero di **ogni**
         * risposta a comando dentro un buffer, e senza il banco quel testo
         * non lo legge nessuno. */
        if (cJSON_IsNumber(id) && (int)id->valuedouble >= ID_PRIMO_COMANDO) {
            char *testo = cJSON_PrintUnformatted(m);
            if (testo) {
                snprintf(ultima_risposta, sizeof ultima_risposta, "%s", testo);
                cJSON_free(testo);
            }
        }
#endif

        if (cJSON_IsNumber(id) && id_stanze &&
            (uint32_t)id->valuedouble == id_stanze) {
            if (riuscito)
                prendi_stanze(cJSON_GetObjectItemCaseSensitive(m, "result"));
            else
                /* Nessuna integrazione Roborock, o un servizio che non
                   risponde: la pagina torna a un campo numerico e lo dice.
                   Si smette di chiedere invece di riprovare a ogni
                   apertura. */
                stanze_rifiutate = true;
        } else if (cJSON_IsNumber(id) && id_storico &&
                   (uint32_t)id->valuedouble == id_storico) {
            if (riuscito)
                prendi_storico(cJSON_GetObjectItemCaseSensitive(m, "result"));
            else {
                /* Home Assistant senza recorder, o troppo vecchio per questo
                   comando. Si dice invece di riprovare all'infinito: un
                   grafico vuoto con un motivo e meglio di uno vuoto e
                   basta. */
                storico_pronto = true;
                storico_vuoto = true;
                registro_aggiungi(LOG_AVVISO, "energia",
                                  "statistics_during_period rifiutato: "
                                  "recorder assente?");
            }
        } else if (cJSON_IsNumber(id) && id_elenco &&
                   (uint32_t)id->valuedouble == id_elenco) {
            /* Qui non c'e ancora l'elenco: `render_template` risponde prima
               con «sottoscritto» e poi manda il risultato come **evento**.
               Questo ramo serve al solo caso in cui il modello non sia
               stato accettato — un Home Assistant che non conosce il
               comando, o un modello che non gli piace.

               La pagina torna a campi di testo, che e come stava prima, e
               lo **dice**: senza, chi guarda un menu vuoto conclude che la
               casa non ha entita. Si smette di chiedere invece di riprovare
               a ogni apertura della pagina. */
            if (!riuscito) {
                elenco_rifiutato = true;
                id_elenco = 0;
                registro_aggiungi(LOG_AVVISO, "ha",
                                  "elenco delle entita rifiutato: "
                                  "render_template non accettato");
            }
        } else if (cJSON_IsNumber(id) && (int)id->valuedouble == ID_FOTOGRAFIA
            && !riuscito) {
            /* La sottoscrizione filtrata non e stata accettata: succede se
               Home Assistant e piu vecchio di subscribe_entities. Lo si
               dice, invece di restare ad aspettare una fotografia che non
               arrivera mai. */
            cade("subscribe_entities rifiutato: Home Assistant troppo vecchio?");
        } else if (!riuscito && cJSON_IsNumber(id)
                   && (int)id->valuedouble >= ID_PRIMO_COMANDO) {
            /* Un comando che Home Assistant rifiuta: si conta, perche e
               diverso da uno che non e nemmeno partito. */
            falliti++;
        }
    } else if (strcmp(t, "event") == 0) {
        const cJSON *e = cJSON_GetObjectItemCaseSensitive(m, "event");

        /* Il risultato del modello che elenca le entita. Si riconosce
           dall'identificatore e non dalla forma, al contrario dei due qui
           sotto: un evento con dentro `result` e' esattamente quello che
           manda anche `render_template` di chiunque altro, e questo
           collegamento un giorno potrebbe averne due. */
        const cJSON *id_e = cJSON_GetObjectItemCaseSensitive(m, "id");
        if (id_elenco && cJSON_IsNumber(id_e)
            && (uint32_t)id_e->valuedouble == id_elenco) {
            prendi_elenco(cJSON_GetObjectItemCaseSensitive(e, "result"));
            /* Preso quello che serviva, si chiude: da qui in poi ogni
               lampadina accesa in casa ne farebbe arrivare un altro. */
            basta_elenco();
            cJSON_Delete(m);
            return;
        }

        /* Due sorgenti di eventi sullo stesso collegamento, e si
           distinguono da cosa portano dentro: `data.new_state` viene da
           state_changed, `a` dalla fotografia filtrata. Distinguerle
           dall'identificatore sarebbe piu fragile — gli identificatori si
           riusano, le forme no. */
        const cJSON *aggiunte = cJSON_GetObjectItemCaseSensitive(e, "a");
        if (cJSON_IsObject(aggiunte)) {
            prendi_fotografia(aggiunte);
            if (stato == HA_ALLINEO) {
                stato = HA_PRONTO;
                basta_fotografia();
            }
        } else {
            const cJSON *d = cJSON_GetObjectItemCaseSensitive(e, "data");
            prendi_stato(cJSON_GetObjectItemCaseSensitive(d, "new_state"));
        }
    }

    cJSON_Delete(m);
}

/* --- il giro ------------------------------------------------------------ */

void ha_gira(uint32_t adesso_ms)
{
    adesso = adesso_ms;

    /* Un token rifiutato non si riprova da solo: aspetta un gesto. */
    if (stato == HA_TOKEN_RIFIUTATO || stato == HA_SPENTO) return;

    if (stato == HA_CADUTO) {
        if ((int32_t)(adesso_ms - prossimo_tentativo_ms) >= 0) apri();
        return;
    }

    ws_gira();

    /* Si svuota **prima** quello che e arrivato, e solo dopo si guarda se
       il collegamento e morto. Home Assistant manda `auth_invalid` e poi
       chiude: controllando l'errore per primo si vedrebbe soltanto la
       chiusura, si direbbe "caduto" e si riproverebbe ogni quindici
       secondi — che e esattamente il ciclo che il collaudo vieta. */
    /* Preso dal mucchio e non dichiarato statico. Dichiararlo statico vuol
       dire chiederlo alla RAM **interna**, e un quarto di megabyte li dentro
       non c'e: il collegamento si rompeva in fase di collegamento, con un
       "region dram0_0_seg overflowed" che non nomina ne Home Assistant ne
       il WebSocket. Dal mucchio, sopra i 16 kB, finisce in PSRAM da solo.

       Allocato una volta e tenuto: e il buffer di un collegamento che dura
       quanto il pannello, non di un'operazione che va e viene. */
    static char *msg;
    if (!msg) {
        msg = malloc(WS_MESSAGGIO_MAX + 1);
        if (!msg) { cade("memoria non disponibile"); return; }
    }

    while (ws_ricevi(msg, WS_MESSAGGIO_MAX + 1)) {
        ha_messaggio(msg, adesso_ms);
        if (stato == HA_TOKEN_RIFIUTATO || stato == HA_CADUTO) return;
        ws_gira();
    }

    if (ws_stato() == WS_ERRORE) { cade(ws_motivo()); return; }

    /* --- il battito -----------------------------------------------------
     *
     * L'eta dell'ultimo dato deve misurare se il **collegamento** e vivo,
     * non se in casa e successo qualcosa. Senza questo, una notte tranquilla
     * — nessuna luce accesa, nessuna porta aperta — assomigliava a una
     * caduta: dopo quindici secondi di silenzio il pannello dichiarava
     * "riconnetto" su un collegamento sano.
     *
     * E il mestiere per cui esistono i battiti. Il secondo, che si vede solo
     * quando serve: un socket puo restare aperto dalla nostra parte e morto
     * dall'altra, e finche nessuno ci scrive sopra nessuno se ne accorge.
     *
     * Dieci secondi e comodamente sotto i quindici della prima soglia, e su
     * una rete locale non e traffico. */
    /* Il ritmo del battito ha un orologio **suo**, e questa e la cosa
       importante di tutto il blocco.
       Prima il ping segnava ultimo_dato_ms come se fosse arrivato un dato.
       Serviva a non mandare un ping a ogni giro quando il pong non
       tornava, e funzionava — ma segnando l'ora di quello che **noi**
       mandiamo dentro il contatore di quello che **loro** dicono, l'eta
       dell'ultimo dato non poteva piu superare i dieci secondi. Mai.

       E quel numero e la sola cosa che regge le due soglie di
       sorveglianza.c: quindici secondi per la fascia, sessanta per
       coprire la schermata. Nessuna delle due poteva piu scattare. Un
       Home Assistant che si pianta col socket ancora aperto — il caso per
       cui quelle soglie sono state scritte — lasciava il pannello a
       mostrare valori vecchi di ore, in verde, senza dire niente.

       Due contatori separati e sono di nuovo due domande diverse: da
       quanto non mandiamo un ping, e da quanto non ci risponde nessuno. */
    /* The heartbeat's id comes from the commands, like those of the entity
       list and of the rooms, and for the same reason: Home Assistant wants
       increasing ids on a connection. It was the constant 4, and from the
       second heartbeat on — or from the first, after any command — it came
       back as "id_reuse" instead of a pong. The connection still looked
       alive, since an error is an answer too; but a heartbeat living on
       errors stops working the day Home Assistant closes on whoever repeats
       them. */
    if (stato == HA_PRONTO && adesso_ms - ultimo_ping_ms >= 10000) {
        char battito[48];
        snprintf(battito, sizeof battito,
                 "{\"id\":%" PRIu32 ",\"type\":\"ping\"}", id_comando++);
        ws_manda(battito);
        ultimo_ping_ms = adesso_ms;
    }
}

#ifdef PANNELLO_BANCO_ROBOT
const char *ha_ultima_risposta(void) { return ultima_risposta; }
#endif
