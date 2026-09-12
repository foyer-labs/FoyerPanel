#include "paginatore.h"

#include "comuni.h"

static pagina_cb_t richiamo;
static int         corrente, totale;

int paginatore_pagine(int elementi, int capienza)
{
    if (capienza <= 0) return 1;
    const int p = (elementi + capienza - 1) / capienza;
    return p < 1 ? 1 : p;
}

static void su_freccia(lv_event_t *e)
{
    const int passo = (int)(intptr_t)lv_event_get_user_data(e);
    int p = corrente + passo;
    /* I tasti oltre il limite non rispondono, non avvolgono: 11-collaudo.md
       §2 lo chiede per i setpoint e vale anche qui. */
    if (p < 0 || p >= totale) return;
    corrente = p;
    if (richiamo) richiamo(p);
}

static lv_obj_t *freccia(lv_obj_t *padre, const char *ico, int passo, bool viva)
{
    lv_obj_t *b = ui_pannello(padre, C_CARD2);
    lv_obj_set_size(b, PRF->tocco.pager.w, PRF->tocco.pager.h);
    lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
    ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui_tocco_minimo(b, PRF->tocco.pager.w, PRF->tocco.pager.h);

    ui_icona(b, ico, viva ? C_TXT : C_LINE, IC_S);

    if (viva) {
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, su_freccia, LV_EVENT_CLICKED,
                            (void *)(intptr_t)passo);
    }
    return b;
}

lv_obj_t *paginatore_frecce(lv_obj_t *padre, int pagina, int pagine,
                            pagina_cb_t su_cambio)
{
    if (pagine < 2) return NULL;

    corrente = pagina;
    totale = pagine;
    richiamo = su_cambio;

    lv_obj_t *p = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_set_size(p, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(p, PRF->geo.gap, 0);

    freccia(p, ICO_CHEVRON_LEFT, -1, pagina > 0);

    static char conteggio[16];
    lv_snprintf(conteggio, sizeof conteggio, "%d / %d", pagina + 1, pagine);
    ui_testo(p, conteggio, C_DIM, FT_S);

    freccia(p, ICO_CHEVRON_RIGHT, +1, pagina < pagine - 1);
    return p;
}

lv_obj_t *paginatore_pallini(lv_obj_t *padre, int pagina, int pagine)
{
    if (pagine < 2) return NULL;

    lv_obj_t *f = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_height(f, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(f, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(f, COM.gap_pallini, 0);

    for (int n = 0; n < pagine; n++) {
        lv_obj_t *d = ui_pannello(f, n == pagina ? C_ACC : C_OFF);
        /* Il pallino della pagina in corso e piu lungo, non piu grande: cosi
           la fascia dei pallini non cambia altezza e non balla. */
        lv_obj_set_size(d, n == pagina ? COM.pallino_paginatore_attivo
                                       : COM.pallino_paginatore,
                        COM.pallino_paginatore);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    }
    return f;
}
