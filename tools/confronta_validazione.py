#!/usr/bin/env python3
"""
Confronta il validatore del firmware con quello Python.

    python tools/confronta_validazione.py --binario build/prova_config

`config.schema.json` e la fonte autorevole, ma il firmware non lo esegue: un
validatore JSON Schema sull'ESP32 costa piu di quanto valga. Il controllo a
bordo e quindi scritto a mano, e 03-config-contratto.md dice che deve
corrispondere allo schema **campo per campo**.

Niente lo impone automaticamente. Lo impone questa prova: prende la
configurazione vera della casa, ne genera una sessantina di varianti — una
per ogni campo, portato fuori dal suo intervallo, del tipo sbagliato o
tolto — e pretende che i due validatori diano lo stesso verdetto. Dove non
lo danno, la differenza si vede subito e si sa da che parte correggere.

Serve `jsonschema` per il lato Python:  pip install jsonschema
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
ESEMPIO = RADICE / "docs" / "04-config.example.json"
VERIFICA = RADICE / "tools" / "verifica_config.py"


# Una rete condivisibile che va bene: serve a costruire i casi in cui a
# essere sbagliato e il **numero** di reti, non il loro contenuto.
RETE_VALIDA = {
    "enabled": False,
    "display_name": "Rete",
    "ssid": "UNA_RETE",
    "security": "WPA",
}


def dentro(d: dict, percorso: str):
    """Il contenitore e la chiave finale di un percorso con la barra."""
    pezzi = percorso.split("/")
    nodo = d
    for p in pezzi[:-1]:
        nodo = nodo[int(p)] if isinstance(nodo, list) else nodo[p]
    return nodo, pezzi[-1]


def muta(base: dict, percorso: str, valore):
    """Copia con un campo cambiato. `valore` a None toglie il campo."""
    import copy
    d = copy.deepcopy(base)
    nodo, chiave = dentro(d, percorso)
    if valore is None:
        if isinstance(nodo, list):
            del nodo[int(chiave)]
        else:
            nodo.pop(chiave, None)
    else:
        if isinstance(nodo, list):
            nodo[int(chiave)] = valore
        else:
            nodo[chiave] = valore
    return d


# Le varianti da provare. Ogni riga: percorso, valore, e un nome per il
# referto. Coprono un campo per tipo di controllo — intervallo, tipo, enum,
# obbligatorieta, forma di un entity_id, cardinalita — piu tutte le regole
# fra campi di 03-config-contratto.md §10.
VARIANTI: list[tuple[str, object, str]] = [
    ("schema", None, "schema mancante"),
    ("schema", "due", "schema non numerico"),

    ("system/panel_name", "", "nome del pannello vuoto"),
    ("system/panel_name", None, "nome del pannello mancante"),
    ("system/profile", "p4-9999x9999", "profilo inventato"),
    ("system/profile", None, "profilo mancante"),
    ("system/timezone", None, "fuso orario mancante"),
    ("system/language", "xx", "lingua che non esiste"),
    ("system/language", 7, "lingua non testuale"),

    ("display/brightness_active", 101, "luminosita oltre 100"),
    ("display/brightness_active", -1, "luminosita negativa"),
    ("display/brightness_active", "molta", "luminosita non numerica"),
    ("display/brightness_active", None, "luminosita mancante"),
    ("display/brightness_standby", 99, "standby piu luminoso dell'attivo"),
    ("display/standby_after_s", 5, "standby sotto il minimo"),
    ("display/standby_after_s", 99999, "standby oltre il massimo"),
    ("display/off_after_s", 30, "spegnimento prima dello standby"),
    ("display/return_home_after_s", 5, "ritorno alla home troppo corto"),
    ("display/long_press_ms", 100, "pressione prolungata troppo corta"),

    ("home_assistant/host", "http://192.0.2.10", "host con lo schema"),
    ("home_assistant/host", None, "host mancante"),
    ("home_assistant/port", 0, "porta zero"),
    ("home_assistant/port", 70000, "porta oltre il massimo"),


    ("access/0/id", "PEDONALE", "id di accesso con maiuscole"),
    ("access/0/type", "pulsante", "tipo di accesso inventato"),
    ("access/0/entity", "pedonale", "entita senza dominio"),
    ("access/0/entity", None, "entita mancante"),
    ("access/0/pulse_ms", None, "impulso senza durata"),
    ("access/0/pulse_ms", 100, "impulso troppo breve"),
    ("access/2/state_sensor", "sensor.qualcosa", "sensore di stato non binary"),

    ("lights/zones/0/entity", "switch.salotto", "zona luci che non e un light"),
    ("lights/zones/0/name", "", "nome di zona vuoto"),
    ("lights/zones/0/entity", None, "entita di zona mancante"),

    ("climate/heating/0/climate", "sensor.salotto", "zona clima non climate"),
    ("climate/heating/0/name", None, "nome di zona clima mancante"),
    ("climate/air_conditioners/0/climate", "switch.salotto", "condizionatore non climate"),
    ("climate/air_conditioners/0/id", "Salotto", "id di condizionatore con maiuscole"),
    ("climate/heating_limits/max", 10, "limiti con min oltre max"),
    ("climate/heating_limits/min", 2, "limite minimo sotto i 5 gradi"),
    ("climate/ac_limits/max", 40, "limite massimo sopra i 35 gradi"),
    ("climate/heating_limits/step", 0.3, "passo non ammesso"),

    ("energy/aggregate_sensor", "switch.energia", "sensore energia non sensor"),
    ("energy/aggregate_sensor", None, "sensore energia mancante"),
    ("energy/signs/grid_positive", "boh", "verso della rete inventato"),

    ("presence/sensor", None, "sensore di presenza mancante"),
    ("presence/sensor", "person.persona_1", "sensore di presenza non sensor"),

    ("weather/entity", "sensor.meteo", "meteo che non e un weather"),

    ("calendar/days", 0, "zero giorni di agenda"),
    ("calendar/days", 30, "troppi giorni di agenda"),
    ("calendar/home_max_events", 9, "troppi eventi in home"),


    ("wifi_sharing/return_home_after_s", 5, "ritorno Wi-Fi troppo corto"),
    ("wifi_sharing", "niente", "condivisione che non e un oggetto"),
    ("wifi_sharing/networks/0/ssid", "", "rete senza SSID"),
    ("wifi_sharing/networks/0/security", "WPA3", "sicurezza inventata"),
    ("wifi_sharing/networks/0/display_name", "n" * 33,
     "nome della rete troppo lungo"),
    # Il sesto elemento e il punto: cinque e il limite, e oltre quello la
    # tabella dei segreti non ha piu voci da assegnare.
    ("wifi_sharing/networks", [RETE_VALIDA] * 6, "sei reti"),

    ("sections", [], "nessuna sezione"),
    ("sections", ["lights", "inventata"], "sezione sconosciuta"),
    # La variante che mancava, ed e quella che ha fatto divergere i due
    # validatori senza che nessuno se ne accorgesse: una chiave che lo
    # schema non conosce. Qui si pretende che **passi** in tutti e due, che
    # e la regola decisa nel contratto (§12): i campi sconosciuti si
    # conservano. Le altre varianti mettono valori sbagliati in chiavi
    # note, e per questo non potevano scoprirlo.
    ("weather/chiave_inventata", 42, "chiave sconosciuta in una sezione"),
    ("system/network/chiave_inventata", "x", "chiave sconosciuta annidata"),
    ("sections", ["lights", "climate", "energy", "access",
                  "calendar", "wifi"], "agenda elencata ma non attiva"),
]


def verdetto_python(percorso: Path) -> bool:
    """Vero se verifica_config.py accetta."""
    r = subprocess.run([sys.executable, str(VERIFICA), str(percorso)],
                       capture_output=True, text=True)
    return r.returncode == 0


def verdetto_firmware(binario: Path, percorso: Path) -> tuple[bool, str]:
    r = subprocess.run([str(binario), str(percorso)],
                       capture_output=True, text=True)
    return r.returncode == 0, r.stdout.strip()


def profili_conosciuti() -> int:
    """Lo schema deve conoscere i profili che esistono davvero.

    I quattro nomi stanno in tre posti: `docs/profili.json`, da cui si
    genera profile.h; `docs/config.schema.json`, che e la fonte autorevole
    della validazione; e l'elenco scritto a mano dentro validazione.c. Il
    resto di questo strumento tiene allineati gli ultimi due. Nessuno
    teneva allineato il primo.

    Conta il giorno che arriva un quinto pannello: profile.h lo conosce, il
    firmware sa disegnarlo, e la sua configurazione viene rifiutata con
    "valore non ammesso" da un elenco che nessuno pensa di andare a
    guardare, perche il profilo *esiste*.
    """
    profili = json.loads((RADICE / "docs" / "profili.json")
                         .read_text(encoding="utf-8"))
    veri = set(profili.get("profili", {}))
    schema = json.loads((RADICE / "docs" / "config.schema.json")
                        .read_text(encoding="utf-8"))
    ammessi = set(
        schema["properties"]["system"]["properties"]["profile"]["enum"])

    if veri == ammessi:
        print(f"profili: {len(veri)} in profili.json, "
              f"tutti ammessi dallo schema")
        return 0

    for p in sorted(veri - ammessi):
        print(f"  MANCA NELLO SCHEMA  {p} - esiste in profili.json, ma una "
              f"configurazione che lo chiede verrebbe rifiutata")
    for p in sorted(ammessi - veri):
        print(f"  NON ESISTE          {p} - lo schema lo ammette, ma "
              f"profili.json non lo conosce")
    return 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--binario", default="build/prova_config",
                    help="l'eseguibile del validatore del firmware")
    ap.add_argument("--tutto", action="store_true",
                    help="elenca anche le varianti su cui i due concordano")
    arg = ap.parse_args()

    binario = Path(arg.binario)
    if not binario.exists():
        print(f"{binario} non c'e: compila prima", file=sys.stderr)
        return 2

    try:
        import jsonschema  # noqa: F401
    except ImportError:
        print("jsonschema non installato: la prova non direbbe niente.\n"
              "  pip install jsonschema", file=sys.stderr)
        return 2

    base = json.loads(ESEMPIO.read_text(encoding="utf-8"))

    casi = [("la configurazione della casa", base)]
    for percorso, valore, nome in VARIANTI:
        try:
            casi.append((nome, muta(base, percorso, valore)))
        except (KeyError, IndexError):
            print(f"variante non applicabile: {percorso}", file=sys.stderr)
            return 2

    discordi = 0
    with tempfile.TemporaryDirectory() as tmp:
        for nome, cfg in casi:
            p = Path(tmp) / "config.json"
            p.write_text(json.dumps(cfg, ensure_ascii=False), encoding="utf-8")

            py = verdetto_python(p)
            fw, dettaglio = verdetto_firmware(binario, p)

            if py == fw:
                if arg.tutto:
                    print(f"  {'accettano' if py else 'rifiutano':10s} {nome}")
                continue

            discordi += 1
            print(f"  DISCORDI  {nome}")
            print(f"      python   {'accetta' if py else 'rifiuta'}")
            print(f"      firmware {'accetta' if fw else 'rifiuta'}")
            if dettaglio:
                for riga in dettaglio.splitlines()[:3]:
                    print(f"        {riga}")

    discordi += profili_conosciuti()

    print(f"\n{len(casi)} configurazioni, {discordi} verdetti discordi")
    if discordi:
        print("Il controllo scritto a mano si e allontanato dallo schema, "
              "oppure lo schema\ndal contratto. Vanno riallineati: la fonte "
              "autorevole e config.schema.json.")
    return 1 if discordi else 0


if __name__ == "__main__":
    sys.exit(main())
