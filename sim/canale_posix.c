/* ------------------------------------------------------------------------
 * Canale — attuazione POSIX, per il simulatore.
 *
 * In chiaro sono socket normali. Cifrato serve OpenSSL, e OpenSSL sul PC
 * puo esserci o non esserci: CMake lo cerca e definisce PANNELLO_CON_OPENSSL
 * se lo trova. Dove non c'e, chiedere un canale cifrato **fallisce dicendo
 * perche** invece di collegarsi in chiaro.
 *
 * Quel ripiego silenzioso sarebbe la cosa peggiore possibile qui: chi prova
 * il simulatore contro il Home Assistant di casa vedrebbe "non si collega" e
 * cercherebbe il guasto nella rete, mentre il pannello vero — dove esp-tls
 * c'e sempre — funziona. Due bersagli che si comportano diverso senza dirlo
 * sono peggio di uno che manca.
 * --------------------------------------------------------------------- */
#include "canale.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef PANNELLO_CON_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

struct canale {
    int   sk;
    bool  cifrato;
    bool  connesso;   /* la connect e finita */
    bool  pronto;     /* anche la stretta di mano, se cifrato */
    char  host[128];
    char  motivo[96];
#ifdef PANNELLO_CON_OPENSSL
    SSL_CTX *ctx;
    SSL     *ssl;
#endif
};

canale_t *canale_apri(const char *host, int porta, bool cifrato)
{
    if (!host || !*host) return NULL;

    canale_t *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->sk = -1;
    c->cifrato = cifrato;
    snprintf(c->host, sizeof c->host, "%s", host);

#ifndef PANNELLO_CON_OPENSSL
    if (cifrato) {
        snprintf(c->motivo, sizeof c->motivo,
                 "simulatore senza OpenSSL: TLS non disponibile");
        return c;   /* rotto, ma con la ragione scritta sopra */
    }
#endif

    char porta_s[8];
    snprintf(porta_s, sizeof porta_s, "%d", porta);

    struct addrinfo suggerimenti = { .ai_family = AF_INET,
                                     .ai_socktype = SOCK_STREAM };
    struct addrinfo *dove = NULL;
    if (getaddrinfo(host, porta_s, &suggerimenti, &dove) != 0 || !dove) {
        snprintf(c->motivo, sizeof c->motivo, "indirizzo non risolto");
        return c;
    }

    c->sk = socket(dove->ai_family, dove->ai_socktype, 0);
    if (c->sk < 0) {
        freeaddrinfo(dove);
        snprintf(c->motivo, sizeof c->motivo, "socket non creato");
        return c;
    }
    fcntl(c->sk, F_SETFL, fcntl(c->sk, F_GETFL, 0) | O_NONBLOCK);
    const int si = 1;
    setsockopt(c->sk, IPPROTO_TCP, TCP_NODELAY, &si, sizeof si);

    const int esito = connect(c->sk, dove->ai_addr, dove->ai_addrlen);
    freeaddrinfo(dove);
    if (esito != 0 && errno != EINPROGRESS) {
        close(c->sk);
        c->sk = -1;
        snprintf(c->motivo, sizeof c->motivo, "collegamento rifiutato");
    }
    return c;
}

#ifdef PANNELLO_CON_OPENSSL
/* La stretta di mano cifrata, un pezzo per chiamata. */
static canale_fase_t stringi(canale_t *c)
{
    if (!c->ctx) {
        c->ctx = SSL_CTX_new(TLS_client_method());
        if (!c->ctx) {
            snprintf(c->motivo, sizeof c->motivo, "TLS non inizializzato");
            return CANALE_ROTTO;
        }
        /* Le radici di sistema, e la verifica **pretesa**: vedi canale.h. */
        SSL_CTX_set_default_verify_paths(c->ctx);
        SSL_CTX_set_verify(c->ctx, SSL_VERIFY_PEER, NULL);

        c->ssl = SSL_new(c->ctx);
        if (!c->ssl) {
            snprintf(c->motivo, sizeof c->motivo, "TLS non inizializzato");
            return CANALE_ROTTO;
        }
        SSL_set_fd(c->ssl, c->sk);
        /* Il nome va detto due volte e per due ragioni diverse: a SNI perche
           il server sappia quale certificato presentare, e alla verifica
           perche il certificato presentato corrisponda. Dimenticare la
           seconda e il modo classico di avere un TLS che non protegge. */
        SSL_set_tlsext_host_name(c->ssl, c->host);
        SSL_set1_host(c->ssl, c->host);
    }

    const int esito = SSL_connect(c->ssl);
    if (esito == 1) {
        c->pronto = true;
        return CANALE_PRONTO;
    }
    const int perche = SSL_get_error(c->ssl, esito);
    if (perche == SSL_ERROR_WANT_READ || perche == SSL_ERROR_WANT_WRITE)
        return CANALE_IN_CORSO;

    const long v = SSL_get_verify_result(c->ssl);
    if (v != X509_V_OK)
        snprintf(c->motivo, sizeof c->motivo,
                 "certificato non verificato: %s",
                 X509_verify_cert_error_string(v));
    else
        snprintf(c->motivo, sizeof c->motivo,
                 "collegamento cifrato non riuscito");
    return CANALE_ROTTO;
}
#endif

canale_fase_t canale_avanza(canale_t *c)
{
    if (!c || c->sk < 0) return CANALE_ROTTO;
    if (c->pronto) return CANALE_PRONTO;

    if (!c->connesso) {
        /* SO_ERROR vale zero anche mentre la connect e ancora per strada:
           prima si chiede se il socket e scrivibile, e solo allora
           l'errore vuol dire qualcosa. */
        struct pollfd p = { .fd = c->sk, .events = POLLOUT };
        if (poll(&p, 1, 0) <= 0) return CANALE_IN_CORSO;

        int err = 0;
        socklen_t n = sizeof err;
        getsockopt(c->sk, SOL_SOCKET, SO_ERROR, &err, &n);
        if (err != 0) {
            snprintf(c->motivo, sizeof c->motivo, "collegamento rifiutato");
            return CANALE_ROTTO;
        }
        c->connesso = true;
    }

    if (!c->cifrato) {
        c->pronto = true;
        return CANALE_PRONTO;
    }
#ifdef PANNELLO_CON_OPENSSL
    return stringi(c);
#else
    return CANALE_ROTTO;
#endif
}

int canale_leggi(canale_t *c, void *buf, size_t n)
{
    if (!c || c->sk < 0 || !c->pronto) return -1;

#ifdef PANNELLO_CON_OPENSSL
    if (c->cifrato) {
        const int k = SSL_read(c->ssl, buf, (int)n);
        if (k > 0) return k;
        const int perche = SSL_get_error(c->ssl, k);
        if (perche == SSL_ERROR_WANT_READ || perche == SSL_ERROR_WANT_WRITE)
            return -1;
        return 0;
    }
#endif
    const ssize_t k = recv(c->sk, buf, n, 0);
    if (k < 0) return (errno == EAGAIN || errno == EWOULDBLOCK) ? -1 : 0;
    return (int)k;
}

int canale_scrivi(canale_t *c, const void *buf, size_t n)
{
    if (!c || c->sk < 0 || !c->pronto) return -1;

#ifdef PANNELLO_CON_OPENSSL
    if (c->cifrato) {
        const int k = SSL_write(c->ssl, buf, (int)n);
        if (k > 0) return k;
        return -1;
    }
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
    const ssize_t k = send(c->sk, buf, n, MSG_NOSIGNAL);
    return k < 0 ? -1 : (int)k;
}

int canale_descrittore(const canale_t *c) { return c ? c->sk : -1; }

void canale_chiudi(canale_t *c)
{
    if (!c) return;
#ifdef PANNELLO_CON_OPENSSL
    if (c->ssl) SSL_free(c->ssl);
    if (c->ctx) SSL_CTX_free(c->ctx);
#endif
    if (c->sk >= 0) close(c->sk);
    free(c);
}

const char *canale_motivo(const canale_t *c)
{
    return c && c->motivo[0] ? c->motivo : "";
}
