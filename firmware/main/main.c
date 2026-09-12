/* ------------------------------------------------------------------------
 * Il pannello, sull'apparecchio.
 *
 * Primo traguardo della Fase 5: l'interfaccia vera sul vetro. Schermo,
 * tocco, LVGL, dati dalla configurazione. **Niente rete**: se questa parte
 * funziona sono provati in un colpo solo lo schermo, la PSRAM, il tocco, e
 * soprattutto il fatto che il codice di `ui/` e davvero
 * portabile — che e l'unica cosa che quattro fasi di simulatore non hanno
 * mai potuto dimostrare.
 *
 * L'ordine dell'accensione non e casuale, e ogni passo dipende dal
 * precedente in un modo che si vede solo sbagliandolo:
 *
 *   1. NVS         perche senza, il Wi-Fi non salva niente e il primo
 *                  avvio non ha dove scrivere
 *   2. LittleFS    perche config.json decide **quante** zone, unita e
 *                  accessi esistono, e quindi cosa si disegna
 *   3. I2C         un filo solo, per il tocco
 *   4. reset       del tocco, prima di cercarlo sul bus
 *   5. schermo     il DSI e LVGL
 *   6. tocco       dopo lo schermo, perche si aggancia al suo display
 *   7. interfaccia
 *   8. retroilluminazione, **per ultima**: accenderla prima vorrebbe dire
 *      mostrare a chi guarda la memoria non ancora inizializzata
 * --------------------------------------------------------------------- */
#include <inttypes.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "aggiornamento.h"
#include "config.h"
#include "schermate.h"
#include "sezioni.h"
#include "sistema.h"
#include "tempi.h"
#include "sorveglianza.h"
#include "web.h"
#include "http.h"
#include "dati.h"
#include "hw_scheda.h"
#include "console.h"
#include "nel_grafico.h"
#include "ha.h"
#include "entita.h"
#include "impronta.h"
#include "orologio.h"
#include "hw_lcd.h"
#include "hw_tocco.h"
#include "porti.h"
#include "rete.h"
#include "profile.h"
#include "scheda.h"
#include "segreti.h"
#include "stati.h"
#include "ui.h"
#include "versione.h"

static const char *TAG = "pannello";

/* --- la scheda e quella che il profilo crede? ---------------------------
 *
 * 13-primo-avvio-p4.md chiede di controllare la flash prima di
 * dare per buona la tabella delle partizioni: le schede dichiarate da 16 MB
 * a volte ne hanno 8, e una tabella che sfora non da un errore — da un
 * pannello che si comporta in modo incomprensibile mesi dopo.
 *
 * Farlo qui invece che a mano con esptool ha un vantaggio: lo si scopre
 * ogni volta che il pannello si accende, non solo il giorno in cui
 * qualcuno si ricorda di controllare.
 */
/* --- il chip che fa da radio ---------------------------------------------
 *
 * Il P4 non ha Wi-Fi. Ce l'ha un ESP32-C6 attaccato via SDIO, con un
 * firmware suo — esp-hosted-mcu — che **non fa parte di questa
 * compilazione**: `idf.py flash` scrive bootloader, partizioni e
 * applicazione, e la partizione `c6_fw` resta come l'ha lasciata la
 * fabbrica.
 *
 * Senza quel firmware il pannello parte, disegna, e la rete non sale mai.
 * Il registro direbbe soltanto che il Wi-Fi non si connette, e si
 * passerebbe la serata a controllare password e SSID: e un guasto che
 * indica la parte sbagliata del problema. Quindi lo si dice qui, prima che
 * qualcuno cominci a cercare.
 *
 * Non si blocca l'avvio. Un pannello senza rete e ancora un pannello con
 * cui si parla dalla console, e con cui si arriva alla pagina di
 * configurazione appena la rete c'e. */
static void verifica_coprocessore(void)
{
    const esp_partition_t *c6 = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 0x40, "c6_fw");
    if (!c6) {
        ESP_LOGW(TAG, "nessuna partizione c6_fw: il firmware della radio "
                 "non ha dove stare");
        return;
    }

    /* --- si guarda l'intestazione, non solo se e cancellata -------------
     *
     * Prima qui bastava un byte diverso da 0xFF per dire «immagine
     * presente», e per un po' lo ha detto su una partizione piena di
     * spazzatura: la mappa di fabbrica metteva altro a quell'indirizzo, la
     * nostra ci ha messo sopra `c6_fw`, e cio che restava non era
     * cancellato. Un controllo che si accontenta di «non e vuoto» risponde
     * di si anche quando la risposta giusta e no, ed e peggio di nessun
     * controllo: manda a cercare altrove.
     *
     * Un'immagine ESP comincia con 0xE9 e dice a quale chip appartiene nel
     * byte dodici — 0x0D e l'ESP32-C6. Due byte, e la differenza fra sapere
     * e credere. */
    uint8_t testa[16] = {0};
    if (esp_partition_read(c6, 0, testa, sizeof testa) != ESP_OK) {
        ESP_LOGW(TAG, "c6_fw non leggibile");
        return;
    }

    if (testa[0] == 0xE9 && testa[12] == 0x0D) {
        ESP_LOGI(TAG, "c6_fw: immagine per esp32c6 (%u kB di partizione)",
                 (unsigned)(c6->size / 1024));
        return;
    }

    bool vuota = true;
    for (unsigned n = 0; n < sizeof testa; n++)
        if (testa[n] != 0xFF) { vuota = false; break; }

    ESP_LOGW(TAG, "c6_fw %s: il co-processore Wi-Fi non ha qui il suo "
             "firmware. Non e un guasto finche la radio sale — quello che "
             "gira sul C6 sta nella sua flash — ma questa partizione non "
             "serve a un aggiornamento. Vedi docs/13-primo-avvio-p4.md",
             vuota ? "e vuota" : "non contiene un'immagine ESP");
}

static void verifica_scheda(void)
{
    uint32_t flash_byte = 0;
    esp_flash_get_size(NULL, &flash_byte);
    const uint32_t flash_mb = flash_byte / (1024 * 1024);

    /* esp_psram_get_size() e non heap_caps_get_total_size(): la seconda dice
       quanta PSRAM e rimasta **nell'heap**, e a questo punto dell'avvio una
       parte se la sono gia presa le istruzioni e le costanti copiate li da
       CONFIG_SPIRAM_FETCH_INSTRUCTIONS. Misurando quella si legge 6 MB su
       un chip da 8 e si grida a un guasto che non c'e — ed e successo al
       primo avvio su ferro vero. */
    const uint32_t psram_mb = (uint32_t)(esp_psram_get_size() / (1024 * 1024));
    const uint32_t psram_libera_kb =
        (uint32_t)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);

    ESP_LOGI(TAG, "flash %" PRIu32 " MB, PSRAM %" PRIu32 " MB "
             "(%" PRIu32 " kB liberi), heap interno %" PRIu32 " kB",
             flash_mb, psram_mb, psram_libera_kb,
             (uint32_t)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));

    /* Non si blocca l'avvio: un pannello che non parte non puo nemmeno
       dire perche. Si urla nel registro, che e raggiungibile da
       /api/log anche a configurazione chiusa. */
    /* I numeri vengono dal **profilo**, non da qui: un controllo che porta
       con se una misura scritta a mano passa in silenzio il giorno che la
       scheda cambia, ed e il genere di verifica che da fiducia senza darne
       motivo. Le cifre stanno in profili.json sotto `memoria`, e sono
       quelle con cui la scheda si dichiara. */
    if (flash_mb < PRF->memoria.flash_mb)
        ESP_LOGE(TAG, "questa scheda ha %" PRIu32 " MB di flash e il profilo "
                 "%s ne dichiara %u: le partizioni alte non esistono",
                 flash_mb, PRF->chiave, PRF->memoria.flash_mb);
    if (psram_mb < PRF->memoria.psram_mb)
        ESP_LOGE(TAG, "questa scheda ha %" PRIu32 " MB di PSRAM e il profilo "
                 "%s ne dichiara %u: i framebuffer potrebbero non entrarci",
                 psram_mb, PRF->chiave, PRF->memoria.psram_mb);

    /* Quello che serve davvero e che ci **stiano**, non che il chip sia
       grande: due framebuffer a fotogramma pieno. Se la PSRAM
       libera non basta, lo schermo fallira l'avvio fra due righe e questo
       messaggio spiega perche. */
    const uint32_t servono_kb = (uint32_t)
        ((size_t)PRF->schermo.larghezza * PRF->schermo.altezza * 2 * 2 / 1024);
    if (psram_libera_kb < servono_kb)
        ESP_LOGE(TAG, "PSRAM libera %" PRIu32 " kB, ai framebuffer ne "
                 "servono %" PRIu32, psram_libera_kb, servono_kb);

    verifica_coprocessore();
}

/* --- il compito che disegna ---------------------------------------------
 *
 * **Solo questo tocca LVGL**, e vale gia adesso che e l'unico compito che
 * esista: le fasi successive aggiungono la rete, e quella parlera per
 * code. Scrivere la regola quando c'e un compito solo costa
 * niente; scriverla dopo vuol dire cercare chi la viola.
 */
/* La pagina di configurazione ha salvato qualcosa. Non basta ricaricare i
   valori: cambiare config.json puo cambiare **quante** zone, unita e
   accessi esistono, cioe la struttura di quello che si disegna. Per questo
   si azzerano le sezioni e si rifa la schermata da capo — qui il ridisegno
   completo e giusto, perche e cambiato davvero tutto.

   ha_riprova() e l'unico modo di uscire da "token rifiutato", che apposta
   non si sblocca da solo: un token nuovo e un gesto di qualcuno, e questo
   lo e. */
/* --- rifare l'interfaccia, ma non da qui -------------------------------
 *
 * Questa la chiama il salvataggio della configurazione, che arriva da dentro
 * http_gira(). Rifare l'interfaccia vuol dire **distruggerla**: lv_obj_clean()
 * cancella l'albero intero, e cancellarlo mentre LVGL ha ancora il dito
 * puntato su quegli oggetti finisce come e finito davvero —
 *
 *     Guru Meditation Error: Core 1 panic'ed (Load access fault)
 *       lv_obj_get_first_not_deleting_child
 *       obj_indev_reset
 *       obj_delete_core
 *
 * — cioe camminando su una lista di figli che non c'e piu. Succedeva a
 * salvataggio, non sempre, e dal muro si vedeva come un pannello che si
 * pianta: il watchdog non mordeva, quindi restava appeso invece di
 * riavviarsi, e l'unica via d'uscita era staccare la corrente.
 *
 * Adesso si segna e basta. La ricostruzione la fa il ciclo principale
 * all'inizio del giro dopo, quando lv_timer_handler() e gia tornata e
 * nessun evento e in volo. Costa un giro di ritardo, che nessuno vede, e
 * toglie di mezzo un'intera categoria di guasti. */
static volatile bool rifare_interfaccia;
/* A translation saved from the page: the same redraw, without reconnecting
   Home Assistant or reloading the data — neither changed. */
static volatile bool rifare_testi;

static void configurazione_cambiata(void) { rifare_interfaccia = true; }
static void testi_cambiati(void) { rifare_testi = true; }

static void rifai_interfaccia_ora(void)
{
    if (!rifare_interfaccia && !rifare_testi) return;
    const bool tutto = rifare_interfaccia;
    rifare_interfaccia = false;
    rifare_testi = false;

    if (tutto) {
        ha_riprova();
        dati_ricarica();
        /* The time zone was read once, when the first address arrived:
           changing it from the page said "saved and applied" and the clock
           stayed on the old zone until a restart. */
        orologio_fuso_da_configurazione();
        /* The same for the NTP servers and the Wi-Fi, which however does
           not apply at once: it is tried, with a safety net. See rete.c.
           Inside this branch: a saved translation changes neither. */
        rete_ntp_rileggi();
        rete_ricontrolla(false);
    }
    sezioni_azzera();

    /* Non basta rifare il contenuto: le sezioni decidono cosa c'e nel rail,
       e il rail si costruisce all'accensione. ui_avvia() rifa tutto, ed e
       richiamabile apposta. */
    ui_avvia();

    /* E la vista di riposo, che vive sul livello superiore e a cui
       ui_avvia() non arriva: senza questa, cambiare la grandezza dei
       caratteri dello standby non si vedeva finche qualcuno non toccava il
       pannello e riaspettava due minuti. */
    tempi_rifai_standby();
}

TaskHandle_t compito_lvgl;

/* --- da dove si e fermato ----------------------------------------------
 *
 * Un avvio che si pianta lascia un registro che finisce e basta, e da un
 * registro che finisce non si capisce se l'ultima riga e stata scritta un
 * millisecondo o venti secondi prima del blocco. Questo lo dice: ogni passo
 * stampa quanto e costato, e il passo che manca e quello dentro cui si e
 * fermato.
 *
 * Costa cinque righe e le tengo: la prossima volta che una configurazione
 * fara qualcosa di strano, questa e la differenza fra guardare e indovinare. */
static uint32_t passo_ms;

static void passo(const char *cosa)
{
    const uint32_t ora = (uint32_t)(esp_timer_get_time() / 1000);
    ESP_LOGI(TAG, "avvio: %s (%" PRIu32 " ms)", cosa, ora - passo_ms);
    passo_ms = ora;
}

/* --- il pannello e gia stato configurato, oppure no? --------------------
 *
 * Vero quando manca qualcosa senza cui il pannello non puo arrivare a Home
 * Assistant.
 *
 * Non basta guardare com'e andata cfg_carica(): un config.json puo esserci
 * benissimo — scritto a mano, copiato dall'altro pannello, messo nella sua
 * partizione insieme al firmware — e non contenere **niente** di cio che
 * serve, perche' la password del Wi-Fi e il token di Home Assistant in quel
 * file non ci vanno per scelta: stanno in NVS. Un pannello con dentro la
 * configurazione della casa e senza segreti e esattamente il caso visto sul
 * banco.
 *
 * La password del Wi-Fi non e fra le condizioni: una rete aperta e legittima
 * e non ha password da chiedere. */
static bool da_configurare(void)
{
    const char *ssid = cfg_testo("system/network/ssid", "");
    if (!ssid || !*ssid) return true;

    const char *host = cfg_testo("home_assistant/host", "");
    if (!host || !*host) return true;

    /* Il token sta in NVS e non nel documento, quindi va chiesto a chi lo
       tiene. E l'unico dei tre che un config.json completo non puo avere. */
    return !segreti_impostato(SEG_HA_TOKEN);
}

static void compito_grafico(void *arg)
{
    (void)arg;

    passo("compito grafico partito");
    ui_avvia();

    /* --- e se il pannello non fosse ancora stato configurato? ------------
     *
     * La schermata del primo avvio esisteva da mesi, completa e collaudata,
     * e sul ferro non si e mai vista: nessuno la chiamava. Il pannello
     * mostrava la home di una casa che non poteva raggiungere, e non c'era
     * **nessuna strada** per dirgli la rete — perche' la pagina di
     * configurazione arriva via Wi-Fi, e il Wi-Fi e proprio quello che
     * manca.
     *
     * La decisione sta qui e non dentro ui_avvia() perche' e una politica
     * del dispositivo, non della vista: sul simulatore quella schermata si
     * chiede con --vista primo-avvio, e aprirla da sola ogni volta
     * impedirebbe di guardare le altre.
     *
     * Dopo ui_avvia() e non al suo posto: la schermata vive su
     * lv_layer_top(), quindi si appoggia sopra a un pannello gia costruito.
     * Quando finisce si toglie e sotto c'e la casa, gia in piedi. */
    if (da_configurare()) stato_primo_avvio(true, 0);
    else                  stato_avvio(true);

    passo("interfaccia costruita");

    /* Il primo fotogramma prima della luce: accendere la retroilluminazione
       su un framebuffer non ancora disegnato mostra a chi guarda quello che
       c'era in memoria. Un lampo bianco all'accensione e la firma di questo
       errore, e si evita con due righe nell'ordine giusto. */
    lv_refr_now(NULL);
    hw_retro(true);
    ESP_LOGI(TAG, "interfaccia in piedi");

    /* --- perche Home Assistant gira **qui** e non in un task suo ---------
     *
     * 05-architettura-firmware.md §2 prevede un ha_ws_task sul core 0 che
     * parla per code. Il motivo di quella scelta e la regola che lo
     * accompagna — solo il compito grafico tocca LVGL — e quella regola
     * resta vera. Ma ha_gira() non tocca LVGL: aggiorna il magazzino delle
     * entita, che il compito grafico **legge** a ogni ridisegno. Due task,
     * uno che scrive e uno che legge, vorrebbero un lucchetto; e un
     * lucchetto preso a ogni lettura di entita, cioe a ogni riquadro
     * disegnato, costa piu del lavoro che protegge ed e un posto in piu
     * dove restare bloccati.
     *
     * Tenendoli nello stesso ciclo c'e un lettore per volta e non serve
     * niente. E la stessa scelta del simulatore, dove gira da quattro fasi:
     * qui e il codice provato, li era il ripiego.
     *
     * Il prezzo, e va detto: il trasporto e a socket non bloccanti, quindi
     * un giro costa poco — tranne all'aggancio, quando arriva la fotografia
     * di tutte le entita e va analizzata in un colpo. Quella e una pausa
     * sola, all'inizio. Se si facesse sentire, la cura non e spostare tutto
     * in un altro task ma spezzare quella singola analisi; e da misurare
     * sul pannello vero, non da decidere adesso.
     */
    /* --- il giro di prova di un firmware appena installato ---------------
     *
     * Un'immagine nuova parte **in prova**. Riavviare non basta a dire che
     * va: puo partire benissimo e non riuscire a fare la cosa per cui
     * esiste — niente rete, token che non passa, schermo che non disegna. Un
     * pannello a muro in quello stato e peggio di un pannello alla versione
     * di prima, perche sembra acceso.
     *
     * Percio si conferma solo dopo aver visto **tutte e tre** le cose che il
     * pannello deve saper fare: la rete su, Home Assistant che risponde con
     * la casa, e un fotogramma disegnato. Sono le stesse tre che il primo
     * avvio mostra come controlli, e non e un caso: sono la definizione di
     * "funziona" per questo apparecchio.
     *
     * Se entro il tempo di grazia non si e visto tutto, si riavvia e il
     * bootloader rimette quella di prima. Nessuno deve accorgersene e
     * nessuno deve intervenire — ed e tutto il punto di avere due
     * partizioni.
     *
     * Centoventi secondi, 05-architettura-firmware.md §7: c'e da dare
     * respiro al co-processore che deve tirare su la radio. */
    const uint32_t GRAZIA_MS = 120000;
    const bool in_prova = agg_in_prova();
    const uint32_t prova_scade = lv_tick_get() + GRAZIA_MS;
    bool prova_chiusa = !in_prova;

    if (in_prova)
        ESP_LOGW(TAG, "versione %s in prova: ha %u secondi per dimostrarsi "
                 "buona, poi si torna indietro",
                 agg_versione(), (unsigned)(GRAZIA_MS / 1000));

    bool ha_partito = false;
    uint32_t ultimo_controllo = 0;
    uint32_t ultima_generazione = 0;
    uint32_t ultima_impronta = 0;
    uint32_t ultimo_orologio = 0;
    int      ultimo_minuto = -1;
    uint32_t ultimo_storico = 0;

    for (;;) {
        /* Prima di tutto, e fuori da qualunque elaborazione di LVGL: se la
           configurazione e cambiata, qui si rifa l'interfaccia. Vedi
           rifai_interfaccia_ora() per il perche non si fa dove la si
           chiede. */
        rifai_interfaccia_ora();

        /* And, at the same point and for the same reason, what the console
           asked for: configuration, secrets and Home Assistant are touched
           only from here. See nel_grafico.h. */
        nel_grafico_gira();

        const uint32_t attesa = lv_timer_handler();
        const uint32_t adesso = lv_tick_get();

        /* The network time, and the trial of a new Wi-Fi network. If the
           trial failed, the configuration went back as it was and is read
           again as after a save. */
        if (rete_gira(adesso)) configurazione_cambiata();

        /* Non si prova a parlare con Home Assistant prima di avere un
           indirizzo: un tentativo che fallira di sicuro riempie il registro
           e non insegna niente. */
        if (!ha_partito && rete_stato() == RETE_CONNESSA) {
            ha_partito = true;
            passo("rete pronta");
            ha_avvia();
            passo("Home Assistant avviato");

            /* Il server ascolta subito, ma /api/status e /api/log sono le
               uniche cose che risponde: tutto il resto e 404 finche non si
               tocca l'interruttore in Impostazioni, sul vetro. E la regola
               di 03-config-contratto.md §6, ed e quello che rende sicuro
               tenere aperta una pagina di configurazione senza password su
               una rete di casa — per cambiare qualcosa bisogna essere
               davanti al pannello. */
            web_quando_cambia(configurazione_cambiata);
            web_when_texts_change(testi_cambiati);
            if (web_avvia(80))
                /* Aperta o chiusa lo dice qui, invece di dire sempre
                   «chiusa»: con la deroga del montaggio attiva, questa riga
                   contraddiceva l'avviso stampato una riga sopra, e due
                   messaggi che si smentiscono valgono meno di nessuno. */
                ESP_LOGI(TAG, "configurazione su http://%s/ - %s",
                         rete_indirizzo(),
                         web_sempre_aperta()
                             ? "sempre aperta (deroga di montaggio)"
                             : "chiusa: sbloccala da Impostazioni");
            else
                ESP_LOGE(TAG, "porta 80 occupata: niente configurazione");
        }
        if (ha_partito) {
            ha_gira(adesso);
            dati_gira(adesso);   /* i rilasci degli impulsi scadono qui */
            /* --- la luce segue lo stato ---------------------------
             *
             * In tempi.c c'era scritto «sul pannello qui si spegne la
             * retroilluminazione», e sul pannello non la spegneva nessuno:
             * ST_SPENTO rendeva trasparente il contenuto e lasciava la
             * lampada accesa dietro un vetro nero. Lo stesso valeva per la
             * luminosita ridotta dello standby, che esisteva solo in
             * configurazione.
             *
             * Si scrive solo quando lo stato cambia: il PWM non ha bisogno
             * di sentirsi ripetere lo stesso valore trenta volte al
             * secondo. */
            static stato_t luce_per = (stato_t)-1;
            const stato_t adesso_st = tempi_stato();
            if (adesso_st != luce_per) {
                luce_per = adesso_st;
                switch (adesso_st) {
                case ST_SPENTO:
                    hw_retro(false);
                    break;
                case ST_STANDBY:
                    hw_retro_percento((uint8_t)cfg_intero(
                        "display/brightness_standby", 40));
                    break;
                default:
                    hw_retro_percento((uint8_t)cfg_intero(
                        "display/brightness_active", 100));
                    break;
                }
            }

            /* La sorveglianza dice **da quanto** un riquadro non si
               aggiorna, e a che punto smettere di far finta che vada bene:
               silenzio sotto i quindici secondi, avviso fino al minuto,
               schermo intero oltre. Su un pannello veloce e una rete di
               sicurezza; su uno lento e la differenza fra un'immagine
               vecchia e un'immagine vecchia **che lo dice**. */
            sorveglianza_gira(adesso);

            web_tempo(adesso);
            http_gira();

            /* Dopo aver servito, non durante: la risposta e gia uscita e
               chi l'ha chiesta ha visto la conferma. Riavviare dentro la
               richiesta vorrebbe dire spegnersi con la risposta ancora nel
               buffer, e dall'altra parte si vedrebbe un errore di rete
               invece di un pannello che riparte. */
            /* Il giro di prova si chiude qui, dentro il ramo che gira solo
               quando la rete c'e: prima di quel momento non c'e niente da
               verificare. */
            if (!prova_chiusa) {
                const bool rete_su = sistema_rete().connessa;
                const bool casa_su = ha_stato() == HA_PRONTO;
                const bool schermo = sistema_stato().fps > 0 ||
                                     lv_tick_get() > 5000;

                /* Il ritorno indietro e l'unica parte dell'aggiornamento
                   che non si prova sul PC: e il bootloader a farlo, e il
                   bootloader gira solo sul silicio. Per vederlo davvero
                   serve un'immagine che non chiuda il giro di prova, e
                   costruirsela rompendo il firmware a mano e un modo per
                   provare tutt'altro guasto.
                   Questa e la maniglia: si compila una volta con

                       idf.py -B build-collaudo -D SDKCONFIG=sdkconfig.collaudo                               -D PANNELLO_ROLLBACK_FINTO=1 build

                   la si installa dalla pagina, e si guarda il pannello
                   tornare da solo alla versione di prima allo scadere della
                   grazia. Fuori da quel comando non esiste.

                   **In una cartella a parte**, e non e un dettaglio: CMake
                   si ricorda le -D nella cache della cartella, quindi
                   costruirla in build/ vorrebbe dire che anche la
                   compilazione "pulita" del giorno dopo continua a non
                   confermarsi. Un firmware guasto che si crede buono, e
                   nessun motivo per sospettarlo. */
            #ifdef PANNELLO_ROLLBACK_FINTO
                (void)rete_su; (void)casa_su; (void)schermo;
                if (false)
            #else
                if (rete_su && casa_su && schermo)
            #endif
                {
                    agg_conferma();
                    prova_chiusa = true;
                } else if ((int32_t)(lv_tick_get() - prova_scade) > 0) {
                    /* Non torna: il bootloader rimette la partizione di
                       prima e riavvia. */
                    ESP_LOGE(TAG, "versione in prova non convince: "
                             "rete %s, casa %s", rete_su ? "si" : "no",
                             casa_su ? "si" : "no");
                    agg_rifiuta();
                }
            }

            /* --- la curva della giornata si rinfresca ------------------
             *
             * Un quarto d'ora. Non e polling di stato — quello il pannello
             * non lo fa, e per gli stati c'e la sottoscrizione — ma di
             * **storico**, che un evento non lo ha: nessuno manda un
             * `state_changed` per dire «la media delle undici adesso e
             * completa». Fra un rinfresco e l'altro l'ultima casella resta
             * indietro di qualche minuto, che su una curva di ventiquattro
             * ore non si vede. */
            if (adesso - ultimo_storico > 15u * 60u * 1000u) {
                ultimo_storico = adesso;
                ha_storico_scade();
            }

            if (web_riavvio_chiesto()) {
                ESP_LOGI(TAG, "riavvio chiesto dalla pagina di configurazione");
                sistema_riavvia();
            }
        }

        /* --- quando si rifa la schermata ------------------------------
         *
         * ui_vai_a() non aggiorna: **butta via il contenuto e lo
         * ricostruisce**. E l'operazione piu cara che l'interfaccia sappia
         * fare, e la prima versione la ripeteva una volta al secondo per
         * sempre, che ci fossero dati nuovi o no. Sul PC non si nota; qui
         * si vedeva come uno schermo che si ridisegna e sfarfalla **da
         * fermo**, e nessun pixel clock lo faceva smettere — perche non era
         * banda, era lavoro inutile.
         *
         * Adesso la condizione e doppia, e le due meta dicono cose diverse:
         * la generazione dice **se** e cambiato qualcosa fra le entita che
         * questa casa segue, l'orologio dice **non piu spesso di cosi**. La
         * prima toglie i ridisegni a vuoto, la seconda evita che una raffica
         * di eventi ne provochi venti di fila. */
        /* Un secondo e piu che abbastanza per un orologio ai minuti, e
           costa due confronti di stringa quando il minuto non e cambiato. */
        if (adesso - ultimo_orologio > 1000) {
            ultimo_orologio = adesso;
            ui_orologi_aggiorna();
        }

        /* --- e quando non e cambiato niente, ma e passato un minuto ----
         *
         * La generazione dice se e cambiato qualcosa **fra le entita**, e
         * per quasi tutto basta. Non basta per i valori che il pannello
         * **calcola dall'ora**: il conto alla rovescia di un timer scende
         * anche se Home Assistant non manda niente — e mentre un timer
         * corre non manda proprio niente, perche il suo stato resta
         * "active" e i suoi attributi non si muovono.
         *
         * Senza questa riga il conto alla rovescia restava fermo al valore
         * che aveva quando il timer e partito, e si sbloccava per caso al
         * primo evento di un'altra entita. Un ricarico al minuto costa
         * qualche microsecondo e nessun pixel: il secondo filtro,
         * l'impronta, decide comunque se c'e qualcosa di nuovo da
         * disegnare. */
        const int minuto = orologio_minuti();
        const bool minuto_nuovo = minuto >= 0 && minuto != ultimo_minuto;

        const uint32_t gen = ent_generazione();
        /* Non si chiede piu se il collegamento e pronto: la generazione lo
           sa gia. Cade il collegamento, i valori si dimenticano, la
           generazione avanza, e lo schermo si rifa una volta per mostrare
           che non sa piu niente — invece di restare con la fotografia di
           prima spacciandola per attuale. */
        if ((gen != ultima_generazione || minuto_nuovo)
            && adesso - ultimo_controllo > 1000) {
            /* Il tempo si segna qui, prima di guardare se e cambiato
               qualcosa: e cosi che una strozzatura strozza. Segnandolo solo
               nel ramo del ridisegno, quando **non** cambiava niente la
               condizione restava vera e si rientrava al giro dopo. */
            ultimo_controllo = adesso;
            ultima_generazione = gen;
            ultimo_minuto = minuto;
            dati_ricarica();

            /* --- il secondo filtro, ed e quello che si vede -------------
             *
             * La generazione dice che e cambiato **qualcosa**; l'impronta
             * dice che e cambiato qualcosa che *questa* sezione mostra.
             * Senza, un contatore di energia che si aggiorna ogni secondo
             * faceva ricostruire anche la pagina delle Luci — e un
             * ridisegno pieno, qui, non e gratis: la periferica RGB resta a
             * secco di pixel, e sul vetro si vede un lampo nero e
             * un'immagine che scivola di lato.
             *
             * In mezzo sta dati_ricarica(), che costa CPU e nessun pixel: e
             * lei a portare i valori nuovi dentro le schede, e va fatta
             * prima di poterle confrontare. */
            const uint32_t imp = impronta_sezione((int)ui_dove());
            if (imp != ultima_impronta) {
                ultima_impronta = imp;
                ui_vai_a(ui_dove(), ui_vista());
            }
        }

        /* --- quanto si puo dormire -------------------------------------
         *
         * lv_timer_handler() dice fra quanto ha da fare **lei**. Ma in
         * questo ciclo non c'e solo lei: ci sono Home Assistant e il server di
         * configurazione, e nessuno dei due ha voce in capitolo su quel
         * numero.
         *
         * Su una schermata ferma LVGL puo dire tranquillamente mezzo
         * secondo, e per mezzo secondo nessuno leggeva il WebSocket. I
         * messaggi non si perdono — se li tiene lwIP — ma si accumulano, e
         * arrivano tutti insieme al risveglio: un tocco su una luce che ci
         * mette mezzo secondo a vedersi, la pagina di configurazione lenta a
         * rispondere senza che il pannello stia facendo niente.
         *
         * Il tetto e lo stesso trenta che gia si usava quando LVGL non ha
         * niente in programma: quel numero diceva gia qual e il passo
         * giusto di questo ciclo, solo che valeva in un caso su due.
         * Trentatre risvegli al secondo su un compito che per lo piu
         * ricontrolla dei socket non si misurano nei fotogrammi. */
        uint32_t dormi = (attesa == LV_NO_TIMER_READY) ? 30 : attesa;
        if (dormi > 30) dormi = 30;
        if (dormi < 5)  dormi = 5;
        vTaskDelay(pdMS_TO_TICKS(dormi));
    }
}

/* --- il JSON vive in PSRAM ----------------------------------------------
 *
 * cJSON chiama malloc() per ogni nodo, e un nodo sono una sessantina di
 * byte. Su ESP-IDF con la PSRAM accesa, malloc() manda in PSRAM solo le
 * richieste **sopra** SPIRAM_MALLOC_ALWAYSINTERNAL, che vale 16 kB: sotto
 * quella soglia si serve dalla RAM interna. Un nodo di sessanta byte ci
 * finisce sempre.
 *
 * Il conto e presto fatto. Il buffer del messaggio WebSocket sta gia in
 * PSRAM perche e da 256 kB, sopra soglia; ma appena arriva, ha.c lo analizza
 * intero — un `get_states` di questa casa e arrivato a 712 kB — e ne fa
 * **migliaia** di nodi da sessanta byte, tutti sotto soglia, tutti interni.
 * Su un chip che di RAM interna ne ha poco piu di duecento kilobyte, e la
 * fine: il minimo storico misurato sul pannello era zero.
 *
 * Con questi due ganci l'intero mondo JSON — l'albero della configurazione,
 * che resta residente, e ogni messaggio di Home Assistant, che va e viene —
 * si sposta dove lo spazio c'e: 2775 kB liberi contro venti.
 *
 * Si paga in velocita: la PSRAM sta su un bus SPI, e su quel bus legge anche
 * lo schermo. Analizzare un messaggio grosso costa di piu e disturba il
 * quadro. Ma il JSON non si analizza a ogni fotogramma, mentre restare senza
 * RAM interna blocca il pannello — ed e cio che stava succedendo.
 *
 * Il ripiego alla RAM interna resta per quando la PSRAM fosse esaurita:
 * meglio lento che fermo. Se cambiasse qualcosa di grosso, questa e da
 * rimisurare con `stato`. */
static void *json_alloca(size_t n)
{
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    return p ? p : malloc(n);
}

static void json_in_psram(void)
{
    cJSON_Hooks ganci = { .malloc_fn = json_alloca, .free_fn = free };
    cJSON_InitHooks(&ganci);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Foyer Panel %s - profile %s (%ux%u)", PANNELLO_VERSIONE,
             PRF->chiave, PRF->schermo.larghezza, PRF->schermo.altezza);
    verifica_scheda();

    /* Prima di qualunque cosa che analizzi JSON: la configurazione si legge
       fra poche righe, e da quel momento l'albero resta in memoria per
       sempre. Cambiare i ganci a cose gia allocate le farebbe liberare con
       la funzione sbagliata. */
    json_in_psram();

    /* segreti_avvia() e non magazzino_avvia(): il magazzino e il porto, i
       segreti sono la politica, e da qui sopra si parla con la politica. */
    if (!segreti_avvia())
        ESP_LOGE(TAG, "senza NVS i segreti non si conservano");

    if (!archivio_monta())
        ESP_LOGE(TAG, "senza storage si parte dalla configurazione "
                 "predefinita");

    /* La configurazione decide **cosa** si disegna: quante zone, quali
       sezioni, quanti accessi. Va letta prima di costruire qualunque cosa,
       non dopo. */
    /* Prima di tutto il resto: e qui che si conta il riavvio e si legge
       perche il chip si e riacceso. Chiedendolo piu tardi si conterebbe una
       volta per ogni domanda. */
    /* Per primo: cosi anche cio che va storto qui sotto finisce nel
       registro che si andra a leggere. */
    registro_aggancia();

    sistema_avvia();

    passo("filesystem montato");
    switch (cfg_carica()) {
    case CFG_LETTA:        ESP_LOGI(TAG, "configurazione letta"); break;
    case CFG_DA_COPIA:     ESP_LOGW(TAG, "config.json non andava: copia"); break;
    case CFG_PREDEFINITA:  ESP_LOGW(TAG, "nessuna configurazione: primo avvio"); break;
    case CFG_TROPPO_NUOVA: ESP_LOGE(TAG, "schema piu nuovo del firmware"); break;
    }

    passo("configurazione letta");

    /* The time zone at once, before anyone asks the time: it is a setenv(),
       and doing it here — before the graphics task, which reads the time
       all the time, exists — means doing it while nobody reads it yet. It
       used to happen when the first address arrived, from the radio's event
       task, that is while the clock was being drawn. */
    if (!orologio_fuso_da_configurazione())
        ESP_LOGW(TAG, "time zone not recognised: the time will be UTC");

    i2c_master_bus_handle_t bus = NULL;
    if (!hw_i2c_avvia(&bus)) return;

    /* La luce resta spenta finche non c'e qualcosa da mostrare. */
    hw_retro(false);
    hw_reset_tocco();

    if (!hw_lcd_avvia()) {
        ESP_LOGE(TAG, "schermo non avviato: non c'e niente da fare qui");
        return;
    }
    passo("schermo acceso");
    if (!hw_tocco_avvia(bus))
        ESP_LOGW(TAG, "senza tocco il pannello si vede ma non si comanda");

    /* Il compito grafico su un core suo. La pila e generosa perche LVGL
       costruisce le schermate ricorsivamente, e una pila stretta si
       manifesta come un riavvio senza spiegazione — che e il modo peggiore
       in cui un difetto possa presentarsi. */
    /* Ventimila e non dodicimila. Quando questo compito disegnava soltanto,
       dodici kilobyte bastavano; adesso nello stesso ciclo girano il
       protocollo di Home Assistant, la decodifica dei fotogrammi e il server
       di configurazione, e ognuno si porta dietro la propria profondita di
       chiamate. Una pila stretta non da un errore: da un riavvio senza
       spiegazione, ed e il difetto piu costoso da inseguire.

       Il numero pero resta un'ipotesi finche non si misura: `stato` sulla
       console dice quanta ne e rimasta libera nel momento peggiore visto
       finora, e quella e la risposta. */
    nel_grafico_avvia();
    xTaskCreatePinnedToCore(compito_grafico, "lvgl", 20480, NULL, 4,
                            &compito_lvgl, 1);

    /* Rete e console **dopo** l'interfaccia, e non e un dettaglio d'ordine:
       il vetro deve accendersi anche se la rete non c'e, e chi ha appena
       appeso il pannello al muro deve vedere qualcosa mentre la radio cerca
       l'access point. Un avvio che aspetta la rete e un avvio che, senza
       rete, non finisce mai.

       La console per ultima di tutto: se qualcosa la sopra e andato storto,
       il registro l'ha gia detto e il prompt arriva sopra a un quadro
       completo invece che in mezzo. */
    passo("radio in avvio");
    if (!rete_avvia())
        ESP_LOGE(TAG, "radio non avviata: resta il pannello, senza dati");

    passo("radio avviata");
    console_avvia();
}
