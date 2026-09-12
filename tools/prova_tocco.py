#!/usr/bin/env python3
"""
Verifica che i comandi dell'interfaccia arrivino dove devono.

    python tools/prova_tocco.py --binario build/pannello

Il simulatore inietta un tocco vero — dispositivo di ingresso finto,
pressione al centro del bersaglio, rilascio — e guarda se la schermata e
andata dove doveva. Qui si prepara la configurazione con cui provarlo e lo
si lancia su tutti i profili.

Serve una configurazione **sua**: quella d'esempio ha le reti private
spente, e su una schermata senza bersagli una prova del tocco passa senza
aver provato niente. Le due forme che la schermata sa prendere — una rete
sola, che salta l'elenco, e piu reti, che ci passano — vanno provate
entrambe, perche il difetto che ha originato questa prova stava proprio in
una delle due e non nell'altra.
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
PROFILI = ["p4-1280x800", "p4-800x1280"]

RETI = [
    {"enabled": True, "display_name": "Casa", "ssid": "CASA", "security": "WPA"},
    {"enabled": True, "display_name": "Studio", "ssid": "STUDIO", "security": "WPA"},
    {"enabled": True, "display_name": "Domotica", "ssid": "IOT", "security": "WPA"},
]

FORME = {"una-rete-privata": RETI[:1], "tre-reti-private": RETI}


def scrivi(cartella: Path, private: list) -> None:
    c = copy.deepcopy(json.loads(ESEMPIO.read_text(encoding="utf-8")))
    # `networks`, and not `private` as it said until the keys went English:
    # `private` was the schema-2 name, no panel had read it since schema 3,
    # and both variants of this test were quietly running on the example's
    # own networks.
    c["wifi_sharing"]["networks"] = copy.deepcopy(private)
    (cartella / "config.json").write_text(
        json.dumps(c, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8", newline="\n")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", default=str(RADICE / "build" / "pannello"))
    arg = ap.parse_args()

    binario = Path(arg.binario)
    if not binario.exists():
        print(f"{binario} non c'e: compila prima", file=sys.stderr)
        return 2

    falliti = 0
    with tempfile.TemporaryDirectory() as tmp:
        for nome, private in FORME.items():
            cartella = Path(tmp) / nome
            cartella.mkdir()
            scrivi(cartella, private)

            for profilo in PROFILI:
                r = subprocess.run(
                    [str(binario), "--profilo", profilo,
                     "--dati", str(cartella), "--prova-tocco"],
                    capture_output=True, text=True)
                etichetta = f"{nome} su {profilo}"
                if r.returncode == 0:
                    print(f"  ok   {etichetta}")
                else:
                    falliti += 1
                    print(f"  NO   {etichetta}")
                    for riga in r.stdout.splitlines():
                        if riga.strip().startswith("NO"):
                            print(f"         {riga.strip()}")

    print(f"\n{'PROVE FALLITE' if falliti else 'tutti i tocchi arrivano'}")
    return 1 if falliti else 0


if __name__ == "__main__":
    raise SystemExit(main())
