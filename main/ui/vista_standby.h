/* ------------------------------------------------------------------------
 * La vista di standby — 01-specifica-ui.md §4.7.
 *
 * Sta in un file suo e non dentro tempi.c per una ragione che si e vista
 * crescendo: tempi.c decide **quando** il pannello si addormenta, questa
 * decide **cosa** si vede mentre dorme. Erano venti righe e stavano
 * insieme; adesso sono due schermate, una orizzontale e una verticale, con
 * pezzi che compaiono e spariscono.
 *
 * Il velo resta di tempi.c: e lui che si prende il tocco di risveglio e che
 * fa traslare il contenuto di pochi pixel ogni tre minuti.
 * --------------------------------------------------------------------- */
#ifndef VISTA_STANDBY_H
#define VISTA_STANDBY_H

#include "lvgl.h"

/* Costruisce la schermata dentro `padre` e la riempie subito con i valori
   di adesso: un secondo di trattini all'ingresso nello standby si nota,
   perche e il momento in cui si guarda. */
void vista_standby_costruisci(lv_obj_t *padre);

/* Rinfresca i valori. Da chiamare una volta al secondo: si riscrivono le
   etichette invece di ricostruire la vista, perche un ridisegno pieno su
   questo schermo si vede.

   Non fa niente se la vista non c'e. */
void vista_standby_aggiorna(void);

/* Da chiamare quando il velo viene distrutto: qui dentro restano puntatori
   a oggetti che non esistono piu, e sono tutti in questo file. */
void vista_standby_dimentica(void);

#endif /* VISTA_STANDBY_H */
