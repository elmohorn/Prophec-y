#!/usr/bin/env python3
"""Build a quantitative P0-A hardware-vs-model report from a capture manifest."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any

from reference_audio_metrics import analyze_file, compare_metrics


DEFAULT_LEVEL_REFERENCE = "MIX_OSC1_C4_RAW127"
VALID_METRICS = {"pitch", "level", "duty", "harmonics"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def finite(value: Any) -> bool:
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def summarize(values: list[float]) -> dict[str, float | int | None]:
    if not values:
        return {"count": 0, "mean_abs": None, "max_abs": None}
    absolute = [abs(v) for v in values]
    return {
        "count": len(values),
        "mean_abs": math.fsum(absolute) / len(absolute),
        "max_abs": max(absolute),
    }


def metric_policy(test: dict[str, Any]) -> set[str]:
    explicit = test.get("analysis_metrics")
    if explicit is not None:
        require(isinstance(explicit, list) and explicit,
                f"{test.get('test_id')}: analysis_metrics must be a non-empty array")
        metrics = {str(value) for value in explicit}
        unknown = metrics - VALID_METRICS
        require(not unknown,
                f"{test.get('test_id')}: unknown analysis metrics {sorted(unknown)}")
        return metrics

    test_id = str(test.get("test_id", ""))
    if test_id.startswith("OSC1_PITCH_") or test_id.startswith("OSC2_FINE_"):
        return {"pitch"}
    if test_id.startswith("OSC1_SHAPE_"):
        return {"pitch", "level", "harmonics"}
    if test_id.startswith("OSC1_PW_"):
        return {"pitch", "level", "duty", "harmonics"}
    if test_id.startswith("MIX_"):
        return {"level"}
    return VALID_METRICS.copy()


def choose_window(note_seconds: float, settle_seconds: float, requested_duration: float) -> tuple[float, float]:
    require(note_seconds > 0.0, "note_seconds must be > 0")
    settle = min(max(0.0, settle_seconds), note_seconds * 0.4)
    available = note_seconds - settle
    guard = min(0.05, available * 0.1)
    duration = min(requested_duration, available - guard)
    require(duration >= 0.05, "note is too short for a stable P0-A analysis window")
    return settle, duration


def analyze_test(
    manifest_dir: Path,
    model_dir: Path,
    acquisition: dict[str, Any],
    test: dict[str, Any],
    settle_seconds: float,
    duration_seconds: float,
) -> dict[str, Any]:
    require(test.get("family") == "P0-A", f"{test.get('test_id')}: not a P0-A test")
    test_id = str(test["test_id"])
    timing = test["timing"]
    pre_roll = float(timing["pre_roll_seconds"])
    note_seconds = float(timing["note_seconds"])
    settle, duration = choose_window(note_seconds, settle_seconds, duration_seconds)

    hardware_path = (manifest_dir / str(test["audio_file"])).resolve()
    model_path = (model_dir / f"{test_id}.wav").resolve()
    require(hardware_path.is_file(), f"missing hardware WAV: {hardware_path}")
    require(model_path.is_file(), f"missing model WAV: {model_path}")

    # Hardware WAVs retain the declared pre-roll. p6_render_reference executes
    # the same pre-roll internally but writes its WAV starting at Note On.
    hardware = analyze_file(hardware_path, pre_roll + settle, duration)
    model = analyze_file(model_path, settle, duration)

    expected_rate = int(acquisition["sample_rate_hz"])
    require(hardware["sample_rate_hz"] == expected_rate,
            f"{test_id}: hardware sample rate differs from manifest")
    require(model["sample_rate_hz"] == expected_rate,
            f"{test_id}: model sample rate differs from manifest")

    return {
        "test_id": test_id,
        "analysis_metrics": sorted(metric_policy(test)),
        "hardware": hardware,
        "model": model,
        "raw_comparison": compare_metrics(hardware, model),
    }


def build_report(
    manifest_path: Path,
    model_dir: Path,
    level_reference: str,
    settle_seconds: float,
    duration_seconds: float,
    allow_missing: bool,
) -> dict[str, Any]:
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(document.get("schema_version") == "p6-reference-capture/1",
            "unsupported manifest schema_version")
    acquisition = document.get("acquisition")
    tests = document.get("tests")
    require(isinstance(acquisition, dict), "manifest.acquisition must be an object")
    require(isinstance(tests, list) and tests, "manifest.tests must be a non-empty array")

    results: list[dict[str, Any]] = []
    missing: list[dict[str, str]] = []
    for test in tests:
        require(isinstance(test, dict), "every manifest test must be an object")
        if test.get("family") != "P0-A":
            continue
        try:
            results.append(analyze_test(
                manifest_path.parent,
                model_dir,
                acquisition,
                test,
                settle_seconds,
                duration_seconds,
            ))
        except (OSError, ValueError) as exc:
            if not allow_missing:
                raise
            missing.append({"test_id": str(test.get("test_id")), "reason": str(exc)})

    require(results, "no complete P0-A hardware/model pairs were available")

    reference = next((r for r in results if r["test_id"] == level_reference), None)
    require(reference is not None,
            f"level-reference test {level_reference!r} is not available")
    anchor_error = reference["raw_comparison"].get("rms_error_db")
    require(finite(anchor_error), "level-reference RMS is not measurable")
    anchor_error_db = float(anchor_error)

    pitch_errors: list[float] = []
    level_errors: list[float] = []
    harmonic_errors: list[float] = []
    duty_errors: list[float] = []

    for result in results:
        raw = result["raw_comparison"]
        metrics = set(result["analysis_metrics"])
        normalized_level = None
        if "level" in metrics and finite(raw.get("rms_error_db")):
            normalized_level = float(raw["rms_error_db"]) - anchor_error_db
            level_errors.append(normalized_level)

        pitch = raw.get("pitch_error_cents") if "pitch" in metrics else None
        harmonic = raw.get("mean_abs_h2_h16_error_db") if "harmonics" in metrics else None
        duty = raw.get("positive_duty_error") if "duty" in metrics else None
        if finite(pitch):
            pitch_errors.append(float(pitch))
        if finite(harmonic):
            harmonic_errors.append(float(harmonic))
        if finite(duty):
            duty_errors.append(float(duty))

        result["normalized_comparison"] = {
            "level_error_db": normalized_level,
            "pitch_error_cents": pitch,
            "mean_abs_h2_h16_error_db": harmonic,
            "positive_duty_error": duty,
            "acceptance": {
                "pitch_under_2_cents": abs(float(pitch)) < 2.0 if finite(pitch) else None,
                "level_under_1_db": abs(normalized_level) <= 1.0 if normalized_level is not None else None,
                "harmonics_under_3_db": float(harmonic) <= 3.0 if finite(harmonic) else None,
            },
        }

    return {
        "report_schema": "p6-p0a-report/1",
        "capture_set_id": document.get("capture_set_id"),
        "manifest": str(manifest_path),
        "model_dir": str(model_dir),
        "sample_rate_hz": acquisition.get("sample_rate_hz"),
        "analysis": {
            "settle_seconds": settle_seconds,
            "requested_duration_seconds": duration_seconds,
            "hardware_wav_includes_pre_roll": True,
            "model_wav_begins_at_note_on": True,
            "level_reference_test_id": level_reference,
            "raw_anchor_model_minus_hardware_db": anchor_error_db,
            "metric_policy": "explicit analysis_metrics when present; otherwise P0-A test-id policy",
        },
        "aggregate": {
            "complete_pairs": len(results),
            "missing_pairs": len(missing),
            "pitch_error_cents": summarize(pitch_errors),
            "normalized_level_error_db": summarize(level_errors),
            "per_test_h2_h16_mean_abs_error_db": summarize(harmonic_errors),
            "positive_duty_error": summarize(duty_errors),
        },
        "missing": missing,
        "tests": results,
    }


def run_self_test() -> int:
    settle, duration = choose_window(3.0, 0.5, 1.0)
    assert abs(settle - 0.5) < 1.0e-12
    assert abs(duration - 1.0) < 1.0e-12
    settle, duration = choose_window(0.2, 0.5, 1.0)
    assert 0.0 <= settle <= 0.0800000001
    assert duration >= 0.05

    values = summarize([-2.0, 1.0, 0.5])
    assert values["count"] == 3
    assert abs(float(values["mean_abs"]) - (3.5 / 3.0)) < 1.0e-12
    assert values["max_abs"] == 2.0
    assert metric_policy({"test_id": "OSC1_PW_C4_RAW127"}) == {"pitch", "level", "duty", "harmonics"}
    assert metric_policy({"test_id": "MIX_NOISE_C4_RAW064"}) == {"level"}
    assert metric_policy({"test_id": "X", "analysis_metrics": ["pitch"]}) == {"pitch"}
    print("analyze_p0a_manifest self-test OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare a materialized P0-A Prophet-6 capture set with model renders."
    )
    parser.add_argument("manifest", type=Path, nargs="?")
    parser.add_argument("--model-dir", type=Path, default=Path("model"))
    parser.add_argument("--output", type=Path, help="write JSON report instead of stdout")
    parser.add_argument("--level-reference", default=DEFAULT_LEVEL_REFERENCE)
    parser.add_argument("--settle-seconds", type=float, default=0.5)
    parser.add_argument("--duration-seconds", type=float, default=1.0)
    parser.add_argument("--allow-missing", action="store_true")
    parser.add_argument("--pretty", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()
    if args.manifest is None:
        parser.error("manifest is required unless --self-test is used")

    report = build_report(
        args.manifest.resolve(),
        args.model_dir.resolve(),
        args.level_reference,
        args.settle_seconds,
        args.duration_seconds,
        args.allow_missing,
    )
    text = json.dumps(report, indent=2 if args.pretty else None, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
        print(f"wrote {args.output}: {report['aggregate']['complete_pairs']} complete P0-A pairs")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"analyze_p0a_manifest: {exc}", file=sys.stderr)
        raise SystemExit(1)
