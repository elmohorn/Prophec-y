#!/usr/bin/env python3
"""Validate and normalize JUCE-generated VST3 moduleinfo.json before packaging.

Some generated bundles contain trailing commas in JSON objects/arrays.
Strict hosts may reject these even when the VST3 binary builds successfully.
"""
from __future__ import annotations

import json
import re
from pathlib import Path


def main() -> None:
    candidates = list(Path("build-web").rglob("Contents/Resources/moduleinfo.json"))
    if len(candidates) != 1:
        raise AssertionError(f"Expected one VST3 moduleinfo.json, found {len(candidates)}")

    path = candidates[0]
    original = path.read_text(encoding="utf-8")
    try:
        data = json.loads(original)
        repaired = False
    except json.JSONDecodeError:
        # Strip only trailing commas immediately before a closing bracket/brace.
        # The final json.loads below is authoritative; never ship malformed JSON.
        normalized = re.sub(r",(?=\s*[}\]])", "", original)
        data = json.loads(normalized)
        path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        repaired = True

    assert data.get("Name") == "P6 Core", "Unexpected VST3 module identity"
    classes = data.get("Classes", [])
    assert any(
        item.get("Category") == "Audio Module Class"
        and "Instrument" in item.get("Sub Categories", [])
        and "Synth" in item.get("Sub Categories", [])
        for item in classes
    ), "VST3 instrument class metadata missing"

    # Validate the on-disk contents, not only the parsed in-memory copy.
    assert json.loads(path.read_text(encoding="utf-8")) == data
    binary = path.parent.parent / "x86_64-win" / "P6 Core.vst3"
    assert binary.is_file() and binary.stat().st_size > 100_000, f"Missing VST3 binary: {binary}"
    print(f"VST3 moduleinfo JSON valid ({'repaired trailing commas' if repaired else 'unchanged'}): {path}")


if __name__ == "__main__":
    main()
