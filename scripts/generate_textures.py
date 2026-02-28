#!/usr/bin/env python3
import math
import random
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures"
OUT.mkdir(parents=True, exist_ok=True)


def write_bmp(path: Path, w: int, h: int, pixel_fn):
    row_padded = (w * 3 + 3) & ~3
    size = 54 + row_padded * h
    with path.open("wb") as f:
        f.write(b"BM")
        f.write(struct.pack("<IHHI", size, 0, 0, 54))
        f.write(struct.pack("<IIIHHIIIIII", 40, w, h, 1, 24, 0, row_padded * h, 2835, 2835, 0, 0))
        for y in range(h - 1, -1, -1):
            row = bytearray()
            for x in range(w):
                r, g, b = pixel_fn(x, y)
                row.extend([b, g, r])
            row.extend(b"\x00" * (row_padded - w * 3))
            f.write(row)


def clamp(v):
    return max(0, min(255, int(v)))


def main():
    w = h = 256
    rnd = random.Random(42)

    write_bmp(OUT / "steel_plate.bmp", w, h, lambda x, y: (
        clamp(95 + rnd.randint(-12, 12)),
        clamp(105 + rnd.randint(-10, 10)),
        clamp(112 + rnd.randint(-10, 10)),
    ))

    write_bmp(OUT / "rust_panel.bmp", w, h, lambda x, y: (
        clamp(120 + rnd.randint(-30, 30)),
        clamp(75 + rnd.randint(-20, 20)),
        clamp(50 + rnd.randint(-20, 20)),
    ))

    write_bmp(OUT / "grate_floor.bmp", w, h, lambda x, y: (
        clamp(80 + (30 if (x % 20 < 2 or y % 20 < 2) else 0) + rnd.randint(-8, 8)),
        clamp(88 + (25 if (x % 20 < 2 or y % 20 < 2) else 0) + rnd.randint(-8, 8)),
        clamp(94 + (25 if (x % 20 < 2 or y % 20 < 2) else 0) + rnd.randint(-8, 8)),
    ))

    write_bmp(OUT / "water_view.bmp", w, h, lambda x, y: (
        clamp(25 + rnd.randint(-8, 8)),
        clamp(70 + 25 * math.sin(x * 0.08) + rnd.randint(-8, 8)),
        clamp(110 + 35 * math.sin((x + y) * 0.05) + rnd.randint(-8, 8)),
    ))

    rnd2 = random.Random(7)
    write_bmp(OUT / "hull_brushed.bmp", w, h, lambda x, y: (
        clamp(110 + 35 * math.sin(y * 0.25) + rnd2.randint(-12, 12)),
        clamp(120 + 20 * math.sin(y * 0.18) + rnd2.randint(-10, 10)),
        clamp(128 + 18 * math.sin(y * 0.2) + rnd2.randint(-10, 10)),
    ))

    write_bmp(OUT / "pipe_oil.bmp", w, h, lambda x, y: (
        clamp(80 + 22 * math.sin((x + y) * 0.08) + rnd2.randint(-8, 8)),
        clamp(90 + 28 * math.sin(x * 0.12) + rnd2.randint(-8, 8)),
        clamp(74 + 18 * math.sin(y * 0.11) + rnd2.randint(-8, 8)),
    ))

    write_bmp(OUT / "warning_paint.bmp", w, h, lambda x, y: (
        clamp((170 + rnd2.randint(-30, 20)) if (x // 22) % 2 == 0 else (30 + rnd2.randint(-20, 10))),
        clamp((120 + rnd2.randint(-25, 20)) if (x // 22) % 2 == 0 else (30 + rnd2.randint(-15, 10))),
        clamp((30 + rnd2.randint(-20, 20)) if (x // 22) % 2 == 0 else (25 + rnd2.randint(-10, 10))),
    ))

    write_bmp(OUT / "deck_plate.bmp", w, h, lambda x, y: (
        clamp(95 + (18 if ((x % 32 < 2) or (y % 32 < 2)) else 0) + rnd2.randint(-8, 8)),
        clamp(102 + (16 if ((x % 32 < 2) or (y % 32 < 2)) else 0) + rnd2.randint(-8, 8)),
        clamp(108 + (16 if ((x % 32 < 2) or (y % 32 < 2)) else 0) + rnd2.randint(-8, 8)),
    ))

    print(f"Generated textures in {OUT}")


if __name__ == "__main__":
    main()
