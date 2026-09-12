/* ------------------------------------------------------------------------
 * Canale — come si muovono i byte, in chiaro o cifrati.
 *
 * Nasce da una scoperta sul campo: 05-architettura-firmware.md §3 diceva
 * "niente TLS, il collegamento e locale", e l'Home Assistant di casa parla
 * **solo** HTTPS. Non era una svista di chi l'ha installato: e un
 * certificato Let's Encrypt vero, per un nome vero, che da dentro la rete
 * risolve all'indirizzo di casa. Cioe il caso in cui il cifrato non costa
 * niente in privacy e si verifica per davvero.
 *
 * **Perche un porto e non due trasporti.** Sotto c'era la tentazione di
 * usare esp_websocket_client sul pannello e tenere quello scritto a mano
 * solo sul PC — l'intestazione di ws.h lo dava per scontato. Ma in
 * ws_socket.c c'e dentro il comportamento che e costato piu debugging di
 * tutti: Home Assistant manda `auth_invalid` e **poi** chiude, e chi butta
 * via il buffer insieme al socket perde proprio il messaggio che spiega
 * perche. Cambiare quel codice sul solo pannello vorrebbe dire riaprire
 * quella domanda sul bersaglio dove e piu difficile rispondere.
 *
 * Quindi resta uno solo il codice che parla WebSocket, e cambia sotto il
 * modo in cui i byte passano — come archivio e magazzino. Il pezzo diverso
 * fra PC e pannello e il piu piccolo possibile.
 *
 * **Non blocca mai.** canale_apri() torna subito e canale_avanza() porta
 * avanti connessione e stretta di mano: su ESP-IDF sono la stessa chiamata
 * (esp_tls_conn_new_async), su POSIX sono due, e da fuori non si vede.
 * --------------------------------------------------------------------- */
#ifndef CANALE_H
#define CANALE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct canale canale_t;

typedef enum {
    CANALE_IN_CORSO = 0,  /* connessione o stretta di mano non finite */
    CANALE_PRONTO,        /* si puo parlare                           */
    CANALE_ROTTO,         /* andata male: canale_motivo() dice come   */
} canale_fase_t;

/* Apre verso host:porta. Con `cifrato` si pretende TLS **e la verifica del
   certificato**: il nome passato qui e quello contro cui si verifica, ed e
   la ragione per cui a Home Assistant ci si collega per nome e non per
   indirizzo — in un certificato l'indirizzo non c'e quasi mai.

   Nessuna scorciatoia per saltare la verifica, e di proposito. Un
   collegamento cifrato che non controlla chi ha dall'altra parte protegge
   da chi ascolta e non da chi si mette in mezzo, e qui in mezzo ci
   passerebbe il token di Home Assistant. Se un giorno servisse un
   certificato privato, la strada e aggiungere **quella** radice, non
   togliere il controllo.

   NULL se non si e potuto nemmeno cominciare. */
canale_t *canale_apri(const char *host, int porta, bool cifrato);

/* Porta avanti connessione e stretta di mano. Da chiamare finche torna
   CANALE_IN_CORSO. Non blocca. */
canale_fase_t canale_avanza(canale_t *c);

/* Byte letti; 0 se l'altro ha chiuso; -1 se per ora non ce n'e (e non e un
   errore) oppure se il canale e rotto — canale_avanza() lo distingue.

   **Questa e la riga su cui i due porti devono essere d'accordo**, ed e
   l'unica in cui si erano allontanati senza che nessuno se ne accorgesse.
   Su POSIX «per ora non ce n'e» e un -1 con errno EAGAIN; su ESP-IDF, con
   TLS sotto, e un codice con un nome suo — ma sul **TCP in chiaro** e di
   nuovo un -1 con errno, e per due anni quel ramo ha risposto 0, cioe
   «chiuso». Nessuno se n'e accorto perche' in chiaro non parlava nessuno:
   Home Assistant e cifrato. La prima cosa a parlare in chiaro sono state
   le telecamere, e il riquadro diceva «la telecamera ha chiuso» prima
   ancora che fosse partita la prima domanda.

   Chi tocca uno dei due porti tocchi anche l'altro, e li confronti qui.

   **E attenzione: da quando le telecamere sono uscite, in chiaro non parla
   di nuovo nessuno.** Questa correzione non ha piu niente che la eserciti,
   e un difetto che nessuno esercita torna in silenzio. Chi accendera il
   prossimo collegamento non cifrato lo provi presto, e su questa riga. */
int canale_leggi(canale_t *c, void *buf, size_t n);

/* Byte scritti, che possono essere meno di `n`; -1 se per ora non entra
   niente. Chi scrive deve essere pronto a riprovare. */
int canale_scrivi(canale_t *c, const void *buf, size_t n);

/* Il descrittore sotto, per aspettarci sopra con poll(). -1 se non c'e.
   Serve **solo** ad aspettare: leggere e scrivere passano da qui sopra,
   perche con TLS sotto ci sono record e non byte. */
int canale_descrittore(const canale_t *c);

/* Chiude e libera. `c` puo essere NULL. */
void canale_chiudi(canale_t *c);

/* Perche e andata male. Mai NULL, mai contiene il token. */
const char *canale_motivo(const canale_t *c);

#endif /* CANALE_H */
