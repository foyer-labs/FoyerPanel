/* ------------------------------------------------------------------------
 * Magazzino a chiave — la controparte di archivio.h per i segreti.
 *
 * Sul pannello e NVS, spazio dei nomi `secrets`; nel simulatore sono file
 * dentro una sottocartella dell'archivio. Sono due cose diverse apposta:
 * la configurazione si esporta e si copia, i segreti no, e tenerli nello
 * stesso posto renderebbe facile sbagliare.
 *
 * Nessuno chiama queste funzioni tranne segreti.c: e li che sta la regola
 * per cui un segreto non torna mai indietro.
 * --------------------------------------------------------------------- */
#ifndef MAGAZZINO_H
#define MAGAZZINO_H

#include <stdbool.h>
#include <stddef.h>

bool magazzino_avvia(void);

/* Scrive `valore` (stringa terminata) sotto `chiave`. */
bool magazzino_scrivi(const char *chiave, const char *valore);

/* Legge dentro `buf`. Falso se la chiave non c'e o non ci sta. */
bool magazzino_leggi(const char *chiave, char *buf, size_t max);

bool magazzino_esiste(const char *chiave);
bool magazzino_cancella(const char *chiave);

/* Cancella tutto lo spazio dei nomi. */
bool magazzino_azzera(void);

#endif /* MAGAZZINO_H */
