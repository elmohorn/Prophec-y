#!/usr/bin/env python3
"""Validate Prophec-y controlled Prophet-6 capture manifests.

This intentionally uses only the Python standard library so a capture set can
be checked on a clean workstation. It validates the contract that matters most
for v0.19: stable IDs, safe relative paths, paired SysEx/WAV files, and WAV
metadata matching the declared acquisition settings.

It is not a general JSON-Schema engine. The formal schema lives beside the
capture corpus in reference_hardware/manifest.schema.json.
"""

from __future__ import annotations

import argparse
import json
import sys
import wave
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "p6-reference-capture/1"
CALIBRATION_SAMPLE_RATES = {48_000, 96_000}


def contains_template_placeholder(value: Any) -> bool:
    return isinstance(value, str) and "RECORD_" in value.upper()


class Report:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)


def require_dict(value: Any, label: str, report: Report) -> dict[str, Any]:
    if not isinstance(value, dict):
        report.error(f"{label} must be an object")
        return {}
    return value


def require_list(value: Any, label: str, report: Report) -> list[Any]:
    if not isinstance(value, list):
        report.error(f"{label} must be an array")
        return []
    return value


def require_nonempty_string(value: Any, label: str, report: Report) -> str:
    if not isinstance(value, str) or not value.strip():
        report.error(f"{label} must be a non-empty string")
        return ""
    return value


def safe_child(base: Path, relative: str, label: str, report: Report) -> Path | None:
    if not relative:
        return None
    path = Path(relative)
    if path.is_absolute():
        report.error(f"{label} must be relative to the manifest directory: {relative}")
        return None

    resolved_base = base.resolve()
    resolved = (base / path).resolve()
    try:
        resolved.relative_to(resolved_base)
    except ValueError:
        report.error(f"{label} escapes the capture-set directory: {relative}")
        return None
    return resolved


def validate_sysex(path: Path, label: str, report: Report) -> None:
    try:
        data = path.read_bytes()
    except OSError as exc:
        report.error(f"{label}: cannot read SysEx file {path}: {exc}")
        return

    if len(data) < 2:
        report.error(f"{label}: SysEx file is too short: {path}")
        return
    if data[0] != 0xF0 or data[-1] != 0xF7:
        report.error(f"{label}: SysEx must start with F0 and end with F7: {path}")


def validate_wav(
    path: Path,
    label: str,
    acquisition: dict[str, Any],
    report: Report,
) -> None:
    try:
        with wave.open(str(path), "rb") as wav:
            channels = wav.getnchannels()
            sample_rate = wav.getframerate()
            sample_width_bits = wav.getsampwidth() * 8
            compression = wav.getcomptype()
    except (OSError, wave.Error) as exc:
        report.error(f"{label}: cannot read PCM WAV {path}: {exc}")
        return

    expected_channels = acquisition.get("channels")
    expected_rate = acquisition.get("sample_rate_hz")
    expected_bits = acquisition.get("bit_depth")

    if isinstance(expected_channels, int) and channels != expected_channels:
        report.error(
            f"{label}: WAV channels={channels}, manifest channels={expected_channels}: {path}"
        )
    if isinstance(expected_rate, int) and sample_rate != expected_rate:
        report.error(
            f"{label}: WAV sample_rate_hz={sample_rate}, "
            f"manifest sample_rate_hz={expected_rate}: {path}"
        )
    if isinstance(expected_bits, int) and sample_width_bits != expected_bits:
        report.error(
            f"{label}: WAV bit_depth={sample_width_bits}, "
            f"manifest bit_depth={expected_bits}: {path}"
        )
    if compression != "NONE":
        report.error(f"{label}: WAV must be uncompressed PCM, got {compression}: {path}")


def validate_midi(midi: dict[str, Any], label: str, report: Report) -> None:
    notes = require_list(midi.get("notes"), f"{label}.notes", report)
    velocities = require_list(midi.get("velocities"), f"{label}.velocities", report)

    if not notes:
        report.error(f"{label}.notes must contain at least one MIDI note")
    if not velocities:
        report.error(f"{label}.velocities must contain at least one velocity")
    if len(velocities) not in (1, len(notes)):
        report.error(
            f"{label}.velocities must contain one shared velocity or one per note"
        )

    for index, note in enumerate(notes):
        if not isinstance(note, int) or not 0 <= note <= 127:
            report.error(f"{label}.notes[{index}] must be an integer 0..127")
    for index, velocity in enumerate(velocities):
        if not isinstance(velocity, int) or not 1 <= velocity <= 127:
            report.error(f"{label}.velocities[{index}] must be an integer 1..127")

    bounded = {
        "channel_pressure": (0, 127),
        "mod_wheel": (0, 127),
        "pitch_bend_14bit": (0, 16383),
    }
    for key, (low, high) in bounded.items():
        if key in midi:
            value = midi[key]
            if not isinstance(value, int) or not low <= value <= high:
                report.error(f"{label}.{key} must be an integer {low}..{high}")


def validate_timing(timing: dict[str, Any], label: str, report: Report) -> None:
    for key in ("pre_roll_seconds", "note_seconds", "release_seconds"):
        value = timing.get(key)
        if not isinstance(value, (int, float)):
            report.error(f"{label}.{key} must be numeric")
            continue
        if value < 0:
            report.error(f"{label}.{key} must be >= 0")
    note_seconds = timing.get("note_seconds")
    if isinstance(note_seconds, (int, float)) and note_seconds <= 0:
        report.error(f"{label}.note_seconds must be > 0")


def validate_manifest(path: Path, metadata_only: bool) -> Report:
    report = Report()

    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        report.error(f"cannot read manifest {path}: {exc}")
        return report

    root = require_dict(document, "manifest", report)
    if root.get("schema_version") != SCHEMA_VERSION:
        report.error(
            f"schema_version must be {SCHEMA_VERSION!r}, got {root.get('schema_version')!r}"
        )

    require_nonempty_string(root.get("capture_set_id"), "capture_set_id", report)

    hardware = require_dict(root.get("hardware"), "hardware", report)
    if hardware.get("model") != "Sequential Prophet-6":
        report.error("hardware.model must be exactly 'Sequential Prophet-6'")
    require_nonempty_string(hardware.get("unit_id"), "hardware.unit_id", report)
    firmware = require_nonempty_string(hardware.get("firmware"), "hardware.firmware", report)
    if firmware.upper().startswith("RECORD_"):
        report.warn("hardware.firmware still contains a template placeholder")

    acquisition = require_dict(root.get("acquisition"), "acquisition", report)
    sample_rate = acquisition.get("sample_rate_hz")
    bit_depth = acquisition.get("bit_depth")
    channels = acquisition.get("channels")
    if not isinstance(sample_rate, int) or sample_rate < 8_000:
        report.error("acquisition.sample_rate_hz must be an integer >= 8000")
    if not isinstance(bit_depth, int) or bit_depth < 8:
        report.error("acquisition.bit_depth must be an integer >= 8")
    if not isinstance(channels, int) or channels < 1:
        report.error("acquisition.channels must be an integer >= 1")
    if acquisition.get("format") != "WAV_PCM":
        report.error("acquisition.format must be 'WAV_PCM'")
    require_nonempty_string(
        acquisition.get("interface_chain"), "acquisition.interface_chain", report
    )
    require_nonempty_string(
        acquisition.get("fixed_gain_reference"),
        "acquisition.fixed_gain_reference",
        report,
    )
    for key in ("interface_chain", "fixed_gain_reference", "output_route"):
        if contains_template_placeholder(acquisition.get(key)):
            report.warn(f"acquisition.{key} still contains a template placeholder")
    globals_map = hardware.get("globals")
    if isinstance(globals_map, dict):
        for key, value in globals_map.items():
            if contains_template_placeholder(value):
                report.warn(f"hardware.globals.{key} still contains a template placeholder")

    tests = require_list(root.get("tests"), "tests", report)
    if not tests:
        report.error("tests must contain at least one capture")

    seen_capture_ids: set[str] = set()
    base = path.parent

    for index, raw_test in enumerate(tests):
        label = f"tests[{index}]"
        test = require_dict(raw_test, label, report)
        capture_id = require_nonempty_string(
            test.get("capture_id"), f"{label}.capture_id", report
        )
        require_nonempty_string(test.get("test_id"), f"{label}.test_id", report)
        family = test.get("family")
        if family not in {"P0-A", "P0-B", "P0-C", "P0-D", "P0-E", "P0-F", "P0-G"}:
            report.error(f"{label}.family must be P0-A .. P0-G")
        grade = test.get("capture_grade")
        if grade not in {"calibration", "diagnostic"}:
            report.error(f"{label}.capture_grade must be 'calibration' or 'diagnostic'")

        if capture_id:
            if capture_id in seen_capture_ids:
                report.error(f"duplicate capture_id: {capture_id}")
            seen_capture_ids.add(capture_id)

        midi = require_dict(test.get("midi"), f"{label}.midi", report)
        validate_midi(midi, f"{label}.midi", report)
        timing = require_dict(test.get("timing"), f"{label}.timing", report)
        validate_timing(timing, f"{label}.timing", report)

        params = test.get("raw_parameters")
        if params is not None:
            params = require_dict(params, f"{label}.raw_parameters", report)
            for nrpn, raw in params.items():
                if not isinstance(nrpn, str) or not nrpn.isdigit():
                    report.error(
                        f"{label}.raw_parameters keys must be decimal NRPN strings"
                    )
                if not isinstance(raw, int) or not 0 <= raw <= 16383:
                    report.error(
                        f"{label}.raw_parameters[{nrpn!r}] must be integer 0..16383"
                    )

        sysex_rel = require_nonempty_string(
            test.get("sysex_file"), f"{label}.sysex_file", report
        )
        audio_rel = require_nonempty_string(
            test.get("audio_file"), f"{label}.audio_file", report
        )
        sysex_path = safe_child(base, sysex_rel, f"{label}.sysex_file", report)
        audio_path = safe_child(base, audio_rel, f"{label}.audio_file", report)

        if grade == "calibration":
            if sample_rate not in CALIBRATION_SAMPLE_RATES:
                report.error(
                    f"{label}: calibration-grade capture must be 48 or 96 kHz"
                )
            if not isinstance(bit_depth, int) or bit_depth < 24:
                report.error(f"{label}: calibration-grade capture must be >=24-bit")
            if channels != 2:
                report.error(f"{label}: calibration-grade capture must be stereo")

        if metadata_only:
            continue

        if sysex_path is not None:
            if not sysex_path.is_file():
                report.error(f"{label}: missing SysEx file: {sysex_path}")
            else:
                validate_sysex(sysex_path, label, report)

        if audio_path is not None:
            if not audio_path.is_file():
                report.error(f"{label}: missing WAV file: {audio_path}")
            else:
                validate_wav(audio_path, label, acquisition, report)

    return report


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate a Prophec-y controlled hardware capture manifest."
    )
    parser.add_argument("manifest", type=Path, help="Path to manifest.json")
    parser.add_argument(
        "--metadata-only",
        action="store_true",
        help="Validate metadata without requiring the referenced WAV/SysEx files.",
    )
    args = parser.parse_args()

    report = validate_manifest(args.manifest, args.metadata_only)

    for warning in report.warnings:
        print(f"warning: {warning}", file=sys.stderr)
    for error in report.errors:
        print(f"error: {error}", file=sys.stderr)

    if report.errors:
        print(
            f"FAILED: {len(report.errors)} error(s), "
            f"{len(report.warnings)} warning(s)",
            file=sys.stderr,
        )
        return 1

    print(
        f"OK: {args.manifest} "
        f"({len(report.warnings)} warning(s), metadata_only={args.metadata_only})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
