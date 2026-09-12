/* ------------------------------------------------------------------------
 * Schermo MIPI-DSI e display LVGL — solo pannello.
 *
 * Sul PC il display lo crea SDL; qui c'e il vetro JD9365 su due corsie
 * DSI, con due framebuffer in PSRAM. Il perche sta in hw_lcd_dsi.c.
 * --------------------------------------------------------------------- */
#ifndef HW_LCD_H
#define HW_LCD_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

/* Accende il vetro e registra il display in LVGL. */
bool hw_lcd_avvia(void);

/* Il display LVGL, per chi deve chiedergli qualcosa. NULL prima di
   hw_lcd_avvia(). */
lv_display_t *hw_lcd_display(void);

/* --- una prova che salta LVGL ------------------------------------------
 *
 * Riempie il framebuffer di un colore solo e lo manda al vetro, senza
 * passare da niente che assomigli a un'interfaccia.
 *
 * Serve a tagliare in due un guasto che altrimenti ha troppe spiegazioni:
 * uno schermo nero puo essere un vetro che non riceve niente o un vetro
 * che riceve del nero. Se dopo questa chiamata lo schermo e rosso, la
 * catena video funziona e il problema sta piu in su; se resta nero, il
 * problema e sotto e LVGL non c'entra.
 *
 * Falso se lo schermo non e stato avviato. */
bool hw_lcd_prova(uint16_t colore_rgb565);

#endif /* HW_LCD_H */
