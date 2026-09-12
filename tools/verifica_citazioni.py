#!/usr/bin/env python3
"""Ogni sezione citata dal codice esiste davvero nei documenti?

I commenti di questo progetto rimandano ai documenti come fonte delle
decisioni — «01-specifica-ui.md §3.5» — e quel rimando vale solo finche' la
sezione c'e'. Una citazione a vuoto sarebbe gia' un problema; una citazione
che punta alla sezione **sbagliata** e' peggio, perche' sembra giusta e
manda chi la segue da un'altra parte.

E' successo davvero: il robot citava §4.9, che esiste ed e' «Primo avvio».
Nessuno se n'era accorto per un mese, perche' niente lo controllava.

    python tools/verifica_citazioni.py

Uscita 0 se tutte le citazioni trovano casa, 1 altrimenti.
"""
import re
import sys
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
DOCS = RADICE / "docs"

# «01-specifica-ui.md §3.5», «05-architettura-firmware.md §8-bis»
CITAZIONE = re.compile(r"(\d\d-[a-z0-9-]+\.md)\s*§\s*([0-9]+(?:[.-][0-9a-z]+)*)")

# Un titolo: «## 8-bis. Rotazione», «### 3.5 Accessi», «## 4. Stati»
TITOLO = re.compile(r"^#{2,4}\s+([0-9]+(?:[.-][0-9a-z]+)*)[.\s]")


def sezioni(documento: Path) -> set:
    if not documento.exists():
        return set()
    trovate = set()
    for riga in documento.read_text(encoding="utf-8", errors="replace").splitlines():
        m = TITOLO.match(riga)
        if m:
            n = m.group(1)
            trovate.add(n)
            # «3.4» copre anche chi cita «3» per il capitolo intero
            parti = n.split(".")
            for i in range(1, len(parti)):
                trovate.add(".".join(parti[:i]))
    return trovate


def main() -> int:
    cache = {}
    guasti = []
    visti = 0

    sorgenti = []
    for cartella in ("main", "firmware/main", "sim", "test", "tools"):
        base = RADICE / cartella
        if base.exists():
            for ext in ("*.c", "*.h", "*.py"):
                sorgenti += list(base.rglob(ext))

    for f in sorgenti:
        try:
            testo = f.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for riga_n, riga in enumerate(testo.splitlines(), 1):
            for doc, sez in CITAZIONE.findall(riga):
                visti += 1
                if doc not in cache:
                    cache[doc] = sezioni(DOCS / doc)
                if not (DOCS / doc).exists():
                    guasti.append((f, riga_n, doc, sez, "il documento non esiste"))
                elif sez not in cache[doc]:
                    guasti.append((f, riga_n, doc, sez, "sezione assente"))

    print("citazioni controllate: %d" % visti)
    if not guasti:
        print("tutte trovano casa")
        return 0

    print("\n%d citazioni che non trovano casa:\n" % len(guasti))
    for f, n, doc, sez, perche in guasti:
        print("  %s:%d  ->  %s §%s  (%s)"
              % (f.relative_to(RADICE).as_posix(), n, doc, sez, perche))
    return 1


if __name__ == "__main__":
    sys.exit(main())
