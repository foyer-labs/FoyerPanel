/* ------------------------------------------------------------------------
 * Il robot lavapavimenti — 01-specifica-ui.md §3.9.
 *
 * Una sola entita `vacuum`, e da li viene tutto: lo stato, la batteria,
 * l'errore. I comandi tornano indietro per la stessa strada.
 *
 * **Le stanze pero non vengono da Home Assistant.** Il robot le conosce per
 * numero — 16, 17, 18 — perche' quei numeri se li e dati da solo mentre
 * disegnava la mappa, e nessuno gli ha mai detto che il 17 e la cucina. Home
 * Assistant non lo sa, e non ha modo di saperlo. Quindi il nome, e la
 * posizione in casa, stanno in configurazione: sono le due cose che solo chi
 * abita li puo dire.
 *
 * La posizione e una griglia di sei colonne per quattro righe, e non e un
 * vezzo grafico: e cio che permette di toccare la cucina perche' e in alto a
 * destra come la cucina vera, senza leggere l'etichetta. Su un pannello che
 * si usa passando, leggere e il costo piu alto che si possa chiedere.
 * --------------------------------------------------------------------- */
#include "dati.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "dati_finti.h"
#include "durata.h"
#include "entita.h"
#include "ha.h"
#include "i18n.h"

/* Le stanze si leggono dalla configurazione a ogni richiesta, come fanno le
   zone delle luci: il documento vive quanto il pannello, i puntatori dentro
   sono stabili, e una copia qui sarebbe una seconda verita da tenere
   allineata. */
#define STANZE_MAX 16

static robot_stanza_t stanza_letta;

int dati_robot_stanze(void)
{
    const int n = cfg_quanti("robot/rooms");
    if (n <= 0) return 0;
    return n > STANZE_MAX ? STANZE_MAX : n;
}

const robot_stanza_t *dati_robot_stanza(int n)
{
    if (n < 0 || n >= dati_robot_stanze()) return NULL;

    stanza_letta = (robot_stanza_t){
        .numero = cfg_intero_in("robot/rooms", n, "number", -1),
        .nome   = cfg_testo_in("robot/rooms", n, "name", tr(TX_ROBOT_ROOM)),
        /* Senza posizione la stanza non sparisce: finisce in coda alla
           griglia, dove la mette chi disegna. Una casa configurata a meta
           deve restare comandabile. */
        .col       = (uint8_t)cfg_intero_in("robot/rooms", n, "col", 0),
        .riga      = (uint8_t)cfg_intero_in("robot/rooms", n, "row", 0),
        .larghezza = (uint8_t)cfg_intero_in("robot/rooms", n, "width", 1),
        .altezza   = (uint8_t)cfg_intero_in("robot/rooms", n, "height", 1),
    };
    if (stanza_letta.larghezza < 1) stanza_letta.larghezza = 1;
    if (stanza_letta.altezza < 1)   stanza_letta.altezza = 1;
    return &stanza_letta;
}

/* --- lo stato -----------------------------------------------------------
 *
 * Home Assistant normalizza lo stato di ogni aspirapolvere in poche parole,
 * ed e su quelle che si traduce: non sulla frase di `status`, che ogni
 * costruttore scrive a modo suo e che cambia con l'aggiornamento del
 * firmware del robot. La frase pero si mostra, perche' dice cose che lo
 * stato non dice — «svuotamento del serbatoio», «lavaggio del panno». */
static robot_stato_t da_parola(const char *s)
{
    if (!s || !*s)                   return ROB_IGNOTO;
    if (strcmp(s, "cleaning") == 0)  return ROB_PULISCE;
    if (strcmp(s, "returning") == 0) return ROB_RIENTRA;
    if (strcmp(s, "docked") == 0)    return ROB_ALLA_BASE;
    if (strcmp(s, "paused") == 0)    return ROB_IN_PAUSA;
    if (strcmp(s, "idle") == 0)      return ROB_FERMO;
    if (strcmp(s, "error") == 0)     return ROB_ERRORE;
    return ROB_IGNOTO;
}

static robot_t inventato(void)
{
    return (robot_t){
        .disponibile   = true,
        .stato         = ROB_PULISCE,
        .dettaglio     = "Living room",
        .batteria_c_e  = true,
        .batteria      = 68,
        .in_carica     = false,
    };
}

robot_t dati_robot(void)
{
    robot_t r = { .dettaglio = "" };

    if (!dati_dal_vero()) return inventato();

    const char *e = cfg_testo("robot/entity", "");
    if (!e || !*e || !ent_vista(e) || !ent_disponibile(e)) return r;

    r.disponibile = true;
    r.stato = da_parola(ent_stato(e));

    /* La frase del costruttore, se c'e. Non sostituisce lo stato: lo
       accompagna, e quando manca non manca niente. */
    r.dettaglio = ent_attributo(e, "status", "");

    /* --- la carica, da due strade e in quest'ordine ---------------------
     *
     * Prima il sensore, se la configurazione ne nomina uno: Home Assistant
     * ha spostato le batterie degli aspirapolvere in entita `sensor.`
     * separate, e sulle integrazioni recenti l'attributo non esiste piu.
     * Poi l'attributo, che sulle installazioni piu vecchie c'e ancora.
     *
     * Se non c'e ne l'uno ne l'altro non si inventa niente: la batteria
     * semplicemente non si disegna, come ogni altro dato mancante di questa
     * schermata. */
    double b = -1;

    const char *sb = cfg_testo("robot/battery", "");
    if (sb && *sb && ent_vista(sb) && ent_disponibile(sb))
        b = ent_numero(sb, -1);

    if (b < 0) b = ent_attributo_numero(e, "battery_level", -1);

    if (b >= 0) {
        r.batteria_c_e = true;
        r.batteria = (int8_t)(b > 100 ? 100 : b);
    }
    r.in_carica = r.stato == ROB_ALLA_BASE && r.batteria_c_e
               && r.batteria < 100;

    return r;
}

/* --- i comandi ----------------------------------------------------------
 *
 * Tre servizi e nessuna astrazione in mezzo. `start` e `return_to_base` sono
 * dell'interfaccia comune degli aspirapolvere e valgono per qualunque
 * robot; la pulizia per stanze no — quella e un comando proprietario, e
 * infatti il suo nome sta in configurazione invece che qui.
 *
 * Sui Roborock si chiama `app_segment_clean` e vuole i numeri delle stanze.
 * Se un giorno arrivera un robot che lo chiama in un altro modo, si cambia
 * una riga di config.json e non una di questo file. */
static const char *entita(void)
{
    const char *e = cfg_testo("robot/entity", "");
    return e && *e ? e : NULL;
}

bool dati_robot_comando_stanze_json(const int *numeri, int quanti,
                                    const char *forma_imposta,
                                    char *fuori, size_t max)
{
    if (!numeri || quanti <= 0 || !fuori || max < 32) return false;

    const char *comando = cfg_testo("robot/rooms_command", "app_segment_clean");

    /* --- due modi di impacchettare le stanze, e non e un dettaglio -------
     *
     * `app_segment_clean` vuole i numeri delle stanze, ma il mediatore fra
     * noi e il robot cambia forma a seconda dell'integrazione:
     *
     *     params: [16, 17]                             elenco piatto
     *     params: [{"segments": [16,17], "repeat": 1}]  oggetto
     *
     * La prima e della vecchia Xiaomi Miio, la seconda dell'integrazione
     * Roborock nativa. Mandando quella sbagliata il comando viene
     * **accettato** e la lista di stanze arriva vuota: il robot esce dalla
     * base, annuncia la pulizia, non trova niente da fare e rientra. Un
     * guasto che non da nessun errore da nessuna parte, e che si presenta
     * come un robot capriccioso.
     *
     * Non si indovina: sta in configurazione accanto al nome del comando,
     * e il valore di riposo e l'oggetto perche' e quello che Home Assistant
     * installa oggi.
     *
     * Sedici numeri da tre cifre piu le virgole stanno abbondantemente qui
     * dentro; il troncamento si controlla lo stesso, perche' un JSON tagliato
     * a meta e un comando che il server rifiuta senza dire perche'. */
    /* La forma puo essere imposta da fuori: serve al banco di prova, che
       la cambia senza toccare la configurazione — provarne una per volta e
       l'unico modo di sapere quale delle due parla la lingua giusta. */
    const char *forma = forma_imposta && *forma_imposta
                      ? forma_imposta
                      : cfg_testo("robot/rooms_format", "segments");
    /* --- misurato sul robot vero, guardandone lo stato secondo per secondo
     *
     * Le due forme **non** si comportano allo stesso modo, e per vederlo
     * bisognava guardare l'entita mentre ci provava invece di guardare se
     * il comando veniva accettato. Vengono accettate tutte e due.
     *
     *   params: [20]                                (elenco piatto)
     *     va in «pulisce» entro quattro secondi, ci resta **quindici**, poi
     *     rientra. E il guasto che si vedeva: esce dalla base, annuncia la
     *     pulizia, non trova niente da fare e torna.
     *
     *   params: [{"segments":[20],"repeat":2}]      (oggetto)
     *     resta alla base **cinquanta secondi** senza dare segno di vita,
     *     poi parte e continua a pulire.
     *
     * Quei cinquanta secondi sono la ragione per cui questa forma sembrava
     * rotta: chi manda il comando e guarda il robot conclude in venti
     * secondi che non e successo niente, e quella conclusione e sbagliata
     * solo perche arriva troppo presto.
     *
     * Resta da confermare **quale stanza** pulisce davvero: lo stato dice
     * che sta pulendo, non dove. Quello si legge dall'applicazione.
     *
     * Il valore di riposo e l'oggetto, che e la forma dell'integrazione
     * Roborock nativa ed e l'unica delle due che sul ferro non si ferma. */
    const bool oggetto = strcmp(forma, "list") != 0;
    int ripeti = (int)cfg_intero("robot/repeats", 1);
    if (ripeti < 1) ripeti = 1;
    if (ripeti > 3) ripeti = 3;

    char *dati = fuori;
    const size_t sizeof_dati = max;
    int n = snprintf(dati, sizeof_dati, "\"command\":\"%s\",\"params\":[%s",
                     comando, oggetto ? "{\"segments\":[" : "");

    for (int i = 0; i < quanti && n > 0 && (size_t)n < sizeof_dati; i++)
        n += snprintf(dati + n, sizeof_dati - (size_t)n, "%s%d",
                      i ? "," : "", numeri[i]);
    if (n < 0 || (size_t)n >= sizeof_dati) return false;

    n += snprintf(dati + n, sizeof_dati - (size_t)n, "%s",
                  oggetto ? "" : "]");
    if (oggetto && n > 0 && (size_t)n < sizeof_dati)
        n += snprintf(dati + n, sizeof_dati - (size_t)n,
                      "],\"repeat\":%d}]", ripeti);
    if (n < 0 || (size_t)n >= sizeof_dati) return false;

    return true;
}

/* Manda un blocco di service_data gia composto. Serve al banco di prova,
   che compone con una forma imposta e poi vuole spedire proprio quella:
   ricomporre dentro dati_robot_pulisci() vorrebbe dire mandare una cosa
   diversa da quella mostrata, e il banco perderebbe il suo unico scopo. */
bool dati_robot_manda_json(const char *service_data)
{
    const char *e = entita();
    return e && service_data && *service_data
        && ha_chiama("vacuum", "send_command", e, service_data);
}

bool dati_robot_pulisci(const int *numeri, int quanti)
{
    const char *e = entita();
    if (!e) return false;

    char dati[288];
    if (!dati_robot_comando_stanze_json(numeri, quanti, NULL, dati, sizeof dati))
        return false;

    return ha_chiama("vacuum", "send_command", e, dati);
}

bool dati_robot_tutto(void)
{
    const char *e = entita();
    return e && ha_chiama("vacuum", "start", e, NULL);
}

bool dati_robot_alla_base(void)
{
    const char *e = entita();
    return e && ha_chiama("vacuum", "return_to_base", e, NULL);
}

/* --- lavatrice e asciugatrice -------------------------------------------
 *
 * Stanno qui e non in un file loro perche sono tre righe di lettura e
 * nessuna logica: un file nuovo costerebbe piu di quello che separa.
 *
 * Il ciclo lo dice l'entita dichiarata, non i watt. Una lavatrice in ammollo
 * assorbe quanto una spenta, e dedurre il ciclo dall'assorbimento vorrebbe
 * dire annunciare «finita» a meta lavaggio — che e il genere di errore che
 * fa aprire l'oblo con dentro l'acqua. */
#define ELETTRO_MAX 2

static elettrodomestico_t elettro[ELETTRO_MAX];
static int n_elettro;

/* I valori del mockup, per quando non c'e nessuna casa che parli: uno che
   lavora e uno fermo, che sono i due stati che la scheda deve saper
   disegnare. Sul pannello vero non si vedono mai. */
static void elettrodomestici_finti(void)
{
    static const struct { const char *nome; bool ciclo; int32_t w; uint8_t q; } F[] = {
        { "Lavatrice",    true,  1850, 84 },
        { "Asciugatrice", false,    0,  0 },
    };
    for (int n = 0; n < n_elettro; n++) {
        const int i = n % 2;
        elettro[n].disponibile = true;
        elettro[n].in_ciclo    = F[i].ciclo;
        elettro[n].potenza_c_e = true;
        elettro[n].watt        = F[i].w;
        elettro[n].quota_pct   = F[i].q;
    }
}

static void leggi_elettrodomestici(void)
{
    n_elettro = cfg_quanti("appliances");
    if (n_elettro > ELETTRO_MAX) n_elettro = ELETTRO_MAX;

    for (int n = 0; n < n_elettro; n++) {
        elettrodomestico_t *e = &elettro[n];
        e->nome = cfg_testo_in("appliances", n, "name", "");
        e->asciuga = n == 1;   /* la seconda dell'elenco */

        const char *ciclo = cfg_testo_in("appliances", n, "cycle", "");
        e->disponibile = ciclo[0] && ent_vista(ciclo) && ent_disponibile(ciclo);
        e->in_ciclo = e->disponibile && ent_stato_e(ciclo, "on");

        const char *pw = cfg_testo_in("appliances", n, "power", "");
        e->potenza_c_e = pw[0] && ent_vista(pw) && ent_disponibile(pw);
        if (e->potenza_c_e) {
            /* L'unita la dichiara Home Assistant e non si assume mai: un
               sensore in kilowatt letto come watt fa sembrare una lavatrice
               una fonderia. Stessa regola dell'energia. */
            const char *u = ent_attributo(pw, "unit_of_measurement", "W");
            const int fattore = (u[0] == 'k' || u[0] == 'K') ? 1000 : 1;
            e->watt = (int32_t)(ent_numero(pw, 0) * fattore);
        } else {
            e->watt = 0;
        }

        const int32_t massimo =
            cfg_intero_in("appliances", n, "max_power", 0);
        e->quota_pct = 0;
        if (massimo > 0 && e->watt > 0) {
            int32_t q = e->watt * 100 / massimo;
            if (q > 100) q = 100;      /* un picco d'avvio non sfonda la barra */
            e->quota_pct = (uint8_t)q;
        }
    }

    /* Senza una casa collegata restano i valori del mockup: qui non c'e
       niente da leggere, e una scheda vuota non farebbe vedere se il disegno
       funziona. */
    if (!dati_dal_vero()) elettrodomestici_finti();
}

int dati_elettrodomestici(void)
{
    leggi_elettrodomestici();
    return n_elettro;
}

const elettrodomestico_t *dati_elettrodomestico(int n)
{
    if (n < 0 || n >= n_elettro) return NULL;
    return &elettro[n];
}

/* --- avvisi e manutenzione — 01-specifica-ui.md §3.9 ---------------------
 *
 * Vedi dati.h per il perche' le frasi non le scrive il pannello. Qui c'e
 * la meccanica, e due decisioni che vale la pena non dimenticare.
 *
 * **Un'entita `sensor.` porta il messaggio nel proprio stato**, e allora il
 * testo configurato non serve: si mostra quello che dice il robot. Un
 * `binary_sensor.` no — «on» non e una frase — e allora il testo serve, e
 * senza si ripiega sul nome amichevole dell'entita, che almeno dice di
 * cosa si sta parlando.
 *
 * **Quali stati vogliono dire «niente da segnalare»**: un sensore d'errore
 * a riposo non e vuoto, dice `none`. Se quella parola non fosse
 * nell'elenco, il pannello mostrerebbe l'avviso «none» per sempre — che e
 * peggio di nessun avviso, perche' insegna a ignorare il triangolo.
 */

#define AVVISI_MAX 12
#define MANUTENZIONI_MAX 12

static robot_avviso_t       avviso_letto;
static robot_manutenzione_t manutenzione_letta;

static bool stato_muto(const char *s)
{
    static const char *const MUTI[] = {
        "", "none", "no_error", "ok", "off", "unknown", "unavailable",
        "nessuno", "clear", "idle",
    };
    for (unsigned n = 0; n < sizeof MUTI / sizeof MUTI[0]; n++)
        if (strcmp(s, MUTI[n]) == 0) return true;
    return false;
}

static bool e_due_stati(const char *entita)
{
    return strncmp(entita, "binary_sensor.", 14) == 0;
}

/* --- i valori del mockup, per quando non c'e nessuna casa che parli ------
 *
 * Uno acceso con la frase che il robot dice davvero, e due a posto: sono i
 * tre casi che la riga deve saper disegnare. Il primo e il messaggio vero
 * di un S7 MaxV Ultra — riportato, non inventato — perche' una schermata
 * di prova con dentro «Avviso 1» non fa vedere quanto e lunga una frase
 * vera, che e la sola cosa che quella schermata deve reggere. */
static const struct { const char *testo; bool attivo; } AVVISI_FINTI[] = {
    { "The clean-water Hall sensor was triggered, check the clean-water "
      "tank", true },
    { "Dirty-water tank to empty", false },
    { "Mop attached", false },
};

static const struct { const char *nome, *valore; bool poco; } MANUT_FINTE[] = {
    { "Main brush",          "214 h", false },
    { "Side brush",          "96 h",  false },
    { "Filter",              "8 h",   true  },
    { "Sensors",             "22 h",  false },
};

int dati_robot_avvisi(void)
{
    if (!dati_dal_vero())
        return (int)(sizeof AVVISI_FINTI / sizeof AVVISI_FINTI[0]);

    const int n = cfg_quanti("robot/alerts");
    if (n <= 0) return 0;
    return n > AVVISI_MAX ? AVVISI_MAX : n;
}

const robot_avviso_t *dati_robot_avviso(int n)
{
    if (n < 0 || n >= dati_robot_avvisi()) return NULL;

    if (!dati_dal_vero()) {
        avviso_letto = (robot_avviso_t){
            .testo = AVVISI_FINTI[n].testo,
            .attivo = AVVISI_FINTI[n].attivo,
            .disponibile = true,
        };
        return &avviso_letto;
    }

    const char *e = cfg_testo_in("robot/alerts", n, "entity", "");
    const char *testo = cfg_testo_in("robot/alerts", n, "text", "");
    const char *stato = ent_stato(e);

    avviso_letto = (robot_avviso_t){
        .testo = "", .attivo = false, .disponibile = ent_disponibile(e),
    };
    if (!*e) return &avviso_letto;

    if (e_due_stati(e)) {
        /* Il verso lo dice la configurazione, e senza vale «acceso»: e il
           caso comune, e l'altro si scrive perche' e l'eccezione. */
        const bool su_acceso = strcmp(
            cfg_testo_in("robot/alerts", n, "when", "on"), "off") != 0;
        avviso_letto.attivo = avviso_letto.disponibile
                           && (strcmp(stato, "on") == 0) == su_acceso;
        avviso_letto.testo = *testo ? testo
                           : ent_attributo(e, "friendly_name", tr(TX_ROBOT_TO_CHECK));
    } else {
        avviso_letto.attivo = avviso_letto.disponibile && !stato_muto(stato);
        /* Il messaggio del robot vince sul testo configurato: se un giorno
           dira una cosa piu precisa, il pannello la dira con lui. */
        avviso_letto.testo = avviso_letto.attivo ? stato
                           : (*testo ? testo : "");
    }
    return &avviso_letto;
}

int dati_robot_avvisi_accesi(void)
{
    int quanti = 0;
    for (int n = 0; n < dati_robot_avvisi(); n++) {
        const robot_avviso_t *a = dati_robot_avviso(n);
        if (a && a->attivo) quanti++;
    }
    return quanti;
}

int dati_robot_manutenzioni(void)
{
    if (!dati_dal_vero())
        return (int)(sizeof MANUT_FINTE / sizeof MANUT_FINTE[0]);

    const int n = cfg_quanti("robot/maintenance");
    if (n <= 0) return 0;
    return n > MANUTENZIONI_MAX ? MANUTENZIONI_MAX : n;
}

const robot_manutenzione_t *dati_robot_manutenzione(int n)
{
    if (n < 0 || n >= dati_robot_manutenzioni()) return NULL;

    if (!dati_dal_vero()) {
        manutenzione_letta = (robot_manutenzione_t){
            .nome = MANUT_FINTE[n].nome,
            .valore = MANUT_FINTE[n].valore,
            .agli_sgoccioli = MANUT_FINTE[n].poco,
            .disponibile = true,
        };
        return &manutenzione_letta;
    }

    const char *e = cfg_testo_in("robot/maintenance", n, "entity", "");
    const char *nome = cfg_testo_in("robot/maintenance", n, "name", "");
    const int32_t soglia = cfg_intero_in("robot/maintenance", n, "threshold", -1);

    /* --- tutto in ore, qualunque cosa dica il sensore -------------------
     *
     * I consumabili di un robot non hanno tutti la stessa unita: alcuni
     * danno le ore, altri i secondi. Mostrarli come arrivano metteva «214 h»
     * accanto a «770400 s» nella stessa schermata — lo stesso dato in due
     * lingue, e a un metro e mezzo da un vetro non si converte a mente.
     *
     * La conversione sta in app/durata.c, che non dipende da niente e ha una
     * prova sopra: qui si chiama e basta. Quello che non e un tempo — una
     * percentuale, un conteggio — resta com'e: inventare le ore da una
     * percentuale darebbe un numero plausibile e falso.
     *
     * Otto buffer a giro perche' una schermata ne mostra piu d'uno, e
     * restituire sempre lo stesso li farebbe tutti uguali. */
    static char valori[8][24];
    static uint8_t giro;
    char *b = valori[giro++ % 8];

    const char *stato = ent_stato(e);
    const char *unita = ent_attributo(e, "unit_of_measurement", "");
    durata_testo(stato, unita, b, sizeof valori[0]);

    manutenzione_letta = (robot_manutenzione_t){
        .nome = *nome ? nome : ent_attributo(e, "friendly_name", tr(TX_ROBOT_MAINTENANCE_ITEM)),
        .valore = b,
        .disponibile = ent_disponibile(e),
        .agli_sgoccioli = false,
    };

    if (manutenzione_letta.disponibile && soglia >= 0) {
        /* La soglia si confronta **in ore** quando il sensore e un tempo, e
           nella sua unita quando non lo e. E l'unico modo perche' un dieci
           scritto in configurazione voglia dire la stessa cosa sui sensori
           in ore e su quelli in secondi — che e cio che chi configura
           intende, e non ha modo di sapere quale sia quale. */
        double v = 0;
        bool numero = durata_in_ore(stato, unita, &v);
        if (!numero) {
            char *fine = NULL;
            v = strtod(stato, &fine);
            /* `fine != stato` distingue «zero» da «non e un numero»: uno
               stato come "unknown" darebbe zero, e zero e proprio il valore
               che fa scattare l'allarme. */
            numero = fine != stato;
        }
        if (numero) manutenzione_letta.agli_sgoccioli = v <= (double)soglia;
    }
    return &manutenzione_letta;
}

int dati_robot_manutenzioni_agli_sgoccioli(void)
{
    int quanti = 0;
    for (int n = 0; n < dati_robot_manutenzioni(); n++) {
        const robot_manutenzione_t *m = dati_robot_manutenzione(n);
        if (m && m->agli_sgoccioli) quanti++;
    }
    return quanti;
}
