/* ------------------------------------------------------------------------
 * Configurazione — 03-config-contratto.md.
 *
 * `config.schema.json` e la fonte autorevole per tipi, intervalli e
 * obbligatorieta. **Il firmware non lo esegue**: un validatore JSON Schema e
 * troppo pesante per l'ESP32. Il controllo qui e scritto a mano e deve
 * corrispondere allo schema campo per campo — quando lo schema cambia,
 * cambia anche cfg/validazione.c, e non c'e niente che lo imponga se non
 * questa riga e la prova che li confronta.
 *
 * Il documento resta in memoria come albero JSON, non come struttura C, e
 * non e pigrizia: §8 dice che i campi sconosciuti al profilo di destinazione
 * vanno **conservati ma ignorati**, non cancellati. Tenendo l'albero, un
 * config.json esportato da un pannello e importato in un altro torna
 * indietro intatto. Con una struct ci si perderebbe tutto quello che
 * la struct non prevede.
 * --------------------------------------------------------------------- */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* La versione di schema che questo firmware conosce. */
#define CFG_SCHEMA 11

/* Nomi dei file nell'archivio. */
#define CFG_FILE  "config.json"
#define CFG_COPIA "config.json.bak"

typedef enum {
    CFG_LETTA = 0,        /* letta e valida                                */
    CFG_DA_COPIA,         /* il principale non andava, si e usata la copia  */
    CFG_PREDEFINITA,      /* non c'era niente: si va al primo avvio         */
    CFG_TROPPO_NUOVA,     /* schema maggiore del nostro: sola lettura       */
} esito_cfg_t;

/* Carica la configurazione dall'archivio, con i ripieghi di §2.4. */
esito_cfg_t cfg_carica(void);

/* Sostituisce la configurazione con quella data, dopo averla validata.
   Falso se la validazione fallisce: **il file non viene toccato**. I campi
   rifiutati restano leggibili con cfg_errori(). */
bool cfg_sostituisci(const char *json, size_t n);

/* Riscrive su file la configurazione in memoria. */
bool cfg_salva(void);

/* Il documento in forma di testo, per l'esportazione e per GET /api/config.
   Chi chiama libera con cfg_libera_testo(). */
char *cfg_esporta(bool oscura_segreti);
void  cfg_libera_testo(char *s);

/* --- errori dell'ultima validazione ------------------------------------- */

#define CFG_ERRORI_MAX 16

typedef enum { CFG_ERRORE = 0, CFG_AVVISO } gravita_t;

typedef struct {
    char      campo[64];      /* "access/2/pulse_ms" */
    char      motivo[96];
    gravita_t gravita;
} errore_cfg_t;

int                 cfg_errori(void);
const errore_cfg_t *cfg_errore(int n);

/* --- accessori tipizzati ------------------------------------------------
 * Quello che l'interfaccia legge. Nessuno di questi tocca l'albero JSON
 * direttamente: se un campo manca o e del tipo sbagliato torna il valore
 * predefinito, perche una configurazione a meta non deve far sparire una
 * schermata.
 */
const char *cfg_testo(const char *percorso, const char *ripiego);

/* --- il nome con cui il pannello risponde in rete -----------------------
 *
 * `sistema.nome_pannello` e' scritto per essere letto — "PannelloIngresso",
 * "Pannello cucina" — e un nome mDNS non puo' avere maiuscole ne' spazi. La
 * traduzione fra i due e' questa: minuscole, spazi e trattini diventano
 * trattino, tutto il resto cade.
 *
 * Sta qui e non in chi avvia mDNS perche' non la usa solo lui: le
 * impostazioni a bordo e il primo avvio scrivono l'indirizzo sul vetro, e
 * per un po' ci hanno scritto **`pannello.local`** a prescindere — il nome
 * predefinito, giusto solo per chi non aveva cambiato il proprio. Chi
 * leggeva quella riga sul pannello di casa leggeva un indirizzo che non
 * rispondeva.
 *
 * Scrive in `buf` il solo nome host, senza `.local` e senza `http://`:
 * quelli li mette chi compone la frase. Torna `buf`. */
const char *cfg_nome_host(char *buf, size_t n);

/* Scrive un testo per percorso, creando gli oggetti che mancano lungo la
   strada. Non salva: chi ha finito di cambiare chiama cfg_salva(), cosi
   piu modifiche costano una scrittura sola.

   **Solo testo, e solo oggetti.** Non c'e la controparte per gli interi ne
   per gli array, e non e una dimenticanza: gli unici campi che qualcuno
   deve poter cambiare senza mandare tutto il file sono quelli che si
   impostano prima di avere una rete, e sono nomi. Tutto il resto passa da
   cfg_sostituisci(), che valida l'intero documento — e un documento intero
   validato e piu sicuro di venti scritture puntuali ognuna valida per conto
   suo. */
bool cfg_imposta_testo(const char *percorso, const char *valore);

/* Come sopra, per vero/falso. Serve a `home_assistant.tls`, che e l'unico
   interruttore che qualcuno deve poter girare prima di avere una rete —
   perche senza girarlo, verso un Home Assistant in HTTPS quella rete non
   serve a niente. */
bool cfg_imposta_vero(const char *percorso, bool valore);
int32_t     cfg_intero(const char *percorso, int32_t ripiego);
double      cfg_decimale(const char *percorso, double ripiego);
bool        cfg_vero(const char *percorso, bool ripiego);
int         cfg_quanti(const char *percorso);   /* elementi di un array */

/* Come sopra ma dentro l'n-esimo elemento di un array:
   cfg_testo_in("lights/zones", 3, "name", "") */
const char *cfg_testo_in(const char *array, int n, const char *campo,
                         const char *ripiego);
int32_t     cfg_intero_in(const char *array, int n, const char *campo,
                          int32_t ripiego);
bool        cfg_vero_in(const char *array, int n, const char *campo,
                        bool ripiego);
/* Vero se il campo esiste e non e null: serve dove la differenza fra
   "assente" e "zero" cambia il comportamento, come sensore_stato. */
bool        cfg_ha_in(const char *array, int n, const char *campo);

#endif /* CONFIG_H */
