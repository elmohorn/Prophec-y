#!/usr/bin/env python3
"""Jointly fit the provisional Prophet-6 cutoff/envelope mapping to Reference A.

The optimizer is deliberately small and interpretable. It does not pretend the
online recording is sample-exact: each trial is ranked with level-invariant
spectral bands, centroid/rolloff in octave units, spectral-shape cosine and a
low-weight amplitude-envelope term. Manifest rows carry trust weights so clean
monophonic factory examples dominate more ambiguous reconstructions.

Cutoff is parameterized by a slope (semitones per raw step) plus a *pivot*
expressed as a fraction of the 0..164 knob range. The intercept is calculated so
that the candidate curve intersects the old 1-semitone/raw law at the pivot:

    scale = 2 ** ((1 - slope) * (164*pivot) / 12)

This is much easier to reason about than independently searching an arbitrary
Hz intercept and slope.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from scipy.io import wavfile

from compare_audio import decode_stereo, envelope, normalize_rms, spectral_features


@dataclass(frozen=True)
class Params:
    slope: float = 1.0
    pivot: float = 0.5
    env_octaves: float = 10.5833333333
    filter_env_exp: float = 3.5
    filter_env_max_s: float = 10.0
    lp_res_scale: float = 1.0

    @property
    def filter_scale(self) -> float:
        pivot_raw = 164.0 * self.pivot
        return 2.0 ** ((1.0 - self.slope) * pivot_raw / 12.0)


@dataclass
class Case:
    program: int
    name: str
    start_s: float
    end_s: float
    sysex: Path
    events: Path
    trust: str
    weight: float
    ref: np.ndarray
    ref_spec: dict
    ref_env: np.ndarray


def read_float_wav(path: Path, target_samples: int) -> np.ndarray:
    sr, x = wavfile.read(path)
    if sr != 48000:
        raise RuntimeError(f"candidate sample rate {sr}, expected 48000")
    if x.dtype.kind not in "fc":
        # Defensive path; calibration renderer should always write IEEE float.
        x = x.astype(np.float64) / float(np.iinfo(x.dtype).max)
    else:
        x = x.astype(np.float64)
    if x.ndim == 1:
        x = np.column_stack([x, x])
    if len(x) < target_samples:
        x = np.pad(x, ((0, target_samples - len(x)), (0, 0)))
    return x[:target_samples, :2]


def octave_error(a: float, b: float) -> float:
    return abs(math.log2(max(a, 1e-9) / max(b, 1e-9)))


def compare_arrays(ref_spec: dict, ref_env: np.ndarray, cand: np.ndarray) -> dict:
    cand_n, _ = normalize_rms(cand)
    cf = spectral_features(cand_n, 48000)
    d = cf["band_db"] - ref_spec["band_db"]
    rmse = float(np.sqrt(np.mean(d * d)))
    cosine = float(np.dot(ref_spec["band_db"], cf["band_db"]) /
                   (np.linalg.norm(ref_spec["band_db"]) * np.linalg.norm(cf["band_db"]) + 1e-20))
    ce = octave_error(cf["centroid_hz"], ref_spec["centroid_hz"])
    re = octave_error(cf["rolloff95_hz"], ref_spec["rolloff95_hz"])
    cenv = envelope(cand_n, 48000)
    n = max(len(ref_env), len(cenv), 2)
    def rs(v: np.ndarray) -> np.ndarray:
        if len(v) == n: return v
        return np.interp(np.linspace(0, 1, n), np.linspace(0, 1, len(v)), v)
    emae = float(np.mean(np.abs(rs(ref_env) - rs(cenv))))
    # Ranking, not a perceptual percentage.  Spectral shape dominates; envelope
    # is deliberately lighter because event velocity/duration are inferred.
    score = rmse * 0.52 + (1.0 - cosine) * 18.0 + ce * 2.0 + re * 1.3 + emae * 5.0
    return {
        "score": score,
        "band_rmse_db": rmse,
        "spectral_cosine": cosine,
        "centroid_hz": float(cf["centroid_hz"]),
        "rolloff95_hz": float(cf["rolloff95_hz"]),
        "centroid_oct_error": ce,
        "rolloff95_oct_error": re,
        "envelope_mae": emae,
    }


def load_cases(reference: Path, manifest: Path) -> list[Case]:
    cases: list[Case] = []
    for r in csv.DictReader(manifest.open(encoding="utf-8")):
        start, end = float(r["start_s"]), float(r["end_s"])
        x = decode_stereo(reference, 48000, start, end)
        x_n, _ = normalize_rms(x)
        cases.append(Case(
            program=int(r["program"]), name=r["name"], start_s=start, end_s=end,
            sysex=Path(r["sysex"]), events=Path(r["events"]), trust=r.get("trust", ""),
            weight=float(r.get("weight") or 1.0), ref=x,
            ref_spec=spectral_features(x_n, 48000), ref_env=envelope(x_n, 48000),
        ))
    return cases


def evaluate(params: Params, cases: list[Case], renderer: Path, work: Path, tag: str) -> tuple[float, list[dict]]:
    work.mkdir(parents=True, exist_ok=True)

    def one(c: Case) -> dict:
        wav = work / f"{tag}_{c.program:03d}.wav"
        cmd = [
            str(renderer), str(c.sysex), str(c.events), str(wav),
            "--filter-scale", f"{params.filter_scale:.12g}",
            "--filter-slope", f"{params.slope:.12g}",
            "--filter-env-octaves", f"{params.env_octaves:.12g}",
            "--filter-env-exponent", f"{params.filter_env_exp:.12g}",
            "--filter-env-max-seconds", f"{params.filter_env_max_s:.12g}",
            "--lp-res-scale", f"{params.lp_res_scale:.12g}",
            "--tail-seconds", "0.12",
        ]
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        n = int(round((c.end_s - c.start_s) * 48000))
        cand = read_float_wav(wav, n)
        m = compare_arrays(c.ref_spec, c.ref_env, cand)
        try: wav.unlink()
        except OSError: pass
        return {
            "program": c.program, "name": c.name, "trust": c.trust, "weight": c.weight,
            **m,
        }

    workers = min(4, max(1, len(cases)))
    with ThreadPoolExecutor(max_workers=workers) as ex:
        rows = list(ex.map(one, cases))
    weighted = sum(r["weight"] * r["score"] for r in rows)
    wsum = sum(r["weight"] for r in rows)
    return weighted / max(wsum, 1e-12), rows


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("reference", type=Path)
    ap.add_argument("manifest", type=Path)
    ap.add_argument("outdir", type=Path)
    ap.add_argument("--renderer", type=Path, required=True)
    args = ap.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)
    work = args.outdir / "_work"
    cases = load_cases(args.reference, args.manifest)
    history: list[dict] = []
    eval_index = 0

    def run(p: Params, stage: str) -> float:
        nonlocal eval_index
        tag = f"e{eval_index:03d}"
        score, rows = evaluate(p, cases, args.renderer, work, tag)
        history.append({
            "eval": eval_index, "stage": stage, "score": score,
            "slope": p.slope, "pivot": p.pivot, "filter_scale": p.filter_scale,
            "env_octaves": p.env_octaves, "filter_env_exp": p.filter_env_exp,
            "filter_env_max_s": p.filter_env_max_s, "lp_res_scale": p.lp_res_scale,
            "cases": rows,
        })
        print(f"{eval_index:03d} {stage:12s} score={score:8.4f}  slope={p.slope:.3f} pivot={p.pivot:.3f} envOct={p.env_octaves:.2f} envExp={p.filter_env_exp:.2f} envMax={p.filter_env_max_s:.2f} res={p.lp_res_scale:.2f}", flush=True)
        eval_index += 1
        return score

    baseline = Params()
    best = baseline
    best_score = run(best, "baseline")

    # Stage 1: cutoff intercept+slope.  The pivot keeps the search physically
    # interpretable and reproduces the promising v0.6 shallow-law experiment.
    for slope in [0.30, 0.40, 0.50, 0.65]:
        for pivot in [0.40, 0.60, 0.80]:
            p = Params(slope=slope, pivot=pivot, env_octaves=best.env_octaves,
                       filter_env_exp=best.filter_env_exp, filter_env_max_s=best.filter_env_max_s,
                       lp_res_scale=best.lp_res_scale)
            s = run(p, "cutoff")
            if s < best_score:
                best, best_score = p, s

    # Stage 2: fit filter-envelope *depth* while holding the cutoff law fixed.
    anchor = best
    for octaves in [6.0, 8.5, 10.5833333333, 13.0]:
        p = Params(anchor.slope, anchor.pivot, octaves, anchor.filter_env_exp,
                   anchor.filter_env_max_s, anchor.lp_res_scale)
        s = run(p, "env-depth")
        if s < best_score: best, best_score = p, s

    # Stage 3: timing range and taper.  Only the filter envelope moves here;
    # amplifier timing stays at the v0.6 default to avoid a confound.
    anchor = best
    for max_s in [1.0, 2.5, 5.0, 10.0]:
        p = Params(anchor.slope, anchor.pivot, anchor.env_octaves,
                   anchor.filter_env_exp, max_s, anchor.lp_res_scale)
        s = run(p, "env-max")
        if s < best_score: best, best_score = p, s
    anchor = best
    for exp in [2.2, 3.0, 3.8, 4.6]:
        p = Params(anchor.slope, anchor.pivot, anchor.env_octaves,
                   exp, anchor.filter_env_max_s, anchor.lp_res_scale)
        s = run(p, "env-exp")
        if s < best_score: best, best_score = p, s

    # Stage 4: a small resonance calibration, after cutoff/envelope are fixed.
    anchor = best
    for res in [0.70, 0.90, 1.0, 1.15]:
        p = Params(anchor.slope, anchor.pivot, anchor.env_octaves,
                   anchor.filter_env_exp, anchor.filter_env_max_s, res)
        s = run(p, "lp-res")
        if s < best_score: best, best_score = p, s

    # One local refinement around the winning cutoff pair using the now-fitted
    # envelope/resonance values.
    anchor = best
    for ds in [-0.05, 0.05]:
        for dp in [-0.06, 0.06]:
            slope = min(1.2, max(0.12, anchor.slope + ds))
            pivot = min(0.95, max(0.15, anchor.pivot + dp))
            p = Params(slope, pivot, anchor.env_octaves, anchor.filter_env_exp,
                       anchor.filter_env_max_s, anchor.lp_res_scale)
            s = run(p, "refine")
            if s < best_score: best, best_score = p, s

    # Re-evaluate best once and save the per-case table separately.
    final_score, final_rows = evaluate(best, cases, args.renderer, work, "final")
    result = {
        "baseline_score": history[0]["score"],
        "best_score": final_score,
        "relative_improvement_fraction": (history[0]["score"] - final_score) / max(history[0]["score"], 1e-12),
        "best": {
            "filter_slope_semitones_per_raw": best.slope,
            "filter_pivot_fraction": best.pivot,
            "filter_scale_vs_8_1758Hz_intercept": best.filter_scale,
            "filter_base_hz": 8.1757989156 * best.filter_scale,
            "filter_env_range_octaves": best.env_octaves,
            "filter_env_curve_exponent": best.filter_env_exp,
            "filter_env_max_seconds": best.filter_env_max_s,
            "lowpass_resonance_scale": best.lp_res_scale,
        },
        "case_count": len(cases),
        "warning": "Fit is provisional: Reference A is MP3/web audio and performances are inferred, not MIDI captures.",
    }
    (args.outdir / "joint_fit_result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    with (args.outdir / "joint_fit_history.csv").open("w", newline="", encoding="utf-8") as f:
        fields=["eval","stage","score","slope","pivot","filter_scale","env_octaves","filter_env_exp","filter_env_max_s","lp_res_scale"]
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader()
        for h in history: w.writerow({k:h[k] for k in fields})
    with (args.outdir / "joint_fit_best_cases.csv").open("w", newline="", encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=list(final_rows[0].keys())); w.writeheader(); w.writerows(final_rows)
    # Full history with case-level metrics for auditability.
    (args.outdir / "joint_fit_history.json").write_text(json.dumps(history, indent=2), encoding="utf-8")
    if work.exists():
        try: work.rmdir()
        except OSError: pass
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
