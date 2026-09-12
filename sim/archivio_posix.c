/* ------------------------------------------------------------------------
 * Archivio su file normali — solo PC.
 *
 * Sul pannello lo stesso contratto lo servira LittleFS. Qui la scrittura
 * atomica si fa come si e sempre fatta su un filesystem serio: si scrive un
 * temporaneo, lo si forza sul supporto con fsync, e solo allora si rinomina.
 * La rinomina e l'operazione che o avviene o non avviene, e non lascia vie
 * di mezzo — ed e proprio quella la garanzia che serve quando manca la
 * corrente a meta salvataggio.
 * --------------------------------------------------------------------- */
#include "archivio.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char radice[256] = "dati";

void archivio_radice(const char *percorso)
{
    if (!percorso) return;
    snprintf(radice, sizeof radice, "%s", percorso);
    mkdir(radice, 0755);
}

const char *archivio_dove(void) { return radice; }

static const char *percorso(const char *nome, char *buf, size_t max)
{
    snprintf(buf, max, "%s/%s", radice, nome);
    return buf;
}

bool archivio_esiste(const char *nome)
{
    char p[512];
    struct stat st;
    return stat(percorso(nome, p, sizeof p), &st) == 0;
}

bool archivio_leggi(const char *nome, char *buf, size_t max, size_t *letti)
{
    char p[512];
    FILE *f = fopen(percorso(nome, p, sizeof p), "rb");
    if (!f) return false;

    const size_t n = fread(buf, 1, max - 1, f);
    const bool troppo = !feof(f);
    fclose(f);
    if (troppo) return false;      /* piu grande di quanto ci sta */

    buf[n] = 0;
    if (letti) *letti = n;
    return true;
}

bool archivio_cancella(const char *nome)
{
    char p[512];
    return remove(percorso(nome, p, sizeof p)) == 0;
}

bool archivio_scrivi(const char *nome, const char *dati, size_t n)
{
    if (n > ARCHIVIO_FILE_MAX) return false;

    /* Piu larghi del percorso perche ci si aggiunge un suffisso. */
    char def[512], tmp[520], bak[520];
    percorso(nome, def, sizeof def);
    snprintf(tmp, sizeof tmp, "%s.tmp", def);
    snprintf(bak, sizeof bak, "%s.bak", def);

    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    const bool scritto = fwrite(dati, 1, n, f) == n;
    /* fflush porta i byte al sistema, fsync li porta sul supporto: senza il
       secondo la rinomina potrebbe arrivare prima dei dati. */
    if (scritto) fflush(f);
    if (scritto) fsync(fileno(f));
    fclose(f);
    if (!scritto) { remove(tmp); return false; }

    /* Il precedente diventa la copia di sicurezza. Se non c'era, pazienza:
       vuol dire che questa e la prima scrittura. */
    remove(bak);
    rename(def, bak);

    if (rename(tmp, def) != 0) {
        /* La rinomina non e riuscita: si rimette a posto il precedente
           invece di lasciare il file principale mancante. */
        rename(bak, def);
        remove(tmp);
        return false;
    }
    return true;
}
