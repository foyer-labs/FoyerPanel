/* ------------------------------------------------------------------------
 * Archivio su LittleFS — solo pannello.
 *
 * Stesso contratto di archivio_posix.c, e la scrittura e atomica per la
 * stessa ragione: temporaneo, sincronizzazione, rinomina. Su un pannello a
 * muro "manca la corrente a meta salvataggio" non e un caso di scuola, e la
 * rinomina e l'operazione che o avviene o non avviene.
 *
 * LittleFS di suo e gia resistente alle interruzioni — tiene un giornale e
 * non lascia mai il filesystem in uno stato incoerente — ma questo protegge
 * il **filesystem**, non il nostro file: senza la rinomina si avrebbe un
 * config.json integro e mezzo scritto, che e peggio di uno mancante perche
 * si legge senza errori.
 * --------------------------------------------------------------------- */
#include "archivio.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "archivio";

/* Il punto di innesto e la partizione: `storage` in firmware/partitions.csv. */
#define RADICE "/dati"
static char radice[64] = RADICE;

void archivio_radice(const char *percorso)
{
    /* Sul pannello la radice e una sola e la decide la tabella delle
       partizioni: l'argomento esiste perche il simulatore ne ha bisogno, e
       qui si accetta senza usarlo invece di far divergere le due firme. */
    (void)percorso;
}

const char *archivio_dove(void) { return radice; }

bool archivio_monta(void)
{
    const esp_vfs_littlefs_conf_t cfg = {
        .base_path = RADICE,
        .partition_label = "storage",
        /* Formattare da soli al primo avvio e giusto: una partizione vuota
           appena uscita di fabbrica non e un guasto, e chiedere all'utente
           di formattare un pannello a muro non e una domanda che si possa
           fare. */
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    const esp_err_t e = esp_vfs_littlefs_register(&cfg);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "storage non montato: %s", esp_err_to_name(e));
        return false;
    }

    size_t totale = 0, usati = 0;
    if (esp_littlefs_info("storage", &totale, &usati) == ESP_OK)
        ESP_LOGI(TAG, "storage: %u kB usati su %u",
                 (unsigned)(usati / 1024), (unsigned)(totale / 1024));
    return true;
}

static const char *percorso(const char *nome, char *buf, size_t max)
{
    snprintf(buf, max, "%s/%s", radice, nome);
    return buf;
}

bool archivio_esiste(const char *nome)
{
    char p[128];
    struct stat st;
    return stat(percorso(nome, p, sizeof p), &st) == 0;
}

bool archivio_leggi(const char *nome, char *buf, size_t max, size_t *letti)
{
    char p[128];
    FILE *f = fopen(percorso(nome, p, sizeof p), "rb");
    if (!f) return false;

    const size_t n = fread(buf, 1, max - 1, f);
    const bool troppo = !feof(f);
    fclose(f);
    if (troppo) return false;

    buf[n] = 0;
    if (letti) *letti = n;
    return true;
}

bool archivio_cancella(const char *nome)
{
    char p[128];
    return remove(percorso(nome, p, sizeof p)) == 0;
}

bool archivio_scrivi(const char *nome, const char *dati, size_t n)
{
    if (n > ARCHIVIO_FILE_MAX) return false;

    char def[128], tmp[136], bak[136];
    percorso(nome, def, sizeof def);
    snprintf(tmp, sizeof tmp, "%s.tmp", def);
    snprintf(bak, sizeof bak, "%s.bak", def);

    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    const bool scritto = fwrite(dati, 1, n, f) == n;
    if (scritto) fflush(f);
    /* fsync porta i byte sulla flash: senza, la rinomina puo arrivare
       prima dei dati e si otterrebbe un file nuovo e vuoto. */
    if (scritto) fsync(fileno(f));
    fclose(f);
    if (!scritto) { remove(tmp); return false; }

    remove(bak);
    rename(def, bak);

    if (rename(tmp, def) != 0) {
        rename(bak, def);
        remove(tmp);
        return false;
    }
    return true;
}
