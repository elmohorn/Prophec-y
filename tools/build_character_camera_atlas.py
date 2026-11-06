#!/usr/bin/env python3
"""Extract only the approved photographic camera interiors for the JUCE Tube bezel.

Source: "Módulos de Tremolo y Spring Ópticos.png" (1817x866).
Output: 448x360 RGB JPEG: spring, lamp dark, lamp lit (120 px each).

No interface chrome, legends, knobs or alternative bezel are included.
Requires Pillow: python -m pip install Pillow
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path
from PIL import Image, ImageChops

SOURCE_SIZE = (1817, 866)
ROW_SIZE = (448, 120)
SPRING_CROP = (425, 535, 1305, 773)
LAMP_CROP = (427, 109, 1305, 347)


def dim_lamp(image: Image.Image) -> Image.Image:
    """Retain bulb, metal socket and LDR; dim orange beam/filament independently."""
    result = image.copy()
    source_pixels = image.load()
    output_pixels = result.load()
    for y in range(ROW_SIZE[1]):
        for x in range(ROW_SIZE[0]):
            r, g, b = source_pixels[x, y]
            warm = max(0.0, r - b) / 255.0
            brightness = max(r, g, b) / 255.0
            beam = math.exp(-(((x - 280) / 155.0) ** 2 +
                              ((y - 62) / 29.0) ** 2))
            bulb = math.exp(-(((x - 190) / 52.0) ** 2 +
                              ((y - 64) / 43.0) ** 2))
            attenuation = min(0.48, max(0.0, (warm * 0.47 + brightness * 0.12) *
                                       (0.6 * beam + 0.9 * bulb)))
            factor = max(0.0, 0.75 - attenuation)
            output_pixels[x, y] = tuple(round(c * factor) for c in (r, g, b))
    return result


def build(source: Path, dest: Path, quality: int) -> None:
    with Image.open(source) as photo:
        if photo.size != SOURCE_SIZE:
            raise SystemExit(f"Wrong source dimensions: {photo.size}; expected {SOURCE_SIZE}")
        photo = photo.convert("RGB")
        resample = Image.Resampling.LANCZOS
        spring = photo.crop(SPRING_CROP).resize(ROW_SIZE, resample)
        lamp_on = photo.crop(LAMP_CROP).resize(ROW_SIZE, resample)
    lamp_off = dim_lamp(lamp_on)
    if ImageChops.difference(lamp_on, lamp_off).getbbox() is None:
        raise SystemExit("Lamp off/on renders must differ")
    atlas = Image.new("RGB", (ROW_SIZE[0], ROW_SIZE[1] * 3))
    atlas.paste(spring, (0, 0))
    atlas.paste(lamp_off, (0, ROW_SIZE[1]))
    atlas.paste(lamp_on, (0, ROW_SIZE[1] * 2))
    dest.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(dest, format="JPEG", quality=quality, subsampling=0, optimize=True)
    with Image.open(dest) as check:
        assert check.size == (448, 360)
        assert check.format == "JPEG"
    print(f"Created {dest} ({dest.stat().st_size} bytes, 448x360)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Approved source PNG")
    parser.add_argument("--output", type=Path,
                        default=Path("plugin/assets/character_cameras_atlas.jpg"))
    parser.add_argument("--quality", type=int, default=81)
    args = parser.parse_args()
    if not 1 <= args.quality <= 100:
        parser.error("--quality must be 1..100")
    build(args.source, args.output, args.quality)
