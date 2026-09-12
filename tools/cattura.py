#!/usr/bin/env python3
"""
Cattura le schermate in tutti i profili, per il confronto con i mockup.

    python tools/cattura.py                 tutte le schermate, tutti i profili
    python tools/cattura.py --profilo p4-1280x800
    python tools/cattura.py --schermata home

I mockup di docs/mockup/ sono rendering 1:1: "indistinguibile dai mockup" si
verifica mettendo le due immagini una accanto all'altra, non a memoria.

La cattura non apre finestre e non usa SDL, quindi funziona anche senza
schermo — dentro WSL senza WSLg, in una macchina di compilazione, in uno
script.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bmp2png import converti  # noqa: E402

RADICE = Path(__file__).resolve().parent.parent

# L'orologio fermo, cosi due serie di catture differiscono solo per quello
# che e cambiato davvero. Con l'ora vera ogni immagine cambierebbe a ogni
# esecuzione proprio nella parte che si guarda per prima, e un confronto
# fra due serie diventerebbe illeggibile.
#
# 25 agosto 2026, 18:41 — l'ora dell'esempio in 10-diagnostica.md, cosi le
# catture e la documentazione raccontano lo stesso momento.
import os
AMBIENTE = dict(os.environ, PANNELLO_ORA=str(1787676060), TZ="Europe/Rome")
BINARIO = RADICE / "build" / "pannello"

# La password della rete ospiti e un segreto e sta in NVS, non in
# config.json: senza, la schermata Wi-Fi dice "non impostata" — che e la
# verita per un pannello appena configurato, ma non e la schermata che si
# vuole confrontare col mockup. Si imposta qui, nell'attrezzo, e non nel
# firmware: e la stessa ragione per cui i nomi delle zone stanno nel file e
# non nel codice.
SEGRETI_DI_PROVA = [
    "--segreto", "wifi_share1_pw=ospiti-2026-benvenuti",
    "--segreto", "wifi_pw=non-la-vedi-mai",
]
USCITA = RADICE / "build" / "catture"

PROFILI = ["p4-1280x800", "p4-800x1280"]

# Le schermate che il simulatore sa gia disegnare: nome -> (sezione, vista)
# oppure (sezione, vista, pagina). Una sezione con piu viste compare piu
# volte. Cresce con l'implementazione.
VISTE = {
    "home":                  ("home", 0),
    "luci":                  ("lights", 0),
    "interruttori":          ("switches", 0),
    "clima":                 ("climate", 0),
    # La seconda pagina del riscaldamento: c'e il piano del box, spento, con
    # le sue zone attenuate. Senza, l'unico stato che il riscaldamento a piani
    # aggiunge resterebbe l'unico che nessuna immagine mostra.
    "clima-2":               ("climate", 0, 1),
    "clima-condizionatori":  ("climate", 1),
    "clima-dettaglio":       ("climate", 10),
    # In verticale il dettaglio e su due pagine: senza catturare anche la
    # seconda, il giorno che oscillazione ed extra tornassero sotto la barra
    # in basso non se ne accorgerebbe nessuno. In orizzontale la pagina e
    # una sola e --pagina 1 non ha effetto: la cattura viene uguale, ed e
    # giusto cosi.
    "clima-dettaglio-2":     ("climate", 10, 1),
    "energia":               ("energy", 0),
    "accessi":               ("access", 0),
    "programmazioni":        ("schedules", 0),
    "agenda":                ("calendar", 0),
    "wifi":                  ("wifi", 0),
    "wifi-scelta":           ("wifi", 1),
    "wifi-privata":          ("wifi", 2),
    "impost":                ("settings", 0),
    "impost-sistema":        ("settings", 3),
    "impost-info":           ("settings", 4),
}
SCHERMATE = list(VISTE)

# I casi limite di 11-collaudo.md §2 che il finto fornitore sa riprodurre.
CASI = ["zero-luci", "una-luce", "tutte-spente", "zero-scene", "nomi-lunghi",
        "non-disponibile", "temp-ignota", "timer-senza-automazione",
        "setpoint-al-limite", "notte", "batteria-ferma", "corrente-zero"]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profilo", action="append", choices=PROFILI,
                    help="solo questo profilo (ripetibile)")
    ap.add_argument("--schermata", action="append", choices=SCHERMATE,
                    help="solo questa schermata (ripetibile)")
    ap.add_argument("--casi", action="append", choices=CASI,
                    help="cattura anche questo caso limite (ripetibile)")
    ap.add_argument("--bmp", action="store_true", help="non convertire in PNG")
    arg = ap.parse_args()

    if not BINARIO.exists():
        print(f"{BINARIO.relative_to(RADICE)} non c'e: compila prima con\n"
              f"  cmake -B build -G Ninja && cmake --build build", file=sys.stderr)
        return 1

    USCITA.mkdir(parents=True, exist_ok=True)
    profili = arg.profilo or PROFILI
    schermate = arg.schermata or SCHERMATE

    fatte = 0
    casi = [None] + (arg.casi or [])
    for caso in casi:
      for s in schermate:
        for p in profili:
            nome = f"{s}-{p}" if caso is None else f"{s}-{caso}-{p}"
            bmp = USCITA / f"{nome}.bmp"
            sezione, vista, *resto = VISTE[s]
            cmd = [str(BINARIO), "--profilo", p, "--sezione", sezione,
                   "--vista", str(vista), "--cattura", str(bmp)]
            if resto:
                cmd += ["--pagina", str(resto[0])]
            cmd += SEGRETI_DI_PROVA
            if caso: cmd += ["--casi", caso]
            r = subprocess.run(cmd, capture_output=True, text=True,
                               env=AMBIENTE)
            if r.returncode != 0 or not bmp.exists():
                print(f"cattura fallita: {s} {p}\n{r.stderr}", file=sys.stderr)
                return 1
            if arg.bmp:
                print(bmp.relative_to(RADICE))
            else:
                png = bmp.with_suffix(".png")
                converti(bmp, png)
                bmp.unlink()
                print(png.relative_to(RADICE))
            fatte += 1

    print(f"\n{fatte} catture in {USCITA.relative_to(RADICE)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
