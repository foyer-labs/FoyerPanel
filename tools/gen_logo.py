#!/usr/bin/env python3
"""Foyer's logo, drawn once.

A single geometry — the mark of roof-shaped waves over a lit door, and the
name in letters built with the same stroke — and from it everything needed:

  docs/logo/foyer-light.svg      the logo for light backgrounds (README, GitHub)
  docs/logo/foyer-dark.svg       the same for dark backgrounds
  docs/logo/foyer-mark.svg       the mark alone, transparent, for dark backgrounds
  docs/logo/foyer-icon.svg       the mark on a square of the panel's glass:
                                 avatar, repository preview
  main/ui/widgets/logo_shape.h   the same geometry for LVGL, which the panel
                                 draws with lines, arcs and rectangles
  web/index.html                 the mark in the header and the tab's icon,
                                 between the logo-mark and logo-favicon
                                 comments

The name uses no font: the letters are strokes like the waves, so the panel
draws them without one more font and the two drawings cannot drift apart —
there is one place where they change, and it is this file.

Coordinates are in mark units, a square of 64. The name sits to its right
with the same reference height; the whole logo is WIDTH units wide.

    python3 tools/gen_logo.py
"""
from __future__ import annotations

import math
from pathlib import Path
from urllib.parse import quote

ROOT = Path(__file__).resolve().parent.parent
LOGO = ROOT / "docs" / "logo"
SHAPE_H = ROOT / "main" / "ui" / "widgets" / "logo_shape.h"
PAGE = ROOT / "web" / "index.html"

AMBER = "#F0A835"
INK_DARK = "#0D1014"     # the panel's glass
INK_LIGHT = "#E8ECF2"    # the panel's text

HEIGHT = 64
WIDTH = 180

# --- the geometry ------------------------------------------------------------
#
# Four primitives, the ones LVGL can draw:
#   line   (points, width, rounded, role, opacity)
#   arc    (cx, cy, r, start, end, width, role)   degrees, 0 = 3 o'clock, clockwise
#   circle (cx, cy, r, role)                      filled
#   rect   (x, y, w, h, role)                     filled
# The role is "i" (ink: the text colour of the background) or "a" (amber).

S = 3.6          # the mark's stroke
L = 3.4          # the letters' stroke: a touch lighter, they are denser
HALF = L / 2

MARK = [
    ("line", [(21, 37), (32, 28), (43, 37)], S, True, "i", 1.00),
    ("line", [(14, 31.5), (32, 17), (50, 31.5)], S, True, "i", 0.60),
    ("line", [(7, 26), (32, 6), (57, 26)], S, True, "i", 0.32),
    ("rect", 26, 47, 12, 10, "a"),          # the door: its body
    ("circle", 32, 47, 6, "a"),             #           and the arch on top
    ("line", [(16, 57), (48, 57)], S, True, "i", 1.00),   # the threshold
]

# The letters: cap height from 21 to 43, centred on the mark.
TOP, BOTTOM, MIDDLE = 21.0, 43.0, 32.0


def letter_f(x):
    return [
        ("rect", x, TOP, L, BOTTOM - TOP, "i"),
        ("rect", x, TOP, 11, L, "i"),
        ("rect", x, MIDDLE - HALF, 9, L, "i"),
    ]


def letter_o(x):
    r = 11.5 - HALF                          # a hair beyond the capitals
    return [("arc", x + 11.5, MIDDLE, r, 0, 360, L, "i")]


def letter_y(x):
    joint = (x + 8, 32.6)
    return [
        ("line", [(x + 0.8, 22.0), joint], L, False, "i", 1.0),
        ("line", [(x + 15.2, 22.0), joint], L, False, "i", 1.0),
        ("rect", x + 8 - HALF, 32.0, L, BOTTOM - 32.0, "i"),
    ]


def letter_e(x):
    return [
        ("rect", x, TOP, L, BOTTOM - TOP, "i"),
        ("rect", x, TOP, 11, L, "i"),
        ("rect", x, MIDDLE - HALF, 9.5, L, "i"),
        ("rect", x, BOTTOM - L, 11, L, "i"),
    ]


def letter_r(x):
    cy = (TOP + HALF + MIDDLE) / 2           # centre of the bowl
    r = MIDDLE - cy
    return [
        ("rect", x, TOP, L, BOTTOM - TOP, "i"),
        ("rect", x, TOP, 6.5, L, "i"),
        ("rect", x, MIDDLE - HALF, 6.5, L, "i"),
        ("arc", x + 6.5, cy, r, 270, 90, L, "i"),
        ("line", [(x + 6.0, MIDDLE), (x + 12.2, BOTTOM - 0.8)], L, False, "i", 1.0),
    ]


NAME = (letter_f(77) + letter_o(94) + letter_y(123.5)
        + letter_e(146.5) + letter_r(164.5))


# --- SVG --------------------------------------------------------------------

def _n(v: float) -> str:
    return f"{v:.2f}".rstrip("0").rstrip(".")


def svg_primitive(p, ink: str) -> str:
    colour = lambda role: AMBER if role == "a" else ink
    kind = p[0]
    if kind == "line":
        _, points, w, rounded, role, opa = p
        pts = " ".join(f"{_n(x)},{_n(y)}" for x, y in points)
        cap = "round" if rounded else "butt"
        join = "round" if rounded else "miter"
        o = f' opacity="{_n(opa)}"' if opa < 1 else ""
        return (f'<polyline points="{pts}" fill="none" stroke="{colour(role)}" '
                f'stroke-width="{_n(w)}" stroke-linecap="{cap}" '
                f'stroke-linejoin="{join}"{o}/>')
    if kind == "arc":
        _, cx, cy, r, a0, a1, w, role = p
        if a1 - a0 >= 360:
            return (f'<circle cx="{_n(cx)}" cy="{_n(cy)}" r="{_n(r)}" fill="none" '
                    f'stroke="{colour(role)}" stroke-width="{_n(w)}"/>')

        def point(a):
            return cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))
        x0, y0 = point(a0)
        x1, y1 = point(a1)
        large = 1 if (a1 - a0) % 360 > 180 else 0
        return (f'<path d="M{_n(x0)} {_n(y0)} A{_n(r)} {_n(r)} 0 {large} 1 '
                f'{_n(x1)} {_n(y1)}" fill="none" stroke="{colour(role)}" '
                f'stroke-width="{_n(w)}"/>')
    if kind == "circle":
        _, cx, cy, r, role = p
        return f'<circle cx="{_n(cx)}" cy="{_n(cy)}" r="{_n(r)}" fill="{colour(role)}"/>'
    if kind == "rect":
        _, x, y, w, h, role = p
        return (f'<rect x="{_n(x)}" y="{_n(y)}" width="{_n(w)}" height="{_n(h)}" '
                f'fill="{colour(role)}"/>')
    raise ValueError(kind)


def svg(primitives, width, ink, background=None, title="Foyer") -> str:
    body = "\n  ".join(svg_primitive(p, ink) for p in primitives)
    ground = ""
    if background:
        ground = (f'<rect width="{width}" height="{HEIGHT}" rx="14" '
                  f'fill="{background}"/>\n  ')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} '
            f'{HEIGHT}" width="{width * 4}" height="{HEIGHT * 4}" '
            f'role="img" aria-label="{title}">\n  <title>{title}</title>\n  '
            f'{ground}{body}\n</svg>\n')


# --- C for LVGL ----------------------------------------------------------------

def c_primitive(p) -> str:
    kind = p[0]
    f = lambda v: f"{v:.2f}f"
    if kind == "line":
        _, points, w, rounded, role, opa = p
        assert len(points) <= 3
        pts = ", ".join(f"{{{f(x)}, {f(y)}}}" for x, y in points)
        return (f"    {{ LOGO_LINE, '{role}', {f(opa)}, {f(w)}, "
                f"{'true' if rounded else 'false'}, {len(points)}, {{ {pts} }} }},")
    if kind == "arc":
        _, cx, cy, r, a0, a1, w, role = p
        return (f"    {{ LOGO_ARC, '{role}', 1.0f, {f(w)}, false, 0, "
                f"{{ {{{f(cx)}, {f(cy)}}}, {{{f(r)}, 0}}, {{{f(a0)}, {f(a1)}}} }} }},")
    if kind == "circle":
        _, cx, cy, r, role = p
        return (f"    {{ LOGO_CIRCLE, '{role}', 1.0f, 0, false, 0, "
                f"{{ {{{f(cx)}, {f(cy)}}}, {{{f(r)}, 0}} }} }},")
    if kind == "rect":
        _, x, y, w, h, role = p
        return (f"    {{ LOGO_RECT, '{role}', 1.0f, 0, false, 0, "
                f"{{ {{{f(x)}, {f(y)}}}, {{{f(w)}, {f(h)}}} }} }},")
    raise ValueError(kind)


HEADER_H = '''/* ------------------------------------------------------------------------
 * The geometry of Foyer's logo — GENERATED by tools/gen_logo.py.
 * Do not edit: the drawing changes there, and the SVGs of docs/logo/ come
 * from the same place.
 *
 * Mark units: a square of LOGO_HEIGHT. The logo with the name is
 * LOGO_WIDTH wide. See logo.c for how it is drawn.
 * --------------------------------------------------------------------- */
#ifndef LOGO_SHAPE_H
#define LOGO_SHAPE_H

#include <stdbool.h>

#define LOGO_HEIGHT {height}
#define LOGO_WIDTH  {width}

typedef enum {{ LOGO_LINE, LOGO_ARC, LOGO_CIRCLE, LOGO_RECT }} logo_kind_t;

/* p[0..2]: the points of a line; for the others
     arc     p[0] centre, p[1].x radius, p[2] start and end in degrees
     circle  p[0] centre, p[1].x radius
     rect    p[0] top-left corner, p[1] width and height */
typedef struct {{
    logo_kind_t kind;
    char        role;       /* 'i' ink, 'a' amber */
    float       opa;
    float       width;
    bool        rounded;
    int         points;
    struct {{ float x, y; }} p[3];
}} logo_prim_t;

'''


def c_table(name: str, primitives) -> str:
    rows = "\n".join(c_primitive(p) for p in primitives)
    return (f"static const logo_prim_t {name}[] = {{\n{rows}\n}};\n"
            f"#define {name.upper()}_N (sizeof {name} / sizeof {name}[0])\n\n")


def replace_between(text: str, name: str, inside: str) -> str:
    """Replaces what is between <!-- name:start --> and <!-- name:end -->."""
    start, end = f"<!-- {name}:start -->", f"<!-- {name}:end -->"
    a, b = text.index(start) + len(start), text.index(end)
    return text[:a] + "\n" + inside + text[b:]


def page() -> None:
    """The mark in the configuration page's header and its icon in the
    browser tab. In the page itself and not in a file of its own: the panel
    serves only that, from its own flash."""
    mark = "".join(svg_primitive(p, "currentColor") for p in MARK)
    in_header = (f'  <svg class="marchio" viewBox="0 0 {HEIGHT} {HEIGHT}" '
                 f'aria-hidden="true">{mark}</svg>\n  ')
    icon = svg(MARK, HEIGHT, INK_LIGHT, background=INK_DARK)
    favicon = (f'<link rel="icon" type="image/svg+xml" '
               f'href="data:image/svg+xml,{quote(icon.strip())}">\n')
    t = PAGE.read_text(encoding="utf-8")
    t = replace_between(t, "logo-mark", in_header)
    t = replace_between(t, "logo-favicon", favicon)
    PAGE.write_text(t, encoding="utf-8", newline="\n")


def main() -> int:
    LOGO.mkdir(parents=True, exist_ok=True)
    whole = MARK + NAME

    (LOGO / "foyer-light.svg").write_text(
        svg(whole, WIDTH, INK_DARK), encoding="utf-8", newline="\n")
    (LOGO / "foyer-dark.svg").write_text(
        svg(whole, WIDTH, INK_LIGHT), encoding="utf-8", newline="\n")
    (LOGO / "foyer-mark.svg").write_text(
        svg(MARK, HEIGHT, INK_LIGHT), encoding="utf-8", newline="\n")
    (LOGO / "foyer-icon.svg").write_text(
        svg(MARK, HEIGHT, INK_LIGHT, background=INK_DARK),
        encoding="utf-8", newline="\n")

    h = HEADER_H.format(height=HEIGHT, width=WIDTH)
    h += c_table("logo_mark", MARK)
    h += c_table("logo_name", NAME)
    h += "#endif /* LOGO_SHAPE_H */\n"
    SHAPE_H.write_text(h, encoding="utf-8", newline="\n")

    page()
    print("written docs/logo/*.svg, main/ui/widgets/logo_shape.h and the logo "
          "in web/index.html")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
