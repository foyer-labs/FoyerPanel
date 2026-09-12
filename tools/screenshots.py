#!/usr/bin/env python3
"""The README's screenshots, from the simulator and a demo house.

    python3 tools/screenshots.py           the panel's screens
    python3 tools/screenshots.py --page    and the configuration page too,
                                           through a headless Chrome or Edge

The house is tools/demo/config.json — the example configuration with
English names — and what the simulator makes up when no Home Assistant is
connected: temperatures, consumption, a robot with an alert, a calendar.
Every screen is in English, at the panel's real resolution, at the same
fixed time (18:41, Tuesday 25 August), and lands in docs/screenshots/.

Needs build/pannello (cmake -B build -G Ninja && cmake --build build).
The PNGs are compressed with Pillow when it is installed, and written by
tools/bmp2png.py otherwise. The page's screenshots need a Chromium browser:
FOYER_PANEL_BROWSER, else chromium or google-chrome on the PATH, else — under
WSL — the Windows Chrome or Edge.
"""
from __future__ import annotations

import argparse
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bmp2png import converti  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
BINARY = ROOT / "build" / "pannello"
DEMO = ROOT / "tools" / "demo" / "config.json"
OUT = ROOT / "docs" / "screenshots"
ENV = dict(os.environ, PANNELLO_ORA=str(1787676060), TZ="Europe/Rome")
SECRETS = ["--segreto", "wifi_share1_pw=guest-welcome-2026",
           "--segreto", "wifi_pw=not-shown"]

LAND, PORT = "p4-1280x800", "p4-800x1280"
# file name -> profile and what to show
SHOTS = {
    # A window open in the kitchen: the home's card and the standby's pill
    # have something to say, instead of "0 openings".
    "home":             (LAND, ["--sezione", "home", "--casi", "apertura"]),
    "home-portrait":    (PORT, ["--sezione", "home", "--casi", "apertura"]),
    "standby":          (LAND, ["--stato", "standby", "--casi", "apertura"]),
    "lights":           (LAND, ["--sezione", "lights"]),
    "climate":          (LAND, ["--sezione", "climate"]),
    "air-conditioners": (LAND, ["--sezione", "climate", "--vista", "1"]),
    "air-conditioner":  (LAND, ["--sezione", "climate", "--vista", "10"]),
    "energy":           (LAND, ["--sezione", "energy"]),
    "access":           (LAND, ["--sezione", "access"]),
    "schedules":        (LAND, ["--sezione", "schedules"]),
    "robot":            (LAND, ["--sezione", "robot"]),
    "switches":         (LAND, ["--sezione", "switches"]),
    "calendar":         (LAND, ["--sezione", "calendar"]),
    "wifi-sharing":     (LAND, ["--sezione", "wifi", "--vista", "1"]),
    "information":      (LAND, ["--sezione", "settings", "--vista", "4"]),
    "boot":             (LAND, ["--stato", "avvio"]),
}
# file name -> browser window and the page's address after the host
PAGE = {
    "page":              ((1280, 860), "/?lang=en#network"),
    "page-translations": ((1280, 860), "/?lang=en#translations"),
}


def save_png(bmp: Path, png: Path) -> None:
    try:
        from PIL import Image
    except ImportError:
        converti(bmp, png)
        return
    Image.open(bmp).convert("RGB").save(png, optimize=True)


def panel(data: Path, only: set[str]) -> int:
    done = 0
    for name, (profile, args) in SHOTS.items():
        if only and name not in only:
            continue
        bmp = OUT / f"{name}.bmp"
        cmd = [str(BINARY), "--dati", str(data), "--profilo", profile,
               "--lingua", "en", *args, *SECRETS, "--cattura", str(bmp)]
        r = subprocess.run(cmd, capture_output=True, text=True, env=ENV, cwd=ROOT)
        if r.returncode != 0 or not bmp.exists():
            print(f"capture failed: {name}\n{r.stderr}", file=sys.stderr)
            return -1
        save_png(bmp, OUT / f"{name}.png")
        bmp.unlink()
        print(f"docs/screenshots/{name}.png")
        done += 1
    return done


def find_browser() -> list[str] | None:
    if os.environ.get("FOYER_PANEL_BROWSER"):
        return [os.environ["FOYER_PANEL_BROWSER"]]
    for b in ("chromium", "chromium-browser", "google-chrome"):
        if shutil.which(b):
            return [shutil.which(b)]
    for b in ("/mnt/c/Program Files/Google/Chrome/Application/chrome.exe",
              "/mnt/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"):
        if Path(b).exists():
            return [b]
    return None


def browser_path(p: Path, browser: str) -> str:
    """A Windows browser run from WSL wants a Windows path."""
    if browser.endswith(".exe"):
        return subprocess.run(["wslpath", "-w", str(p)], capture_output=True,
                              text=True).stdout.strip()
    return str(p)


def page(data: Path, only: set[str]) -> int:
    wanted = [n for n in PAGE if not only or n in only]
    if not wanted:
        return 0
    browser = find_browser()
    if not browser:
        print("no Chromium browser found: set FOYER_PANEL_BROWSER", file=sys.stderr)
        return -1
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        port = s.getsockname()[1]
    sim = subprocess.Popen([str(BINARY), "--dati", str(data), "--lingua", "en",
                            "--web", str(port), "--sblocca", *SECRETS],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           env=ENV, cwd=ROOT)
    try:
        for _ in range(50):
            try:
                socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.2)
        done = 0
        for name in wanted:
            (w, h), path = PAGE[name]
            png = OUT / f"{name}.png"
            cmd = browser + ["--headless=new", "--disable-gpu", "--hide-scrollbars",
                             f"--window-size={w},{h}", "--virtual-time-budget=8000",
                             f"--screenshot={browser_path(png, browser[0])}",
                             f"http://localhost:{port}{path}"]
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            if not png.exists():
                print(f"page screenshot failed: {name}\n{r.stderr[-400:]}", file=sys.stderr)
                return -1
            print(f"docs/screenshots/{name}.png")
            done += 1
        return done
    finally:
        sim.terminate()
        sim.wait(timeout=10)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--page", action="store_true", help="also the configuration page")
    ap.add_argument("only", nargs="*", help="only these screenshots, by file name")
    arg = ap.parse_args()
    if not BINARY.exists():
        print("build/pannello is missing: cmake -B build -G Ninja && cmake --build build",
              file=sys.stderr)
        return 1
    OUT.mkdir(parents=True, exist_ok=True)
    only = set(arg.only)
    with tempfile.TemporaryDirectory() as tmp:
        data = Path(tmp)
        shutil.copy(DEMO, data / "config.json")
        n = panel(data, only)
        if n < 0:
            return 1
        if arg.page or only & set(PAGE):
            m = page(data, only)
            if m < 0:
                return 1
            n += m
    print(f"{n} screenshots in docs/screenshots/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
