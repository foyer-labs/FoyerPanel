/* ------------------------------------------------------------------------
 * Luce, reset e tocco — la scheda Guition JC8012P4A1C.
 *
 * Il DSI occupa due coppie fisse nel silicio e il resto dei piedini e
 * libero: retroilluminazione e reset sono piedini veri, e il bus I2C serve
 * al solo tocco. Il codice condiviso chiama hw_retro() e hw_reset_tocco()
 * senza sapere quali numeri ci siano sotto: stanno in scheda.h.
 * --------------------------------------------------------------------- */
#include "hw_scheda.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_touch_gsl3680.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "scheda.h"

static const char *TAG = "scheda";

bool hw_i2c_avvia(i2c_master_bus_handle_t *bus)
{
    const i2c_master_bus_config_t cfg = {
        .i2c_port = -1,                  /* il primo libero */
        .sda_io_num = HW_I2C_SDA,
        .scl_io_num = HW_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&cfg, bus) != ESP_OK) {
        ESP_LOGE(TAG, "bus I2C non aperto su sda=%d scl=%d",
                 HW_I2C_SDA, HW_I2C_SCL);
        return false;
    }
    return true;
}

/* Un piedino d'uscita, acceso o spento. */
static bool piedino(int n, bool alto)
{
    if (n < 0) return true;              /* non collegato su questa scheda */
    static uint64_t preparati;
    const uint64_t bit = 1ULL << n;
    if (!(preparati & bit)) {
        const gpio_config_t c = {
            .pin_bit_mask = bit,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        if (gpio_config(&c) != ESP_OK) return false;
        preparati |= bit;
    }
    return gpio_set_level(n, alto ? 1 : 0) == ESP_OK;
}

/* --- la luce, che e una manopola e non un interruttore -----------------
 *
 * La retroilluminazione e un piedino attaccato al driver dei LED, e il BSP
 * della scheda la pilota in PWM: e la stessa cosa che 01-specifica chiede
 * per lo standby, dove la luce non si spegne, si abbassa.
 *
 * hw_retro() resta acceso/spento perche' e il contratto che il codice
 * condiviso conosce, ma sotto e una percentuale: cosi il giorno che la
 * luminosita si collega davvero non c'e niente da riscrivere qui.
 */
static bool luce_pronta;

static bool prepara_luce(void)
{
    if (luce_pronta) return true;

    const ledc_timer_config_t tempo = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_1,
        /* Ventimila hertz, che e la frequenza del BSP del costruttore. Ne
           avevo scelti duemila ragionando — sopra il ronzio udibile, sotto
           la frequenza a cui i driver dei LED economici scaldano — e il
           ragionamento non era sbagliato, ma questo convertitore lo ha
           tarato qualcuno che lo ha misurato. */
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&tempo) != ESP_OK) {
        ESP_LOGE(TAG, "il tempo della luce non si configura");
        return false;
    }

    const ledc_channel_config_t canale = {
        .gpio_num = HW_P4_RETRO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
    };
    if (ledc_channel_config(&canale) != ESP_OK) {
        ESP_LOGE(TAG, "il canale della luce non si configura");
        return false;
    }

    luce_pronta = true;
    return true;
}

bool hw_retro_percento(uint8_t percento)
{
    if (!prepara_luce()) return false;
    if (percento > 100) percento = 100;

    const uint32_t duty = (1023u * percento) / 100u;
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty) != ESP_OK)
        return false;
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1) == ESP_OK;
}

bool hw_retro(bool accesa) { return hw_retro_percento(accesa ? 100 : 0); }

/* Il reset del vetro non sta qui: lo fa il driver del pannello, che ha il
   piedino nella propria configurazione, e tirare la stessa linea da due
   posti vorrebbe dire, il giorno che i due non fossero d'accordo, un vetro
   spento senza che nessuno dei due lo sappia.

   Quello del tocco invece serve, e va fatto **prima** di cercarlo sul bus:
   il chip guarda la propria linea di interrupt all'uscita dal reset per
   decidere a quale dei due indirizzi rispondere. */
bool hw_reset_tocco(void)
{
    if (!piedino(HW_P4_TOCCO_RST, false)) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    if (!piedino(HW_P4_TOCCO_RST, true)) return false;
    vTaskDelay(pdMS_TO_TICKS(50));
    return true;
}

/* --- il GSL3680 di questa scheda ----------------------------------------
 *
 * Sta a 0x40, ed era nella scansione del bus fin dal primo giro.
 *
 * Questo chip **non ha il firmware a bordo**: il
 * driver glielo carica via I2C a ogni accensione, ed e per questo che il
 * componente pesa ottantamila righe di tabella. Quel blocco e legato al
 * modello di vetro e viene dal pacchetto del costruttore: non e codice che
 * si possa dedurre.
 *
 * Reset e interrupt sono attivi bassi, e lo specchio verticale pure viene
 * dal BSP: il vetro capacitivo e montato girato rispetto al pannello, e
 * senza quel `mirror_y` il dito andrebbe dove non e. */
bool hw_tocco_crea(i2c_master_bus_handle_t bus, uint16_t larghezza,
                   uint16_t altezza, esp_lcd_touch_handle_t *fuori)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg =
        ESP_LCD_TOUCH_IO_I2C_GSL3680_CONFIG();
    io_cfg.scl_speed_hz = HW_I2C_HZ;

    if (esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io) != ESP_OK) {
        ESP_LOGE(TAG, "io i2c del tocco non creato");
        return false;
    }

    /* Il driver vuole sapere l'indirizzo **una seconda volta**, e non e una
       ripetizione inutile: il GSL3680 sceglie fra due indirizzi guardando il
       piedino di interrupt mentre esce dal reset. Senza questo puntatore il
       driver salta quella sequenza — lo dice a voce, «Unable to initialize
       the I2C address» — e ripiega su un reset semplice, che lascia il chip
       all'indirizzo che aveva prima. Con, la sequenza e quella del firmware
       di fabbrica.

       Statica e non locale perche' il driver conserva il puntatore. */
    static esp_lcd_touch_io_gsl3680_config_t indirizzo = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_GSL3680_ADDRESS,
    };

    const esp_lcd_touch_config_t cfg = {
        .x_max = larghezza,
        .y_max = altezza,
        .rst_gpio_num = HW_P4_TOCCO_RST,
        .int_gpio_num = HW_TOCCO_INT,
        .levels = { .reset = 0, .interrupt = 0 },
        /* --- gli specchi, misurati e non dedotti ----------------------
           Erano stati scelti a occhio, e sbagliati tutti e due: mirror_y
           acceso e mirror_x spento. Un tocco nell'angolo in alto a sinistra
           veniva consegnato a LVGL come **783,1210**, cioe l'angolo opposto
           nei due sensi.

           Il conto e diretto. In verticale il chip aveva gia ragione — 1280
           meno 1210 fa 70, che e la riga giusta — e lo specchio la
           rovesciava; in orizzontale aveva torto — 800 meno 783 fa 17, che e
           la colonna giusta — e non c'era nessuno specchio a raddrizzarlo.
           Quindi i due si scambiano di posto.

           Si vedeva come «i pulsanti non fanno niente»: il velo dello
           standby copre tutto lo schermo e si toglie da qualunque punto,
           quindi il risveglio funzionava e faceva credere che il tocco
           andasse. Un pulsante invece va colpito.

           Con il vetro montato a testa in giu (HW_LCD_SPECCHIA_180) i due
           si invertono entrambi: 180 gradi sono esattamente questo. La
           costante e la stessa che specchia il disegno, apposta — vedi
           scheda_p4.h. */
        .flags = { .swap_xy = 0,
#if HW_LCD_SPECCHIA_180
                   .mirror_x = 0, .mirror_y = 1 },
#else
                   .mirror_x = 1, .mirror_y = 0 },
#endif
        .driver_data = &indirizzo,
    };
    if (esp_lcd_touch_new_i2c_gsl3680(io, &cfg, fuori) != ESP_OK) {
        ESP_LOGE(TAG, "GSL3680 non risponde a 0x40");
        return false;
    }
    return true;
}
