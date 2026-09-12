#!/usr/bin/env python3
"""Un campo facoltativo dello schema arriva alla pagina col suo tipo?

Nello schema un campo facoltativo si scrive `anyOf: [{...}, {"type":"null"}]`,
che vuol dire «questo tipo, oppure niente». Il costo nascosto e' che il tipo
smette di stare al primo livello: `s.type` diventa undefined, e chiunque lo
guardi non lo trova piu'.

E' successo. `elettrodomestici[].potenza_massima` e' un intero facoltativo:
la pagina non lo riconosceva come numero, gli metteva davanti un campo di
testo e salvava `"1000"` con le virgolette. Il pannello lo legge con
cfg_intero_in(), ripiegava su zero, e con zero la barretta del consumo non
si disegnava. Nessun errore in nessun punto della catena.

Questa prova non apre un browser: legge lo schema, trova i campi facoltativi,
e pretende che nella pagina il tipo si risolva prima di guardarlo — cioe' che
`appiattisci()` ci sia e che chi decide il tipo di un campo passi da li'.

    python tools/verifica_pagina_tipi.py

Uscita 0 se la pagina appiattisce, 1 altrimenti.
"""
import json
import re
import sys
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
SCHEMA = RADICE / "docs" / "config.schema.json"
PAGINA = RADICE / "web" / "index.html"


def facoltativi(nodo, dove=""):
    """I campi scritti anyOf con un solo ramo non-null, col loro tipo."""
    fuori = []
    if not isinstance(nodo, dict):
        return fuori
    if "anyOf" in nodo:
        rami = [a for a in nodo["anyOf"]
                if isinstance(a, dict) and a.get("type") != "null"]
        if len(rami) == 1:
            fuori.append((dove, rami[0].get("type")))
    for chiave, valore in nodo.items():
        if chiave == "properties":
            for k, v in valore.items():
                fuori += facoltativi(v, dove + "/" + k)
        elif chiave == "items":
            fuori += facoltativi(valore, dove + "[]")
    return fuori


def main() -> int:
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    pagina = PAGINA.read_text(encoding="utf-8", errors="replace")

    campi = facoltativi(schema)
    numerici = [(d, t) for d, t in campi if t in ("integer", "number")]

    print("campi facoltativi nello schema: %d (%d numerici)"
          % (len(campi), len(numerici)))

    guasti = []
    if "function appiattisci(" not in pagina:
        guasti.append("la pagina non ha appiattisci(): un campo facoltativo "
                      "arriva senza tipo")

    # Chi decide il tipo del campo e chi valida devono vedere lo schema
    # appiattito, non quello grezzo.
    if not re.search(r"s = appiattisci\(s\.properties\[k\]\)", pagina):
        guasti.append("sottoschema() non appiattisce: il disegno del campo "
                      "vedrebbe ancora anyOf")
    if not re.search(r"const sub = appiattisci\(grezzo\)", pagina):
        guasti.append("validaTutto() non appiattisce: la validazione "
                      "salterebbe i campi facoltativi")

    if guasti:
        print("\n%d cose che non tornano:\n" % len(guasti))
        for g in guasti:
            print("  " + g)
        print("\nI campi che ne pagherebbero il prezzo per primi:")
        for d, t in numerici:
            print("  %-46s %s" % (d, t))
        return 1

    print("la pagina risolve il tipo prima di guardarlo")
    for d, t in numerici:
        print("  %-46s %s -> campo numerico" % (d, t))
    return 0


if __name__ == "__main__":
    sys.exit(main())
