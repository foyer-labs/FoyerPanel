# Foyer Panel — Architettura del firmware

> Il pannello di riferimento è `p4-800x1280`.


**Una sola base di codice per due pannelli.** Il profilo è un parametro,
non una variante del progetto: tutte le divergenze stanno in
`09-profili.md` e, nel codice, nel solo `profile.h`.

Dove questo documento descrive scelte che dipendono dall'hardware, lo dice
esplicitamente indicando il profilo.

Se una scelta qui contrasta con un vincolo reale scoperto in sviluppo,
segnalalo invece di cambiarla in silenzio.

---

## 1. Partizioni

### Profilo `p4-1280x800` (Flash 16 MB)

Qui c'era scritto 32 MB, con accanto un avvertimento: «verificare la Flash
reale, alcune varianti compatibili ne hanno 16». **Ne ha 16.** Lo ha detto
`esptool flash_id` al primo collegamento, e la tabella disegnata per 32 non
ci sarebbe entrata. L'avvertimento era giusto; la tabella lo ignorava.

| Partizione | Tipo | Dimensione | Contenuto |
|---|---|---|---|
| `nvs` | data | 32 KB | segreti, contatori |
| `otadata` | data | 8 KB | stato OTA |
| `phy_init` | data | 4 KB | — |
| `factory` | app | 3,5 MB | firmware di recupero |
| `ota_0` | app | 3,5 MB | applicazione |
| `ota_1` | app | 3,5 MB | applicazione |
| `c6_fw` | data | 2 MB | immagine firmware del co-processore ESP32-C6 |
| `storage` | data, littlefs | 3,4 MB | `config.json`, registro |

Le tre partizioni di applicazione sono **uguali**: una `factory` che non
contiene l'immagine che stai per installare non e una partizione di
recupero. Il binario di oggi ne occupa 2,13 su 3,5.

`storage` arriva all'ultimo byte della flash. Non resta niente libero, ed e
voluto: dello spazio non assegnato su una flash stretta non si potrebbe piu
fare niente senza spostare LittleFS, e spostarlo cancella la
configurazione di chi aggiorna.

La partizione `c6_fw` esiste perché il P4 non ha radio: il Wi-Fi passa da un
ESP32-C6, il co-processore va aggiornato insieme all'applicazione, e la sua
immagine deve viaggiare con il firmware.

La partizione di recupero non è opzionale: il pannello è a muro.

---

## 2. Task

| Task | Core | Priorità | Compito |
|---|---|---|---|
| `lvgl_task` | 1 | 4 | `lv_timer_handler()`, unico a toccare LVGL |
| `ha_ws_task` | 0 | 5 | WebSocket, autenticazione, eventi, comandi |
| `http_srv_task` | 0 | 2 | server di configurazione (solo se sbloccato) |
| `sys_task` | 0 | 2 | NTP, standby, luminosità, sorveglianza |

Le regole:
- **solo `lvgl_task` chiama funzioni LVGL**; gli altri accodano messaggi
- comunicazione con code FreeRTOS di strutture
- nessuna operazione bloccante di rete nel task grafico

---

## 3. Memoria

**PSRAM 32 MB** — abbondante:

| Voce | `p4-1280x800` |
|---|---|
| Framebuffer doppio | 2 × 2,05 MB |
| Buffer di disegno LVGL | ~400 KB |
| Configurazione e stato entità | < 100 KB |
| **PSRAM disponibile** | **32 MB** |

Restano oltre 27 MB liberi. Erano quattro voci in più — riquadro grande,
quadrivisione, striscia in home, cache dei fotogrammi — e da sole facevano
due megabyte e mezzo: sono uscite con le telecamere l'08/09/2026.

**RAM interna**: 768 KB. È la risorsa scarsa, ed è la ragione per cui non
si usa TLS: il collegamento è locale.

---

## 4. Wi-Fi tramite ESP-Hosted

Il P4 non ha radio. Il modulo ESP32-C6 collegato via SDIO fa da scheda di
rete: sul P4 gira `esp_wifi_remote`, sul C6 il firmware `esp-hosted-mcu`.

Conseguenze operative:

1. **Due firmware da tenere allineati.** Una versione del P4 che si aspetta
   un protocollo diverso da quello del C6 non si connette. L'immagine del
   C6 sta nella partizione `c6_fw` e va verificata all'avvio.
2. **L'inizializzazione del C6 è un passo separato** nella schermata di
   avvio, e può fallire da solo.
3. **La latenza è leggermente superiore** a un Wi-Fi nativo: irrilevante
   per Home Assistant, da tenere presente nel dimensionamento dei timeout.
4. Il Bluetooth passa dallo stesso collegamento. **Non lo usiamo** in
   questa versione.

---

> **Qui c'erano due capitoli**: §5, la catena delle telecamere — go2rtc,
> `stream.mjpeg` con la connessione tenuta aperta, i 7,6 fotogrammi al
> secondo misurati sul go2rtc di casa contro i 5,8 secondi di una richiesta
> singola — e §5-bis, la decodifica JPEG in silicio del P4 con le sue tre
> trappole (`element_order` BGR, i buffer da `jpeg_alloc_decoder_mem`,
> `conv_std` da dichiarare).
>
## 5. Il video: perché non si fa, e cosa servirebbe

> **Le telecamere sono uscite dal progetto due volte in due giorni**, e la
> seconda volta con una risposta invece che con una richiesta. Questo
> capitolo non racconta com'era fatto il codice — quello sta in `git log`,
> commit `ba3a2e2`, `1153660`, e vive lì benissimo. Racconta **i numeri**,
> perché sono la cosa che non si può dedurre e che costerebbe rifare.

### Il fatto che decide tutto

**L'ESP32-P4 non ha un decodificatore H.264 in hardware.** Ne ha uno per
*comprimere* — il verso opposto, pensato per chi manda video, non per chi
lo guarda — e ha un decodificatore **JPEG**, quello sì in silicio.

Per l'H.264 esiste un solo decodificatore su questo chip: quello software
del componente `espressif/esp_h264`, derivato da tinyH264. Accetta il solo
profilo **constrained baseline**: niente CABAC, niente immagini B.

Il numero dichiarato dal costruttore: **1280×720 a 10 fotogrammi al
secondo**, *un* flusso, con la CPU tutta per sé. Sono **9,2 megapixel al
secondo** di capacità di decodifica.

### Il conto che chiude la questione

Otto riquadri a 640×360, a otto fotogrammi l'uno, chiedono **14,7 megapixel
al secondo**: il 60% oltre quel tetto, e quel tetto presuppone un chip che
non stia facendo nient'altro. Qui deve anche disegnare a 52 fps e tenere il
WebSocket di Home Assistant.

Con la CPU che resta davvero, otto telecamere farebbero circa **due
fotogrammi al secondo l'una** — e il vetro scatterebbe, perché la
decodifica gira nello stesso ciclo che disegna. Quattro invece di otto
dimezza il conto e resta uno scarso.

**Non è una strada lenta: è una strada chiusa** per qualunque cosa oltre un
singolo flusso baseline piccolo.

### E le telecamere di questa casa sono sopra baseline

Misurato il 09/09/2026 sulla Foscam dell'ingresso, con due segnali
indipendenti che dicono la stessa cosa:

- l'SDP dichiara un `profile-level-id` sopra 66, letto prima ancora di
  chiedere il video
- il decodificatore, alimentato coi fotogrammi veri, rifiuta l'SPS:
  `Serious error in decoding, failed to activate param sets`

Cioè: anche volendo accettare due fotogrammi al secondo, quei flussi non si
aprirebbero affatto.

### Quello che invece il silicio sa fare

Il decodificatore JPEG è in hardware, e il numero è stato misurato in
questa stessa casa il 29/08/2026, interrogando una Foscam:

> 8 scatti in 0,8 s = **9,9 al secondo, 1280×720**, 63 kB l'uno

Lì la decodifica non costa CPU. Il limite si sposta sulla banda verso la
radio — e anche quello è misurato: quattro dirette a 720p facevano
riavviare il pannello, circa 2,5 MB/s sul collegamento SDIO verso l'ESP32-C6.
A misura di riquadro (640×360, una trentina di kB) otto telecamere a tre o
quattro fotogrammi al secondo fanno circa 1 MB/s: **plausibile, mai
provato**.

### Cosa dovrebbe cambiare per riprovarci

Una delle due, non a scelta ma a seconda di cosa si presenta per primo:

1. **Un microcontrollore con un decodificatore H.264 in hardware.** È la
   condizione che chi usa il pannello ha nominato quando ha deciso di
   fermarsi, ed è quella giusta: con la decodifica gratis, il limite
   tornerebbe a essere la banda, che è un problema molto più docile.
2. **Telecamere che emettano constrained baseline**, e allora anche il
   decodificatore software basta — ma per **una o due**, non per otto. Il
   conto qui sopra non cambia.

La strada JPEG resta percorribile oggi e non è stata scelta: reintrodurrebbe
quello che era già stato tolto una volta, e il gesto che vale davvero
davanti a un pannello a muro — tocco e guardo *una* telecamera — non chiede
otto dirette.

### Cosa resta, di tutto questo

Un difetto vero, trovato per strada e **non** rimosso con le telecamere: in
`canale_esp.c` il TCP in chiaro scambiava «adesso non c'è niente da
leggere» per «l'altro ha chiuso». Riguardava qualunque collegamento non
cifrato, non solo il video, e non si era mai visto perché in chiaro non
parlava nessuno — Home Assistant è cifrato. Vedi `main/rete/canale.h`.

## 6. Configurazione

Come da `03-config-contratto.md`:

- il firmware dichiara il proprio **profilo** in `GET /api/status`
- l'impaginazione delle griglie è decisa dal firmware, non memorizzata
- `config.json` è interscambiabile fra i due orientamenti

---

## 7. Aggiornamento OTA

Si carica un file dalla pagina di configurazione, e il pannello fa il resto.
Tre garanzie, e ognuna copre un modo diverso di andare male.

**Si scrive sempre sull'altra partizione.** Quella che sta girando non si
tocca. Se la scrittura si interrompe — corrente, rete, un pacchetto corrotto
— al riavvio riparte quella di prima, intatta. È il motivo per cui ci sono
`ota_0` e `ota_1` invece di una sola.

**La firma si verifica prima che la partizione di avvio cambi.** Non durante
la scrittura: un'immagine ancora a pezzi non si può verificare. Il controllo
è alla chiusura, su tutto il blocco, dentro `esp_ota_end()` — e di nuovo al
riavvio, dal bootloader.

**Il firmware nuovo si dichiara valido da solo, e solo se funziona.**
Riavviare non basta: un'immagine può partire benissimo e non riuscire a fare
la cosa per cui esiste. Parte **in prova**, e si conferma solo dopo aver
visto tutte e tre le cose che questo apparecchio deve saper fare — rete su,
Home Assistant che risponde con la casa, uno schermo che disegna. Sono gli
stessi tre controlli del primo avvio, e non è un caso: sono la definizione
di «funziona» per questo pannello.

Se entro il tempo di grazia non si è visto tutto, il pannello si riavvia e
il bootloader rimette quella di prima. **Nessuno deve accorgersene e nessuno
deve intervenire**: 120 secondi, per dare respiro al co-processore che
deve tirare su la radio.

### Il trasporto

Un'immagine sono due megabyte, e non si tengono in memoria mentre li si
scrive in flash. `/api/ota` è l'**unico** percorso a flusso del server: il
corpo arriva a pezzi da 4 kB e ognuno va dritto in `esp_ota_write()`. Il
resto del server continua a bufferizzare, che per un documento di
configurazione è la cosa giusta e lo rende provabile senza aprire un socket.

Il file si manda grezzo, non in un multipart: il server è scritto a mano, e
insegnargli i confini fra le parti sarebbe codice in più su un apparecchio
che ne ha già abbastanza, per trasportare una cosa sola.

Il percorso a flusso **scavalca il dispatcher**, quindi il controllo dello
sblocco è scritto una seconda volta dentro `ota_apri()`. Sembra un doppione
e non lo è: toglierlo lascerebbe aperta sulla rete la porta più grossa che
questo apparecchio abbia. C'è una prova apposta.

### La chiave

`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`, non il secure boot vero. Quello
brucia gli eFuse, blocca anche il bootloader e non si torna indietro; questo
verifica la firma dell'applicazione e si può spegnere. Protegge da
un'immagine che arriva dalla **rete**, non da chi ha il pannello in mano — e
per un pannello di casa è il taglio giusto: chi ci arriva fisicamente può
staccarlo e riflasharlo da USB comunque.

La chiave sta in `firmware/chiave_firma.pem` e **non è versionata**. Si rifà
così:

```
idf.py secure-generate-signing-key --version 2 chiave_firma.pem
```

Perderla significa non poter più aggiornare via rete *quel* pannello: le
immagini firmate con una chiave nuova vengono rifiutate da quello che ci gira
sopra. Non è un vicolo cieco — si riflasha da USB e riparte tutto — ma è un
pomeriggio.

### Cosa **non** fa

**Non aggiorna il co-processore C6** del p4. Il pacchetto contiene la sola
applicazione. Due firmware che si aggiornano separatamente prima o poi si
disallineano, ed è un problema vero — ma riprogrammare il C6 attraverso SDIO
non si può provare senza la scheda, e un aggiornamento non provato che gira
su un pannello a muro è peggio di un aggiornamento che non c'è. All'avvio il
pannello dice se `c6_fw` è vuota, così il guasto non si presenta come «il
Wi-Fi non va».

**Non scarica da un server.** Il file lo si porta, non lo si va a prendere:
un pannello che si aggiorna da solo da un indirizzo su Internet è una
superficie in più su una rete di casa, e il guadagno — non dover premere un
pulsante due volte l'anno — non la vale.

---

## 8. Macchina a stati dell'interfaccia

```
AVVIO ─→ HOME ⇄ SEZIONE ─→ MODALE (conferma, PIN)
  │        │
  │        ├─ 120 s ─→ STANDBY ─ 600 s ─→ SPENTO
  │        │
  │        └─ connessione persa > 60 s ─→ HA_GIU ─→ ritorno
  └─ errore non recuperabile ─→ PRIMO_AVVIO
```

Il timer di inattività si azzera a ogni tocco. Standby e spegnimento sono
sospesi con la schermata Wi-Fi aperta o un modale attivo.

---

## 8-bis. Rotazione del display

L'orientamento fa parte del profilo (`09-profili.md` §1-bis). Il vetro è
nativo 800×1280, verticale: è il profilo *orizzontale* a richiedere la
rotazione, che qui costa poco — MIPI-DSI, nessuna contesa di banda e un
acceleratore grafico disponibile.

In nessun caso la rotazione va aggirata ridisegnando i widget ruotati a
mano: il layout è già scritto per l'orientamento giusto nel profilo, e la
rotazione riguarda solo il rapporto fra framebuffer e vetro.

## 9. Configurazione ESP-IDF

### Profilo `p4-1280x800`

```
CONFIG_IDF_TARGET=esp32p4
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_ESP_WIFI_REMOTE_ENABLED=y
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_LV_COLOR_DEPTH_16=y
```

Il MIPI-DSI va configurato a 2 corsie, 832 Mbps per corsia: perché non i
1500 del BSP lo dice `main/hw/scheda_p4.h`.

---

## 10. Struttura del progetto

```
pannello/
├── main/
│   ├── main.c
│   ├── app_state.h/.c
│   ├── profile.h              ← UNICO file con le misure di layout
│   │                             generato da profili.json
│   ├── ui/
│   │   ├── theme.h/.c
│   │   ├── screen_*.c         home, luci, clima, energia, camere,
│   │   │                        accessi, agenda, wifi, impostazioni,
│   │   │                        avvio, primo_avvio
│   │   ├── overlay_*.c        conferma, pin
│   │   ├── widgets/
│   │   └── fonts/  icons/
│   ├── ha/    cam/    cfg/    web/    sys/
├── sim/
└── firmware/
    ├── partitions.csv
    └── sdkconfig.defaults
```

`profile.h` raccoglie **tutte** le misure che cambiano fra i profili.
Generalo da `profili.json` invece di trascriverlo a mano.

**Nessun altro file sorgente può contenere numeri di layout.** Un
componente che ha bisogno di sapere quanto è largo il rail lo chiede al
profilo. È il vincolo che tiene insieme una sola base di codice per due
schermi; senza, i due pannelli divergono nel giro di poche settimane.

Il codice in `ui/` deve compilare sia per il simulatore sia per il
pannello: nessuna chiamata all'hardware.
