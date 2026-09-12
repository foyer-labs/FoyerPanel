#include "deflettore.h"

#include "comuni.h"

lv_obj_t *deflettore_disegno(lv_obj_t *padre, deflettore_t posizione,
                             lv_color_t colore)
{
    const uint16_t w = PRF->clima.deflettore.w;
    const uint16_t h = PRF->clima.deflettore.h;

    /* Corpo macchina: un rettangolo con gli angoli smussati, vuoto. */
    lv_obj_t *corpo = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(corpo, LV_OPA_TRANSP, 0);
    lv_obj_set_size(corpo, w, h);
    lv_obj_set_style_radius(corpo, PRF->geo.radius_btn, 0);
    ui_bordo(corpo, LV_BORDER_SIDE_FULL, colore);
    lv_obj_set_layout(corpo, LV_LAYOUT_NONE);

    /* Lamella: una barretta larga quanto il corpo meno il margine, che
       scende dall'alto verso il basso lungo le cinque posizioni. */
    const int32_t margine = COM.bordo * 3;
    const int32_t lam_w = w - margine * 2;
    const int32_t lam_h = COM.bordo * 3;

    /* L'escursione va dal margine superiore a quello inferiore: cinque
       posizioni equidistanti, la prima in alto e l'ultima in basso. */
    const int32_t alto = margine;
    const int32_t basso = h - margine - lam_h;
    const int32_t passo = (basso - alto) / (DEFL_QUANTE - 1);

    lv_obj_t *lamella = ui_pannello(corpo, colore);
    lv_obj_set_size(lamella, lam_w, lam_h);
    lv_obj_set_style_radius(lamella, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_pos(lamella, margine, alto + passo * (int32_t)posizione);
    return corpo;
}
