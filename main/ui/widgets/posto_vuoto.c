#include "posto_vuoto.h"

#include "comuni.h"

static void libera(lv_event_t *e)
{
    lv_free(lv_obj_get_user_data(lv_event_get_target(e)));
}

/* I quattro lati, ridisegnati quando il riquadro cambia dimensione: nella
   griglia la dimensione arriva dopo la costruzione, non prima. */
static void ridisegna(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    const int32_t w = lv_obj_get_width(o);
    const int32_t h = lv_obj_get_height(o);
    if (w <= 0 || h <= 0) return;

    lv_point_precise_t *punti = lv_obj_get_user_data(o);
    if (!punti) return;

    const int32_t r = PRF->geo.radius;   /* i lati partono dopo la curva */
    const int32_t x0 = r, x1 = w - r, y0 = r, y1 = h - r;

    punti[0] = (lv_point_precise_t){ x0, 0     };
    punti[1] = (lv_point_precise_t){ x1, 0     };
    punti[2] = (lv_point_precise_t){ x0, h - COM.bordo };
    punti[3] = (lv_point_precise_t){ x1, h - COM.bordo };
    punti[4] = (lv_point_precise_t){ 0,  y0    };
    punti[5] = (lv_point_precise_t){ 0,  y1    };
    punti[6] = (lv_point_precise_t){ w - COM.bordo, y0 };
    punti[7] = (lv_point_precise_t){ w - COM.bordo, y1 };

    for (uint32_t n = 0; n < lv_obj_get_child_count(o); n++)
        lv_line_set_points(lv_obj_get_child(o, n), &punti[n * 2], 2);
}

void posto_vuoto(lv_obj_t *o)
{
    /* Niente sfondo e niente bordo pieno: resta solo il tratteggio. */
    lv_obj_clean(o);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_layout(o, LV_LAYOUT_NONE);

    /* Gli otto vertici vivono quanto il riquadro: LVGL tiene un puntatore ai
       punti della linea, non una copia, quindi non possono stare sullo
       stack ne in un giro di buffer statici. */
    lv_point_precise_t *punti = lv_malloc_zeroed(sizeof *punti * 8);
    if (!punti) return;
    lv_obj_set_user_data(o, punti);
    lv_obj_add_event_cb(o, libera, LV_EVENT_DELETE, NULL);

    for (int n = 0; n < 4; n++) {
        lv_obj_t *l = lv_line_create(o);
        lv_obj_set_style_line_color(l, C_OFF, 0);
        lv_obj_set_style_line_width(l, COM.bordo, 0);
        lv_obj_set_style_line_dash_width(l, COM.tratteggio_segno, 0);
        lv_obj_set_style_line_dash_gap(l, COM.tratteggio_vuoto, 0);
        lv_obj_set_style_line_opa(l, ui_opa(COM.opacita_dato_vecchio_pct), 0);
    }

    lv_obj_add_event_cb(o, ridisegna, LV_EVENT_SIZE_CHANGED, NULL);
    lv_obj_add_event_cb(o, ridisegna, LV_EVENT_LAYOUT_CHANGED, NULL);
}
