/* ------------------------------------------------------------------------
 * Validazione della configurazione — 03-config-contratto.md §4 e §10.
 *
 * Due livelli, come `tools/verifica_config.py`:
 *
 *   1  quello che `config.schema.json` gia impone — tipi, intervalli,
 *      obbligatorieta — ricontrollato a mano, perche un validatore JSON
 *      Schema sull'ESP32 costa piu di quanto valga
 *   2  le regole fra campi, che JSON Schema non sa esprimere e che vanno
 *      scritte a mano comunque, su tutti e due i lati
 *
 * Il primo livello deve corrispondere allo schema **campo per campo**.
 * Niente lo impone automaticamente: lo impone la prova
 * `prova_validazione`, che passa le stesse configurazioni a questo
 * validatore e a `verifica_config.py` e pretende lo stesso verdetto.
 *
 * Un **errore** blocca il salvataggio, un **avviso** no: un avviso segnala
 * una configurazione che funziona ma non fa quello che probabilmente ti
 * aspetti.
 * --------------------------------------------------------------------- */
#ifndef VALIDAZIONE_H
#define VALIDAZIONE_H

#include <stdbool.h>

#include "cJSON.h"
#include "config.h"

/* Vero se non ci sono errori. Gli avvisi non fanno fallire nulla ma restano
   leggibili con cfg_errori(). */
bool validazione_esegui(const cJSON *c);

/* Usate anche da config.c per riportare gli esiti. */
void cfg_azzera_errori(void);
void cfg_aggiungi_errore(gravita_t g, const char *campo, const char *motivo);

/* L'albero in memoria, per chi deve leggerlo direttamente. */
cJSON *cfg_albero(void);

#endif /* VALIDAZIONE_H */
