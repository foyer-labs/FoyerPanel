/* ------------------------------------------------------------------------
 * Le sezioni del pannello — 01-specifica-ui.md §1.
 *
 * Nove sezioni previste, piu la home e le impostazioni che non sono
 * sezioni ma destinazioni fisse. Quali siano attive lo dice la
 * configurazione (`sezioni` in config.json): una sezione assente **non
 * compare nel rail ne nel dock**, ed e cosi che oggi si esclude l'agenda.
 *
 * L'ordine dell'array di configurazione e l'ordine a schermo, e conta:
 * quando le sezioni attive sono piu dei pulsanti che il dock puo tenere
 * (09-profili.md §6), il dock mostra le prime.
 * --------------------------------------------------------------------- */
#ifndef SEZIONI_H
#define SEZIONI_H

#include <stdbool.h>

#include "i18n.h"

typedef enum {
    SEZ_LUCI = 0,
    SEZ_INTERRUTTORI,
    SEZ_CLIMA,
    SEZ_ENERGIA,
    SEZ_ACCESSI,
    SEZ_PROGRAMMAZIONI,
    SEZ_AGENDA,
    SEZ_WIFI,
    SEZ_ROBOT,
    SEZ_QUANTE,

    /* Non sono sezioni: non stanno in `sezioni` e non si possono togliere. */
    SEZ_HOME,
    SEZ_IMPOSTAZIONI,
    SEZ_DIAGNOSTICA,
} sezione_t;

typedef struct {
    sezione_t   id;
    const char *chiave;   /* come in config.json */
    tx_t        nome;     /* come a schermo: tr(info->nome) */
    const char *icona;    /* ICO_* di icons.h */
} sezione_info_t;

/* Descrizione di una sezione, home e impostazioni comprese. Mai NULL. */
const sezione_info_t *sezione(sezione_t s);

/* La sezione con quella chiave di configurazione, o SEZ_HOME se non esiste. */
sezione_t sezione_da_chiave(const char *chiave);

/* --- quali sono attive -----------------------------------------------------
 * L'elenco e l'ordine vengono da `sezioni` di config.json. Una sezione
 * elencata ma priva della propria sorgente dati non compare lo stesso:
 * l'agenda senza calendari, la condivisione Wi-Fi senza reti da mostrare.
 * Senza configurazione leggibile ci sono tutte, che e meglio di una
 * navigazione vuota — e comunque si finisce al primo avvio.
 */

/* Quante sezioni attive, e la n-esima nell'ordine di configurazione. */
/* Dimentica l'elenco: al prossimo accesso si rilegge da config.json.
   Serve dopo un salvataggio, perche `sezioni` puo essere cambiato. */
void      sezioni_azzera(void);

int       sezioni_attive(void);
sezione_t sezione_attiva(int n);
bool      sezione_e_attiva(sezione_t s);

/* Sostituisce l'elenco attivo. `quante` a zero rimette il predefinito.
   Serve alla configurazione e, nel simulatore, ai casi limite di
   11-collaudo.md §2 sulle quantita. */
void sezioni_imposta(const sezione_t *elenco, int quante);

#endif /* SEZIONI_H */
