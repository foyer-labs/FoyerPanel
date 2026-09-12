/* ------------------------------------------------------------------------
 * Client WebSocket su socket — **condiviso fra PC e pannello**.
 *
 * Era `sim/ws_posix.c`, scritto per il simulatore. Arrivati al porting e
 * saltata fuori una cosa che cambia il conto: su ESP-IDF i socket ci sono
 * gia. lwIP espone l'interfaccia BSD — socket, connect, poll, getaddrinfo —
 * e questo file compila per l'apparecchio senza toccare una riga di logica.
 *
 * Riscriverlo su esp_websocket_client sarebbe stato lavoro per avere **due**
 * implementazioni dello stesso protocollo, di cui una sola provata. Quella
 * provata e questa: quattro fasi di prove contro Home Assistant vero le ha
 * gia fatte, e sono le stesse righe che gireranno sul muro.
 *
 * Quello che cambia davvero fra i due bersagli e altrove — dove stanno i
 * file, dove stanno i segreti — e infatti quei porti sono rimasti due.
 * --------------------------------------------------------------------- */
#include "ws.h"

#include "canale.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
/* poll() c'e su tutti e due, ma non nella stessa intestazione: su ESP-IDF
   la porta newlib in sys/poll.h, su POSIX sta in poll.h. */
#ifdef ESP_PLATFORM
#include <sys/poll.h>
#else
#include <poll.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* lwIP non definisce sempre MSG_NOSIGNAL: li i segnali non ci sono, e
   quindi non c'e niente da sopprimere. Zero e il comportamento giusto. */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* --- SHA-1, per verificare la risposta alla stretta di mano -------------
 *
 * Sessanta righe per controllare un campo che un server locale non
 * sbaglierebbe mai. Si tengono lo stesso: senza, il client accetterebbe
 * come WebSocket qualunque cosa risponda "101", e il primo frame andrebbe
 * a finire chissa dove. Un errore di configurazione — la porta di un altro
 * servizio — diventerebbe un comportamento incomprensibile invece di un
 * messaggio in chiaro.
 */
typedef struct { uint32_t h[5]; uint64_t n; uint8_t buf[64]; size_t usati; } sha1_t;

static uint32_t ruota(uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }

static void sha1_blocco(sha1_t *s, const uint8_t *p)
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16
             | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 80; i++)
        w[i] = ruota(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);            k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                     k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);   k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                     k = 0xCA62C1D6; }
        const uint32_t t = ruota(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ruota(b, 30); b = a; a = t;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}

static void sha1(const uint8_t *dati, size_t n, uint8_t fuori[20])
{
    sha1_t s = { .h = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476,
                        0xC3D2E1F0 }, .n = n };
    size_t i = 0;
    for (; i + 64 <= n; i += 64) sha1_blocco(&s, dati + i);

    uint8_t coda[128] = {0};
    const size_t resto = n - i;
    memcpy(coda, dati + i, resto);
    coda[resto] = 0x80;
    const size_t lung = resto + 1 <= 56 ? 64 : 128;
    const uint64_t bit = (uint64_t)n * 8;
    for (int k = 0; k < 8; k++) coda[lung - 1 - k] = (uint8_t)(bit >> (k * 8));
    for (size_t o = 0; o < lung; o += 64) sha1_blocco(&s, coda + o);

    for (int k = 0; k < 5; k++) {
        fuori[k * 4]     = (uint8_t)(s.h[k] >> 24);
        fuori[k * 4 + 1] = (uint8_t)(s.h[k] >> 16);
        fuori[k * 4 + 2] = (uint8_t)(s.h[k] >> 8);
        fuori[k * 4 + 3] = (uint8_t)s.h[k];
    }
}

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64(const uint8_t *d, size_t n, char *fuori)
{
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t v = (uint32_t)d[i] << 16
                         | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0)
                         | (i + 2 < n ? d[i + 2] : 0);
        fuori[o++] = B64[(v >> 18) & 63];
        fuori[o++] = B64[(v >> 12) & 63];
        fuori[o++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
        fuori[o++] = i + 2 < n ? B64[v & 63] : '=';
    }
    fuori[o] = 0;
}

/* --- stato del collegamento --------------------------------------------- */

#define IN_MAX  (WS_MESSAGGIO_MAX + 1024)

static canale_t   *canale;
static bool        cifrato_richiesto;
static ws_stato_t  stato = WS_CHIUSO;
static char        motivo[96] = "";
static char        chiave[32];        /* la Sec-WebSocket-Key mandata */

static char   *dentro;                /* byte grezzi in arrivo */
static size_t  dentro_n;
static char   *messaggio;             /* frame riassemblati */
static size_t  messaggio_n;
static bool    messaggio_pronto;
static bool    testa_letta;           /* la risposta 101 e stata consumata */

const char *ws_motivo(void) { return motivo; }
ws_stato_t  ws_stato(void)  { return stato; }

/* Il collegamento e finito, ma quello che era gia arrivato resta leggibile.
   Non e un dettaglio: Home Assistant manda `auth_invalid` e **poi** chiude,
   e buttando via il buffer insieme al socket si perderebbe proprio il
   messaggio che spiega perche. Il pannello direbbe "collegamento caduto" e
   riproverebbe in ciclo, invece di dire "token rifiutato" e fermarsi. */
static void fallisci(const char *perche)
{
    snprintf(motivo, sizeof motivo, "%s", perche);
    canale_chiudi(canale);
    canale = NULL;
    stato = WS_ERRORE;
    dentro_n = 0;
    testa_letta = false;
    /* messaggio_n e messaggio_pronto restano: li consuma ws_ricevi(). */
}

/* Un limite che si lamenta senza dire il numero costringe chi ripara ad
   alzarlo a tentativi. Con il numero accanto, si sa in un colpo se manca un
   filo o un ordine di grandezza — e nel secondo caso la cura non e alzare
   il tetto ma chiedere meno roba. */
static void troppo_grande(uint64_t quanto)
{
    char m[96];
    snprintf(m, sizeof m,
             "messaggio troppo grande: %u kB, il massimo e %u kB",
             (unsigned)(quanto / 1024), (unsigned)(WS_MESSAGGIO_MAX / 1024));
    fallisci(m);
}

void ws_chiudi(void)
{
    if (canale) {
        /* Un frame di chiusura educato: senza, il server registra un
           collegamento caduto invece di uno chiuso, e chi legge quel
           registro perde tempo a cercare un guasto che non c'e. */
        const uint8_t f[6] = { 0x88, 0x80, 0, 0, 0, 0 };
        canale_scrivi(canale, f, sizeof f);
        canale_chiudi(canale);
    }
    canale = NULL;
    stato = WS_CHIUSO;
    motivo[0] = 0;
    dentro_n = messaggio_n = 0;
    messaggio_pronto = testa_letta = false;
}

static bool memoria(void)
{
    if (!dentro)    dentro = malloc(IN_MAX);
    if (!messaggio) messaggio = malloc(WS_MESSAGGIO_MAX + 1);
    return dentro && messaggio;
}

/* --- stretta di mano ---------------------------------------------------- */

/* Scrive tutto, aspettando che il socket accetti — ma con una fine.
   La prima versione riprovava all'infinito su EAGAIN, e su un socket non
   bloccante la cui connect non e ancora finita EAGAIN e la risposta
   normale: il simulatore restava dentro questa funzione a girare a vuoto,
   e non c'era niente che potesse farlo uscire. Un ciclo senza uscita in un
   trasporto di rete e un apparecchio che si pianta, non un errore che si
   vede. */
static bool manda_tutto(const void *d, size_t n)
{
    const char *p = d;
    size_t fatti = 0;

    while (fatti < n) {
        const int k = canale_scrivi(canale, p + fatti, n - fatti);
        if (k > 0) { fatti += (size_t)k; continue; }
        /* Si aspetta che il canale accetti, e non per sempre: due secondi
           sono molti su una rete locale. Col cifrato il descrittore serve
           solo ad aspettare — i byte passano dal canale, perche sotto ci
           sono record e non byte. */
        const int fd = canale_descrittore(canale);
        if (fd < 0) return false;
        struct pollfd pf = { .fd = fd, .events = POLLOUT };
        if (poll(&pf, 1, 2000) <= 0) return false;
    }
    return true;
}

static bool richiesta_mandata;
static char richiesta_pendente[512];

static void avvia_stretta(const char *host, int porta, const char *percorso)
{
    snprintf(richiesta_pendente, sizeof richiesta_pendente,
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        percorso, host, porta, chiave);
    richiesta_mandata = false;
}

bool ws_apri(const char *host, int porta, const char *percorso,
             bool cifrato)
{
    ws_chiudi();
    if (!memoria()) { fallisci("memoria non disponibile"); return false; }

    cifrato_richiesto = cifrato;

    /* Chi apre il canale si occupa di risolvere il nome, connettersi e —
       se cifrato — verificare il certificato. Qui sopra non cambia niente:
       il protocollo e lo stesso, cambia solo cosa c'e sotto i byte. */
    canale = canale_apri(host, porta, cifrato);
    if (!canale) { fallisci("memoria non disponibile"); return false; }

    /* Sedici byte casuali: non devono essere imprevedibili, devono solo
       essere diversi fra un collegamento e l'altro perche i proxy non
       riusino una risposta. */
    uint8_t seme[16];
    for (int n = 0; n < 16; n++) seme[n] = (uint8_t)(rand() >> 7);
    base64(seme, sizeof seme, chiave);

    /* Il socket e non bloccante e la connect e ancora in corso: la
       richiesta si prepara adesso e parte da ws_gira(), quando il socket
       diventa scrivibile. */
    avvia_stretta(host, porta,
                  percorso && *percorso ? percorso : "/api/websocket");

    stato = WS_CONNETTO;
    motivo[0] = 0;
    return true;
}

/* L'accept che il server deve rispondere: SHA-1 della chiave piu il GUID
   fisso del protocollo, in base64. */
static void accept_atteso(char *fuori)
{
    static const char GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char misto[64];
    const int n = snprintf(misto, sizeof misto, "%s%s", chiave, GUID);
    uint8_t d[20];
    sha1((const uint8_t *)misto, (size_t)n, d);
    base64(d, sizeof d, fuori);
}

static void leggi_testa(void)
{
    dentro[dentro_n] = 0;
    char *fine = strstr(dentro, "\r\n\r\n");
    if (!fine) return;

    if (strncmp(dentro, "HTTP/1.1 101", 12) != 0) {
        /* 401 qui vuol dire che il percorso e protetto da qualcos'altro:
           il token di Home Assistant si presenta dopo, sul WebSocket. */
        fallisci(strstr(dentro, " 401") ? "la porta non e un WebSocket (401)"
                                        : "il server non ha accettato l'upgrade");
        return;
    }

    char atteso[32];
    accept_atteso(atteso);
    if (!strstr(dentro, atteso)) {
        fallisci("risposta di upgrade non valida");
        return;
    }

    /* Quello che avanza dopo l'intestazione sono gia frame. */
    const size_t testa = (size_t)(fine - dentro) + 4;
    memmove(dentro, dentro + testa, dentro_n - testa);
    dentro_n -= testa;
    testa_letta = true;
    stato = WS_APERTO;
}

/* --- frame -------------------------------------------------------------- */

bool ws_manda(const char *testo)
{
    if (stato != WS_APERTO || !testo) return false;

    const size_t n = strlen(testo);
    uint8_t testa[14];
    size_t t = 0;
    testa[t++] = 0x81;                       /* FIN + testo */

    const uint8_t masc = 0x80;               /* il client maschera sempre */
    if (n < 126)          testa[t++] = masc | (uint8_t)n;
    else if (n <= 0xFFFF) { testa[t++] = masc | 126;
                            testa[t++] = (uint8_t)(n >> 8);
                            testa[t++] = (uint8_t)n; }
    else                  { testa[t++] = masc | 127;
                            for (int k = 7; k >= 0; k--)
                                testa[t++] = (uint8_t)(n >> (k * 8)); }

    uint8_t chiave_masc[4];
    for (int k = 0; k < 4; k++) {
        chiave_masc[k] = (uint8_t)(rand() >> 7);
        testa[t++] = chiave_masc[k];
    }
    if (!manda_tutto(testa, t)) { fallisci("scrittura fallita"); return false; }

    /* A pezzi, per non allocare una copia del messaggio intero: get_states
       in risposta puo essere grande, ma le richieste che mandiamo noi sono
       piccole e questa resta una scrittura sola nella pratica. */
    uint8_t pezzo[512];
    for (size_t i = 0; i < n; ) {
        size_t k = 0;
        while (k < sizeof pezzo && i < n) {
            pezzo[k] = (uint8_t)testo[i] ^ chiave_masc[i % 4];
            k++; i++;
        }
        if (!manda_tutto(pezzo, k)) { fallisci("scrittura fallita"); return false; }
    }
    return true;
}

static void pong(const uint8_t *dati, size_t n)
{
    uint8_t f[4 + 125 + 4];
    size_t t = 0;
    f[t++] = 0x8A;                        /* FIN + pong */
    if (n > 125) n = 125;
    f[t++] = 0x80 | (uint8_t)n;
    uint8_t m[4];
    for (int k = 0; k < 4; k++) { m[k] = (uint8_t)(rand() >> 7); f[t++] = m[k]; }
    for (size_t i = 0; i < n; i++) f[t++] = dati[i] ^ m[i % 4];
    manda_tutto(f, t);
}

/* Prova a staccare un frame da `dentro`. Falso se non ce n'e uno intero. */
static bool un_frame(void)
{
    if (dentro_n < 2) return false;
    const uint8_t *p = (const uint8_t *)dentro;

    const bool fine = (p[0] & 0x80) != 0;
    const uint8_t codice = p[0] & 0x0F;
    const bool mascherato = (p[1] & 0x80) != 0;
    uint64_t lung = p[1] & 0x7F;
    size_t testa = 2;

    if (lung == 126) {
        if (dentro_n < 4) return false;
        lung = (uint64_t)p[2] << 8 | p[3];
        testa = 4;
    } else if (lung == 127) {
        if (dentro_n < 10) return false;
        lung = 0;
        for (int k = 0; k < 8; k++) lung = lung << 8 | p[2 + k];
        testa = 10;
    }
    /* Un server non maschera mai, ma se lo facesse il conto cambierebbe. */
    if (mascherato) testa += 4;

    if (lung > WS_MESSAGGIO_MAX) {
        troppo_grande(lung);
        return false;
    }
    if (dentro_n < testa + lung) return false;

    const uint8_t *carico = p + testa;

    switch (codice) {
    case 0x0:   /* continuazione */
    case 0x1:   /* testo */
        if (messaggio_n + lung > WS_MESSAGGIO_MAX) {
            troppo_grande(messaggio_n + lung);
            return false;
        }
        memcpy(messaggio + messaggio_n, carico, lung);
        messaggio_n += lung;
        if (fine) { messaggio[messaggio_n] = 0; messaggio_pronto = true; }
        break;
    case 0x8:   /* il server chiude */
        fallisci("collegamento chiuso dal server");
        return false;
    case 0x9:   /* ping */
        pong(carico, (size_t)lung);
        break;
    default:
        break;   /* pong e frame binari: non ci riguardano */
    }

    const size_t consumati = testa + (size_t)lung;
    memmove(dentro, dentro + consumati, dentro_n - consumati);
    dentro_n -= consumati;
    return true;
}

static bool un_frame(void);

bool ws_ricevi(char *buf, size_t max)
{
    if (!messaggio_pronto || !buf) return false;

    /* memcpy e non snprintf: la lunghezza si sa gia, e snprintf su un quarto
       di megabyte va a cercare i segnaposto in ogni singolo byte. Finche i
       messaggi erano di qualche kilobyte la differenza non si vedeva; con la
       fotografia di tutte le entita di una casa, si vede — ed e tempo rubato
       al compito grafico, che qui dentro e anche quello che legge il dito. */
    const size_t quanti = messaggio_n < max - 1 ? messaggio_n : max - 1;
    memcpy(buf, messaggio, quanti);
    buf[quanti] = 0;

    messaggio_n = 0;
    messaggio_pronto = false;
    return true;
}

void ws_gira(void)
{
    if (!canale) return;

    if (stato == WS_CONNETTO && !richiesta_mandata) {
        /* Prima che si possa chiedere l'upgrade devono essere finite le
           strette di mano di sotto: quella TCP e, se il canale e cifrato,
           quella TLS. Sono cose diverse e si concludono in momenti diversi,
           ma da qui sopra sono una domanda sola — il canale risponde
           "in corso" finche non e pronto per tutte e due. */
        const canale_fase_t fase = canale_avanza(canale);
        if (fase == CANALE_IN_CORSO) return;   /* non ancora: si riprova */
        if (fase == CANALE_ROTTO) {
            /* Il canale sa **perche**, e la ragione va portata su intatta:
               "certificato non verificato" e "collegamento rifiutato"
               mandano chi ripara in due posti diversi, e un unico
               "caduto" per entrambi li manda a caso. */
            const char *m = canale_motivo(canale);
            fallisci(*m ? m : "collegamento rifiutato");
            return;
        }

        if (!manda_tutto(richiesta_pendente, strlen(richiesta_pendente))) {
            fallisci("richiesta di upgrade non spedita");
            return;
        }
        richiesta_mandata = true;
        return;
    }

    /* One reading round: everything there is, without waiting for more.
     *
     * With the buffer full, reading stops and the rest stays in the socket,
     * where TCP keeps it: there is almost certainly a whole frame to take
     * off — IN_MAX is a kilobyte above the largest accepted message — and
     * taking it off makes room for the next round. The connection used to
     * be closed, and one large message followed by a few updates arriving
     * together was enough to disconnect the panel from a perfectly healthy
     * Home Assistant. The real failure is only when not even a full buffer
     * gives a frame (below). */
    for (;;) {
        if (dentro_n >= IN_MAX - 1) break;
        const int k = canale_leggi(canale, dentro + dentro_n,
                                   IN_MAX - 1 - dentro_n);
        if (k > 0) { dentro_n += (size_t)k; continue; }
        if (k == 0) {
            /* Chiusura pulita: prima si finisce di leggere quello che e gia
               nel buffer, poi si dichiara chiuso. L'ultimo messaggio prima
               di una chiusura e quasi sempre quello che spiega perche. */
            if (testa_letta) while (!messaggio_pronto && un_frame()) { }
            fallisci("collegamento chiuso dal server");
            return;
        }
        break;   /* per ora non c'e altro: -1 dal canale non e un guasto */
    }

    if (!testa_letta) {
        leggi_testa();
        if (!testa_letta && stato != WS_ERRORE && dentro_n >= IN_MAX - 1)
            fallisci("upgrade answer too long");
        return;
    }

    /* Un messaggio per giro: chi legge lo consuma prima del prossimo, e
       cosi non serve una coda. */
    while (!messaggio_pronto && stato == WS_APERTO)
        if (!un_frame()) break;

    /* Full, no message ready and no frame taken off: there is no way to
       make room, and waiting would change nothing. */
    if (stato == WS_APERTO && !messaggio_pronto && dentro_n >= IN_MAX - 1)
        fallisci("buffer pieno");
}
