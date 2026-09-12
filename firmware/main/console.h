/* ------------------------------------------------------------------------
 * Console sulla seriale — solo pannello. Vedi console.c per il perche.
 * --------------------------------------------------------------------- */
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>

/* Apre la console sulla UART. Falso se non parte: il pannello funziona lo
   stesso, si perde solo il modo di correggere una password senza rete. */
bool console_avvia(void);

#endif /* CONSOLE_H */
