"""Generates Pixie's icon (resources/icon/pixie.png and pixie.ico) from a 16x16 pixel-art map.

Run from the repo root:  python tools/make_icon.py
"""
from pathlib import Path

from PIL import Image

# '.' = transparent. Letters are palette keys.
ART = [
    "..BBBBBBBBBBBB..",
    ".BBBBBBBBBBBBBB.",
    "BBBBBBBOOBBBBBBB",
    "BBBBBBBYYBBBBPBB",
    "BBBBBBBYYBBBBBBB",
    "BBBBBBYYYYBBBBBB",
    "BBBBYYYWWYYYBBBB",
    "BYYYYYWWWWYYYYYB",
    "BYYYYYWWWWYYYYYB",
    "BBBBYYYWWYYYBBBB",
    "BBBBBBYYYYBBBBBB",
    "BBBBBBBYYBBBBBBB",
    "BBBBBBBYYBBBBBBB",
    "BBCBBBBOOBBBBBBB",
    ".BBBBBBBBBBBBBB.",
    "..BBBBBBBBBBBB..",
]

PALETTE = {
    ".": (0, 0, 0, 0),
    "B": (32, 32, 58, 255),    # background
    "b": (70, 70, 120, 255),   # outline
    "Y": (255, 216, 74, 255),  # sparkle
    "W": (255, 255, 255, 255), # sparkle core
    "O": (255, 150, 58, 255),  # sparkle tips
    "C": (90, 214, 255, 255),  # small cyan dust
    "P": (255, 110, 199, 255), # small pink dust
}

ICO_SIZES = [16, 32, 48, 64, 128, 256]  # all multiples of 16, so scaling is exact


def build_base():
    size = len(ART)
    image = Image.new("RGBA", (size, size))
    for y, row in enumerate(ART):
        assert len(row) == size, f"row {y} has {len(row)} columns"
        for x, key in enumerate(row):
            image.putpixel((x, y), PALETTE[key])

    # Outline: background pixels touching the edge of the shape become the lighter color.
    outline = []
    for y in range(size):
        for x in range(size):
            if ART[y][x] != "B":
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < size and 0 <= ny < size) or ART[ny][nx] == ".":
                    outline.append((x, y))
                    break
    for x, y in outline:
        image.putpixel((x, y), PALETTE["b"])
    return image


def main():
    base = build_base()
    out_dir = Path(__file__).resolve().parent.parent / "resources" / "icon"
    out_dir.mkdir(parents=True, exist_ok=True)

    scaled = {s: base.resize((s, s), Image.NEAREST) for s in ICO_SIZES}
    scaled[256].save(out_dir / "pixie.png")
    scaled[256].save(
        out_dir / "pixie.ico",
        format="ICO",
        sizes=[(s, s) for s in ICO_SIZES],
        append_images=[scaled[s] for s in ICO_SIZES if s != 256],
    )
    print("wrote", out_dir / "pixie.png", "and", out_dir / "pixie.ico")


if __name__ == "__main__":
    main()
