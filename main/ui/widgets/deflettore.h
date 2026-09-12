/* ------------------------------------------------------------------------
 * Deflettore — 02-design-tokens.md.
 *
 * "Le cinque posizioni del deflettore non sono icone di libreria ma un
 * disegno parametrico: rettangolo del corpo macchina piu una lamella
 * posizionata a cinque altezze diverse. Va generato, non cercato."
 *
 * Il perche si capisce guardando l'alternativa: cinque icone diverse per
 * cinque angoli sarebbero cinque disegni da confrontare a mente, mentre la
 * stessa figura con la lamella che scende dice da sola cosa cambia.
 * --------------------------------------------------------------------- */
#ifndef DEFLETTORE_H
#define DEFLETTORE_H

#include "dati.h"
#include "lvgl.h"

/* Disegna il corpo macchina con la lamella all'altezza `posizione`.
   `colore` e quello dell'accento quando la posizione e scelta. */
lv_obj_t *deflettore_disegno(lv_obj_t *padre, deflettore_t posizione,
                             lv_color_t colore);

#endif /* DEFLETTORE_H */
