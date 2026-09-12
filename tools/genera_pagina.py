#!/usr/bin/env python3
"""Impacchetta la pagina di configurazione e lo schema in main/web/pagina.h.

Il pannello non ha un filesystem da cui servire una cartella di risorse: la
pagina sta nella flash del firmware, come i font e le icone. Qui la si
trasforma in due stringhe C.

Lo schema viaggia con la pagina perche e **lo stesso file** che il firmware
usa per validare: se la pagina se ne portasse dietro una copia propria, la
prima modifica allo schema le farebbe accettare cose che il pannello
rifiuta, e l'utente vedrebbe un 422 su un campo che la pagina gli aveva
appena detto essere valido.

Lo schema si minimizza — via indentazione e spazi — perche di quei 26 KB
sulla flash non serve niente al browser tranne i dati.

    python3 tools/genera_pagina.py [--verifica]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
PAGINA = RADICE / "web" / "index.html"
SCHEMA = RADICE / "docs" / "config.schema.json"
USCITA = RADICE / "main" / "web" / "pagina.h"


def stringa_c(dati: bytes, per_riga: int = 20) -> str:
    """I byte come array C.

    Non una stringa fra virgolette: il contenuto ha virgolette, backslash,
    caratteri accentati in UTF-8 e sequenze che un compilatore leggerebbe
    come trigrafi. Con i numeri non c'e niente da sfuggire e niente da
    sbagliare, e il compilatore non deve rileggersi 30 KB di escape.
    """
    righe = []
    for n in range(0, len(dati), per_riga):
        pezzo = dati[n:n + per_riga]
        righe.append("    " + " ".join(f"{b:3d}," for b in pezzo))
    return "\n".join(righe)


def riferimenti(nodo, radice, dove="#"):
    """Controlla che ogni $ref punti a qualcosa che c'e.

    La pagina disegna i campi guardando `type` e `properties`. Un $ref non
    ne ha nessuno dei due: se non lo srotola, o se lo srotola e non trova
    niente, quel pezzo di schema finisce fra i campi semplici e diventa una
    casella di testo che non configura nulla. E successo davvero — la rete
    ospiti compariva cosi, e per mesi il suo nome non si poteva cambiare da
    nessuna parte. Un $ref rotto e silenzioso: non e un errore, e un campo
    che sparisce.
    """
    guai = []
    if isinstance(nodo, list):
        for n, v in enumerate(nodo):
            guai += riferimenti(v, radice, f"{dove}/{n}")
        return guai
    if not isinstance(nodo, dict):
        return guai

    r = nodo.get("$ref")
    if isinstance(r, str):
        if not r.startswith("#/"):
            guai.append(f"{dove}: $ref fuori dal file — {r}")
        else:
            d = radice
            for k in r[2:].split("/"):
                d = d.get(k) if isinstance(d, dict) else None
            if d is None:
                guai.append(f"{dove}: $ref che non porta da nessuna parte — {r}")

    for k, v in nodo.items():
        guai += riferimenti(v, radice, f"{dove}/{k}")
    return guai


def genera() -> str:
    html = PAGINA.read_bytes()
    albero = json.loads(SCHEMA.read_text(encoding="utf-8"))

    guai = riferimenti(albero, albero)
    if guai:
        raise SystemExit("$ref rotti nello schema:\n  " + "\n  ".join(guai))

    # La pagina deve saperli srotolare: senza, i $ref restano nel documento
    # che le arriva e i campi che ci stanno dietro non si disegnano.
    if b"$ref" in json.dumps(albero).encode() and b"srotola" not in html:
        raise SystemExit("lo schema usa $ref ma la pagina non li srotola")

    schema = json.dumps(albero, ensure_ascii=False,
                        separators=(",", ":")).encode("utf-8")

    return f"""/* ------------------------------------------------------------------------
 * GENERATO da tools/genera_pagina.py — non modificare a mano.
 *
 * Sorgenti: web/index.html, docs/config.schema.json
 *
 * La pagina di configurazione e lo schema vivono nella flash del firmware:
 * il pannello non ha una cartella di risorse da cui servirli, e comunque
 * devono esserci anche il giorno che la rete e giu — che e proprio il
 * giorno in cui si va a vedere cosa non va.
 *
 * Lo schema e **lo stesso file** che valida il firmware. Se la pagina se ne
 * portasse dietro una copia propria, la prima modifica allo schema le
 * farebbe accettare cose che il pannello rifiuta, e si vedrebbe un 422 su
 * un campo che la pagina aveva appena dichiarato valido.
 *
 * Pagina: {len(html)} byte. Schema: {len(schema)} byte, minimizzato.
 * --------------------------------------------------------------------- */
#ifndef PAGINA_H
#define PAGINA_H

#include <stddef.h>

static const char PAGINA_HTML[] = {{
{stringa_c(html)}
}};
#define PAGINA_HTML_N ((size_t){len(html)})

static const char PAGINA_SCHEMA[] = {{
{stringa_c(schema)}
}};
#define PAGINA_SCHEMA_N ((size_t){len(schema)})

#endif /* PAGINA_H */
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--verifica", action="store_true",
                    help="non scrive: dice solo se il file e allineato")
    arg = ap.parse_args()

    nuovo = genera()
    vecchio = USCITA.read_text(encoding="utf-8") if USCITA.exists() else ""

    if arg.verifica:
        if nuovo == vecchio:
            print(f"{USCITA.relative_to(RADICE)} allineato ai sorgenti")
            return 0
        print(f"{USCITA.relative_to(RADICE)} NON allineato: "
              f"rilancia tools/genera_pagina.py", file=sys.stderr)
        return 1

    if nuovo != vecchio:
        USCITA.parent.mkdir(parents=True, exist_ok=True)
        # LF on every system: on Windows write_text() would use CRLF, and
        # git would report the file changed and then normalise it back.
        USCITA.write_text(nuovo, encoding="utf-8", newline="\n")
    print(f"scritto {USCITA.relative_to(RADICE)} "
          f"({len(nuovo.splitlines())} righe)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
