<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/logo/foyer-dark.svg">
    <img src="docs/logo/foyer-light.svg" alt="Foyer Panel" width="420">
  </picture>
</p>

<p align="center">
  <b>Foyer Panel</b>: un pannello da parete per Home Assistant che è firmware, non un browser.<br>
  ESP32-P4 da 10,1″, 800×1280, C nativo con LVGL 9. Una sola base di codice, verticale e orizzontale.
</p>

<p align="center">
  <a href="https://github.com/foyer-labs/FoyerPanel/actions/workflows/build.yml"><img src="https://github.com/foyer-labs/FoyerPanel/actions/workflows/build.yml/badge.svg" alt="Compilazione e prove"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/licenza-GPL--3.0--or--later-blue" alt="Licenza: GPL-3.0-or-later"></a>
</p>

<p align="center">
  <a href="README.md">English</a> · <b>Italiano</b>
</p>

<p align="center">
  <img src="docs/screenshots/home-portrait.png" alt="La home, in verticale" height="400">
  &nbsp;
  <img src="docs/screenshots/home.png" alt="La home, in orizzontale" height="400">
</p>

Foyer Panel è il firmware di un pannello da 10,1″ appeso in ingresso:
mostra la casa e la comanda, e non fa altro. È scritto in C con
[LVGL 9](https://lvgl.io) su ESP-IDF 5.5, e parla con Home Assistant dalla
sua API WebSocket: si iscrive agli stati una volta e ridisegna solo ciò che
è cambiato, nel momento in cui cambia. Sul pannello non c'è un browser e
dietro non c'è una dashboard — le schermate sono il firmware, e quello che
ci compare viene da un `config.json` che si modifica da una pagina servita
dal pannello stesso.

Lo stesso codice fa girare il pannello in verticale (800×1280) e in
orizzontale (1280×800): l'orientamento è un parametro di compilazione, non
una variante del progetto.

È un progetto personale, non il prodotto di un'azienda. Il pannello
verticale è avvitato a un muro di casa e si usa tutti i giorni: quasi tutto
quello che c'è qui dentro c'è perché è servito, e quello che manca manca
perché non è servito ancora.

## Perché non un tablet

Un tablet a muro con una dashboard nel browser funziona, e per molti è la
risposta giusta. Questo progetto nasce da tre cose che, per quella strada,
restano.

- **La dashboard diventa un secondo progetto da mantenere.** Schede, temi,
  componenti presi da fuori, e ogni tanto un aggiornamento che ne cambia
  una e un pomeriggio speso a rimetterla a posto. Qui si configura **quali**
  entità mostrare, non **come** disegnarle.
- **Un browser non è fatto per restare aperto per mesi.** Ricarica, perde
  la sessione, chiede di aggiornarsi, tiene una barra dove non serve; e
  sotto c'è un sistema operativo con idee sue su quando spegnere lo
  schermo. Il pannello fa una cosa sola, da quando prende corrente.
- **Fra il dito e il comando c'è meno strada.** Il tocco arriva al task
  grafico, il comando parte sulla WebSocket già aperta, e lo stato nuovo
  torna sullo stesso collegamento.

In cambio si perde la libertà del browser: una schermata nuova vuole del C
e una ricompilazione, non una scheda incollata in un file YAML. È uno
scambio fatto apposta, ed è il motivo per cui la specifica in
[`docs/`](docs/) viene prima del codice: se cambiare l'interfaccia costa,
conviene sapere prima cosa si vuole.

## Cosa fa

- **Lo stato arriva, non si va a chiedere.** Il pannello si iscrive agli
  eventi di Home Assistant: nessun ciclo di interrogazioni, e ogni comando
  mostra il suo esito.
- **Dove non sa, non finge di sapere.** Un cancello senza sensore di stato
  lo dichiara invece di disegnare un lucchetto a caso, e un dispositivo che
  ha smesso di rispondere non viene fatto passare per spento.
- **Le schermate che servono in un ingresso.** Presenza, energia, aperture
  e luci, riscaldamento e condizionatori, accessi, programmazioni, robot,
  lavatrice e asciugatrice, agenda del giorno, Wi-Fi ospiti come codice QR.
  Sono tutte qui sotto. Una sezione che non ha niente da mostrare non
  compare.
- **Uno standby da termostato.** Ora e temperatura della stanza in grande,
  su nero puro, con la retroilluminazione al minimo e il contenuto che si
  sposta di qualche pixel ogni tre minuti, così un anno a muro non segna lo
  schermo.
- **Si configura da una pagina servita dal pannello stesso.** Si apre solo
  toccando il vetro e si richiude da sola dopo quindici minuti: essere
  sulla rete di casa non basta. Ha validazione, annulla, esportazione e
  importazione; e un Wi-Fi nuovo si prova tenendo da parte le credenziali
  che funzionavano — se entro un minuto non arriva un indirizzo, tornano
  quelle.
- **Con le tue parole.** Inglese e italiano già dentro, e ogni testo del
  pannello e della pagina si riscrive dalla pagina, in qualunque lingua i
  caratteri sappiano disegnare.
- **I segreti si scrivono, non si rileggono.** Le password del Wi-Fi e il
  token di Home Assistant stanno nella NVS del chip. La pagina può
  cambiarli, non conoscerli: non tornano indietro dall'API, non finiscono
  nel registro che si manda a chi aiuta, non escono nella configurazione
  esportata.
- **Aggiornamenti che si possono disfare.** Un'immagine firmata, caricata
  dalla pagina, scritta nella partizione che non sta girando. Poi il
  firmware nuovo parte in prova e si conferma solo dopo aver visto le tre
  cose per cui questo apparecchio esiste — rete su, Home Assistant che
  risponde con la casa, un fotogramma disegnato. Se entro due minuti non le
  ha viste, il bootloader rimette quello di prima e nessuno deve fare
  niente.
- **Gira anche senza una casa.** Un simulatore da desktop disegna ogni
  schermata con dati dimostrativi, alla risoluzione esatta del pannello.
  Tutte le immagini qui sotto vengono da lì.
- **Scritto a partire da una specifica.** I documenti in [`docs/`](docs/)
  sono il primo commit del repository, e cambiano nello stesso commit del
  codice che descrivono. Trentaquattro prove girano con `ctest`, senza
  bisogno di hardware.

## Le schermate

<table>
  <tr>
    <td width="50%"><img src="docs/screenshots/standby.png" alt="Standby"><br><sub><b>Standby</b>: ora e temperatura della stanza, cosa è aperto, l'energia a colpo d'occhio</sub></td>
    <td width="50%"><img src="docs/screenshots/energy.png" alt="Energia"><br><sub><b>Energia</b>: sole, casa, rete, batteria, e chi sta consumando adesso</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/lights.png" alt="Luci"><br><sub><b>Luci</b>: le zone, con accensione e luminosità</sub></td>
    <td><img src="docs/screenshots/climate.png" alt="Riscaldamento"><br><sub><b>Riscaldamento</b>: zone per piano, temperatura richiesta e umidità</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/air-conditioners.png" alt="Condizionatori"><br><sub><b>Condizionatori</b>: tutte le unità in una schermata</sub></td>
    <td><img src="docs/screenshots/air-conditioner.png" alt="Un condizionatore"><br><sub><b>Un condizionatore</b>: modalità, ventilazione, deflettore, timer di spegnimento</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/access.png" alt="Accessi"><br><sub><b>Accessi</b>: cancelli e garage, a pressione prolungata dove conta</sub></td>
    <td><img src="docs/screenshots/schedules.png" alt="Programmazioni"><br><sub><b>Programmazioni</b>: le fasce orarie, modificabili dal vetro</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/robot.png" alt="Robot aspirapolvere"><br><sub><b>Robot aspirapolvere</b>: pulizia stanza per stanza, avvisi, manutenzione</sub></td>
    <td><img src="docs/screenshots/switches.png" alt="Interruttori"><br><sub><b>Interruttori</b>: prese ed elettrodomestici, con la potenza che stanno assorbendo</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/calendar.png" alt="Agenda"><br><sub><b>Agenda</b>: oggi e i prossimi giorni, dai calendari di Home Assistant</sub></td>
    <td><img src="docs/screenshots/wifi-sharing.png" alt="Wi-Fi ospiti"><br><sub><b>Wi-Fi ospiti</b>: si inquadra il codice e ci si collega, senza dettare la password</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/boot.png" alt="Avvio"><br><sub><b>Avvio</b>: ogni passo verificato, e sola lettura se uno non risponde</sub></td>
    <td><img src="docs/screenshots/information.png" alt="Impostazioni"><br><sub><b>Impostazioni</b>: sul pannello solo ciò che serve quando la rete non va</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/page.png" alt="Pagina di configurazione"><br><sub><b>Pagina di configurazione</b>: servita dal pannello, aperta dal vetro, per quindici minuti</sub></td>
    <td><img src="docs/screenshots/page-translations.png" alt="Traduzioni"><br><sub><b>Traduzioni</b>: ogni testo del pannello e della pagina, riscrivibile qui</sub></td>
  </tr>
</table>

Le schermate sono in inglese; il pannello parla la lingua scelta in
configurazione.

## Provarlo, prima di comprare qualcosa

Il simulatore disegna le stesse schermate del pannello, alla stessa
risoluzione, partendo dalla stessa configurazione. Gira su Linux o su WSL2,
con SDL2:

```bash
sudo apt install -y build-essential cmake ninja-build libsdl2-dev python3
```

```bash
cmake -B build -G Ninja && cmake --build build
```

```bash
./build/pannello --profilo p4-1280x800 --lingua it
```

Senza altro mostra dati dimostrativi. Con `--ha` si collega a un'istanza
vera e comanda la casa come farebbe il pannello; con `--web` serve la
pagina di configurazione sulla porta 8080, che è il modo più rapido per
vedere come si configura senza avere niente in mano; `--aiuto` elenca il
resto.

Le prove si lanciano con `ctest --test-dir build --output-on-failure`.

## Hardware

| | |
|---|---|
| Scheda | Guition JC8012P4A1C |
| SoC | ESP32-P4, 32 MB di PSRAM, 16 MB di flash |
| Display | IPS da 10,1″, 800×1280, MIPI-DSI (JD9365) |
| Tocco | capacitivo, GSL3680 |
| Radio | coprocessore ESP32-C6 via esp_hosted: Wi-Fi 6 (il Bluetooth c'è, ma il firmware non lo usa) |

L'orizzontale è lo stesso vetro girato di lato, con lo specchiamento fatto
in hardware.

### Supporto da parete

Una cornice da stampare in 3D per appendere il pannello in verticale:
[`3D Printable wall mount/`](3D%20Printable%20wall%20mount/). Misura circa
243 × 159 × 23 mm e si appende a tre viti attraverso le asole a goccia; il
display si fissa alle sei alette intorno alla cornice.

<p align="center">
  <img src="docs/screenshots/wall-mount.png" alt="Il supporto da parete da stampare in 3D, visto davanti e dal lato del muro" width="640">
</p>

### Metterlo su un pannello

Per compilare il firmware e scriverlo su una scheda nuova ci sono la
[guida di sviluppo](docs/sviluppo.md) e la
[guida al primo avvio](docs/13-primo-avvio-p4.md), che elenca anche cosa
guardare se il pannello non parte. Da lì in avanti si aggiorna via OTA,
dalla pagina di configurazione.

Le schede di riepilogo della home — chi è in casa, le aperture, l'energia —
leggono qualche sensore template invece delle entità grezze. Il pacchetto
da copiare dentro Home Assistant sta in
[`docs/06-ha-package.yaml`](docs/06-ha-package.yaml): gli identificativi
delle entità lì dentro sono inventati, e si sostituiscono con i propri.

## Com'è fatto

La specifica è venuta prima ed è il primo commit del repository. È ancora
lì, ancora vera, e tenuta così apposta: una modifica al comportamento
cambia il documento che la descrive nello stesso commit.

Due regole fanno quasi tutto il lavoro.

**Un solo file può contenere una misura.** `main/profile.h` è l'unico
sorgente a cui è permesso avere dentro un numero di layout, ed è generato
da `docs/profili.json`. Dappertutto altrove il codice chiede al profilo:

```c
lv_obj_set_width(rail, PRF->geo.rail_w);   /* si chiede al profilo */
lv_obj_set_width(rail, 96);                /* mai */
```

Senza quel vincolo i due orientamenti divergono in poche settimane e ogni
correzione va fatta due volte.

**Le prove controllano i documenti contro il codice.** Sono trentaquattro,
girano con `ctest`, e nessuna ha bisogno di hardware. Alcune sono normali:
l'heap dopo aver navigato tutte le schermate nei due orientamenti, le
temporizzazioni con l'orologio mandato avanti. Quelle utili sono le altre,
quelle che prendono ciò che nessuno nota:

| Prova | Cosa impedisce |
|---|---|
| `numeri_di_layout` | una misura di layout scritta fuori da `profile.h` |
| `citazioni_ai_documenti` | un commento nel codice che cita la sezione di un documento che non esiste — o peggio, che esiste e dice un'altra cosa |
| `config_paths_in_schema` | una chiave di configurazione che il codice legge e lo schema non ha: l'impostazione che hai cambiato non arriva mai |
| `pagina_copre_lo_schema` | una chiave dello schema che nessuna sezione della pagina mostra, e quindi una funzione che non si accende mai |
| `page_texts` | un testo che la pagina chiede e che nessuna lingua ha, e che compare come chiave grezza su una pagina che sembra finita |

Ognuna è stata scritta dopo che la cosa che controlla era già successa una
volta.

## Cosa non fa

Sta scritto qui perché un limite che si trova nel README è un limite; uno
che si trova dopo aver comprato la scheda è una lamentela.

- **La pagina del pannello non ha TLS.** Le credenziali che ci si scrivono
  passano in chiaro sulla rete locale. Il collegamento a Home Assistant può
  usare TLS; la pagina no, e
  [`docs/12-sicurezza.md`](docs/12-sicurezza.md) lo dice con queste stesse
  parole.
- **Chi arriva fisicamente al pannello arriva a tutto.** La NVS non è
  cifrata: con un cavo USB si riscrive il firmware e si leggono i segreti.
  È una scelta, non una dimenticanza: è un pannello avvitato a un muro
  dentro una casa.
- **`diagnostics.config_always_open`, mentre è accesa**, toglie del tutto
  la barriera davanti alla pagina di configurazione. È una deroga
  dichiarata, da spegnere quando il pannello è a regime.
- **Una scheda sola, in due orientamenti.** Un altro pannello vuole un
  profilo nuovo e, quasi certamente, un altro driver del vetro.
- **I documenti sono in italiano.** Il codice, l'interfaccia, la pagina di
  configurazione e il README inglese sono in inglese.

## Documentazione

Si parte da [`docs/README.md`](docs/README.md).

| | |
|---|---|
| [Guida di sviluppo](docs/sviluppo.md) | ambiente, compilazione, simulatore, prove, aggiornamenti, licenze |
| [Specifica dell'interfaccia](docs/01-specifica-ui.md) | ogni schermata, stato e temporizzazione |
| [Architettura del firmware](docs/05-architettura-firmware.md) | task, memoria, partizioni, OTA |
| [Sicurezza](docs/12-sicurezza.md) | cosa protegge il pannello, da chi, e cosa non protegge |
| [Contratto della configurazione](docs/03-config-contratto.md) | `config.json`, l'API, le migrazioni, la validazione |
| [Configurazione d'esempio](docs/04-config.example.json) | completa, con entità inventate |
| [Pacchetto per Home Assistant](docs/06-ha-package.yaml) | i sensori aggregati che il pannello legge |
| [Profili](docs/09-profili.md) | tutte le differenze fra i due orientamenti, in un posto solo |

## Stato

I due orientamenti sono completi, e il pannello verticale è appeso a un
muro e in uso tutti i giorni. Le prossime lingue dell'interfaccia sono
francese, tedesco e spagnolo.

## Il nome

*Foyer* è l'ingresso di casa: il posto dove il pannello sta appeso, e il
punto da cui si accede alla casa e ai suoi sistemi. In origine la parola
voleva dire anche *focolare*, il centro della casa. Le due cose insieme
sono quello che questo progetto vuole essere.

## Licenza

[GPL-3.0-or-later](LICENSE). Il codice di terzi mantiene la sua licenza:
l'elenco sta nella [guida di sviluppo](docs/sviluppo.md#licenza).
