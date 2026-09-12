/* ------------------------------------------------------------------------
 * Aggiornamento del firmware, sul pannello — vedi aggiornamento.h.
 * --------------------------------------------------------------------- */
#include "aggiornamento.h"

#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

#include "registro.h"

static const char *TAG = "ota";

static esp_ota_handle_t      scrittura;
static const esp_partition_t *bersaglio;
static bool   in_corso;
static size_t scritti;
static char   motivo[96];

static void fallito(const char *perche, esp_err_t e)
{
    snprintf(motivo, sizeof motivo, "%s (%s)", perche, esp_err_to_name(e));
    ESP_LOGE(TAG, "%s", motivo);
}

bool agg_apri(size_t totale)
{
    if (in_corso) {
        snprintf(motivo, sizeof motivo, "un aggiornamento e gia in corso");
        return false;
    }
    motivo[0] = 0;
    scritti = 0;

    /* La partizione **ferma**, mai quella che sta girando. */
    bersaglio = esp_ota_get_next_update_partition(NULL);
    if (!bersaglio) {
        snprintf(motivo, sizeof motivo, "nessuna partizione libera");
        return false;
    }

    /* Se non ci sta, meglio dirlo adesso che dopo due minuti di
       trasferimento: chi carica sta guardando una barra avanzare. */
    if (totale && totale > bersaglio->size) {
        snprintf(motivo, sizeof motivo,
                 "immagine di %u kB: la partizione ne tiene %u",
                 (unsigned)(totale / 1024), (unsigned)(bersaglio->size / 1024));
        return false;
    }

    const esp_err_t e = esp_ota_begin(bersaglio, totale ? totale
                                                        : OTA_WITH_SEQUENTIAL_WRITES,
                                      &scrittura);
    if (e != ESP_OK) { fallito("partizione non preparata", e); return false; }

    in_corso = true;
    ESP_LOGI(TAG, "aggiornamento su %s, %u kB attesi",
             bersaglio->label, (unsigned)(totale / 1024));
    return true;
}

bool agg_scrivi(const void *dati, size_t n)
{
    if (!in_corso) return false;
    if (!n) return true;

    const esp_err_t e = esp_ota_write(scrittura, dati, n);
    if (e != ESP_OK) {
        fallito("scrittura fallita", e);
        esp_ota_abort(scrittura);
        in_corso = false;
        return false;
    }
    scritti += n;
    return true;
}

bool agg_chiudi(bool completo)
{
    if (!in_corso) return false;
    in_corso = false;

    if (!completo) {
        esp_ota_abort(scrittura);
        snprintf(motivo, sizeof motivo, "trasferimento interrotto a %u kB",
                 (unsigned)(scritti / 1024));
        ESP_LOGW(TAG, "%s", motivo);
        return false;
    }

    /* Qui dentro ESP-IDF verifica l'immagine intera: intestazione, checksum
       e — con CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT — la **firma**. E il
       punto in cui un binario che non viene da noi viene rifiutato, e
       avviene prima che la partizione di avvio cambi: se non convince, al
       riavvio riparte quella di adesso. */
    esp_err_t e = esp_ota_end(scrittura);
    if (e != ESP_OK) {
        fallito(e == ESP_ERR_OTA_VALIDATE_FAILED
                ? "immagine rifiutata: firma o checksum non tornano"
                : "chiusura fallita", e);
        return false;
    }

    e = esp_ota_set_boot_partition(bersaglio);
    if (e != ESP_OK) { fallito("partizione di avvio non impostata", e); return false; }

    registro_aggiungi(LOG_INFO, "ota",
                      "immagine verificata: al prossimo riavvio si prova");
    ESP_LOGI(TAG, "installata su %s, %u kB", bersaglio->label,
             (unsigned)(scritti / 1024));
    return true;
}

const char *agg_motivo(void) { return motivo; }
size_t      agg_scritti(void) { return scritti; }

/* --- il giro di prova --------------------------------------------------- */

bool agg_in_prova(void)
{
    esp_ota_img_states_t stato;
    const esp_partition_t *p = esp_ota_get_running_partition();
    if (!p || esp_ota_get_state_partition(p, &stato) != ESP_OK) return false;
    return stato == ESP_OTA_IMG_PENDING_VERIFY;
}

void agg_conferma(void)
{
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
        registro_aggiungi(LOG_INFO, "ota",
                          "questa versione ha funzionato: confermata");
}

void agg_rifiuta(void)
{
    registro_aggiungi(LOG_ERRORE, "ota",
                      "questa versione non ha funzionato: torno alla precedente");
    /* Non torna: il bootloader rimette la partizione di prima e riavvia. */
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

const char *agg_versione(void)
{
    const esp_app_desc_t *d = esp_app_get_description();
    return d ? d->version : "";
}
