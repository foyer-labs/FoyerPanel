/* ------------------------------------------------------------------------
 * L'impronta di una sezione. Vedi impronta.c per il perche.
 * --------------------------------------------------------------------- */
#ifndef IMPRONTA_H
#define IMPRONTA_H

#include <stdint.h>

/* Un numero che riassume cio che quella sezione **mostra**. Se non cambia,
   non c'e niente di nuovo da far vedere e ridisegnare sarebbe lavoro
   sprecato — che su questo pannello non e gratis: un ridisegno pieno fa
   restare a secco la periferica RGB, e si vede.

   Va chiamata **dopo** dati_ricarica(), che e quella che porta i valori
   nuovi dentro le schede. */
uint32_t impronta_sezione(int sezione);

#endif /* IMPRONTA_H */
