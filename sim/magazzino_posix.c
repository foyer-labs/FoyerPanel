/* ------------------------------------------------------------------------
 * Magazzino dei segreti su file — solo PC.
 *
 * Sul pannello e NVS. Qui sono file dentro `<archivio>/segreti/`, uno per
 * chiave, con i permessi piu stretti che il sistema conceda.
 *
 * **Non e un magazzino sicuro e non pretende di esserlo**: su un PC di
 * sviluppo non c'e niente da proteggere e la partizione NVS non e cifrata
 * comunque. Serve a un'altra cosa, che e la sola che conti qui: far
 * rispettare al codice lo stesso contratto che rispettera sul pannello, in
 * modo che nessuno prenda l'abitudine di leggersi un segreto quando gli fa
 * comodo. La cartella e separata da config.json apposta, cosi
 * l'esportazione della configurazione non se li porta dietro nemmeno per
 * sbaglio.
 * --------------------------------------------------------------------- */
#include "magazzino.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "archivio.h"
#include "segreti.h"

#define SOTTOCARTELLA "segreti"

static bool cartella(char *buf, size_t max)
{
    snprintf(buf, max, "%s/" SOTTOCARTELLA, archivio_dove());
    return true;
}

static bool percorso(const char *chiave, char *buf, size_t max)
{
    /* Le chiavi sono l'elenco chiuso di segreti.c, non arrivano dalla rete.
       Il controllo c'e lo stesso: e una riga, e toglie di mezzo la classe
       di errori in cui una chiave contiene una barra. */
    if (!chiave || !*chiave || strchr(chiave, '/') || strchr(chiave, '\\')
        || strstr(chiave, ".."))
        return false;

    char dir[512];
    cartella(dir, sizeof dir);
    snprintf(buf, max, "%s/%s", dir, chiave);
    return true;
}

bool magazzino_avvia(void)
{
    char dir[512];
    cartella(dir, sizeof dir);
    mkdir(dir, 0700);
    return true;
}

bool magazzino_scrivi(const char *chiave, const char *valore)
{
    char p[600];
    if (!percorso(chiave, p, sizeof p) || !valore) return false;

    magazzino_avvia();

    /* Creato a 0600 prima di scriverci: aprire e poi cambiare i permessi
       lascerebbe una finestra in cui il file e leggibile da chiunque. */
    const int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return false;

    const size_t n = strlen(valore);
    const bool scritto = n == 0 || write(fd, valore, n) == (ssize_t)n;
    if (scritto) fsync(fd);
    close(fd);
    if (!scritto) remove(p);
    return scritto;
}

bool magazzino_leggi(const char *chiave, char *buf, size_t max)
{
    char p[600];
    if (!percorso(chiave, p, sizeof p) || !buf || max == 0) return false;

    FILE *f = fopen(p, "rb");
    if (!f) return false;

    const size_t n = fread(buf, 1, max - 1, f);
    const bool troppo = !feof(f);
    fclose(f);
    if (troppo) return false;

    buf[n] = 0;
    return true;
}

bool magazzino_esiste(const char *chiave)
{
    char p[600];
    struct stat st;
    return percorso(chiave, p, sizeof p) && stat(p, &st) == 0;
}

bool magazzino_cancella(const char *chiave)
{
    char p[600];
    if (!percorso(chiave, p, sizeof p)) return false;
    /* Gia assente e un successo: chi cancella vuole che non ci sia. */
    return remove(p) == 0 || !magazzino_esiste(chiave);
}

bool magazzino_azzera(void)
{
    bool tutto = true;
    for (int n = 0; n < SEG_QUANTI; n++)
        if (!magazzino_cancella(segreto_nome((segreto_t)n))) tutto = false;

    char dir[512];
    cartella(dir, sizeof dir);
    rmdir(dir);
    return tutto;
}
