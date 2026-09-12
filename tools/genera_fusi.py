#!/usr/bin/env python3
"""Da nome IANA a stringa TZ POSIX, presa dal database dei fusi.

ESP-IDF non ha un database dei fusi: setenv("TZ", ...) vuole il formato
POSIX — "CET-1CEST,M3.5.0,M10.5.0/3" — e un nome come "Europe/Rome" non lo
capisce. Non da errore: da UTC in silenzio, cioe un orologio sbagliato di
un'ora per sei mesi all'anno, sul primo apparecchio a cui si guarda l'ora.

Il contratto pero tiene il nome IANA, ed e giusto: e leggibile, e quello che
l'utente riconosce, ed e lo stesso che usa Home Assistant. Serve quindi una
tabella, e questa la genera dai dati veri invece di scriverla a memoria: in
coda a ogni file di /usr/share/zoneinfo c'e proprio la stringa POSIX.

I nomi sono quelli che lo schema propone in `examples`, e non e un caso:
proporre un fuso che il pannello non sa tradurre sarebbe la trappola di
prima con un passaggio in piu.

    python3 tools/genera_fusi.py
"""
from __future__ import annotations

import io
import json
import sys
from collections import OrderedDict
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
SCHEMA = RADICE / "docs" / "config.schema.json"
USCITA = RADICE / "main" / "app" / "fusi.c"
ZONEINFO = Path("/usr/share/zoneinfo")


def posix_di(nome: str) -> str | None:
    """L'ultima riga del file del fuso e la stringa POSIX."""
    f = ZONEINFO / nome
    if not f.is_file():
        return None
    dati = f.read_bytes()
    # Il blocco finale e delimitato da a capo; il contenuto binario prima
    # non contiene a capo isolati alla fine, quindi l'ultima riga non vuota
    # e quella giusta.
    righe = [r for r in dati.split(b"\n") if r]
    if not righe:
        return None
    try:
        return righe[-1].decode("ascii")
    except UnicodeDecodeError:
        return None


def main() -> int:
    verifica = "--verifica" in sys.argv

    schema = json.load(io.open(SCHEMA, encoding="utf-8"),
                       object_pairs_hook=OrderedDict)
    nomi = schema["properties"]["system"]["properties"]["timezone"]["examples"]

    coppie, mancanti = [], []
    for n in nomi:
        p = posix_di(n)
        if p:
            coppie.append((n, p))
        else:
            mancanti.append(n)

    if mancanti:
        print("fusi senza corrispondenza: " + ", ".join(mancanti),
              file=sys.stderr)
        return 1

    largo = max(len(n) for n, _ in coppie) + 3
    righe = "\n".join(
        '    {{ {:<{w}} "{}" }},'.format('"%s",' % n, p, w=largo)
        for n, p in coppie)

    testo = ('''/* ------------------------------------------------------------------------
 * Fusi orari — generato da tools/genera_fusi.py. Non si modifica a mano.
 *
 * ESP-IDF non ha un database dei fusi: setenv("TZ", ...) vuole il formato
 * POSIX, e un nome come "Europe/Rome" non lo capisce. Non da errore: da UTC
 * in silenzio, cioe un orologio sbagliato di un'ora per sei mesi all'anno.
 *
 * Il contratto tiene il nome IANA — leggibile, riconoscibile, lo stesso che
 * usa Home Assistant — e questa tabella lo traduce. I nomi sono esattamente
 * quelli che lo schema propone: proporne uno che il pannello non sa
 * tradurre sarebbe la stessa trappola con un passaggio in piu, e una prova
 * lo impedisce.
 * --------------------------------------------------------------------- */
#include "fusi.h"

#include <string.h>

static const struct { const char *iana; const char *posix; } FUSI[] = {
''' + righe + '''
};

const char *fuso_posix(const char *iana)
{
    if (!iana || !*iana) return NULL;

    for (unsigned n = 0; n < sizeof FUSI / sizeof FUSI[0]; n++)
        if (strcmp(FUSI[n].iana, iana) == 0) return FUSI[n].posix;

    /* Chi ha scritto direttamente una stringa POSIX se la tiene: si
       riconosce perche non contiene la barra dei nomi IANA, e passarla
       cosi com'e e piu utile che rifiutarla. Un fuso che questa tabella non
       conosce, invece, torna NULL — e chi chiama deve dirlo, non fingere
       che vada bene. */
    return strchr(iana, '/') ? NULL : iana;
}

int fusi_quanti(void) { return (int)(sizeof FUSI / sizeof FUSI[0]); }

const char *fuso_nome(int n)
{
    return (n >= 0 && n < fusi_quanti()) ? FUSI[n].iana : NULL;
}
''')

    if verifica:
        # Non basta che la tabella esista: deve corrispondere a **questo**
        # schema. Chi aggiunge un fuso alla pagina e dimentica di
        # rigenerarla lascerebbe un nome che l'utente puo scegliere e il
        # pannello non sa tradurre — e la traduzione mancante non da errore,
        # da UTC in silenzio. Meglio scoprirlo qui.
        attuale = USCITA.read_text(encoding="utf-8") if USCITA.exists() else ""
        if attuale.replace("\r\n", "\n") != testo:
            print("main/app/fusi.c non corrisponde allo schema: "
                  "rigeneralo con python3 tools/genera_fusi.py",
                  file=sys.stderr)
            return 1
        print(f"fusi allineati ({len(coppie)})")
        return 0

    USCITA.write_text(testo, encoding="utf-8", newline="\n")
    print(f"scritto {USCITA.relative_to(RADICE)} ({len(coppie)} fusi)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
