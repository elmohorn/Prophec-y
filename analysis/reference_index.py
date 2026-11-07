#!/usr/bin/env python3
"""Build a coarse 500-program time index for a Prophet-6 factory-demo recording.

This deliberately does *not* pretend the boundaries are ground truth.  It fits a
near-regular 500-program grid to acoustic change points, then locally refines
internal boundaries.  The resulting CSV is a starting point for matching a
hardware recording to the official factory program order.

Requires: numpy, scipy, ffmpeg (for non-WAV inputs).
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import get_window


def decode_mono_8k(path: Path) -> tuple[int, np.ndarray]:
    if path.suffix.lower() == ".wav":
        sr, x = wavfile.read(path)
        if x.ndim == 2:
            x = x.astype(np.float64).mean(axis=1)
        else:
            x = x.astype(np.float64)
        if np.issubdtype(x.dtype, np.integer):
            x /= np.iinfo(x.dtype).max
        if sr == 8000:
            return sr, x.astype(np.float64)

    with tempfile.NamedTemporaryFile(suffix=".wav") as tmp:
        cmd = [
            "ffmpeg", "-v", "error", "-y", "-i", str(path),
            "-ac", "1", "-ar", "8000", "-c:a", "pcm_s16le", tmp.name,
        ]
        subprocess.run(cmd, check=True)
        sr, x = wavfile.read(tmp.name)
        return sr, x.astype(np.float64) / 32768.0


def robust_z(x: np.ndarray) -> np.ndarray:
    med = np.median(x)
    mad = np.median(np.abs(x - med)) * 1.4826
    if mad < 1e-12:
        mad = np.std(x) + 1e-12
    return (x - med) / mad


def acoustic_novelty(x: np.ndarray, sr: int, hop_s: float = 0.05) -> tuple[np.ndarray, np.ndarray]:
    hop = max(1, int(round(hop_s * sr)))
    win = 1024
    window = get_window("hann", win, fftbins=True)
    n = 1 + max(0, (len(x) - win) // hop)
    if n < 20:
        raise ValueError("recording too short")

    freqs = np.fft.rfftfreq(win, 1.0 / sr)
    edges = np.geomspace(45.0, min(3800.0, sr * 0.47), 25)
    bands = []
    rms = np.empty(n, dtype=np.float64)
    feat = np.empty((n, len(edges) - 1), dtype=np.float64)

    for b in range(len(edges) - 1):
        idx = np.flatnonzero((freqs >= edges[b]) & (freqs < edges[b + 1]))
        bands.append(idx if len(idx) else np.array([np.argmin(np.abs(freqs - (edges[b] + edges[b + 1]) / 2))]))

    for i in range(n):
        frame = x[i * hop:i * hop + win]
        rms[i] = math.sqrt(float(np.mean(frame * frame)) + 1e-15)
        mag = np.abs(np.fft.rfft(frame * window)) ** 2
        for b, idx in enumerate(bands):
            feat[i, b] = math.log(float(np.mean(mag[idx])) + 1e-12)

    # Compare spectral means across a 0.30 s window on each side.
    half = max(2, int(round(0.30 / hop_s)))
    novelty = np.zeros(n, dtype=np.float64)
    for i in range(half, n - half):
        left = feat[i - half:i].mean(axis=0)
        right = feat[i:i + half].mean(axis=0)
        novelty[i] = np.linalg.norm(right - left) / math.sqrt(feat.shape[1])

    log_rms = np.log(rms + 1e-10)
    level_jump = np.zeros_like(novelty)
    level_jump[1:] = np.abs(np.diff(log_rms))

    # A local level valley is useful when the player pauses between presets,
    # but not required because many changes are direct cuts.
    valley = np.zeros_like(novelty)
    w = max(2, int(round(0.25 / hop_s)))
    for i in range(w, n - w):
        neighbourhood = np.r_[log_rms[i - w:i], log_rms[i + 1:i + w + 1]]
        valley[i] = max(0.0, float(np.median(neighbourhood) - log_rms[i]))

    score = robust_z(novelty) + 0.35 * robust_z(level_jump) + 0.20 * np.maximum(0.0, robust_z(valley))
    # Limit extreme single-frame codec/click spikes so they cannot dominate the grid fit.
    score = np.clip(score, -4.0, 10.0)
    times = (np.arange(n) * hop + win / 2) / sr
    return times, score


def interp_score(times: np.ndarray, score: np.ndarray, t: np.ndarray) -> np.ndarray:
    return np.interp(t, times, score, left=-10.0, right=-10.0)


def estimate_activity_bounds(x: np.ndarray, sr: int) -> tuple[float, float]:
    """Estimate where the sustained demo content begins/ends.

    Uses 200 ms RMS and a conservative floor. This is only an anchor for the
    near-regular grid; local acoustic refinement still determines transitions.
    """
    hop = max(1, int(round(0.20 * sr)))
    n = len(x) // hop
    if n < 2:
        return 0.0, len(x) / sr
    r = np.sqrt(np.mean(x[:n * hop].reshape(n, hop) ** 2, axis=1) + 1e-20)
    p95 = float(np.percentile(r, 95))
    threshold = max(10 ** (-45.0 / 20.0), p95 * 0.01)
    active = np.flatnonzero(r >= threshold)
    if len(active) == 0:
        return 0.0, len(x) / sr
    return float(active[0] * hop / sr), float((active[-1] + 1) * hop / sr)


def fit_grid(times: np.ndarray, score: np.ndarray, count: int,
             start_min: float, start_max: float,
             period_min: float, period_max: float,
             activity_start: float, activity_end: float) -> tuple[float, float, float]:
    best = (-1e99, 0.0, 0.0)
    # Coarse-to-fine search keeps the script fast on a 37-minute file.
    for step_p, step_s in [(0.004, 0.08), (0.0005, 0.01)]:
        if best[0] < -1e90:
            p_lo, p_hi = period_min, period_max
            s_lo, s_hi = start_min, start_max
        else:
            _, bs, bp = best
            p_lo, p_hi = max(period_min, bp - 0.012), min(period_max, bp + 0.012)
            s_lo, s_hi = max(start_min, bs - 0.24), min(start_max, bs + 0.24)
        ps = np.arange(p_lo, p_hi + step_p * 0.5, step_p)
        ss = np.arange(s_lo, s_hi + step_s * 0.5, step_s)
        for p in ps:
            k = np.arange(1, count, dtype=np.float64)
            for s in ss:
                b = s + k * p
                if b[-1] >= times[-1]:
                    continue
                vals = interp_score(times, score, b)
                # Trimmed mean is robust to presets whose transition is smooth.
                qlo, qhi = np.quantile(vals, [0.05, 0.98])
                clipped = np.clip(vals, qlo, qhi)
                objective = float(clipped.mean())
                # Keep the 500-segment grid tied to the sustained content.
                # Without this term a shorter false period can score well by
                # repeatedly landing on within-preset musical changes.
                predicted_end = s + count * p
                objective -= 0.20 * ((s - activity_start) / 1.5) ** 2
                objective -= 0.35 * ((predicted_end - activity_end) / 2.5) ** 2
                if objective > best[0]:
                    best = (objective, float(s), float(p))
    if best[0] < -1e90:
        raise RuntimeError("failed to fit program grid")
    return best[1], best[2], best[0]


def refine_boundaries(times: np.ndarray, score: np.ndarray, start: float, period: float,
                      count: int, radius: float = 0.65) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    boundaries = np.empty(count + 1, dtype=np.float64)
    confidence = np.ones(count + 1, dtype=np.float64)
    shift = np.zeros(count + 1, dtype=np.float64)
    boundaries[0] = start
    boundaries[-1] = start + count * period

    sigma = radius * 0.55
    for k in range(1, count):
        expected = start + k * period
        mask = (times >= expected - radius) & (times <= expected + radius)
        idx = np.flatnonzero(mask)
        if len(idx) == 0:
            boundaries[k] = expected
            confidence[k] = 0.0
            continue
        local_t = times[idx]
        distance_weight = np.exp(-0.5 * ((local_t - expected) / sigma) ** 2)
        objective = score[idx] + 1.25 * distance_weight
        j = idx[int(np.argmax(objective))]
        boundaries[k] = times[j]
        shift[k] = boundaries[k] - expected

        local = score[idx]
        med = float(np.median(local))
        mad = float(np.median(np.abs(local - med)) * 1.4826 + 1e-9)
        z = (float(score[j]) - med) / mad
        confidence[k] = 1.0 / (1.0 + math.exp(-0.9 * (z - 1.0)))

    # Guard against pathological local maxima. With the fitted period and
    # radius these should already be ordered, but enforce a small minimum gap.
    min_gap = max(0.25, period * 0.45)
    for k in range(1, count):
        boundaries[k] = max(boundaries[k], boundaries[k - 1] + min_gap)
    for k in range(count - 1, 0, -1):
        boundaries[k] = min(boundaries[k], boundaries[k + 1] - min_gap)
    return boundaries, confidence, shift


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("audio", type=Path)
    ap.add_argument("--count", type=int, default=500)
    ap.add_argument("--names", type=Path, default=Path("data/factory_program_names_2015.txt"))
    ap.add_argument("--out-csv", type=Path, default=Path("reference_segments_coarse.csv"))
    ap.add_argument("--out-json", type=Path, default=Path("reference_segments_coarse.json"))
    ap.add_argument("--start-min", type=float)
    ap.add_argument("--start-max", type=float)
    ap.add_argument("--period-min", type=float)
    ap.add_argument("--period-max", type=float)
    args = ap.parse_args()

    names = [line.rstrip("\n") for line in args.names.read_text(encoding="utf-8").splitlines()]
    if len(names) != args.count:
        raise SystemExit(f"name list has {len(names)} entries, expected {args.count}")

    sr, x = decode_mono_8k(args.audio)
    activity_start, activity_end = estimate_activity_bounds(x, sr)
    nominal_period = (activity_end - activity_start) / args.count
    start_min = args.start_min if args.start_min is not None else max(0.0, activity_start - 1.5)
    start_max = args.start_max if args.start_max is not None else activity_start + 1.5
    period_min = args.period_min if args.period_min is not None else nominal_period - 0.08
    period_max = args.period_max if args.period_max is not None else nominal_period + 0.08

    times, score = acoustic_novelty(x, sr)
    start, period, grid_score = fit_grid(times, score, args.count,
                                         start_min, start_max, period_min, period_max,
                                         activity_start, activity_end)
    boundaries, confidence, shifts = refine_boundaries(times, score, start, period, args.count)

    args.out_csv.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    with args.out_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["program", "name", "start_s", "end_s", "duration_s", "boundary_confidence", "boundary_shift_s"])
        for i in range(args.count):
            conf = float(confidence[i]) if i > 0 else 1.0
            row = [i, names[i], float(boundaries[i]), float(boundaries[i + 1]),
                   float(boundaries[i + 1] - boundaries[i]), conf, float(shifts[i])]
            rows.append(row)
            w.writerow([i, names[i], f"{boundaries[i]:.6f}", f"{boundaries[i+1]:.6f}",
                        f"{boundaries[i+1]-boundaries[i]:.6f}", f"{conf:.4f}", f"{shifts[i]:+.6f}"])

    meta = {
        "source": str(args.audio),
        "analysis_sample_rate": sr,
        "program_count": args.count,
        "method": "near-regular grid fit to spectral/level novelty + local boundary refinement",
        "status": "coarse_alignment_not_ground_truth",
        "estimated_activity_start_s": activity_start,
        "estimated_activity_end_s": activity_end,
        "grid_start_s": start,
        "grid_period_s": period,
        "grid_objective": grid_score,
        "indexed_start_s": float(boundaries[0]),
        "indexed_end_s": float(boundaries[-1]),
        "mean_program_duration_s": float(np.mean(np.diff(boundaries))),
        "median_internal_boundary_confidence": float(np.median(confidence[1:-1])),
        "mean_abs_boundary_refinement_s": float(np.mean(np.abs(shifts[1:-1]))),
        "codec_note": "Use for relative timbre/timing comparisons; lossy online audio is not an absolute ultrasonic/amplitude reference.",
        "segments_csv": str(args.out_csv),
    }
    args.out_json.write_text(json.dumps(meta, indent=2, ensure_ascii=False), encoding="utf-8")

    print(json.dumps(meta, indent=2, ensure_ascii=False))
    print("first 5:")
    for r in rows[:5]:
        print(f"  {r[0]:03d} {r[1]:24s} {r[2]:8.3f} -> {r[3]:8.3f}  conf={r[5]:.2f}")
    print("last 3:")
    for r in rows[-3:]:
        print(f"  {r[0]:03d} {r[1]:24s} {r[2]:8.3f} -> {r[3]:8.3f}  conf={r[5]:.2f}")


if __name__ == "__main__":
    main()
