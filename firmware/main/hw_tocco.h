/* Tocco — solo pannello. Il chip lo crea hw_scheda.c; qui si legge il dito. */
#ifndef HW_TOCCO_H
#define HW_TOCCO_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "lvgl.h"

/* Registra il tocco come dispositivo di ingresso di LVGL. Da chiamare
   **dopo** hw_lcd_avvia(), che crea il display a cui si aggancia, e dopo il
   reset del tocco, che decide a quale indirizzo risponde. */
bool hw_tocco_avvia(i2c_master_bus_handle_t bus);

/* Quanti byte di pila non ha mai usato il compito che legge il tocco.

   Serve a decidere quanto costa davvero, invece di tirare a indovinare: su
   questo pannello la RAM interna e la risorsa scarsa — l'ultima misura
   diceva diciassette kilobyte liberi in tutto — e una pila dimensionata a
   occhio e o uno spreco su una riserva che non c'e, o un guasto che compare
   il giorno che il chip risponde piu lentamente. Il numero si legge con
   `stato`. */
uint32_t hw_tocco_pila_libera(void);

/* Stampa gli ultimi tocchi **consegnati a LVGL**, con coordinate e istante.
   Non quelli letti dal chip: e la differenza fra i due che dice se un dito
   si e perso per strada, e da quale parte. Il comando di console e `tocco`.

   Provvisorio: sta qui finche i tocchi non sono a posto. */
void hw_tocco_racconta(void);

#endif /* HW_TOCCO_H */
