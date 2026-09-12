/* ------------------------------------------------------------------------
 * Agenda — 01-specifica-ui.md §3.6.
 *
 * Una colonna per giorno in orizzontale — tre o quattro secondo il profilo —
 * ed **elenco unico con intestazioni di giorno** in verticale
 * (09-profili.md §1-ter e §5).
 *
 * La sezione oggi non compare nel rail perche in Home Assistant non esiste
 * nessuna entita calendar. Il codice c'e lo stesso: quando un calendario
 * arrivera bastera accendere `agenda.attiva`, e questa e la differenza fra
 * una sezione non configurata e una sezione non scritta.
 * --------------------------------------------------------------------- */
#include "comuni.h"
#include "dati.h"
#include "schermate.h"
#include "ui.h"

static lv_color_t colore_calendario(uint8_t n)
{
    switch (n) {
    case 0:  return C_CAL_1;
    case 1:  return C_CAL_2;
    case 2:  return C_CAL_3;
    default: return C_CAL_4;
    }
}

/* --- un evento ---------------------------------------------------------- */

static void evento(lv_obj_t *padre, const evento_t *e)
{
    lv_obj_t *r = ui_pannello(padre, C_CARD);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(r, COM.pastiglia_gap, 0);
    /* Gli eventi passati sono attenuati: ci sono ancora ma non chiedono
       attenzione. */
    if (e->passato)
        lv_obj_set_style_opa(r, ui_opa(COM.opacita_evento_passato_pct), 0);

    /* Barretta del colore del calendario, alta quanto la riga. */
    lv_obj_t *barra = ui_pannello(r, colore_calendario(e->colore));
    lv_obj_set_size(barra, COM.tratto, LV_PCT(100));
    lv_obj_set_style_radius(barra, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_min_height(barra, PRF->font.f_m, 0);

    lv_obj_t *ora = ui_testo(r, e->ora ? e->ora : tr(TX_AGENDA_ALL_DAY),
                             e->passato ? C_DIM : C_ACC, FT_S);
    lv_obj_set_width(ora, PRF->clima.timer_passo.w);

    lv_obj_t *col = ui_pannello(r, C_CARD);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, COM.gap_stretto, 0);

    /* Un titolo lunghissimo va a capo al massimo su due righe, poi tronca
       (11-collaudo.md §2). */
    lv_obj_t *tit = ui_testo(col, e->titolo, C_TXT, FT_M);
    lv_label_set_long_mode(tit, LV_LABEL_LONG_DOT);
    lv_obj_set_width(tit, LV_PCT(100));
    lv_obj_set_style_max_height(tit,
        lv_font_get_line_height(font(FT_M)) * COM.righe_titolo_evento, 0);

    ui_testo(col, e->calendario, C_DIM, FT_S);
}

/* --- legenda ------------------------------------------------------------ */

static void legenda(lv_obj_t *c)
{
    lv_obj_t *f = ui_pannello(c, C_CARD2);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_height(f, PRF->tocco.pager.h);
    lv_obj_set_style_radius(f, PRF->geo.radius, 0);
    ui_bordo(f, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_style_pad_hor(f, PRF->geo.pad, 0);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(f, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(f, PRF->geo.pad, 0);

    /* Al massimo quattro calendari: i colori sono quattro. */
    int quanti = dati_calendari();
    if (quanti > 4) quanti = 4;

    for (int n = 0; n < quanti; n++) {
        lv_obj_t *v = ui_pannello(f, C_CARD2);
        lv_obj_set_size(v, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(v, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(v, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(v, COM.pastiglia_gap, 0);

        lv_obj_t *p = ui_pannello(v, colore_calendario((uint8_t)n));
        lv_obj_set_size(p, COM.pallino_paginatore, COM.pallino_paginatore);
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
        ui_testo(v, dati_calendario(n), C_DIM, FT_S);
    }
}

/* --- colonne per giorno, in orizzontale --------------------------------- */

static void colonne(lv_obj_t *c)
{
    const uint8_t quante = PRF->griglia.agenda_col;

    lv_obj_t *g = ui_griglia(c, quante, 1);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_flex_grow(g, 1);

    for (uint8_t n = 0; n < quante; n++) {
        lv_obj_t *k = ui_scheda(g);
        lv_obj_set_grid_cell(k, LV_GRID_ALIGN_STRETCH, n, 1,
                                LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

        /* L'ultima colonna raccoglie tutto quello che resta della settimana. */
        const bool ultima = n == quante - 1;
        ui_occhiello(k, dati_giorno_nome(ultima ? 4 : n));

        int trovati = 0;
        for (int e = 0; e < dati_eventi_agenda(); e++) {
            const evento_t *ev = dati_evento_agenda(e);
            if (!ev) continue;
            if (ultima ? ev->giorno < n : ev->giorno != n) continue;
            evento(k, ev);
            trovati++;
        }
        if (trovati == 0) ui_testo(k, tr(TX_AGENDA_NOTHING_PLANNED), C_DIM, FT_S);
    }
}

/* --- elenco unico, in verticale ----------------------------------------- */

static void elenco(lv_obj_t *c)
{
    lv_obj_t *k = ui_scheda(c);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_flex_grow(k, 1);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);
    lv_obj_add_flag(k, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(k, LV_DIR_VER);

    int giorno_scritto = -1;
    for (int e = 0; e < dati_eventi_agenda(); e++) {
        const evento_t *ev = dati_evento_agenda(e);
        if (!ev) continue;
        /* Intestazione di giorno solo quando il giorno cambia: e cosi che un
           elenco unico resta leggibile come quattro colonne. */
        if (ev->giorno != giorno_scritto) {
            giorno_scritto = ev->giorno;
            ui_occhiello(k, dati_giorno_nome(ev->giorno));
        }
        evento(k, ev);
    }
}

/* --- costruzione -------------------------------------------------------- */

void schermata_agenda(lv_obj_t *c)
{
    if (!dati_agenda_attiva()) {
        /* Non si arriva qui dal rail, perche la sezione non c'e. Ci si
           arriva solo dal simulatore, e allora vale la pena dire perche. */
        ui_testata(tr(TX_SECTION_AGENDA), tr(TX_AGENDA_NO_CALENDARS));
        lv_obj_t *k = ui_scheda(c);
        lv_obj_set_size(k, LV_PCT(100), LV_PCT(100));
        lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);
        ui_icona(k, ICO_CALENDAR_MONTH, C_LINE, IC_L);
        ui_testo(k, tr(TX_AGENDA_NO_CALENDAR_ENTITY),
                 C_DIM, FT_M);
        ui_testo(k, tr(TX_AGENDA_HIDDEN_UNTIL),
                 C_DIM, FT_S);
        return;
    }

    int oggi = 0, domani = 0;
    for (int e = 0; e < dati_eventi_agenda(); e++) {
        const evento_t *ev = dati_evento_agenda(e);
        if (!ev) continue;
        if (ev->giorno == 0 && !ev->passato) oggi++;
        if (ev->giorno == 1) domani++;
    }

    static char sotto[64];
    if (oggi == 0)
        lv_snprintf(sotto, sizeof sotto, tr(TX_AGENDA_NOTHING_ELSE_TODAY), domani);
    else
        lv_snprintf(sotto, sizeof sotto, trn(TXN_AGENDA_TODAY_TOMORROW, oggi),
                    oggi, domani);
    ui_testata(tr(TX_SECTION_AGENDA), sotto);

    if (PRF->griglia.agenda_col <= 1) elenco(c);
    else                              colonne(c);
    legenda(c);
}
