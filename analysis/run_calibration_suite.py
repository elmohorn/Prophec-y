#!/usr/bin/env python3
"""Render and compare a trusted Prophet-6 reference suite.

The manifest deliberately separates *performance reconstruction* from DSP
calibration. Each row points at a program SysEx and an event CSV that has
already been inferred/reviewed. The tool renders float WAVs with the calibration
renderer, compares them to the indexed hardware recording, and emits one compact
CSV plus a JSON report per case.

This is intended for relative A/B work against lossy online reference audio;
the aggregate score is an engineering ranking only, never a percentage of
"Prophet accuracy".
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
from pathlib import Path


def calibration_args(args: argparse.Namespace) -> list[str]:
    out: list[str] = []
    if args.filter_scale is not None:
        out += ["--filter-scale", str(args.filter_scale)]
    if args.filter_slope is not None:
        out += ["--filter-slope", str(args.filter_slope)]
    if args.lp_res_scale is not None:
        out += ["--lp-res-scale", str(args.lp_res_scale)]
    if args.filter_env_octaves is not None:
        out += ["--filter-env-octaves", str(args.filter_env_octaves)]
    if args.env_exponent is not None:
        out += ["--env-exponent", str(args.env_exponent)]
    if args.env_max_seconds is not None:
        out += ["--env-max-seconds", str(args.env_max_seconds)]
    if args.vintage:
        out += ["--vintage"]
    return out


def engineering_score(diff: dict) -> float:
    # Ratios are expressed in octaves so that a 2x error at 200 vs 400 Hz and
    # at 2 vs 4 kHz has the same weight. The band RMSE contributes gently.
    # Envelope shape is useful but performance timing/velocity is inferred, so
    # keep it lower weight than spectral shape.
    centroid_st = abs(12.0 * math.log2(max(1e-9, 1.0 + diff["centroid_delta_hz"] / max(1e-9, diff.get("reference_centroid_hz", 1.0))))) if "reference_centroid_hz" in diff else 0.0
    return (
        diff["log_spectral_band_rmse_db"] * 0.45
        + abs(diff["envelope_shape_mae_0_to_1"]) * 8.0
        + abs(diff["duration_delta_s"]) * 0.5
        + centroid_st * 0.15
    )


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("reference", type=Path)
    ap.add_argument("manifest", type=Path)
    ap.add_argument("outdir", type=Path)
    ap.add_argument("--renderer", type=Path, required=True)
    ap.add_argument("--filter-scale", type=float)
    ap.add_argument("--filter-slope", type=float)
    ap.add_argument("--lp-res-scale", type=float)
    ap.add_argument("--filter-env-octaves", type=float)
    ap.add_argument("--env-exponent", type=float)
    ap.add_argument("--env-max-seconds", type=float)
    ap.add_argument("--vintage", action="store_true")
    ap.add_argument("--tail-seconds", type=float, default=0.25)
    args = ap.parse_args()

    args.outdir.mkdir(parents=True, exist_ok=True)
    compare = Path(__file__).with_name("compare_audio.py")
    rows = list(csv.DictReader(args.manifest.open(encoding="utf-8")))
    summary: list[dict] = []
    cal = calibration_args(args)

    for row in rows:
        program = int(row["program"])
        name = row["name"]
        syx = Path(row["sysex"])
        events = Path(row["events"])
        start = float(row["start_s"])
        end = float(row["end_s"])
        duration = end - start
        stem = f"{program:03d}_{name.replace(' ', '_').replace('/', '_')}"
        wav = args.outdir / f"{stem}.wav"
        report = args.outdir / f"{stem}.json"

        cmd = [
            str(args.renderer), str(syx), str(events), str(wav),
            "--tail-seconds", str(args.tail_seconds), *cal,
        ]
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
        cmp_cmd = [
            "python3", str(compare), str(args.reference), str(wav),
            "--ref-start", str(start), "--ref-end", str(end),
            "--candidate-start", "0", "--candidate-end", str(duration),
            "--no-trim", "--out", str(report),
        ]
        subprocess.run(cmp_cmd, check=True, stdout=subprocess.DEVNULL)
        r = json.loads(report.read_text(encoding="utf-8"))
        rd, cd, d = r["reference_descriptors"], r["candidate_descriptors"], r["differences"]
        # Add reference centroid only for the aggregate helper above.
        score_diff = dict(d)
        score_diff["reference_centroid_hz"] = rd["centroid_hz"]
        summary.append({
            "program": program,
            "name": name,
            "trust": row.get("trust", ""),
            "reference_centroid_hz": rd["centroid_hz"],
            "candidate_centroid_hz": cd["centroid_hz"],
            "centroid_delta_hz": d["centroid_delta_hz"],
            "reference_rolloff95_hz": rd["rolloff95_hz"],
            "candidate_rolloff95_hz": cd["rolloff95_hz"],
            "rolloff95_delta_hz": d["rolloff95_delta_hz"],
            "band_rmse_db": d["log_spectral_band_rmse_db"],
            "spectral_cosine": d["spectral_shape_cosine"],
            "envelope_mae": d["envelope_shape_mae_0_to_1"],
            "case_score": engineering_score(score_diff),
        })

    out_csv = args.outdir / "suite_summary.csv"
    with out_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(summary[0].keys()))
        w.writeheader(); w.writerows(summary)

    aggregate = {
        "case_count": len(summary),
        "mean_band_rmse_db": sum(x["band_rmse_db"] for x in summary) / len(summary),
        "mean_spectral_cosine": sum(x["spectral_cosine"] for x in summary) / len(summary),
        "mean_envelope_mae": sum(x["envelope_mae"] for x in summary) / len(summary),
        "mean_case_score": sum(x["case_score"] for x in summary) / len(summary),
        "calibration": {
            "filter_scale": args.filter_scale,
            "filter_slope": args.filter_slope,
            "lp_res_scale": args.lp_res_scale,
            "filter_env_octaves": args.filter_env_octaves,
            "env_exponent": args.env_exponent,
            "env_max_seconds": args.env_max_seconds,
            "vintage": args.vintage,
        },
        "warning": "Aggregate is for A/B ranking only; performance and web-audio capture are not sample-exact.",
    }
    (args.outdir / "suite_aggregate.json").write_text(json.dumps(aggregate, indent=2), encoding="utf-8")
    print(json.dumps(aggregate, indent=2))


if __name__ == "__main__":
    main()
