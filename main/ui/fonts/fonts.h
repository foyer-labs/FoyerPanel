/* ------------------------------------------------------------------------
 * fonts.h — i corpi tipografici di 09-profili.md §4.
 *
 * GENERATO da tools/genera_font.py. Non modificare a mano.
 *
 * Il codice non nomina mai un corpo: chiede un *ruolo* e il profilo attivo
 * decide quale font compilato serve.
 *
 *     lv_obj_set_style_text_font(l, font(FT_M), 0);
 *
 * I numeri che compaiono nei .c generati sono identita di font, non misure
 * di layout: quelle stanno solo in profile.h.
 * --------------------------------------------------------------------- */
#ifndef FONTS_H
#define FONTS_H

#include "lvgl.h"

typedef enum {
    FT_XXL,
    FT_XL,
    FT_L,
    FT_M,
    FT_S,
    FT_XS,
    FT_STANDBY,
    FT_STANDBY_TEMP,
    FT_STANDBY_1,
    FT_STANDBY_2,
    FT_STANDBY_3,
    FT_STANDBY_4,
    FT_STANDBY_5,
    FT_ENERGIA,
    FT_CLIMA,
    FT_MONO,
    FT_DECIMI_XL,
    FT_QUANTI,
} font_ruolo_t;

/* Il font del ruolo, per il profilo attivo. Mai NULL. */
const lv_font_t *font(font_ruolo_t ruolo);

#endif /* FONTS_H */
