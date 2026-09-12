<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/logo/foyer-dark.svg">
    <img src="docs/logo/foyer-light.svg" alt="Foyer Panel" width="420">
  </picture>
</p>

<p align="center">
  <b>Foyer Panel</b> — a wall panel for Home Assistant, on a 10.1″ ESP32-P4 display.<br>
  Native firmware — no browser, no dashboard to maintain, no polling.
</p>

<p align="center">
  <b>English</b> · <a href="README.it.md">Italiano</a>
</p>

<p align="center">
  <img src="docs/screenshots/home-portrait.png" alt="Home screen, portrait" height="400">
  &nbsp;
  <img src="docs/screenshots/home.png" alt="Home screen, landscape" height="400">
</p>

**Foyer Panel** takes its name from the *foyer*, the entrance of a house —
where the panel hangs, and the point from which you reach the house and its
systems. The word once also meant *the hearth*, the centre of the home.
That is what this project wants to be.

It is firmware written in C with [LVGL 9](https://lvgl.io) for ESP-IDF 5.5.
It talks to Home Assistant over its WebSocket API and redraws only what
changed, the moment it changes. The same code runs the panel in landscape
(1280×800) and portrait (800×1280): the orientation is a build parameter,
not a fork.

## Highlights

- **Instant and honest.** State arrives from Home Assistant as it changes.
  A command shows its outcome, and where a device has no state sensor the
  panel says so instead of guessing.
- **Everything a hallway needs.** People at home, energy, open windows,
  lights, heating zones, air conditioners, gates and garage, schedules, a
  robot vacuum, washer and dryer, the day's calendar, guest Wi-Fi as a QR code.
- **A thermostat-like standby.** Time and room temperature in large type on
  pure black, the backlight dimmed, pixels shifted to avoid burn-in.
- **Configured from a web page the panel serves itself.** It is opened only
  from the panel and closes on its own after 15 minutes. Validation, undo,
  export and import are included, and a new Wi-Fi network is tried with a
  safety net before it is kept.
- **Your words.** English and Italian built in. Every label on the panel and
  the page can be rewritten from the page, in any language the fonts can draw.
- **Secrets stay secret.** Wi-Fi passwords and the Home Assistant token live
  in the chip's NVS and are never read back — not by the page, not in the
  log, not in the exported configuration.
- **Safe updates.** Signed OTA images, uploaded from the page, with automatic
  rollback and a recovery partition.
- **Works without a house.** A desktop simulator draws every screen, with
  demo data, at the panel's exact resolution. All the screenshots here come
  from it.

## Screens

<table>
  <tr>
    <td width="50%"><img src="docs/screenshots/standby.png" alt="Standby"><br><sub><b>Standby</b> — time and room temperature, what is open, energy at a glance</sub></td>
    <td width="50%"><img src="docs/screenshots/lights.png" alt="Lights"><br><sub><b>Lights</b> — zones with on/off and brightness</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/climate.png" alt="Heating"><br><sub><b>Heating</b> — zones by floor, setpoint and humidity</sub></td>
    <td><img src="docs/screenshots/air-conditioners.png" alt="Air conditioners"><br><sub><b>Air conditioners</b> — every unit on one screen</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/air-conditioner.png" alt="One air conditioner"><br><sub><b>One air conditioner</b> — mode, fan, vane, off timer</sub></td>
    <td><img src="docs/screenshots/energy.png" alt="Energy"><br><sub><b>Energy</b> — solar, house, grid, battery, and who is using power</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/access.png" alt="Access"><br><sub><b>Access</b> — gates and garage, with hold-to-open where it matters</sub></td>
    <td><img src="docs/screenshots/schedules.png" alt="Schedules"><br><sub><b>Schedules</b> — time slots edited on the glass</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/robot.png" alt="Robot vacuum"><br><sub><b>Robot vacuum</b> — room-by-room cleaning, alerts, maintenance</sub></td>
    <td><img src="docs/screenshots/switches.png" alt="Switches"><br><sub><b>Switches</b> — sockets and appliances, with their power</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/calendar.png" alt="Calendar"><br><sub><b>Calendar</b> — today and the days ahead, from Home Assistant</sub></td>
    <td><img src="docs/screenshots/wifi-sharing.png" alt="Guest Wi-Fi"><br><sub><b>Guest Wi-Fi</b> — scan the code to connect</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/boot.png" alt="Boot"><br><sub><b>Boot</b> — each step checked, and read-only mode if one fails</sub></td>
    <td><img src="docs/screenshots/information.png" alt="Settings"><br><sub><b>Settings</b> — on the panel, only what you need when the network is down</sub></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/page.png" alt="Configuration page"><br><sub><b>Configuration page</b> — served by the panel, in the browser</sub></td>
    <td><img src="docs/screenshots/page-translations.png" alt="Translations"><br><sub><b>Translations</b> — every text of the panel and the page</sub></td>
  </tr>
</table>

## Hardware

| | |
|---|---|
| Board | Guition JC8012P4A1C |
| SoC | ESP32-P4, 32 MB PSRAM, 16 MB flash |
| Display | 10.1″ IPS, 800×1280, MIPI-DSI (JD9365) |
| Touch | capacitive, GSL3680 |
| Radio | ESP32-C6 co-processor over esp_hosted: Wi-Fi 6 and Bluetooth LE |

Landscape is the same glass turned sideways, mirrored in hardware.

### Wall mount

A 3D-printable frame that hangs the panel in portrait:
[`3D Printable wall mount/`](3D%20Printable%20wall%20mount/). It measures
about 243 × 159 × 23 mm and hangs on three screws through keyhole slots;
the display is fixed to the six small tabs around the frame.

<p align="center">
  <img src="docs/screenshots/wall-mount.png" alt="The 3D-printable wall mount, from the front and from the wall side" width="640">
</p>

## Try it

The simulator runs on Linux or WSL2 with SDL2:

```bash
sudo apt install -y build-essential cmake ninja-build libsdl2-dev python3
```

```bash
cmake -B build -G Ninja && cmake --build build
```

```bash
./build/pannello --profilo p4-1280x800
```

Without Home Assistant it shows demo data. With `--ha` it connects to a real
instance, and `--web` serves the configuration page on port 8080.

To build the firmware and flash a panel, see the
[developer guide](docs/sviluppo.md) and the
[first-boot guide](docs/13-primo-avvio-p4.md).

## Documentation

The project is built from a written specification that is kept in step with
the code. Start from [`docs/README.md`](docs/README.md).

| | |
|---|---|
| [Developer guide](docs/sviluppo.md) | environment, build, simulator, tests, updates, licences |
| [UI specification](docs/01-specifica-ui.md) | every screen, state and timing |
| [Configuration contract](docs/03-config-contratto.md) | `config.json`, the API, migrations, validation |
| [Example configuration](docs/04-config.example.json) | a complete one, with made-up entities |
| [Home Assistant package](docs/06-ha-package.yaml) | the aggregate sensors the panel reads |
| [Firmware architecture](docs/05-architettura-firmware.md) | tasks, memory, partitions, OTA |
| [Security](docs/12-sicurezza.md) | what the panel protects, and from whom |

The documents are in Italian for now; English versions are on the way.

## Status

Both profiles are complete, and the portrait panel is in daily use on a
wall. More interface languages (French, German, Spanish) are next.

## License

[GPL-3.0-or-later](LICENSE). Third-party code keeps its own licence — the
list is in the [developer guide](docs/sviluppo.md#licenza).
