/* ------------------------------------------------------------------------
 * Scelta del profilo all'avvio del simulatore.
 *
 * Sul pannello il profilo e un parametro di compilazione; nel simulatore
 * serve poterli confrontare senza ricompilare, che e il modo pratico di
 * verificare il criterio di 11-collaudo.md §1 — ogni schermata si apre in
 * entrambi i profili, e passare da orizzontale a verticale non tocca niente.
 *
 * La finestra e piccola e brutta apposta: e un attrezzo, non una schermata.
 * --------------------------------------------------------------------- */
#include "sim.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "profile.h"
#include "theme.h"

static const char *scelto;

static void su_voce(lv_event_t *e)
{
    scelto = lv_event_get_user_data(e);
}

const char *sim_scegli_profilo(void)
{
    scelto = NULL;

    lv_display_t *disp = lv_sdl_window_create(SIM_SCELTA_W, SIM_SCELTA_H);
    lv_sdl_window_set_title(disp, "Foyer Panel — choose the profile");
    lv_sdl_mouse_create();

    lv_obj_t *s = lv_display_get_screen_active(disp);
    lv_obj_remove_style_all(s);
    lv_obj_set_style_bg_color(s, C_BG, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s, SIM_SCELTA_PAD, 0);
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s, SIM_SCELTA_GAP, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(s);
    lv_label_set_text(t, "Quale pannello simulare?");
    lv_obj_set_style_text_color(t, C_TXT, 0);

    static char testo[PRF_QUANTI][64];
    for (int n = 0; n < PRF_QUANTI; n++) {
        const profilo_t *p = &PROFILI[n];
        lv_snprintf(testo[n], sizeof testo[n], "%-14s %ux%u  %s",
                    p->chiave, p->schermo.larghezza, p->schermo.altezza,
                    p->orientamento == VERTICALE ? "verticale" : "orizzontale");

        lv_obj_t *b = lv_button_create(s);
        lv_obj_set_size(b, LV_PCT(100), SIM_VOCE_H);
        lv_obj_set_style_bg_color(b, C_CARD, 0);
        lv_obj_set_style_border_color(b, C_LINE, 0);
        lv_obj_set_style_border_width(b, COM.bordo, 0);
        lv_obj_set_style_bg_color(b, C_SEL_BG, LV_STATE_PRESSED);
        lv_obj_add_event_cb(b, su_voce, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)p->chiave);

        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, testo[n]);
        lv_obj_set_style_text_color(l, C_TXT, 0);
        lv_obj_center(l);
    }

    while (!scelto) {
        uint32_t attesa = lv_timer_handler();
        if (attesa == LV_NO_TIMER_READY) attesa = LV_DEF_REFR_PERIOD;
        lv_delay_ms(attesa);
    }

    /* La finestra della scelta se ne va: quella del pannello nasce alla
       risoluzione del profilo, che e il punto di tutto l'esercizio. */
    lv_display_delete(disp);
    return scelto;
}
