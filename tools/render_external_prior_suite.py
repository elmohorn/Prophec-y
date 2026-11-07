#!/usr/bin/env python3
"""Render calibration candidates defined in calibration/external_priors.json.

The suite CSV intentionally contains paths, not copyrighted audio. A local
Reference-A workspace can therefore point at legally obtained SysEx/event files
without adding those assets to the repository.

CSV columns:
    case_id,sysex,events

Optional columns are ignored, so the same file may also carry trust/domain/
reference-segment metadata for later scoring.
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import subprocess
import sys


def load_profiles(path: Path) -> dict[str, dict]:
    with path.open("r", encoding="utf-8") as handle:
        document = json.load(handle)
    profiles = document.get("profiles")
    if not isinstance(profiles, dict) or not profiles:
        raise ValueError(f"{path}: missing non-empty profiles object")
    return profiles


def load_cases(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise ValueError(f"{path}: no cases")
    required = {"case_id", "sysex", "events"}
    missing = required - set(rows[0])
    if missing:
        raise ValueError(f"{path}: missing columns: {', '.join(sorted(missing))}")
    for row_number, row in enumerate(rows, start=2):
        for field in required:
            if not row.get(field, "").strip():
                raise ValueError(f"{path}:{row_number}: empty {field}")
    return rows


def resolve_case_path(value: str, manifest: Path) -> Path:
    candidate = Path(value)
    return candidate if candidate.is_absolute() else (manifest.parent / candidate)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Render the same event/SysEx suite with named external calibration priors."
    )
    parser.add_argument("suite", type=Path, help="CSV with case_id, sysex, events")
    parser.add_argument(
        "--profiles-file",
        type=Path,
        default=Path("calibration/external_priors.json"),
        help="machine-readable prior registry",
    )
    parser.add_argument(
        "--renderer",
        type=Path,
        default=Path("build/p6_render_events_cal"),
        help="p6_render_events_cal executable",
    )
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument(
        "--profiles",
        default="current,reference_a_v09,phosphor_pw_v019,reference_a_v09_plus_phosphor_pw",
        help="comma-separated profile names",
    )
    parser.add_argument("--oversampling", choices=("1", "2", "4", "8"), default="2")
    parser.add_argument("--pre-roll-seconds", type=float, default=0.0)
    parser.add_argument("--tail-seconds", type=float, default=2.0)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    try:
        profiles = load_profiles(args.profiles_file)
        cases = load_cases(args.suite)
        selected = [name.strip() for name in args.profiles.split(",") if name.strip()]
        unknown = [name for name in selected if name not in profiles]
        if unknown:
            raise ValueError("unknown profiles: " + ", ".join(unknown))

        renderer = args.renderer
        output_root = args.output_dir
        if not args.dry_run:
            output_root.mkdir(parents=True, exist_ok=True)

        manifest_rows: list[dict[str, str]] = []
        for row in cases:
            case_id = row["case_id"].strip()
            sysex = resolve_case_path(row["sysex"].strip(), args.suite)
            events = resolve_case_path(row["events"].strip(), args.suite)

            if not args.dry_run:
                if not sysex.is_file():
                    raise FileNotFoundError(f"{case_id}: SysEx not found: {sysex}")
                if not events.is_file():
                    raise FileNotFoundError(f"{case_id}: event CSV not found: {events}")

            for profile_name in selected:
                profile = profiles[profile_name]
                profile_args = profile.get("renderer_args", [])
                if not isinstance(profile_args, list) or not all(isinstance(x, str) for x in profile_args):
                    raise ValueError(f"profile {profile_name}: renderer_args must be a string array")

                profile_dir = output_root / profile_name
                output = profile_dir / f"{case_id}.wav"
                command = [
                    str(renderer),
                    str(sysex),
                    str(events),
                    str(output),
                    "--oversampling",
                    args.oversampling,
                    "--pre-roll-seconds",
                    str(max(0.0, args.pre_roll_seconds)),
                    "--tail-seconds",
                    str(max(0.0, args.tail_seconds)),
                    *profile_args,
                ]

                print(" ".join(command))
                if not args.dry_run:
                    profile_dir.mkdir(parents=True, exist_ok=True)
                    subprocess.run(command, check=True)

                manifest_rows.append(
                    {
                        "case_id": case_id,
                        "profile": profile_name,
                        "output": str(output),
                        "description": str(profile.get("description", "")),
                    }
                )

        if not args.dry_run:
            manifest_path = output_root / "render_manifest.csv"
            with manifest_path.open("w", encoding="utf-8", newline="") as handle:
                writer = csv.DictWriter(
                    handle, fieldnames=("case_id", "profile", "output", "description")
                )
                writer.writeheader()
                writer.writerows(manifest_rows)
            print(f"wrote {manifest_path}")

        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
