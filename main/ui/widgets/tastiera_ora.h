/* ------------------------------------------------------------------------
 * Il tastierino di un orario — 01-specifica-ui.md §3.10.
 *
 * Si apre toccando un orario nelle Programmazioni, e sta sopra un velo come
 * gli altri modali del pannello. Ore e minuti con due coppie di tasti
 * grandi, i minuti a passi di cinque.
 *
 * **Perche un tastierino e non due tasti +/- sulla riga.** La riga di una
 * finestra tiene due orari, e con quattro tasti per orario diventerebbe una
 * fila di otto bersagli da mirare — su un elenco che scorre, e a portata di
 * pollice mentre si scorre. Cosi invece la riga resta bassa e il gesto
 * costa un tocco in piu solo a chi vuole davvero cambiare un'ora.
 *
 * **Non scrive finche non si conferma.** Cambiare un orario in Home
 * Assistant fa scattare le automazioni che lo guardano: mandare a ogni
 * tocco di «+» vorrebbe dire far partire la pompa passando dalle 15:00
 * mentre si va verso le 16:00.
 * --------------------------------------------------------------------- */
#ifndef TASTIERA_ORA_H
#define TASTIERA_ORA_H

#include <stdbool.h>

/* Apre il tastierino sull'orario di una finestra. `capo` vero e
   l'accensione, falso lo spegnimento. Non fa niente se quell'orario non si
   sa leggere: un tastierino che parte da «--:--» chiederebbe di inventare
   un'ora al posto di correggerne una. */
void tastiera_ora_apri(int gruppo, int finestra, bool capo);

#endif /* TASTIERA_ORA_H */
