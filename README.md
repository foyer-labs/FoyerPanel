<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/logo/foyer-dark.svg">
    <img src="docs/logo/foyer-light.svg" alt="Foyer Panel" width="420">
  </picture>
</p>

<p align="center">
  <b>Foyer Panel</b> — a Home Assistant wall panel that runs as firmware, not as a browser.<br>
  10.1″ ESP32-P4, 800×1280, native C with LVGL 9. One codebase, portrait and landscape.
</p>

<p align="center">
  <a href="https://github.com/foyer-labs/FoyerPanel/actions/workflows/build.yml"><img src="https://github.com/foyer-labs/FoyerPanel/actions/workflows/build.yml/badge.svg" alt="Build and tests"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/licence-GPL--3.0--or--later-blue" alt="Licence: GPL-3.0-or-later"></a>
</p>

<p align="center">
  <b>English</b> · <a href="README.it.md">Italiano</a>
</p>

<p align="center">
  <img src="docs/screenshots/home-portrait.png" alt="Home screen, portrait" height="400">
  &nbsp;
  <img src="docs/screenshots/home.png" alt="Home screen, landscape" height="400">
</p>

Foyer Panel is the firmware of a 10.1″ panel that hangs by the door: it
shows the house and it commands it, and it does nothing else. It is written
in C with [LVGL 9](https://lvgl.io) on ESP-IDF 5.5, and it talks to Home
Assistant through its WebSocket API: it subscribes to the states once and
redraws only what changed, the moment it changes. There is no browser on
the panel and no dashboard behind it — the screens are the firmware, and
what appears on them comes from a `config.json` you edit from a page the
panel serves itself.

The same code runs the panel in portrait (800×1280) and in landscape
(1280×800). The orientation is a build parameter, not a fork of the
project.

It is a personal project, not a product. The portrait panel is screwed to a
wall at home and used every day: nearly everything in here is here because
it was needed, and what is missing is missing because it has not been
needed yet.

## Why not a tablet

A tablet on the wall with a dashboard in the browser works, and for many
people it is the right answer. This project comes from three things that,
down that road, stay as they are.

- **The dashboard becomes a second project to maintain.** Cards, themes,
  components from elsewhere, and every so often an update that changes one
  of them and an afternoon spent putting it back. Here you configure
  **which** entities to show, not **how** to draw them.
- **A browser is not made to stay open for months.** It reloads, it loses
  the session, it asks to update, it keeps a bar where you do not want one;
  and underneath it there is an operating system with its own ideas about
  when to turn the screen off. The panel does one thing, from the moment it
  gets power.
- **There is less road between the finger and the command.** The touch
  reaches the graphics task, the command leaves on the WebSocket that is
  already open, and the new state comes back on the same connection.

What you give up is the freedom of the browser: a new screen means C and a
rebuild, not a card pasted into a YAML file. It is a deliberate trade, and
it is the reason the specification in [`docs/`](docs/) comes before the
code: when changing the interface costs something, it pays to know first
what you want.

## What it does

- **State arrives; nobody goes asking for it.** The panel subscribes to
  Home Assistant's events: no polling loop, and every command shows its
  outcome.
- **Where it does not know, it does not pretend to.** A gate with no state
  sensor says so instead of drawing a padlock at random, and a device that
  stopped answering is not passed off as switched off.
- **The screens an entrance needs.** Who is home, energy, openings and
  lights, heating and air conditioners, gates and garage, schedules, a
  robot vacuum, washer and dryer, the day's calendar, guest Wi-Fi as a QR
  code. They are all below. A section with nothing to show does not appear.
- **A standby like a thermostat's.** Time and room temperature in large
  type on pure black, the backlight down, and the content shifted a few
  pixels every three minutes so a year on a wall does not mark the screen.
- **Configured from a page the panel serves itself.** It opens only by
  touching the glass and closes again on its own after fifteen minutes:
  being on the house network is not enough. It has validation, undo, export
  and import; and a new Wi-Fi network is tried while the credentials that
  worked are kept aside — if no address arrives within a minute, those come
  back.
- **Your words.** English and Italian are built in, and every text of the
  panel and of the page can be rewritten from the page, in any language the
  bundled fonts can draw.
- **Secrets are written, never read back.** The Wi-Fi passwords and the
  Home Assistant token live in the chip's NVS. The page can change them,
  not know them: they do not come back from the API, they are not in the
  log you send to whoever is helping you, and they are not in the exported
  configuration.
- **Updates that undo themselves.** A signed image, uploaded from the page,
  written to the partition that is not running. Then the new firmware
  starts on trial and confirms itself only once it has seen the three
  things this device exists for — network up, Home Assistant answering with
  the house, a frame drawn. If it has not seen them within two minutes, the
  bootloader puts the old one back and nobody has to do anything.
- **It runs without a house.** A desktop simulator draws every screen with
  demo data, at the panel's exact resolution. Every screenshot below came
  out of it.
- **Written from a specification.** The documents in [`docs/`](docs/) are
  the repository's first commit, and they change in the same commit as the
  code they describe. Thirty-four tests run under `ctest`, without any
  hardware.

## The screens

<table>
  <tr>
    <td width="50%"><img src="docs/screenshots/standby.png" alt="Standby"><br><sub><b>Standby</b> — time and room temperature, what is open, energy at a glance</sub></td>
    <td width="50%"><img src="docs/screenshots/energy.png" alt="Energy"><br><sub><b>Energy</b> — sun, house, grid, battery, and what is drawing power right now</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/lights.png" alt="Lights"><br><sub><b>Lights</b> — the zones, with on/off and brightness</sub></td>
    <td><img src="docs/screenshots/climate.png" alt="Heating"><br><sub><b>Heating</b> — zones by floor, target temperature and humidity</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/air-conditioners.png" alt="Air conditioners"><br><sub><b>Air conditioners</b> — every unit on one screen</sub></td>
    <td><img src="docs/screenshots/air-conditioner.png" alt="One air conditioner"><br><sub><b>One air conditioner</b> — mode, fan, vane, off timer</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/access.png" alt="Access"><br><sub><b>Access</b> — gates and garage, hold to open where it matters</sub></td>
    <td><img src="docs/screenshots/schedules.png" alt="Schedules"><br><sub><b>Schedules</b> — the time slots, edited on the glass</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/robot.png" alt="Robot vacuum"><br><sub><b>Robot vacuum</b> — room-by-room cleaning, alerts, maintenance</sub></td>
    <td><img src="docs/screenshots/switches.png" alt="Switches"><br><sub><b>Switches</b> — sockets and appliances, with the power they are drawing</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/calendar.png" alt="Calendar"><br><sub><b>Calendar</b> — today and the days ahead, from Home Assistant's calendars</sub></td>
    <td><img src="docs/screenshots/wifi-sharing.png" alt="Guest Wi-Fi"><br><sub><b>Guest Wi-Fi</b> — scan the code and connect, without dictating the password</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/boot.png" alt="Boot"><br><sub><b>Boot</b> — each step checked, and read-only mode if one does not answer</sub></td>
    <td><img src="docs/screenshots/information.png" alt="Settings"><br><sub><b>Settings</b> — on the panel, only what you need when the network is down</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/page.png" alt="Configuration page"><br><sub><b>Configuration page</b> — served by the panel, opened from the glass, for fifteen minutes</sub></td>
    <td><img src="docs/screenshots/page-translations.png" alt="Translations"><br><sub><b>Translations</b> — every text of the panel and of the page, rewritable here</sub></td>
  </tr>
</table>

## Try it before buying anything

The simulator draws the same screens as the panel, at the same resolution,
from the same configuration. It runs on Linux or on WSL2, with SDL2:

```bash
sudo apt install -y build-essential cmake ninja-build libsdl2-dev python3
```

```bash
cmake -B build -G Ninja && cmake --build build
```

```bash
./build/pannello --profilo p4-1280x800
```

With nothing else it shows demo data. `--ha` connects it to a real instance
and commands the house as the panel would; `--web` serves the configuration
page on port 8080, which is the quickest way to see how it is set up
without having anything in your hands; `--aiuto` lists the rest. The flags
are Italian, like the source: `--profilo` is the panel profile, `--lingua`
the interface language.

The tests are `ctest --test-dir build --output-on-failure`.

## Hardware

| | |
|---|---|
| Board | Guition JC8012P4A1C |
| SoC | ESP32-P4, 32 MB PSRAM, 16 MB flash |
| Display | 10.1″ IPS, 800×1280, MIPI-DSI (JD9365) |
| Touch | capacitive, GSL3680 |
| Radio | ESP32-C6 co-processor over esp_hosted: Wi-Fi 6 (Bluetooth is there, the firmware does not use it) |

Landscape is the same glass turned sideways, mirrored in hardware.

### Wall mount

A 3D-printable frame that hangs the panel in portrait:
[`3D Printable wall mount/`](3D%20Printable%20wall%20mount/). It measures
about 243 × 159 × 23 mm and hangs on three screws through keyhole slots;
the display is fixed to the six small tabs around the frame.

<p align="center">
  <img src="docs/screenshots/wall-mount.png" alt="The 3D-printable wall mount, from the front and from the wall side" width="640">
</p>

### Putting it on a panel

To build the firmware and write it to a new board there are the
[developer guide](docs/sviluppo.md) and the
[first-boot guide](docs/13-primo-avvio-p4.md), which also lists what to
look at when the panel does not start. From then on it updates over the
air, from the configuration page.

The home's summary cards — who is home, the openings, the energy — read a
few template sensors rather than raw entities. There is a package to copy
into Home Assistant in [`docs/06-ha-package.yaml`](docs/06-ha-package.yaml);
the entity ids in it are invented, and you replace them with yours.

## How it is built

The specification came first and is the repository's first commit. It is
still there, still true, and kept that way on purpose: a change in
behaviour edits the document that describes it in the same commit.

Two rules do most of the work.

**One file may hold a measurement.** `main/profile.h` is the only source
file allowed to contain a layout number, and it is generated from
`docs/profili.json`. Everywhere else the code asks the profile:

```c
lv_obj_set_width(rail, PRF->geo.rail_w);   /* ask the profile */
lv_obj_set_width(rail, 96);                /* never */
```

Without that constraint the two orientations drift apart in a few weeks and
every fix has to be made twice.

**The tests check the documents against the code.** There are thirty-four,
they run under `ctest`, and none of them needs hardware. Some are ordinary:
the heap after walking every screen in both orientations, the timings with
the clock fast-forwarded. The useful ones are the ones that catch what
nobody notices:

| Test | What it stops |
|---|---|
| `numeri_di_layout` | a layout measurement written outside `profile.h` |
| `citazioni_ai_documenti` | a code comment citing a section of a document that does not exist — or worse, one that exists and says something else |
| `config_paths_in_schema` | a configuration key the code reads and the schema does not have: the setting you changed never arrives |
| `pagina_copre_lo_schema` | a schema key that no section of the configuration page exposes, so the feature never switches on |
| `page_texts` | a text the page asks for and no language has, which shows as a raw key on a page that otherwise looks finished |

Each of them was written after the thing it checks had already happened
once.

## What it does not do

Written here because a limitation you find in the README is a limitation;
one you find after buying the board is a complaint.

- **The panel's own page has no TLS.** The credentials you type into it
  cross the local network in clear. The link to Home Assistant can use TLS;
  the page cannot, and [`docs/12-sicurezza.md`](docs/12-sicurezza.md) says
  so in the same words.
- **Physical access is full access.** NVS is not encrypted: a USB cable
  rewrites the firmware and reads the secrets. It is a choice, not an
  oversight — it is a panel screwed to a wall inside a house.
- **`diagnostics.config_always_open`, while it is on**, removes the barrier
  in front of the configuration page entirely. It is a declared exception,
  to be switched off once the panel is settled.
- **One board, in two orientations.** Another panel needs a new profile
  and, most likely, another display driver.
- **The documents are in Italian.** The code, the interface, the
  configuration page and this README are in English.

## Documentation

Start from [`docs/README.md`](docs/README.md).

| | |
|---|---|
| [Developer guide](docs/sviluppo.md) | environment, build, simulator, tests, updates, licences |
| [UI specification](docs/01-specifica-ui.md) | every screen, state and timing |
| [Firmware architecture](docs/05-architettura-firmware.md) | tasks, memory, partitions, OTA |
| [Security](docs/12-sicurezza.md) | what the panel protects, from whom, and what it does not |
| [Configuration contract](docs/03-config-contratto.md) | `config.json`, the API, migrations, validation |
| [Example configuration](docs/04-config.example.json) | a complete one, with invented entities |
| [Home Assistant package](docs/06-ha-package.yaml) | the aggregate sensors the panel reads |
| [Profiles](docs/09-profili.md) | every difference between the two orientations, in one place |

## Status

Both orientations are complete, and the portrait panel is on a wall and in
daily use. French, German and Spanish are the next interface languages.

## The name

*Foyer* is the entrance of a house: where the panel hangs, and the point
from which you reach the house and its systems. The word once also meant
*the hearth*, the centre of the home. Between the two is what this project
wants to be.

## Licence

[GPL-3.0-or-later](LICENSE). Third-party code keeps its own licence — the
list is in the [developer guide](docs/sviluppo.md#licenza).
