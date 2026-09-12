#!/usr/bin/env python3
"""Does the configuration page find every text it asks for?

The page writes no text of its own: it asks the panel for the "web"
section of i18n/<code>.json and looks texts up by key. A key it asks for
and English does not have shows as the key itself — "field.robot.name.label"
on a label — and nothing fails anywhere: the page is drawn, only wrong.

What is checked, against i18n/en.json (gen_texts.py already makes every
other language have the same keys):

  - every key written literally in web/index.html: t("..."), tn("..."),
    tIf("...") and data-t="...";
  - the keys the page builds: title and introduction of every section,
    its group, the secrets it lists, the contrast pairs;
  - every field of docs/config.schema.json has field.<path>.label —
    the user asked to be able to translate every single label, and a field
    without one would show its raw key in every language;
  - the choices of a list: when one value has field.<path>.option.<value>,
    all of them do, and no option names a value the schema does not have;
  - no field.<path>.* text names a field the schema no longer has: a key
    renamed in the schema leaves its label behind, translated in every
    language and shown nowhere.

    python3 tools/check_page_texts.py

Exit 0 when everything is found, 1 otherwise.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAGE = ROOT / "web" / "index.html"
SCHEMA = ROOT / "docs" / "config.schema.json"
EN = ROOT / "i18n" / "en.json"

# A key ending in a dot is the start of one built at run time — "ha.state."
# plus the state — and not a key.
LITERAL = re.compile(r"""\b(?:t|tn|tIf)\(\s*["']([a-z][a-z0-9_.]*[a-z0-9_])["']""")
DATA_T = re.compile(r'data-t="([a-z][a-z0-9_.]*)"')


def schema_fields() -> dict[str, list | None]:
    """Every field path, without row indices, and its choices if any."""
    s = json.loads(SCHEMA.read_text(encoding="utf-8"))
    defs = s.get("$defs", s.get("definitions", {}))

    def resolve(x):
        if isinstance(x, dict) and "$ref" in x:
            d = dict(defs[x["$ref"].split("/")[-1]])
            d.update({k: v for k, v in x.items() if k != "$ref"})
            return resolve(d)
        return x

    def flat(x):
        x = resolve(x)
        if isinstance(x, dict) and "anyOf" in x:
            branches = [b for b in x["anyOf"] if resolve(b).get("type") != "null"]
            if len(branches) == 1:
                y = dict(resolve(branches[0]))
                y.update({k: v for k, v in x.items() if k != "anyOf"})
                return y
        return x

    out: dict[str, list | None] = {}

    def walk(x, path):
        x = flat(x)
        if not isinstance(x, dict):
            return
        if path:
            items = resolve(x.get("items", {})) or {}
            out[path] = x.get("enum") or (items.get("enum") if isinstance(items, dict) else None)
        if x.get("type") == "object":
            for k, v in x.get("properties", {}).items():
                walk(v, f"{path}.{k}" if path else k)
        if x.get("type") == "array":
            it = flat(x.get("items", {}))
            if isinstance(it, dict) and it.get("type") == "object":
                for k, v in it.get("properties", {}).items():
                    walk(v, f"{path}.{k}")

    walk(s, "")
    return out


def option_key(value) -> str:
    """As voce() in the page: lowercase, anything else an underscore."""
    return re.sub(r"[^a-z0-9_]", "_", str(value).lower())


def main() -> int:
    page = PAGE.read_text(encoding="utf-8")
    web = json.loads(EN.read_text(encoding="utf-8")).get("web", {})
    fields = schema_fields()
    errors: list[str] = []

    wanted = set(LITERAL.findall(page)) | set(DATA_T.findall(page))

    sections = re.search(r"^const SEZIONI = \[\n(.*?)^\];", page, re.S | re.M)
    if not sections:
        errors.append("index.html: SEZIONI not found")
    else:
        body = sections.group(1)
        for sid in re.findall(r'\bid:"([a-z_]+)"', body):
            wanted |= {f"section.{sid}.title", f"section.{sid}.lead"}
        wanted |= {f"group.{g}" for g in re.findall(r'\bgruppo:"([a-z_]+)"', body)}
        for block in re.findall(r"segreti:\[([^\]]*)\]", body, re.S):
            for name in re.findall(r'"([a-z0-9_]+)"', block):
                wanted |= {f"secret.{name}.title", f"secret.{name}.hint"}

    pairs = re.search(r"^const COPPIE = \[\n(.*?)^\];", page, re.S | re.M)
    if pairs:
        for front, back in re.findall(r'\["([a-z_]+)",\s*"([a-z_]+)"', pairs.group(1)):
            wanted.add(f"contrast.pair.{front}_on_{back}")

    for key in sorted(wanted - set(web)):
        errors.append(f"index.html asks for \"{key}\", i18n/en.json web has no such text")

    for path in sorted(fields):
        if f"field.{path}.label" not in web:
            errors.append(f"schema field {path} has no field.{path}.label")

    for path, choices in sorted(fields.items()):
        if not choices:
            continue
        have = {k.rsplit(".", 1)[1] for k in web if k.startswith(f"field.{path}.option.")}
        if not have:
            continue
        need = {option_key(c) for c in choices}
        for c in sorted(need - have):
            errors.append(f"field.{path}: choice \"{c}\" has no option text, the others do")
        for c in sorted(have - need):
            errors.append(f"field.{path}.option.{c}: the schema has no such choice")

    known = set(fields)
    for key in sorted(web):
        if not key.startswith("field."):
            continue
        m = re.fullmatch(r"field\.(.+)\.(label|hint|count|option\.[a-z0-9_]+)", key)
        if not m:
            errors.append(f"{key}: a field text is field.<path>.label, .hint, .count or .option.<value>")
        elif m.group(1) not in known:
            errors.append(f"{key}: the schema has no field {m.group(1)}")

    for e in errors:
        print(e)
    print(f"{len(wanted)} keys asked for, {len(fields)} fields: "
          + ("OK" if not errors else f"{len(errors)} problems"))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
