#!/usr/bin/env python3
"""
Confronta profili.json con le misure vere dei mockup.

    python tools/estrai_misure.py            elenca solo le divergenze
    python tools/estrai_misure.py --tutto    elenca tutte le misure
    python tools/estrai_misure.py --json     scrive il referto in forma JSON

I mockup sono rendering 1:1 delle schermate approvate: quando una misura
manca da profili.json o non torna, la risposta si legge li dentro. Piu di
un mockup mostra la stessa parte di interfaccia e le revisioni sono
scivolate di qualche pixel; lo script riporta *tutte* le fonti, cosi la
deriva si vede invece di essere scelta a caso.

Regola con cui sono state risolte le divergenze, applicata a mano:
  - scarto entro 2 px  -> vince profili.json / 09-profili.md
  - misura assente o scarto grosso -> vince il mockup, e si annota
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from css_mockup import RADICE, foglio  # noqa: E402

PROFILI = RADICE / "docs" / "profili.json"

# Quale mockup mostra quale profilo, e con che variante di selettore.
FONTI: dict[str, dict[str, tuple[str, str | None]]] = {
    "principale": {
        "p4-1280x800": ("10-pollici/p4-pannello-1280x800.html", None),
        "p4-800x1280": ("verticale/sezioni-verticale.html", None),
    },
    "clima": {
        "p4-1280x800": ("clima-condizionatori.html", None),
        "p4-800x1280": ("verticale/sezioni-verticale.html", None),
    },
    "wifi": {
        "p4-1280x800": ("10-pollici/p4-stati-flussi-1280x800.html", None),
        "p4-800x1280": ("verticale/stati-verticale.html", None),
    },
    "stati": {
        "p4-1280x800": ("10-pollici/p4-stati-flussi-1280x800.html", None),
        "p4-800x1280": ("verticale/stati-verticale.html", None),
    },
}

ORIZZONTALI = ("p4-1280x800",)
VERTICALI = ("p4-800x1280",)
TUTTI = ORIZZONTALI + VERTICALI

# Dove profili.json diverge dal mockup di proposito, con il motivo.
# Serve a distinguere una scelta da una svista: senza, la prima rilettura
# del referto "corregge" un valore deciso apposta.
ECCEZIONI: dict[tuple[str, str], str] = {
}

# chiave in profili.json -> come si misura sul mockup.
# modo: px | corpo | var | griglia
MISURE: list[dict] = [
    # ---- telaio comune ----
    dict(chiave="geometria.rail_w", fonti=["principale", "clima", "wifi", "stati"],
         sel=".mrail", prop="width", modo="px", profili=ORIZZONTALI),
    dict(chiave="geometria.rail_home", fonti=["principale", "wifi"],
         sel=".mhome", prop=("width", "height"), modo="px2", profili=ORIZZONTALI),
    dict(chiave="geometria.rail_gear", fonti=["wifi", "stati"],
         sel=".mgear", prop=("width", "height"), modo="px2", profili=ORIZZONTALI),
    dict(chiave="geometria.head_h", fonti=["principale", "clima"],
         sel=".mhead", prop="height", modo="px", profili=ORIZZONTALI),
    dict(chiave="geometria.head_h", fonti=["principale"],
         sel=".sechead", prop="height", modo="px", profili=VERTICALI),
    dict(chiave="geometria.home_head_h", fonti=["principale"],
         sel=".hh", prop="height", modo="px", profili=TUTTI),
    dict(chiave="geometria.navbar_h", fonti=["principale"],
         sel=".navbar", prop="height", modo="px", profili=VERTICALI),
    dict(chiave="geometria.radius", fonti=["principale", "clima"],
         var="--r", modo="var", profili=TUTTI),
    dict(chiave="geometria.pad", fonti=["clima", "principale"],
         var="--pad", modo="var", profili=TUTTI),
    dict(chiave="geometria.gap", fonti=["clima", "principale"],
         var="--gap", modo="var", profili=TUTTI),

    # ---- tipografia di scala ----
    dict(chiave="font.f_xxl", fonti=["principale"],
         sel=".hh .t", modo="corpo", profili=ORIZZONTALI),
    dict(chiave="font.f_l", fonti=["principale"],
         sel=".mhead .ti", modo="corpo", profili=ORIZZONTALI),
    dict(chiave="font.energia_home", fonti=["principale"],
         sel=".en-main .v", modo="corpo", profili=ORIZZONTALI),

    # ---- griglie ----
    dict(chiave="griglie.luci", fonti=["principale"],
         sel=".g4", modo="griglia", profili=("p4-1280x800",)),

    # ---- home ----
    dict(chiave="home.fascia_h", fonti=["principale"],
         sel=".band", prop="height", modo="px", profili=ORIZZONTALI),
    dict(chiave="home.presenza_w", fonti=["principale"],
         sel=".c-pres", prop="width", modo="px", profili=ORIZZONTALI),
    dict(chiave="home.energia_w", fonti=["principale"],
         sel=".c-en", prop="width", modo="px", profili=ORIZZONTALI),
    dict(chiave="home.aperture_w", fonti=["principale"],
         sel=".c-open", prop="width", modo="px", profili=ORIZZONTALI),
    dict(chiave="home.agenda_w", fonti=["principale"],
         sel=".c-ag", prop="width", modo="px", profili=ORIZZONTALI),

    # ---- aree di tocco ----
    dict(chiave="tocco.interruttore", fonti=["principale"],
         sel=".lc .sw", prop=("width", "height"), modo="px2", profili=ORIZZONTALI),
    dict(chiave="tocco.clima_pm", fonti=["clima"],
         sel=".zone .set .b", prop=("width", "height"), modo="px2", profili=TUTTI),

    # ---- padding interno delle schede ----
    # Non e il padding del contenuto, e nei mockup e piu stretto sopra e
    # sotto che ai lati. Tre selettori diversi perche i tre mockup chiamano
    # la scheda in tre modi.
    dict(chiave="geometria.pad_scheda", fonti=["principale"],
         sel=".cardp", modo="pad2", profili=("p4-1280x800",)),
    dict(chiave="geometria.pad_scheda", fonti=["principale"],
         sel=".card", modo="pad2", profili=VERTICALI),

    # ---- righe a pastiglia del dettaglio condizionatore ----
    dict(chiave="clima.segmento_h", fonti=["clima"],
         sel=".seg", prop="height", modo="px", profili=ORIZZONTALI),
    dict(chiave="clima.extra_h", fonti=["clima"],
         sel=".exb", prop="height", modo="px", profili=ORIZZONTALI),
]


def leggi(percorso: str, dato: dict):
    cur = dato
    for pezzo in percorso.split("."):
        if not isinstance(cur, dict) or pezzo not in cur:
            return "(assente)"
        cur = cur[pezzo]
    return cur


def misura(voce: dict, profilo: str, fonte: str):
    if profilo not in FONTI[fonte]:
        return None
    rel, variante = FONTI[fonte][profilo]
    f = foglio(rel)
    modo = voce["modo"]
    if modo == "px":
        return f.px(voce["sel"], voce["prop"], variante)
    if modo == "px2":
        a = f.px(voce["sel"], voce["prop"][0], variante)
        b = f.px(voce["sel"], voce["prop"][1], variante)
        return None if a is None and b is None else [a, b]
    if modo == "pad2":
        # `padding: 14px 16px` e una scorciatoia: prima il verticale, poi
        # l'orizzontale. In profili.json l'ordine e quello di misura_t,
        # cioe larghezza poi altezza: qui si girano.
        v = f.valore(voce["sel"], "padding", variante)
        if v is None:
            return None
        n = [float(x) for x in re.findall(r"(-?[\d.]+)px", v)]
        if len(n) == 1:
            return [n[0], n[0]]
        if len(n) >= 2:
            return [n[1], n[0]]
        return None
    if modo == "corpo":
        return f.corpo(voce["sel"], variante)
    if modo == "var":
        return f.var_px(voce["var"], variante)
    if modo == "griglia":
        g = f.griglia(voce["sel"], variante)
        return None if g is None else list(g)
    raise ValueError(modo)


def normalizza(v):
    if isinstance(v, float):
        return int(v) if v == int(v) else v
    if isinstance(v, list):
        return [normalizza(x) for x in v]
    return v


def concorda(atteso, trovato) -> bool:
    """Vero se le due misure sono la stessa cosa entro 2 px."""
    if atteso is None or trovato is None or atteso == "(assente)":
        return False
    if isinstance(atteso, list) and isinstance(trovato, list):
        return len(atteso) == len(trovato) and all(
            concorda(a, b) for a, b in zip(atteso, trovato))
    try:
        return abs(float(atteso) - float(trovato)) <= 2
    except (TypeError, ValueError):
        return atteso == trovato


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tutto", action="store_true", help="elenca anche cio che torna")
    ap.add_argument("--json", action="store_true", help="referto in forma JSON")
    arg = ap.parse_args()

    profili = json.loads(PROFILI.read_text(encoding="utf-8"))["profili"]
    referto = []

    for voce in MISURE:
        for profilo in voce["profili"]:
            atteso = normalizza(leggi(voce["chiave"], profili[profilo]))
            trovate = {}
            for fonte in voce["fonti"]:
                v = normalizza(misura(voce, profilo, fonte))
                if v is not None and v != [None, None]:
                    trovate.setdefault(json.dumps(v), []).append(fonte)
            if not trovate:
                continue
            valori = [json.loads(k) for k in trovate]
            ok = any(concorda(atteso, v) for v in valori)
            motivo = ECCEZIONI.get((voce["chiave"], profilo))
            referto.append(dict(chiave=voce["chiave"], profilo=profilo,
                                profili_json=atteso,
                                mockup={f"{','.join(v)}": json.loads(k)
                                        for k, v in trovate.items()},
                                concorda=ok, deriva=len(trovate) > 1,
                                eccezione=motivo))

    if arg.json:
        print(json.dumps(referto, ensure_ascii=False, indent=2))
        return 0

    div = [r for r in referto if not r["concorda"] and not r["eccezione"]]
    ecc = [r for r in referto if not r["concorda"] and r["eccezione"]]
    der = [r for r in referto if r["concorda"] and r["deriva"]]

    if div:
        print("DIVERGENZE — profili.json non torna col mockup\n")
        for r in div:
            print(f"  {r['chiave']:28s} {r['profilo']:14s} "
                  f"json={r['profili_json']!s:12s} mockup={r['mockup']}")
        print()
    if ecc:
        print("DIVERGENZE VOLUTE — profili.json si discosta dal mockup "
              "con un motivo\n")
        for r in ecc:
            print(f"  {r['chiave']:28s} {r['profilo']:14s} "
                  f"json={r['profili_json']!s:12s} mockup={r['mockup']}")
            print(f"      {r['eccezione']}")
        print()
    if der:
        print("DERIVA FRA MOCKUP — stessa misura, revisioni diverse "
              "(entro 2 px, vince profili.json)\n")
        for r in der:
            print(f"  {r['chiave']:28s} {r['profilo']:14s} "
                  f"json={r['profili_json']!s:12s} mockup={r['mockup']}")
        print()
    if arg.tutto:
        print("TUTTE LE MISURE\n")
        for r in referto:
            segno = "ok " if r["concorda"] else "!! "
            print(f"  {segno}{r['chiave']:28s} {r['profilo']:14s} "
                  f"json={r['profili_json']!s:12s} mockup={r['mockup']}")
        print()

    print(f"{len(referto)} misure confrontate, {len(div)} divergenze, "
          f"{len(ecc)} volute, {len(der)} con deriva fra mockup")
    return 1 if div else 0


if __name__ == "__main__":
    sys.exit(main())
