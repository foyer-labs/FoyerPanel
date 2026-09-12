/* ------------------------------------------------------------------------
 * Il registro diagnostico — un anello di righe, le ultime che contano.
 *
 * Fino a ieri la schermata di diagnostica mostrava sei righe scritte a mano
 * nel codice: plausibili, immobili, e mai vere. Sono servite alle catture
 * della Fase 1 e sono rimaste li.
 *
 * --- la regola che governa questo file ----------------------------------
 *
 * 10-diagnostica.md: "Il registro **non contiene mai** token, password,
 * SSID o l'URL completo di una telecamera con le credenziali."
 *
 * Le prime due non ci arrivano: nessuna riga del progetto le scrive, ed e
 * una regola che vale a monte. Il **nome della rete** invece si: lo stampa
 * il driver Wi-Fi di ESP-IDF, con un "connected with <SSID>" che non
 * controlliamo noi. Per questo qui le righe passano da un filtro che lo
 * sostituisce con "(rete)" prima di conservarle.
 *
 * Il filtro toglie quello che si sa di dover togliere. Non e una garanzia
 * contro tutto — nessun filtro lo e — ed e la ragione per cui resta valida
 * la regola di prima: non scrivere segreti nei messaggi, invece di fidarsi
 * che qualcuno li tolga dopo.
 * --------------------------------------------------------------------- */
#include "registro.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "orologio.h"

/* Trentadue righe. La diagnostica ne mostra una schermata e chi indaga
   guarda le ultime: tenerne mille vorrebbe dire spendere in RAM interna —
   che su questo pannello e la risorsa scarsa — per righe che nessuno
   leggera. */
#define RIGHE_MAX   32
#define TESTO_MAX   96
#define SORGENTE_MAX 12

typedef struct {
    char      ora[9];             /* "18:41:07" */
    livello_t livello;
    char      sorgente[SORGENTE_MAX];
    char      testo[TESTO_MAX];
} voce_t;

static voce_t anello[RIGHE_MAX];
static int    quante, prima;      /* prima = la piu vecchia */
static int    errori;
/* Non si azzera con registro_svuota(): dice se questo registro ha mai
   ricevuto qualcosa, non se ha qualcosa adesso. */
static bool   mai = true;

/* --- il filtro --------------------------------------------------------- */

/* Sostituisce ogni occorrenza dell'SSID con "(rete)". Si lavora sulla copia
   gia troncata: quello che non entra nella riga non e un problema di
   riservatezza, e cercare nel testo lungo per poi tagliarlo darebbe un
   messaggio tagliato a meta di una sostituzione. */
static void togli_ssid(char *s)
{
    const char *ssid = cfg_testo("system/network/ssid", "");
    const size_t n = strlen(ssid);
    /* Sotto i tre caratteri non si sostituisce: un SSID di due lettere
       comparirebbe dentro mezzo dizionario, e il registro diventerebbe
       illeggibile per proteggere un nome che l'access point grida
       comunque a chiunque passi. */
    if (n < 3) return;

    char *p;
    while ((p = strstr(s, ssid)) != NULL) {
        static const char sost[] = "(rete)";
        const size_t resto = strlen(p + n);
        memmove(p + sizeof sost - 1, p + n, resto + 1);
        memcpy(p, sost, sizeof sost - 1);
    }
}

void registro_aggiungi(livello_t l, const char *sorgente, const char *testo)
{
    mai = false;
    if (!testo) return;

    const int dove = (prima + quante) % RIGHE_MAX;
    voce_t *v = &anello[dove];

    /* L'ora vera se c'e, altrimenti niente: "01:00:07" su un orologio non
       ancora sincronizzato sarebbe una riga che mente sul quando, ed e
       proprio il quando che si guarda in un registro. */
    if (orologio_valido()) {
        char hhmm[8];
        orologio_ora(hhmm, sizeof hhmm);
        snprintf(v->ora, sizeof v->ora, "%s", hhmm);
    } else {
        snprintf(v->ora, sizeof v->ora, "--:--");
    }

    v->livello = l;
    snprintf(v->sorgente, sizeof v->sorgente, "%s", sorgente ? sorgente : "?");
    snprintf(v->testo, sizeof v->testo, "%s", testo);
    togli_ssid(v->testo);

    if (quante < RIGHE_MAX) quante++;
    else                    prima = (prima + 1) % RIGHE_MAX;

    if (l == LOG_ERRORE && errori < 9999) errori++;
}

/* --- lettura ------------------------------------------------------------ */

/* Dalla piu recente: chi apre la diagnostica vuole sapere cosa e appena
   successo, non com'e andato l'avvio di tre ore fa. */
int registro_righe(void) { return quante; }

const riga_log_t *registro_riga(int n)
{
    if (n < 0 || n >= quante) return NULL;

    static riga_log_t fuori;
    const voce_t *v = &anello[(prima + quante - 1 - n) % RIGHE_MAX];
    fuori = (riga_log_t){ .ora = v->ora, .livello = v->livello,
                          .sorgente = v->sorgente, .messaggio = v->testo };
    return &fuori;
}

int registro_errori(void) { return errori; }

bool registro_mai_scritto(void) { return mai; }

void registro_svuota(void)
{
    quante = 0;
    prima  = 0;
    errori = 0;
    /* L'anello non si azzera: le voci vecchie restano in memoria ma non
       sono piu raggiungibili, perche registro_riga() si ferma a `quante`.
       Ripulire trecento strutture per non farsi vedere da nessuno
       sarebbe lavoro speso per un pudore. */
}
