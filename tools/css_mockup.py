#!/usr/bin/env python3
"""
Lettore del CSS dei mockup.

I mockup in docs/mockup/ sono rendering 1:1 delle schermate approvate: sono
la fonte delle misure, non un'illustrazione. Questo modulo li legge come
fossero un foglio di stile e permette di interrogarli per selettore.

Non fa nulla di generale: implementa il minimo di CSS che i mockup usano
davvero, cioe regole piatte, proprieta custom sul contenitore .screen e la
scorciatoia `font:`.
"""
from __future__ import annotations

import re
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
MOCKUP = RADICE / "docs" / "mockup"

_COMMENTI = re.compile(r"/\*.*?\*/", re.S)
_STILE = re.compile(r"<style>(.*?)</style>", re.S | re.I)
_REGOLA = re.compile(r"([^{}]+)\{([^{}]*)\}", re.S)
_PX = re.compile(r"(-?\d+(?:\.\d+)?)px")
_VAR = re.compile(r"var\(\s*(--[\w-]+)\s*(?:,\s*([^)]*))?\)")

# la scorciatoia `font:` dei mockup e sempre  peso  corpo/interlinea  famiglia
_FONT = re.compile(r"^\s*(\d+)\s+(-?\d+(?:\.\d+)?)px\s*/")


class Foglio:
    """Il CSS di un mockup, interrogabile per selettore."""

    def __init__(self, percorso: Path):
        self.percorso = percorso
        testo = percorso.read_text(encoding="utf-8", errors="replace")
        css = "\n".join(_STILE.findall(testo))
        css = _COMMENTI.sub("", css)
        self.regole: list[tuple[list[str], dict[str, str]]] = []
        for selettori, corpo in _REGOLA.findall(css):
            if selettori.strip().startswith("@"):
                continue
            sel = [s.strip() for s in selettori.split(",") if s.strip()]
            decl: dict[str, str] = {}
            for pezzo in corpo.split(";"):
                if ":" not in pezzo:
                    continue
                nome, _, valore = pezzo.partition(":")
                decl[nome.strip()] = valore.strip()
            if sel and decl:
                self.regole.append((sel, decl))

    # ---- proprieta custom -------------------------------------------------

    def variabili(self, variante: str | None = None) -> dict[str, str]:
        """Le --custom-property viste da .screen, con la variante applicata sopra.

        `variante` e il suffisso di selettore che distingue un profilo
        dall'altro dentro lo stesso mockup, per esempio '[data-r="v"]'.
        """
        base: dict[str, str] = {}
        sopra: dict[str, str] = {}
        for sel, decl in self.regole:
            custom = {k: v for k, v in decl.items() if k.startswith("--")}
            if not custom:
                continue
            for s in sel:
                if s == ".screen" or s.endswith(".screen"):
                    base.update(custom)
                elif variante and s.endswith(".screen" + variante):
                    sopra.update(custom)
        base.update(sopra)
        return base

    # ---- interrogazione ---------------------------------------------------

    def decl(self, selettore: str, variante: str | None = None) -> dict[str, str]:
        """Tutte le dichiarazioni delle regole che citano esattamente il selettore."""
        fuori: dict[str, str] = {}
        for sel, d in self.regole:
            if selettore in sel:
                fuori.update(d)
        # una regola piu specifica per la variante vince
        if variante:
            for sel, d in self.regole:
                if any(s.startswith(".screen" + variante) and s.endswith(selettore)
                       for s in sel):
                    fuori.update(d)
        return fuori

    def valore(self, selettore: str, proprieta: str,
               variante: str | None = None) -> str | None:
        v = self.decl(selettore, variante).get(proprieta)
        return None if v is None else self.risolvi(v, variante)

    def risolvi(self, valore: str, variante: str | None = None) -> str:
        """Sostituisce le var(--x) con il loro valore."""
        vars_ = self.variabili(variante)

        def sost(m: re.Match) -> str:
            nome, ripiego = m.group(1), m.group(2)
            return vars_.get(nome, ripiego or "")

        for _ in range(4):  # le var dei mockup non si annidano piu di cosi
            nuovo = _VAR.sub(sost, valore)
            if nuovo == valore:
                break
            valore = nuovo
        return valore.strip()

    # ---- estrattori tipizzati ---------------------------------------------

    def px(self, selettore: str, proprieta: str,
           variante: str | None = None) -> float | None:
        """Il primo valore in px della proprieta, gia risolto."""
        v = self.valore(selettore, proprieta, variante)
        if v is None:
            return None
        m = _PX.search(v)
        return float(m.group(1)) if m else None

    def corpo(self, selettore: str, variante: str | None = None) -> float | None:
        """Il corpo tipografico dichiarato con la scorciatoia `font:`."""
        d = self.decl(selettore, variante)
        if "font-size" in d:
            m = _PX.search(self.risolvi(d["font-size"], variante))
            if m:
                return float(m.group(1))
        if "font" in d:
            m = _FONT.match(self.risolvi(d["font"], variante))
            if m:
                return float(m.group(2))
        return None

    def var_px(self, nome: str, variante: str | None = None) -> float | None:
        v = self.variabili(variante).get(nome)
        if v is None:
            return None
        m = _PX.search(v)
        return float(m.group(1)) if m else None

    def griglia(self, selettore: str,
                variante: str | None = None) -> tuple[int, int] | None:
        """Colonne e righe da grid-template-columns/rows: repeat(N,1fr)."""
        d = self.decl(selettore, variante)

        def n(prop: str) -> int | None:
            v = d.get(prop)
            if not v:
                return None
            m = re.search(r"repeat\(\s*(\d+)", self.risolvi(v, variante))
            return int(m.group(1)) if m else None

        c, r = n("grid-template-columns"), n("grid-template-rows")
        return None if c is None or r is None else (c, r)


_cache: dict[Path, Foglio] = {}


def foglio(relativo: str) -> Foglio:
    """Il foglio di un mockup, per percorso relativo a docs/mockup/."""
    p = MOCKUP / relativo
    if p not in _cache:
        _cache[p] = Foglio(p)
    return _cache[p]
