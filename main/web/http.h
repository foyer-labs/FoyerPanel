/* ------------------------------------------------------------------------
 * Trasporto HTTP — la controparte di archivio.h e magazzino.h.
 *
 * Sul pannello e esp_http_server; nel simulatore sono socket POSIX. Il
 * codice che decide **cosa** rispondere non deve sapere quale dei due, ed e
 * la ragione per cui questa e un'intestazione a se.
 *
 * Non c'e TLS e non ci sara: rete locale, nessun certificato da rinnovare
 * su un apparecchio a muro, nessuna autorita che possa firmare per
 * `pannello.local`. E una scelta scritta nel prompt del progetto, non una
 * mancanza.
 *
 * Il server e minuscolo di proposito: un metodo, un percorso, un corpo. Non
 * gestisce chunked, non gestisce keep-alive, non gestisce multipart. Le
 * richieste che arrivano sono quelle della pagina di configurazione, e sono
 * cinque.
 * --------------------------------------------------------------------- */
#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stddef.h>

/* Corpo massimo accettato: config.json puo arrivare a 24 KB (§2.5), piu il
   margine dell'involucro JSON. Oltre, si risponde 413 senza leggere. */
#define HTTP_CORPO_MAX 32768

typedef enum { HTTP_GET = 0, HTTP_POST, HTTP_ALTRO } metodo_t;

typedef struct {
    metodo_t    metodo;
    const char *percorso;      /* senza query: "/api/config"          */
    const char *query;         /* dopo il '?', "" se non c'e          */
    const char *corpo;         /* terminato, "" se non c'e            */
    size_t      corpo_n;
    const char *chiamante;     /* indirizzo, per il limite di frequenza */
} richiesta_t;

typedef struct {
    int         codice;        /* 200, 404, 422, 429, ...             */
    const char *tipo;          /* "application/json", "text/html"     */
    const char *corpo;
    size_t      corpo_n;       /* 0 = usa strlen(corpo)               */
    /* Come liberare `corpo` dopo averlo spedito, o NULL se non c'e niente
       da liberare. E una funzione e non un booleano perche chi alloca sa
       con che allocatore l'ha fatto — cJSON ha il suo — e il trasporto no,
       e non deve impararlo. */
    void      (*libera)(void *corpo);
} risposta_t;

/* Il gestore: riceve la richiesta, riempie la risposta. Ne esiste uno solo
   — web_servi() — e fa lui lo smistamento, perche le regole di accesso
   sono le stesse per tutti i percorsi e conviene che stiano in un posto
   solo invece che ripetute in ogni gestore. */
typedef void (*http_gestore_t)(const richiesta_t *r, risposta_t *s);

/* Avvia sulla porta data. Sul pannello e la 80; nel simulatore una alta,
   perche sotto la 1024 servirebbe essere root e non e il caso. */
bool http_avvia(int porta, http_gestore_t gestore);

/* --- un corpo che non ci sta in memoria ---------------------------------
 *
 * Tutto il resto del server bufferizza la richiesta intera e poi la passa al
 * gestore: e la cosa giusta per un documento di configurazione, che sono
 * pochi kilobyte, e rende il gestore una funzione pura che si prova senza
 * socket.
 *
 * Un'immagine di firmware sono due megabyte. Bufferizzarla vorrebbe dire
 * tenerne una copia in RAM mentre la si scrive in flash, e su questo
 * apparecchio non c'e nessuna memoria dove starebbe due volte.
 *
 * Per **un solo percorso**, quindi, il corpo non si accumula: arriva a
 * pezzi, nell'ordine, e chi li riceve li consuma man mano. Un percorso solo
 * e non un meccanismo generale, perche il resto del server non ne ha
 * bisogno e ogni endpoint che diventasse a flusso perderebbe la proprieta
 * che lo rende provabile senza rete.
 *
 * `apri` riceve quanto e dichiarato — zero se il client non lo dice — e puo
 * rifiutare. `pezzo` riceve i blocchi in ordine; tornando falso ferma tutto.
 * `chiudi` riceve vero se sono arrivati tutti i byte dichiarati, e torna
 * l'esito da mandare indietro: chi carica deve sapere se e installato. */
typedef struct {
    const char *percorso;
    bool (*apri)(size_t dichiarati);
    bool (*pezzo)(const void *dati, size_t n);
    bool (*chiudi)(bool completo);
    /* Cosa rispondere. `esito` e quello che ha detto chiudi(). */
    void (*risposta)(bool esito, risposta_t *s);
} http_flusso_t;

/* Registra il percorso a flusso. Uno solo: chiamarla due volte sostituisce.
   NULL lo toglie. */
void http_flusso(const http_flusso_t *f);
void http_ferma(void);
bool http_attivo(void);

/* Fa avanzare il server: accetta e serve al piu una richiesta, poi torna.
   Non blocca. Sul pannello esp_http_server ha il suo compito e questa non
   fa nulla; nel simulatore la chiama il ciclo principale. */
void http_gira(void);

#endif /* HTTP_H */
