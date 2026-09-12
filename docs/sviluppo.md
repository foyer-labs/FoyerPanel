# Foyer Panel — Sviluppo

Come si compila, si prova e si mette su un pannello. La presentazione del
progetto sta nel [README](../README.it.md) in cima al repository; la
specifica, documento per documento, in [`README.md`](README.md) di questa
cartella.

Il logo sta in [`logo/`](logo/): la versione per fondi chiari e per fondi
scuri, il solo segno e l'icona. Si disegna una volta sola, in
[`tools/gen_logo.py`](../tools/gen_logo.py), e da lì escono sia gli SVG sia
la geometria che il pannello disegna con LVGL. Le schermate del README si
rifanno con `python3 tools/screenshots.py --page`, dal simulatore e dalla
configurazione in `tools/demo/config.json`; l'immagine del supporto da
parete con `python tools/render_stl.py`, che vuole numpy e Pillow.

Due pannelli di controllo da parete collegati a Home Assistant, **stessa
interfaccia e una sola base di codice**. Il profilo è un parametro di
compilazione, non una variante del progetto.

| Profilo | Pannello | Risoluzione | Orientamento |
|---|---|---|---|
| `p4-1280x800` | 10,1" ESP32-P4 | 1280×800 | orizzontale |
| `p4-800x1280` | 10,1" ESP32-P4 | 800×1280 | verticale |

La specifica sta in questa cartella ed è il primo commit del repository.
Si legge partendo da [`README.md`](README.md).

---

## La regola che tiene insieme il progetto

**`main/profile.h` è l'unico sorgente che può contenere misure di layout**,
ed è generato da `docs/profili.json`, che a sua volta rispecchia
`docs/09-profili.md`. Nessun altro file, nemmeno per le misure uguali sui
due profili.

```c
lv_obj_set_width(rail, PRF->geo.rail_w);   /* si chiede al profilo */
lv_obj_set_width(rail, 96);                /* mai */
```

Senza questo vincolo i due pannelli divergono in poche settimane e ogni
correzione va fatta due volte.

Se una misura serve e non c'è: la si legge dal mockup 1:1 in
`docs/mockup/`, la si aggiunge a `09-profili.md` **e** a `profili.json`, e
si rigenera. Mai nel codice.

---

## Ambiente di sviluppo

Il sistema di sviluppo è **Windows 11**, ma la Fase 1 si compila in
**WSL2 con Ubuntu**: SDL2 e la toolchain ESP-IDF sono molto più lisci lì.

### Preparazione, una volta sola

Da PowerShell, se WSL non c'è ancora:

```bash
wsl --install -d Ubuntu
```

Poi, dentro Ubuntu:

```bash
sudo apt update && sudo apt install -y build-essential cmake ninja-build pkg-config libsdl2-dev git python3
```

La finestra del simulatore si apre sul desktop di Windows tramite WSLg,
che è già incluso in Windows 11: non serve un server X.

Per verificare le configurazioni serve anche `jsonschema`, dal lato che
preferisci:

```bash
pip install jsonschema
```

### Compilare ed eseguire

```bash
cmake -B build -G Ninja && cmake --build build
```

```bash
./build/pannello
```

Senza argomenti parte sul **profilo predefinito**, che è `p4-800x1280` — il
pannello verticale montato al muro. Sta scritto in `docs/profili.json` sotto
`_predefinito`, e da lì finisce in `profile.h`. Per l'altro orientamento:

```bash
./build/pannello --profilo p4-1280x800
```

| Opzione | Effetto |
|---|---|
| `--profilo CHIAVE` | avvia su un profilo diverso da quello predefinito |
| `--scegli` | apre il selettore dei profili |
| `--cattura FILE` | disegna un fotogramma in un BMP ed esce, **senza aprire finestre** |
| `--sezione CHIAVE` | apre una sezione invece della home |
| `--vista N` | apre una vista dentro la sezione (sul clima: 0 riscaldamento, 1 condizionatori) |
| `--pagina N` | apre una schermata paginata sulla pagina N |
| `--web [PORTA]` | serve la pagina di configurazione (predefinita 8080) |
| `--sblocca` | parte con la configurazione già sbloccata |
| `--ha` | si collega a Home Assistant come farebbe il pannello |
| `--prova-sorveglianza` | verifica le tre soglie del collegamento |
| `--dati CARTELLA` | dove stanno `config.json` e la sua copia (predefinita: `dati/`) |
| `--vuoto` | parte senza configurazione, cioè al primo avvio |
| `--segreto N=V` | imposta un segreto (`wifi_pw`, `ha_token`, `wifi_share1_pw`, …) |
| `--prova-tempi` | fa scorrere il tempo e verifica le temporizzazioni |
| `--prova-struttura` | stampa cosa ha ricavato da `config.json` ed esce |
| `--prova-heap [N]` | naviga tutte le schermate e controlla la memoria |
| `--stato NOME` | mostra uno stato di `01-specifica-ui.md` §4: `avvio`, `primo-avvio`, `riconnessione`, `ha-giu`, `conferma` |
| `--casi NOME` | mette i dati in un caso limite di `11-collaudo.md` §2 |
| `--fps` | mostra il contatore di prestazioni di LVGL |
| `--elenca` | stampa i profili disponibili |
| `--aiuto` | tutte le opzioni |

`profile.h` viene rigenerato a ogni configurazione di CMake: modificare
`profili.json` e ricompilare basta, non c'è un passo da ricordare.

Alla prima esecuzione il simulatore copia `docs/04-config.example.json` in
`dati/config.json` e da lì in poi legge quello: da quel momento è un file
dell'archivio come lo sarebbe sul pannello, si può modificare a mano e si
riscrive con lo stesso salvataggio atomico. Per rifare la prova da pannello
appena tolto dalla scatola basta cancellare la cartella, o usare `--vuoto`.

### La pagina di configurazione

```bash
./build/pannello --web 8080
```

Il server ascolta subito, ma **tutto risponde `404`** tranne `/api/status` e
`/api/log`: la pagina si apre solo dopo aver toccato "Consenti
configurazione" in Impostazioni sul pannello, e si richiude da sola dopo
quindici minuti. `--sblocca` la apre subito e esiste solo nel simulatore —
sul pannello lo sblocco è fisico, e questo è il punto.

La pagina sta in `web/index.html` e finisce nella flash del firmware:
`tools/genera_pagina.py` la impacchetta in `main/web/pagina.h` insieme a
`config.schema.json`, e CMake lo rifà a ogni configurazione. I campi non
sono scritti a mano: li genera lo schema, che è **lo stesso file** con cui
valida il firmware — se la pagina se ne portasse dietro una copia, la prima
modifica allo schema le farebbe accettare cose che il pannello rifiuta.

### Home Assistant

```bash
python3 tools/finto_ha.py --porta 8123
./build/pannello --ha
```

Con `--ha` il simulatore si collega come farebbe il pannello: WebSocket,
token a lunga durata, sottoscrizione a `state_changed`, **nessun polling**.
Senza, i dati restano quelli inventati che servono alle catture.

`tools/finto_ha.py` è un finto Home Assistant che risponde le entità della
configurazione d'esempio. Non è un'emulazione: serve a far succedere le
situazioni che il collaudo richiede e che altrimenti si possono solo
aspettare — `--token-cattivo`, `--cadi-dopo N`, `--manca ENTITÀ`,
`--comandi-falliscono`. `tools/prova_ha.py` le esegue tutte.

I segreti — password del Wi-Fi, token di Home Assistant — non stanno in
`config.json`: sul pannello vivono in NVS, nel
simulatore in `dati/segreti/`. Si scrivono e si può chiedere se ci sono,
ma non si rileggono: `segreti.h` non ha una funzione che ne restituisca
uno. Chi deve usarne uno lo riceve in prestito per la durata di una
chiamata. `--segreto` esiste solo qui, per le prove e le catture: sul
pannello i segreti arrivano dal primo avvio o dalla pagina web.

Quali zone, quali unità, quali accessi e quali sezioni
esistono lo dice quel file, non il codice: cambiarlo cambia l'interfaccia
senza ricompilare. Quello che il finto fornitore di dati inventa è solo lo
**stato** — se una luce è accesa, a che temperatura è una stanza — e sparirà
in Fase 3 quando risponderà Home Assistant.

### Compilare per un pannello vero

Il simulatore gira sul PC; il pannello si compila con ESP-IDF, per
l'ESP32-P4: schermo MIPI-DSI e radio su un ESP32-C6 a parte.

Il pannello è la **Guition JC8012P4A1C**, 800×1280, con una cartella di
compilazione sua. **Prima della prima compilazione serve una chiave di
firma**: senza, la compilazione si ferma — come crearla è più sotto, in
[Aggiornare un pannello già installato](#aggiornare-un-pannello-già-installato).
La guida al primo avvio della scheda sta in
[`docs/13-primo-avvio-p4.md`](13-primo-avvio-p4.md).

```bash
cd firmware && idf.py -B build-p4 -D SDKCONFIG=sdkconfig.p4 set-target esp32p4
```

```bash
cd firmware && idf.py -B build-p4 -D SDKCONFIG=sdkconfig.p4 -D PANNELLO_PROFILO_SCELTA=PRF_P4_800X1280 -p COMx flash monitor
```

La cartella separata non è pignoleria: `set-target` riscrive `sdkconfig` da
zero, e riusare una cartella vorrebbe dire perdere le regolazioni tarate
sul vetro.

Il profilo predefinito è il verticale, quello appeso al muro. Per
l'orizzontale si passa `-D PANNELLO_PROFILO_SCELTA=PRF_P4_1280X800`: il
valore resta nella cache di CMake, quindi lo si ripete solo quando si
ricomincia da zero.

#### Cosa ha detto la scheda vera

Il p4 è stato acceso, e cinque numeri che erano stati dedotti dalla
documentazione erano sbagliati. Stanno tutti in `main/hw/scheda_p4.h`, ora
marcati `[costruttore]` invece che `[da confermare]`:

| | si diceva | è |
|---|---|---|
| Flash | 32 MB | **16 MB** |
| Revisione del chip | v3.1+ | **v1.3** (`SELECTS_REV_LESS_V3`) |
| Retroilluminazione | GPIO 26, acceso/spento | **GPIO 23, a PWM** |
| Reset del tocco | GPIO 23 | **GPIO 22** |
| Corsie DSI | 1000 Mbit/s | **840 Mbit/s** |

I due driver che questa scheda vuole — il vetro JD9365 con la sequenza a
due corsie e il GSL3680 con il suo firmware — stanno in
`firmware/components/`, copiati dal pacchetto del costruttore. Il pacchetto
intero è ottocento megabyte e non è versionato: si scarica da
`pan.jczn1688.com`, il link sta nel manuale del display.

La console del pannello ha tre comandi che servono quando una scheda nuova
non dice cosa ha dentro: `i2c` cerca i chip sul bus, `schermo COLORE`
riempie il vetro saltando l'interfaccia, `luce` cerca il piedino della
retroilluminazione. Sono stati scritti per questo primo avvio e restano per
il prossimo.

### Aggiornare un pannello già installato

Dalla pagina di configurazione, sezione **Stato e registro**: si sceglie il
`pannello.bin` prodotto dalla compilazione e si preme Installa.

Si scrive sulla partizione ferma, la firma si verifica prima che la
partizione di avvio cambi, e la versione nuova parte **in prova**: si
conferma da sola solo dopo aver visto rete, Home Assistant e schermo che
funzionano. Se non ce la fa, entro un minuto e mezzo torna alla precedente
senza che nessuno debba intervenire.

Le immagini sono firmate con `firmware/chiave_firma.pem`, che **non è nel
repository**. Se manca:

```bash
cd firmware && idf.py secure-generate-signing-key --version 2 chiave_firma.pem
```

Attenzione: una chiave nuova non aggiorna più i pannelli che hanno la
vecchia — le loro immagini verrebbero rifiutate. Si rimettono in pari
riflashandoli da USB. Vale la pena tenerne una copia.

### Prove

```bash
ctest --test-dir build --output-on-failure
```

Dodici prove: le quattro tabelle di profilo controllate insieme; ogni
profilo compilato **da solo**, come sul pannello, per accorgersi subito se
un campo esiste solo nel ramo del simulatore; tre verifiche sulla regola
del progetto — nessun numero di layout fuori da `profile.h`, `profile.h`
allineato a `profili.json`, `profili.json` allineato ai mockup; due prove
delle temporizzazioni, che fanno scorrere il tempo e controllano ritorno
alla home, standby, spegnimento e risveglio; e due prove dell'heap, che
navigano tutte le schermate e controllano che la memoria non cresca.

### Confronto con i mockup

I mockup in `docs/mockup/` sono rendering 1:1 delle schermate approvate.
Per mettere a confronto quello che il codice disegna:

```bash
python tools/cattura.py
```

Cattura ogni schermata in tutti i profili sotto `build/catture/` e
converte in PNG. Non serve uno schermo: la cattura non usa SDL.

Con `--casi` cattura anche i casi limite di `11-collaudo.md` §2:

```bash
python tools/cattura.py --schermata luci --casi una-luce --casi nomi-lunghi
```

---

## Attrezzi

| Comando | Cosa fa |
|---|---|
| `python tools/genera_profilo.py` | riscrive `main/profile.h` da `docs/profili.json` |
| `python tools/genera_profilo.py --verifica` | esce 1 se `profile.h` è disallineato |
| `python tools/estrai_misure.py` | rilegge il CSS dei mockup e lo confronta con `profili.json` |
| `python tools/genera_font.py` | compila i corpi di `09-profili.md` §4 in font LVGL (serve Node) |
| `python tools/genera_icone.py` | compila le icone in font-icona (serve Node) |
| `python tools/verifica_numeri.py` | cerca numeri di layout fuori da `profile.h` |
| `python tools/cattura.py` | cattura le schermate in tutti i profili |
| `python tools/bmp2png.py build/catture` | converte le catture in PNG |
| `python tools/verifica_config.py docs/04-config.example.json` | valida una configurazione: schema più regole fra campi |

---

## Struttura

```
docs/          la specifica, primo commit del repository
main/
  profile.h    GENERATO — l'unico file con misure di layout
  profile_def.c   l'unica unita che definisce la tabella
  ui/          interfaccia: niente chiamate all'hardware, compila anche sul PC
  app/         stato applicativo e fornitori di dati
  ha/ cam/ cfg/ web/ sys/    fasi 2-5
sim/           simulatore SDL2, solo PC
test/          prove che girano senza hardware
tools/         generatori e verifiche
```

`ui_sources.cmake` è l'elenco condiviso dei sorgenti: lo leggono sia la
compilazione per il PC sia quella per il pannello, così un file che smette
di compilare per uno dei due bersagli si nota subito.

---

## Fasi

| Fase | Contenuto | Tag |
|---|---|---|
| 1 | interfaccia nel simulatore, tutti i profili | `v0.1` |
| 2 | configurazione su LittleFS e pagina web | `v0.2` |
| 3 | client WebSocket di Home Assistant | `v0.3` |
| 4 | telecamere via go2rtc — **tolte l'08/09/2026** | `v0.4` |
| 5 | hardware, OTA, messa in opera | `v1.0` |

Una fase non si considera chiusa finché i criteri di
[`docs/11-collaudo.md`](11-collaudo.md) non passano.

Un ramo per fase (`fase-1-simulatore`, `fase-2-config`, …), commit in
italiano all'imperativo con il prefisso d'area (`ui:`, `cfg:`, `ha:`,
`cam:`, `web:`, `sys:`, `docs:`, `build:`).

Il numero di versione mostrato all'avvio e in `/api/status` viene dal tag
più recente con `+dev` quando ci sono commit successivi, ricavato dalla
build: non esiste un `#define` da aggiornare a mano.

---

## Licenza

Il progetto è distribuito sotto **GPL-3.0-or-later**: il testo sta in
[`LICENSE`](../LICENSE). In breve, chiunque può usarlo, modificarlo e
ridistribuirlo, anche compilato dentro un pannello, a patto di dare a sua
volta i sorgenti con la stessa licenza.

La versione 3 non è una preferenza. Il firmware del P4 unisce due codici
che le versioni precedenti non riescono a tenere insieme: il driver del
tocco è GPL-2.0-*or-later* e il driver del vetro è Apache-2.0, e fra le
licenze GPL la sola compatibile con Apache-2.0 è la 3.

Il codice di altri che sta nel repository, o che la compilazione scarica,
mantiene la sua licenza:

| Cosa | Dove | Licenza |
|---|---|---|
| driver del tocco GSL3680 | `firmware/components/esp_lcd_touch_gsl3680` | GPL-2.0-or-later, più il firmware del chip senza licenza dichiarata — vedi il [README](../firmware/components/esp_lcd_touch_gsl3680/README.md) della cartella |
| driver del vetro JD9365 | `firmware/components/esp_lcd_jd9365` | Apache-2.0 |
| Inter, JetBrains Mono | `tools/font` | SIL Open Font License 1.1 |
| Material Symbols | `tools/font` | Apache-2.0 |
| LVGL, cJSON | scaricati alla compilazione | MIT |
| LittleFS | scaricato alla compilazione | MIT il componente, BSD-3-Clause il file system |
| ESP-IDF, esp_hosted, esp_wifi_remote, mDNS | scaricati alla compilazione | Apache-2.0 |

I testi delle licenze dei font stanno accanto ai font, quello del JD9365
accanto al suo driver.
