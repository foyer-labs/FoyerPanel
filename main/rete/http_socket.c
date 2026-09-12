/* ------------------------------------------------------------------------
 * Trasporto HTTP su socket — **condiviso fra PC e pannello**.
 *
 * Vedi ws_socket.c per il perche: lwIP dà i socket BSD anche sull'ESP32, e
 * lo stesso file serve tutti e due i bersagli. Un server HTTP minuscolo
 * scritto una volta e provato una volta.
 * --------------------------------------------------------------------- */
#include "http.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

static int            ascolto = -1;
static http_gestore_t gestore;

bool http_attivo(void) { return ascolto >= 0; }

bool http_avvia(int porta, http_gestore_t g)
{
    if (ascolto >= 0) return true;

    ascolto = socket(AF_INET, SOCK_STREAM, 0);
    if (ascolto < 0) return false;

    /* Senza, riavviare il simulatore due volte di fila trova la porta
       ancora occupata dal TIME_WAIT della volta prima. */
    const int si = 1;
    setsockopt(ascolto, SOL_SOCKET, SO_REUSEADDR, &si, sizeof si);

    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)porta);

    if (bind(ascolto, (struct sockaddr *)&a, sizeof a) != 0 ||
        listen(ascolto, 4) != 0) {
        close(ascolto);
        ascolto = -1;
        return false;
    }

    fcntl(ascolto, F_SETFL, fcntl(ascolto, F_GETFL, 0) | O_NONBLOCK);
    gestore = g;
    return true;
}

void http_ferma(void)
{
    if (ascolto >= 0) close(ascolto);
    ascolto = -1;
    gestore = NULL;
}

/* Quanto dichiara Content-Length, o zero. Case-insensitive a mano: le
   intestazioni HTTP non hanno un caso obbligato e i browser non sono
   d'accordo fra loro. */
static long dichiarato(const char *buf, const char *fine)
{
    for (const char *p = buf; p < fine; p++)
        if ((*p == 'c' || *p == 'C') &&
            strncasecmp(p, "content-length:", 15) == 0)
            return atol(p + 15);
    return 0;
}

/* Legge finche non ha **l'intestazione** intera, piu quel po' di corpo che
   e arrivato insieme. Torna quanti byte ci sono in `buf`, o -1.
 *
 * Fermarsi qui e cio che permette di decidere, prima di leggere il resto, se
 * questo corpo si puo tenere in memoria o va consumato a pezzi. Prima si
 * leggeva tutto in un colpo, e su un'immagine da due megabyte voleva dire
 * riempire il buffer, smettere a trentaquattro kilobyte e lasciare il resto
 * nel socket. */
static long leggi_intestazione(int c, char *buf, size_t max)
{
    size_t n = 0;
    while (n < max - 1) {
        const ssize_t letti = recv(c, buf + n, max - 1 - n, 0);
        if (letti <= 0) break;
        n += (size_t)letti;
        buf[n] = 0;
        if (strstr(buf, "\r\n\r\n")) break;
    }
    return n ? (long)n : -1;
}

/* Finisce di leggere il corpo dentro lo stesso buffer, per i percorsi
   normali. Torna quanti byte ci sono in tutto, o -1. */
static long leggi_resto(int c, char *buf, size_t max, size_t n)
{
    const char *fine = strstr(buf, "\r\n\r\n");
    if (!fine) return (long)n;

    const size_t inizio = (size_t)(fine - buf) + 4;
    const long atteso = dichiarato(buf, fine);

    while (n < max - 1 && n < inizio + (size_t)atteso) {
        const ssize_t letti = recv(c, buf + n, max - 1 - n, 0);
        if (letti <= 0) break;
        n += (size_t)letti;
        buf[n] = 0;
    }
    return n ? (long)n : -1;
}

/* Questa richiesta e per il percorso a flusso? In tal caso dice anche dove
   finisce l'intestazione e quanto corpo e dichiarato. */
static bool intestazione_dice(const char *buf, const char *percorso,
                              size_t *testa_n, long *quanti)
{
    if (!percorso) return false;

    const char *fine = strstr(buf, "\r\n\r\n");
    if (!fine) return false;

    /* Solo POST, e il confronto si ferma allo spazio o al punto
       interrogativo, come fa il resto del server. */
    if (strncmp(buf, "POST ", 5) != 0) return false;
    const char *p = buf + 5;
    const size_t l = strlen(percorso);
    if (strncmp(p, percorso, l) != 0) return false;
    if (p[l] != ' ' && p[l] != '?') return false;

    *testa_n = (size_t)(fine - buf) + 4;
    *quanti  = dichiarato(buf, fine);
    return true;
}

/* --- il percorso a flusso ------------------------------------------------
 *
 * Uno solo, e registrato da chi lo usa. Vedi http.h per il perche non e un
 * meccanismo generale. */
static http_flusso_t flusso;
static bool          flusso_c_e;

void http_flusso(const http_flusso_t *f)
{
    if (f) { flusso = *f; flusso_c_e = true; }
    else     flusso_c_e = false;
}

/* Legge il corpo a pezzi e lo passa a chi lo consuma. Torna vero se sono
   arrivati tutti i byte dichiarati **e** nessun pezzo e stato rifiutato.
 *
 * `gia` sono i byte di corpo che erano gia arrivati insieme
 * all'intestazione: su una richiesta grande il primo recv() ne porta sempre
 * un po', e buttarli vorrebbe dire cominciare l'immagine dal byte
 * sbagliato — cioe un firmware corrotto che si scopre solo alla firma. */
static bool travasa_corpo(int c, const char *gia, size_t gia_n, long dichiarati)
{
    if (!flusso.apri(dichiarati > 0 ? (size_t)dichiarati : 0)) return false;

    size_t fatti = 0;
    bool   bene = true;

    if (gia_n) {
        bene = flusso.pezzo(gia, gia_n);
        fatti = gia_n;
    }

    /* Il pezzo e piccolo di proposito: e RAM interna, e sta sulla pila di
       chi serve la richiesta. Quattro kilobyte per volta sono piu che
       sufficienti — il collo di bottiglia e la scrittura in flash, non
       questo buffer — e ottantamila richieste da 4 kB non sono piu lente di
       ottomila da 40. */
    static char pezzo[4096];
    while (bene && (dichiarati <= 0 || fatti < (size_t)dichiarati)) {
        const ssize_t letti = recv(c, pezzo, sizeof pezzo, 0);
        if (letti <= 0) break;          /* caduta o fine */
        bene = flusso.pezzo(pezzo, (size_t)letti);
        fatti += (size_t)letti;
    }

    const bool completo = bene && dichiarati > 0 && fatti >= (size_t)dichiarati;
    return flusso.chiudi(completo);
}

static const char *frase(int codice)
{
    switch (codice) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 413: return "Payload Too Large";
    case 422: return "Unprocessable Entity";
    case 429: return "Too Many Requests";
    default:  return "Internal Server Error";
    }
}

static void rispondi(int c, const risposta_t *s)
{
    const size_t n = s->corpo_n ? s->corpo_n
                                : (s->corpo ? strlen(s->corpo) : 0);
    char testa[256];
    const int t = snprintf(testa, sizeof testa,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        /* La pagina la serve il pannello stesso: nessuna origine esterna
           deve poter parlare con questi percorsi da un'altra scheda del
           browser. Non c'e Access-Control-Allow-Origin, ed e voluto. */
        "X-Content-Type-Options: nosniff\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "\r\n",
        s->codice, frase(s->codice), s->tipo ? s->tipo : "text/plain", n);

    if (t > 0) send(c, testa, (size_t)t, 0);
    if (n) send(c, s->corpo, n, 0);
}

void http_gira(void)
{
    if (ascolto < 0 || !gestore) return;

    struct sockaddr_in da;
    socklen_t l = sizeof da;
    const int c = accept(ascolto, (struct sockaddr *)&da, &l);
    if (c < 0) return;          /* nessuno ha bussato */

    /* Un client che apre e non parla non deve fermare l'interfaccia. */
    struct timeval t = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof t);

    /* --- il buffer della richiesta, e dove sta ------------------------
     *
     * Era `static char buf[...]`, e trentaquattro kilobyte statici su
     * questo chip non stanno dove capita: stanno in **RAM interna**, che
     * e la memoria scarsa, e ci stanno dall'accensione anche nei giorni in
     * cui nessuno apre la pagina di configurazione.
     *
     * Chiesto al mucchio finisce da solo in PSRAM: sopra i sedici
     * kilobyte l'allocatore va li, e trentaquattro ci sono comodamente.
     * Un buffer di rete in PSRAM non si sente — il collo di bottiglia e
     * il Wi-Fi, non la memoria — mentre trentaquattro kilobyte interni si
     * sentono eccome, ed erano quelli che mancavano quando il minimo
     * storico segnava zero.
     *
     * Chiesto una volta e tenuto: la pagina si apre di rado ma quando si
     * apre fa molte richieste di fila, e chiedere e restituire a ogni
     * richiesta frammenterebbe il mucchio per non guadagnare niente. */
    enum { BUF_N = HTTP_CORPO_MAX + 2048 };
    static char *buf;
    if (!buf) {
        buf = malloc(BUF_N);
        if (!buf) { close(c); return; }   /* si riprovera al prossimo giro */
    }

    /* --- il percorso a flusso si riconosce **prima** di leggere il corpo -
     *
     * leggi_tutto() aspetta di avere tutti i byte che Content-Length
     * dichiara: su un'immagine da due megabyte riempirebbe il buffer,
     * smetterebbe di leggere a trentaquattro kilobyte, e il resto
     * resterebbe nel socket. Percio qui si legge **solo l'intestazione**, e
     * se il percorso e quello a flusso il corpo prosegue a pezzi. */
    long n = leggi_intestazione(c, buf, BUF_N);
    if (n <= 0) { close(c); return; }

    size_t testa_n = 0;
    long   dichiarati = 0;
    const bool a_flusso = flusso_c_e &&
                          intestazione_dice(buf, flusso.percorso,
                                            &testa_n, &dichiarati);
    if (a_flusso) {
        const bool esito = travasa_corpo(c, buf + testa_n, (size_t)n - testa_n,
                                         dichiarati);
        risposta_t s = {0};
        flusso.risposta(esito, &s);
        rispondi(c, &s);
        if (s.libera) s.libera((void *)s.corpo);
        close(c);
        return;
    }

    /* Too large is known from the header already: it says so. Reading it
       to refuse it afterwards would mean filling the buffer, cutting it and
       handing a truncated body to the handler — which for JSON is an
       "invalid" instead of the "too large" it really is. */
    {
        const char *fine_testa = strstr(buf, "\r\n\r\n");
        const long quanto = fine_testa ? dichiarato(buf, fine_testa) : 0;
        if (quanto > HTTP_CORPO_MAX) {
            const risposta_t grande = {
                .codice = 413, .tipo = "application/json",
                .corpo = "{\"error\":\"too large\"}",
            };
            rispondi(c, &grande);
            close(c);
            return;
        }
    }

    /* Percorso normale: si finisce di leggere il corpo come sempre. */
    n = leggi_resto(c, buf, BUF_N, (size_t)n);
    if (n <= 0) { close(c); return; }

    /* "METODO /percorso?query HTTP/1.1" */
    char *sp1 = strchr(buf, ' ');
    char *sp2 = sp1 ? strchr(sp1 + 1, ' ') : NULL;
    if (!sp1 || !sp2) { close(c); return; }
    *sp1 = 0;
    *sp2 = 0;

    richiesta_t r = {0};
    r.metodo = strcmp(buf, "GET") == 0    ? HTTP_GET
             : strcmp(buf, "POST") == 0   ? HTTP_POST
                                          : HTTP_ALTRO;
    r.percorso = sp1 + 1;

    char *dom = strchr(sp1 + 1, '?');
    if (dom) { *dom = 0; r.query = dom + 1; }
    else       r.query = "";

    const char *corpo = strstr(sp2 + 1, "\r\n\r\n");
    r.corpo = corpo ? corpo + 4 : "";
    r.corpo_n = strlen(r.corpo);

    /* Less arrived than declared: the client stopped, or the network lost
       the tail beyond the two seconds of waiting. A truncated body does not
       reach the handler — a configuration cut in half is a syntax error at
       best. */
    const long attesi = corpo ? dichiarato(sp2 + 1, corpo) : 0;
    const size_t arrivati = corpo ? (size_t)n - (size_t)(r.corpo - buf) : 0;
    if (attesi > 0 && arrivati < (size_t)attesi) {
        const risposta_t monca = {
            .codice = 400, .tipo = "application/json",
            .corpo = "{\"error\":\"incomplete request\"}",
        };
        rispondi(c, &monca);
        close(c);
        return;
    }

    static char chi[46];
    inet_ntop(AF_INET, &da.sin_addr, chi, sizeof chi);
    r.chiamante = chi;

    risposta_t s = {0};
    if (r.corpo_n > HTTP_CORPO_MAX) {
        s.codice = 413;
        s.tipo = "application/json";
        s.corpo = "{\"errore\":\"troppo grande\"}";
    } else {
        gestore(&r, &s);
    }

    rispondi(c, &s);
    if (s.libera) s.libera((void *)s.corpo);
    close(c);
}
