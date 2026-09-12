/* ------------------------------------------------------------------------
 * Dettaglio del condizionatore — 01-specifica-ui.md §3.2.
 *
 * Vista a tutta pagina, con "← Zone" nella testata al posto del tasto
 * indietro generico: non e una gerarchia di navigazione, e il ritorno al
 * gruppo da cui si e arrivati.
 *
 *   colonna sinistra   temperatura in stanza, setpoint con tasti grandi,
 *                      riquadro del timer
 *   colonna destra     Modalita (6), Ventilazione (6 con barrette),
 *                      Deflettore (5 posizioni piu 4 tasti), Extra (4)
 *
 * In verticale le due colonne non ci stanno affiancate — su 800 px
 * comprimerle renderebbe i tasti piu piccoli del minimo di tocco — e
 * nemmeno impilate: una sotto l'altra chiedono piu di duecento pixel oltre
 * il vetro, e oscillazione ed extra finivano sotto la barra in basso.
 * Diventano **due pagine**, che e come il pannello impagina dappertutto:
 * frecce in testata al posto dell'orologio, pallini in fondo.
 *
 * The first page is what is needed almost always: temperature, setpoint,
 * **mode** and timer. The mode was on the second, which meant changing
 * page for the most frequent gesture of all — turning the air on or off —
 * while the first gave more than half the screen to the temperature's
 * number. Now the temperature takes the room that is left, and the second
 * page — fan, vane, extras — has bigger controls in the room the mode left.
 *
 * Le dodici combinazioni reali di swing_modes si ottengono da cinque
 * posizioni per fisso o oscillante, piu predefinito e tutta l'escursione.
 * Dodici tasti sarebbero illeggibili; quattro opzioni sarebbero sbagliate.
 * --------------------------------------------------------------------- */
#include "comuni.h"
#include "dati.h"
#include "schermate.h"
#include "ui.h"
#include "widgets/deflettore.h"
#include "widgets/interruttore.h"
#include "widgets/paginatore.h"

/* La vista del dettaglio e VISTA_DETTAGLIO piu l'indice dell'unita. */
#define VISTA_DETTAGLIO 10

static lv_color_t accento;      /* il colore della modalita in corso */

/* Vero quando i riquadri non possono dividersi l'altezza rimasta e devono
   averne una loro. Con le due pagine non succede piu: ogni pagina ha
   l'altezza intera del contenuto, come la colonna dell'orizzontale. */
static bool colonna_unica;

/* Quale delle due pagine, in verticale. In orizzontale la pagina e una
   sola e questa resta a zero. */
static int pagina;

/* True in portrait: each page has the whole height and fewer controls than
   the landscape column, and the tiles grow — two rows of three instead of
   one of six, text and icons a step up. In landscape everything stays as
   it was: six tiles in a row are what the column holds. */
static bool grande;

static font_ruolo_t testo_riquadro(void) { return grande ? FT_M : FT_S; }
static icona_corpo_t icona_riquadro(void) { return grande ? IC_M : IC_S; }

void schermate_pagina(int p)
{
    pagina = p < 0 ? 0 : p;
    /* Vale per tutte le schermate paginate, non solo per questa: chi cattura
       chiede «la pagina N» e si aspetta la pagina N di quello che sta
       guardando. */
    schermata_clima_pagina(p);
}

static void su_pagina(int p)
{
    pagina = p;
    ui_vai_a(SEZ_CLIMA, ui_vista());   /* la vista porta gia l'indice */
}

static void su_indietro(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_vai_a(SEZ_CLIMA, 1);     /* torna al gruppo Condizionatori */
}

/* --- riquadri e tasti --------------------------------------------------- */

/* `peso` e quante righe di comandi contiene il blocco, non una misura: il
   deflettore ne ha due — le cinque posizioni e i quattro tasti sotto — e a
   parita di spazio con gli altri la griglia delle posizioni finiva tagliata.
   E una proporzione, quindi sta qui e non in profile.h. */
static lv_obj_t *blocco(lv_obj_t *padre, const char *occhiello, int peso)
{
    lv_obj_t *b = ui_scheda(padre);
    lv_obj_set_width(b, LV_PCT(100));
    if (colonna_unica || peso == 0) lv_obj_set_height(b, LV_SIZE_CONTENT);
    else                            lv_obj_set_flex_grow(b, peso);
    lv_obj_set_style_pad_row(b, PRF->geo.gap, 0);
    ui_occhiello(b, occhiello);
    return b;
}

/* Un riquadro scelto prende il colore della modalita; gli altri restano
   spenti. E il modo per vedere da lontano com'e impostata l'unita. */
static lv_obj_t *riquadro(lv_obj_t *padre, bool scelto)
{
    lv_obj_t *t = ui_pannello(padre, scelto ? accento : C_CARD2);
    lv_obj_set_style_radius(t, PRF->geo.radius_tile, 0);
    ui_bordo(t, LV_BORDER_SIDE_FULL, scelto ? accento : C_LINE);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(t, COM.pastiglia_gap, 0);
    lv_obj_set_style_pad_all(t, COM.pastiglia_pad_v, 0);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    return t;
}

/* `righe` > 1 lays the tiles on several rows, filling row by row: whoever
   places the cells uses cella_riquadro(). Fixed, each row is `alta` tall. */
static lv_obj_t *griglia_riquadri(lv_obj_t *padre, int colonne, int righe,
                                  bool fisso, int32_t alta)
{
    lv_obj_t *g = ui_griglia(padre, colonne, righe);
    lv_obj_set_width(g, LV_PCT(100));
    if (colonna_unica || fisso)
        lv_obj_set_height(g, alta * righe + PRF->geo.gap * (righe - 1));
    else
        lv_obj_set_flex_grow(g, 1);
    return g;
}

static void cella_riquadro(lv_obj_t *t, int n, int colonne)
{
    lv_obj_set_grid_cell(t, LV_GRID_ALIGN_STRETCH, n % colonne, 1,
                            LV_GRID_ALIGN_STRETCH, n / colonne, 1);
}

/* --- colonna sinistra --------------------------------------------------- */

/* --- dal tocco al condizionatore ---------------------------------------
 *
 * L'indice dell'unita e uno solo per tutta la schermata, quindi sta qui
 * invece che dentro il dato di ogni evento: cosi ogni gestore porta il
 * proprio valore — quale modo, quale velocita — e non deve impacchettare
 * due cose in un puntatore.
 *
 * Dopo ogni comando si ricostruisce: lo stato vero arrivera da Home
 * Assistant col prossimo evento, ma il riquadro scelto deve accendersi
 * subito o il tocco sembra non aver fatto niente. Quello che si ridisegna e
 * quello che il pannello **sa**, non quello che spera. */
static int unita_mostrata;

static void rifai(void)
{
    dati_ricarica();
    ui_vai_a(ui_dove(), ui_vista());
}

static void su_modo(lv_event_t *e)
{
    dati_condizionatore_modo(unita_mostrata,
                             (modo_clima_t)(intptr_t)lv_event_get_user_data(e));
    rifai();
}

static void su_ventilazione(lv_event_t *e)
{
    dati_condizionatore_ventilazione(
        unita_mostrata, (ventilazione_t)(intptr_t)lv_event_get_user_data(e));
    rifai();
}

/* Posizione e oscillazione viaggiano insieme perche insieme fanno una sola
   parola di swing_mode: mandarle in due comandi vorrebbe dire un passaggio
   intermedio che l'apparecchio non ha mai avuto. */
static void su_deflettore(lv_event_t *e)
{
    const int v = (int)(intptr_t)lv_event_get_user_data(e);
    dati_condizionatore_deflettore(unita_mostrata, (deflettore_t)(v & 0xFF),
                                   (oscillazione_t)(v >> 8));
    rifai();
}

static void su_extra(lv_event_t *e)
{
    const int quale = (int)(intptr_t)lv_event_get_user_data(e);
    const condizionatore_t *u = dati_condizionatore(unita_mostrata);
    if (u) dati_condizionatore_extra(unita_mostrata, quale, !u->extra[quale]);
    rifai();
}

static void su_setpoint(lv_event_t *e)
{
    const int verso = (int)(intptr_t)lv_event_get_user_data(e);
    const condizionatore_t *u = dati_condizionatore(unita_mostrata);
    if (!u) return;
    const limiti_t l = dati_limiti_condizionatori();
    dati_condizionatore_imposta(unita_mostrata,
                                (int16_t)(u->richiesta + verso * l.passo));
    rifai();
}

/* Un quarto d'ora per pressione. Non e una misura di layout — non sta in
   profile.h — e non e un numero a caso: e il passo con cui si ragiona su
   quanto tenere acceso un condizionatore. Cinque minuti vorrebbero dire
   sei pressioni per mezz'ora, un'ora sarebbe troppo grossa per
   correggere. */
#define TIMER_PASSO_MIN 15

/* La durata scelta sul vetro, per l'unita che si sta guardando.
 *
 * Non e in configurazione e non e in Home Assistant: e cosa si sta per
 * chiedere, e vive quanto la pagina. `durata_predefinita` la fa partire da
 * un valore sensato invece che da zero; si torna li da soli cambiando
 * unita, perche una durata scelta per la mansarda non vuol dire niente per
 * la camera dei bambini. */
static int durata_scelta_min;
static int durata_scelta_di = -1;

static void durata_scelta_prepara(const condizionatore_t *u)
{
    if (durata_scelta_di == unita_mostrata && durata_scelta_min > 0) return;
    durata_scelta_di  = unita_mostrata;
    durata_scelta_min = u->durata_min ? u->durata_min : 90;
}

/* Un quarto d'ora e mezza giornata: sotto non si ragiona, sopra non e piu
   un timer di spegnimento ma una programmazione, che e un'altra cosa e non
   si fa da qui. */
#define TIMER_MIN_MIN  15
#define TIMER_MAX_MIN  (12 * 60)

static void su_timer_passo(lv_event_t *e)
{
    const int verso = (int)(intptr_t)lv_event_get_user_data(e);
    const condizionatore_t *u = dati_condizionatore(unita_mostrata);
    if (!u) return;

    if (u->timer_corre) {
        /* Corre: si allunga o si accorcia quello che sta correndo. */
        dati_condizionatore_timer_regola(unita_mostrata,
                                         (int16_t)(verso * TIMER_PASSO_MIN));
    } else {
        /* Fermo: non si comanda niente, si sceglie per quanto avviarlo.
           Prima questi due pulsanti erano spenti e non c'era modo di far
           partire un timer dal pannello: si poteva solo guardare uno
           avviato da qualcun altro. */
        durata_scelta_prepara(u);
        durata_scelta_min += verso * TIMER_PASSO_MIN;
        if (durata_scelta_min < TIMER_MIN_MIN) durata_scelta_min = TIMER_MIN_MIN;
        if (durata_scelta_min > TIMER_MAX_MIN) durata_scelta_min = TIMER_MAX_MIN;
    }
    rifai();
}

static void su_timer_avvio(lv_event_t *e)
{
    LV_UNUSED(e);
    const condizionatore_t *u = dati_condizionatore(unita_mostrata);
    if (!u) return;

    if (u->timer_corre) {
        dati_condizionatore_timer_annulla(unita_mostrata);
    } else {
        durata_scelta_prepara(u);
        dati_condizionatore_timer_avvia(unita_mostrata,
                                        (uint16_t)durata_scelta_min);
    }
    rifai();
}

static void su_automazione(lv_event_t *e)
{
    LV_UNUSED(e);
    const condizionatore_t *u = dati_condizionatore(unita_mostrata);
    if (u) dati_condizionatore_automazione(unita_mostrata,
                                           !u->automazione_attiva);
    rifai();
}

static void riquadro_temperatura(lv_obj_t *padre, const condizionatore_t *u)
{
    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_width(k, LV_PCT(100));
    if (colonna_unica) lv_obj_set_height(k, LV_SIZE_CONTENT);
    else               lv_obj_set_flex_grow(k, 1);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    ui_testo(k, tr(TX_CLIMATE_ROOM_TEMPERATURE), C_DIM, FT_S);

    static char v[12];
    lv_snprintf(v, sizeof v, "%s°", ui_temp(u->stanza));
    ui_testo(k, v, u->modo == MODO_SPENTO ? C_DIM : accento, FT_CLIMA);

    const char *stato;
    switch (u->modo) {
    case MODO_FREDDO:      stato = tr(TX_CLIMATE_STATE_COOLING); break;
    case MODO_CALDO:       stato = tr(TX_CLIMATE_STATE_HEATING); break;
    case MODO_DEUMIDIFICA: stato = tr(TX_CLIMATE_STATE_DRYING); break;
    case MODO_VENTILATORE: stato = tr(TX_CLIMATE_STATE_FAN_ONLY); break;
    case MODO_AUTO:        stato = tr(TX_CLIMATE_STATE_AUTO); break;
    default:               stato = tr(TX_CLIMATE_STATE_OFF); break;
    }
    ui_testo(k, stato, C_DIM, FT_M);
}

static void riquadro_setpoint(lv_obj_t *padre, const condizionatore_t *u)
{
    const limiti_t lim = dati_limiti_condizionatori();
    const bool spento = u->modo == MODO_SPENTO;

    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    const misura_t m = PRF->tocco.clima_pm_dettaglio;

    for (int lato = 0; lato < 2; lato++) {
        if (lato == 1) {
            lv_obj_t *mezzo = ui_pannello(k, C_CARD);
            lv_obj_set_flex_grow(mezzo, 1);
            lv_obj_set_height(mezzo, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(mezzo, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(mezzo, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(mezzo, PRF->geo.gap, 0);

            ui_temperatura(mezzo, u->richiesta, spento ? C_DIM : C_TXT,
                           FT_XL);
            ui_testo(mezzo, tr(TX_CLIMATE_SET_TEMPERATURE), C_DIM, FT_S);
        }

        const bool vivo = !spento &&
            (lato == 0 ? u->richiesta > lim.min : u->richiesta < lim.max);

        lv_obj_t *b = ui_pannello(k, C_CARD2);
        lv_obj_set_size(b, m.w, m.h);
        lv_obj_set_style_radius(b, PRF->geo.radius, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        ui_icona(b, lato == 0 ? ICO_REMOVE : ICO_ADD, vivo ? C_TXT : C_OFF,
                 IC_M);
        if (vivo) {
            lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(b, su_setpoint, LV_EVENT_CLICKED,
                                (void *)(intptr_t)(lato == 0 ? -1 : 1));
        }
    }
}

static void riquadro_timer(lv_obj_t *padre, const condizionatore_t *u)
{
    /* La riga compare solo se la configurazione ha timer e automazione
       insieme: mezza funzione e peggio di nessuna. */
    if (!u->ha_timer) return;

    durata_scelta_prepara(u);

    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    /* intestazione con l'interruttore che agisce sull'automazione */
    lv_obj_t *hd = ui_pannello(k, C_CARD);
    lv_obj_set_width(hd, LV_PCT(100));
    lv_obj_set_height(hd, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hd, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hd, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hd, PRF->geo.gap, 0);

    ui_icona(hd, ICO_TIMER, C_ACC, IC_S);
    ui_testo(hd, tr(TX_CLIMATE_OFF_TIMER), C_TXT, FT_M);
    ui_spazio(hd);
    lv_obj_t *sw = interruttore(hd, u->automazione_attiva, true);
    /* Agisce sull'**automazione**, non sul timer: sono due entita
       indipendenti, e spegnere il timer lascerebbe l'automazione pronta a
       scattare su un timer che non corre piu. */
    lv_obj_add_event_cb(sw, su_automazione, LV_EVENT_CLICKED, NULL);

    /* durata regolabile */
    lv_obj_t *body = ui_pannello(k, C_CARD);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_height(body, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int lato = 0; lato < 2; lato++) {
        if (lato == 1) {
            lv_obj_t *mezzo = ui_pannello(body, C_CARD);
            lv_obj_set_flex_grow(mezzo, 1);
            lv_obj_set_height(mezzo, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(mezzo, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(mezzo, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(mezzo, COM.pastiglia_gap, 0);

            /* Il numero e quello che i due pulsanti muovono, e cambia
               con la situazione: a timer fermo e la durata che verra
               usata, a timer che corre e quanto manca — perche `timer.change`
               allunga o accorcia **quello che sta correndo**, e mostrare la
               durata impostata mentre si preme "+" farebbe sembrare che i
               pulsanti non facciano niente. */
            const unsigned mostra = u->timer_corre
                                    ? u->restano_min
                                    : (unsigned)durata_scelta_min;
            static char d[12];
            lv_snprintf(d, sizeof d, "%u:%02u", mostra / 60, mostra % 60);
            ui_testo(mezzo, d, C_TXT, FT_L);
            ui_testo(mezzo, u->timer_corre ? tr(TX_CLIMATE_REMAINING) : tr(TX_CLIMATE_HOURS_MINUTES),
                     C_DIM, FT_S);
        }
        lv_obj_t *b = ui_pannello(body, C_CARD2);
        lv_obj_set_size(b, PRF->clima.timer_passo.w, PRF->clima.timer_passo.h);
        lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        /* Vivi sempre, ma non per la stessa ragione: a timer che corre
           allungano o accorciano quello che sta correndo, a timer fermo
           scelgono per quanto avviarlo. Erano spenti da fermo — corretto
           finche l'unica cosa che sapevano fare era `timer.change`, che su
           un timer fermo da errore — e il risultato era un riquadro che dal
           pannello non si poteva usare in nessun modo. */
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, su_timer_passo, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(lato == 0 ? -1 : +1));
        ui_icona(b, lato == 0 ? ICO_REMOVE : ICO_ADD, C_DIM, IC_S);
    }

    /* --- avvia / annulla ------------------------------------------------
     *
     * Mancava, e senza di lui il riquadro era una vetrina: si vedeva un
     * timer avviato altrove e non se ne poteva avviare uno. Icone gia in
     * carattere — ICO_TIMER e ICO_CLOSE — perche aggiungerne una vuol dire
     * rigenerare i font, e non vale un disegno piu preciso. */
    {
        lv_obj_t *az = ui_pannello(k, C_CARD2);
        lv_obj_set_width(az, LV_PCT(100));
        lv_obj_set_height(az, PRF->tocco.apertura.h);
        lv_obj_set_style_radius(az, PRF->geo.radius_btn, 0);
        ui_bordo(az, LV_BORDER_SIDE_FULL, u->timer_corre ? C_LINE : C_ACC);
        lv_obj_set_flex_flow(az, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(az, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(az, PRF->geo.gap, 0);

        static char avvia[24];
        lv_snprintf(avvia, sizeof avvia, tr(TX_CLIMATE_START_FOR),
                    (unsigned)durata_scelta_min / 60,
                    (unsigned)durata_scelta_min % 60);

        ui_icona(az, u->timer_corre ? ICO_CLOSE : ICO_TIMER,
                 u->timer_corre ? C_DIM : C_ACC, IC_S);
        ui_testo(az, u->timer_corre ? tr(TX_CLIMATE_CANCEL_TIMER) : avvia,
                 u->timer_corre ? C_DIM : C_TXT, FT_M);

        lv_obj_add_flag(az, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(az, su_timer_avvio, LV_EVENT_CLICKED, NULL);
    }

    /* Conto alla rovescia. Se il timer corre ma l'automazione e disattivata
       si mostra attenuato e si dice cosa succedera davvero: vedere i minuti
       scorrere e credere che l'unita si spegnera sarebbe un inganno. */
    /* Fermo: non si scrive niente. «Il timer non sta correndo» diceva
       un'assenza con una frase, e una frase occupa una riga e chiede di
       essere letta per scoprire che non c'era niente da leggere. Il timer
       fermo si vede dai pulsanti, che sono li sopra e sono spenti. */
    if (!u->timer_corre) return;

    static char riga[64];
    lv_snprintf(riga, sizeof riga, tr(TX_CLIMATE_TURNS_OFF_AT),
                u->spegne_alle ? u->spegne_alle : "—",
                u->restano_min / 60, u->restano_min % 60);

    if (u->automazione_attiva) {
        ui_testo(k, riga, C_DIM, FT_S);
    } else {
        lv_obj_t *l = ui_testo(k, riga, C_DIM, FT_S);
        lv_obj_set_style_opa(l, ui_opa(COM.opacita_dato_vecchio_pct), 0);
        ui_testo(k, tr(TX_CLIMATE_TIMER_NO_EFFECT), C_WARN, FT_S);
    }
}

/* --- colonna destra ----------------------------------------------------- */

static void blocco_modalita(lv_obj_t *padre, const condizionatore_t *u)
{
    static const char *const ICONE[MODO_QUANTI] = {
        ICO_POWER_SETTINGS_NEW, ICO_LOCAL_FIRE_DEPARTMENT, ICO_AC_UNIT,
        ICO_AUTORENEW, ICO_WATER_DROP, ICO_MODE_FAN,
    };

    /* In portrait it is on the first page, under the setpoint, with a
       height of its own: the temperature above takes what is left. Two
       rows of three, because six tiles in a row on 800 px leave the names
       one syllable. */
    const int colonne = grande ? MODO_QUANTI / 2 : MODO_QUANTI;
    const int righe   = grande ? 2 : 1;
    lv_obj_t *b = blocco(padre, tr(TX_CLIMATE_MODE_HEADER), grande ? 0 : 1);
    lv_obj_t *g = griglia_riquadri(b, colonne, righe, grande,
                                   PRF->clima.riquadro_h);

    for (int n = 0; n < MODO_QUANTI; n++) {
        const bool scelto = n == (int)u->modo;
        lv_obj_t *t = riquadro(g, scelto);
        cella_riquadro(t, n, colonne);
        if (u->disponibile) {
            lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(t, su_modo, LV_EVENT_CLICKED,
                                (void *)(intptr_t)n);
        }
        /* Six in a row: small icon and short name, since there is room for
           neither a big one nor the whole other. Three per row: both, and
           the full name. */
        ui_icona(t, ICONE[n], scelto ? C_INK : C_DIM, icona_riquadro());
        lv_obj_t *l = ui_testo(t, grande ? dati_modo_nome((modo_clima_t)n)
                                         : dati_modo_nome_corto((modo_clima_t)n),
                               scelto ? C_INK : C_DIM, testo_riquadro());
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
}

static void blocco_ventilazione(lv_obj_t *padre, const condizionatore_t *u)
{
    const int colonne = grande ? VENT_QUANTE / 2 : VENT_QUANTE;
    const int righe   = grande ? 2 : 1;
    lv_obj_t *b = blocco(padre, tr(TX_CLIMATE_FAN_HEADER), 1);
    lv_obj_t *g = griglia_riquadri(b, colonne, righe, false,
                                   PRF->clima.riquadro_h);

    for (int n = 0; n < VENT_QUANTE; n++) {
        const bool scelto = n == (int)u->ventilazione;
        lv_obj_t *t = riquadro(g, scelto);
        cella_riquadro(t, n, colonne);
        if (u->disponibile) {
            lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(t, su_ventilazione, LV_EVENT_CLICKED,
                                (void *)(intptr_t)n);
        }

        /* Indicatore a barrette: quante ne sono piene dice la velocita.
           Auto le mostra tutte in tratteggio di intensita media, perche non
           e una velocita ma una decisione dell'unita. */
        /* In portrait one and a half times, like the text next to them: in
           a tile a fifth of the screen tall the usual bars were a mark one
           had to look for. */
        const int32_t bw = grande ? COM.barretta_w * 3 / 2 : COM.barretta_w;
        const int32_t bh = grande ? COM.barretta_h * 3 / 2 : COM.barretta_h;
        const int32_t bg = grande ? COM.barretta_gap * 3 / 2 : COM.barretta_gap;

        lv_obj_t *barre = ui_pannello(t, C_CARD2);
        lv_obj_set_style_bg_opa(barre, LV_OPA_TRANSP, 0);
        lv_obj_set_size(barre, LV_SIZE_CONTENT, bh);
        lv_obj_set_flex_flow(barre, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(barre, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_column(barre, bg, 0);

        for (int k = 1; k < VENT_QUANTE; k++) {
            const bool piena = n == VENT_AUTO || k <= n;
            lv_obj_t *i = ui_pannello(barre, scelto ? C_INK
                                                    : (piena ? C_DIM : C_OFF));
            lv_obj_set_size(i, bw, bh * k / (VENT_QUANTE - 1));
            lv_obj_set_style_radius(i, LV_RADIUS_CIRCLE, 0);
            if (!piena) lv_obj_set_style_opa(i, ui_opa(COM.opacita_dato_vecchio_pct), 0);
        }

        lv_obj_t *l = ui_testo(t, dati_ventilazione_nome((ventilazione_t)n),
                               scelto ? C_INK : C_DIM, testo_riquadro());
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);

        /* Le barrette sono pannelli, e un pannello nasce cliccabile: senza
           questa riga il riquadro rispondeva solo sul nome sotto. */
        if (u->disponibile) ui_tocco_su_tutto(t);
    }
}

static void blocco_deflettore(lv_obj_t *padre, const condizionatore_t *u)
{
    /* In portrait it grows together with the fan and they share the room
       the mode left: if only the fan grew, its tiles would become columns a
       quarter of the screen tall. */
    lv_obj_t *b = blocco(padre, tr(TX_CLIMATE_VANE_HEADER), grande ? 1 : 0);

    lv_obj_t *g = griglia_riquadri(b, DEFL_QUANTE, 1, !grande,
                                   PRF->clima.riquadro_h);
    for (int n = 0; n < DEFL_QUANTE; n++) {
        const bool scelto = n == (int)u->deflettore;
        lv_obj_t *t = riquadro(g, scelto);
        cella_riquadro(t, n, DEFL_QUANTE);
        if (u->disponibile) {
            lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
            /* Si cambia la posizione tenendo l'oscillazione com'e: chi tocca
               "in alto" non sta chiedendo anche di smettere di oscillare. */
            lv_obj_add_event_cb(t, su_deflettore, LV_EVENT_CLICKED,
                                (void *)(intptr_t)(n | (u->oscillazione << 8)));
        }
        deflettore_disegno(t, (deflettore_t)n, scelto ? C_INK : C_DIM);
        lv_obj_t *l = ui_testo(t, dati_deflettore_nome((deflettore_t)n),
                               scelto ? C_INK : C_DIM, testo_riquadro());
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);

        /* Il disegno della lamella e fatto di pannelli: stessa storia. */
        if (u->disponibile) ui_tocco_su_tutto(t);
    }

    /* I quattro tasti che completano le dodici combinazioni reali. */
    static const tx_t NOMI[4] = {
        TX_CLIMATE_VANE_FIXED, TX_CLIMATE_SWINGING,
        TX_CLIMATE_VANE_FULL_RANGE, TX_CLIMATE_VANE_DEFAULT,
    };
    static const char *const ICONE[4] = {
        ICO_HEIGHT, ICO_SWAP_VERT, ICO_WIND_POWER, ICO_AUTORENEW,
    };

    /* In portrait two rows of two: with bigger text, four in a row would
       cut «Full swing» in half. */
    const int seg_col = grande ? 2 : 4;
    const int seg_rig = grande ? 2 : 1;
    const int32_t seg_h = PRF->clima.segmento_h;
    lv_obj_t *seg = ui_griglia(b, seg_col, seg_rig);
    lv_obj_set_width(seg, LV_PCT(100));
    lv_obj_set_height(seg, seg_h * seg_rig + PRF->geo.gap * (seg_rig - 1));

    for (int n = 0; n < 4; n++) {
        const bool scelto = n == (int)u->oscillazione;
        lv_obj_t *t = ui_pannello(seg, scelto ? accento : C_CARD2);
        cella_riquadro(t, n, seg_col);
        lv_obj_set_style_min_width(t, 0, 0);
        lv_obj_set_style_radius(t, PRF->geo.radius_btn, 0);
        ui_bordo(t, LV_BORDER_SIDE_FULL, scelto ? accento : C_LINE);
        lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(t, COM.pastiglia_gap, 0);
        lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
        if (u->disponibile)
            lv_obj_add_event_cb(t, su_deflettore, LV_EVENT_CLICKED,
                                (void *)(intptr_t)(u->deflettore | (n << 8)));

        ui_icona(t, ICONE[n], scelto ? C_INK : C_DIM, icona_riquadro());
        lv_obj_t *l = ui_testo(t, tr(NOMI[n]), scelto ? C_INK : C_DIM,
                               testo_riquadro());
        /* Senza una larghezza LV_LABEL_LONG_DOT non ha niente su cui
           decidere: l'etichetta resta lunga quanto il testo e a tagliarla e
           il riquadro, senza puntini. "Tutta l'escursione" in un riquadro
           stretto diventava "Tutta l'escursion" e sembrava un errore di
           battitura. */
        lv_obj_set_flex_grow(l, 1);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
}

static void blocco_extra(lv_obj_t *padre, const condizionatore_t *u)
{
    lv_obj_t *f = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_height(f, PRF->clima.extra_h);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f, PRF->geo.gap, 0);

    static const char *const ICONE[4] = {
        ICO_VOLUME_OFF, ICO_LIGHT_MODE, ICO_AIR, ICO_MODE_FAN,
    };

    for (int n = 0; n < 4; n++) {
        /* Un interruttore Gree che la configurazione non ha non si mostra
           spento: non si mostra affatto. */
        if (!u->ha_extra[n]) continue;

        const bool acceso = u->extra[n];
        lv_obj_t *t = ui_pannello(f, acceso ? C_SEL_BG : C_CARD2);
        lv_obj_set_flex_grow(t, 1);
        lv_obj_set_height(t, LV_PCT(100));
        lv_obj_set_style_min_width(t, 0, 0);
        lv_obj_set_style_radius(t, PRF->geo.radius_btn, 0);
        ui_bordo(t, LV_BORDER_SIDE_FULL, acceso ? C_SEL_LINE : C_LINE);
        lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(t, COM.pastiglia_gap, 0);
        lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
        if (u->disponibile)
            lv_obj_add_event_cb(t, su_extra, LV_EVENT_CLICKED,
                                (void *)(intptr_t)n);

        ui_icona(t, ICONE[n], acceso ? C_ACC : C_DIM, icona_riquadro());
        lv_obj_t *l = ui_testo(t, dati_extra_nome(n), acceso ? C_ACC : C_DIM,
                               testo_riquadro());
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    }
}

/* --- costruzione -------------------------------------------------------- */

void schermata_clima_dettaglio(lv_obj_t *c, int indice)
{
    const condizionatore_t *u = dati_condizionatore(indice);
    if (!u) { ui_vai_a(SEZ_CLIMA, 1); return; }

    unita_mostrata = indice;

    accento = u->modo == MODO_FREDDO || u->modo == MODO_DEUMIDIFICA ? C_COOL
            : u->modo == MODO_CALDO                                 ? C_CALDO
            : u->modo == MODO_SPENTO                                ? C_DIM
                                                                    : C_TXT;

    static char titolo[48];
    lv_snprintf(titolo, sizeof titolo, tr(TX_CLIMATE_UNIT_TITLE), u->nome);
    static char sotto[64];
    lv_snprintf(sotto, sizeof sotto, tr(TX_CLIMATE_DETAIL_SUBTITLE),
                dati_modo_nome(u->modo), ui_temp_compatta(u->richiesta),
                dati_ventilazione_nome(u->ventilazione));
    /* In verticale la testata deve reggere il paginatore **e** "← Zone", e
       il sottotitolo non ci sta piu. Va via lui perche e l'unica cosa qui
       dentro che si legge anche altrove: mode and setpoint are on the
       first page, the fan on the second. */
    const bool due_pagine = PRF->clima.dettaglio_col_w == 0;
    grande = due_pagine;
    ui_testata(titolo, due_pagine ? NULL : sotto);

    /* La testata destra si svuota una volta sola: chiamarla due volte
       cancellerebbe quello che ci si e appena messo. */
    lv_obj_t *td = ui_testata_destra();

    const int pagine = due_pagine ? 2 : 1;
    if (pagina >= pagine) pagina = 0;
    if (pagine > 1) paginatore_frecce(td, pagina, pagine, su_pagina);

    /* "← Zone" al posto del tasto indietro generico. Dopo il paginatore,
       cosi resta all'estremita destra dov'era prima. */
    lv_obj_t *dietro = ui_pannello(td, C_CARD2);
    lv_obj_set_size(dietro, LV_SIZE_CONTENT, PRF->tocco.pager.h);
    lv_obj_set_style_pad_hor(dietro, PRF->geo.gap, 0);
    lv_obj_set_style_radius(dietro, PRF->geo.radius_btn, 0);
    ui_bordo(dietro, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_flow(dietro, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dietro, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dietro, COM.pastiglia_gap, 0);
    lv_obj_add_flag(dietro, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dietro, su_indietro, LV_EVENT_CLICKED, NULL);
    ui_icona(dietro, ICO_ARROW_BACK, C_TXT, IC_S);
    ui_testo(dietro, tr(TX_CLIMATE_BACK_TO_ZONES), C_TXT, FT_M);

    /* Ogni pagina ha l'altezza intera: i riquadri si dividono lo spazio
       come nella colonna dell'orizzontale, e non serve piu dare loro
       un'altezza fissa. */
    const bool due_colonne = !due_pagine;
    colonna_unica = false;

    lv_obj_t *fuori = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(fuori, LV_OPA_TRANSP, 0);
    lv_obj_set_width(fuori, LV_PCT(100));
    /* Non tutta l'altezza: sotto ci vanno i pallini, e con LV_PCT(100) si
       ritroverebbero fuori dal vetro. */
    lv_obj_set_flex_grow(fuori, 1);
    lv_obj_set_flex_flow(fuori, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(fuori, PRF->geo.gap, 0);

    /* What is looked at and touched most: temperature, setpoint and timer —
       plus, in portrait, the mode. In landscape it is the left column, in
       portrait the first page. */
    if (due_colonne || pagina == 0) {
        lv_obj_t *sinistra = ui_pannello(fuori, C_BG);
        lv_obj_set_style_bg_opa(sinistra, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_flow(sinistra, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(sinistra, PRF->geo.gap, 0);
        lv_obj_set_height(sinistra, LV_PCT(100));
        if (due_colonne) lv_obj_set_width(sinistra, PRF->clima.dettaglio_col_w);
        else             lv_obj_set_flex_grow(sinistra, 1);

        riquadro_temperatura(sinistra, u);
        riquadro_setpoint(sinistra, u);
        if (due_pagine) blocco_modalita(sinistra, u);
        riquadro_timer(sinistra, u);
    }

    /* Quello che si imposta. */
    if (due_colonne || pagina == 1) {
        lv_obj_t *destra = ui_pannello(fuori, C_BG);
        lv_obj_set_style_bg_opa(destra, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_flow(destra, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(destra, PRF->geo.gap, 0);
        lv_obj_set_flex_grow(destra, 1);
        lv_obj_set_height(destra, LV_PCT(100));

        if (due_colonne) blocco_modalita(destra, u);
        blocco_ventilazione(destra, u);
        blocco_deflettore(destra, u);
        blocco_extra(destra, u);
    }

    paginatore_pallini(c, pagina, pagine);
}

int schermata_clima_vista_dettaglio(int indice) { return VISTA_DETTAGLIO + indice; }
