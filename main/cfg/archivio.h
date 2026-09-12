/* ------------------------------------------------------------------------
 * Archivio dei file — 03-config-contratto.md §1 e §2.
 *
 * Sul pannello e LittleFS su una partizione dedicata; nel simulatore sono
 * file normali in una cartella. Il codice della configurazione non sa quale
 * dei due, ed e la ragione per cui questa e un'intestazione a se.
 *
 * La scrittura e **atomica**: config.tmp, poi fsync, poi rinomina su
 * config.json, e il precedente diventa config.bak.json. Un salvataggio
 * interrotto a meta — che su un pannello a muro vuol dire un'interruzione di
 * corrente, non un caso di scuola — non deve lasciare un file ibrido.
 * --------------------------------------------------------------------- */
#ifndef ARCHIVIO_H
#define ARCHIVIO_H

#include <stdbool.h>
#include <stddef.h>

/* Dimensione massima di config.json: 24 KB (§2.5). Oltre, la scrittura
   viene rifiutata invece di riempire una partizione. */
#define ARCHIVIO_MAX 24576

/* The largest file the store writes at all. config.json keeps its own 24 KB
   limit above, enforced by config.c; this one is for the translations
   edited on the panel, where a whole language — the panel and the page —
   is several times that. It still refuses what would fill a partition. */
#define ARCHIVIO_FILE_MAX (128 * 1024)

/* Legge un file per intero. `letti` riceve quanti byte, terminatore escluso.
   Falso se il file non c'e o non ci sta in `max`. */
bool archivio_leggi(const char *nome, char *buf, size_t max, size_t *letti);

/* Scrive in modo atomico. Chi c'era prima diventa `nome`.bak, cosi un file
   corrotto ha sempre un predecessore da cui ripartire. */
bool archivio_scrivi(const char *nome, const char *dati, size_t n);

bool archivio_esiste(const char *nome);
bool archivio_cancella(const char *nome);

/* Dove stanno i file. Sul pannello e la partizione LittleFS montata; nel
   simulatore una cartella che si sceglie con --dati. */
void        archivio_radice(const char *percorso);
const char *archivio_dove(void);

#endif /* ARCHIVIO_H */
