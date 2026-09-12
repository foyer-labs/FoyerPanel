/* ------------------------------------------------------------------------
 * Posto libero in una griglia paginata — 01-specifica-ui.md §3.1.
 *
 * "Nell'ultima pagina i posti liberi restano vuoti e tratteggiati: le schede
 * non si allargano, cosi la posizione di ogni zona non cambia mai fra una
 * pagina e l'altra."
 *
 * LVGL non sa disegnare un bordo tratteggiato su un oggetto — il tratteggio
 * esiste solo per le linee — quindi il rettangolo si compone di quattro
 * linee tratteggiate. Sono venti righe di codice per un dettaglio che si
 * nota appena, ma e la differenza fra "qui non c'e niente" e "qui manca
 * qualcosa", e chi guarda il pannello non ha modo di chiedere.
 * --------------------------------------------------------------------- */
#ifndef POSTO_VUOTO_H
#define POSTO_VUOTO_H

#include "lvgl.h"

/* Trasforma un contenitore gia dimensionato in un posto vuoto tratteggiato.
   Va chiamata su una scheda che ha gia la sua cella nella griglia, cosi la
   dimensione resta identica a quella delle schede piene. */
void posto_vuoto(lv_obj_t *riquadro);

#endif /* POSTO_VUOTO_H */
