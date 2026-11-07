#!/usr/bin/env python3
"""Hard-sync diagnostics for Reference-A calibration.

Compares a Prophet-6 reference segment with a candidate render without assuming
that the loudest spectral peak is the played pitch.  Hard sync is periodic at
Oscillator 2 (the master), so the script estimates the master period from
normalized autocorrelation and then reports a harmonic profile at that F0.

This is intentionally a diagnostic rather than a fitter: it exposes which
harmonics differ so a change cannot hide behind one aggregate score.
Requires numpy/scipy and ffmpeg.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
from pathlib import Path

import numpy as np
from scipy.signal import find_peaks, welch


def decode_mono(path: Path, sr: int, start: float = 0.0, duration: float | None = None) -> np.ndarray:
    cmd = ["ffmpeg", "-v", "error", "-ss", f"{start:.9f}", "-i", str(path)]
    if duration is not None:
        cmd += ["-t", f"{duration:.9f}"]
    cmd += ["-ac", "1", "-ar", str(sr), "-f", "f32le", "-acodec", "pcm_f32le", "pipe:1"]
    raw = subprocess.check_output(cmd)
    return np.frombuffer(raw, dtype="<f4").astype(np.float64)


def estimate_periodic_f0(x: np.ndarray, sr: int, fmin: float, fmax: float) -> tuple[float, float]:
    if len(x) < 512:
        raise ValueError("audio window too short")
    # Several overlapping windows are averaged so one VCO phase realization
    # cannot dominate the F0 decision.
    frame_n = min(8192, len(x))
    hop = max(1, frame_n // 4)
    acc = np.zeros(frame_n, dtype=np.float64)
    count = 0
    for start in range(0, max(1, len(x) - frame_n + 1), hop):
        f = x[start:start + frame_n]
        if len(f) < frame_n:
            break
        f = (f - np.mean(f)) * np.hanning(frame_n)
        nfft = 1
        while nfft < frame_n * 2:
            nfft *= 2
        X = np.fft.rfft(f, n=nfft)
        ac = np.fft.irfft(np.abs(X) ** 2, n=nfft)[:frame_n]
        if ac[0] > 1e-20:
            acc += ac / ac[0]
            count += 1
    if count == 0:
        raise ValueError("could not form autocorrelation windows")
    ac = acc / count
    lo = max(1, int(math.floor(sr / fmax)))
    hi = min(len(ac) - 1, int(math.ceil(sr / fmin)))
    peaks, props = find_peaks(ac[lo:hi + 1], prominence=0.015)
    if len(peaks) == 0:
        k = int(np.argmax(ac[lo:hi + 1])) + lo
    else:
        peaks = peaks + lo
        # Prefer the strongest peak. For a genuinely periodic sync waveform,
        # the master period usually beats its half-period even if the master
        # fundamental is weak in the magnitude spectrum.
        k = int(peaks[np.argmax(ac[peaks])])
    # Quadratic interpolation of the autocorrelation peak.
    delta = 0.0
    if 1 <= k < len(ac) - 1:
        y0, y1, y2 = ac[k - 1], ac[k], ac[k + 1]
        den = y0 - 2.0 * y1 + y2
        if abs(den) > 1e-12:
            delta = 0.5 * (y0 - y2) / den
            delta = float(np.clip(delta, -0.5, 0.5))
    lag = k + delta
    return sr / lag, float(ac[k])


def harmonic_profile(x: np.ndarray, sr: int, f0: float, harmonics: int, cents: float) -> list[float]:
    if len(x) < 256:
        return []
    nper = min(16384, len(x))
    freqs, psd = welch(x - np.mean(x), fs=sr, window="hann", nperseg=nper,
                       noverlap=nper // 2, detrend=False, scaling="spectrum")
    out: list[float] = []
    ratio = 2.0 ** (cents / 1200.0)
    for h in range(1, harmonics + 1):
        hz = f0 * h
        if hz >= sr * 0.48:
            break
        lo, hi = hz / ratio, hz * ratio
        mask = (freqs >= lo) & (freqs <= hi)
        if not np.any(mask):
            idx = int(np.argmin(np.abs(freqs - hz)))
            power = psd[idx]
        else:
            power = float(np.sum(psd[mask]))
        out.append(10.0 * math.log10(max(power, 1e-30)))
    if out:
        top = max(out)
        out = [v - top for v in out]
    return out


def spectral_shape_metrics(x: np.ndarray, sr: int) -> dict[str, float]:
    nper = min(16384, len(x))
    f, p = welch(x - np.mean(x), fs=sr, window="hann", nperseg=nper,
                 noverlap=nper // 2, detrend=False, scaling="spectrum")
    p = np.maximum(p, 0.0)
    total = float(np.sum(p)) + 1e-30
    centroid = float(np.sum(f * p) / total)
    c = np.cumsum(p)
    roll95 = float(f[min(len(f) - 1, int(np.searchsorted(c, 0.95 * c[-1])))])
    return {"centroid_hz": centroid, "rolloff95_hz": roll95}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("reference", type=Path)
    ap.add_argument("candidate", type=Path)
    ap.add_argument("--sample-rate", type=int, default=48000)
    ap.add_argument("--reference-start", type=float, default=0.0)
    ap.add_argument("--candidate-start", type=float, default=0.0)
    ap.add_argument("--duration", type=float, default=1.5)
    ap.add_argument("--f0", type=float, help="force common master F0 instead of estimating reference")
    ap.add_argument("--fmin", type=float, default=40.0)
    ap.add_argument("--fmax", type=float, default=600.0)
    ap.add_argument("--harmonics", type=int, default=20)
    ap.add_argument("--band-cents", type=float, default=35.0)
    ap.add_argument("--csv", type=Path)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()

    ref = decode_mono(args.reference, args.sample_rate, args.reference_start, args.duration)
    cand = decode_mono(args.candidate, args.sample_rate, args.candidate_start, args.duration)
    n = min(len(ref), len(cand))
    ref, cand = ref[:n], cand[:n]
    if n < 512:
        raise SystemExit("comparison window is too short")

    if args.f0 is None:
        f0_ref, periodicity_ref = estimate_periodic_f0(ref, args.sample_rate, args.fmin, args.fmax)
    else:
        f0_ref, periodicity_ref = args.f0, float("nan")
    f0_cand, periodicity_cand = estimate_periodic_f0(cand, args.sample_rate, args.fmin, args.fmax)

    ref_h = harmonic_profile(ref, args.sample_rate, f0_ref, args.harmonics, args.band_cents)
    cand_h = harmonic_profile(cand, args.sample_rate, f0_ref, args.harmonics, args.band_cents)
    count = min(len(ref_h), len(cand_h))
    rows = []
    diffs = []
    for i in range(count):
        d = cand_h[i] - ref_h[i]
        diffs.append(d)
        rows.append({
            "harmonic": i + 1,
            "frequency_hz": f0_ref * (i + 1),
            "reference_db_rel": ref_h[i],
            "candidate_db_rel": cand_h[i],
            "candidate_minus_reference_db": d,
        })

    report = {
        "reference": str(args.reference),
        "candidate": str(args.candidate),
        "window_duration_s": n / args.sample_rate,
        "reference_f0_hz": f0_ref,
        "reference_periodicity": periodicity_ref,
        "candidate_f0_hz": f0_cand,
        "candidate_periodicity": periodicity_cand,
        "f0_error_cents": 1200.0 * math.log2(f0_cand / f0_ref),
        "harmonic_rmse_db": float(math.sqrt(np.mean(np.square(diffs)))) if diffs else float("nan"),
        "reference_shape": spectral_shape_metrics(ref, args.sample_rate),
        "candidate_shape": spectral_shape_metrics(cand, args.sample_rate),
        "harmonics": rows,
    }

    if args.csv:
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        with args.csv.open("w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else ["harmonic"])
            w.writeheader()
            w.writerows(rows)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
