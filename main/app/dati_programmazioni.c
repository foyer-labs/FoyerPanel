/* ------------------------------------------------------------------------
 * Programmazioni — le cose che vanno a orario.
 *
 * Rispecchia la logica che c'e gia in Home Assistant, e non ne inventa una
 * seconda. Li una scheda tiene insieme il dispositivo, gli `input_datetime`
 * degli orari e le automazioni che li fanno scattare; qui un gruppo tiene
 * insieme le stesse cose, e i comandi che manda sono quelli che si
 * manderebbero da quella scheda.
 *
 * **Gli orari non si copiano.** Vivono negli `input_datetime`: il pannello
 * li legge da li e ce li riscrive. Tenerne una copia vorrebbe dire due idee
 * di quando si accende la pompa, e quella sbagliata sarebbe sempre quella
 * che si guarda.
 *
 * **Il pannello non fa scattare niente.** Le automazioni le esegue Home
 * Assistant; da qui si possono solo abilitare e disabilitare. Un pannello
 * che facesse partire l'automazione al posto suo sarebbe un secondo posto
 * da cui la casa si comanda, e il primo giorno che i due non sono
 * d'accordo nessuno saprebbe quale guardare.
 * --------------------------------------------------------------------- */
#include "dati.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "dati_finti.h"
#include "entita.h"
#include "ha.h"
#include "i18n.h"
#include "orologio.h"

#define GRUPPI_MAX      12
#define FINESTRE_MAX     4
#define AUTOMAZIONI_MAX 12

static programmazione_t gruppi[GRUPPI_MAX];
static finestra_t       finestre[GRUPPI_MAX][FINESTRE_MAX];
static automazione_t    automazioni[GRUPPI_MAX][AUTOMAZIONI_MAX];
static int              quanti;

/* Il dominio, cioe cio che sta prima del punto. Zero se quel punto non
   c'e: un identificatore senza dominio non e comandabile, e la riga lo dira
   invece di mandare un servizio inventato. */
static size_t dominio(const char *entita, char *buf, size_t n)
{
    const char *punto = entita ? strchr(entita, '.') : NULL;
    if (!punto || punto == entita) return 0;

    const size_t l = (size_t)(punto - entita);
    if (l >= n) return 0;
    memcpy(buf, entita, l);
    buf[l] = 0;
    return l;
}

/* --- da "07:30:00" a 450 -------------------------------------------------
 *
 * Lo stato di un `input_datetime` con `has_date: false` e l'ora in lettere.
 * I secondi ci sono e non servono: questi orari fanno scattare automazioni
 * al minuto, e un pannello che mostrasse 07:30:00 direbbe una precisione
 * che non c'e.
 *
 * Si legge lo **stato** e non gli attributi `hour`/`minute`: gli attributi
 * ci sono, ma lo stato c'e sempre — anche quando l'entita arriva da una
 * sottoscrizione che gli attributi non li ha ancora portati. */
static int16_t minuti_da_stato(const char *entita)
{
    if (!entita || !entita[0] || !ent_vista(entita)) return ORARIO_IGNOTO;
    if (!ent_disponibile(entita)) return ORARIO_IGNOTO;

    const char *s = ent_stato(entita);
    if (!s || !s[0]) return ORARIO_IGNOTO;

    /* "HH:MM" o "HH:MM:SS": si prendono i primi due numeri e si pretende
       che siano tali. Un `input_datetime` con la data dentro — has_date
       vero — arriva come "2026-09-08 07:30:00" e qui non passa: e giusto,
       perche il pannello scrive il solo campo `time` e su quello non
       funzionerebbe. */
    char *fine = NULL;
    const long ore = strtol(s, &fine, 10);
    if (!fine || *fine != ':' || ore < 0 || ore > 23) return ORARIO_IGNOTO;
    const long min = strtol(fine + 1, NULL, 10);
    if (min < 0 || min > 59) return ORARIO_IGNOTO;

    return (int16_t)(ore * 60 + min);
}

/* Siamo dentro la finestra, adesso?
 *
 * Una finestra che scavalca la mezzanotte — 22:00 → 06:00, il corridoio — non e
 * un caso strano da gestire dopo: e' proprio quella che c'e in casa. Con gli
 * estremi al contrario si guarda **fuori** invece che dentro, ed e' la
 * stessa condizione che scriverebbe Home Assistant. */
static bool dentro_adesso(int16_t da, int16_t a, int16_t ora)
{
    if (da == ORARIO_IGNOTO || a == ORARIO_IGNOTO) return false;
    if (da == a) return false;
    return da < a ? (ora >= da && ora < a) : (ora >= da || ora < a);
}

/* Quando e scattata l'ultima volta, in parole.
 *
 * `last_triggered` e ISO in UTC, come `finishes_at` dei timer, e si tratta
 * allo stesso modo: si converte in locale e si dice l'ora. Piu vecchio di
 * ieri non si scrive la data — «tre settimane fa» e un'informazione, «il 14
 * agosto alle 06:00» e un dato che nessuno confronta con niente. */
static const char *ultimo_scatto(const char *entita, char *buf, size_t n)
{
    const char *iso = ent_attributo(entita, "last_triggered", "");
    if (!iso || !iso[0]) return NULL;

    const long long quando = orologio_da_iso(iso);
    const long long adesso = orologio_adesso_utc();
    if (quando <= 0 || adesso <= 0) return NULL;

    const long long fa = adesso - quando;
    if (fa < 0) return NULL;

    char ora[8];
    if (fa < 48LL * 3600 && !orologio_ora_locale(quando, ora, sizeof ora))
        return NULL;

    if (fa < 24LL * 3600)      snprintf(buf, n, "%s", ora);
    else if (fa < 48LL * 3600) snprintf(buf, n, tr(TX_TIME_YESTERDAY_AT), ora);
    else {
        const int giorni = (int)(fa / 86400);
        snprintf(buf, n, trn(TXN_TIME_DAYS_AGO, giorni), giorni);
    }
    return buf;
}

/* --- i valori inventati, per quando non c'e nessuna casa che parli -------
 *
 * Servono alle catture e a guardare la schermata prima di appenderla al
 * muro. Sul pannello vero non si vedono mai: appena Home Assistant risponde
 * vale quello che dice lui. */
static void programmazioni_finte(void)
{
    static const int16_t ORARI[][2] = {
        { 8 * 60, 22 * 60 + 30 }, { 6 * 60 + 30, 8 * 60 },
        { 18 * 60 + 30, 23 * 60 + 30 }, { 9 * 60, 13 * 60 },
        { 17 * 60, 23 * 60 }, { 22 * 60, 6 * 60 },
    };
    static const int N = (int)(sizeof ORARI / sizeof ORARI[0]);

    for (int g = 0; g < quanti; g++) {
        gruppi[g].disponibile = true;
        gruppi[g].acceso = (g % 3) == 1;
        if (gruppi[g].potenza_c_e) gruppi[g].watt = gruppi[g].acceso ? 148 : 0;

        for (int f = 0; f < gruppi[g].finestre; f++) {
            const int i = (g + f) % N;
            finestre[g][f].da_min = ORARI[i][0];
            finestre[g][f].a_min  = ORARI[i][1];
            finestre[g][f].in_corso = gruppi[g].acceso && f == 0;
        }
        for (int a = 0; a < gruppi[g].automazioni; a++) {
            automazioni[g][a].attiva = !(g == 2 && a == 4);
            automazioni[g][a].disponibile = true;
            /* "yesterday 07:00" in the panel's language, written when
               the fake data are built — the same sentence the real
               provider writes in ultimo_scatto(). */
            static char ieri[24];
            snprintf(ieri, sizeof ieri, tr(TX_TIME_YESTERDAY_AT), "07:00");
            automazioni[g][a].ultimo_scatto = a % 3 == 0 ? "18:30"
                                            : a % 3 == 1 ? ieri : NULL;
        }
    }
}

static void leggi(void)
{
    quanti = cfg_quanti("schedules");
    if (quanti > GRUPPI_MAX) quanti = GRUPPI_MAX;

    /* -1 quando l'ora non e credibile: un pannello appena acceso crede
       che sia il 1970, e una finestra «in corso» calcolata su quello
       colorerebbe di ambra la riga sbagliata. */
    const int16_t adesso = (int16_t)orologio_minuti();

    for (int g = 0; g < quanti; g++) {
        char base[40];
        snprintf(base, sizeof base, "schedules/%d", g);

        programmazione_t *p = &gruppi[g];
        p->nome  = cfg_testo_in("schedules", g, "name", "");
        p->icona = cfg_testo_in("schedules", g, "icon", "");

        p->entita = cfg_testo_in("schedules", g, "entity", "");
        p->c_e_entita = p->entita && p->entita[0];
        p->disponibile = p->c_e_entita && ent_vista(p->entita)
                         && ent_disponibile(p->entita);
        p->acceso = p->disponibile && ent_stato_e(p->entita, "on");

        const char *pw = cfg_testo_in("schedules", g, "power", "");
        p->potenza_c_e = pw[0] && ent_vista(pw) && ent_disponibile(pw);
        if (p->potenza_c_e) {
            /* L'unita la dichiara Home Assistant e non si assume mai: un
               sensore in kilowatt letto come watt fa sembrare una pompa una
               fonderia. Stessa regola dell'energia e degli interruttori. */
            const char *u = ent_attributo(pw, "unit_of_measurement", "W");
            const int fattore = (u[0] == 'k' || u[0] == 'K') ? 1000 : 1;
            p->watt = (int32_t)(ent_numero(pw, 0) * fattore);
        } else {
            p->watt = 0;
        }

        char dove[64];
        snprintf(dove, sizeof dove, "%s/windows", base);
        p->finestre = cfg_quanti(dove);
        if (p->finestre > FINESTRE_MAX) p->finestre = FINESTRE_MAX;

        for (int f = 0; f < p->finestre; f++) {
            finestra_t *w = &finestre[g][f];
            char qui[96];
            snprintf(qui, sizeof qui, "%s/%d/on_time", dove, f);
            w->accensione = cfg_testo(qui, "");
            snprintf(qui, sizeof qui, "%s/%d/off_time", dove, f);
            w->spegnimento = cfg_testo(qui, "");
            snprintf(qui, sizeof qui, "%s/%d/validity", dove, f);
            w->validita = cfg_vero(qui, false);

            w->da_min = minuti_da_stato(w->accensione);
            w->a_min  = minuti_da_stato(w->spegnimento);
            w->in_corso = dentro_adesso(w->da_min, w->a_min, adesso);
        }

        snprintf(dove, sizeof dove, "%s/automations", base);
        p->automazioni = cfg_quanti(dove);
        if (p->automazioni > AUTOMAZIONI_MAX) p->automazioni = AUTOMAZIONI_MAX;

        for (int a = 0; a < p->automazioni; a++) {
            automazione_t *u = &automazioni[g][a];
            char qui[96];
            snprintf(qui, sizeof qui, "%s/%d/entity", dove, a);
            u->entita = cfg_testo(qui, "");
            snprintf(qui, sizeof qui, "%s/%d/name", dove, a);
            const char *nome = cfg_testo(qui, "");

            u->disponibile = u->entita[0] && ent_vista(u->entita)
                             && ent_disponibile(u->entita);
            u->attiva = u->disponibile && ent_stato_e(u->entita, "on");

            /* Il nome di configurazione vince, ma solo se c'e: senza, vale
               quello che l'automazione ha in Home Assistant. Cosi
               rinominarla la rinomina anche qui, e non c'e un secondo nome
               da tenere allineato al primo. */
            u->nome = nome[0] ? nome
                    : ent_attributo(u->entita, "friendly_name", u->entita);

            static char quando[GRUPPI_MAX][AUTOMAZIONI_MAX][20];
            u->ultimo_scatto = u->disponibile
                ? ultimo_scatto(u->entita, quando[g][a], sizeof quando[0][0])
                : NULL;
        }
    }

    if (!dati_dal_vero()) programmazioni_finte();
}

int dati_programmazioni(void)
{
    leggi();
    return quanti;
}

const programmazione_t *dati_programmazione(int n)
{
    if (n < 0 || n >= quanti) return NULL;
    return &gruppi[n];
}

const finestra_t *dati_finestra(int gruppo, int n)
{
    const programmazione_t *p = dati_programmazione(gruppo);
    if (!p || n < 0 || n >= p->finestre) return NULL;
    return &finestre[gruppo][n];
}

const automazione_t *dati_automazione(int gruppo, int n)
{
    const programmazione_t *p = dati_programmazione(gruppo);
    if (!p || n < 0 || n >= p->automazioni) return NULL;
    return &automazioni[gruppo][n];
}

bool dati_programmazione_premi(int gruppo, bool acceso)
{
    const programmazione_t *p = dati_programmazione(gruppo);
    if (!p || !p->disponibile) return false;
    if (!dati_dal_vero()) return false;

    char dom[24];
    if (!dominio(p->entita, dom, sizeof dom)) return false;

    return ha_chiama(dom, acceso ? "turn_on" : "turn_off", p->entita, NULL);
}

bool dati_automazione_abilita(int gruppo, int n, bool attiva)
{
    const automazione_t *u = dati_automazione(gruppo, n);
    if (!u || !u->disponibile) return false;
    if (!dati_dal_vero()) return false;

    return ha_chiama("automation", attiva ? "turn_on" : "turn_off",
                     u->entita, NULL);
}

bool dati_finestra_imposta(int gruppo, int n, bool capo, int minuti)
{
    const finestra_t *w = dati_finestra(gruppo, n);
    if (!w) return false;
    if (minuti < 0 || minuti >= 24 * 60) return false;

    const char *e = capo ? w->accensione : w->spegnimento;
    if (!e || !e[0]) return false;
    if (!dati_dal_vero()) return false;

    /* Solo il campo `time`: questi helper hanno has_date falso, e mandare
       anche una data farebbe rifiutare la chiamata. I secondi si scrivono a
       zero invece di lasciarli: un orario che scatta ai 30 secondi del
       minuto giusto e un orario che scatta un minuto dopo. */
    char dati[48];
    snprintf(dati, sizeof dati, "\"time\": \"%02d:%02d:00\"",
             minuti / 60, minuti % 60);
    return ha_chiama("input_datetime", "set_datetime", e, dati);
}
