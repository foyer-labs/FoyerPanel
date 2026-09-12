/* ------------------------------------------------------------------------
 * Interruttore — 09-profili.md §8 per le misure.
 *
 * Non e lv_switch: quello ha uno stile suo che andrebbe smontato pezzo per
 * pezzo, e ci vorrebbero piu righe di queste. Qui e una traccia con un
 * pallino dentro, che e esattamente cio che disegnano i mockup.
 * --------------------------------------------------------------------- */
#ifndef INTERRUTTORE_H
#define INTERRUTTORE_H

#include "lvgl.h"

/* Un interruttore acceso o spento. Se `attivo` e falso resta visibile ma
   attenuato e non risponde: e il caso dell'entita non disponibile, dove il
   pannello non sa lo stato e non deve fingerlo. */
lv_obj_t *interruttore(lv_obj_t *padre, bool acceso, bool attivo);

#endif /* INTERRUTTORE_H */
