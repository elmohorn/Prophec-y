#!/usr/bin/env python3
"""Bake reproducible 640x190 RGBA Character camera layers from the approved atlas.

Source JPEG is a photographic study, NOT an artist-provided alpha master.
These are interim, offline-separated layers. Replace with individually
authored RGBA masters when approved; keep stable filenames and dimensions.
The native JUCE renderer never performs this extraction per frame.
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

PHOTO_W, PHOTO_H = 448, 100
CANVAS = (640, 190)
DEST = (29, 14, 582, 130)
NAMES = (
    "spring_backplate.png", "spring_upper_coil.png",
    "spring_lower_coil.png", "spring_reflection_overlay.png",
    "optical_backplate.png", "optical_lamp_off.png",
    "optical_lamp_on.png", "optical_ldr_overlay.png",
)


def clamp(x: float) -> float:
    return max(0.0, min(1.0, x))


def smooth(x: float) -> float:
    x = clamp(x)
    return x * x * (3.0 - 2.0 * x)


def atlas_row(atlas: Image.Image, index: int) -> Image.Image:
    # Approved atlas contains three 448x120 rows; retain the central 100px.
    return atlas.crop((0, index * 120 + 14, 448, index * 120 + 114)).convert("RGB")


def on_canvas(img: Image.Image) -> Image.Image:
    out = Image.new("RGBA", CANVAS, (0, 0, 0, 0))
    fitted = img.convert("RGBA").resize((DEST[2], DEST[3]), Image.Resampling.LANCZOS)
    out.paste(fitted, DEST[:2])
    return out


def write_png(folder: Path, name: str, image: Image.Image) -> None:
    assert image.size == CANVAS
    assert image.mode == "RGBA"
    image.save(folder / name, "PNG", optimize=True)


def spring_layers(photo: Image.Image) -> tuple[Image.Image, Image.Image, Image.Image, Image.Image]:
    # Coordinates are from the original photographic source (not editor UI).
    # The two anchor sockets and metal frame remain in the static backplate.
    original = photo.load()
    back = photo.copy()
    output = [Image.new("RGBA", (PHOTO_W, PHOTO_H), (0, 0, 0, 0)) for _ in range(2)]
    out_pixels = [image.load() for image in output]
    back_pixels = back.load()
    bands = ((24, 62), (73, 114))  # original atlas row coordinates
    for y in range(PHOTO_H):
        ay = y + 14
        for x in range(PHOTO_W):
            r, g, b = original[x, y]
            for i, (top, bottom) in enumerate(bands):
                if not (94 <= x <= 357 and top <= ay <= bottom):
                    continue
                xf = smooth(min(x - 94, 357 - x) / 19.0)
                yf = smooth(min(ay - top, bottom - ay) / 5.0)
                bounds = xf * yf
                metallic = clamp(max(((g / 255.0) - 0.29 * r / 255.0) * 5.3,
                                     ((b / 255.0) - 0.09 * r / 255.0) * 4.8))
                bright = clamp(((r / 255.0) - 0.11) * 4.8)
                matte = bounds * clamp(0.23 * bright + 0.92 * metallic)
                if matte <= 0.01:
                    continue
                out_pixels[i][x, y] = (r, g, b, round(255 * matte))
                # Neighboring-row reconstruction; fixed sockets are excluded.
                upper = original[x, max(0, top - 5 - 14)]
                lower = original[x, min(PHOTO_H - 1, bottom + 5 - 14)]
                fraction = clamp((ay - top) / max(1.0, bottom - top))
                clean = tuple(round(0.76 * ((1 - fraction) * upper[c] +
                                            fraction * lower[c])) for c in range(3))
                back_pixels[x, y] = tuple(round((1 - bounds) * original[x, y][c] +
                                                  bounds * clean[c]) for c in range(3))
    # A separate, stationary highlight layer follows *measured* spring energy,
    # but does not move chassis metal, screws, or the coil anchor sockets.
    reflection = Image.new("RGBA", CANVAS, (0, 0, 0, 0))
    glow = Image.new("L", CANVAS, 0)
    draw = ImageDraw.Draw(glow)
    draw.ellipse((220, 55, 485, 100), fill=85)
    draw.ellipse((225, 112, 492, 148), fill=70)
    glow = glow.filter(ImageFilter.GaussianBlur(radius=16))
    reflection.paste(Image.new("RGBA", CANVAS, (255, 161, 60, 255)), (0, 0))
    reflection.putalpha(glow)
    return (on_canvas(back), on_canvas(output[0]), on_canvas(output[1]), reflection)


def optical_layers(dark: Image.Image, lit: Image.Image) -> tuple[Image.Image, Image.Image, Image.Image, Image.Image]:
    # Geometry stays anchored: lamp/socket at x~159, sensor at x~380.
    off = dark.load()
    on = lit.load()
    lamp_off = Image.new("RGBA", (PHOTO_W, PHOTO_H), (0, 0, 0, 0))
    lamp_layer = Image.new("RGBA", (PHOTO_W, PHOTO_H), (0, 0, 0, 0))
    sensor_layer = Image.new("RGBA", (PHOTO_W, PHOTO_H), (0, 0, 0, 0))
    out_off, out_lamp, out_sensor = lamp_off.load(), lamp_layer.load(), sensor_layer.load()
    for y in range(PHOTO_H):
        fy = float(y + 14)
        for x in range(PHOTO_W):
            fx = float(x)
            old = off[x, y]
            new = on[x, y]
            # Partial glass/lamp geometry, kept still under a separate glow.
            glass = math.exp(-((fx - 159) / 67) ** 4 - ((fy - 71) / 46) ** 4)
            if glass > 0.005:
                out_off[x, y] = (*old, round(255 * glass))
            bulb = math.exp(-((fx - 159) / 65) ** 2 - ((fy - 71) / 45) ** 2)
            width = 14 + 0.19 * max(0.0, fx - 205)
            beam = (math.exp(-((fy - 75) / width) ** 2) *
                    smooth(min(fx - 205, 378 - fx) / 18)
                    if 205 < fx < 378 else 0.0)
            lamp_focus = clamp(max(bulb, beam)) * smooth((fx - 85) / 23) * smooth((367 - fx) / 20)
            sensor_focus = (math.exp(-((fx - 380) / 36) ** 2 - ((fy - 70) / 40) ** 2) *
                            smooth((fx - 350) / 13) * smooth((428 - fx) / 13))
            difference = [max(0, new[c] - old[c]) for c in range(3)]
            required_alpha = max((difference[c] / max(1, 255 - old[c]))
                                 for c in range(3))
            if required_alpha <= 0:
                continue
            glow_rgb = tuple(min(255, round(old[c] + difference[c] / required_alpha))
                             for c in range(3))
            a_lamp = round(255 * clamp(required_alpha * lamp_focus))
            a_sensor = round(255 * clamp(required_alpha * sensor_focus))
            if a_lamp:
                out_lamp[x, y] = (*glow_rgb, a_lamp)
            if a_sensor:
                out_sensor[x, y] = (*glow_rgb, a_sensor)
    # Backplate already contains real dark glass/metal, so the off overlay
    # reproduces it without moving hardware. Future human-authored layers
    # may replace these pixels without a renderer/API change.
    return (on_canvas(dark), on_canvas(lamp_off),
            on_canvas(lamp_layer), on_canvas(sensor_layer))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    atlas = Image.open(args.source).convert("RGB")
    if atlas.size != (448, 360):
        raise SystemExit(f"Expected approved atlas 448x360, got {atlas.size}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    images = (*spring_layers(atlas_row(atlas, 0)),
              *optical_layers(atlas_row(atlas, 1), atlas_row(atlas, 2)))
    for name, image in zip(NAMES, images):
        write_png(args.output_dir, name, image)
        pixels = image.getchannel("A")
        minimum, maximum = pixels.getextrema()
        if minimum != 0 or maximum == 0:
            raise SystemExit(f"Invalid transparency or empty asset: {name}")
        print(name, image.size, "alpha range", (minimum, maximum))


if __name__ == "__main__":
    main()
