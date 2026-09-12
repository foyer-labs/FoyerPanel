/* ------------------------------------------------------------------------
 * Console sulla seriale — solo pannello.
 *
 * Serve a far entrare le due cose che non possono stare in nessun file del
 * progetto: la password della rete e il token di Home Assistant. Le scrive
 * chi ha il pannello davanti, sul proprio terminale, e da li vanno dritte in
 * NVS attraverso segreti_scrivi(). Non passano da config.json, non finiscono
 * nel registro, e non tornano indietro: segreti.h non ha una funzione che
 * restituisca un segreto, e questa console non fa eccezione — `stato` dice
 * "impostata" o "manca", mai il valore.
 *
 * Non e impalcatura da buttare dopo il collaudo. E il rientro per quando il
 * pannello **non** riesce ad attaccarsi alla rete, che e esattamente il
 * momento in cui la pagina di configurazione non e raggiungibile e in cui
 * serve poter correggere una password. Un apparecchio che si configura solo
 * attraverso la rete che non ha e un apparecchio da staccare dal muro.
 * --------------------------------------------------------------------- */
#include "console.h"

#include <string.h>

#include "argtable3/argtable3.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "hw_lcd.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

/* La pila del compito grafico si misura di la, ma si guarda di qua. */
extern TaskHandle_t compito_lvgl;

#include "archivio.h"
#include "lvgl.h"

#include "config.h"
#include "ha.h"
#include "hw_tocco.h"
#include "nel_grafico.h"
#include "orologio.h"
#include "rete.h"
#include "tempi.h"
#include "segreti.h"

static const char *TAG = "console";

/* --- rimettere insieme quello che lo spezzettatore ha diviso -------------
 *
 * Una password puo contenere spazi. Chi la scrive fra virgolette arriva qui
 * come un argomento solo; chi non lo fa arriva a pezzi, e rimetterli insieme
 * con uno spazio in mezzo e la cosa giusta nella stragrande maggioranza dei
 * casi. Gli spazi doppi si perdono: sta scritto nell'aiuto. */
static void ricuci(int argc, char **argv, char *fuori, size_t max)
{
    fuori[0] = 0;
    for (int i = 1; i < argc; i++) {
        if (i > 1) strlcat(fuori, " ", max);
        strlcat(fuori, argv[i], max);
    }
}

/* --- who runs them: the graphics task --------------------------------------
 *
 * Configuration, secrets and Home Assistant belong to the graphics task, and
 * none of the three has a lock: see nel_grafico.h. The commands that touch
 * them prepare here what they need and have it run over there.
 *
 * If the graphics task does not answer within five seconds, the command
 * runs from here, as it always did. Not a shortcut: this console is also
 * the way back from a stuck graphics task, and then there is nobody over
 * there to race with — while waiting forever would take away exactly the
 * emergency exit. */
#define ATTESA_GRAFICO_MS 5000

/* What a command takes over there, and what it brings back. */
typedef struct {
    char testo[SEGRETO_MAX];   /* network name, password, token, address */
    bool cifrato;
    int  esito;                /* what the command returns               */
} lavoro_t;

static void esegui(void (*fai)(void *), lavoro_t *l)
{
    if (nel_grafico(fai, l, ATTESA_GRAFICO_MS)) return;
    printf("(the graphics task has not answered for %d s: running from here)\n",
           ATTESA_GRAFICO_MS / 1000);
    fai(l);
}

/* --- commands ------------------------------------------------------------ */

static void fai_rete(void *d)
{
    lavoro_t *l = d;
    l->esito = 1;

    /* Two steps and two messages. A single "could not do it" for two
       different faults — no document in memory, or the flash write failed —
       makes whoever repairs guess, and a rescue console cannot afford that
       kind of ambiguity. */
    if (!cfg_imposta_testo("system/network/ssid", l->testo)) {
        printf("could not set the name: no configuration in memory\n");
        return;
    }
    if (!cfg_salva()) {
        printf("name set but not saved: the flash write failed\n");
        return;
    }
    /* The name is not a secret and can be read back from config.json, but
       it is not printed here either: what shows on the terminal ends up in
       the history of whoever is looking, and 10-diagnostica.md puts the
       SSID in the same list as the password. */
    printf("network name saved\n");
    rete_riprova();
    l->esito = 0;
}

static int c_rete(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: rete <network name>\n");
        return 1;
    }
    lavoro_t l = {0};
    /* A network name fits in 32 bytes: the rest of the field is unused,
       but the limit stays the 64 it was. */
    ricuci(argc, argv, l.testo, 64);
    esegui(fai_rete, &l);
    return l.esito;
}

static void fai_chiave(void *d)
{
    lavoro_t *l = d;
    const bool ok = segreti_scrivi(SEG_WIFI_PASSWORD, l->testo);
    /* Erased before anything is printed. */
    memset(l->testo, 0, sizeof l->testo);

    printf(ok ? "password saved\n" : "could not save it\n");
    if (ok) rete_riprova();
    l->esito = ok ? 0 : 1;
}

static int c_chiave(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: chiave <password>   (in quotes if it has spaces)\n");
        return 1;
    }
    lavoro_t l = {0};
    ricuci(argc, argv, l.testo, sizeof l.testo);
    esegui(fai_chiave, &l);
    /* Again, and not twice for nothing: fai_chiave() erases the password
       when it writes it, but if it never got there the copy would stay on
       this stack. */
    const int esito = l.esito;
    memset(&l, 0, sizeof l);
    return esito;
}

static void fai_token(void *d)
{
    lavoro_t *l = d;
    const bool ok = segreti_scrivi(SEG_HA_TOKEN, l->testo);
    memset(l->testo, 0, sizeof l->testo);

    printf(ok ? "token saved\n" : "could not save it\n");
    if (ok) ha_riprova();
    l->esito = ok ? 0 : 1;
}

static int c_token(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: token <Home Assistant long-lived access token>\n");
        return 1;
    }
    lavoro_t l = {0};
    ricuci(argc, argv, l.testo, sizeof l.testo);
    esegui(fai_token, &l);
    const int esito = l.esito;
    memset(&l, 0, sizeof l);
    return esito;
}

static void fai_ha(void *d)
{
    lavoro_t *l = d;
    l->esito = 1;
    /* The address only: port and path already have a sensible fallback
       (8123 and /api/websocket), and whoever has different ones goes
       through the configuration page, which validates the whole document.
       This console is for getting **to** that page, not for replacing it. */
    if (!cfg_imposta_testo("home_assistant/host", l->testo)) {
        printf("could not set it: no configuration in memory\n");
        return;
    }
    if (!cfg_salva()) {
        printf("set but not saved: the flash write failed\n");
        return;
    }
    if (!cfg_imposta_vero("home_assistant/tls", l->cifrato) || !cfg_salva()) {
        printf("address saved, but not the encryption\n");
        return;
    }
    printf(l->cifrato ? "address saved, over TLS\n"
                      : "address saved, in clear\n");
    ha_riprova();
    l->esito = 0;
}

static int c_ha(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ha <name or address> [cifrato]\n");
        printf("       without http:// and without a path; with \"cifrato\" it talks TLS,\n");
        printf("       and then it needs the **name**, because that is what the\n");
        printf("       certificate declares: against an address the check fails.\n");
        return 1;
    }
    lavoro_t l = {0};
    strlcpy(l.testo, argv[1], sizeof l.testo);
    l.cifrato = (argc > 2 && strcmp(argv[2], "cifrato") == 0);
    esegui(fai_ha, &l);
    return l.esito;
}

/* `stato` goes over there too, and not out of scruple: it reads texts of
   the configuration by pointer — which a save from the web page replaces —
   and asks LVGL how much memory is left, that is it walks its heap while
   the graphics task allocates in it. */
static void fai_stato(void *d)
{
    lavoro_t *l = d;

    const char *ssid = cfg_testo("system/network/ssid", "");
    printf("network set      : %s\n", ssid[0] ? "yes" : "no");
    printf("password         : %s\n",
           segreti_impostato(SEG_WIFI_PASSWORD) ? "set" : "missing");
    /* The state alone is not enough: "dropped" for a wrong address and
       "dropped" for an unconvincing certificate send whoever repairs to two
       different places. The reason is the useful half of the line. */
    const char *perche = ha_motivo();
    printf("Home Assistant   : %s%s - %s%s%s\n",
           cfg_testo("home_assistant/host", "(no address)"),
           cfg_vero("home_assistant/tls", false) ? " (TLS)" : "",
           ha_stato_nome(ha_stato()),
           perche && *perche ? " - " : "", perche ? perche : "");
    printf("HA token         : %s\n",
           segreti_impostato(SEG_HA_TOKEN) ? "set" : "missing");
    printf("connection       : %s\n", rete_descrizione());

    /* The time is the first thing one looks at on a wall panel, and the
       first noticed wrong. The applied time zone is here too, because a
       right time in the wrong place looks like a clock fault and not a
       configuration one. */
    char ora[8], data[40];
    orologio_ora(ora, sizeof ora);
    orologio_data(data, sizeof data);
    printf("time             : %s %s (%s)\n", ora,
           data[0] ? data : "- not synchronised yet",
           cfg_testo("system/timezone", "no time zone"));

    /* The lowest ever, not the value now: a stack is tight at the worst
       moment, and that moment is almost never the one someone looks at. */
    static const char *const STATI[] = { "boot", "active", "standby", "off" };
    printf("screen           : %s, idle for %u s\n",
           STATI[tempi_stato() % 4], (unsigned)(tempi_fermo_ms() / 1000));
    /* LVGL's memory is a fixed block in internal RAM, and we have hit it
       before: the diagnostics screen built more objects than fit,
       lv_obj_create returned NULL and the panel restarted. The **largest**
       free piece matters more than the total: it decides whether the next
       object fits. */
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    printf("graphics memory  : %u of %u bytes used, largest free piece %u\n",
           (unsigned)(m.total_size - m.free_size), (unsigned)m.total_size,
           (unsigned)m.free_biggest_size);

    printf("graphics stack   : %u bytes free at worst\n",
           (unsigned)(uxTaskGetStackHighWaterMark(compito_lvgl) *
                      sizeof(StackType_t)));
    printf("touch stack      : %u bytes free at worst\n",
           (unsigned)hw_tocco_pila_libera());

    /* Free **now** says little: it is looked at in a quiet moment and
       hides the minute the panel was at risk. The lowest ever is the
       number to decide on. */
    printf("memory           : %u kB internal now, %u kB at the lowest, "
           "%u kB in PSRAM\n",
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    l->esito = 0;
}

static int c_stato(int argc, char **argv)
{
    (void)argc; (void)argv;
    lavoro_t l = {0};
    esegui(fai_stato, &l);
    return l.esito;
}

/* Provvisorio, finche i tocchi non sono a posto: dice cosa e arrivato
   davvero all'interfaccia, che e l'unica cosa che un dito non puo
   raccontare. */
static int c_tocco(int argc, char **argv)
{
    (void)argc; (void)argv;
    hw_tocco_racconta();
    return 0;
}

static void fai_dimentica(void *d)
{
    lavoro_t *l = d;
    cfg_imposta_testo("system/network/ssid", "");
    cfg_salva();
    segreti_scrivi(SEG_WIFI_PASSWORD, "");   /* empty = erase */
    printf("network forgotten\n");
    rete_riprova();
    l->esito = 0;
}

static int c_dimentica(int argc, char **argv)
{
    (void)argc; (void)argv;
    lavoro_t l = {0};
    esegui(fai_dimentica, &l);
    return l.esito;
}

/* --- il rientro da una configurazione che non va ------------------------
 *
 * Una configurazione sbagliata puo bloccare il compito grafico, e con lui
 * se ne vanno interfaccia e pagina di configurazione: non si puo sbloccare
 * il pannello dal vetro perche il vetro non risponde, e non si puo
 * correggere dalla pagina perche la pagina non c'e. Restano la seriale e
 * questo comando.
 *
 * Cancella **solo** la configurazione: rete, password e token restano dove
 * sono, in NVS. E la scelta giusta perche quello che si sta cercando di
 * recuperare e la configurazione, non l'accesso — e ritrovarsi anche senza
 * rete dopo un ripristino vorrebbe dire due problemi invece di uno.
 *
 * Poi riavvia, perche quello che gira in memoria e proprio la cosa che si e
 * appena tolta da sotto i piedi. */
static int c_azzera(int argc, char **argv)
{
    (void)argc; (void)argv;

    const bool a = archivio_cancella(CFG_FILE);
    const bool b = archivio_cancella(CFG_COPIA);
    printf("configurazione: %s, copia: %s\n",
           a ? "cancellata" : "non c'era",
           b ? "cancellata" : "non c'era");
    printf("riavvio: riparte come appena montato\n");
    fflush(stdout);
    esp_restart();
    return 0;
}

static int c_riavvia(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("riavvio\n");
    fflush(stdout);
    esp_restart();
    return 0;
}

/* --- cercare un chip sul bus invece di sapere dov'e -----------------------
 *
 * I piedini I2C di scheda_p4.h portano scritto [da confermare]: sono
 * ricostruiti dalla documentazione della famiglia, non letti da uno schema.
 * Al primo avvio il tocco non ha risposto, e da li ci sono due strade —
 * provare coppie di numeri ricompilando ogni volta, oppure chiedere alla
 * scheda.
 *
 * Questo comando chiede alla scheda. Apre un bus sui piedini che gli si
 * danno, interroga i centoventotto indirizzi e dice chi ha risposto. Un
 * giro dura meno di un secondo, e la risposta e un fatto invece di
 * un'ipotesi.
 *
 * Senza argomenti prova le coppie plausibili una dopo l'altra: sono quelle
 * che le schede P4 in giro usano davvero, e trovarne una e piu veloce che
 * leggere uno schema.
 */
static void scan_coppia(int sda, int scl)
{
    i2c_master_bus_handle_t bus = NULL;
    const i2c_master_bus_config_t cfg = {
        .i2c_port = -1,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&cfg, &bus) != ESP_OK) {
        printf("sda=%2d scl=%2d : il bus non si apre\n", sda, scl);
        return;
    }

    int trovati = 0;
    printf("sda=%2d scl=%2d :", sda, scl);
    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (i2c_master_probe(bus, a, 50) == ESP_OK) {
            printf(" 0x%02X", a);
            trovati++;
        }
    }
    printf("%s\n", trovati ? "" : " nessuno");
    i2c_del_master_bus(bus);
}

static int c_i2c(int argc, char **argv)
{
    if (argc >= 3) {
        scan_coppia(atoi(argv[1]), atoi(argv[2]));
        return 0;
    }

    /* Le coppie che si trovano sulle schede P4 in circolazione. La prima e
       quella che scheda_p4.h dichiara oggi. */
    static const int8_t COPPIE[][2] = {
        { 7, 8 }, { 8, 7 }, { 7, 6 }, { 6, 7 },
        { 20, 21 }, { 21, 20 }, { 22, 23 }, { 23, 22 },
        { 4, 5 }, { 5, 4 }, { 47, 48 }, { 48, 47 },
        { 12, 13 }, { 13, 12 }, { 1, 0 }, { 0, 1 },
    };
    printf("cerco un chip I2C. Il tocco, un GSL3680, risponde a 0x40.\n");
    for (unsigned n = 0; n < sizeof COPPIE / sizeof COPPIE[0]; n++)
        scan_coppia(COPPIE[n][0], COPPIE[n][1]);
    printf("fine. Con due argomenti si prova una coppia sola: i2c 7 8\n");
    return 0;
}

static bool libero(int p)
{
    if (p == 7 || p == 8) return false;            /* il bus che c'e gia   */
    if (p >= 14 && p <= 19) return false;          /* SDIO verso il C6     */
    if (p == 54) return false;                     /* reset del C6         */
    if (p == 37 || p == 38) return false;          /* console UART         */

    /* Qui c'era anche 24-33, escluso come "flash e PSRAM". Non lo e: sul P4
       quelle memorie hanno piedini dedicati, e il registro d'avvio lo dice
       — "psram CS IO is dedicated". La prova che era di troppo e che il
       firmware pilota gia il 26 per la retroilluminazione, e non e mai
       caduto: quel guardiano vietava proprio il piedino che si stava
       cercando. */
    return p >= 0 && p <= 53;
}

/* Riempie lo schermo di un colore saltando LVGL.
 *
 * Uno schermo nero ha troppe spiegazioni: tempi sbagliati, corsie
 * sbagliate, alimentazione del PHY, oppure semplicemente un'interfaccia che
 * disegna del nero. Questo comando taglia il dubbio in due — se lo schermo
 * diventa rosso la catena video funziona e il problema sta sopra; se resta
 * nero il problema sta sotto, e LVGL non c'entra.
 *
 * L'ha guadagnato: e cosi che si e scoperto che il nero non veniva
 * dall'interfaccia. */
static int c_schermo(int argc, char **argv)
{
    static const struct { const char *nome; uint16_t rgb565; } COLORI[] = {
        { "rosso",  0xF800 }, { "verde",  0x07E0 }, { "blu",    0x001F },
        { "bianco", 0xFFFF }, { "nero",   0x0000 },
    };
    const char *voluto = argc >= 2 ? argv[1] : "rosso";

    for (unsigned n = 0; n < sizeof COLORI / sizeof COLORI[0]; n++) {
        if (strcmp(voluto, COLORI[n].nome) != 0) continue;
        if (!hw_lcd_prova(COLORI[n].rgb565)) {
            printf("lo schermo non e avviato\n");
            return 1;
        }
        printf("%s per cinque secondi. Guarda il vetro.\n", COLORI[n].nome);
        fflush(stdout);

        /* Si riscrive di continuo perche' l'interfaccia ridisegna
           l'orologio una volta al secondo e si riprende il framebuffer:
           una scrittura sola lampeggerebbe e sparirebbe, e chi guarda
           direbbe di non aver visto niente. */
        for (int giro = 0; giro < 50; giro++) {
            hw_lcd_prova(COLORI[n].rgb565);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        printf("finito: l'interfaccia si riprende lo schermo.\n");
        return 0;
    }
    printf("colori: rosso verde blu bianco nero\n");
    return 1;
}

/* --- trovare la luce -----------------------------------------------------
 *
 * Il vetro potrebbe disegnare benissimo e restare buio perche' nessuno lo
 * illumina. E l'ipotesi che il sintomo sostiene: luce al reset, poi buio, e
 * il buio arriva proprio quando il codice chiama hw_retro(true) — che su un
 * piedino attivo-basso e la riga che spegne.
 *
 * Qui si accende il bianco e si cammina sui piedini liberi, provando i due
 * livelli. Chi guarda il vetro dice quale numero lo illumina: si scopre il
 * piedino e la polarita nello stesso giro.
 *
 * I piedini esclusi sono gli stessi della ricerca I2C, e per la stessa
 * ragione: 7 e 8 il bus, 14-19 e 54 il co-processore, 24-33 flash e PSRAM,
 * 37 e 38 la console. Toccarli non darebbe una risposta sbagliata, farebbe
 * cadere il pannello.
 */
static void tieni_bianco(void)
{
    hw_lcd_prova(0xFFFF);
}

static int c_luce(int argc, char **argv)
{
    if (argc >= 3) {
        const int pin = atoi(argv[1]);
        const int liv = atoi(argv[2]);
        if (!libero(pin)) {
            printf("il piedino %d e occupato da qualcosa che serve\n", pin);
            return 1;
        }
        const gpio_config_t c = {
            .pin_bit_mask = 1ULL << pin,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&c);
        gpio_set_level(pin, liv != 0);
        tieni_bianco();
        printf("piedino %d a %d, e bianco sullo schermo.\n", pin, liv != 0);
        return 0;
    }

    printf("Accendo il bianco e cammino sui piedini. Guarda il vetro e\n");
    printf("dimmi quale numero lo illumina.\n");
    fflush(stdout);

    for (int p = 0; p <= 53; p++) {
        if (!libero(p)) continue;

        const gpio_config_t c = {
            .pin_bit_mask = 1ULL << p,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&c);

        for (int liv = 0; liv <= 1; liv++) {
            printf("piedino %2d = %d\n", p, liv);
            fflush(stdout);
            gpio_set_level(p, liv);
            /* Il bianco si riscrive di continuo: l'interfaccia ridisegna
               l'orologio ogni secondo e si riprenderebbe il framebuffer. */
            for (int g = 0; g < 12; g++) {
                tieni_bianco();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        /* Si lascia il piedino com'era: in ingresso non pilota niente, e
           camminare avanti lasciando dietro venti uscite accese sarebbe un
           modo per trovare la luce e rompere altro. */
        const gpio_config_t spento = {
            .pin_bit_mask = 1ULL << p,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&spento);
    }
    printf("finito. Con 'luce PIEDINO LIVELLO' si ferma su una scelta.\n");
    return 0;
}

/* --- avvio -------------------------------------------------------------- */

static void registra(const char *nome, const char *aiuto,
                     esp_console_cmd_func_t f)
{
    const esp_console_cmd_t c = { .command = nome, .help = aiuto, .func = f };
    esp_console_cmd_register(&c);
}

bool console_avvia(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    cfg.prompt = "pannello>";
    cfg.max_cmdline_length = 1024;   /* i token JWT sono lunghi */

    /* La console si apre su **qualunque** sia la porta primaria, e non
       sempre su UART0.

       Le schede si collegano in modi diversi: c'e chi mette
       un ponte USB-seriale davanti a UART0 e chi porta fuori la USB nativa
       del chip. Aprendo sempre UART0, sulla seconda si otterrebbe il caso
       peggiore possibile — il registro **si vede**, quindi tutto sembra
       funzionare, ma quello che si digita non arriva a nessuno. Si
       perderebbero `stato`, `rete`, `chiave` e `token`, cioe il modo in cui
       un pannello nuovo viene configurato la prima volta e l'unica via di
       rientro quando la rete non c'e.

       Quale sia la primaria lo dice la configurazione, quindi la scelta si
       fa qui e non si chiede a chi compila. */
#if defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t porta =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    const esp_err_t e = esp_console_new_repl_usb_serial_jtag(&porta, &cfg, &repl);
    const char *dove = "USB nativa";
#else
    esp_console_dev_uart_config_t porta = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    const esp_err_t e = esp_console_new_repl_uart(&porta, &cfg, &repl);
    const char *dove = "UART0";
#endif
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "console non avviata su %s", dove);
        return false;
    }
    ESP_LOGI(TAG, "console su %s", dove);

    esp_console_register_help_command();
    registra("rete", "nome della rete Wi-Fi", c_rete);
    registra("chiave", "password della rete (fra virgolette se ha spazi)",
             c_chiave);
    registra("token", "token a lunga durata di Home Assistant", c_token);
    registra("ha", "indirizzo di Home Assistant", c_ha);
    registra("stato", "cosa e configurato e come va il collegamento", c_stato);
    registra("tocco", "gli ultimi tocchi arrivati all'interfaccia", c_tocco);
    registra("i2c", "cerca i chip sul bus: 'i2c' prova le coppie note, "
                    "'i2c SDA SCL' una sola", c_i2c);
    registra("schermo", "riempie lo schermo di un colore saltando LVGL: "
                        "'schermo rosso'", c_schermo);
    registra("luce", "cerca il piedino della retroilluminazione: 'luce' "
                     "cammina, 'luce PIEDINO LIVELLO' si ferma", c_luce);
    registra("dimentica", "cancella rete e password", c_dimentica);
    registra("azzera", "cancella la configurazione e riavvia (rete e token restano)",
             c_azzera);
    registra("riavvia", "riavvia il pannello", c_riavvia);

    if (esp_console_start_repl(repl) != ESP_OK) {
        ESP_LOGE(TAG, "console non partita");
        return false;
    }
    ESP_LOGI(TAG, "console pronta - scrivi help");
    return true;
}
