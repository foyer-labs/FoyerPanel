/* ------------------------------------------------------------------------
 * Luce, reset e tocco — i piedini della scheda, solo pannello.
 *
 * Quello che il resto del firmware chiede alla scheda senza sapere quale
 * sia: il bus I2C, la retroilluminazione, il reset del tocco, e il chip del
 * tocco dietro una maniglia esp_lcd_touch. I numeri stanno in scheda.h.
 * --------------------------------------------------------------------- */
#ifndef HW_SCHEDA_H
#define HW_SCHEDA_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_lcd_touch.h"

/* Apre il bus I2C. Il bus torna a chi lo chiama perche ci vive sopra il
   tocco, che lo riceve da hw_tocco_avvia(). */
bool hw_i2c_avvia(i2c_master_bus_handle_t *bus);

/* La retroilluminazione, accesa o spenta. */
bool hw_retro(bool accesa);

/* La luce a percentuale: sulla scheda e un PWM vero.

   Esiste perche' lo standby non deve spegnere la luce, deve abbassarla:
   uno schermo spento in corridoio sembra un pannello rotto. */
bool hw_retro_percento(uint8_t percento);

/* Il reset del tocco va fatto **prima** di parlargli: e all'uscita dal
   reset che il chip decide a quale dei due indirizzi rispondere. */
bool hw_reset_tocco(void);

/* --- il vetro capacitivo -------------------------------------------------
 *
 * Un GSL3680. Del chip il resto del firmware conosce solo cosa torna — una
 * maniglia esp_lcd_touch — e nient'altro.
 *
 * Sta qui e non in hw_tocco.c: hw_tocco.c legge dita e non deve sapere
 * quale silicio ha sotto, mentre questo file **e** la scheda. */
bool hw_tocco_crea(i2c_master_bus_handle_t bus, uint16_t larghezza,
                   uint16_t altezza, esp_lcd_touch_handle_t *fuori);

#endif /* HW_SCHEDA_H */
