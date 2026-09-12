/* ------------------------------------------------------------------------
 * Tocco — solo pannello.
 *
 * Il chip lo crea il file della scheda, hw_scheda.c, dopo averlo tolto dal
 * reset; qui si legge il dito e lo si consegna a LVGL.
 * --------------------------------------------------------------------- */
#include "hw_scheda.h"
#include "hw_tocco.h"

#include "esp_lcd_touch.h"
#include "esp_log.h"
#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "profile.h"
#include "tocco_eventi.h"
#include "scheda.h"

static const char *TAG = "tocco";

static esp_lcd_touch_handle_t tocco;
static TaskHandle_t           compito;
static lv_indev_t            *ingresso;

/* --- perche il tocco ha un compito tutto suo ----------------------------
 *
 * Il chip del tocco rileva a ritmo suo, e esp_lcd_touch_get_data azzera il
 * conteggio dei punti dopo averlo letto: una lettura capitata fra due
 * rilevamenti trova zero e riporta "dito alzato" mentre il dito e ancora
 * giu.
 *
 * Contro quello bastava una tenuta a tempo: una lettura vuota non e una
 * prova che il dito si sia alzato, lo diventa dopo sessanta millisecondi di
 * letture vuote. Quella cura e ancora qui sotto e serve ancora.
 *
 * Ma non copriva il caso opposto, ed e quello che si e visto sul vetro come
 * "ogni cosa vuole due tocchi": **nessuna lettura**. Finche il tocco veniva
 * letto dentro lv_timer_handler, lo si leggeva quando il ciclo principale
 * arrivava a chiamarlo — e quel ciclo, oltre a disegnare un fotogramma
 * intero e ad aspettare il ritorno di quadro, fa girare Home Assistant e
 * il server web. Fra una lettura e l'altra possono passare
 * duecento millisecondi, e un tocco deciso ne dura centocinquanta: comincia
 * e finisce nel buco, il chip non lo tiene in serbo per nessuno, e sul vetro
 * non e successo niente. Si tocca di nuovo, e magari quella volta cade bene.
 *
 * Da qui il compito separato. Legge il chip per conto suo — svegliato dal
 * piedino di interrupt, e comunque almeno ogni quindici millisecondi, cosi
 * se l'interrupt non arrivasse resterebbe una lettura periodica e non un
 * pannello morto.
 *
 * Quello che legge non lo tiene come **stato** ma come **successione**, in
 * main/app/tocco_eventi.c: uno stato letto in ritardo racconta com'e adesso,
 * e un tocco cominciato e finito nel frattempo non lascia traccia. La prima
 * versione di questo file lo teneva come stato con due bandiere, e nessuno
 * — chi l'ha scritta compresa — e mai riuscito a dimostrarla giusta
 * guardandola. La coda si dimostra guardandola, e si prova sul PC: venticinque
 * asserzioni in test/prova_tocco_eventi.c, compresi i due casi che le
 * bandiere non sapevano coprire.
 *
 * Costa una pila in RAM interna, che qui e la cosa piu scarsa che ci sia. E
 * il prezzo giusto: la RAM si guarda in un contatore, un tocco perso lo
 * paga chi sta davanti al pannello ogni volta che lo usa.
 */
#define TOCCO_TENUTA_MS   60
#define TOCCO_PERIODO_MS  15
/* Misurata due volte sul pannello. Con 3072 ne restavano liberi 1468, con
   2048 ne restavano 436: il compito ne usa milleseicento e rotti, costante,
   perche fa sempre le stesse due chiamate I2C.

   Duemila erano troppo pochi. Quattrocento byte di margine reggono finche
   tutto va bene, ma la prima riga di registro stampata da dentro il driver
   — un errore sul bus, e succede — ne mangia altrettanti, e una pila che
   trabocca non da un guasto leggibile: da un riavvio senza spiegazione.
   Duemilacinquecentosessanta lasciano quasi un kilobyte di margine e
   restituiscono comunque mezzo kilobyte rispetto ai 3072 di partenza.

   Il numero si rilegge con `stato`, riga "pila tocco". */
#define TOCCO_PILA        2560

/* Fra il compito che legge e LVGL che chiede c'e la coda delle transizioni
   — main/app/tocco_eventi.c, che si prova sul PC perche non tocca hardware.
   Qui resta solo il lucchetto: quella coda non si protegge da sola apposta,
   cosi non si porta dentro un tipo di FreeRTOS e resta compilabile dove la
   si puo mettere alla prova. */
static portMUX_TYPE      serratura = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t bussata;      /* l'interrupt del tocco */

static void IRAM_ATTR su_interrupt(esp_lcd_touch_handle_t tp)
{
    LV_UNUSED(tp);
    BaseType_t sveglia = pdFALSE;
    xSemaphoreGiveFromISR(bussata, &sveglia);
    if (sveglia == pdTRUE) portYIELD_FROM_ISR();
}

static void compito_tocco(void *arg)
{
    LV_UNUSED(arg);
    uint32_t visto_ms = 0;
    bool     giu_locale = false;

    for (;;) {
        /* Con l'attesa a tempo il compito funziona anche se il piedino di
           interrupt non dicesse niente: allora e una lettura ogni quindici
           millisecondi, che e il ritmo del chip. */
        xSemaphoreTake(bussata, pdMS_TO_TICKS(TOCCO_PERIODO_MS));

        esp_lcd_touch_point_data_t punto = {0};
        uint8_t quanti = 0;

        /* Due chiamate e non una: la prima interroga il chip sull'I2C, la
           seconda legge quello che ha risposto. */
        esp_lcd_touch_read_data(tocco);
        esp_lcd_touch_get_data(tocco, &punto, &quanti, 1);

        const uint32_t ora = (uint32_t)(esp_timer_get_time() / 1000);

        /* La tenuta vale solo per il **rilascio**: una lettura vuota non e
           una prova che il dito si sia alzato, lo diventa dopo sessanta
           millisecondi di letture vuote. Una lettura piena, invece, e sempre
           una prova che il dito c'e. */
        if (quanti > 0) {
            giu_locale = true;
            visto_ms = ora;
        } else if (giu_locale && (ora - visto_ms) >= TOCCO_TENUTA_MS) {
            giu_locale = false;
        } else {
            continue;   /* nel dubbio non si dice niente */
        }

        portENTER_CRITICAL(&serratura);
        tocco_letto((int16_t)punto.x, (int16_t)punto.y, giu_locale);
        portEXIT_CRITICAL(&serratura);
    }
}

/* LVGL chiede se qualcuno sta toccando, e qui non si parla piu con l'I2C:
   si consuma quello che il compito ha gia messo in fila. Un dito solo — il
   pannello non ha nessun gesto a due dita.

   Una transizione per chiamata, la piu vecchia. Se ne fossero successe due
   fra due chiamate, la seconda esce alla chiamata dopo: quello che non
   succede mai e che una sparisca. E tutta la differenza con lo stato, che
   letto in ritardo racconta com'e adesso e non cosa e successo. */
/* --- l'ultimo tratto di dito, per poterlo guardare ----------------------
 *
 * Un tocco che non arriva e muto: sul vetro non succede niente e non resta
 * niente da leggere. Questo anello tiene gli ultimi eventi **consegnati a
 * LVGL** — non quelli letti dal chip: e la differenza fra i due che conta,
 * perche il chip puo aver visto benissimo un dito che poi si e perso per
 * strada. Si legge dalla console con `tocco`.
 *
 * Otto voci da otto byte: sessantaquattro byte di RAM interna. Sta qui
 * finche il difetto dei tocchi non e chiuso. */
#define TOCCO_STORIA 8
static struct { int16_t x, y; uint16_t ms; bool giu; } storia[TOCCO_STORIA];
static uint8_t storia_n;

void hw_tocco_racconta(void)
{
    printf("ultimi tocchi consegnati a LVGL (piu recente per ultimo):\n");
    for (int k = 0; k < TOCCO_STORIA; k++) {
        const int i = (storia_n + k) % TOCCO_STORIA;
        if (!storia[i].ms) continue;
        printf("  %5u ms  %4d,%-4d  %s\n", storia[i].ms,
               storia[i].x, storia[i].y,
               storia[i].giu ? "premuto" : "rilasciato");
    }
    printf("in coda: %d, perse perche la coda era piena: %u\n",
           tocco_in_attesa(), (unsigned)tocco_perse());
}

static void annota(int16_t x, int16_t y, bool giu)
{
    const int prec = (storia_n + TOCCO_STORIA - 1) % TOCCO_STORIA;
    if (storia[prec].ms && storia[prec].giu == giu) return;

    storia[storia_n].x = x;
    storia[storia_n].y = y;
    storia[storia_n].ms = (uint16_t)(esp_timer_get_time() / 1000);
    storia[storia_n].giu = giu;
    storia_n = (uint8_t)((storia_n + 1) % TOCCO_STORIA);
}

static void leggi(lv_indev_t *indev, lv_indev_data_t *dati)
{
    LV_UNUSED(indev);

    portENTER_CRITICAL(&serratura);
    const tocco_evento_t e = tocco_prossimo();
    portEXIT_CRITICAL(&serratura);

    annota(e.x, e.y, e.giu);

    /* Le coordinate ci sono sempre, anche da rilasciato: LVGL le usa per
       sapere **dove** e finito il tocco. */
    dati->point.x = e.x;
    dati->point.y = e.y;
    dati->state = e.giu ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool hw_tocco_avvia(i2c_master_bus_handle_t bus)
{
    /* Quale chip sia lo sa il file della scheda, non questo: qui si legge
       il dito, e un dito e un dito. */
    if (!hw_tocco_crea(bus, PRF->schermo.larghezza, PRF->schermo.altezza,
                       &tocco))
        return false;

    tocco_azzera();

    bussata = xSemaphoreCreateBinary();
    if (!bussata) {
        ESP_LOGE(TAG, "semaforo del tocco non creato");
        return false;
    }
    /* Se l'interrupt non si aggancia non e fatale — resta la lettura ogni
       quindici millisecondi — ma va detto: la differenza fra "va a
       interrupt" e "va a tempo" e visibile solo qui. */
    if (esp_lcd_touch_register_interrupt_callback(tocco, su_interrupt) != ESP_OK)
        ESP_LOGW(TAG, "interrupt non agganciato: si legge a tempo");

    if (xTaskCreate(compito_tocco, "tocco", TOCCO_PILA, NULL, 5, &compito)
        != pdPASS) {
        ESP_LOGE(TAG, "compito del tocco non avviato");
        return false;
    }

    ingresso = lv_indev_create();
    lv_indev_set_type(ingresso, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ingresso, leggi);
    lv_indev_set_display(ingresso, lv_display_get_default());

    ESP_LOGI(TAG, "vetro pronto su %ux%u",
             PRF->schermo.larghezza, PRF->schermo.altezza);
    return true;
}

uint32_t hw_tocco_pila_libera(void)
{
    if (!compito) return 0;
    return (uint32_t)(uxTaskGetStackHighWaterMark(compito) * sizeof(StackType_t));
}
