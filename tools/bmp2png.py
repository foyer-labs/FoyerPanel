#!/usr/bin/env python3
"""Converte i BMP prodotti da --cattura in PNG. Nessuna dipendenza."""
import struct, sys, zlib
from pathlib import Path


def converti(src: Path, dst: Path) -> None:
    d = src.read_bytes()
    off = struct.unpack_from("<I", d, 10)[0]
    w = struct.unpack_from("<i", d, 18)[0]
    h = struct.unpack_from("<i", d, 22)[0]
    dall_alto = h < 0
    h = abs(h)
    riga_byte = (w * 3 + 3) & ~3

    righe = []
    for y in range(h):
        i = off + y * riga_byte
        r = d[i:i + w * 3]
        righe.append(b"\x00" + bytes(
            b for x in range(w) for b in (r[x * 3 + 2], r[x * 3 + 1], r[x * 3])))
    if not dall_alto:
        righe.reverse()

    def blocco(tipo, dati):
        return (struct.pack(">I", len(dati)) + tipo + dati
                + struct.pack(">I", zlib.crc32(tipo + dati)))

    dst.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + blocco(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
        + blocco(b"IDAT", zlib.compress(b"".join(righe), 6))
        + blocco(b"IEND", b""))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        raise SystemExit(1)
    for a in sys.argv[1:]:
        p = Path(a)
        for f in ([p] if p.is_file() else sorted(p.glob("*.bmp"))):
            converti(f, f.with_suffix(".png"))
            print(f.with_suffix(".png"))
