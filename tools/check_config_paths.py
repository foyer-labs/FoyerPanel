#!/usr/bin/env python3
"""
Every configuration path the code reads exists in the schema.

    python tools/check_config_paths.py

The firmware reads config.json through cfg_testo("robot/entity", ...),
cfg_testo_in("lights/zones", n, "name", ...) and their siblings. A path
the schema does not have is not an error anywhere else: the accessor
returns its fallback, the panel shows a default, and the setting the user
changed on the configuration page simply never arrives. When every key was
renamed from Italian to English, this is the check that said which reads
had been missed; after that, it is the one that catches a typo.

What is scanned: string literals passed to the cfg_* accessors in main/,
firmware/main/ and sim/, and the path-shaped literals given to snprintf to
build them ("schedules/%d", "%s/windows/%d"). A number or %d stands for an
array element, %s for any key. test/ is left out on purpose: the tests
build old and broken documents to prove the migrations and the validator.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCHEMA = ROOT / "docs" / "config.schema.json"
SOURCES = [ROOT / "main", ROOT / "firmware" / "main", ROOT / "sim"]
# In config.c the migrations speak the old names on purpose: the lines of
# the migra_* functions are skipped, and only those. Skipping the whole file
# once hid cfg_nome_host() reading a key that no longer existed.
MIGRATION = re.compile(r"^static (void|bool) migra")

PLAIN = re.compile(r'\bcfg_(testo|intero|vero|quanti|decimale|imposta_testo|imposta_vero|ha)'
                   r'\(\s*"([^"]+)"')
IN = re.compile(r'\bcfg_(testo|intero|vero|ha|decimale)_in\(\s*"([^"]+)"\s*,[^,]+,\s*"([^"]+)"')
BUILT = re.compile(r'snprintf\([^;]*?"([a-z_%]+(?:/[a-z_%0-9*]+)+)"')


def schema_paths() -> set[str]:
    s = json.loads(SCHEMA.read_text(encoding="utf-8"))
    defs = s.get("$defs", {})
    out: set[str] = set()

    def walk(n, path):
        if not isinstance(n, dict):
            return
        if "$ref" in n:
            walk(defs[n["$ref"].split("/")[-1]], path)
        for alt in ("anyOf", "oneOf", "allOf"):
            for x in n.get(alt, []):
                walk(x, path)
        for k, v in n.get("properties", {}).items():
            p = f"{path}/{k}" if path else k
            out.add(p)
            walk(v, p)
        if "items" in n:
            out.add(path + "/*")
            walk(n["items"], path + "/*")

    walk(s, "")
    return out


def normalise(p: str) -> str:
    return "/".join("*" if (x.isdigit() or x == "%d") else x for x in p.split("/"))


def known(p: str, paths: set[str]) -> bool:
    p = normalise(p)
    if p in paths:
        return True
    if "%s" in p:
        rx = re.compile("^" + re.escape(p).replace("%s", "[^/]+") + "$")
        return any(rx.match(q) for q in paths)
    # A path built in two steps ("%s/windows/%d" and then the element) is
    # checked by its tail: some schema path must end the same way.
    return False


def main() -> int:
    paths = schema_paths()
    bad: list[str] = []
    seen = 0
    for base in SOURCES:
        for f in sorted(base.rglob("*.c")):
            rel = f.relative_to(ROOT).as_posix()
            text = f.read_text(encoding="utf-8")
            lines = text.split("\n")
            # Lines that are skipped: the migrations, and comments.
            skip = set()
            in_migration = False
            for n, line in enumerate(lines, 1):
                if MIGRATION.match(line):
                    in_migration = True
                if in_migration or line.lstrip().startswith(("*", "/*", "//")):
                    skip.add(n)
                if in_migration and line.startswith("}"):
                    in_migration = False

            # The accessors are matched on the whole text, not line by line:
            # a call that wraps before the field name —
            #     cfg_testo_in("climate/air_conditioners", n,
            #                  "default_duration", "1:30")
            # — was invisible to a line-by-line scan, and three such reads
            # kept their Italian names through the rename unseen.
            for m in PLAIN.finditer(text):
                n = text.count("\n", 0, m.start()) + 1
                if n in skip:
                    continue
                seen += 1
                if not known(m.group(2), paths):
                    bad.append(f"{rel}:{n}: cfg_{m.group(1)}(\"{m.group(2)}\")")
            for m in IN.finditer(text):
                n = text.count("\n", 0, m.start()) + 1
                if n in skip:
                    continue
                seen += 1
                p = f"{m.group(2)}/*/{m.group(3)}"
                if not known(p, paths):
                    bad.append(f"{rel}:{n}: cfg_{m.group(1)}_in(\"{m.group(2)}\", n, \"{m.group(3)}\")")

            for n, line in enumerate(lines, 1):
                if n in skip:
                    continue
                for m in BUILT.finditer(line):
                    p = m.group(1)
                    pieces = p.split("/")
                    # "%s/%s" joins a directory and a file name: no key in it.
                    if all(x.startswith("%") for x in pieces):
                        continue
                    seen += 1
                    roots = {q.split("/")[0] for q in paths}
                    if pieces[0] in roots:
                        ok = known(p, paths)
                    else:
                        # Relative: "%s/windows/%d", or "extras/%s" used as
                        # the field of an element. Some schema path must
                        # end the same way.
                        tail = normalise(p[3:] if p.startswith("%s/") else p)
                        rx = re.compile("(^|/)" + re.escape(tail).replace("%s", "[^/]+")
                                        .replace(r"\*", r"\*") + "$")
                        ok = any(rx.search(q) for q in paths)
                    if not ok:
                        bad.append(f"{rel}:{n}: built path \"{p}\"")
    for b in bad:
        print(b)
    print(f"{seen} configuration reads checked against the schema: "
          + ("all found" if not bad else f"{len(bad)} not in the schema"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
