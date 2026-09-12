#!/usr/bin/env python3
"""Ogni chiave dello schema ha un posto dove si configura?

La pagina web non e' generata dallo schema: ogni sezione dichiara a mano
quali chiavi di primo livello mostra (`chiavi:[...]`). E' una scelta —
l'ordine e i testi di accompagnamento contano piu' di quanto costi
l'elenco — ma ha un buco: una chiave nuova nello schema **non compare da
nessuna parte** finche' qualcuno non la aggiunge a una sezione, e non lo
dice nessuno.

E' successo con `elettrodomestici`: schema, validatore, firmware e
interfaccia tutti pronti, e nella pagina non c'era modo di scriverci
dentro. Dal browser si vedeva la spunta «mostra lavatrice e asciugatrice» e
sembrava tutto a posto; sul muro non compariva niente, perche' l'elenco era
vuoto e non c'era modo di riempirlo.

    python tools/verifica_copertura_pagina.py

Uscita 0 se ogni chiave di primo livello e' coperta, 1 altrimenti.
"""
import json
import re
import sys
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
SCHEMA = RADICE / "docs" / "config.schema.json"
PAGINA = RADICE / "web" / "index.html"

# Chiavi che non si configurano e non devono comparire: sono meta, non
# impostazioni. `schema` e' il numero di versione del documento; `sistema`
# e' coperto ma con un id di sezione diverso, quindi non serve elencarlo.
FUORI = {"schema"}


def chiavi_schema() -> set:
    d = json.loads(SCHEMA.read_text(encoding="utf-8"))
    return set(d.get("properties", {})) - FUORI


def chiavi_pagina() -> set:
    t = PAGINA.read_text(encoding="utf-8", errors="replace")
    coperte = set()
    # chiavi:["a","b"]  — anche su piu' righe
    for blocco in re.findall(r"chiavi\s*:\s*\[([^\]]*)\]", t, re.S):
        coperte |= set(re.findall(r'"([a-z_0-9]+)"', blocco))
    # le sezioni speciali non hanno `chiavi` ma coprono comunque qualcosa
    if 'speciale:"stato"' in t:
        coperte.add("diagnostics")
    return coperte


def main() -> int:
    schema, pagina = chiavi_schema(), chiavi_pagina()
    scoperte = sorted(schema - pagina)
    inventate = sorted(pagina - schema)

    print("chiavi di primo livello nello schema: %d" % len(schema))
    print("coperte da una sezione della pagina:  %d" % len(schema & pagina))

    if not scoperte and not inventate:
        print("\nogni chiave ha un posto dove si configura")
        return 0

    if scoperte:
        print("\n%d chiavi che la pagina non mostra da nessuna parte:"
              % len(scoperte))
        for k in scoperte:
            print("  " + k)
        print("\nSi aggiungono a `chiavi` di una sezione in web/index.html,")
        print("poi si rigenera con tools/genera_pagina.py.")
    if inventate:
        print("\n%d chiavi che la pagina mostra e lo schema non conosce:"
              % len(inventate))
        for k in inventate:
            print("  " + k)
    return 1


if __name__ == "__main__":
    sys.exit(main())
