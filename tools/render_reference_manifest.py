#!/usr/bin/env python3
"""Render every P0-A manifest condition with p6_render_reference.

The manifest is the authority for note, velocity, timing and sample rate. This
script deliberately supports P0-A first; later families can add their own
family-specific rendering policies instead of silently inheriting the wrong
isolation mode.
"""

from __future__ import annotations

import argparse
import json
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "p6-reference-capture/1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def build_command(
    renderer: Path,
    manifest_dir: Path,
    output_dir: Path,
    acquisition: dict[str, Any],
    test: dict[str, Any],
    oversampling: int,
    calibration: str | None,
) -> list[str]:
    require(test.get("family") == "P0-A", "only P0-A manifests are supported")

    midi = test.get("midi")
    timing = test.get("timing")
    require(isinstance(midi, dict), "test.midi must be an object")
    require(isinstance(timing, dict), "test.timing must be an object")

    notes = midi.get("notes")
    velocities = midi.get("velocities")
    require(isinstance(notes, list) and len(notes) == 1,
            "P0-A renderer requires exactly one MIDI note per test")
    require(isinstance(velocities, list) and len(velocities) == 1,
            "P0-A renderer requires exactly one velocity per test")

    note = int(notes[0])
    velocity_raw = int(velocities[0])
    require(0 <= note <= 127, "MIDI note must be 0..127")
    require(1 <= velocity_raw <= 127, "MIDI velocity must be 1..127")

    sample_rate = int(acquisition.get("sample_rate_hz", 0))
    require(sample_rate in (48000, 96000), "P0-A sample rate must be 48000 or 96000")

    sysex_rel = test.get("sysex_file")
    test_id = test.get("test_id")
    require(isinstance(sysex_rel, str) and sysex_rel, "sysex_file is required")
    require(isinstance(test_id, str) and test_id, "test_id is required")

    sysex_path = (manifest_dir / sysex_rel).resolve()
    output_path = (output_dir / f"{test_id}.wav").resolve()

    pre_roll = float(timing.get("pre_roll_seconds", 0.0))
    note_seconds = float(timing.get("note_seconds", 0.0))
    release_seconds = float(timing.get("release_seconds", 0.0))
    require(pre_roll >= 0.0, "pre_roll_seconds must be >= 0")
    require(note_seconds > 0.0, "note_seconds must be > 0")
    require(release_seconds >= 0.0, "release_seconds must be >= 0")

    command = [
        str(renderer),
        str(sysex_path),
        str(output_path),
        "--note", str(note),
        "--velocity", f"{velocity_raw / 127.0:.12g}",
        "--sample-rate", str(sample_rate),
        "--oversampling", str(oversampling),
        "--pre-roll-seconds", f"{pre_roll:.12g}",
        "--note-seconds", f"{note_seconds:.12g}",
        "--release-seconds", f"{release_seconds:.12g}",
        "--dry-isolation",
    ]
    if calibration:
        command.append(calibration)
    return command


def load_manifest(path: Path) -> dict[str, Any]:
    document = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(document, dict), "manifest root must be an object")
    require(document.get("schema_version") == SCHEMA_VERSION,
            f"schema_version must be {SCHEMA_VERSION}")
    require(isinstance(document.get("acquisition"), dict),
            "manifest.acquisition must be an object")
    require(isinstance(document.get("tests"), list) and document["tests"],
            "manifest.tests must be a non-empty array")
    return document


def resolve_renderer(path: Path) -> Path:
    if path.is_file():
        return path.resolve()
    found = shutil.which(str(path))
    if found:
        return Path(found).resolve()
    raise ValueError(f"renderer not found as file or on PATH: {path}")


def run_self_test() -> int:
    acquisition = {"sample_rate_hz": 96000}
    test = {
        "test_id": "OSC1_PW_C4_RAW127",
        "family": "P0-A",
        "sysex_file": "sysex/OSC1_PW_C4_RAW127.syx",
        "midi": {"notes": [60], "velocities": [127]},
        "timing": {
            "pre_roll_seconds": 2.0,
            "note_seconds": 3.0,
            "release_seconds": 2.0,
        },
    }
    command = build_command(
        Path("p6_render_reference"),
        Path("capture"),
        Path("model"),
        acquisition,
        test,
        4,
        "--reference-a-v09",
    )
    joined = " ".join(command)
    assert "--note 60" in joined
    assert "--velocity 1" in joined
    assert "--sample-rate 96000" in joined
    assert "--oversampling 4" in joined
    assert "--dry-isolation" in command
    assert command[-1] == "--reference-a-v09"
    assert command[1].endswith("capture/sysex/OSC1_PW_C4_RAW127.syx")
    assert command[2].endswith("model/OSC1_PW_C4_RAW127.wav")

    rejected = dict(test)
    rejected["family"] = "P0-B"
    try:
        build_command(Path("r"), Path("c"), Path("o"), acquisition, rejected, 2, None)
    except ValueError:
        pass
    else:
        raise AssertionError("non-P0-A family was not rejected")

    print("render_reference_manifest self-test OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Batch-render a v0.19 P0-A capture manifest with p6_render_reference."
    )
    parser.add_argument("manifest", type=Path, nargs="?", help="capture-set manifest.json")
    parser.add_argument("--renderer", type=Path, default=Path("p6_render_reference"))
    parser.add_argument("--output-dir", type=Path, default=Path("model"))
    parser.add_argument("--oversampling", type=int, choices=(1, 2, 4, 8), default=2)
    parser.add_argument(
        "--calibration",
        choices=("default", "v07", "v08", "v09"),
        default="default",
        help="Mapper candidate used by the software render.",
    )
    parser.add_argument("--dry-run", action="store_true", help="print commands without executing")
    parser.add_argument("--overwrite", action="store_true", help="replace existing model WAVs")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()
    if args.manifest is None:
        parser.error("manifest is required unless --self-test is used")

    manifest_path = args.manifest.resolve()
    document = load_manifest(manifest_path)
    manifest_dir = manifest_path.parent
    output_dir = args.output_dir.resolve()

    calibration_flags = {
        "default": None,
        "v07": "--reference-a-v07",
        "v08": "--reference-a-v08",
        "v09": "--reference-a-v09",
    }
    calibration = calibration_flags[args.calibration]

    renderer = args.renderer.resolve() if args.dry_run else resolve_renderer(args.renderer)
    if not args.dry_run:
        output_dir.mkdir(parents=True, exist_ok=True)

    rendered = 0
    skipped = 0
    tests = document["tests"]
    for index, test in enumerate(tests):
        require(isinstance(test, dict), f"tests[{index}] must be an object")
        command = build_command(
            renderer,
            manifest_dir,
            output_dir,
            document["acquisition"],
            test,
            args.oversampling,
            calibration,
        )
        output_path = Path(command[2])

        if output_path.exists() and not args.overwrite and not args.dry_run:
            print(f"skip {test['test_id']}: {output_path} already exists")
            skipped += 1
            continue

        if args.dry_run:
            print(shlex.join(command))
            rendered += 1
            continue

        sysex_path = Path(command[1])
        require(sysex_path.is_file(), f"missing SysEx for {test['test_id']}: {sysex_path}")
        print(f"[{index + 1}/{len(tests)}] {test['test_id']}")
        subprocess.run(command, check=True)
        rendered += 1

    print(
        f"P0-A batch complete: {rendered} rendered/planned, {skipped} skipped; "
        f"calibration={args.calibration}, oversampling={args.oversampling}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError, subprocess.CalledProcessError) as exc:
        print(f"render_reference_manifest: {exc}", file=sys.stderr)
        raise SystemExit(1)
