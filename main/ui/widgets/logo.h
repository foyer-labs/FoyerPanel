/* ------------------------------------------------------------------------
 * Foyer's logo, drawn by LVGL.
 *
 * Not an image: the same lines, arcs and rectangles as the SVGs in
 * docs/logo/, read from logo_shape.h, which tools/gen_logo.py writes from a
 * single geometry. So it stays sharp at every size, takes the text colour
 * of the theme in use, and costs no image in flash for each size it is
 * shown at.
 * --------------------------------------------------------------------- */
#ifndef LOGO_H
#define LOGO_H

#include <stdbool.h>

#include "lvgl.h"

/* The logo, `height` pixels tall. With `with_name`, FOYER is written next
   to the mark; without, the mark alone in a square. `ink` is the colour of
   the waves, the threshold and the name — usually the theme's text; the
   door is always the logo's amber. */
lv_obj_t *logo_foyer(lv_obj_t *parent, int32_t height, bool with_name,
                     lv_color_t ink);

/* How wide the logo `height` tall is, for whoever must make room for it. */
int32_t logo_foyer_width(int32_t height, bool with_name);

#endif /* LOGO_H */
