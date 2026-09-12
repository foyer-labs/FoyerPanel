#!/usr/bin/env python3
"""Le icone che si possono scegliere sono le stesse in tutti i posti?

Interruttori e Programmazioni lasciano scegliere l'icona di ogni voce dalla
pagina web. Quell'elenco vive in cinque posti che devono dire la stessa cosa:

    tools/genera_icone.py     il gruppo `interruttori`: decide cosa si
                              compila nel firmware, ed e la fonte
    docs/config.schema.json   l'enum di `interruttori[].icona` **e** quello
                              di `programmazioni[].icona`: decidono cosa la
                              pagina web offre. Sono lo stesso elenco perche
                              e la stessa domanda
    main/cfg/validazione.c    l'elenco che il pannello accetta
    web/index.html            i glifi dell'anteprima, generati: la tendina
                              mostra accanto l'icona vera, e se quel glifo
                              mancasse l'anteprima direbbe «?» proprio dove
                              serve a decidere

Se divergono nessuno se ne accorge subito, e il modo in cui si vede e' il
peggiore: la pagina offre un nome, il pannello lo accetta, e sul vetro
compare un **rettangolo vuoto** — perche' quel glifo nel font non c'e. Dal
muro si scambia per un difetto del disegno, e si va a cercare nel posto
sbagliato.

    python tools/verifica_icone_scelta.py

Uscita 0 se i tre elenchi coincidono, 1 altrimenti.
"""
import json
import re
import sys
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
GENERATORE = RADICE / "tools" / "genera_icone.py"
SCHEMA = RADICE / "docs" / "config.schema.json"
VALIDATORE = RADICE / "main" / "cfg" / "validazione.c"
ICONS_H = RADICE / "main" / "ui" / "icons" / "icons.h"
PAGINA = RADICE / "web" / "index.html"


def dal_generatore() -> list:
    t = GENERATORE.read_text(encoding="utf-8")
    m = re.search(r'"interruttori":\s*\[(.*?)\]', t, re.S)
    if not m:
        raise SystemExit("gruppo `interruttori` non trovato in genera_icone.py")
    return re.findall(r'"([a-z0-9_]+)"', m.group(1))


def dallo_schema(blocco: str) -> list:
    d = json.loads(SCHEMA.read_text(encoding="utf-8"))
    icona = d["properties"][blocco]["items"]["properties"]["icon"]
    for ramo in icona.get("anyOf", [icona]):
        if "enum" in ramo:
            return list(ramo["enum"])
    raise SystemExit(f"l'enum di {blocco}[].icona non c'e nello schema")


def dalla_pagina() -> list:
    """I glifi che l'anteprima sa disegnare. Il blocco lo scrive
    genera_icone.py fra due segni, e qui si legge quello."""
    t = PAGINA.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"const GLIFI = \{(.*?)\};", t, re.S)
    if not m:
        raise SystemExit("il blocco GLIFI non c'e in web/index.html")
    return re.findall(r'"([a-z0-9_]+)"\s*:', m.group(1))


def dal_validatore() -> list:
    t = VALIDATORE.read_text(encoding="utf-8")
    m = re.search(r"static const char \*const ICONE\[\] = \{(.*?)\};", t, re.S)
    if not m:
        raise SystemExit("l'elenco ICONE non c'e in validazione.c")
    return re.findall(r'"([a-z0-9_]+)"', m.group(1))


def compilate() -> set:
    t = ICONS_H.read_text(encoding="utf-8", errors="replace")
    return {n.lower() for n in re.findall(r"^#define ICO_([A-Z_0-9]+)", t, re.M)}


def main() -> int:
    fonte = dal_generatore()
    altrove = {
        "docs/config.schema.json · switches": dallo_schema("switches"),
        "docs/config.schema.json · schedules": dallo_schema("schedules"),
        "main/cfg/validazione.c": dal_validatore(),
        "web/index.html · anteprima": sorted(dalla_pagina()),
    }

    print("icone scegliibili, secondo tools/genera_icone.py: %d" % len(fonte))

    guasti = []
    for dove, elenco in altrove.items():
        # L'ordine conta dove decide cosa si vede — la tendina della pagina
        # elenca in ordine alfabetico, gli altri nell'ordine della fonte.
        if sorted(elenco) != sorted(fonte) or (
                "index.html" not in dove and elenco != fonte):
            manca = [x for x in fonte if x not in elenco]
            in_piu = [x for x in elenco if x not in fonte]
            guasti.append((dove, manca, in_piu))

    # e devono esistere davvero nel font compilato
    ci_sono = compilate()
    fuori = [x for x in fonte if x not in ci_sono]
    if fuori:
        guasti.append(("main/ui/icons/icons.h", fuori, []))

    if not guasti:
        print("gli elenchi coincidono tutti, e i glifi esistono")
        return 0

    print("\n%d elenchi che non tornano:\n" % len(guasti))
    for dove, manca, in_piu in guasti:
        print("  " + dove)
        if manca:
            print("    mancano: " + ", ".join(manca))
        if in_piu:
            print("    in piu:  " + ", ".join(in_piu))
    print("\nLa fonte e il gruppo `interruttori` di tools/genera_icone.py.")
    print("Dopo averlo cambiato: python tools/genera_icone.py --tutto")
    return 1


if __name__ == "__main__":
    sys.exit(main())
