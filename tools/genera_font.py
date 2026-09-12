#!/usr/bin/env python3
"""
Compila i font di 09-profili.md §4 in font bitmap LVGL.

    python tools/genera_font.py              compila cio che manca
    python tools/genera_font.py --tutto      ricompila tutto
    python tools/genera_font.py --elenca     mostra il piano e le dimensioni

Serve Node (per lv_font_conv, scaricato al volo con npx).

Dieci corpi per famiglia: i sei di scala piu i quattro fuori scala. Le due
famiglie hardware hanno due scale; i profili verticali usano quella della
propria famiglia, non una terza (09-profili.md §4).

Le cifre sono tabulari perche i sorgenti sono copie di Inter con la feature
`tnum` congelata dentro il cmap: LVGL non applica feature OpenType a runtime,
quindi va fatto prima. Se i .ttf non ci sono, questo script spiega come
rifarli invece di indovinare.

Il sottoinsieme di caratteri non e un'ottimizzazione: un corpo da 132 px con
tutto l'alfabeto occuperebbe piu della partizione. L'orologio di standby ha
bisogno di dieci cifre e due punti, e prende solo quelli.
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
SORGENTI = RADICE / "tools" / "font"
USCITA = RADICE / "main" / "ui" / "fonts"
PROFILI = RADICE / "docs" / "profili.json"

# --- sorgenti ---------------------------------------------------------------
# peso -> file, con la corrispondenza pesi/uso di 09-profili.md §4
TTF = {
    "rg": SORGENTI / "Inter-Regular-tnum.ttf",    # 400
    "md": SORGENTI / "Inter-Medium-tnum.ttf",     # 500
    "sb": SORGENTI / "Inter-SemiBold-tnum.ttf",   # 600
    "mono": SORGENTI / "JetBrainsMono-Regular.ttf",
}

# --- insiemi di caratteri ---------------------------------------------------
# Tutto il Latin-1, e non piu solo le accentate dell'italiano.
#
# Le ragioni sono due, e la seconda vale anche per chi usa il pannello in
# inglese. La prima: l'interfaccia parla cinque lingue, e francese, tedesco e
# spagnolo vogliono lettere che l'italiano non ha — ç ê ë î ï ô û, ä ö ü ß,
# ñ ¡ ¿. La seconda: **i nomi li scrive chi installa**. Una stanza che in
# Home Assistant si chiama «Küche» arriva sul pannello con la sua ü, qualunque
# lingua abbia scelto, e un font che non la conosce la mostra come un
# quadratino.
#
# Il Latin-1 intero costa poco piu delle lettere scelte una per una, ed e un
# insieme che si puo nominare: chi aggiunge una lingua che ci sta dentro non
# deve ricordarsi di venire qui.
LETTERE = (
    "0xA0-0xFF,"                       # Latin-1: accentate, ß ñ ç, ¡ ¿ « » ° · º ª
    "0x152,0x153,0x178,"               # Œ œ Ÿ, che il Latin-1 non ha e il francese si
)
TESTO = (
    "0x20-0x7E,"                       # ASCII stampabile
    + LETTERE +
    "0x2013,0x2014,"                   # trattini lunghi
    "0x2018,0x2019,0x201A,"            # apostrofi, virgoletta bassa tedesca
    "0x201C,0x201D,0x201E,"            # virgolette, virgolette basse tedesche
    "0x2022,0x2026,"                   # pallino, puntini di sospensione
    "0x20AC"                           # euro
)
# I valori grandi mostrano numeri ma anche parole brevi: "spenta", "aus",
# "éteint", "kW". Le lettere sono le stesse del testo: una parola breve in
# un'altra lingua non deve perdere l'accento perche e scritta in grande.
TESTO_CORTO = "0x20-0x7E," + LETTERE + "0x2013,0x2014"

# Il trattino lungo c'e ovunque si mostri un valore: 11-collaudo.md §2 vuole
# "—" quando la temperatura non e disponibile, e un segnaposto che diventa un
# rettangolo vuoto e peggio del problema che segnala.
# Il corpo grande dello standby non mostra piu solo l'ora: mostra l'ora e la
# temperatura della stanza, affiancate e della stessa taglia. Servono quindi
# la virgola dei decimi, il grado e la C — e senza, si vedrebbero rettangoli
# vuoti nel numero piu grande dello schermo.
CIFRE_ORA = " 0123456789:,.°C—"
CIFRE_VAL = " 0123456789,.+-—"
CIFRE_GRADI = " 0123456789,.+-°—"

# --- piano ------------------------------------------------------------------
# ruolo -> (peso, insieme, bpp). L'ordine e quello dell'enum in fonts.h.
PIANO = [
    ("xxl",     "sb",   "simboli", CIFRE_VAL + ":%°", 4),
    ("xl",      "sb",   "range",   TESTO_CORTO,            4),
    ("l",       "sb",   "range",   TESTO,                  4),
    ("m",       "md",   "range",   TESTO,                  4),
    ("s",       "rg",   "range",   TESTO,                  4),
    ("xs",      "sb",   "range",   TESTO,                  4),
    ("standby", "sb",   "simboli", CIFRE_ORA,              4),
    ("standby_temp", "sb", "simboli", CIFRE_ORA,          4),
    # I cinque gradini della schermata di riposo, con l'insieme di
    # caratteri dell'orologio. Servono sia l'ora sia la temperatura: sono
    # la stessa roba — cifre, virgola, grado, C — e due scale separate
    # sarebbero due cose da tenere allineate per niente.
    ("standby_1", "sb", "simboli", CIFRE_ORA,             4),
    ("standby_2", "sb", "simboli", CIFRE_ORA,             4),
    ("standby_3", "sb", "simboli", CIFRE_ORA,             4),
    ("standby_4", "sb", "simboli", CIFRE_ORA,             4),
    ("standby_5", "sb", "simboli", CIFRE_ORA,             4),
    ("energia", "sb",   "simboli", CIFRE_VAL,              4),
    ("clima",   "sb",   "simboli", CIFRE_GRADI,            4),
    ("mono",    "mono", "range",   "0x20-0x7E",            4),
    # The tenths next to a big value: "18" in the xl size and ".7°" a
    # step below, so that the separator does not widen the number. The
    # scale's step would be l, and 38 to 26 is not a step but a jump: the
    # tenths would look like a footnote. Thirty keeps the digit readable as
    # part of the same number. Last in the list, because the order is the
    # enum's and the earlier roles must not move. Both separators are in
    # CIFRE_GRADI: the language decides which one is drawn.
    ("decimi_xl", "sb", "simboli", CIFRE_GRADI,           4),
]

# ruolo -> chiave del corpo dentro profili.json
CORPO = {
    "xxl": "f_xxl", "xl": "f_xl", "l": "f_l", "m": "f_m", "s": "f_s",
    "xs": "f_xs", "standby": "standby", "standby_temp": "standby_temp",
    "standby_1": "standby_1", "standby_2": "standby_2",
    "standby_3": "standby_3", "standby_4": "standby_4",
    "standby_5": "standby_5",
    "energia": "energia_home",
    "clima": "clima_dettaglio", "mono": "password_mono",
    "decimi_xl": "decimi_xl",
}

# Un profilo per famiglia basta: il verticale usa la scala del proprio
# orizzontale. Le famiglie si leggono da profili.json invece di stare scritte
# qui: una riga scritta a mano continuerebbe a far compilare corpi che
# nessun binario carica, sulla flash di un pannello che non li usa.
def campioni() -> dict:
    prof = json.loads(PROFILI.read_text(encoding="utf-8"))["profili"]
    fuori = {}
    for chiave, p in prof.items():
        fam = chiave.split("-", 1)[0]
        if fam in fuori:
            continue
        fuori[fam] = chiave
        # se c'e' l'orizzontale della stessa famiglia, e' lui il campione
        if p.get("orientamento") == "verticale":
            for altra, q in prof.items():
                if (altra.split("-", 1)[0] == fam
                        and q.get("orientamento") != "verticale"):
                    fuori[fam] = altra
                    break
    return fuori


def piano_completo() -> list[dict]:
    prof = json.loads(PROFILI.read_text(encoding="utf-8"))["profili"]
    voci = []
    for fam, chiave in campioni().items():
        font = prof[chiave]["font"]
        for ruolo, peso, modo, insieme, bpp in PIANO:
            corpo = int(font[CORPO[ruolo]])
            voci.append(dict(
                fam=fam, ruolo=ruolo, peso=peso, corpo=corpo,
                modo=modo, insieme=insieme, bpp=bpp,
                simbolo=f"font_{peso}_{corpo}",
                file=USCITA / f"font_{peso}_{corpo}.c"))
    # due famiglie possono chiedere lo stesso peso allo stesso corpo:
    # si compila una volta sola e si usa due volte
    visti, unici = set(), []
    for v in voci:
        if v["simbolo"] not in visti:
            visti.add(v["simbolo"])
            unici.append(v)
    return voci, unici


def compila(v: dict) -> None:
    sorgente = TTF[v["peso"]]
    if not sorgente.exists():
        raise SystemExit(
            f"manca {sorgente.relative_to(RADICE)}.\n"
            "I sorgenti sono Inter e JetBrains Mono, entrambi OFL, con la\n"
            "feature tnum congelata perche le cifre siano tabulari:\n"
            "  pip install opentype-feature-freezer\n"
            "  python -m opentype_feature_freezer.cli -f tnum -S -U Tab \\\n"
            "      Inter-SemiBold.ttf Inter-SemiBold-tnum.ttf")

    cmd = [
        "npx", "--yes", "lv_font_conv",
        # Relative to the project, and run from its root: lv_font_conv
        # writes these paths in the file's header, and an absolute one is
        # the folder of whoever built it.
        "--font", sorgente.relative_to(RADICE).as_posix(),
        "--size", str(v["corpo"]),
        "--bpp", str(v["bpp"]),
        "--format", "lvgl",
        "--no-compress",          # decomprimere a ogni glifo costa piu della flash
        "--lv-font-name", v["simbolo"],
        "-o", v["file"].relative_to(RADICE).as_posix(),
    ]
    cmd += (["--symbols", v["insieme"]] if v["modo"] == "simboli"
            else ["-r", v["insieme"]])

    r = subprocess.run(cmd, capture_output=True, text=True, cwd=RADICE,
                       shell=(sys.platform == "win32"))
    if r.returncode != 0 or not v["file"].exists():
        raise SystemExit(f"lv_font_conv fallito per {v['simbolo']}:\n{r.stderr}{r.stdout}")


INTESTAZIONE_H = '''/* ------------------------------------------------------------------------
 * fonts.h — i corpi tipografici di 09-profili.md §4.
 *
 * GENERATO da tools/genera_font.py. Non modificare a mano.
 *
 * Il codice non nomina mai un corpo: chiede un *ruolo* e il profilo attivo
 * decide quale font compilato serve.
 *
 *     lv_obj_set_style_text_font(l, font(FT_M), 0);
 *
 * I numeri che compaiono nei .c generati sono identita di font, non misure
 * di layout: quelle stanno solo in profile.h.
 * --------------------------------------------------------------------- */
#ifndef FONTS_H
#define FONTS_H

#include "lvgl.h"

typedef enum {'''

INTESTAZIONE_C = '''/* GENERATO da tools/genera_font.py — non modificare a mano. */
#include "fonts.h"

#include "profile.h"

'''


def scrivi_intestazioni(voci: list[dict]) -> None:
    ruoli = [r for r, *_ in PIANO]

    h = [INTESTAZIONE_H]
    for r in ruoli:
        h.append(f"    FT_{r.upper()},")
    h.append("    FT_QUANTI,\n} font_ruolo_t;\n")
    h.append("/* Il font del ruolo, per il profilo attivo. Mai NULL. */")
    h.append("const lv_font_t *font(font_ruolo_t ruolo);\n")
    h.append("#endif /* FONTS_H */")
    (USCITA / "fonts.h").write_text("\n".join(h) + "\n", encoding="utf-8",
                                    newline="\n")

    per_fam: dict[str, dict[str, str]] = {}
    for v in voci:
        per_fam.setdefault(v["fam"], {})[v["ruolo"]] = v["simbolo"]

    c = [INTESTAZIONE_C]
    for s in sorted({v["simbolo"] for v in voci}):
        c.append(f"LV_FONT_DECLARE({s});")
    c.append("")
    c.append("static const lv_font_t *const TABELLA[FAM_QUANTE][FT_QUANTI] = {")
    for fam in campioni():
        c.append(f"    [FAM_{fam.upper()}] = {{")
        for r in ruoli:
            c.append(f"        [FT_{r.upper()}] = &{per_fam[fam][r]},")
        c.append("    },")
    c.append("};\n")
    c.append('''const lv_font_t *font(font_ruolo_t ruolo)
{
    if ((unsigned)ruolo >= (unsigned)FT_QUANTI) ruolo = FT_M;
    const lv_font_t *f = TABELLA[PRF->famiglia][ruolo];
    return f ? f : LV_FONT_DEFAULT;
}''')
    (USCITA / "fonts.c").write_text("\n".join(c) + "\n", encoding="utf-8",
                                    newline="\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tutto", action="store_true", help="ricompila anche cio che c'e")
    ap.add_argument("--elenca", action="store_true", help="mostra il piano ed esce")
    arg = ap.parse_args()

    voci, unici = piano_completo()
    USCITA.mkdir(parents=True, exist_ok=True)

    if arg.elenca:
        print(f"{'famiglia':9s} {'ruolo':9s} {'peso':5s} {'corpo':>5s}  "
              f"{'bpp':>3s}  {'byte':>8s}  simbolo")
        for v in voci:
            b = v["file"].stat().st_size if v["file"].exists() else 0
            print(f"{v['fam']:9s} {v['ruolo']:9s} {v['peso']:5s} {v['corpo']:5d}  "
                  f"{v['bpp']:3d}  {b:8d}  {v['simbolo']}")
        tot = sum(v["file"].stat().st_size for v in unici if v["file"].exists())
        print(f"\n{len(unici)} font, {tot / 1024:.0f} kB di sorgente C")
        return 0

    if not shutil.which("npx") and not shutil.which("npx.cmd"):
        raise SystemExit("serve Node per lv_font_conv: https://nodejs.org")

    fatti = 0
    for v in unici:
        if v["file"].exists() and not arg.tutto:
            continue
        print(f"  {v['simbolo']:16s} corpo {v['corpo']:3d}  {v['peso']}")
        compila(v)
        fatti += 1

    scrivi_intestazioni(voci)
    print(f"\n{fatti} font compilati, {len(unici)} in totale; "
          f"fonts.h e fonts.c riscritti")
    return 0


if __name__ == "__main__":
    sys.exit(main())
