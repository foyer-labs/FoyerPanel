/* ------------------------------------------------------------------------
 * Finto fornitore — energia. Solo Fase 1.
 *
 * I numeri sono quelli della home del mockup: 3,42 kW dal sole, 1,20 kW in
 * casa, 2,20 kW in rete, batteria al 78% in carica a 0,90 kW. Le due
 * stringhe hanno tensione e corrente ma non la potenza, come fanno molti
 * inverter: la potenza si calcola.
 * --------------------------------------------------------------------- */
#include "dati_finti.h"

#include <stddef.h>
#include <string.h>

#include "config.h"
#include "entita.h"
#include "ha.h"

/* I valori del mockup, che servono finche non parla l'inverter vero. */
static const stringa_t VALORI_STRINGHE[] = {
    { .nome = "Stringa 1", .tensione_decimi = 3812, .corrente_centesimi = 271,
      .potenza_w = -1, .disponibile = true },
    { .nome = "Stringa 2", .tensione_decimi = 3745, .corrente_centesimi = 258,
      .potenza_w = -1, .disponibile = true },
};
#define N_VALORI_STRINGHE ((int)(sizeof VALORI_STRINGHE / sizeof VALORI_STRINGHE[0]))
#define STRINGHE_MAX 8

/* I consumi del mockup, in watt: servono a poter guardare la schermata
   prima che una casa vera ci parli. L'ordine qui e volutamente **sbagliato**
   — la lavastoviglie prima del forno — cosi una cattura mostra se
   l'ordinamento funziona invece di mostrare l'ordine in cui erano scritti. */
static const struct { const char *nome; int32_t w; } CONSUMI[] = {
    { "Dishwasher",      310 },
    { "Heat pump",       820 },
    { "Fridge",          115 },
    { "Oven",            540 },
    { "Washer",           62 },
    { "Dryer",             0 },
};
#define N_CONSUMI ((int)(sizeof CONSUMI / sizeof CONSUMI[0]))

/* Ventiquattro ore, in watt: una giornata di sole con le nuvole del
   pomeriggio, che e la forma che il grafico deve saper disegnare. */
static const int32_t STORICO[24] = {
    0, 0, 0, 0, 0, 30, 320, 980, 1740, 2560, 3180, 3620,
    3840, 3720, 3420, 2980, 2210, 1480, 760, 210, 0, 0, 0, 0,
};

#define DISPOSITIVI_MAX 12

static energia_t     ora;
static stringa_t     stringhe[STRINGHE_MAX];
static int           n_stringhe;
static dispositivo_t dispositivi[DISPOSITIVI_MAX];
static int           n_dispositivi;
static int32_t       dispositivi_totale_w;
static giornata_t    giornata;
static caso_t     caso;
static bool       pronto;

/* L'aggregato e un sensore solo con tutto dentro negli attributi: e la
   scelta di 03-config-contratto.md §4, ed evita cinque sottoscrizioni per
   cinque numeri che cambiano insieme. **Quali** attributi lo dice la
   configurazione, perche i nomi li ha scelti chi ha scritto il template in
   Home Assistant, non noi. */
/* --- l'unita non si assume mai ------------------------------------------
 *
 * Vale qui come per l'aggregato, e per la stessa ragione: un sensore che
 * riporta kilowatt letto come watt fa sembrare un forno una fonderia, senza
 * nessun errore da nessuna parte. Home Assistant la dichiara in
 * `unit_of_measurement`, ed e il dato piu affidabile che ci sia perche viene
 * dalla stessa entita che porta il valore. */
static int32_t watt_di(const char *entita)
{
    const char *u = ent_attributo(entita, "unit_of_measurement", "W");
    const int fattore = (u[0] == 'k' || u[0] == 'K') ? 1000 : 1;
    return (int32_t)(ent_numero(entita, 0) * fattore);
}

/* Wattora, con la stessa regola: kWh o Wh lo dice l'entita. */
static int32_t wattora_di(const char *entita)
{
    const char *u = ent_attributo(entita, "unit_of_measurement", "kWh");
    const int fattore = (u[0] == 'k' || u[0] == 'K') ? 1000 : 1;
    return (int32_t)(ent_numero(entita, 0) * fattore);
}

/* --- i dispositivi, ordinati -------------------------------------------
 *
 * L'ordinamento e a bolle su dodici elementi: sono dodici, e una volta al
 * secondo. Qualunque cosa piu furba costerebbe piu righe di quante ne
 * risparmi, e le righe qui si leggono piu spesso di quanto si eseguano.
 *
 * Chi non e disponibile finisce in fondo invece di sparire: una presa che
 * non risponde e un'informazione, e toglierla dall'elenco farebbe pensare
 * che non sia configurata. */
static void ordina_dispositivi(void)
{
    for (int i = 0; i < n_dispositivi - 1; i++)
        for (int j = 0; j < n_dispositivi - 1 - i; j++) {
            const bool scambia =
                (!dispositivi[j].disponibile && dispositivi[j + 1].disponibile) ||
                (dispositivi[j].disponibile == dispositivi[j + 1].disponibile &&
                 dispositivi[j].watt < dispositivi[j + 1].watt);
            if (scambia) {
                const dispositivo_t x = dispositivi[j];
                dispositivi[j] = dispositivi[j + 1];
                dispositivi[j + 1] = x;
            }
        }

    /* La quota sul totale **dei monitorati**, non su quello di casa: vedi
       dati.h, la somma delle prese non fa il consumo della casa. */
    if (dispositivi_totale_w > 0)
        for (int n = 0; n < n_dispositivi; n++)
            dispositivi[n].quota_pct =
                (uint8_t)((int64_t)dispositivi[n].watt * 100 / dispositivi_totale_w);
}

static void leggi_dispositivi(void)
{
    /* Zero in configurazione vuol dire **zero**, non «usa quelli finti»: su
       una casa vera un elenco inventato sarebbe peggio di un elenco vuoto. */
    n_dispositivi = cfg_quanti("energy/devices");
    if (n_dispositivi > DISPOSITIVI_MAX) n_dispositivi = DISPOSITIVI_MAX;
    dispositivi_totale_w = 0;

    for (int n = 0; n < n_dispositivi; n++) {
        const char *e = cfg_testo_in("energy/devices", n, "entity", "");
        dispositivi[n].nome = cfg_testo_in("energy/devices", n, "name", "");
        dispositivi[n].disponibile = ent_vista(e) && ent_disponibile(e);
        dispositivi[n].watt = dispositivi[n].disponibile ? watt_di(e) : 0;
        dispositivi[n].quota_pct = 0;
        if (dispositivi[n].watt > 0) dispositivi_totale_w += dispositivi[n].watt;
    }

    ordina_dispositivi();
}

/* --- i quattro totali della giornata ----------------------------------- */
static void leggi_giornata(void)
{
    static const struct { const char *chiave; int quale; } Q[] = {
        { "energy/today/produced",  0 },
        { "energy/today/consumed", 1 },
        { "energy/today/imported", 2 },
        { "energy/today/exported",   3 },
    };
    int32_t *valori[4] = { &giornata.prodotto_wh, &giornata.consumato_wh,
                           &giornata.prelevato_wh, &giornata.immesso_wh };
    bool *ci_sono[4] = { &giornata.prodotto_c_e, &giornata.consumato_c_e,
                         &giornata.prelevato_c_e, &giornata.immesso_c_e };

    for (int n = 0; n < 4; n++) {
        const char *e = cfg_testo(Q[n].chiave, "");
        const bool c_e = e[0] && ent_vista(e) && ent_disponibile(e);
        *ci_sono[Q[n].quale] = c_e;
        *valori[Q[n].quale] = c_e ? wattora_di(e) : 0;
    }

    /* Il prodotto di oggi lo sa gia l'aggregato, se nessuno ha indicato un
       sensore suo: e lo stesso numero, e chiederlo due volte sarebbe un modo
       per farli diventare diversi. */
    if (!giornata.prodotto_c_e && ora.disponibile) {
        giornata.prodotto_wh = ora.oggi_wh;
        giornata.prodotto_c_e = true;
    }
}

/* An attribute of the aggregate sensor, by the name the configuration
 * gives. When the entity does not have it and the name is one of the
 * package's defaults, the Italian name of the same attribute is tried:
 * the package in 06-ha-package.yaml said `rete_w`, `batteria_pct`, `casa`
 * until schema 11, and a house that installed it then should not have to
 * edit its Home Assistant because the panel changed language. */
static double attributo(const char *entita, const char *nome)
{
    static const struct { const char *nuovo, *vecchio; } PRIMA[] = {
        { "grid_w", "rete_w" }, { "battery_w", "batteria_w" },
        { "battery_pct", "batteria_pct" }, { "today_kwh", "oggi_kwh" },
        { "house_w", "casa" },
    };
    const double ASSENTE = -1e18;
    double v = ent_attributo_numero(entita, nome, ASSENTE);
    if (v > ASSENTE) return v;
    for (unsigned n = 0; n < sizeof PRIMA / sizeof PRIMA[0]; n++)
        if (strcmp(nome, PRIMA[n].nuovo) == 0) {
            v = ent_attributo_numero(entita, PRIMA[n].vecchio, ASSENTE);
            if (v > ASSENTE) return v;
        }
    return 0;
}

static void da_home_assistant(void)
{
    const char *agg = cfg_testo("energy/aggregate_sensor", "");
    if (!ent_vista(agg)) {
        if (dati_dal_vero()) {
            ora.disponibile = false;
            for (int n = 0; n < n_stringhe; n++) stringhe[n].disponibile = false;
        }
        return;
    }

    ora.disponibile = ent_disponibile(agg);

    /* --- watt o kilowatt, e chi lo decide ---------------------------------
     *
     * Il pannello lavora in watt interi, che e la sola unita in cui non si
     * perde niente. Quello che arriva da Home Assistant pero puo essere in
     * watt o in kilowatt, e chi lo scrive non e sempre la stessa persona.
     *
     * Prima il codice moltiplicava tutto per mille, cioe **assumeva**
     * kilowatt ovunque. Su un impianto che riporta watt il risultato era
     * mille volte troppo: ottocentocinquanta watt diventavano ottocento-
     * cinquanta chilowatt, e la home mostrava una casa che consuma quanto un
     * quartiere. Nessun errore, nessun avviso: solo un numero sbagliato con
     * l'unita giusta accanto, che e il modo peggiore di sbagliare.
     *
     * Adesso l'unita non si assume mai:
     *
     * - **Per lo stato** la dichiara Home Assistant, in
     *   `unit_of_measurement`. E il dato piu affidabile che ci sia, perche
     *   viene dalla stessa entita che porta il valore.
     * - **Per gli attributi** un'unita dichiarata non esiste, e allora la
     *   dice il nome che il contratto ha scelto: `rete_w` sono watt,
     *   `oggi_kwh` sono kilowattora. Il nome e la chiave in config.json, non
     *   quello dell'attributo su Home Assistant, quindi non dipende da come
     *   qualcuno ha chiamato le sue cose.
     */
    const char *unita = ent_attributo(agg, "unit_of_measurement", "W");
    const int fattore = (unita[0] == 'k' || unita[0] == 'K') ? 1000 : 1;
    ora.produzione_w = (int32_t)(ent_numero(agg, 0) * fattore);

    ora.rete_w = (int32_t)attributo(
        agg, cfg_testo("energy/attributes/grid_w", "grid_w"));
    ora.batteria_w = (int32_t)attributo(
        agg, cfg_testo("energy/attributes/battery_w", "battery_w"));
    ora.batteria_pct = (uint8_t)attributo(
        agg, cfg_testo("energy/attributes/battery_pct", "battery_pct"));
    ora.oggi_wh = (int32_t)(attributo(
        agg, cfg_testo("energy/attributes/today_kwh", "today_kwh")) * 1000);

    /* Quale verso sia "positivo" lo decide l'impianto, non noi: si gira il
       segno una volta qui invece che in ogni schermata che lo legge. */
    if (strcmp(cfg_testo("energy/signs/grid_positive", "import"),
               "export") == 0)
        ora.rete_w = -ora.rete_w;
    if (strcmp(cfg_testo("energy/signs/battery_positive", "charge"),
               "discharge") == 0)
        ora.batteria_w = -ora.batteria_w;

    leggi_dispositivi();
    leggi_giornata();

    /* La curva della giornata. Il sensore e quello dichiarato, o
       l'aggregato: sono lo stesso numero, e chiederne due sarebbe un modo
       per farli diventare diversi. La chiamata e senza effetto se la
       risposta e gia arrivata — se ne occupa ha_storico_chiedi(). */
    const char *st = cfg_testo("energy/history", "");
    ha_storico_chiedi(st[0] ? st : agg);

    for (int n = 0; n < n_stringhe; n++) {
        const char *v = cfg_testo_in("energy/strings", n, "voltage", "");
        const char *i = cfg_testo_in("energy/strings", n, "current", "");
        const char *w = cfg_testo_in("energy/strings", n, "power", "");

        stringhe[n].disponibile = ent_disponibile(v) || ent_disponibile(w);
        if (ent_vista(v))
            stringhe[n].tensione_decimi = (uint16_t)(ent_numero(v, 0) * 10);
        if (ent_vista(i))
            stringhe[n].corrente_centesimi = (uint16_t)(ent_numero(i, 0) * 100);
        /* La potenza dichiarata vince su V x I: se l'inverter la da, quella
           e la sua, e la nostra moltiplicazione sarebbe un'approssimazione
           in piu su un numero che qualcuno legge. */
        stringhe[n].potenza_w = ent_vista(w) ? (int32_t)ent_numero(w, 0) : -1;
    }
}

void finto_energia_applica(caso_t c)
{
    caso = c;
    pronto = true;

    /* I numeri sono quelli del mockup con una correzione: li produzione,
       casa, rete e batteria non soddisfano l'identita che la specifica
       dichiara (3,42 = 1,20 + 2,20 + 0,90 non torna). Qui la rete e messa a
       -1,32 kW perche casa venga 1,20 kW come nel mockup, cosi il numero che
       si legge e insieme quello disegnato e quello che la formula produce. */
    ora = (energia_t){ .produzione_w = 3420, .rete_w = -1320,
                       .batteria_w = 900, .batteria_pct = 78,
                       .oggi_wh = 24800, .disponibile = true };

    /* Quante stringhe, e come si chiamano, lo dice la configurazione. */
    n_stringhe = cfg_quanti("energy/strings");
    if (n_stringhe > STRINGHE_MAX) n_stringhe = STRINGHE_MAX;
    for (int n = 0; n < n_stringhe; n++) {
        stringhe[n] = VALORI_STRINGHE[n % N_VALORI_STRINGHE];
        stringhe[n].nome = cfg_testo_in("energy/strings", n, "name", "stringa");
    }

    /* I dispositivi e i totali del mockup, quando non c'e una casa che
       parli: da_home_assistant() li sovrascrive se li trova. */
    n_dispositivi = cfg_quanti("energy/devices");
    if (!n_dispositivi) n_dispositivi = N_CONSUMI;
    if (n_dispositivi > DISPOSITIVI_MAX) n_dispositivi = DISPOSITIVI_MAX;
    dispositivi_totale_w = 0;
    for (int n = 0; n < n_dispositivi; n++) {
        dispositivi[n].nome = CONSUMI[n % N_CONSUMI].nome;
        dispositivi[n].watt = CONSUMI[n % N_CONSUMI].w;
        dispositivi[n].disponibile = true;
        dispositivi_totale_w += dispositivi[n].watt;
    }
    ordina_dispositivi();

    giornata = (giornata_t){ .prodotto_wh = 18400, .consumato_wh = 12100,
                             .prelevato_wh = 3200, .immesso_wh = 9500,
                             .prodotto_c_e = true, .consumato_c_e = true,
                             .prelevato_c_e = true, .immesso_c_e = true };

    da_home_assistant();

    switch (caso) {
    case CASO_NOTTE:
        /* Di notte non si scrive "0,00 kW" ripetuto: si scrive che non c'e
           produzione. La batteria si scarica, la rete prende. */
        ora.produzione_w = 0;
        ora.rete_w = 640;
        ora.batteria_w = -480;
        ora.batteria_pct = 41;
        for (int n = 0; n < n_stringhe; n++) {
            stringhe[n].tensione_decimi = 0;
            stringhe[n].corrente_centesimi = 0;
        }
        break;
    case CASO_BATTERIA_FERMA:
        ora.batteria_w = 20;    /* sotto la soglia: "ferma" */
        break;
    case CASO_CORRENTE_ZERO:
        if (n_stringhe) stringhe[0].corrente_centesimi = 0;
        break;
    case CASO_NON_DISPONIBILE:
        ora.disponibile = false;
        if (n_stringhe > 1) stringhe[1].disponibile = false;
        break;
    default:
        break;
    }
}

static void assicura(void)
{
    if (!pronto) finto_energia_applica(CASO_NORMALE);
}

energia_t dati_energia(void) { assicura(); return ora; }

verso_t dati_verso(int32_t w)
{
    if (w > ENERGIA_SOGLIA_W)  return VERSO_POSITIVO;
    if (w < -ENERGIA_SOGLIA_W) return VERSO_NEGATIVO;
    return VERSO_NULLO;
}

/* Il consumo di casa non e un sensore: 01-specifica-ui.md §3.3 lo definisce
   come produzione + rete - batteria, con i segni della tabella. */
int32_t dati_energia_casa_w(void)
{
    assicura();
    return ora.produzione_w + ora.rete_w - ora.batteria_w;
}

int dati_stringhe(void) { assicura(); return n_stringhe; }

const stringa_t *dati_stringa(int n)
{
    assicura();
    return (n >= 0 && n < n_stringhe) ? &stringhe[n] : NULL;
}

/* L'inverter espone tensione e corrente ma non la potenza per stringa: si
   calcola V x I e si presenta come stimata. La somma delle due stringhe
   sara leggermente superiore alla produzione dichiarata, perche quella e in
   alternata e queste in continua: non e un errore e non va "corretto"
   facendo quadrare i numeri. */
int32_t dati_stringa_potenza_w(int n)
{
    const stringa_t *s = dati_stringa(n);
    if (!s || !s->disponibile) return -1;
    if (s->potenza_w >= 0) return s->potenza_w;
    /* Corrente a zero: zero watt, non una divisione che salta. */
    return (int32_t)s->tensione_decimi * (int32_t)s->corrente_centesimi / 1000;
}

int dati_storico_punti(void)
{
    assicura();
    return (int)(sizeof STORICO / sizeof STORICO[0]);
}

int32_t dati_storico(int n)
{
    assicura();
    if (n < 0 || n >= dati_storico_punti()) return -1;

    /* Quella vera, se e arrivata. L'array costante resta per il simulatore
       e per le catture: li non c'e nessuna casa da interrogare, e un
       grafico piatto non farebbe vedere se il disegno funziona. */
    if (ha_storico_c_e()) return ha_storico_ora(n);
    if (dati_dal_vero())  return -1;

    return caso == CASO_NOTTE ? 0 : STORICO[n];
}

bool dati_storico_vero(void)
{
    assicura();
    return ha_storico_c_e();
}

bool dati_storico_negato(void)
{
    assicura();
    return ha_storico_no();
}

int dati_dispositivi(void)
{
    assicura();
    return n_dispositivi;
}

const dispositivo_t *dati_dispositivo(int n)
{
    assicura();
    if (n < 0 || n >= n_dispositivi) return NULL;
    return &dispositivi[n];
}

int32_t dati_dispositivi_totale_w(void)
{
    assicura();
    return dispositivi_totale_w;
}

giornata_t dati_giornata(void)
{
    assicura();
    return giornata;
}
