#!/usr/bin/env python3
"""
Nessun numero di layout fuori da profile.h.

    python tools/verifica_numeri.py            controlla, esce 1 se trova
    python tools/verifica_numeri.py --elenca   mostra anche cosa ha guardato

E il criterio di 11-collaudo.md §1 reso automatico, ed e il vincolo che
tiene insieme una sola base di codice per quattro schermi: un componente che
ha bisogno di sapere quanto e largo il rail lo chiede al profilo.

    lv_obj_set_width(rail, PRF->geo.rail_w);   /* si */
    lv_obj_set_width(rail, 96);                /* no */

Cerca i numeri passati alle chiamate che decidono una geometria. Non e un
compilatore: guarda le chiamate, non le espressioni, ed e volutamente
severo — un falso positivo si risolve mettendo la misura dove deve stare.

Cosa non guarda, e perche:
  main/profile.h        e il posto dove i numeri devono stare
  main/ui/fonts/        generato: quei numeri sono bitmap e metriche di font
  main/ui/icons/        idem
  sim/                  il simulatore non e il pannello: la finestra della
                        scelta del profilo e un attrezzo, non una schermata
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
GUARDA = [RADICE / "main"]
SALTA = {
    RADICE / "main" / "profile.h",
    RADICE / "main" / "ui" / "fonts",
    RADICE / "main" / "ui" / "icons",
}

# Chiamate che fissano una geometria. Il numero passato qui e una misura.
CHIAMATE = re.compile(r"""\b(
      lv_obj_set_(?:width|height|size|x|y|pos|content_width|content_height)
    | lv_obj_set_style_(?:width|height|min_width|max_width|min_height|max_height
                        |pad_\w+|margin_\w+|radius|border_width|outline_\w+
                        |transform_\w+|translate_\w+|line_width|arc_width
                        |text_letter_space|text_line_space|shadow_\w+)
    | lv_obj_align(?:_to)?
    | lv_obj_set_ext_click_area
    | lv_obj_set_scroll\w*
    | lv_style_set_\w+
    | lv_area_set\w*
    | lv_canvas_set_\w+
)\s*\(""", re.X)

# Fuori dall'elenco di proposito: lv_arc_set_range, lv_bar_set_value e
# simili prendono valori nell'unita del widget — percentuali, gradi — non
# pixel. Lo spessore di un arco, che invece e in pixel, passa comunque da
# lv_obj_set_style_arc_width, che l'elenco ha.

# Numeri che non sono misure:
#   0        assenza di un vincolo
#   1        indice, moltiplicatore, flex_grow
#   -1       "non impostato" per LVGL
CONSENTITI = {"0", "1", "-1"}

# Costanti di LVGL che non sono misure. Le percentuali le toglie
# senza_percentuali(), che sa gestire anche un'espressione dentro LV_PCT().
INNOCUI = re.compile(r"\bLV_PCT\s*\(\s*-?\d+\s*\)|\bLV_SIZE_CONTENT\b"
                     r"|\bLV_COORD_MAX\b|\bLV_OPA_\w+\b")

NUMERO = re.compile(r"(?<![\w.>])(-?\d+)(?![\w.])")


def argomenti(testo: str, apertura: int) -> tuple[str, int]:
    """Il contenuto della parentesi che si apre in `apertura`, e dove finisce."""
    livello, i = 0, apertura
    while i < len(testo):
        if testo[i] == "(":
            livello += 1
        elif testo[i] == ")":
            livello -= 1
            if livello == 0:
                return testo[apertura + 1:i], i
        elif testo[i] in "\"'":
            virg = testo[i]
            i += 1
            while i < len(testo) and testo[i] != virg:
                i += 2 if testo[i] == "\\" else 1
        i += 1
    return testo[apertura + 1:], len(testo)


def senza_percentuali(t: str) -> str:
    """Toglie le espressioni dentro LV_PCT(...), parentesi annidate comprese."""
    fuori, i = [], 0
    while True:
        k = t.find("LV_PCT", i)
        if k < 0:
            fuori.append(t[i:])
            return "".join(fuori)
        fuori.append(t[i:k])
        j = t.find("(", k)
        if j < 0:
            return "".join(fuori) + t[k:]
        livello, n = 0, j
        while n < len(t):
            if t[n] == "(":
                livello += 1
            elif t[n] == ")":
                livello -= 1
                if livello == 0:
                    break
            n += 1
        i = n + 1


def senza_commenti(t: str) -> str:
    """Sostituisce i commenti con spazi, mantenendo le posizioni."""
    fuori = []
    i = 0
    while i < len(t):
        if t.startswith("/*", i):
            fine = t.find("*/", i + 2)
            fine = len(t) if fine < 0 else fine + 2
            fuori.append("".join(c if c == "\n" else " " for c in t[i:fine]))
            i = fine
        elif t.startswith("//", i):
            fine = t.find("\n", i)
            fine = len(t) if fine < 0 else fine
            fuori.append(" " * (fine - i))
            i = fine
        else:
            fuori.append(t[i])
            i += 1
    return "".join(fuori)


def controlla(percorso: Path) -> list[tuple[int, str, str]]:
    testo = senza_commenti(percorso.read_text(encoding="utf-8", errors="replace"))
    trovati = []
    for m in CHIAMATE.finditer(testo):
        arg, fine = argomenti(testo, m.end() - 1)
        pulito = INNOCUI.sub(" ", senza_percentuali(arg))
        numeri = [n for n in NUMERO.findall(pulito) if n not in CONSENTITI]
        if numeri:
            riga = testo.count("\n", 0, m.start()) + 1
            chiamata = " ".join(testo[m.start():fine + 1].split())
            trovati.append((riga, m.group(1), chiamata[:100]))
    return trovati


def da_guardare() -> list[Path]:
    file = []
    for radice in GUARDA:
        for p in sorted(radice.rglob("*")):
            if p.suffix not in (".c", ".h"):
                continue
            if p in SALTA or any(s in p.parents for s in SALTA):
                continue
            file.append(p)
    return file


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elenca", action="store_true", help="mostra i file guardati")
    arg = ap.parse_args()

    file = da_guardare()
    tutti = 0
    for p in file:
        rel = p.relative_to(RADICE).as_posix()
        trovati = controlla(p)
        if arg.elenca and not trovati:
            print(f"  ok  {rel}")
        for riga, funzione, chiamata in trovati:
            print(f"{rel}:{riga}: misura di layout in {funzione}\n      {chiamata}")
            tutti += 1

    print()
    if tutti:
        print(f"{tutti} misure fuori posto in {len(file)} file.\n"
              "Il numero va in docs/profili.json (e prima in docs/09-profili.md),\n"
              "poi si rigenera con tools/genera_profilo.py e si legge da PRF o COM.")
        return 1
    print(f"{len(file)} file controllati: nessun numero di layout fuori da profile.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
