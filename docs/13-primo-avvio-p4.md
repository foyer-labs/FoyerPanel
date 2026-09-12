# Foyer Panel — Il primo avvio della scheda P4

La scheda di riferimento è la **JC8012P4A1C**: ESP32-P4, vetro JD9365 su
MIPI-DSI a due corsie, tocco GSL3680 e un ESP32-C6 che fa da radio
attraverso `esp_hosted`. Tutto quello che dipende dal silicio è deciso nel
codice; tutto quello che dipende dalla *scheda* — piedini, corsie DSI,
indirizzi — sta in un solo file, `main/hw/scheda_p4.h`, con ogni valore
marcato per quanto vale.

Questo documento serve il giorno in cui la scheda è nuova, oppure non è
esattamente quella di riferimento.

## Compilare e scrivere

```
idf.py -B build-p4 -D SDKCONFIG=sdkconfig.p4 set-target esp32p4
idf.py -B build-p4 -D SDKCONFIG=sdkconfig.p4 -p COMx flash monitor
```

La cartella separata non è pignoleria: `set-target` riscrive `sdkconfig` da
zero, e con una cartella sola cambiare pannello vorrebbe dire ricompilare
tutto ogni volta e perdere le regolazioni tarate sul vetro dell'altro.

Per il pannello verticale si aggiunge `-D PANNELLO_PROFILO_SCELTA=PRF_P4_800X1280`.

**Al primo giro i componenti si scaricano.** LVGL, mDNS e la radio remota
arrivano dal registro dei componenti di Espressif e finiscono in
`firmware/managed_components/`; il vetro e il tocco invece stanno in
`firmware/components/`, perché le versioni del registro non vanno bene per
questa scheda. Sono una trentina di secondi e
una connessione; vale la pena saperlo prima di trovarsi a compilare senza
rete.

## Il firmware del co-processore **non** è in questo progetto

`idf.py flash` scrive bootloader, tabella delle partizioni, dati OTA e
applicazione. **La partizione `c6_fw` resta come l'ha lasciata la
fabbrica**: il firmware dell'ESP32-C6 che fa da radio — `esp-hosted-mcu` —
è un progetto ESP-IDF a parte, per un altro silicio.

Le strade sono due:

- **la scheda arriva col C6 già programmato**, che è il caso normale sulle
  schede vendute come "P4 + C6": allora non c'è niente da fare;
- **non lo è**: si compila `esp-hosted-mcu` per l'esp32c6 e lo si scrive sul
  C6 attraverso la sua porta.

La versione di `esp_hosted` usata dal P4 deve parlare con quella che gira
sul C6: è fissata in `firmware/main/idf_component.yml` per questa ragione, e
non va aggiornata da sola.

Il pannello guarda la partizione all'avvio e lo dice nel registro. Se legge
`c6_fw e vuota`, lì non c'è un'immagine da cui aggiornare la radio — ma la
radio usa quella che ha nella sua flash, e se sale va tutto bene. Se invece
la rete non sale, la distinzione conta: **non è un problema di password né
di SSID**, e senza quel messaggio si comincerebbe a cercare dalla parte
sbagliata.

Portare l'immagine del C6 dentro il pacchetto OTA, come dice
`05-architettura-firmware.md` §9, è lavoro che resta da fare: due firmware
che si aggiornano insieme sono l'unico modo perché non si disallineino.

## Prima di tutto, la Flash

```
esptool.py --port COMx flash_id
```

Cerca la riga con la dimensione. La scheda di riferimento ne ha **16 MB**, e
`firmware/partitions.csv` li usa fino all'ultimo byte: tre partizioni di
applicazione da 3,5 MB, il firmware del C6 e LittleFS. Una variante con
meno flash vuole una tabella sua; una con di più funziona, e il resto
semplicemente non si usa. La tabella stessa racconta perché è fatta così.

Il pannello fa lo stesso controllo a ogni accensione e lo scrive nel
registro: lo si scopre ogni volta, non solo il giorno in cui qualcuno si
ricorda di controllare.

## Cosa guardare, in ordine

Il registro sulla seriale dice a che punto si è fermato. I modi in cui può
fermarsi sono questi, e ognuno ha un colpevole diverso:

| Sintomo | Dove guardare |
|---|---|
| `questa scheda ha N MB di flash e il profilo … ne dichiara M` | la scheda non è quella che il profilo crede: o è una variante con meno flash, o `memoria.flash_mb` in `profili.json` è sbagliato. Lo stesso messaggio c'è per la PSRAM |
| `regolatore delle corsie MIPI non acceso` | `chan_id` in `firmware/main/hw_lcd_dsi.c`: è il canale LDO delle corsie, e su schede diverse può cambiare |
| `vetro JD9365 non riconosciuto` | il pannello monta un altro controllore — l'EK79007 è l'altro comune. Il JD9365 sta in `firmware/components/esp_lcd_jd9365` perché la sequenza del registro dichiara quattro corsie e questa scheda ne ha due; un altro vetro vuole il suo componente e `HW_LCD_PANNELLO` |
| DSI parte senza errori ma **lo schermo resta nero** | quasi sempre la retroilluminazione: `HW_P4_RETRO`. È il primo piedino da confrontare col foglio della scheda. Dalla console seriale, `luce` cerca il piedino e `schermo` riempie il vetro di un colore saltando LVGL |
| il tocco non risponde | `HW_I2C_SDA` / `HW_I2C_SCL`, poi `HW_TOCCO_ADDR` e `HW_TOCCO_INT`. Il GSL3680 risponde a 0x40 e vuole il proprio firmware caricato a ogni accensione. Il comando `i2c` della console seriale elenca i chip che rispondono sul bus: un controllore diverso si vede lì |
| la rete non sale | è il co-processore. Due cause distinte: **i piedini SDIO** verso il C6 — `idf.py -B build-p4 menuconfig`, sezione ESP-Hosted, dove c'è un preset per le schede note e i sei piedini uno per uno; oppure **il firmware del C6**, che deve essere allineato a quello del P4 |

La scheda di riferimento parte con i valori del repository. Le righe della
tabella sono i punti in cui una scheda *diversa* può divergere: scritte
perché il giorno che il pannello resta nero si sappia da dove cominciare.
