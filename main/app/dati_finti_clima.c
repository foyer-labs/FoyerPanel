/* ------------------------------------------------------------------------
 * Finto fornitore — clima. Solo Fase 1.
 *
 * Le zone e le unita sono quelle vere di 04-config.example.json: dodici zone
 * di riscaldamento e quattro condizionatori Gree. Dodici zone su una
 * capienza di sei o otto e, come per le luci, il caso che obbliga a
 * impaginare — e conviene averlo davanti dal primo giorno invece di
 * scoprirlo quando la casa e gia collegata.
 * --------------------------------------------------------------------- */
#include "cJSON.h"
#include "dati_finti.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "entita.h"
#include "ha.h"
#include "i18n.h"
#include "orologio.h"

/* --- stato inventato, struttura dalla configurazione --------------------
 *
 * I valori qui sotto sono quelli del mockup, e restano finche non arriva
 * Home Assistant. Nomi ed entita invece vengono da config.json: se domani
 * si aggiunge una zona, compare senza toccare questo file.
 */
/* Campi per nome e non per posizione: aggiungere un campo alla
   struttura non deve far scivolare in silenzio tutti i valori di una
   colonna, che e quello che era appena successo aggiungendo `entita`. */
static const zona_clima_t VALORI_ZONE[] = {
    { .nome = "Living room", .misurata = 245, .richiesta = 240, .umidita = 48,
      .chiama = true, .accesa = true, .disponibile = true },
    { .nome = "Kitchen", .misurata = 248, .richiesta = 240, .umidita = 51,
      .chiama = false, .accesa = true, .disponibile = true },
    { .nome = "Guest room", .misurata = 226, .richiesta = 220, .umidita = 47,
      .chiama = false, .accesa = true, .disponibile = true },
    { .nome = "Kids' room", .misurata = 231, .richiesta = 230, .umidita = 46,
      .chiama = false, .accesa = true, .disponibile = true },
    { .nome = "Study", .misurata = 234, .richiesta = 230, .umidita = 45,
      .chiama = false, .accesa = true, .disponibile = true },
    { .nome = "Guest bathroom", .misurata = 241, .richiesta = 250, .umidita = 58,
      .chiama = true, .accesa = true, .disponibile = true },
    { .nome = "Laundry", .misurata = 238, .richiesta = 230, .umidita = 55,
      .chiama = false, .accesa = true, .disponibile = true },
    { .nome = "Master bedroom", .misurata = 198, .richiesta = 200, .umidita = 54,
      .chiama = false, .accesa = false, .disponibile = true },
    { .nome = "Walk-in closet", .misurata = 201, .richiesta = 200, .umidita = 53,
      .chiama = false, .accesa = false, .disponibile = true },
    { .nome = "Main bathroom", .misurata = 195, .richiesta = 190, .umidita = 57,
      .chiama = false, .accesa = false, .disponibile = true },
    { .nome = "Ground floor thermostat", .misurata = 243, .richiesta = 240, .umidita = 49,
      .chiama = false, .accesa = true, .disponibile = true },
    { .nome = "First floor thermostat", .misurata = 205, .richiesta = 200, .umidita = 52,
      .chiama = false, .accesa = false, .disponibile = true },
};
#define N_VALORI_ZONE ((int)(sizeof VALORI_ZONE / sizeof VALORI_ZONE[0]))
#define ZONE_MAX 24

static const condizionatore_t VALORI_CLIMI[] = {
    { .nome = "Living room", .modo = MODO_FREDDO, .stanza = 300, .richiesta = 280,
      .ventilazione = VENT_MEDIA, .deflettore = DEFL_CENTRO,
      .oscillazione = OSC_OSCILLANTE, .disponibile = true,
      .ha_timer = true, .timer_corre = true, .automazione_attiva = true,
      .durata_min = 90, .restano_min = 67, .spegne_alle = "20:12",
      .timer_totale_min = 90, .timer_quota_pct = 74,
      .extra = { false, true, false, true },
      .ha_extra = { true, true, true, true } },

    { .nome = "Attic", .modo = MODO_SPENTO, .stanza = 232, .richiesta = 240,
      .ventilazione = VENT_AUTO, .deflettore = DEFL_CENTRO,
      .oscillazione = OSC_PREDEFINITO, .disponibile = true,
      .ha_timer = true, .timer_corre = false, .automazione_attiva = true,
      .durata_min = 90, .restano_min = 0, .spegne_alle = NULL,
      .extra = { false, false, false, false },
      .ha_extra = { true, true, true, true } },

    { .nome = "Kids' room", .modo = MODO_AUTO, .stanza = 246, .richiesta = 250,
      .ventilazione = VENT_BASSA, .deflettore = DEFL_MEDIO_ALTO,
      .oscillazione = OSC_FISSO, .disponibile = true,
      .ha_timer = true, .timer_corre = false, .automazione_attiva = true,
      .durata_min = 90, .restano_min = 0, .spegne_alle = NULL,
      .extra = { true, false, false, false },
      .ha_extra = { true, true, true, true } },

    { .nome = "Master bedroom", .modo = MODO_CALDO, .stanza = 219,
      .richiesta = 230, .ventilazione = VENT_MEDIO_ALTA,
      .deflettore = DEFL_BASSO, .oscillazione = OSC_FISSO, .disponibile = true,
      .ha_timer = true, .timer_corre = true, .automazione_attiva = true,
      .durata_min = 90, .restano_min = 42, .spegne_alle = "19:24",
      .timer_totale_min = 120, .timer_quota_pct = 35,
      .extra = { false, true, true, false },
      .ha_extra = { true, true, true, true } },
};
#define N_VALORI_CLIMI ((int)(sizeof VALORI_CLIMI / sizeof VALORI_CLIMI[0]))
#define CLIMI_MAX 8

static zona_clima_t     zone[ZONE_MAX];
static condizionatore_t climi[CLIMI_MAX];
static int              n_zone, n_climi;

/* --- i piani ------------------------------------------------------------
 *
 * Quattro bastano e avanzano: una casa con cinque impianti di riscaldamento
 * separati non e la casa per cui questo pannello e stato scritto. */
#define PIANI_MAX 4
static piano_t          piani[PIANI_MAX];
static int              n_piani;
static int              piano_pannello = -1;
/* L'ora di spegnimento sta qui e non nel cJSON degli attributi: quel
   puntatore vale finche l'entita non si aggiorna, e una stringa mostrata
   sul vetro deve valere finche si guarda. */
static char ora_fine[CLIMI_MAX][8];
static caso_t           caso;
static bool             pronto;

/* --- limiti — 04-config.example.json ------------------------------------
 * Riscaldamento 15,0-26,0 con passo mezzo grado; condizionatori 16-30 con
 * passo intero, come accettano le unita Gree. Sono due impianti diversi e
 * non condividono la scala.
 */
/* In decimi di grado: la configurazione li scrive in gradi con la virgola,
   qui diventano interi perche sul pannello il virgola mobile costa e non
   serve. */
static limiti_t limiti_da(const char *dove, int16_t mn, int16_t mx, int16_t pa)
{
    char p[64];
    limiti_t l;
    snprintf(p, sizeof p, "%s/min", dove);
    l.min = (int16_t)(cfg_decimale(p, mn / 10.0) * 10);
    snprintf(p, sizeof p, "%s/max", dove);
    l.max = (int16_t)(cfg_decimale(p, mx / 10.0) * 10);
    snprintf(p, sizeof p, "%s/step", dove);
    l.passo = (int16_t)(cfg_decimale(p, pa / 10.0) * 10);
    if (l.passo <= 0) l.passo = pa;
    return l;
}

limiti_t dati_limiti_riscaldamento(void)
{
    return limiti_da("climate/heating_limits", 150, 260, 5);
}

limiti_t dati_limiti_condizionatori(void)
{
    return limiti_da("climate/ac_limits", 160, 300, 10);
}

/* Gli extra nell'ordine di dati.h: silenzioso, luce pannello, aria fresca,
   xfan. I nomi sono quelli di 03-config-contratto.md §4.6. */
static const char *const EXTRA_CAMPI[4] = {
    "quiet", "panel_light", "fresh_air", "xfan",
};

/* "1:30" sono novanta minuti. La configurazione li scrive cosi perche e
   come li si legge, il pannello li conta in minuti perche e come li mostra. */
/* "0:59:00" sono cinquantanove minuti. E il formato di `remaining`, che ha
   i secondi e non e quello di "1:30" della configurazione: leggerlo con
   l'altra funzione darebbe un'ora e trenta a un timer che ne ha zero e
   cinquantanove. Zero se non si sa leggere — un ripiego generoso qui
   direbbe che manca un'ora e mezza a un timer di cui non si sa niente. */
static uint16_t minuti_hms(const char *hms)
{
    int h = 0, m = 0, s = 0;
    if (!hms || sscanf(hms, "%d:%d:%d", &h, &m, &s) != 3) return 0;
    if (h < 0 || m < 0 || s < 0) return 0;
    const long tot = h * 3600L + m * 60L + s;
    if (tot > 24 * 3600L) return 0;
    return (uint16_t)((tot + 59) / 60);
}

static uint16_t minuti(const char *hhmm)
{
    int h = 0, m = 0;
    if (!hhmm || sscanf(hhmm, "%d:%d", &h, &m) != 2) return 90;
    return (uint16_t)(h * 60 + m);
}

/* --- traduzione da Home Assistant ---------------------------------------
 *
 * I nomi che usa Home Assistant e quelli che usa il pannello non
 * coincidono, e va bene cosi: `hvac_mode` e una stringa che cambia da
 * un'integrazione all'altra, l'enumerazione del pannello e fissa perche i
 * riquadri a schermo sono sei. La traduzione sta qui, in un posto, e non
 * sparsa fra le schermate.
 */
static modo_clima_t modo_da(const char *s)
{
    if (!s) return MODO_SPENTO;
    if (!strcmp(s, "heat"))     return MODO_CALDO;
    if (!strcmp(s, "cool"))     return MODO_FREDDO;
    if (!strcmp(s, "auto") || !strcmp(s, "heat_cool")) return MODO_AUTO;
    if (!strcmp(s, "dry"))      return MODO_DEUMIDIFICA;
    if (!strcmp(s, "fan_only")) return MODO_VENTILATORE;
    return MODO_SPENTO;
}

static ventilazione_t vent_da(const char *s)
{
    if (!s) return VENT_AUTO;
    if (!strcmp(s, "low"))         return VENT_BASSA;
    if (!strcmp(s, "medium low") || !strcmp(s, "medium_low"))
                                   return VENT_MEDIO_BASSA;
    if (!strcmp(s, "medium"))      return VENT_MEDIA;
    if (!strcmp(s, "medium high") || !strcmp(s, "medium_high"))
                                   return VENT_MEDIO_ALTA;
    if (!strcmp(s, "high"))        return VENT_ALTA;
    return VENT_AUTO;
}

/* Le dodici combinazioni di swing_mode diventano cinque posizioni piu
   un'oscillazione: e la scelta di 01-specifica-ui.md §3.2, e questa e la
   funzione che la applica. */
static void deflettore_da(const char *s, deflettore_t *pos, oscillazione_t *osc)
{
    *pos = DEFL_CENTRO;
    *osc = OSC_PREDEFINITO;
    if (!s) return;

    if (strstr(s, "upper") || !strcmp(s, "top"))         *pos = DEFL_ALTO;
    else if (strstr(s, "middle upper"))                  *pos = DEFL_MEDIO_ALTO;
    else if (strstr(s, "middle lower"))                  *pos = DEFL_MEDIO_BASSO;
    else if (strstr(s, "lower") || !strcmp(s, "bottom")) *pos = DEFL_BASSO;
    else if (strstr(s, "middle"))                        *pos = DEFL_CENTRO;

    if (!strcmp(s, "default"))       *osc = OSC_PREDEFINITO;
    else if (strstr(s, "full"))      *osc = OSC_TUTTA;
    else if (strstr(s, "swing"))     *osc = OSC_OSCILLANTE;
    else                             *osc = OSC_FISSO;
}

/* Le temperature di Home Assistant sono in gradi con la virgola; il
   pannello le tiene in decimi interi, perche il virgola mobile costa e
   mezzo grado e il passo piu fine dell'impianto. */
static int16_t decimi(double gradi) { return (int16_t)(gradi * 10 + (gradi < 0 ? -0.5 : 0.5)); }

/* La frazione che manca, per la barra della scheda.
 *
 * Il denominatore e l'attributo `duration` del timer e **non**
 * `durata_predefinita` della configurazione: quella e' la durata che il
 * pannello userebbe avviandolo lui, e un timer fatto partire dall'app con
 * tre ore, diviso per l'ora e mezza della configurazione, darebbe il
 * duecento per cento.
 *
 * Zero quando non si sa: chi disegna la salta. */
static void quota(condizionatore_t *c, const char *ent)
{
    c->timer_totale_min = minuti_hms(ent_attributo(ent, "duration", NULL));
    if (!c->timer_totale_min || !c->restano_min) return;

    /* Piu del cento per cento si puo': `timer.change` allunga il conteggio
       senza toccare `duration`, e un timer da un'ora prolungato di un quarto
       d'ora ne ha settantacinque su sessanta. La barra si ferma al fondo
       invece di sfondarlo. */
    uint32_t q = (uint32_t)c->restano_min * 100u / c->timer_totale_min;
    if (q > 100) q = 100;
    c->timer_quota_pct = (uint8_t)q;
}

/* --- quanto manca, e a che ora finisce ----------------------------------
 *
 * Il primo tentativo leggeva un attributo `remaining_s` che **Home Assistant
 * non ha**: il conto alla rovescia restava a zero e la riga diceva "mancano
 * 0h00" su un timer che stava correndo. Un attributo che non esiste non da
 * errore, da il valore di ripiego, ed e per questo che ci e voluto un
 * pannello sul muro per accorgersene.
 *
 * Quello che un timer di Home Assistant ha davvero:
 *
 *   duration     "1:00:00"   la durata impostata
 *   remaining    "0:59:00"   **ferma** al valore d'inizio finche corre
 *   finishes_at  ISO 8601    l'istante di fine, solo mentre corre
 *
 * Quindi il conto alla rovescia si calcola da `finishes_at` meno adesso, e
 * non si legge. Cosi scende davvero, resta giusto anche se il timer e stato
 * avviato da un'altra parte, e non ha bisogno che Home Assistant mandi
 * niente — perche mentre un timer corre non manda **nessun** aggiornamento.
 *
 * `remaining` resta come ripiego per quando l'orologio del pannello non e
 * ancora stato corretto: e fermo, ma e meglio di zero. */
static void quanto_manca(condizionatore_t *c, const char *ent,
                         char *ora, size_t ora_n)
{
    c->restano_min = 0;
    c->spegne_alle = NULL;
    c->timer_totale_min = 0;
    c->timer_quota_pct = 0;
    if (!c->timer_corre) return;

    const long long fine =
        orologio_da_iso(ent_attributo(ent, "finishes_at", NULL));
    const long long adesso = orologio_adesso_utc();

    if (fine > 0 && adesso > 0) {
        long long resta = fine - adesso;
        if (resta < 0) resta = 0;
        if (resta > 24 * 3600) resta = 24 * 3600;
        /* Per eccesso. Un timer avviato adesso per un'ora e mezza deve dire
           "1h30" e non "1h29": il secondo gia passato non e un minuto in
           meno, e vedere il conto partire sotto quello chiesto fa
           sospettare del pannello invece che dell'aritmetica. */
        c->restano_min = (uint16_t)((resta + 59) / 60);
        if (orologio_ora_locale(fine, ora, ora_n)) c->spegne_alle = ora;
        quota(c, ent);
        return;
    }

    /* Senza istante di fine o senza orologio: il valore fermo, che almeno
       dice di che ordine di grandezza si parla. L'ora di spegnimento resta
       vuota — inventarla sarebbe peggio che non scriverla. */
    c->restano_min = minuti_hms(ent_attributo(ent, "remaining", NULL));
    quota(c, ent);
}

/* --- caso in corso ------------------------------------------------------ */

void finto_clima_applica(caso_t c)
{
    caso = c;
    pronto = true;

    const limiti_t lr = dati_limiti_riscaldamento();
    const limiti_t lc = dati_limiti_condizionatori();

    /* --- i piani, prima delle zone: le zone si agganciano al loro id --- */
    n_piani = cfg_quanti("climate/floors");
    if (n_piani > PIANI_MAX) n_piani = PIANI_MAX;
    for (int n = 0; n < n_piani; n++) {
        piano_t *pi = &piani[n];
        pi->id     = cfg_testo_in("climate/floors", n, "id", "");
        pi->nome   = cfg_testo_in("climate/floors", n, "name", "");
        pi->entita = cfg_testo_in("climate/floors", n, "switch", "");
        pi->disponibile = pi->entita[0] && ent_vista(pi->entita)
                          && ent_disponibile(pi->entita);
        pi->acceso = pi->disponibile && ent_stato_e(pi->entita, "on");
        pi->zone = pi->chiedono = 0;
        /* Senza casa collegata: acceso il primo, spento il resto. Sono i due
           stati che la fascia deve saper disegnare. */
        if (!dati_dal_vero()) { pi->disponibile = true; pi->acceso = n == 0; }
    }

    piano_pannello = -1;
    {
        const char *mio = cfg_testo("climate/panel_floor", "");
        for (int n = 0; n < n_piani && mio[0]; n++)
            if (strcmp(piani[n].id, mio) == 0) piano_pannello = n;
    }

    n_zone = cfg_quanti("climate/heating");
    if (n_zone > ZONE_MAX) n_zone = ZONE_MAX;

    for (int n = 0; n < n_zone; n++) {
        /* Lo stato gira sui valori di esempio, il nome viene dalla
           configurazione: cosi una zona in piu non resta senza numeri. */
        zone[n] = VALORI_ZONE[n % N_VALORI_ZONE];
        zone[n].indice = n;
        zone[n].nome = cfg_testo_in("climate/heating", n, "name", "zona");
        zone[n].entita = cfg_testo_in("climate/heating", n, "climate", "");

        /* Il piano, per id. Meno uno se non gliene e stato dato uno o se
           l'id non corrisponde a nessun piano: in tutti e due i casi la
           zona finisce nel gruppo di quelle senza piano, che si vede. Un id
           scritto male che facesse sparire una zona sarebbe il difetto
           peggiore possibile — si nota solo quando quella stanza e fredda. */
        zone[n].piano = -1;
        {
            const char *pid = cfg_testo_in("climate/heating", n, "floor", "");
            for (int k = 0; k < n_piani && pid[0]; k++)
                if (strcmp(piani[k].id, pid) == 0) zone[n].piano = k;
        }

        if (ent_vista(zone[n].entita)) {
            const char *e = zone[n].entita;
            zone[n].disponibile = ent_disponibile(e);
            zone[n].accesa = !ent_stato_e(e, "off");
            zone[n].misurata = decimi(ent_attributo_numero(e, "current_temperature", -99));
            if (ent_attributo_numero(e, "current_temperature", -99) <= -99)
                zone[n].misurata = TEMP_IGNOTA;
            zone[n].richiesta = decimi(ent_attributo_numero(e, "temperature", 20));
            zone[n].umidita = (uint8_t)ent_attributo_numero(e, "current_humidity", 0);
            /* "Chiama calore" non e "e accesa": lo dice hvac_action, ed e
               la differenza fra una richiesta e una raggiunta. */
            zone[n].chiama = strcmp(ent_attributo(e, "hvac_action", ""), "heating") == 0;
        } else if (dati_dal_vero()) {
            zone[n].disponibile = false;
            zone[n].misurata = TEMP_IGNOTA;
            zone[n].umidita = 0;
            zone[n].chiama = false;
        }
        switch (caso) {
        case CASO_NOMI_LUNGHI:
            zone[n].nome = FINTO_NOME_LUNGO;
            break;
        case CASO_NON_DISPONIBILE:
            zone[n].disponibile = (n % 2) == 0;
            break;
        case CASO_TEMP_IGNOTA:
            zone[n].misurata = TEMP_IGNOTA;
            zone[n].umidita = 0;
            break;
        case CASO_SETPOINT_AL_LIMITE:
            zone[n].richiesta = (n % 2) ? lr.max : lr.min;
            break;
        default:
            break;
        }
    }

    n_climi = cfg_quanti("climate/air_conditioners");
    if (n_climi > CLIMI_MAX) n_climi = CLIMI_MAX;

    for (int n = 0; n < n_climi; n++) {
        climi[n] = VALORI_CLIMI[n % N_VALORI_CLIMI];
        climi[n].nome = cfg_testo_in("climate/air_conditioners", n, "name", "unita");
        climi[n].entita = cfg_testo_in("climate/air_conditioners", n, "climate", "");

        if (ent_vista(climi[n].entita)) {
            const char *e = climi[n].entita;
            climi[n].disponibile = ent_disponibile(e);
            climi[n].modo = modo_da(ent_stato(e));
            climi[n].stanza = decimi(ent_attributo_numero(e, "current_temperature", -99));
            if (ent_attributo_numero(e, "current_temperature", -99) <= -99)
                climi[n].stanza = TEMP_IGNOTA;
            climi[n].richiesta = decimi(ent_attributo_numero(e, "temperature", 24));
            climi[n].ventilazione = vent_da(ent_attributo(e, "fan_mode", NULL));
            deflettore_da(ent_attributo(e, "swing_mode", NULL),
                          &climi[n].deflettore, &climi[n].oscillazione);
        } else if (dati_dal_vero()) {
            climi[n].disponibile = false;
            climi[n].stanza = TEMP_IGNOTA;
        }
        /* L'interruttore del timer compare solo se la configurazione ha
           **entrambi** i campi: meta funzione e peggio di nessuna. */
        climi[n].ha_timer = cfg_ha_in("climate/air_conditioners", n, "timer") &&
                            cfg_ha_in("climate/air_conditioners", n, "timer_automation");
        climi[n].durata_min = minuti(cfg_testo_in("climate/air_conditioners", n,
                                                 "default_duration", "1:30"));

        /* Timer e automazione sono due entita indipendenti, e il pannello
           deve saperlo: se il timer corre ma l'automazione e disattivata,
           allo scadere non succede nulla. Vedere i minuti scorrere e
           credere che l'unita si spegnera sarebbe un inganno. */
        if (climi[n].ha_timer) {
            const char *t_ent = cfg_testo_in("climate/air_conditioners", n, "timer", "");
            const char *a_ent = cfg_testo_in("climate/air_conditioners", n,
                                             "timer_automation", "");
            if (ent_vista(t_ent)) {
                climi[n].timer_corre = ent_stato_e(t_ent, "active");
                quanto_manca(&climi[n], t_ent, ora_fine[n], sizeof ora_fine[0]);
            }
            if (ent_vista(a_ent))
                climi[n].automazione_attiva = ent_stato_e(a_ent, "on");
        }
        /* Un extra compare solo se l'unita ha quell'interruttore: le Gree
           non hanno tutte le stesse funzioni, e un pulsante che non comanda
           niente e peggio di un pulsante che manca. */
        for (int k = 0; k < 4; k++) {
            char campo[32];
            snprintf(campo, sizeof campo, "extras/%s", EXTRA_CAMPI[k]);
            climi[n].ha_extra[k] = cfg_ha_in("climate/air_conditioners", n, campo);
            if (!climi[n].ha_extra[k]) { climi[n].extra[k] = false; continue; }

            const char *e = cfg_testo_in("climate/air_conditioners", n, campo, "");
            if (ent_vista(e))            climi[n].extra[k] = ent_stato_e(e, "on");
            else if (dati_dal_vero())    climi[n].extra[k] = false;
        }
        switch (caso) {
        case CASO_NOMI_LUNGHI:
            climi[n].nome = FINTO_NOME_LUNGO;
            break;
        case CASO_NON_DISPONIBILE:
            climi[n].disponibile = (n % 2) == 0;
            break;
        case CASO_TEMP_IGNOTA:
            climi[n].stanza = TEMP_IGNOTA;
            break;
        case CASO_SETPOINT_AL_LIMITE:
            climi[n].richiesta = (n % 2) ? lc.max : lc.min;
            break;
        case CASO_TIMER_SENZA_AUTO:
            /* Il timer corre ma l'automazione e disattivata: allo scadere
               non succedera nulla, e il pannello deve dirlo invece di far
               vedere i minuti che scorrono. */
            climi[n].timer_corre = true;
            climi[n].restano_min = 42;
            climi[n].spegne_alle = "19:24";
            climi[n].timer_totale_min = 120;
            climi[n].timer_quota_pct = 35;
            climi[n].automazione_attiva = false;
            break;
        default:
            break;
        }
    }


    /* I conteggi vanno per ultimi: a quale piano appartiene una zona, e se
       sta chiamando calore, si sa solo dopo averla letta. */
    for (int n = 0; n < n_zone; n++) {
        const int k = zone[n].piano;
        if (k < 0 || k >= n_piani) continue;
        piani[k].zone++;
        if (zone[n].chiama) piani[k].chiedono++;
    }
}

static void assicura(void)
{
    if (!pronto) finto_clima_applica(CASO_NORMALE);
}

/* --- riscaldamento ------------------------------------------------------ */

int dati_zone_clima(void)
{
    assicura();
    return caso == CASO_ZERO_LUCI ? 0 : n_zone;
}

const zona_clima_t *dati_zona_clima(int n)
{
    assicura();
    return (n >= 0 && n < dati_zone_clima()) ? &zone[n] : NULL;
}

int dati_zone_in_richiesta(void)
{
    int quante = 0;
    for (int n = 0; n < dati_zone_clima(); n++)
        if (zone[n].chiama) quante++;
    return quante;
}

/* --- condizionatori ----------------------------------------------------- */

int dati_condizionatori(void)
{
    assicura();
    return caso == CASO_ZERO_LUCI ? 0 : n_climi;
}

const condizionatore_t *dati_condizionatore(int n)
{
    assicura();
    return (n >= 0 && n < dati_condizionatori()) ? &climi[n] : NULL;
}

int dati_condizionatori_accesi(void)
{
    int quanti = 0;
    for (int n = 0; n < dati_condizionatori(); n++)
        if (climi[n].modo != MODO_SPENTO) quanti++;
    return quanti;
}

/* La stessa che mostrano la home e lo standby, chiesta a chi la risolve.
   Aveva una fonte sua — `clima.sensore_esterno` — e con lei la possibilita
   di dire un numero diverso dalle altre due schermate. */
int16_t dati_temperatura_esterna(void)
{
    assicura();
    if (dati_caso_corrente() == CASO_TEMP_IGNOTA) return TEMP_IGNOTA;
    return dati_meteo().temperatura;
}

/* --- nomi da mostrare --------------------------------------------------- */

static const tx_t MODI[MODO_QUANTI] = {
    TX_CLIMATE_MODE_OFF, TX_CLIMATE_MODE_HEAT, TX_CLIMATE_MODE_COOL,
    TX_CLIMATE_MODE_AUTO, TX_CLIMATE_MODE_DRY, TX_CLIMATE_MODE_FAN,
};

/* Nei sei riquadri affiancati del dettaglio i due nomi lunghi andrebbero a
   capo, e la seconda riga finirebbe fuori dal riquadro. Abbreviati solo li:
   nella pastiglia della modalita corrente, dove lo spazio c'e, restano
   interi. Un nome accorciato dappertutto sarebbe un peggioramento pagato
   anche dove non serviva. */
static const tx_t MODI_CORTI[MODO_QUANTI] = {
    TX_CLIMATE_MODE_SHORT_OFF, TX_CLIMATE_MODE_SHORT_HEAT, TX_CLIMATE_MODE_SHORT_COOL,
    TX_CLIMATE_MODE_SHORT_AUTO, TX_CLIMATE_MODE_SHORT_DRY, TX_CLIMATE_MODE_SHORT_FAN,
};
static const tx_t VENTI[VENT_QUANTE] = {
    TX_CLIMATE_FAN_AUTO, TX_CLIMATE_FAN_LOW, TX_CLIMATE_FAN_MEDIUM_LOW,
    TX_CLIMATE_FAN_MEDIUM, TX_CLIMATE_FAN_MEDIUM_HIGH, TX_CLIMATE_FAN_HIGH,
};
static const tx_t DEFLETTORI[DEFL_QUANTE] = {
    TX_CLIMATE_VANE_TOP, TX_CLIMATE_VANE_UPPER, TX_CLIMATE_VANE_MIDDLE,
    TX_CLIMATE_VANE_LOWER, TX_CLIMATE_VANE_BOTTOM,
};
static const tx_t EXTRA[4] = {
    TX_CLIMATE_EXTRA_QUIET, TX_CLIMATE_EXTRA_PANEL_LIGHT,
    TX_CLIMATE_EXTRA_FRESH_AIR, TX_CLIMATE_EXTRA_XFAN,
};

const char *dati_modo_nome(modo_clima_t m)
{
    return tr((m >= 0 && m < MODO_QUANTI) ? MODI[m] : MODI[0]);
}

const char *dati_modo_nome_corto(modo_clima_t m)
{
    return tr((m >= 0 && m < MODO_QUANTI) ? MODI_CORTI[m] : MODI_CORTI[0]);
}

const char *dati_ventilazione_nome(ventilazione_t v)
{
    return tr((v >= 0 && v < VENT_QUANTE) ? VENTI[v] : VENTI[0]);
}

const char *dati_deflettore_nome(deflettore_t d)
{
    return tr((d >= 0 && d < DEFL_QUANTE) ? DEFLETTORI[d] : DEFLETTORI[0]);
}

const char *dati_extra_nome(int n)
{
    return tr((n >= 0 && n < 4) ? EXTRA[n] : EXTRA[0]);
}


/* ========================================================================
 * COMANDARE
 *
 * --- perche la parola da mandare non la scriviamo noi -------------------
 *
 * Per leggere, questo file traduce le parole di Home Assistant nelle sue
 * enumerazioni: "heat" diventa MODO_CALDO, "middle upper" diventa
 * DEFL_MEDIO_ALTO. Per scrivere servirebbe l'inverso, e la tentazione e una
 * tabella di stringhe fisse.
 *
 * Sarebbe fragile. Le parole dello swing_mode di un condizionatore Gree non
 * sono uno standard: sono quello che quel modello, con quella integrazione,
 * con quella versione, dichiara di accettare. Una tabella scritta oggi
 * funziona su questo impianto e sbaglia sul prossimo, e sbaglia in silenzio
 * — il comando parte, Home Assistant lo rifiuta, e chi guarda vede un
 * pulsante che non fa niente.
 *
 * Ogni entita climate pero **dichiara** cosa accetta, negli attributi
 * `hvac_modes`, `fan_modes`, `swing_modes`. Quindi non si inventa niente: si
 * scorre l'elenco dell'apparecchio, si decodifica ogni voce con lo **stesso**
 * lettore usato per leggere, e si prende quella che significa la cosa
 * voluta. Il codificatore e il decodificatore girato al contrario sopra le
 * parole di chi risponde, e non puo divergere da lui.
 * ===================================================================== */

/* La scelta va copiata: l'albero JSON degli attributi viene liberato subito
   dopo, e ha_chiama() legge la stringa piu tardi. */
static char scelta[48];

typedef bool (*corrisponde_t)(const char *parola, const void *voluto);

static const char *scegli(const char *entita, const char *elenco,
                          corrisponde_t uguale, const void *voluto)
{
    char *json = ent_attributo_json(entita, elenco);
    if (!json) return NULL;

    cJSON *a = cJSON_Parse(json);
    ent_libera_testo(json);

    const char *trovata = NULL;
    for (const cJSON *v = cJSON_IsArray(a) ? a->child : NULL; v; v = v->next) {
        const char *s = cJSON_GetStringValue((cJSON *)v);
        if (s && uguale(s, voluto)) {
            snprintf(scelta, sizeof scelta, "%s", s);
            trovata = scelta;
            break;
        }
    }
    cJSON_Delete(a);
    return trovata;
}

static bool e_questo_modo(const char *s, const void *voluto)
{
    return modo_da(s) == *(const modo_clima_t *)voluto;
}

static bool e_questa_vent(const char *s, const void *voluto)
{
    return vent_da(s) == *(const ventilazione_t *)voluto;
}

typedef struct { deflettore_t pos; oscillazione_t osc; } lamella_t;

static bool e_questa_lamella(const char *s, const void *voluto)
{
    const lamella_t *l = voluto;
    deflettore_t pos;
    oscillazione_t osc;
    deflettore_da(s, &pos, &osc);
    return pos == l->pos && osc == l->osc;
}

/* --- il setpoint --------------------------------------------------------
 *
 * Limitato ai valori del gruppo prima di partire. Un setpoint fuori scala
 * non lo rifiuta il pannello per pignoleria: lo rifiuta l'impianto, e
 * allora il comando parte, fallisce, e chi guarda vede un numero che torna
 * indietro da solo senza capire perche. */
/* --- che forma hanno i dati di un comando -------------------------------
 *
 * ha_chiama() avvolge gia il terzo argomento in "service_data":{...}: qui
 * va il **contenuto** dell'oggetto, non un oggetto completo. Passando
 * {"hvac_mode":"cool"} usciva "service_data":{{"hvac_mode":"cool"}}, che
 * non e JSON — e Home Assistant a un messaggio malformato non risponde con
 * un errore: **chiude il collegamento**.
 *
 * Sul pannello si vedeva cosi: premere una modalita faceva sparire Home
 * Assistant, senza una riga di errore da nessuna parte, e dopo qualche
 * secondo il collegamento tornava da solo. Il comando degli accessi
 * funzionava perche passa NULL. */
static bool manda_temperatura(const char *entita, int16_t decimi, limiti_t l)
{
    if (decimi < l.min) decimi = l.min;
    if (decimi > l.max) decimi = l.max;

    char dati[48];
    snprintf(dati, sizeof dati, "\"temperature\":%d.%d",
             decimi / 10, (decimi < 0 ? -decimi : decimi) % 10);
    return ha_chiama("climate", "set_temperature", entita, dati);
}

bool dati_zona_clima_imposta(int n, int16_t decimi)
{
    const zona_clima_t *z = dati_zona_clima(n);
    if (!z || !z->disponibile || !z->entita || !*z->entita) return false;
    /* Senza Home Assistant si muove il valore inventato, cosi il simulatore
       resta usabile, e si dice che il comando non e partito. E la stessa
       scelta di dati_luce_accendi(): fingere che sia andato sarebbe la bugia
       piu facile da scrivere. */
    if (!dati_dal_vero()) { zone[n].richiesta = decimi; return false; }
    return manda_temperatura(z->entita, decimi, dati_limiti_riscaldamento());
}

bool dati_zona_clima_accendi(int n, bool accesa)
{
    const zona_clima_t *z = dati_zona_clima(n);
    if (!z || !z->disponibile || !z->entita || !*z->entita) return false;
    if (!dati_dal_vero()) return false;

    const modo_clima_t voluto = accesa ? MODO_CALDO : MODO_SPENTO;
    const char *parola = scegli(z->entita, "hvac_modes", e_questo_modo, &voluto);
    if (!parola) parola = accesa ? "heat" : "off";

    char dati[64];
    snprintf(dati, sizeof dati, "\"hvac_mode\":\"%s\"", parola);
    return ha_chiama("climate", "set_hvac_mode", z->entita, dati);
}

/* --- i condizionatori --------------------------------------------------- */

bool dati_condizionatore_imposta(int n, int16_t decimi)
{
    const condizionatore_t *c = dati_condizionatore(n);
    if (!c || !c->disponibile || !c->entita || !*c->entita) return false;
    if (!dati_dal_vero()) return false;
    return manda_temperatura(c->entita, decimi, dati_limiti_condizionatori());
}

bool dati_condizionatore_modo(int n, modo_clima_t modo)
{
    const condizionatore_t *c = dati_condizionatore(n);
    if (!c || !c->disponibile || !c->entita || !*c->entita) return false;
    if (!dati_dal_vero()) return false;

    const char *parola = scegli(c->entita, "hvac_modes", e_questo_modo, &modo);
    /* Se l'apparecchio non offre quel modo, il comando non parte. E la
       risposta onesta: mandarne uno diverso perche "somiglia" vorrebbe dire
       accendere il caldo a chi ha chiesto il freddo. */
    if (!parola) return false;

    char dati[64];
    snprintf(dati, sizeof dati, "\"hvac_mode\":\"%s\"", parola);
    return ha_chiama("climate", "set_hvac_mode", c->entita, dati);
}

bool dati_condizionatore_ventilazione(int n, ventilazione_t v)
{
    const condizionatore_t *c = dati_condizionatore(n);
    if (!c || !c->disponibile || !c->entita || !*c->entita) return false;
    if (!dati_dal_vero()) return false;

    const char *parola = scegli(c->entita, "fan_modes", e_questa_vent, &v);
    if (!parola) return false;

    char dati[64];
    snprintf(dati, sizeof dati, "\"fan_mode\":\"%s\"", parola);
    return ha_chiama("climate", "set_fan_mode", c->entita, dati);
}

bool dati_condizionatore_deflettore(int n, deflettore_t pos,
                                    oscillazione_t osc)
{
    const condizionatore_t *c = dati_condizionatore(n);
    if (!c || !c->disponibile || !c->entita || !*c->entita) return false;
    if (!dati_dal_vero()) return false;

    const lamella_t voluta = { .pos = pos, .osc = osc };
    const char *parola = scegli(c->entita, "swing_modes", e_questa_lamella,
                                &voluta);
    if (!parola) return false;

    char dati[80];
    snprintf(dati, sizeof dati, "\"swing_mode\":\"%s\"", parola);
    return ha_chiama("climate", "set_swing_mode", c->entita, dati);
}

/* --- gli extra e il timer ----------------------------------------------
 *
 * Non passano dall'entita climate: sono interruttori a se, e la
 * configurazione li nomina uno per uno. Mandarli a climate non darebbe
 * errore — darebbe un servizio sconosciuto su un'entita che non c'entra. */
static const char *const CHIAVI_EXTRA[4] = {
    "quiet", "panel_light", "fresh_air", "xfan",
};

bool dati_condizionatore_extra(int n, int quale, bool acceso)
{
    const condizionatore_t *c = dati_condizionatore(n);
    if (!c || !c->disponibile || quale < 0 || quale > 3) return false;
    if (!c->ha_extra[quale]) return false;
    if (!dati_dal_vero()) return false;

    char percorso[64];
    snprintf(percorso, sizeof percorso, "extras/%s", CHIAVI_EXTRA[quale]);
    const char *e = cfg_testo_in("climate/air_conditioners", n, percorso, "");
    if (!e || !*e) return false;

    return ha_chiama("switch", acceso ? "turn_on" : "turn_off", e, NULL);
}

bool dati_condizionatore_timer_regola(int n, int16_t minuti)
{
    const condizionatore_t *c = dati_condizionatore(n);
    if (!c || !c->ha_timer || minuti == 0) return false;
    /* Un timer fermo non si regola: timer.change vuole un timer attivo, e
       mandarglielo lo stesso vorrebbe dire un errore nel registro di Home
       Assistant e niente sul vetro. */
    if (!c->timer_corre) return false;
    if (!dati_dal_vero()) return false;

    const char *e = cfg_testo_in("climate/air_conditioners", n, "timer", "");
    if (!e || !*e) return false;

    /* Il formato e quello dei servizi di Home Assistant, segno davanti:
       "00:15:00" allunga, "-00:15:00" accorcia. */
    const int m = minuti < 0 ? -minuti : minuti;
    char dati[64];
    snprintf(dati, sizeof dati, "\"duration\":\"%s%02d:%02d:00\"",
             minuti < 0 ? "-" : "", m / 60, m % 60);
    return ha_chiama("timer", "change", e, dati);
}

bool dati_condizionatore_automazione(int n, bool attiva)
{
    const condizionatore_t *c = dati_condizionatore(n);
    if (!c || !c->ha_timer) return false;
    if (!dati_dal_vero()) return false;

    const char *e = cfg_testo_in("climate/air_conditioners", n,
                                 "timer_automation", "");
    if (!e || !*e) return false;

    return ha_chiama("automation", attiva ? "turn_on" : "turn_off", e, NULL);
}

bool dati_condizionatore_timer_avvia(int n, uint16_t minuti)
{
    const condizionatore_t *c = dati_condizionatore(n);
    if (!c || !c->ha_timer || minuti == 0) return false;
    if (!dati_dal_vero()) return false;

    const char *e = cfg_testo_in("climate/air_conditioners", n, "timer", "");
    if (!e || !*e) return false;

    /* La durata si manda sempre. Quella scritta in Home Assistant e la sua
       — in questa casa un'ora — e quella scelta sul vetro puo essere
       un'altra: `timer.start` senza durata userebbe la prima, e il timer
       finirebbe mezz'ora prima di quanto dice il pannello. */
    char dati[64];
    snprintf(dati, sizeof dati, "\"duration\":\"%02u:%02u:00\"",
             minuti / 60u, minuti % 60u);
    return ha_chiama("timer", "start", e, dati);
}

bool dati_condizionatore_timer_annulla(int n)
{
    const condizionatore_t *c = dati_condizionatore(n);
    if (!c || !c->ha_timer) return false;
    if (!dati_dal_vero()) return false;

    const char *e = cfg_testo_in("climate/air_conditioners", n, "timer", "");
    if (!e || !*e) return false;

    /* `timer.cancel` e non `timer.finish`: finire vuol dire scatenare
       l'evento di scadenza, e l'automazione collegata spegnerebbe l'unita.
       Annullare un timer non deve spegnere il condizionatore. */
    return ha_chiama("timer", "cancel", e, NULL);
}

/* --- i piani ------------------------------------------------------------ */

int dati_piani(void) { assicura(); return n_piani; }

const piano_t *dati_piano(int n)
{
    assicura();
    return (n >= 0 && n < n_piani) ? &piani[n] : NULL;
}

int dati_piano_del_pannello(void) { assicura(); return piano_pannello; }

bool dati_piano_accendi(int n, bool acceso)
{
    const piano_t *p = dati_piano(n);
    if (!p || !p->disponibile) return false;
    if (!dati_dal_vero()) return false;

    /* Il dominio decide il servizio, come per gli interruttori: un
       `input_boolean.` comandato con `switch.turn_on` non da errore sul
       vetro, da un interruttore che si muove e non succede niente. */
    const char *punto = strchr(p->entita, '.');
    if (!punto || punto == p->entita) return false;

    char dom[24];
    const size_t l = (size_t)(punto - p->entita);
    if (l >= sizeof dom) return false;
    memcpy(dom, p->entita, l);
    dom[l] = 0;

    return ha_chiama(dom, acceso ? "turn_on" : "turn_off", p->entita, NULL);
}
