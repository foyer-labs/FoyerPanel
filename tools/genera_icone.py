#!/usr/bin/env python3
"""
Compila le icone in un font-icona LVGL.

    python tools/genera_icone.py             compila cio che manca
    python tools/genera_icone.py --tutto     ricompila tutto
    python tools/genera_icone.py --elenca    mostra il piano

Serve Node (lv_font_conv) e fontTools.

02-design-tokens.md chiede tratto 1,6 px su griglia 24x24 con terminazioni
arrotondate, e icone in font-icona o immagini precompilate, **mai SVG a
runtime**. Material Symbols Rounded (Apache-2.0) e un font variabile: lo si
istanzia a wght 300 / opsz 24 / GRAD 0 / FILL 0, che e dove il tratto cade
sul valore giusto, e lo si riduce alle sole icone in uso.

Quattro corpi per famiglia — i primi tre dalle misure dei mockup, da 15 a
31 px; il quarto solo per la lampadina delle luci
e si agganciano a questi tre, come i corpi tipografici si agganciano alla
scala di 09-profili.md §4.

Le cinque posizioni del deflettore NON sono qui: 02-design-tokens.md dice
che sono un disegno parametrico — corpo macchina piu lamella a cinque
altezze — e vanno generate con le primitive di LVGL, non cercate in una
libreria.
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
USCITA = RADICE / "main" / "ui" / "icons"
PROFILI = RADICE / "docs" / "profili.json"

VARIABILE = SORGENTI / "MaterialSymbolsRounded.ttf"
ISTANZA = SORGENTI / "MaterialSymbolsRounded-w300.ttf"

# Dove cade il tratto di 1,6 px su griglia 24: peso 300, dimensione ottica 24.
ASSI = {"wght": 300, "FILL": 0, "GRAD": 0, "opsz": 24}

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

# --- le icone in uso --------------------------------------------------------
# nome in Material Symbols -> nome nel codice. Raggruppate per dove servono,
# cosi quando una schermata cambia si sa quali toccare.
ICONE: dict[str, list[str]] = {
    "navigazione": [
        "home", "lightbulb", "device_thermostat", "solar_power", "videocam",
        "garage", "calendar_month", "wifi", "settings", "mop",
    ],
    "home": [
        "person", "bolt", "battery_full", "sensor_window", "lock", "lock_open",
        "notifications", "notifications_off", "schedule",
        # Lavatrice e asciugatrice: due icone distinte e non una sola
        # ripetuta, perche in una scheda divisa in due meta uguali
        # l'icona e la sola cosa che dice quale meta stai guardando.
        "local_laundry_service", "dry",
    ],
    "meteo": ["sunny", "cloud", "rainy", "bedtime"],
    # Il riscaldamento acceso di un piano. **Non e' una fiamma**, ed e' una
    # scelta: sullo stesso vetro c'e' gia' `local_fire_department` a dire
    # un'altra cosa — «questa stanza sta chiedendo calore adesso» — e due
    # fiamme quasi uguali a venti pixel di distanza renderebbero illeggibili
    # tutti e due i significati. Onde di calore contro fiamma: si
    # distinguono con la coda dell'occhio.
    "riscaldamento": ["heat"],
    "primo_avvio": ["signal_wifi_4_bar", "network_check", "done"],
    "accessi": ["fence", "gate", "door_front", "door_open"],
    "clima": [
        "local_fire_department", "ac_unit", "autorenew", "water_drop",
        "mode_fan", "power_settings_new", "timer", "air", "volume_off",
        "light_mode", "swap_vert", "height", "wind_power",
    ],
    "comandi": [
        "add", "remove", "check", "close", "arrow_back", "arrow_forward",
        "chevron_left", "chevron_right", "expand_more", "keyboard_arrow_up",
        "more_horiz", "done", "visibility", "visibility_off", "content_copy",
        "backspace", "keyboard_capslock", "space_bar", "keyboard",
    ],
    "stati": [
        "warning", "error", "info", "cloud_off", "refresh", "sync",
        "sync_problem", "wifi_off", "hourglass_empty", "update",
    ],
    # Le uniche che chi configura puo **scegliere**: la sezione Interruttori
    # lascia decidere l'icona di ogni voce dalla pagina web. E un elenco
    # chiuso e non un campo libero, perche i caratteri-icona sono bitmap
    # compilati qui: un nome fuori elenco non darebbe errore, darebbe un
    # rettangolo vuoto sul vetro.
    #
    # Diciassette erano gia compilate per altre schermate e non costano
    # niente; undici sono nuove, e si vedono: le icone passano da 85 a 96 e
    # i sorgenti generati da 324 a 360 kB.
    # Aggiungerne una e una riga qui piu una nell'enum di
    # config.schema.json, e tools/verifica_icone_scelta.py pretende che le
    # due restino uguali.
    "interruttori": [
        "outlet", "power_settings_new", "lightbulb", "bolt", "cable",
        "mode_fan", "wind_power", "ac_unit", "local_fire_department",
        "heat_pump", "water_drop", "valve", "pool", "shower", "grass",
        "sunny", "tv", "speaker", "coffee", "kitchen", "videocam",
        "router", "storage", "garage", "door_front", "speed", "timer",
        "sensors",
    ],
    "diagnostica": [
        "qr_code_2", "delete", "memory", "storage", "speed", "bug_report",
        "filter_alt", "restart_alt", "brightness_medium", "network_check",
        "router", "sensors", "tune",
    ],
}


def tutte() -> list[str]:
    viste, fuori = set(), []
    for gruppo in ICONE.values():
        for n in gruppo:
            if n not in viste:
                viste.add(n)
                fuori.append(n)
    return fuori


def codepoint() -> dict[str, int]:
    from fontTools.ttLib import TTFont
    if not VARIABILE.exists():
        raise SystemExit(
            f"manca {VARIABILE.relative_to(RADICE)}.\n"
            "E il font variabile Material Symbols Rounded, Apache-2.0:\n"
            "  https://github.com/google/material-design-icons"
            "/raw/master/variablefont/\n"
            "  MaterialSymbolsRounded%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf")
    cmap = TTFont(VARIABILE).getBestCmap()
    rovescio: dict[str, int] = {}
    for cp, nome in cmap.items():
        rovescio.setdefault(nome, cp)
    mancanti = [n for n in tutte() if n not in rovescio]
    if mancanti:
        raise SystemExit(f"icone che il font non ha: {', '.join(mancanti)}")
    return {n: rovescio[n] for n in tutte()}


def istanzia() -> None:
    """Congela gli assi del font variabile: LVGL vuole un font statico."""
    from fontTools import subset
    from fontTools.ttLib import TTFont
    from fontTools.varLib import instancer

    print(f"  istanzio a {', '.join(f'{k} {v}' for k, v in ASSI.items())}")
    f = instancer.instantiateVariableFont(TTFont(VARIABILE), ASSI, inplace=False)

    cp = codepoint()
    o = subset.Options(layout_features=[], notdef_outline=True,
                       glyph_names=True, recommended_glyphs=True)
    s = subset.Subsetter(options=o)
    s.populate(unicodes=list(cp.values()))
    s.subset(f)
    f.save(ISTANZA)
    print(f"  {ISTANZA.name}  {ISTANZA.stat().st_size / 1024:.0f} kB, "
          f"{len(cp)} icone")


# Il quarto corpo esiste per una cosa sola: la lampadina al centro della
# scheda di una luce, che e insieme lo stato e il bersaglio del dito. Un font
# intero a cinquantasei pixel costerebbe mezzo megabyte di flash per
# un'icona; ridotto a quella che serve, sono pochi kilobyte.
#
# Chi aggiunge un'icona grande la aggiunge **qui**, non nella lista generale:
# la differenza si paga in flash, e va vista.
# `heat` sta qui perche' nello standby va accanto a una temperatura di 84 px
# e deve vedersi dalla porta: a 30 px sarebbe un dettaglio. Il costo si paga
# in flash — il font da 64 px conteneva una sola icona, 8,8 kB — ed e'
# esattamente il motivo per cui questo elenco esiste separato.
ICONE_XL = ["lightbulb", "heat"]


def piano() -> list[dict]:
    prof = json.loads(PROFILI.read_text(encoding="utf-8"))["profili"]
    voci = []
    for fam, chiave in campioni().items():
        ic = prof[chiave]["icone"]
        for ruolo in ("s", "m", "l", "xl"):
            corpo = int(ic[ruolo])
            voci.append(dict(fam=fam, ruolo=ruolo, corpo=corpo,
                             simbolo=f"icone_{corpo}",
                             file=USCITA / f"icone_{corpo}.c"))
    visti, unici = set(), []
    for v in voci:
        if v["simbolo"] not in visti:
            visti.add(v["simbolo"])
            unici.append(v)
    return voci, unici


def compila(v: dict, cp: dict[str, int]) -> None:
    if v["ruolo"] == "xl":
        cp = {k: c for k, c in cp.items() if k in ICONE_XL}

    intervalli = ",".join(hex(c) for c in sorted(cp.values()))
    cmd = [
        "npx", "--yes", "lv_font_conv",
        # Relative to the project, and run from its root: lv_font_conv
        # writes these paths in the file's header, and an absolute one is
        # the folder of whoever built it.
        "--font", ISTANZA.relative_to(RADICE).as_posix(),
        "--size", str(v["corpo"]),
        "--bpp", "4",
        "--format", "lvgl",
        "--no-compress",
        "--lv-font-name", v["simbolo"],
        "-r", intervalli,
        "-o", v["file"].relative_to(RADICE).as_posix(),
    ]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=RADICE,
                       shell=(sys.platform == "win32"))
    if r.returncode != 0 or not v["file"].exists():
        raise SystemExit(f"lv_font_conv fallito per {v['simbolo']}:\n"
                         f"{r.stderr}{r.stdout}")


def utf8(cp: int) -> str:
    return "".join(f"\\x{b:02X}" for b in chr(cp).encode("utf-8"))


def scrivi_intestazioni(voci: list[dict], cp: dict[str, int]) -> None:
    h = ['''/* ------------------------------------------------------------------------
 * icons.h — le icone in uso, come stringhe UTF-8.
 *
 * GENERATO da tools/genera_icone.py. Non modificare a mano: l'icona nuova si
 * aggiunge all'elenco dello script, che la ritrova nel font e ricompila.
 *
 * Sono normalissime stringhe, quindi si usano come tali:
 *
 *     lv_label_set_text(l, ICO_LUCE);
 *     lv_obj_set_style_text_font(l, icona(IC_M), 0);
 *
 * Font-icona, non SVG: 02-design-tokens.md lo vieta a runtime.
 * --------------------------------------------------------------------- */
#ifndef ICONS_H
#define ICONS_H

#include "lvgl.h"

/* Quattro corpi, che scalano col profilo. Il quarto — IC_XL — esiste per
   una cosa sola: la lampadina al centro della scheda di una luce, che e
   insieme lo stato e il bersaglio del dito. Il suo font contiene **solo**
   le icone elencate in ICONE_XL dentro tools/genera_icone.py, perche a
   quella dimensione un font completo costerebbe mezzo megabyte di flash.
   Chiedere IC_XL per un'icona che non e in quell'elenco da un rettangolo
   vuoto: si aggiunge li, dove il costo si vede. */
typedef enum { IC_S, IC_M, IC_L, IC_XL, IC_QUANTI } icona_corpo_t;

/* Il font-icona del corpo, per il profilo attivo. Mai NULL. */
const lv_font_t *icona(icona_corpo_t corpo);
''']

    for gruppo, nomi in ICONE.items():
        h.append(f"\n/* --- {gruppo} --- */")
        for n in nomi:
            simbolo = "ICO_" + n.upper()
            h.append(f'#define {simbolo:34s} "{utf8(cp[n])}"'
                     f'  /* U+{cp[n]:04X} {n} */')

    h.append('''
/* --- l'icona scelta in configurazione -----------------------------------
 *
 * La sezione Interruttori lascia scegliere l'icona di ogni voce dalla
 * pagina web, e in config.json quella scelta e una **stringa**: "outlet",
 * "mode_fan". Qui si traduce nel glifo.
 *
 * L'elenco e chiuso — il gruppo `interruttori` di tools/genera_icone.py — e
 * lo schema propone gli stessi nomi. Un nome che non c'e torna il ripiego
 * invece di NULL: chi disegna non deve avere un ramo in piu, e un
 * interruttore senza icona valida e comunque un interruttore da mostrare. */
const char *icona_da_nome(const char *nome, const char *ripiego);

/* Quante ne puo scegliere chi configura, e la n-esima: servono alla prova
   che confronta questo elenco con l'enum dello schema. */
int         icone_scegliibili(void);
const char *icona_scegliibile(int n);''')
    h.append("\n#endif /* ICONS_H */")
    (USCITA / "icons.h").write_text("\n".join(h) + "\n", encoding="utf-8")

    per_fam: dict[str, dict[str, str]] = {}
    for v in voci:
        per_fam.setdefault(v["fam"], {})[v["ruolo"]] = v["simbolo"]

    c = ['/* GENERATO da tools/genera_icone.py — non modificare a mano. */',
         '#include "icons.h"', '', '#include "profile.h"', '']
    for s in sorted({v["simbolo"] for v in voci}):
        c.append(f"LV_FONT_DECLARE({s});")
    c.append("")
    c.append("static const lv_font_t *const TABELLA[FAM_QUANTE][IC_QUANTI] = {")
    for fam in campioni():
        c.append(f"    [FAM_{fam.upper()}] = {{")
        for r, e in (("s", "IC_S"), ("m", "IC_M"), ("l", "IC_L"),
                     ("xl", "IC_XL")):
            c.append(f"        [{e}] = &{per_fam[fam][r]},")
        c.append("    },")
    c.append("};\n")
    c.append('''const lv_font_t *icona(icona_corpo_t corpo)
{
    if ((unsigned)corpo >= (unsigned)IC_QUANTI) corpo = IC_M;
    const lv_font_t *f = TABELLA[PRF->famiglia][corpo];
    return f ? f : LV_FONT_DEFAULT;
}''')
    scelte = ICONE["interruttori"]
    c.append("\n/* --- l'icona scelta in configurazione --- */")
    c.append("static const struct { const char *nome, *glifo; } SCELTE[] = {")
    for n in scelte:
        c.append(f'    {{ "{n}", ICO_{n.upper()} }},')
    c.append("};")
    c.append(f"#define SCELTE_QUANTE {len(scelte)}\n")
    c.append('''const char *icona_da_nome(const char *nome, const char *ripiego)
{
    if (!nome || !*nome) return ripiego;
    for (int n = 0; n < SCELTE_QUANTE; n++) {
        const char *a = SCELTE[n].nome, *b = nome;
        while (*a && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0) return SCELTE[n].glifo;
    }
    return ripiego;
}

int icone_scegliibili(void) { return SCELTE_QUANTE; }

const char *icona_scegliibile(int n)
{
    return (n >= 0 && n < SCELTE_QUANTE) ? SCELTE[n].nome : NULL;
}''')
    (USCITA / "icons.c").write_text("\n".join(c) + "\n", encoding="utf-8")


# --- il carattere-icona della pagina di configurazione ---------------------
#
# La pagina lascia scegliere l'icona di un interruttore o di una
# programmazione da un elenco chiuso di nomi. Un nome come `mode_fan` o
# `heat_pump` non dice cosa si vedra sul vetro, e l'unico modo di scoprirlo
# era salvare e andare a guardare il pannello.
#
# Percio la pagina si porta dentro un **sottoinsieme** del carattere-icona:
# le sole scegliibili, ventotto, quattro kilobyte in woff2. Non un CDN e non
# un file a parte: la pagina la serve il pannello dalla propria flash e deve
# funzionare il giorno che la linea e giu, che e proprio il giorno in cui la
# si apre.
#
# Si genera **qui** e non a mano perche qui si sa gia quale glifo ha quale
# nome: e la stessa tabella con cui si compilano i font del pannello, quindi
# l'anteprima non puo mostrare un'icona diversa da quella che comparira.
def scrivi_pagina_web(cp: dict[str, int]) -> int:
    import base64
    import json as _json

    pagina = RADICE / "web" / "index.html"
    schema = RADICE / "docs" / "config.schema.json"
    if not pagina.exists() or not schema.exists():
        return 0

    # Quali sono scegliibili lo dice lo schema, che e la stessa fonte da cui
    # la pagina costruisce le tendine.
    d = _json.loads(schema.read_text(encoding="utf-8"))
    voci = d["properties"]["switches"]["items"]["properties"]["icon"]
    scelte = sorted(set((voci.get("anyOf", [{}])[0] if "anyOf" in voci else voci)
                        .get("enum", [])))
    scelte = [n for n in scelte if n in cp]
    if not scelte:
        return 0

    try:
        from fontTools.ttLib import TTFont
        from fontTools.subset import Subsetter, Options
    except ImportError:
        print("  (fonttools assente: l'anteprima delle icone resta com'era)")
        return 0

    f = TTFont(ISTANZA)
    o = Options()
    o.flavor = "woff2"
    o.desubroutinize = True
    o.layout_features = []      # niente legature: si indirizza per codepoint
    o.name_IDs = []
    o.notdef_outline = False
    s = Subsetter(options=o)
    s.populate(unicodes=[cp[n] for n in scelte])
    s.subset(f)

    import io as _io
    buf = _io.BytesIO()
    f.flavor = "woff2"
    f.save(buf)
    b64 = base64.b64encode(buf.getvalue()).decode("ascii")

    glifi = "{" + ",".join(f'"{n}":"\\u{cp[n]:04x}"' for n in scelte) + "}"

    testo = pagina.read_text(encoding="utf-8")
    blocco = (
        "/* GENERATO da tools/genera_icone.py — non si scrive a mano */\n"
        "@font-face{font-family:\"IconePannello\";font-display:block;\n"
        f"  src:url(data:font/woff2;base64,{b64}) format(\"woff2\")}}\n"
    )
    testo = fra_segni(testo, "ICONE-FONT", blocco)
    testo = fra_segni(testo, "ICONE-GLIFI",
                      "/* GENERATO da tools/genera_icone.py */\n"
                      f"const GLIFI = {glifi};\n")
    pagina.write_text(testo, encoding="utf-8", newline="")
    return len(scelte)


# Sostituisce quello che sta fra due segni, o lo aggiunge se i segni non ci
# sono ancora. I segni restano nel file: e cosi che si vede, leggendolo, che
# quel pezzo non si tocca a mano.
def fra_segni(testo: str, nome: str, dentro: str) -> str:
    apri, chiudi = f"/* <{nome}> */", f"/* </{nome}> */"
    i, j = testo.find(apri), testo.find(chiudi)
    if i < 0 or j < 0:
        raise SystemExit(f"segni {nome} non trovati in web/index.html")
    return testo[:i + len(apri)] + "\n" + dentro + testo[j:]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tutto", action="store_true")
    ap.add_argument("--elenca", action="store_true")
    arg = ap.parse_args()

    voci, unici = piano()
    USCITA.mkdir(parents=True, exist_ok=True)

    if arg.elenca:
        cp = codepoint()
        for gruppo, nomi in ICONE.items():
            print(f"\n{gruppo}")
            for n in nomi:
                print(f"  U+{cp[n]:04X}  {n}")
        print(f"\n{len(cp)} icone, {len(unici)} corpi:")
        for v in unici:
            b = v["file"].stat().st_size if v["file"].exists() else 0
            print(f"  {v['simbolo']:12s} corpo {v['corpo']:3d}  {b:8d} byte")
        return 0

    if not shutil.which("npx") and not shutil.which("npx.cmd"):
        raise SystemExit("serve Node per lv_font_conv: https://nodejs.org")

    cp = codepoint()
    if not ISTANZA.exists() or arg.tutto:
        istanzia()

    fatti = 0
    for v in unici:
        if v["file"].exists() and not arg.tutto:
            continue
        print(f"  {v['simbolo']:12s} corpo {v['corpo']:3d}")
        compila(v, cp)
        fatti += 1

    scrivi_intestazioni(voci, cp)
    n_web = scrivi_pagina_web(cp)
    print(f"\n{fatti} font-icona compilati, {len(unici)} in totale, "
          f"{len(cp)} icone; icons.h e icons.c riscritti")
    print(f"pagina web: {n_web} icone scegliibili nel carattere incorporato")
    return 0


if __name__ == "__main__":
    sys.exit(main())
