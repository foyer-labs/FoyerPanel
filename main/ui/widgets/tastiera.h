/* ------------------------------------------------------------------------
 * Tastiera QWERTY — 01-specifica-ui.md §4.9.
 *
 * "Tastiera QWERTY (misure: §8), riga funzioni con MAIUSC, CANC, 123, @#€,
 * spazio e tasto d'azione ambra."
 *
 * Non e lv_keyboard. Quella e una matrice di pulsanti con uno stile suo, e
 * per portarla al disegno approvato — il tasto d'azione ambra, le funzioni
 * distinte dalle lettere, le misure che vengono dal profilo — ci vorrebbe
 * piu codice di quanto ne serva a scriverla. Qui i tasti sono gli stessi
 * pannelli del resto dell'interfaccia, e si comportano come tutto il resto.
 *
 * Chi scrive un indirizzo IP ha bisogno delle cifre senza cambiare pagina,
 * e chi scrive un token a lunga durata ha bisogno di maiuscole, minuscole e
 * simboli: sono i due soli campi che questa tastiera deve servire, e la
 * disposizione e fatta per quelli.
 * --------------------------------------------------------------------- */
#ifndef TASTIERA_H
#define TASTIERA_H

#include "lvgl.h"

/* Cosa e stato premuto. `testo` e il carattere da aggiungere; con `testo`
   a NULL guarda `azione`. */
typedef enum {
    TASTO_CARATTERE = 0,
    TASTO_CANCELLA,
    TASTO_AZIONE,        /* il tasto ambra in fondo a destra */
} tasto_t;

typedef void (*tastiera_cb_t)(tasto_t tipo, const char *testo);

/* Costruisce la tastiera dentro `padre`. `azione` e l'etichetta del tasto
   ambra ("Avanti", "Collega", …). */
lv_obj_t *tastiera(lv_obj_t *padre, const char *azione, tastiera_cb_t su_tasto);

#endif /* TASTIERA_H */
