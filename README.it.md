<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/logo/foyer-dark.svg">
    <img src="docs/logo/foyer-light.svg" alt="Foyer Panel" width="420">
  </picture>
</p>

<p align="center">
  <b>Foyer Panel</b>: un pannello da parete per Home Assistant, su un display ESP32-P4 da 10,1″.<br>
  Firmware nativo: niente browser, niente dashboard da mantenere, niente polling.
</p>

<p align="center">
  <a href="README.md">English</a> · <b>Italiano</b>
</p>

<p align="center">
  <img src="docs/screenshots/home-portrait.png" alt="La home, in verticale" height="400">
  &nbsp;
  <img src="docs/screenshots/home.png" alt="La home, in orizzontale" height="400">
</p>

**Foyer Panel** prende il nome dal *foyer*, l'ingresso di casa: il posto
dove il pannello sta appeso, e il punto da cui si accede alla casa e ai suoi
sistemi. In origine la parola voleva dire anche *focolare*, il centro della
casa. Le due cose insieme sono quello che questo progetto vuole essere.

È un firmware scritto in C con [LVGL 9](https://lvgl.io) per ESP-IDF 5.5.
Parla con Home Assistant attraverso la sua API WebSocket e ridisegna solo
ciò che è cambiato, nel momento in cui cambia. Lo stesso codice fa girare
il pannello in orizzontale (1280×800) e in verticale (800×1280):
l'orientamento è un parametro di compilazione, non una variante del progetto.

## In breve

- **Immediato e onesto.** Lo stato arriva da Home Assistant quando cambia.
  Ogni comando mostra il suo esito, e dove un dispositivo non ha un sensore
  di stato il pannello lo dice invece di tirare a indovinare.
- **Tutto quello che serve in un ingresso.** Chi è in casa, l'energia, le
  finestre aperte, le luci, le zone di riscaldamento, i condizionatori,
  cancelli e garage, le programmazioni, il robot aspirapolvere, lavatrice e
  asciugatrice, l'agenda del giorno, il Wi-Fi ospiti come codice QR.
- **Uno standby da termostato.** Ora e temperatura della stanza in grande,
  su nero puro, con la retroilluminazione al minimo e i pixel che si
  spostano per non segnare lo schermo.
- **Si configura da una pagina web servita dal pannello stesso.** Si apre
  solo dal pannello e si richiude da sola dopo 15 minuti. Ha la validazione,
  l'annulla, l'esportazione e l'importazione, e una nuova rete Wi-Fi viene
  provata con una rete di sicurezza prima di essere tenuta.
- **Con le tue parole.** Inglese e italiano già dentro. Ogni testo del
  pannello e della pagina si può riscrivere dalla pagina, in qualunque
  lingua i caratteri sappiano disegnare.
- **I segreti restano segreti.** Le password del Wi-Fi e il token di Home
  Assistant stanno nella NVS del chip e non vengono mai riletti: né dalla
  pagina, né nel registro, né nella configurazione esportata.
- **Aggiornamenti sicuri.** Immagini OTA firmate, caricate dalla pagina, con
  ritorno automatico alla versione precedente e una partizione di recupero.
- **Funziona anche senza una casa.** Un simulatore da desktop disegna ogni
  schermata con dati dimostrativi, alla risoluzione esatta del pannello.
  Tutte le schermate qui sotto vengono da lì.

## Le schermate

<table>
  <tr>
    <td width="50%"><img src="docs/screenshots/standby.png" alt="Standby"><br><sub><b>Standby</b>: ora e temperatura della stanza, cosa è aperto, l'energia a colpo d'occhio</sub></td>
    <td width="50%"><img src="docs/screenshots/lights.png" alt="Luci"><br><sub><b>Luci</b>: zone con accensione e luminosità</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/climate.png" alt="Riscaldamento"><br><sub><b>Riscaldamento</b>: zone per piano, temperatura richiesta e umidità</sub></td>
    <td><img src="docs/screenshots/air-conditioners.png" alt="Condizionatori"><br><sub><b>Condizionatori</b>: tutte le unità in una schermata</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/air-conditioner.png" alt="Un condizionatore"><br><sub><b>Un condizionatore</b>: modalità, ventilazione, deflettore, timer</sub></td>
    <td><img src="docs/screenshots/energy.png" alt="Energia"><br><sub><b>Energia</b>: sole, casa, rete, batteria, e chi sta consumando</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/access.png" alt="Accessi"><br><sub><b>Accessi</b>: cancelli e garage, a pressione prolungata dove conta</sub></td>
    <td><img src="docs/screenshots/schedules.png" alt="Programmazioni"><br><sub><b>Programmazioni</b>: le fasce orarie, modificabili dal vetro</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/robot.png" alt="Robot aspirapolvere"><br><sub><b>Robot aspirapolvere</b>: pulizia stanza per stanza, avvisi, manutenzione</sub></td>
    <td><img src="docs/screenshots/switches.png" alt="Interruttori"><br><sub><b>Interruttori</b>: prese ed elettrodomestici, con la loro potenza</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/calendar.png" alt="Agenda"><br><sub><b>Agenda</b>: oggi e i prossimi giorni, da Home Assistant</sub></td>
    <td><img src="docs/screenshots/wifi-sharing.png" alt="Wi-Fi ospiti"><br><sub><b>Wi-Fi ospiti</b>: si inquadra il codice e ci si collega</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/boot.png" alt="Avvio"><br><sub><b>Avvio</b>: ogni passo verificato, e sola lettura se uno non risponde</sub></td>
    <td><img src="docs/screenshots/information.png" alt="Impostazioni"><br><sub><b>Impostazioni</b>: sul pannello solo ciò che serve quando la rete non va</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/page.png" alt="Pagina di configurazione"><br><sub><b>Pagina di configurazione</b>: servita dal pannello, nel browser</sub></td>
    <td><img src="docs/screenshots/page-translations.png" alt="Traduzioni"><br><sub><b>Traduzioni</b>: ogni testo del pannello e della pagina</sub></td>
  </tr>
</table>

Le schermate sono in inglese; il pannello parla la lingua scelta in
configurazione.

## Hardware

| | |
|---|---|
| Scheda | Guition JC8012P4A1C |
| SoC | ESP32-P4, 32 MB di PSRAM, 16 MB di flash |
| Display | IPS da 10,1″, 800×1280, MIPI-DSI (JD9365) |
| Tocco | capacitivo, GSL3680 |
| Radio | coprocessore ESP32-C6 via esp_hosted: Wi-Fi 6 e Bluetooth LE |

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

## Provarlo

Il simulatore gira su Linux o su WSL2, con SDL2:

```bash
sudo apt install -y build-essential cmake ninja-build libsdl2-dev python3
```

```bash
cmake -B build -G Ninja && cmake --build build
```

```bash
./build/pannello --profilo p4-1280x800 --lingua it
```

Senza Home Assistant mostra dati dimostrativi. Con `--ha` si collega a
un'istanza vera, e con `--web` serve la pagina di configurazione sulla porta
8080.

Per compilare il firmware e scriverlo su un pannello ci sono la
[guida di sviluppo](docs/sviluppo.md) e la
[guida al primo avvio](docs/13-primo-avvio-p4.md).

## Documentazione

Il progetto nasce da una specifica scritta, tenuta allineata al codice. Si
parte da [`docs/README.md`](docs/README.md).

| | |
|---|---|
| [Guida di sviluppo](docs/sviluppo.md) | ambiente, compilazione, simulatore, prove, aggiornamenti, licenze |
| [Specifica dell'interfaccia](docs/01-specifica-ui.md) | ogni schermata, stato e temporizzazione |
| [Contratto della configurazione](docs/03-config-contratto.md) | `config.json`, l'API, le migrazioni, la validazione |
| [Configurazione d'esempio](docs/04-config.example.json) | completa, con entità inventate |
| [Pacchetto per Home Assistant](docs/06-ha-package.yaml) | i sensori aggregati che il pannello legge |
| [Architettura del firmware](docs/05-architettura-firmware.md) | task, memoria, partizioni, OTA |
| [Sicurezza](docs/12-sicurezza.md) | cosa protegge il pannello, e da chi |

## Stato

I due profili sono completi, e il pannello verticale è appeso a un muro e
in uso tutti i giorni. Le prossime lingue dell'interfaccia sono francese,
tedesco e spagnolo.

## Licenza

[GPL-3.0-or-later](LICENSE). Il codice di terzi mantiene la sua licenza:
l'elenco sta nella [guida di sviluppo](docs/sviluppo.md#licenza).
