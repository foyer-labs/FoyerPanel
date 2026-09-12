/* ------------------------------------------------------------------------
 * Canale — attuazione su ESP-IDF, con esp-tls.
 *
 * esp-tls copre tutti e due i casi: con `is_plain_tcp` si comporta da socket
 * normale, senza si porta dietro la stretta di mano cifrata. Una strada
 * sola, quindi, e non due che divergono col tempo.
 *
 * La verifica del certificato usa il fascio di radici di ESP-IDF
 * (esp_crt_bundle), lo stesso insieme che ha un browser. Basta a un
 * certificato Let's Encrypt come quello dell'Home Assistant di casa, e non
 * richiede di copiare niente nel pannello: le radici cambiano di rado e
 * arrivano con l'aggiornamento del firmware.
 * --------------------------------------------------------------------- */
#include "canale.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "esp_tls.h"

struct canale {
    esp_tls_t *tls;
    char       host[128];
    int        porta;
    bool       cifrato;
    bool       pronto;
    char       motivo[96];
};

canale_t *canale_apri(const char *host, int porta, bool cifrato)
{
    if (!host || !*host) return NULL;

    canale_t *c = calloc(1, sizeof *c);
    if (!c) return NULL;

    snprintf(c->host, sizeof c->host, "%s", host);
    c->porta = porta;
    c->cifrato = cifrato;

    c->tls = esp_tls_init();
    if (!c->tls) {
        free(c);
        return NULL;
    }
    return c;
}

canale_fase_t canale_avanza(canale_t *c)
{
    if (!c) return CANALE_ROTTO;
    if (c->pronto) return CANALE_PRONTO;
    if (!c->tls) return CANALE_ROTTO;

    esp_tls_cfg_t cfg = { 0 };
    if (c->cifrato) {
        /* Il fascio di radici, e nessun modo di saltarlo: vedi canale.h. */
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
        cfg.common_name = c->host;
    } else {
        cfg.is_plain_tcp = true;
    }
    cfg.non_block = true;
    cfg.timeout_ms = 10000;

    /* Torna 1 quando ha finito, 0 mentre e in corso, -1 se e andata male.
       Va richiamata con gli **stessi** argomenti finche non conclude: e la
       forma non bloccante, e riprendere da dove aveva lasciato e compito
       suo. */
    const int esito = esp_tls_conn_new_async(c->host, (int)strlen(c->host),
                                             c->porta, &cfg, c->tls);
    if (esito == 1) {
        c->pronto = true;
        return CANALE_PRONTO;
    }
    if (esito == 0) return CANALE_IN_CORSO;

    /* Si distingue il fallimento della **verifica** da tutto il resto: sono
       due pomeriggi diversi. Un certificato che non convince vuol dire nome
       sbagliato in configurazione o una radice che non abbiamo, e in nessuno
       dei due casi serve guardare la rete. */
    esp_tls_error_handle_t err = NULL;
    int flag_tls = 0, codice = 0;
    if (esp_tls_get_error_handle(c->tls, &err) == ESP_OK && err) {
        esp_tls_get_and_clear_last_error(err, &codice, &flag_tls);
    }
    if (flag_tls != 0)
        snprintf(c->motivo, sizeof c->motivo,
                 "certificato non verificato (0x%x): il nome corrisponde?",
                 -flag_tls);
    else if (codice == ESP_ERR_MBEDTLS_SSL_SETUP_FAILED)
        /* Quasi sempre e memoria: mbedTLS vuole due buffer da 16 kB e li
           chiede dove gli e stato detto di chiederli. Dirlo con il numero
           di byte liberi accanto risparmia la traduzione di "-0x7F00", che
           e il modo in cui questo guasto si presenta nel registro e non
           somiglia per niente a una memoria finita. */
        snprintf(c->motivo, sizeof c->motivo,
                 "TLS non inizializzato: %u kB interni liberi",
                 (unsigned)(esp_get_free_internal_heap_size() / 1024));
    else if (c->cifrato)
        snprintf(c->motivo, sizeof c->motivo,
                 "collegamento cifrato non riuscito (0x%x)", codice);
    else
        snprintf(c->motivo, sizeof c->motivo, "collegamento rifiutato");

    return CANALE_ROTTO;
}

/* --- «adesso non c'e niente» non e «e finita» ---------------------------
 *
 * Con TLS sotto, esp-tls dice le due cose con due codici suoi, e bastava
 * riconoscere quelli. **Sul TCP in chiaro no**: li `esp_tls_conn_read` e'
 * una recv() nuda, che su un socket non bloccante senza dati risponde -1
 * con errno EAGAIN — lo stesso -1 di un collegamento che si e' rotto.
 *
 * Questo file ha vissuto due anni senza accorgersene perche' il TCP in
 * chiaro non lo usava nessuno: Home Assistant e' cifrato, e da quella parte
 * i codici con un nome ci sono. La prima cosa a parlare in chiaro sono
 * state le telecamere, e il risultato era un riquadro che diceva «la
 * telecamera ha chiuso» prima ancora che il pannello avesse mandato la
 * prima domanda — la telecamera non aveva chiuso niente: non le era stato
 * chiesto niente.
 *
 * Percio' quando il numero non ha un nome si guarda errno. */
static bool solo_non_adesso(int errore)
{
    return errore == EAGAIN || errore == EWOULDBLOCK || errore == EINTR;
}

int canale_leggi(canale_t *c, void *buf, size_t n)
{
    if (!c || !c->tls || !c->pronto) return -1;

    errno = 0;
    const int k = esp_tls_conn_read(c->tls, buf, n);
    if (k == ESP_TLS_ERR_SSL_WANT_READ || k == ESP_TLS_ERR_SSL_WANT_WRITE)
        return -1;
    if (k < 0) {
        if (solo_non_adesso(errno)) return -1;
        snprintf(c->motivo, sizeof c->motivo, "lettura fallita");
        return 0;   /* per chi legge, e finita: il canale non da altro */
    }
    return k;
}

int canale_scrivi(canale_t *c, const void *buf, size_t n)
{
    if (!c || !c->tls || !c->pronto) return -1;

    errno = 0;
    const int k = esp_tls_conn_write(c->tls, buf, n);
    if (k == ESP_TLS_ERR_SSL_WANT_READ || k == ESP_TLS_ERR_SSL_WANT_WRITE)
        return -1;
    if (k < 0) {
        /* Qui il valore di ritorno e' lo stesso nei due casi — chi scrive
           riprova comunque — ma il motivo no: scriverlo quando il socket
           era solo pieno vorrebbe dire un guasto stampato per una
           condizione normale. */
        if (!solo_non_adesso(errno))
            snprintf(c->motivo, sizeof c->motivo, "scrittura fallita");
        return -1;
    }
    return k;
}

int canale_descrittore(const canale_t *c)
{
    if (!c || !c->tls) return -1;
    int fd = -1;
    /* Il tipo non e const nell'API: la copia serve solo a chiedere il
       descrittore, e non cambia niente di quello che c'e dentro. */
    esp_tls_get_conn_sockfd((esp_tls_t *)c->tls, &fd);
    return fd;
}

void canale_chiudi(canale_t *c)
{
    if (!c) return;
    if (c->tls) esp_tls_conn_destroy(c->tls);
    free(c);
}

const char *canale_motivo(const canale_t *c)
{
    return c && c->motivo[0] ? c->motivo : "";
}
