/* ------------------------------------------------------------------------
 * Fusi orari — dal nome IANA alla stringa che il sistema capisce.
 *
 * config.json tiene "Europe/Rome": leggibile, riconoscibile, ed e lo stesso
 * nome che usa Home Assistant. ESP-IDF pero non ha un database dei fusi e
 * setenv("TZ", ...) vuole il formato POSIX. Passargli il nome IANA non da
 * errore: da UTC in silenzio, e un orologio indietro di un'ora per sei mesi
 * l'anno e il genere di difetto che nessuno attribuisce alla configurazione.
 * --------------------------------------------------------------------- */
#ifndef FUSI_H
#define FUSI_H

/* La stringa POSIX per quel nome, o NULL se non e fra quelli conosciuti.
   Una stringa gia in formato POSIX passa cosi com'e. NULL va detto a chi
   guarda, non ignorato: un fuso sconosciuto e una configurazione da
   correggere, non un caso da tirare avanti. */
const char *fuso_posix(const char *iana);

/* L'elenco, per le prove: serve a pretendere che ogni nome proposto dalla
   pagina di configurazione sia anche traducibile. */
int         fusi_quanti(void);
const char *fuso_nome(int n);

#endif /* FUSI_H */
