#!/usr/bin/env python3
"""Compile and run the JUCE-free Character safety probes without GitHub Actions.

Requires Python 3 and a C++20 compiler. No repository checkout downloads,
JUCE, VST3 host, Python packages, or network connection required.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    ("character_visual_dynamics_probe.cpp", "visual"),
    ("character_telemetry_mailbox_probe.cpp", "mailbox"),
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stress-rounds", type=int, default=10,
                        help="concurrent mailbox repetitions (default: 10)")
    parser.add_argument("--sanitizers", choices=("none", "asan-ubsan", "tsan"),
                        default="none", help="optional GCC/Clang sanitizers")
    args = parser.parse_args()
    if args.stress_rounds < 1 or args.stress_rounds > 10000:
        parser.error("--stress-rounds must be between 1 and 10000")

    compiler = shlex.split(os.environ.get("CXX", "cl" if os.name == "nt" else "c++"))
    if not compiler:
        parser.error("CXX cannot be empty")
    is_msvc = Path(compiler[0]).stem.lower() == "cl"
    if is_msvc and args.sanitizers != "none":
        parser.error("select a GCC or Clang compiler to use sanitizers")

    with tempfile.TemporaryDirectory(prefix="p6-character-safety-") as tmp:
        for filename, label in SOURCES:
            source = ROOT / "tests" / filename
            if not source.is_file():
                raise FileNotFoundError(source)
            target = Path(tmp) / (label + (".exe" if os.name == "nt" else ""))
            if is_msvc:
                cmd = compiler + ["/nologo", "/std:c++20", "/EHsc", "/O2",
                                  "/UNDEBUG", "/W4", "/I" + str(ROOT / "plugin"),
                                  str(source), "/Fe:" + str(target)]
            else:
                cmd = compiler + ["-std=c++20", "-O2", "-UNDEBUG",
                                  "-Wall", "-Wextra", "-Wpedantic", "-pthread",
                                  "-I", str(ROOT / "plugin")]
                if args.sanitizers == "asan-ubsan":
                    cmd += ["-O1", "-g", "-fno-omit-frame-pointer",
                            "-fsanitize=address,undefined"]
                elif args.sanitizers == "tsan":
                    cmd += ["-O1", "-g", "-fno-omit-frame-pointer",
                            "-fsanitize=thread"]
                cmd += [str(source), "-o", str(target)]
            print("Compiling:", filename, flush=True)
            subprocess.run(cmd, check=True, cwd=ROOT)
            repetitions = args.stress_rounds if label == "mailbox" else 1
            for n in range(repetitions):
                subprocess.run([str(target)], check=True, cwd=ROOT,
                               stdout=subprocess.DEVNULL if repetitions > 1 else None)
            print(f"PASS {label}: {repetitions} run(s)", flush=True)
    print("JUCE-free Character safety probes passed; this is NOT a VST3 build.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"FAIL Character preflight: {exc}", file=sys.stderr)
        sys.exit(1)
