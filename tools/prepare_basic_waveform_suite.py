#!/usr/bin/env python3
"""Reacquire the hash-pinned Bonedo waveform-reference media for local analysis.

Publisher audio stays private; repository contains URLs, hashes and measurements.
Run from the repository root. Requires ffmpeg/ffprobe and numpy.
"""
import hashlib
import json
from pathlib import Path
import urllib.request

from inspect_reference_media import inspect


def main():
    protocol = json.loads(Path('calibration/v019_basic_waves/protocol.json').read_text())
    root = Path('reference_private/acquisition')
    root.mkdir(parents=True, exist_ok=True)
    for asset in protocol['media']:
        path = root/asset['file']
        if path.is_file():
            data = path.read_bytes()
        else:
            with urllib.request.urlopen(asset['source_url'], timeout=30) as response:
                data = response.read(50*1024*1024+1)
        if len(data) > 50*1024*1024 or hashlib.sha256(data).hexdigest() != asset['sha256']:
            raise ValueError(f"{asset['file']}: changed/invalid download; refusing input")
        if not path.exists():
            path.write_bytes(data)
        inspect(path, expected_duration=asset['decoded_duration_s'])
    print(f"{len(protocol['media'])} sources hash-verified and fully decoded")


if __name__ == '__main__':
    main()
