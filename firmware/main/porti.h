/* ------------------------------------------------------------------------
 * Le poche funzioni che esistono solo sul pannello.
 *
 * I porti — archivio, magazzino — condividono l'intestazione col simulatore
 * perche il codice sopra non deve sapere su cosa gira. Restano fuori da
 * quelle intestazioni le operazioni che sul PC non hanno un corrispondente:
 * montare una partizione non e una di quelle cose che si fanno su un file
 * system che c'e gia.
 * --------------------------------------------------------------------- */
#ifndef PORTI_H
#define PORTI_H

#include <stdbool.h>

/* Monta la partizione `storage`. La formatta se non si monta: una
   partizione vuota appena uscita di fabbrica non e un guasto. */
bool archivio_monta(void);

/* Aggancia l'uscita di ESP-IDF al registro diagnostico. Da chiamare per
   prima, cosi anche cio che va storto durante l'avvio finisce nel registro
   che si andra a leggere. */
void registro_aggancia(void);

#endif /* PORTI_H */
