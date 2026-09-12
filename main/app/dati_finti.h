/* ------------------------------------------------------------------------
 * Fra i pezzi del finto fornitore — solo Fase 1, non e un'interfaccia
 * pubblica. L'interfaccia e dati.h, e chi disegna vede solo quella.
 * --------------------------------------------------------------------- */
#ifndef DATI_FINTI_H
#define DATI_FINTI_H

#include "dati.h"

/* Quaranta caratteri con accenti e apostrofi, per il caso di 11-collaudo.md
   §2: deve troncare con l'ellissi senza che la scheda cambi dimensione. */
extern const char *const FINTO_NOME_LUNGO;

/* Rifanno i dati della propria area secondo il caso scelto. */
void finto_clima_applica(caso_t caso);
void finto_energia_applica(caso_t caso);
void finto_accessi_applica(caso_t caso);
void finto_agenda_applica(caso_t caso);
void finto_riassunto_applica(caso_t caso);

#endif /* DATI_FINTI_H */
