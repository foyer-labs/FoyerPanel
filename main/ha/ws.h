/* ------------------------------------------------------------------------
 * Trasporto WebSocket — la controparte di http.h, dall'altra parte.
 *
 * Sul pannello sara esp_websocket_client; nel simulatore sono socket POSIX
 * con la stretta di mano di RFC 6455 scritta a mano. Il codice che parla il
 * protocollo di Home Assistant non deve sapere quale dei due.
 *
 * **TLS quando serve**, e non e un ripensamento estetico. Il progetto
 * diceva "niente TLS, il collegamento e locale", e su un Home Assistant
 * raggiungibile in chiaro resta vero. Ma un'installazione con un
 * certificato vero, valido per un nome che da dentro casa risolve
 * all'indirizzo di casa, in chiaro non risponde affatto: li il cifrato non
 * e prudenza in piu, e l'unico modo di parlare. Chi apre decide, e sotto se
 * ne occupa canale.h.
 *
 * L'interfaccia e volutamente povera: apri, manda testo, ricevi testo,
 * chiudi. Niente binario, niente estensioni, niente compressione. Home
 * Assistant parla JSON su frame di testo e basta.
 * --------------------------------------------------------------------- */
#ifndef WS_H
#define WS_H

#include <stdbool.h>
#include <stddef.h>

/* Il messaggio piu grande che ci si aspetta e la risposta a get_states, che
   arriva in un frame solo e contiene **tutte** le entita della casa con
   tutti i loro attributi.

   Erano 64 kB, con scritto accanto che "qualche centinaio di entita" ci
   stava. Una casa vera l'ha smentito al primo collegamento. Il numero
   originale nasceva da un vincolo giusto — un buffer che cresce a piacere,
   su un apparecchio con 512 kB di RAM interna, e un riavvio che aspetta —
   applicato pero alla memoria sbagliata: sopra i 16 kB queste allocazioni
   finiscono in PSRAM, dove di spazio ce n'e a megabyte.

   Resta un tetto e non diventa "quanto serve": senza, un Home Assistant
   con un'entita impazzita che sputa attributi porterebbe giu il pannello
   invece di dare un errore. Quando lo si supera, il messaggio dice **di
   quanto**: un limite che si lamenta senza dire il numero costringe ad
   alzarlo a tentativi. */
#define WS_MESSAGGIO_MAX (256 * 1024)

typedef enum {
    WS_CHIUSO = 0,      /* non collegato, e nessuno ci sta provando        */
    WS_CONNETTO,        /* stretta di mano in corso                        */
    WS_APERTO,          /* si puo parlare                                  */
    WS_ERRORE,          /* l'ultimo tentativo e andato male                */
} ws_stato_t;

/* Apre verso host:porta, chiedendo `percorso` (di solito "/api/websocket").
   Non blocca: torna subito, e ws_gira() porta avanti la stretta di mano —
   quella TCP, quella TLS se richiesta, e quella WebSocket, in quest'ordine.

   Con `cifrato` il certificato **viene verificato**, e per questo `host`
   deve essere il nome e non l'indirizzo: in un certificato l'indirizzo non
   c'e quasi mai, e verificarlo contro un IP fallisce sempre. */
bool ws_apri(const char *host, int porta, const char *percorso, bool cifrato);

void       ws_chiudi(void);
ws_stato_t ws_stato(void);

/* Manda un frame di testo. Falso se non c'e collegamento. */
bool ws_manda(const char *testo);

/* Il prossimo messaggio arrivato, dentro `buf`. Falso se non ce n'e uno
   intero: **non** blocca e non aspetta. */
bool ws_ricevi(char *buf, size_t max);

/* Fa avanzare il trasporto: stretta di mano, lettura, riassemblaggio dei
   frame. Da chiamare spesso; non blocca mai. */
void ws_gira(void);

/* Perche l'ultimo tentativo e fallito, per il registro e per la
   diagnostica. Mai NULL, mai contiene il token. */
const char *ws_motivo(void);

#endif /* WS_H */
