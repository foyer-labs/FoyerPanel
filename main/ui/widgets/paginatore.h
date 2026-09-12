/* ------------------------------------------------------------------------
 * Paginazione — 01-specifica-ui.md §3.1.
 *
 * Frecce in testata al posto dell'orologio, pallini in basso. Le sezioni
 * paginate sono Luci, Clima e Telecamere, e si comportano tutte allo stesso
 * modo, quindi il paginatore e uno solo.
 *
 * La regola che conta: **nell'ultima pagina i posti liberi restano vuoti e
 * tratteggiati**. Le schede non si allargano, cosi la posizione di una zona
 * non cambia mai fra una pagina e l'altra — e chi cerca la luce del salotto
 * la trova sempre nello stesso punto.
 * --------------------------------------------------------------------- */
#ifndef PAGINATORE_H
#define PAGINATORE_H

#include "lvgl.h"

typedef void (*pagina_cb_t)(int pagina);

/* Quante pagine servono per `elementi` con una capienza di `capienza`.
   Almeno una, anche con zero elementi. */
int paginatore_pagine(int elementi, int capienza);

/* Frecce e conteggio "1 / 2", da mettere in testata al posto dell'orologio.
   Se le pagine sono meno di due non costruisce nulla e restituisce NULL. */
lv_obj_t *paginatore_frecce(lv_obj_t *padre, int pagina, int pagine,
                            pagina_cb_t su_cambio);

/* Pallini in fondo alla schermata. NULL se la pagina e una sola. */
lv_obj_t *paginatore_pallini(lv_obj_t *padre, int pagina, int pagine);

#endif /* PAGINATORE_H */
