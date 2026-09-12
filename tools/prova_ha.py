#!/usr/bin/env python3
"""Fa girare gli scenari della Fase 3 contro il finto Home Assistant.

Ogni scenario vuole un server con opzioni diverse: un token che rifiuta,
un collegamento che cade a metà di un comando, un'entità che non esiste.
Qui si avvia quello giusto, si lascia parlare il client, e si guarda il
verdetto.

Le porte sono alte e diverse per scenario, così due esecuzioni vicine — o
un `ctest -j` — non si contendono la stessa.

    python3 tools/prova_ha.py [--binario build/prova_ha] [--solo normale]
"""
from __future__ import annotations

import argparse
import os
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
FINTO = RADICE / "tools" / "finto_ha.py"

# scenario -> (porta, opzioni del finto server)
SCENARI = {
    "normale":            (18123, []),
    "token-cattivo":      (18124, ["--token-cattivo"]),
    # Cade al terzo messaggio: auth, subscribe, get_states, poi il comando.
    "caduta":             (18125, ["--cadi-al-comando"]),
    "manca":              (18126, ["--manca", "light.soggiorno"]),
    "comandi-falliscono": (18127, ["--comandi-falliscono"]),
    "indisponibile":      (18128, []),
    # Resta collegato e smette di rispondere: il socket è sano, Home
    # Assistant no. Ammutolisce dal primo battito, che arriva solo da un
    # pannello già collegato: così lo scenario parte sempre dal punto
    # giusto, qualunque cosa faccia l'avvio del collegamento.
    "muto":               (18129, ["--muto-al-battito"]),
    # The heartbeat after a command: the fake server closes on an id that
    # does not increase, as the real one would grow suspicious.
    "battito":            (18130, []),
}


def porta_libera(porta: int, secondi: float = 5.0) -> bool:
    """Aspetta che il server abbia davvero aperto la porta."""
    fine = time.time() + secondi
    while time.time() < fine:
        with socket.socket() as s:
            s.settimeout(0.2)
            if s.connect_ex(("127.0.0.1", porta)) == 0:
                return True
        time.sleep(0.05)
    return False


def uno(binario: Path, nome: str, porta: int, opzioni: list[str]) -> bool:
    server = subprocess.Popen(
        [sys.executable, str(FINTO), "--porta", str(porta), *opzioni],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=RADICE)
    try:
        if not porta_libera(porta):
            print(f"il finto server non ha aperto la porta {porta}",
                  file=sys.stderr)
            return False

        r = subprocess.run([str(binario), str(porta), nome], cwd=RADICE,
                           capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=120)
        print(r.stdout, end="")
        if r.returncode != 0:
            print(r.stderr, file=sys.stderr)
        return r.returncode == 0
    except subprocess.TimeoutExpired:
        print(f"scenario {nome}: tempo scaduto", file=sys.stderr)
        return False
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", default=str(RADICE / "build" / "prova_ha"))
    ap.add_argument("--solo", action="append", choices=list(SCENARI),
                    help="esegue solo gli scenari indicati")
    arg = ap.parse_args()

    binario = Path(arg.binario)
    if not binario.exists():
        print(f"manca {binario}: compila prima", file=sys.stderr)
        return 1

    scelti = arg.solo or list(SCENARI)
    caduti = []
    for nome in scelti:
        porta, opzioni = SCENARI[nome]
        if not uno(binario, nome, porta, opzioni):
            caduti.append(nome)

    print(f"\n{len(scelti)} scenari, {len(caduti)} falliti"
          + (f": {', '.join(caduti)}" if caduti else ""))
    return 1 if caduti else 0


if __name__ == "__main__":
    raise SystemExit(main())
