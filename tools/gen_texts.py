#!/usr/bin/env python3
"""
Panel texts: from i18n/*.json to main/i18n/texts_gen.{h,c}.

    python tools/gen_texts.py            rewrite the generated files
    python tools/gen_texts.py --check    exit 1 if anything is wrong

Every text the panel shows lives in `i18n/<code>.json`, one file per
language, and nowhere else. English is the reference: its keys are the
list of texts that exist, and every other language must have exactly the
same keys — no more, no fewer.

`--check` is what ctest runs, and it refuses four things, each of which
would otherwise reach a wall-mounted panel silently:

  - **a missing or extra key** in any language. A missing one would fall
    back to English in the middle of an Italian screen; an extra one is
    a text nobody shows, usually a key renamed in English only;
  - **different printf placeholders** from English, or the same ones in a
    different order. The panel passes the arguments in the order English
    expects: "%s %d" translated as "%d %s" is not a style problem, it is
    a string read as a number and a crash;
  - **a character the fonts do not have.** It would be drawn as an empty
    box, and only on the screen that uses that word in that language;
  - **generated files older than the JSON**, which is the error that
    compiles cleanly and shows yesterday's text.

A value is either a string or, for texts that depend on a count, an object
with the forms `one` and `other`. The panel picks the form with the plural
rule of the language (see main/i18n/i18n.c): it is a separate enum so that
code cannot pass a count-dependent key where a plain one is expected.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCES = ROOT / "i18n"
OUT_H = ROOT / "main" / "i18n" / "texts_gen.h"
OUT_C = ROOT / "main" / "i18n" / "texts_gen.c"
SCHEMA = ROOT / "docs" / "config.schema.json"
FONTS = ROOT / "tools" / "genera_font.py"

REFERENCE = "en"
PLURAL_FORMS = ("one", "other")
KEY_RE = re.compile(r"^[a-z][a-z0-9_]*(\.[a-z0-9_]+)*$")
# The conversions lv_snprintf and newlib understand. The whole spec is
# compared, flags and width included: "%3d" and "%d" line up differently.
PRINTF_RE = re.compile(
    r"%(?:%|[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|h|ll|l|z|j|t|L)?"
    r"[diouxXeEfFgGaAcsp])")
# The page's placeholders: {name}, filled by name in JavaScript.
NAMED_RE = re.compile(r"\{([a-z_][a-z0-9_]*)\}")


def load() -> dict[str, dict]:
    langs = {}
    for f in sorted(SOURCES.glob("*.json")):
        with f.open(encoding="utf-8") as fh:
            doc = json.load(fh)
        code = f.stem
        meta = doc.get("_meta", {})
        if meta.get("code") != code:
            sys.exit(f"{f.name}: _meta.code must be \"{code}\"")
        langs[code] = doc
    if REFERENCE not in langs:
        sys.exit(f"i18n/{REFERENCE}.json is missing: it is the reference")
    # English first, then the others by code: the order of lang_t.
    order = [REFERENCE] + sorted(c for c in langs if c != REFERENCE)
    return {c: langs[c] for c in order}


def font_codepoints() -> set[int]:
    """The characters every text font has, read from genera_font.py.

    Read, not copied: a second list here would be the one nobody updates
    the day a language is added to the fonts."""
    spec = importlib.util.spec_from_file_location("genera_font", FONTS)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    cps: set[int] = set()
    for part in mod.TESTO.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-")
            cps.update(range(int(a, 16), int(b, 16) + 1))
        else:
            cps.add(int(part, 16))
    cps.add(ord("\n"))
    return cps


def ranges(cps: set[int]) -> list[tuple[int, int]]:
    """A set of code points as sorted closed ranges."""
    out: list[tuple[int, int]] = []
    for c in sorted(cps):
        if out and c == out[-1][1] + 1:
            out[-1] = (out[-1][0], c)
        else:
            out.append((c, c))
    return out


def enum_name(prefix: str, key: str) -> str:
    return prefix + key.upper().replace(".", "_")


def c_string(s: str) -> str:
    out = []
    for ch in s:
        if ch == "\\": out.append("\\\\")
        elif ch == '"': out.append('\\"')
        elif ch == "\n": out.append("\\n")
        else: out.append(ch)
    return '"' + "".join(out) + '"'


def split(section: dict) -> tuple[list[str], list[str]]:
    plain = sorted(k for k, v in section.items() if isinstance(v, str))
    plural = sorted(k for k, v in section.items() if isinstance(v, dict))
    return plain, plural


def check(langs: dict[str, dict]) -> list[str]:
    errors: list[str] = []
    ref = langs[REFERENCE].get("panel", {})
    glyphs = font_codepoints()

    for key, v in ref.items():
        if not KEY_RE.match(key):
            errors.append(f"en: key \"{key}\" is not lowercase.dotted_snake")
        if isinstance(v, dict) and set(v) != set(PLURAL_FORMS):
            errors.append(f"en: \"{key}\" must have exactly {PLURAL_FORMS}")
        elif not isinstance(v, (str, dict)):
            errors.append(f"en: \"{key}\" is neither text nor plural forms")

    for code, doc in langs.items():
        meta = doc.get("_meta", {})
        for field in ("name", "decimal"):
            if not meta.get(field):
                errors.append(f"{code}: _meta.{field} is missing")
        if len(meta.get("decimal", "")) != 1:
            errors.append(f"{code}: _meta.decimal must be one character")

        panel = doc.get("panel", {})
        for key in sorted(set(ref) - set(panel)):
            errors.append(f"{code}: missing \"{key}\"")
        for key in sorted(set(panel) - set(ref)):
            errors.append(f"{code}: \"{key}\" does not exist in English")

        for key, v in panel.items():
            r = ref.get(key)
            if r is None:
                continue
            if isinstance(r, str) != isinstance(v, str):
                errors.append(f"{code}: \"{key}\" must be "
                              + ("text" if isinstance(r, str) else "plural forms"))
                continue
            pairs = [(r, v)] if isinstance(r, str) else \
                    [(r[f], v.get(f, "")) for f in PLURAL_FORMS]
            if isinstance(v, dict) and set(v) != set(PLURAL_FORMS):
                errors.append(f"{code}: \"{key}\" must have exactly {PLURAL_FORMS}")
            for a, b in pairs:
                if PRINTF_RE.findall(a) != PRINTF_RE.findall(b):
                    errors.append(f"{code}: \"{key}\" placeholders "
                                  f"{PRINTF_RE.findall(b)} differ from English "
                                  f"{PRINTF_RE.findall(a)}")
                bad = sorted({ch for ch in b if ord(ch) not in glyphs})
                if bad:
                    errors.append(f"{code}: \"{key}\" uses characters the fonts "
                                  f"do not have: {' '.join(bad)} "
                                  f"({', '.join(f'U+{ord(c):04X}' for c in bad)})")

    # The keyboard layout is a text too, and the one text whose content the
    # panel parses. Three rows of lowercase ASCII letters, and all
    # twenty-six of them: a layout missing a letter makes some passwords
    # impossible to type, and only in that language.
    for code, doc in langs.items():
        rows = doc.get("panel", {}).get("keyboard.letters")
        if not isinstance(rows, str):
            continue
        parts = rows.split("|")
        if len(parts) != 3 or not all(re.fullmatch(r"[a-z]{1,12}", p) for p in parts):
            errors.append(f"{code}: keyboard.letters must be three rows of a-z "
                          f"separated by '|', at most twelve keys each")
        missing = sorted(set("abcdefghijklmnopqrstuvwxyz") - set(rows))
        if missing:
            errors.append(f"{code}: keyboard.letters lacks {' '.join(missing)}")

    # --- the configuration page's texts ------------------------------------
    # Same shape as the panel's, with two differences: the placeholders are
    # {names} — the page fills them by name, so their order may change in a
    # translation, but not their set — and there is no font check, since the
    # browser draws them with its own fonts.
    ref_web = langs[REFERENCE].get("web", {})
    for key, v in ref_web.items():
        if not KEY_RE.match(key):
            errors.append(f"en: web key \"{key}\" is not lowercase.dotted_snake")
    for code, doc in langs.items():
        web = doc.get("web", {})
        for key in sorted(set(ref_web) - set(web)):
            errors.append(f"{code}: missing web \"{key}\"")
        for key in sorted(set(web) - set(ref_web)):
            errors.append(f"{code}: web \"{key}\" does not exist in English")
        for key, v in web.items():
            r = ref_web.get(key)
            if r is None:
                continue
            if isinstance(r, str) != isinstance(v, str):
                errors.append(f"{code}: web \"{key}\" must be "
                              + ("text" if isinstance(r, str) else "plural forms"))
                continue
            pairs = [(r, v)] if isinstance(r, str) else \
                    [(r[f], v.get(f, "")) for f in PLURAL_FORMS]
            for a, b in pairs:
                if sorted(NAMED_RE.findall(a)) != sorted(NAMED_RE.findall(b)):
                    errors.append(f"{code}: web \"{key}\" placeholders "
                                  f"{sorted(NAMED_RE.findall(b))} differ from English "
                                  f"{sorted(NAMED_RE.findall(a))}")

    # The config schema lists the languages a panel accepts. Written by
    # hand there, because the schema is the authority on configuration —
    # and checked here, because a language in i18n/ that the schema does
    # not accept is one nobody can choose.
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    try:
        enum = schema["properties"]["system"]["properties"]["language"]["enum"]
    except KeyError:
        errors.append("config.schema.json: sistema.lingua has no enum")
    else:
        if sorted(enum) != sorted(langs):
            errors.append(f"config.schema.json: sistema.lingua lists {enum}, "
                          f"i18n/ has {list(langs)}")
    return errors


def render(langs: dict[str, dict]) -> tuple[str, str]:
    plain, plural = split(langs[REFERENCE]["panel"])
    codes = list(langs)
    head = ("/* GENERATED by tools/gen_texts.py from i18n/<code>.json.\n"
            " * Do not edit: change the JSON and rebuild. */\n")

    h = [head, "#ifndef TEXTS_GEN_H", "#define TEXTS_GEN_H", ""]
    h.append("typedef enum {")
    h += [f"    LANG_{c.upper()}," for c in codes]
    h += ["    LANG_COUNT", "} lang_t;", ""]
    h.append("/* The codes in lang_t order, for a table in the same order. */")
    h.append("#define I18N_CODES " + ", ".join(f'"{c}"' for c in codes))
    h.append("")
    h.append("typedef enum {")
    h += [f"    {enum_name('TX_', k)}," for k in plain]
    h += ["    TX_COUNT", "} tx_t;", ""]
    h.append("typedef enum {")
    h += [f"    {enum_name('TXN_', k)}," for k in plural]
    h += ["    TXN_COUNT", "} txn_t;", ""]
    h += ["typedef struct {",
          "    const char *code;",
          "    const char *name;     /* in its own language */",
          "    char decimal;         /* decimal separator */",
          "} lang_info_t;", "",
          "extern const lang_info_t LANG_INFO[LANG_COUNT];",
          "/* One slot more than the keys: C has no empty arrays, and a",
          "   language with no plural keys yet is a real starting point. */",
          "extern const char *const TEXTS[LANG_COUNT][TX_COUNT + 1];",
          "extern const char *const TEXTS_N[LANG_COUNT][TXN_COUNT + 1][2];",
          "",
          "/* The keys by name, as in i18n/<code>.json: a translation edited on",
          "   the configuration page arrives by name, not by number. */",
          "extern const char *const TX_KEYS[TX_COUNT + 1];",
          "extern const char *const TXN_KEYS[TXN_COUNT + 1];",
          "",
          "/* The characters the text fonts can draw, as closed ranges. The",
          "   panel refuses a translation that uses others: it would show as an",
          "   empty box, on one screen, in one language. */",
          "extern const unsigned int I18N_GLYPHS[][2];",
          f"#define I18N_GLYPHS_N {len(ranges(font_codepoints()))}",
          "",
          "/* The configuration page's texts in each language, as the JSON of",
          "   the \"web\" section: the page asks the panel for them, and the",
          "   panel adds what has been edited on top. */",
          "extern const char *const WEB_TEXTS[LANG_COUNT];",
          "", "#endif /* TEXTS_GEN_H */", ""]

    c = [head, '#include "texts_gen.h"', ""]
    c.append("const lang_info_t LANG_INFO[LANG_COUNT] = {")
    for code, doc in langs.items():
        m = doc["_meta"]
        c.append(f"    {{ \"{code}\", {c_string(m['name'])}, "
                 f"'{m['decimal']}' }},")
    c += ["};", ""]

    c.append("const char *const TEXTS[LANG_COUNT][TX_COUNT + 1] = {")
    for code, doc in langs.items():
        panel = doc.get("panel", {})
        c.append(f"    [LANG_{code.upper()}] = {{")
        for k in plain:
            v = panel.get(k)
            c.append(f"        [{enum_name('TX_', k)}] = "
                     f"{c_string(v) if isinstance(v, str) else 'NULL'},")
        c.append("    },")
    c += ["};", ""]

    c.append("const char *const TEXTS_N[LANG_COUNT][TXN_COUNT + 1][2] = {")
    for code, doc in langs.items():
        panel = doc.get("panel", {})
        c.append(f"    [LANG_{code.upper()}] = {{")
        for k in plural:
            v = panel.get(k)
            if isinstance(v, dict):
                c.append(f"        [{enum_name('TXN_', k)}] = "
                         f"{{ {c_string(v['one'])}, {c_string(v['other'])} }},")
            else:
                c.append(f"        [{enum_name('TXN_', k)}] = {{ NULL, NULL }},")
        c.append("    },")
    c += ["};", ""]

    c.append("const char *const TX_KEYS[TX_COUNT + 1] = {")
    c += [f"    [{enum_name('TX_', k)}] = \"{k}\"," for k in plain]
    c += ["};", ""]
    c.append("const char *const TXN_KEYS[TXN_COUNT + 1] = {")
    c += [f"    [{enum_name('TXN_', k)}] = \"{k}\"," for k in plural]
    c += ["};", ""]

    c.append("const unsigned int I18N_GLYPHS[][2] = {")
    c += [f"    {{ 0x{a:X}, 0x{b:X} }}," for a, b in ranges(font_codepoints())]
    c += ["};", ""]

    c.append("const char *const WEB_TEXTS[LANG_COUNT] = {")
    for code, doc in langs.items():
        web = json.dumps(doc.get("web", {}), ensure_ascii=False,
                         separators=(",", ":"), sort_keys=True)
        c.append(f"    [LANG_{code.upper()}] =")
        # Split into chunks the compiler joins: one literal of forty
        # kilobytes is legal, and unreadable in a diff.
        for i in range(0, len(web), 100):
            c.append(f"        {c_string(web[i:i + 100])}")
        c[-1] += ","
    c.append("};")
    c.append("")
    return "\n".join(h), "\n".join(c)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true",
                    help="verify instead of writing; exit 1 on any problem")
    arg = ap.parse_args()

    langs = load()
    errors = check(langs)
    h, c = render(langs)

    if arg.check:
        for path, text in ((OUT_H, h), (OUT_C, c)):
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                errors.append(f"{path.relative_to(ROOT)} is out of date: "
                              "run tools/gen_texts.py")
        for e in errors:
            print(e)
        n = len(langs[REFERENCE].get("panel", {}))
        print(f"{len(langs)} languages, {n} texts: "
              + ("OK" if not errors else f"{len(errors)} problems"))
        return 1 if errors else 0

    if errors:
        # Written anyway: a missing translation falls back to English at
        # run time, and refusing to generate would block the build over a
        # word. ctest is where it becomes an error.
        for e in errors:
            print("warning:", e, file=sys.stderr)
    OUT_H.parent.mkdir(parents=True, exist_ok=True)
    for path, text in ((OUT_H, h), (OUT_C, c)):
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            path.write_text(text, encoding="utf-8", newline="\n")
    n = len(langs[REFERENCE].get("panel", {}))
    print(f"{len(langs)} languages, {n} texts")
    return 0


if __name__ == "__main__":
    sys.exit(main())
