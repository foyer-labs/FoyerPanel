#!/usr/bin/env python3
"""I colori predefiniti sono gli stessi in tutti e tre i posti?

Un colore predefinito è scritto tre volte, e ognuna delle tre serve a
qualcosa di diverso:

    main/ui/tema.c            la tabella `PREDEFINITI`: è quello che il
                              pannello usa davvero quando la configurazione
                              non dice niente
    docs/config.schema.json   il `default` di ogni colore: è quello che la
                              pagina di configurazione **mostra** nel campo
                              prima che tu tocchi qualcosa
    docs/02-design-tokens.md  la tabella della palette: è quello che legge
                              chi progetta, e che i mockup rendono 1:1

Se il secondo si allontanasse dal primo, la pagina mostrerebbe un colore e
il pannello ne userebbe un altro — e siccome il valore mostrato non viene
scritto nel documento finché non lo si tocca, nessuno se ne accorgerebbe
guardando il JSON. È il difetto più silenzioso di tutti: due schermate
diverse, nessun errore da nessuna parte.

Se il terzo si allontanasse, i mockup smetterebbero di essere il
riferimento contro cui si confrontano le catture, e «indistinguibile dai
mockup» diventerebbe una frase.

    python tools/verifica_palette.py

Uscita 0 se i tre elenchi coincidono, 1 altrimenti.
"""
import json
import re
import sys
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
TEMA = RADICE / "main" / "ui" / "tema.c"
SCHEMA = RADICE / "docs" / "config.schema.json"
DOCUMENTO = RADICE / "docs" / "02-design-tokens.md"

# Il nome in configurazione di ognuna delle voci scelte, e il nome con
# cui la stessa voce compare nella tabella del documento. Sono due elenchi
# perché sono due lingue: `background` è come si chiama per chi configura, `bg`
# è come si chiama per chi disegna.
SCELTI = {
    "background":   ("TEMA_BG",   "bg"),
    "cards":        ("TEMA_CARD", "card"),
    "borders":      ("TEMA_LINE", "line"),
    "text":         ("TEMA_TXT",  "txt"),
    "text_dim":     ("TEMA_DIM",  "dim"),
    "accent":       ("TEMA_ACC",  "acc"),
    "positive":     ("TEMA_OK",   "ok"),
    "negative":     ("TEMA_WARN", "warn"),
    "lights_on":    ("TEMA_LUCE", "luce"),
}


def dal_firmware() -> dict:
    """La tabella PREDEFINITI di tema.c, voce -> 0xRRGGBB."""
    t = TEMA.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"PREDEFINITI\[TEMA_QUANTI\]\s*=\s*\{(.*?)\n\};", t, re.S)
    if not m:
        raise SystemExit("la tabella PREDEFINITI non si trova in tema.c")
    fuori = {}
    for voce, valore in re.findall(r"\[(TEMA_[A-Z0-9_]+)\]\s*=\s*0x([0-9A-Fa-f]{6})",
                                   m.group(1)):
        fuori[voce] = valore.upper()
    return fuori


def dallo_schema() -> dict:
    """I `default` del blocco `appearance`, nome -> RRGGBB."""
    d = json.loads(SCHEMA.read_text(encoding="utf-8"))
    a = d.get("properties", {}).get("appearance", {}).get("properties")
    if not a:
        raise SystemExit("il blocco `appearance` non c'e nello schema")
    fuori = {}
    for nome, campo in a.items():
        v = campo.get("default")
        if not isinstance(v, str) or not re.fullmatch(r"#[0-9A-Fa-f]{6}", v):
            raise SystemExit(f"appearance.{nome} non ha un default utilizzabile")
        fuori[nome] = v[1:].upper()
    return fuori


def dal_documento() -> dict:
    """La tabella della palette in 02-design-tokens.md, nome -> RRGGBB."""
    t = DOCUMENTO.read_text(encoding="utf-8", errors="replace")
    fuori = {}
    for nome, valore in re.findall(r"^\|\s*`([a-z0-9_]+)`\s*\|\s*`#([0-9A-Fa-f]{6})`",
                                   t, re.M):
        fuori[nome] = valore.upper()
    if not fuori:
        raise SystemExit("la tabella della palette non si legge nel documento")
    return fuori


def main() -> int:
    firmware = dal_firmware()
    schema = dallo_schema()
    documento = dal_documento()

    guasti = []

    if sorted(schema) != sorted(SCELTI):
        guasti.append("lo schema non offre le voci attese: "
                      + ", ".join(sorted(set(schema) ^ set(SCELTI))))

    for nome, (voce, nel_documento) in sorted(SCELTI.items()):
        atteso = firmware.get(voce)
        if atteso is None:
            guasti.append(f"{voce} non e in PREDEFINITI")
            continue

        s = schema.get(nome)
        if s != atteso:
            guasti.append(f"appearance.{nome}: lo schema dice #{s}, "
                          f"il pannello usa #{atteso}")

        d = documento.get(nel_documento)
        if d is None:
            guasti.append(f"`{nel_documento}` non e nella tabella del documento")
        elif d != atteso:
            guasti.append(f"{nel_documento}: il documento dice #{d}, "
                          f"il pannello usa #{atteso}")

    # Le voci del documento che non si scelgono devono comunque coincidere
    # col firmware: sono la palette, e i mockup le rendono.
    ALTRI = {"cool": "TEMA_COOL", "caldo": "TEMA_CALDO",
             "off": "TEMA_OFF", "ink": "TEMA_INK",
             "card2": "TEMA_CARD2",
             "meteo_sole": "TEMA_METEO_SOLE",
             "meteo_nuvola": "TEMA_METEO_NUVOLA",
             "meteo_pioggia": "TEMA_METEO_PIOGGIA",
             "meteo_neve": "TEMA_METEO_NEVE"}
    for nel_documento, voce in sorted(ALTRI.items()):
        d, atteso = documento.get(nel_documento), firmware.get(voce)
        if d and atteso and d != atteso:
            guasti.append(f"{nel_documento}: il documento dice #{d}, "
                          f"il pannello usa #{atteso}")

    if guasti:
        for g in guasti:
            print("  " + g)
        print(f"{len(guasti)} disaccordi fra i tre posti")
        return 1

    print(f"i {len(SCELTI)} colori scegliibili coincidono nei tre posti, "
          f"e cosi i {len(ALTRI)} ricavati che il documento elenca")
    return 0


if __name__ == "__main__":
    sys.exit(main())
