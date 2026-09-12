/* ------------------------------------------------------------------------
 * Magazzino dei segreti su NVS — solo pannello.
 *
 * Questo e il posto per cui il contratto di segreti.h e stato scritto:
 * spazio dei nomi `secrets`, separato dalla partizione dove vive
 * config.json. Il file si esporta e si copia; NVS no.
 *
 * NVS non e cifrato salvo abilitare la cifratura della flash, che e una
 * decisione di messa in opera e non di codice. Quello che questo strato
 * garantisce e piu modesto e piu utile: che un segreto non esca da nessuna
 * delle strade che il pannello offre.
 * --------------------------------------------------------------------- */
#include "magazzino.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "segreti.h"

static const char *TAG = "magazzino";
static const char *SPAZIO = "secrets";

bool magazzino_avvia(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS pieno o di una versione piu vecchia: si cancella e si
           riparte. Si perdono i segreti, e il pannello va al primo avvio —
           che e meglio di un pannello che non parte. */
        ESP_LOGW(TAG, "NVS da rifare: si riparte dal primo avvio");
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "NVS non disponibile: %s", esp_err_to_name(e));
        return false;
    }
    return true;
}

bool magazzino_scrivi(const char *chiave, const char *valore)
{
    if (!chiave || !valore) return false;

    nvs_handle_t h;
    if (nvs_open(SPAZIO, NVS_READWRITE, &h) != ESP_OK) return false;

    const bool ok = nvs_set_str(h, chiave, valore) == ESP_OK
                 && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool magazzino_leggi(const char *chiave, char *buf, size_t max)
{
    if (!chiave || !buf || !max) return false;

    nvs_handle_t h;
    if (nvs_open(SPAZIO, NVS_READONLY, &h) != ESP_OK) return false;

    size_t n = max;
    const bool ok = nvs_get_str(h, chiave, buf, &n) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool magazzino_esiste(const char *chiave)
{
    if (!chiave) return false;

    nvs_handle_t h;
    if (nvs_open(SPAZIO, NVS_READONLY, &h) != ESP_OK) return false;

    /* Si chiede solo la lunghezza: leggere il valore per poi buttarlo
       vorrebbe dire farne una copia in chiaro per niente, ed e proprio
       quello che questo strato esiste per evitare. */
    size_t n = 0;
    const bool ok = nvs_get_str(h, chiave, NULL, &n) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool magazzino_cancella(const char *chiave)
{
    if (!chiave) return false;

    nvs_handle_t h;
    if (nvs_open(SPAZIO, NVS_READWRITE, &h) != ESP_OK) return false;

    const esp_err_t e = nvs_erase_key(h, chiave);
    const bool ok = e == ESP_OK || e == ESP_ERR_NVS_NOT_FOUND;
    if (ok) nvs_commit(h);
    nvs_close(h);
    return ok;
}

bool magazzino_azzera(void)
{
    nvs_handle_t h;
    if (nvs_open(SPAZIO, NVS_READWRITE, &h) != ESP_OK) return false;

    /* Tutto lo spazio dei nomi, non chiave per chiave: e il ripristino di
       03-config-contratto.md §7, e deve non lasciare niente indietro. */
    const bool ok = nvs_erase_all(h) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}
