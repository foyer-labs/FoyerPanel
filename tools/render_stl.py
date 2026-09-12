#!/usr/bin/env python3
"""A picture of an STL, for the README: the wall mount seen from the front
and from the wall, side by side, on a transparent background.

    python tools/render_stl.py                        the wall mount
    python tools/render_stl.py FILE.stl OUT.png       any binary STL

Needs numpy and Pillow, nothing 3D: a small z-buffer rasteriser with flat
shading and dark edges where the surface folds, drawn at three times the
size and scaled down so the edges come out smooth.

The part is drawn as it hangs: its longest side upright, the face that
lies on the print bed against the wall.
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
STL = ROOT / "3D Printable wall mount" / "Display 10.1 wall support.stl"
OUT = ROOT / "docs" / "screenshots" / "wall-mount.png"

SS = 3                                  # supersampling
VIEW_W, VIEW_H = 760, 940               # one view, final pixels
COLOUR = np.array([240, 168, 53], float)    # Foyer's amber, as filament
EDGE = np.array([92, 58, 12], float)
LIGHT = np.array([-0.45, 0.6, 0.66])    # from upper left, in front


def load(path: Path) -> np.ndarray:
    """The triangles, n x 3 x 3, of a binary STL."""
    dt = np.dtype([("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")])
    data = path.read_bytes()
    if data[:5] == b"solid" and b"facet" in data[:300]:
        raise SystemExit(f"{path}: ASCII STL, only binary is read")
    tri = np.frombuffer(data, dtype=dt, offset=84)["v"].astype(float)
    return tri - (tri.reshape(-1, 3).max(0) + tri.reshape(-1, 3).min(0)) / 2


def hang(tri: np.ndarray) -> np.ndarray:
    """From print-bed coordinates to the wall: the longest side up, the
    thinnest axis pointing at whoever looks at it."""
    size = tri.reshape(-1, 3).max(0) - tri.reshape(-1, 3).min(0)
    up, side = np.argsort(size)[::-1][:2]
    depth = 3 - up - side
    m = np.zeros((3, 3))
    m[0, side] = -1                     # right, keeping the handedness
    m[1, up] = 1                        # up
    m[2, depth] = 1                     # towards the viewer
    if np.linalg.det(m) < 0:
        m[0] *= -1
    return tri @ m.T


def rotation(yaw_deg: float, pitch_deg: float) -> np.ndarray:
    y, p = np.radians(yaw_deg), np.radians(pitch_deg)
    ry = np.array([[np.cos(y), 0, np.sin(y)], [0, 1, 0], [-np.sin(y), 0, np.cos(y)]])
    rx = np.array([[1, 0, 0], [0, np.cos(p), -np.sin(p)], [0, np.sin(p), np.cos(p)]])
    return rx @ ry


def render(tri: np.ndarray, rot: np.ndarray, scale: float) -> Image.Image:
    w, h = VIEW_W * SS, VIEW_H * SS
    t = tri @ rot.T
    # screen: x right, y down; z towards the viewer, bigger is nearer
    sx = t[..., 0] * scale * SS + w / 2
    sy = -t[..., 1] * scale * SS + h / 2
    sz = t[..., 2]

    n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    light = LIGHT / np.linalg.norm(LIGHT)
    diffuse = np.abs(n @ light)
    shade = 0.32 + 0.68 * diffuse

    zbuf = np.full((h, w), -np.inf)
    face = np.full((h, w), -1, int)
    for i in range(len(t)):
        x0, x1 = int(max(sx[i].min(), 0)), int(min(sx[i].max() + 1, w))
        y0, y1 = int(max(sy[i].min(), 0)), int(min(sy[i].max() + 1, h))
        if x0 >= x1 or y0 >= y1:
            continue
        px, py = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        (ax, bx, cx), (ay, by, cy) = sx[i], sy[i]
        area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
        if abs(area) < 1e-9:
            continue
        w0 = ((bx - px) * (cy - py) - (by - py) * (cx - px)) / area
        w1 = ((cx - px) * (ay - py) - (cy - py) * (ax - px)) / area
        w2 = 1 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not inside.any():
            continue
        z = w0 * sz[i, 0] + w1 * sz[i, 1] + w2 * sz[i, 2]
        zb = zbuf[y0:y1, x0:x1]
        win = inside & (z > zb)
        zb[win] = z[win]
        face[y0:y1, x0:x1][win] = i

    covered = face >= 0
    rgb = np.zeros((h, w, 3))
    rgb[covered] = COLOUR * shade[face[covered], None]

    # Edges: where the neighbouring pixel belongs to a face turned another
    # way, or to nothing. Flat faces split in triangles stay unbroken.
    nb = np.zeros((h, w, 3))
    nb[covered] = n[face[covered]]
    edge = np.zeros((h, w), bool)
    for dy, dx in ((0, 1), (1, 0), (1, 1), (1, -1)):
        a = nb[max(dy, 0):h, max(dx, 0):w + min(dx, 0)]
        b = nb[:h - dy, max(-dx, 0):w - max(dx, 0)]
        ca = covered[max(dy, 0):h, max(dx, 0):w + min(dx, 0)]
        cb = covered[:h - dy, max(-dx, 0):w - max(dx, 0)]
        fold = ((a * b).sum(-1) < 0.94) | (ca != cb)
        edge[max(dy, 0):h, max(dx, 0):w + min(dx, 0)] |= fold & ca
        edge[:h - dy, max(-dx, 0):w - max(dx, 0)] |= fold & cb
    rgb[edge] = EDGE

    alpha = np.where(covered | edge, 255, 0)
    img = np.dstack([np.clip(rgb, 0, 255), alpha]).astype(np.uint8)
    return Image.fromarray(img, "RGBA").resize((VIEW_W, VIEW_H), Image.LANCZOS)


def main() -> int:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else STL
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else OUT
    tri = hang(load(src))
    views = [rotation(-28, 16), rotation(180 + 28, 16)]    # front, the wall side
    # one scale for both, so the two views are the same size
    reach = max(np.abs((tri.reshape(-1, 3) @ r.T)[:, :2]).max(0)[k] / lim
                for r in views for k, lim in ((0, VIEW_W / 2), (1, VIEW_H / 2)))
    scale = 0.9 / reach
    imgs = [render(tri, r, scale) for r in views]
    sheet = Image.new("RGBA", (VIEW_W * 2, VIEW_H), (0, 0, 0, 0))
    for k, im in enumerate(imgs):
        sheet.alpha_composite(im, (k * VIEW_W, 0))
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out, optimize=True)
    print(out.relative_to(ROOT) if out.is_relative_to(ROOT) else out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
