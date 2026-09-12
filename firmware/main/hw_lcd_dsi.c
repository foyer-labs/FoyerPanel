/* ------------------------------------------------------------------------
 * Schermo MIPI-DSI e display LVGL — solo pannello.
 *
 * Fra il chip e il vetro c'e un controllore serio: il segnale e
 * differenziale su due coppie, i tempi li tiene il pannello, e non c'e
 * niente da tarare. In cambio c'e una sequenza di accensione lunga decine
 * di registri, che nessuno deduce e che porta il componente del vetro.
 *
 * La parte che conta per l'interfaccia: due framebuffer e disegno a
 * fotogramma pieno, con LVGL che dipinge sempre in quello nascosto. Chi
 * disegna dentro la memoria che la periferica sta mostrando fa vedere il
 * pennello al lavoro.
 *
 * La scelta e stata pagata. Cercando uno schermo nero l'ho abbandonata per
 * il framebuffer singolo del demo del costruttore, e il nero non c'entrava
 * niente — era il vetro configurato con i tempi della variante sbagliata.
 * Col framebuffer singolo il vetro si e acceso, e si e acceso **a righe
 * orizzontali**: il pennello al lavoro.
 *
 * Anche il costruttore lo sa: i suoi esempi semplici usano un framebuffer e
 * sfarfallano, quello serio accende AVOID_TEAR e DIRECT_MODE, che vuol dire
 * due framebuffer e scambio al ritorno di quadro. Come qui.
 * --------------------------------------------------------------------- */
#include "hw_lcd.h"

#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_heap_caps.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_lcd_jd9365.h"

#include "profile.h"
#include "scheda.h"
#include "sistema.h"

static const char *TAG = "lcd";

static esp_lcd_panel_handle_t     pannello;
static esp_lcd_dsi_bus_handle_t   bus;
static esp_lcd_panel_io_handle_t  io;
static lv_display_t              *display;
static SemaphoreHandle_t          quadro;

/* Il ritorno di quadro: il controllore DSI ha finito di mostrare un
   fotogramma e lo scambio dei due buffer e avvenuto. */
static bool IRAM_ATTR su_quadro(esp_lcd_panel_handle_t p,
                                esp_lcd_dpi_panel_event_data_t *e, void *dato)
{
    (void)p; (void)e; (void)dato;
    BaseType_t sveglia = pdFALSE;
    /* I fotogrammi si contano qui e non nel travaso: con il disegno a
       pezzi il travaso avviene molte volte per fotogramma, e contarlo li
       darebbe un numero che somiglia agli fps senza esserlo. Qui e la
       periferica che dice di avere finito di mostrarne uno. */
    sistema_fotogramma();
    xSemaphoreGiveFromISR(quadro, &sveglia);
    return sveglia == pdTRUE;
}

/* --- il travaso ---------------------------------------------------------
 *
 * draw_bitmap qui non copia niente: il buffer che LVGL passa **e** uno dei
 * due framebuffer, quindi il controllore si limita a cambiare quale dei due
 * legge, dal prossimo ritorno di quadro. */
static void travasa(lv_display_t *d, const lv_area_t *area, uint8_t *px)
{
    (void)area;

    /* --- una chiamata per zona, uno scambio per fotogramma --------------
     *
     * In modo diretto LVGL chiama questa funzione **una volta per ogni zona
     * dichiarata cambiata**, e i pixel li ha gia scritti dentro il
     * framebuffer: non c'e niente da copiare. Quello che va fatto una volta
     * sola e lo scambio dei due buffer, e va fatto quando le zone sono
     * finite — se no si scambierebbe a meta fotogramma, mostrando un quadro
     * disegnato per meta.
     *
     * Lo dice LVGL con lv_display_flush_is_last(). Senza quella riga il
     * difetto non sarebbe uno schermo rotto: sarebbe uno schermo che a volte
     * mostra mezzo aggiornamento, cioe il genere di cosa che si vede e non
     * si sa spiegare. */
    if (!lv_display_flush_is_last(d)) return;

    /* Il ritorno precedente non interessa: si azzera il segnale prima di
       chiedere lo scambio, cosi chi aspetta aspetta **questo**. */
    xSemaphoreTake(quadro, 0);

    esp_lcd_panel_draw_bitmap(pannello, 0, 0, PRF->schermo.larghezza,
                              PRF->schermo.altezza, px);
}

/* Qui si aspetta, ed e LVGL a decidere quando: chiama questa funzione
 * all'inizio del ridisegno successivo, cioe solo quando gli serve davvero
 * il buffer che il controllore potrebbe ancora star mostrando.
 *
 * Non dentro travasa: quello gira dentro lv_timer_handler, e fermarsi li
 * fermerebbe tutti i timer di LVGL, compreso quello che legge il dito. */
static void aspetta_scambio(lv_display_t *d)
{
    (void)d;
    xSemaphoreTake(quadro, pdMS_TO_TICKS(100));
}

/* LVGL vuole sapere che ora e. */
static uint32_t adesso_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* --- l'alimentazione delle corsie ---------------------------------------
 *
 * Le corsie MIPI vogliono 2,5 V da un regolatore interno che va acceso a
 * mano. Non e un dettaglio da manuale: senza, il bus DSI si configura senza
 * lamentarsi e il vetro resta nero — il genere di guasto che fa cercare per
 * ore dalla parte sbagliata. */
static bool accendi_corsie(void)
{
    static esp_ldo_channel_handle_t ldo;
    const esp_ldo_channel_config_t cfg = {
        .chan_id = 3,               /* [silicio] il canale delle corsie MIPI */
        .voltage_mv = 2500,
    };
    if (esp_ldo_acquire_channel(&cfg, &ldo) != ESP_OK) {
        ESP_LOGE(TAG, "regolatore delle corsie MIPI non acceso");
        return false;
    }
    return true;
}

bool hw_lcd_avvia(void)
{
    quadro = xSemaphoreCreateBinary();
    if (!quadro) return false;

    if (!accendi_corsie()) return false;

    const esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = HW_LCD_DSI_CORSIE,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = HW_LCD_DSI_MBPS_CORSIA,
    };
    if (esp_lcd_new_dsi_bus(&bus_cfg, &bus) != ESP_OK) {
        ESP_LOGE(TAG, "bus DSI non creato");
        return false;
    }

    const esp_lcd_dbi_io_config_t io_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    if (esp_lcd_new_panel_io_dbi(bus, &io_cfg, &io) != ESP_OK) {
        ESP_LOGE(TAG, "canale comandi DSI non creato");
        return false;
    }

    /* Il pixel clock viene dal costruttore, non da un conto: vedi
       HW_LCD_DPI_MHZ in scheda_p4.h. Le due somme qui sotto servono solo a
       scrivere nel registro a quanti fotogrammi al secondo si va, che e
       l'unica cosa che quel numero racconta a chi legge. */
    const uint32_t riga = (uint32_t)PRF->schermo.larghezza + HW_LCD_HSYNC_PULSE
                          + HW_LCD_HSYNC_BACK + HW_LCD_HSYNC_FRONT;
    const uint32_t righe_tot = (uint32_t)PRF->schermo.altezza + HW_LCD_VSYNC_PULSE
                            + HW_LCD_VSYNC_BACK + HW_LCD_VSYNC_FRONT;
    const uint32_t quadri = (HW_LCD_DPI_MHZ * 1000000u) / (riga * righe_tot);

    const esp_lcd_dpi_panel_config_t dpi = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = HW_LCD_DPI_MHZ,
        .virtual_channel = 0,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = 2,          /* vedi il perche in cima al file */
        .video_timing = {
            .h_size = PRF->schermo.larghezza,
            .v_size = PRF->schermo.altezza,
            .hsync_back_porch  = HW_LCD_HSYNC_BACK,
            .hsync_pulse_width = HW_LCD_HSYNC_PULSE,
            .hsync_front_porch = HW_LCD_HSYNC_FRONT,
            .vsync_back_porch  = HW_LCD_VSYNC_BACK,
            .vsync_pulse_width = HW_LCD_VSYNC_PULSE,
            .vsync_front_porch = HW_LCD_VSYNC_FRONT,
        },
        .flags.use_dma2d = true,
        /* --- il collegamento non scende mai in bassa potenza -------------
         *
         * Per impostazione predefinita il DSI passa in bassa potenza
         * durante **ogni** intervallo orizzontale, cioe fra una riga e
         * l'altra, e risale in alta velocita per quella dopo. E una scelta
         * di consumo, e su un vetro che non digerisce bene quella
         * transizione si paga riga per riga: le aree piene si vedono
         * striate, e si vedono striate anche riempiendo il framebuffer di
         * bianco senza passare da nessuna interfaccia — che e come lo
         * abbiamo trovato.
         *
         * Con questo il collegamento resta in alta velocita per tutto il
         * quadro. Costa qualche milliampere su un apparecchio attaccato al
         * muro, e sono milliampere ben spesi. */
        .flags.disable_lp = true,
    };

    /* init_cmds a NULL: si usa la sequenza predefinita del componente, che
       viene dal costruttore del vetro. Se il pannello di questa scheda ne
       volesse una sua, e qui che va messa — ma va **avuta**, non dedotta. */
    const jd9365_vendor_config_t vendor = {
        .init_cmds = NULL,
        .init_cmds_size = 0,
        .mipi_config = { .dsi_bus = bus, .dpi_config = &dpi,
                         .lane_num = HW_LCD_DSI_CORSIE },
    };
    const esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = HW_P4_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor,
    };
    if (esp_lcd_new_panel_jd9365(io, &dev, &pannello) != ESP_OK) {
        ESP_LOGE(TAG, "vetro JD9365 non riconosciuto");
        return false;
    }

    esp_lcd_panel_reset(pannello);
    esp_lcd_panel_init(pannello);

    /* **E questa accende il vetro**, e mancava.
     *
     * `init` esegue la sequenza del costruttore e fa uscire il pannello dal
     * sonno, ma lascia il display spento: sono due comandi diversi del
     * JD9365, e il primo non implica il secondo. Senza questa riga il
     * pannello rispondeva a tutto — la lettura dell'identificativo dava
     * `LCD ID: 93 65 04`, quindi il DSI parlava — e restava **nero con la
     * retroilluminazione accesa**.
     *
     * E il guasto peggiore da cercare, perche' somiglia a tutti gli altri:
     * tempi sbagliati, corsie sbagliate, alimentazione del PHY, piedini
     * della scheda. Erano tutti giusti, e a scagionarli e stata quella riga
     * dell'identificativo nel registro: un vetro che risponde e un vetro
     * collegato bene. */
    if (esp_lcd_panel_disp_on_off(pannello, true) != ESP_OK)
        ESP_LOGW(TAG, "il vetro non ha accettato l'accensione del display");

    /* --- montato a testa in giu (scheda_p4.h) --------------------------
     *
     * Specchiare i due assi equivale a ruotare di 180 gradi, e farlo qui —
     * nel registro del pannello — non costa niente: il framebuffer resta
     * quello che era, la modalita di rendering resta DIRECT, gli fps
     * restano quelli.
     *
     * Se il driver non lo accetta lo si **dice**: su alcuni pannelli DSI la
     * direzione di scansione la fissa il generatore di temporizzazioni e
     * questa chiamata torna ESP_ERR_NOT_SUPPORTED. Restare zitti vorrebbe
     * dire un vetro dritto e un tocco rovesciato — perche il tocco si
     * specchia comunque, leggendo la stessa costante — cioe il guasto
     * peggiore dei due. */
#if HW_LCD_SPECCHIA_180
    {
        const esp_err_t e = esp_lcd_panel_mirror(pannello, true, true);
        if (e == ESP_OK)
            ESP_LOGI(TAG, "vetro specchiato di 180 gradi");
        else
            ESP_LOGE(TAG, "il vetro non accetta lo specchio (%s): il disegno "
                          "restera dritto e il tocco rovesciato", esp_err_to_name(e));
    }
#endif

    const esp_lcd_dpi_panel_event_callbacks_t eventi = {
        .on_refresh_done = su_quadro,
    };
    esp_lcd_dpi_panel_register_event_callbacks(pannello, &eventi, NULL);

    void *fb1 = NULL, *fb2 = NULL;
    if (esp_lcd_dpi_panel_get_frame_buffer(pannello, 2, &fb1, &fb2) != ESP_OK) {
        ESP_LOGE(TAG, "framebuffer non ottenuti");
        return false;
    }

    lv_init();
    lv_tick_set_cb(adesso_ms);

    display = lv_display_create(PRF->schermo.larghezza, PRF->schermo.altezza);
    if (!display) return false;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, travasa);
    lv_display_set_flush_wait_cb(display, aspetta_scambio);

    /* --- diretto, cioe solo le zone cambiate ---------------------------
     *
     * Qui c'era il modo a fotogramma pieno, con accanto la ragione per cui
     * non si faceva altrimenti: «ridisegnare solo le zone cambiate vorrebbe
     * dire tenere allineati due framebuffer che si alternano, e una zona
     * dimenticata resterebbe vecchia a giri alterni». La preoccupazione era
     * giusta. Quello che non era vero e che toccasse a noi risolverla: LVGL
     * in modo diretto con due buffer si tiene l'elenco delle zone del giro
     * precedente e le ricopia nell'altro — refr_sync_areas() in lv_refr.c.
     *
     * La differenza si e vista quando i riquadri video hanno cominciato ad
     * aggiornarsi davvero — c'erano le telecamere, che poi sono uscite dal
     * progetto. Dichiarare cambiata una miniatura da 130x73 faceva
     * ridisegnare 800x1280: un megapixel per un decimo di percento di
     * schermo, cinque volte al secondo per quattro riquadri. Il
     * pannello restava in piedi e non rispondeva piu: il server web sopra
     * gli otto secondi, la RAM interna libera al minimo scesa da 122 a
     * 65 kB.
     *
     * I due framebuffer restano, e resta lo scambio al ritorno di quadro
     * che ha curato le righe orizzontali: cambia solo quanto si ridipinge
     * fra uno scambio e l'altro. */
    const size_t byte = (size_t)PRF->schermo.larghezza *
                        PRF->schermo.altezza * 2;
    lv_display_set_buffers(display, fb1, fb2, byte,
                           LV_DISPLAY_RENDER_MODE_DIRECT);

    ESP_LOGI(TAG, "DSI: %u MHz su %ux%u compresi i ritorni = %u quadri/s",
             (unsigned)HW_LCD_DPI_MHZ, (unsigned)riga, (unsigned)righe_tot,
             (unsigned)quadri);
    ESP_LOGI(TAG, "DSI pronto: %ux%u, %d corsie a %d Mbit/s",
             PRF->schermo.larghezza, PRF->schermo.altezza,
             HW_LCD_DSI_CORSIE, HW_LCD_DSI_MBPS_CORSIA);
    return true;
}

lv_display_t *hw_lcd_display(void) { return display; }

bool hw_lcd_prova(uint16_t colore)
{
    if (!pannello) return false;

    /* --- da un buffer **nostro**, non dal framebuffer del driver ---------
     *
     * Il demo del costruttore non chiama mai
     * esp_lcd_dpi_panel_get_frame_buffer(): consegna a draw_bitmap un
     * buffer proprio e lascia che il driver copi. Noi facciamo l'altra
     * cosa — scriviamo dentro i suoi framebuffer e gli chiediamo di
     * scambiarli — ed e l'unica strada che il loro demo non percorre.
     *
     * Questa prova passa dalla strada loro apposta: se il bianco si vede
     * da qui e non dall'interfaccia, il problema e in quella nostra e non
     * nel vetro.
     *
     * Il buffer si tiene: due megabyte in PSRAM su trentadue, e chiederli
     * e restituirli a ogni prova frammenterebbe il mucchio per niente. */
    static uint16_t *mio;
    const size_t quanti = (size_t)PRF->schermo.larghezza *
                          PRF->schermo.altezza;
    if (!mio) {
        mio = heap_caps_malloc(quanti * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!mio) {
            ESP_LOGE(TAG, "niente memoria per il buffer di prova");
            return false;
        }
    }

    for (size_t n = 0; n < quanti; n++) mio[n] = colore;

    return esp_lcd_panel_draw_bitmap(pannello, 0, 0, PRF->schermo.larghezza,
                                     PRF->schermo.altezza, mio) == ESP_OK;
}
