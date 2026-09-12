/* ------------------------------------------------------------------------
 * Il registro diagnostico. Vedi registro.c per la regola che lo governa.
 * --------------------------------------------------------------------- */
#ifndef REGISTRO_H
#define REGISTRO_H

#include "dati.h"   /* livello_t, riga_log_t */

/* Aggiunge una riga. Il testo viene troncato e ripulito dal nome della rete
   prima di essere conservato — ma **non contare su quello**: la regola vera
   e non scriverci dentro segreti, non fidarsi che qualcuno li tolga dopo. */
void registro_aggiungi(livello_t l, const char *sorgente, const char *testo);

/* Le righe conservate, dalla piu recente: chi apre la diagnostica vuole
   sapere cosa e appena successo. */
int               registro_righe(void);
const riga_log_t *registro_riga(int n);

/* Quanti errori da quando e acceso. */
int registro_errori(void);

/* Vero se in questo registro non e mai entrato niente da quando il
   pannello e acceso. Non e la stessa cosa di "e vuoto": svuotarlo a mano
   lo rende vuoto, non mai scritto.

   Serve a distinguere due situazioni che si assomigliano e vogliono
   risposte opposte: sul pannello il registro riceve le righe del sistema
   fin dall'accensione, quindi vuoto vuol dire "svuotato" e si dice; sul
   PC non le riceve mai, e li ha senso mostrare le righe d'esempio senza
   le quali non si vedrebbe com'e fatto un registro. */
bool registro_mai_scritto(void);

/* Butta via tutto quello che c'e dentro, contatore degli errori compreso.
   Serve al pulsante della diagnostica: quando si sta cercando un guasto
   che si riesce a rifare, un registro pulito prima di rifarlo vale piu di
   trecento righe in cui la cosa che interessa e sepolta. */
void registro_svuota(void);

#endif /* REGISTRO_H */
