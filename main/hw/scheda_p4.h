/* ------------------------------------------------------------------------
 * Scheda ESP32-P4 — piedini e tempi del pannello da 10,1".
 *
 * **Leggere questo prima di fidarsi dei numeri.**
 *
 * Questi valori non sono stati tutti trascritti da uno schema, e non si fa
 * finta: la scheda non era davanti a chi scriveva, e i primi numeri sono
 * stati **ricostruiti** dalla
 * documentazione della famiglia — ESP32-P4 con 32 MB di flash e 32 MB di
 * PSRAM, ESP32-C6 per la radio, vetro da 10,1" a 1280x800 su MIPI-DSI, che
 * e cio che i profili di questo progetto dichiarano.
 *
 * Ognuno porta scritto quanto vale. Quelli marcati **[silicio]** vengono dal
 * manuale del chip e non cambiano con la scheda. Quelli marcati
 * **[misurato]** li ha detti la scheda vera al primo avvio, e non sono piu
 * ipotesi. Quelli marcati **[da confermare]** sono ancora ricostruzione.
 *
 * Il primo avvio, il 28 agosto 2026, ha confermato: 16 MB di flash (non 32,
 * come qui si diceva), 32 MB di PSRAM a 200 MHz, chip revisione v1.3, vetro
 * JD9365 a 1280x800 su due corsie, bus I2C su 7/8, ESP32-C6 raggiunto via
 * SDIO sui piedini predefiniti dello slot 1. Ha smentito una cosa sola: il
 * tocco.
 *
 * Un piedino sbagliato qui non da un'interfaccia storta: da uno schermo nero
 * o un tocco muto. Per questo l'avvio non tira a indovinare — verifica_scheda()
 * in main.c dice **cosa** non ha trovato e dove, invece di lasciare il
 * pannello spento senza spiegazione.
 * --------------------------------------------------------------------- */
#ifndef SCHEDA_P4_H
#define SCHEDA_P4_H

/* --- lo schermo: MIPI-DSI ------------------------------------------------
 *
 * Il segnale e differenziale su due coppie, i tempi li tiene il
 * controllore del pannello, e non c'e niente da tarare. In cambio c'e una
 * sequenza di accensione da decine di registri che nessuno deduce: la porta
 * il componente del vetro (esp_lcd_jd9365).
 *
 * I piedini DSI **non si dichiarano**: sul P4 sono fissi nel silicio.
 */

/* [misurato] Il vetro ha risposto: `jd9365: LCD ID: 93 65 04` al primo
   avvio, e il DSI e partito a 1280x800 su due corsie. Era un'ipotesi — si
   diceva che l'altro comune fosse l'EK79007 — ed era quella giusta. */
#define HW_LCD_PANNELLO_JD9365   1

/* --- la variante del vetro, e come si riconosce -------------------------
 *
 * Questa scheda esiste in due versioni, "Old Panel" e "New Panel", con
 * sequenze di accensione e tempi **diversi**. Si distinguono
 * dall'etichetta dietro il guscio: con `V2` dopo il codice e la nuova,
 * senza e la vecchia. Le istruzioni di programmazione del costruttore lo
 * dicono in una riga, e quella riga vale una serata: con i numeri della
 * nuova il vetro risponde all'identificativo e resta **nero**.
 *
 * La nostra e la **vecchia**, e lo ha detto il ferro: il firmware di
 * fabbrica New Panel dava schermo nero, quello Old Panel lo accende.
 *
 * Due corsie a 832 Mbit/s, e questo numero **non** viene dal BSP.
 *
 * Il BSP della variante Old Panel ne dichiara 1500. Con quelli il vetro si
 * accende e mostra le aree piene **striate**, compreso un bianco pieno
 * disegnato saltando l'interfaccia — cioe un difetto che sta sotto a
 * tutto, nel collegamento.
 *
 * Il datasheet di questo pannello, in 4-Driver_IC_Data_Sheet, dice
 * `PLL_CLOCK=416`: per il MIPI sono 832 Mbit/s per corsia, meta di
 * quello che il BSP chiede. E il numero del costruttore del **vetro**,
 * non della scheda, e fra i due vince chi ha fatto il vetro.
 *
 * Che 1500 fosse troppo lo diceva anche il conto: a sessanta megahertz di
 * pixel clock e sedici bit per pixel servono 480 Mbit/s per corsia. 832
 * lascia respiro, 1500 e spinta oltre la taratura per niente. */
#define HW_LCD_DSI_CORSIE        2
#define HW_LCD_DSI_MBPS_CORSIA   832

/* [costruttore, Old Panel] I tempi del pannello, dalla macro
   JD9365_800_1280_PANEL_60HZ_DPI_CONFIG del demo Old Panel. I tre
   orizzontali sono gli stessi nelle due varianti; i due verticali no —
   la nuova vuole 10 e 30, questa 8 e 20. */
#define HW_LCD_HSYNC_PULSE       20
#define HW_LCD_HSYNC_BACK        20
#define HW_LCD_HSYNC_FRONT       40
#define HW_LCD_VSYNC_PULSE        4
#define HW_LCD_VSYNC_BACK         8
#define HW_LCD_VSYNC_FRONT       20

/* [costruttore, Old Panel] Il pixel clock, in megahertz.
 *
 * **Non si calcola.** C'e stato un momento in cui questo file lo ricavava
 * dai tempi — pixel per riga, per righe, per sessanta — e il conto tornava
 * anche: dava settanta, esattamente il numero del BSP della variante
 * nuova. Ma qui il conto darebbe sessantanove e il costruttore dice
 * sessanta, cioe cinquantadue fotogrammi al secondo invece di sessanta.
 *
 * Un vetro non e una formula: e un pezzo di hardware tarato da qualcuno
 * che lo ha misurato. Quando il suo numero e disponibile si usa il suo. */
#define HW_LCD_DPI_MHZ           60

/* --- i tre piedini della scheda, e i due che erano sbagliati ------------
 *
 * [costruttore] Dal BSP del pacchetto JC8012P4A1C_I_W_Y, ramo `#else` —
 * quello del vetro da 800x1280. Il ramo `#if CONFIG_BSP_LCD_TYPE_1024_600`
 * ha altri numeri, ed e da li che venivano i nostri: 26 per la
 * retroilluminazione e nessun reset per il tocco. Erano i valori giusti
 * dell'altro vetro.
 *
 * **La retroilluminazione e il 23, non il 26**, e il 23 era esattamente il
 * piedino che questo file dichiarava come reset del tocco. Cioe: a ogni
 * avvio si mandava un impulso sulla luce credendo di svegliare il vetro
 * capacitivo, e la si lasciava dove capitava. Il "cambio di illuminazione
 * al reset" che si vedeva era quello — l'unico segno di vita di uno
 * schermo che sembrava rotto era il nostro stesso codice che sbagliava
 * piedino.
 *
 * Il reset del tocco e il 22.
 */
/* --- il vetro montato a testa in giu ------------------------------------
 *
 * La cornice che regge questo pannello e al contrario, e rifarla costa piu
 * che ruotare il software. A 1 il vetro viene specchiato di 180 gradi **nel
 * pannello** — registro di orientamento del JD9365 — e non in LVGL: li
 * costa zero a runtime, mentre la rotazione software di LVGL 9 non
 * funziona con RENDER_MODE_DIRECT e obbligherebbe a smontare il doppio
 * framebuffer con scambio a vsync.
 *
 * **Lo leggono in due**, il vetro e il tocco, ed e il punto di averlo
 * scritto qui: specchiare il disegno e dimenticare il tocco da un pannello
 * che si vede bene e risponde dalla parte opposta. E gia successo una volta
 * con gli specchi scelti a occhio, e non deve poter succedere per
 * distrazione. */
#define HW_LCD_SPECCHIA_180      1

#define HW_P4_RETRO             23   /* a PWM, vedi hw_scheda.c */
#define HW_P4_LCD_RST           27
#define HW_P4_TOCCO_RST         22

/* --- I2C: solo il tocco -------------------------------------------------- */
/* [misurato] Il bus e questo: al primo avvio il comando `i2c` della console
   ha trovato quattro chip su sda=7 scl=8 — 0x18, 0x32, 0x36, 0x40 — e
   nessuno su nessun'altra coppia provata. Non era piu un'ipotesi. */
#define HW_I2C_SDA               7
#define HW_I2C_SCL               8
#define HW_I2C_HZ           400000

/* --- il tocco: un GSL3680 -----------------------------------------------
 *
 * Sta a 0x40: era nella primissima scansione del bus, insieme a 0x18, 0x32
 * e 0x36.
 *
 * I Silead non hanno il firmware a bordo: glielo si carica via I2C a ogni
 * accensione, ed e legato al modello di vetro. Quello di questa scheda sta
 * nel componente esp_lcd_touch_gsl3680 del pacchetto del costruttore.
 *
 * Gli altri tre indirizzi hanno un nome, e nessuno di loro e il tocco:
 * 0x18 il codec audio, 0x32 l'orologio del portapila, 0x36 il misuratore
 * della batteria. La scheda ha tutte e tre le cose.
 */
#define HW_TOCCO_ADDR         0x40   /* GSL3680 */
#define HW_TOCCO_INT            21

#endif /* SCHEDA_P4_H */
