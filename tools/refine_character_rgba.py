#!/usr/bin/env python3
"""Deterministic interim RGBA mask refinement from the approved camera atlas.

Regenerates original masks in a temporary directory before refining them,
so repeated runs are idempotent. NEVER run automatically on an artist's PNG
masters; review output and commit explicitly.
"""
from __future__ import annotations
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path
import cv2
import numpy as np
from PIL import Image

CANVAS = (640, 190)
ROI = (29, 14, 582, 130)


def output_canvas(inner: np.ndarray) -> Image.Image:
    out = Image.new("RGBA", CANVAS)
    piece = Image.fromarray(inner.astype("uint8"), "RGB")
    out.paste(piece.resize((ROI[2], ROI[3]), Image.Resampling.LANCZOS), ROI[:2])
    return out


def refine_spring_backplate(atlas: Image.Image) -> Image.Image:
    # Remove the complete winding silhouettes, not just bright silver traces.
    # Only the central cavity is inpainted; physical sockets remain untouched.
    original = np.array(atlas.crop((0, 14, 448, 114)).convert("RGB"))
    mask = np.zeros((100, 448), np.uint8)
    cv2.rectangle(mask, (105, 8), (345, 51), 255, -1)
    cv2.rectangle(mask, (105, 62), (345, 99), 255, -1)
    cleaned = cv2.inpaint(original, mask, 6, cv2.INPAINT_TELEA)
    feather = cv2.GaussianBlur(mask.astype("float32")/255, (0, 0), 4)[:,:,None]
    result = np.clip(original*(1-feather)+cleaned*feather,0,255).astype("uint8")
    return output_canvas(result)


def tighten_alpha(image: Image.Image, threshold: int, gamma: float) -> Image.Image:
    arr = np.array(image.convert("RGBA"))
    a = arr[:,:,3].astype("float32")
    norm = np.clip((a-threshold)/max(1,255-threshold),0,1)
    smooth = norm*norm*(3-2*norm)
    arr[:,:,3] = np.rint(np.clip(a*(smooth**gamma),0,255)).astype("uint8")
    return Image.fromarray(arr, "RGBA")


def localize_light(image: Image.Image, lamp: bool) -> Image.Image:
    arr = np.array(image.convert("RGBA"))
    h,w = arr.shape[:2]
    ys,xs = np.mgrid[:h,:w].astype("float32")
    if lamp:
        bulb = np.exp(-(((xs-267)/103)**4+((ys-91)/61)**4))
        corridor = ((xs>=330)&(xs<=512))*np.exp(-((ys-93)/38)**2)
        focus = np.maximum(bulb, corridor*.85)
        focus *= np.clip((xs-128)/36,0,1)*np.clip((515-xs)/35,0,1)
    else:
        focus = np.exp(-(((xs-520)/53)**4+((ys-91)/62)**4))
        focus *= np.clip((xs-465)/28,0,1)*np.clip((580-xs)/30,0,1)
    arr[:,:,3] = np.rint(arr[:,:,3].astype("float32")*focus).astype("uint8")
    return Image.fromarray(arr, "RGBA")


def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source", type=Path, required=True)
    ap.add_argument("--output-dir", type=Path, required=True)
    args=ap.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="p6-rgba-base-") as folder:
        base=Path(folder)
        subprocess.check_call([sys.executable,
             str(Path(__file__).with_name("build_character_rgba.py")),
             "--source",str(args.source),"--output-dir",str(base)])
        for f in sorted(base.glob("*.png")):
            img=Image.open(f).convert("RGBA")
            if f.name=="spring_backplate.png":
                img=refine_spring_backplate(Image.open(args.source).convert("RGB"))
            elif f.name in ("spring_upper_coil.png","spring_lower_coil.png"):
                img=tighten_alpha(img,15,.42)
            elif f.name=="optical_lamp_on.png":
                img=localize_light(tighten_alpha(img,8,.28),True)
            elif f.name=="optical_ldr_overlay.png":
                img=localize_light(tighten_alpha(img,8,.22),False)
            elif f.name=="spring_reflection_overlay.png":
                img=tighten_alpha(img,2,.12)
            elif f.name=="optical_lamp_off.png":
                img=tighten_alpha(img,3,.18)
            img.save(args.output_dir/f.name,"PNG",optimize=True)
            alpha=np.asarray(img.getchannel("A"))
            assert img.size==CANVAS and alpha.min()==0 and alpha.max()>0,f.name
            print(f.name, "alpha coverage",round(float(np.count_nonzero(alpha))/alpha.size,3))


if __name__=="__main__":
    main()
