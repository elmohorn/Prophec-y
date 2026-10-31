#!/usr/bin/env python3
"""Compare a Prophet-6 hardware reference segment with a candidate render.

The output intentionally reports multiple domain metrics instead of a single
"percent accurate" score.  It is designed for lossy web references where
sample-exact waveform correlation would be misleading.

Requires: numpy, scipy, ffmpeg.
"""
from __future__ import annotations

import argparse
import json
import math
import subprocess
from pathlib import Path

import numpy as np
from scipy.signal import stft


def decode_stereo(path: Path, sr: int, start: float | None = None, end: float | None = None) -> np.ndarray:
    cmd = ["ffmpeg", "-v", "error"]
    if start is not None:
        cmd += ["-ss", f"{start:.9f}"]
    cmd += ["-i", str(path)]
    if end is not None:
        duration = end - (start or 0.0)
        if duration > 0:
            cmd += ["-t", f"{duration:.9f}"]
    cmd += ["-ac", "2", "-ar", str(sr), "-f", "f32le", "-acodec", "pcm_f32le", "pipe:1"]
    raw = subprocess.check_output(cmd)
    x = np.frombuffer(raw, dtype="<f4").reshape(-1, 2).astype(np.float64)
    return x


def trim_activity(x: np.ndarray, sr: int, threshold_db: float = -45.0, pad_s: float = 0.08) -> np.ndarray:
    mono = x.mean(axis=1)
    hop = max(1, int(0.01 * sr))
    n = len(mono) // hop
    if n < 2:
        return x
    env = np.sqrt(np.mean(mono[:n * hop].reshape(n, hop) ** 2, axis=1) + 1e-15)
    peak = float(env.max())
    if peak <= 1e-10:
        return x
    active = np.flatnonzero(20 * np.log10(env / peak + 1e-12) >= threshold_db)
    if len(active) == 0:
        return x
    pad = int(round(pad_s * sr))
    a = max(0, int(active[0] * hop) - pad)
    b = min(len(x), int((active[-1] + 1) * hop) + pad)
    return x[a:b]


def rms(x: np.ndarray) -> float:
    return math.sqrt(float(np.mean(x * x)) + 1e-20)


def normalize_rms(x: np.ndarray, target: float = 0.1) -> tuple[np.ndarray, float]:
    r = rms(x)
    if r < 1e-10:
        return x.copy(), 0.0
    g = target / r
    return x * g, 20 * math.log10(g)


def spectral_features(x: np.ndarray, sr: int) -> dict:
    mono = x.mean(axis=1)
    if len(mono) < 2048:
        mono = np.pad(mono, (0, 2048 - len(mono)))
    f, _, z = stft(mono, fs=sr, window="hann", nperseg=2048, noverlap=1536, boundary=None, padded=False)
    p = np.abs(z) ** 2 + 1e-18
    mean_p = p.mean(axis=1)
    total = float(mean_p.sum()) + 1e-20
    centroid = float((f * mean_p).sum() / total)
    c = np.cumsum(mean_p) / total
    roll85 = float(f[min(len(f) - 1, int(np.searchsorted(c, 0.85)))])
    roll95 = float(f[min(len(f) - 1, int(np.searchsorted(c, 0.95)))])

    edges = np.geomspace(40.0, min(20000.0, sr * 0.47), 33)
    band_db = []
    for lo, hi in zip(edges[:-1], edges[1:]):
        idx = (f >= lo) & (f < hi)
        v = float(mean_p[idx].mean()) if np.any(idx) else 1e-18
        band_db.append(10.0 * math.log10(v + 1e-18))
    band_db = np.array(band_db)
    # Remove overall level so this describes spectral shape, not loudness.
    band_db -= np.mean(band_db)
    return {"centroid_hz": centroid, "rolloff85_hz": roll85, "rolloff95_hz": roll95, "band_db": band_db}


def envelope(x: np.ndarray, sr: int) -> np.ndarray:
    mono = x.mean(axis=1)
    hop = max(1, int(round(0.010 * sr)))
    n = len(mono) // hop
    if n == 0:
        return np.zeros(1)
    e = np.sqrt(np.mean(mono[:n * hop].reshape(n, hop) ** 2, axis=1) + 1e-15)
    m = float(e.max())
    return e / m if m > 1e-12 else e


def resample_vector(v: np.ndarray, n: int) -> np.ndarray:
    if len(v) == n:
        return v
    old = np.linspace(0.0, 1.0, len(v))
    new = np.linspace(0.0, 1.0, n)
    return np.interp(new, old, v)


def stereo_metrics(x: np.ndarray) -> dict:
    l, r = x[:, 0], x[:, 1]
    lr = np.corrcoef(l, r)[0, 1] if np.std(l) > 1e-10 and np.std(r) > 1e-10 else 1.0
    mid = (l + r) * 0.5
    side = (l - r) * 0.5
    width = rms(side) / max(rms(mid), 1e-10)
    return {"lr_correlation": float(lr), "side_to_mid_rms": float(width)}


def descriptors(x: np.ndarray, sr: int) -> dict:
    mono = x.mean(axis=1)
    r = rms(mono)
    peak = float(np.max(np.abs(mono))) if len(mono) else 0.0
    crest = peak / max(r, 1e-10)
    sf = spectral_features(x, sr)
    st = stereo_metrics(x)
    return {
        "duration_s": len(x) / sr,
        "rms": r,
        "peak": peak,
        "crest_factor": crest,
        "centroid_hz": sf["centroid_hz"],
        "rolloff85_hz": sf["rolloff85_hz"],
        "rolloff95_hz": sf["rolloff95_hz"],
        **st,
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("reference", type=Path)
    ap.add_argument("candidate", type=Path)
    ap.add_argument("--ref-start", type=float)
    ap.add_argument("--ref-end", type=float)
    ap.add_argument("--candidate-start", type=float)
    ap.add_argument("--candidate-end", type=float)
    ap.add_argument("--sample-rate", type=int, default=48000)
    ap.add_argument("--no-trim", action="store_true")
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()

    ref = decode_stereo(args.reference, args.sample_rate, args.ref_start, args.ref_end)
    cand = decode_stereo(args.candidate, args.sample_rate, args.candidate_start, args.candidate_end)
    if not args.no_trim:
        ref = trim_activity(ref, args.sample_rate)
        cand = trim_activity(cand, args.sample_rate)

    ref_n, ref_gain = normalize_rms(ref)
    cand_n, cand_gain = normalize_rms(cand)
    rf = spectral_features(ref_n, args.sample_rate)
    cf = spectral_features(cand_n, args.sample_rate)
    band_diff = cf["band_db"] - rf["band_db"]
    band_rmse = float(np.sqrt(np.mean(band_diff ** 2)))
    band_mae = float(np.mean(np.abs(band_diff)))
    cosine = float(np.dot(rf["band_db"], cf["band_db"]) /
                   (np.linalg.norm(rf["band_db"]) * np.linalg.norm(cf["band_db"]) + 1e-20))

    er, ec = envelope(ref_n, args.sample_rate), envelope(cand_n, args.sample_rate)
    nenv = max(len(er), len(ec), 2)
    envelope_mae = float(np.mean(np.abs(resample_vector(er, nenv) - resample_vector(ec, nenv))))

    result = {
        "reference": str(args.reference),
        "candidate": str(args.candidate),
        "sample_rate": args.sample_rate,
        "preprocessing": {
            "activity_trim": not args.no_trim,
            "reference_gain_db_for_rms_normalization": ref_gain,
            "candidate_gain_db_for_rms_normalization": cand_gain,
        },
        "reference_descriptors": descriptors(ref_n, args.sample_rate),
        "candidate_descriptors": descriptors(cand_n, args.sample_rate),
        "differences": {
            "log_spectral_band_rmse_db": band_rmse,
            "log_spectral_band_mae_db": band_mae,
            "spectral_shape_cosine": cosine,
            "centroid_delta_hz": cf["centroid_hz"] - rf["centroid_hz"],
            "rolloff95_delta_hz": cf["rolloff95_hz"] - rf["rolloff95_hz"],
            "envelope_shape_mae_0_to_1": envelope_mae,
            "duration_delta_s": len(cand) / args.sample_rate - len(ref) / args.sample_rate,
        },
        "interpretation": "Multi-domain engineering metrics; not a perceptual accuracy percentage.",
    }
    text = json.dumps(result, indent=2, ensure_ascii=False)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
