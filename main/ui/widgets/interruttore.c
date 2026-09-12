#include "interruttore.h"

#include "comuni.h"

lv_obj_t *interruttore(lv_obj_t *padre, bool acceso, bool attivo)
{
    const uint16_t w = PRF->tocco.interruttore.w;
    const uint16_t h = PRF->tocco.interruttore.h;

    lv_obj_t *t = ui_pannello(padre, acceso ? C_ACC : C_OFF);
    lv_obj_set_size(t, w, h);
    lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, 0);
    ui_tocco_minimo(t, w, h);

    /* Il pallino sta dentro la traccia con un margine di un bordo per lato:
       e il rapporto che disegnano i mockup, e resta giusto su entrambi i
       pannelli perche discende dall'altezza della traccia. */
    const int32_t margine = COM.bordo * 3;
    const int32_t d = h - margine * 2;

    lv_obj_t *p = ui_pannello(t, acceso ? C_INK : C_DIM);
    lv_obj_set_size(p, d, d);
    lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(p, acceso ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID,
                 acceso ? -margine : margine, 0);

    /* --- il pallino non deve mangiarsi il tocco -----------------------
     *
     * Il pallino e un pannello, e un pannello nasce cliccabile: chi mira il
     * pallino — cioe chiunque, perche e la parte che si muove — colpiva un
     * oggetto senza gestore e non succedeva niente. Chi prendeva la traccia
     * intorno riusciva. Da fuori si vede come un interruttore che vuole due
     * tocchi, e non e chiaro perche uno funzioni e l'altro no. */
    if (attivo) ui_tocco_su_tutto(t);
    else        lv_obj_set_style_opa(t, ui_opa(COM.opacita_dato_vecchio_pct), 0);
    return t;
}
