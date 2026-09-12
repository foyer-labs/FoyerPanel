#!/usr/bin/env python3
"""La struttura dell'interfaccia viene da config.json, non dal codice.

E la promessa su cui poggia tutta la Fase 2, ed e anche la piu facile da
rompere senza accorgersene: basta che un elenco resti agganciato ai valori
di esempio e i conteggi tornano lo stesso, perche i valori di esempio *sono*
quelli della casa vera.

Per questo il confronto non si fa sulla configurazione d'esempio. Se ne
costruiscono altre — meno zone, nessuna scena, un accesso in piu, l'agenda
accesa, la rete ospiti spenta — e si pretende che il pannello riporti
esattamente quello che c'e scritto nel file. Su una configurazione sola non
si distinguerebbe un elenco che legge da un elenco che indovina.

    python3 tools/prova_struttura.py [--binario build/pannello]
"""
from __future__ import annotations

import argparse
import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
ESEMPIO = RADICE / "docs" / "04-config.example.json"


def leggi(binario: Path, cartella: Path) -> dict[str, str]:
    """Fa dire al pannello che struttura ha ricavato."""
    r = subprocess.run(
        [str(binario), "--dati", str(cartella), "--prova-struttura"],
        capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print(r.stdout, r.stderr, file=sys.stderr)
        raise SystemExit(f"il pannello e uscito con {r.returncode}")
    fuori = {}
    for riga in r.stdout.splitlines():
        if "=" in riga and not riga.startswith("["):
            k, _, v = riga.partition("=")
            fuori[k.strip()] = v.strip()
    return fuori


def atteso(c: dict) -> dict[str, str]:
    """Quello che il documento dice, letto senza passare dal C."""
    luci = c.get("lights", {}).get("zones", [])
    scene = c.get("lights", {}).get("scenes", [])
    risc = c.get("climate", {}).get("heating", [])
    cond = c.get("climate", {}).get("air_conditioners", [])
    acc = c.get("access", [])
    wifi = c.get("wifi_sharing", {})

    a = {
        "luci": str(len(luci)),
        "scene": str(len(scene)),
        "riscaldamento": str(len(risc)),
        "condizionatori": str(len(cond)),
        "accessi": str(len(acc)),
    }
    for n, z in enumerate(luci):
        a[f"luce.{n}"] = z["name"]
    for n, z in enumerate(risc):
        a[f"zona.{n}"] = z["name"]
    for n, u in enumerate(cond):
        a[f"unita.{n}"] = u["name"]
    for n, x in enumerate(acc):
        a[f"accesso.{n}"] = "{},{},{}".format(
            x["id"], x.get("type", "pulse"),
            "sensore" if x.get("state_sensor") is not None else "cieco")

    # Una sezione elencata ma senza la propria sorgente dati non compare.
    def ha_sorgente(chiave: str) -> bool:
        if chiave == "calendar":
            return bool(c.get("calendar", {}).get("enabled"))
        if chiave == "wifi":
            return any(r.get("enabled", True)
                       for r in wifi.get("networks", []))
        return True

    viste = [s for s in c.get("sections", []) if ha_sorgente(s)]
    for n, s in enumerate(viste):
        a[f"sezione.{n}"] = s
    return a


def varianti(base: dict):
    """La stessa casa, diversa in un punto per volta."""
    yield "esempio", base

    c = copy.deepcopy(base)
    c["lights"]["zones"] = c["lights"]["zones"][:3]
    yield "tre-zone-di-luce", c

    c = copy.deepcopy(base)
    c["lights"]["scenes"] = [
        {"id": "sera", "nome": "Sera", "entita": "scene.sera"},
        {"id": "notte", "nome": "Notte", "entita": "scene.notte"},
    ]
    yield "due-scene", c

    c = copy.deepcopy(base)
    for n, z in enumerate(c["climate"]["heating"]):
        z["name"] = f"Zona rinominata {n + 1}"
    yield "zone-rinominate", c

    c = copy.deepcopy(base)
    c["climate"]["air_conditioners"] = c["climate"]["air_conditioners"][:1]
    yield "una-unita", c

    c = copy.deepcopy(base)
    # Un accesso che sa dire come sta, dove prima nessuno lo sapeva.
    c["access"][0]["state_sensor"] = "binary_sensor.cancello_pedonale_aperto"
    c["access"][1]["type"] = "switch"
    c["access"][1]["pulse_ms"] = None
    yield "accessi-diversi", c

    c = copy.deepcopy(base)
    c["sections"] = ["climate", "lights", "access"]
    yield "tre-sezioni-in-altro-ordine", c

    c = copy.deepcopy(base)
    c["sections"] = c["sections"] + ["calendar"]
    yield "agenda-elencata-ma-spenta", c

    c = copy.deepcopy(base)
    c["sections"] = c["sections"] + ["calendar"]
    c["calendar"]["enabled"] = True
    c["calendar"]["calendars"] = [
        {"id": "casa", "nome": "Casa", "entita": "calendar.casa"}]
    yield "agenda-accesa", c

    # Una rete spenta nell'elenco: la sezione resta, perche ne restano altre.
    c = copy.deepcopy(base)
    c["wifi_sharing"]["networks"][0]["enabled"] = False
    yield "una-rete-spenta", c

    # Piu reti, tutte accese.
    c = copy.deepcopy(base)
    c["wifi_sharing"]["networks"] = [
        { "attiva": True, "nome_mostrato": "Casa", "ssid": "CASA" },
        { "attiva": True, "nome_mostrato": "Studio", "ssid": "STUDIO" },
        { "attiva": True, "nome_mostrato": "Ospiti", "ssid": "OSPITI" },
    ]
    yield "tre-reti", c

    # Nessuna rete accesa: la sezione sparisce da sola, senza che nessuno
    # la nasconda.
    c = copy.deepcopy(base)
    c["wifi_sharing"]["networks"] = []
    yield "niente-da-condividere", c


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", default=str(RADICE / "build" / "pannello"))
    args = ap.parse_args()

    binario = Path(args.binario)
    if not binario.exists():
        print(f"manca {binario}: compila prima il simulatore")
        return 1

    base = json.loads(ESEMPIO.read_text(encoding="utf-8"))
    discordi = 0
    provate = 0

    with tempfile.TemporaryDirectory() as tmp:
        cartella = Path(tmp)
        for nome, c in varianti(base):
            (cartella / "config.json").write_text(
                json.dumps(c, ensure_ascii=False, indent=2), encoding="utf-8")
            (cartella / "config.json.bak").unlink(missing_ok=True)

            visto = leggi(binario, cartella)
            voglio = atteso(c)
            provate += 1

            differenze = []
            for k in sorted(set(voglio) | set(visto)):
                if voglio.get(k) != visto.get(k):
                    differenze.append(
                        f"    {k}: documento {voglio.get(k)!r}, "
                        f"pannello {visto.get(k)!r}")
            if differenze:
                discordi += 1
                print(f"DISCORDI  {nome}")
                print("\n".join(differenze))
            else:
                print(f"ok        {nome}")

    print(f"\n{provate} configurazioni, {discordi} discordanti")
    return 1 if discordi else 0


if __name__ == "__main__":
    raise SystemExit(main())
