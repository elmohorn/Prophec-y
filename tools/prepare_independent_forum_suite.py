#!/usr/bin/env python3
"""Reacquire the exact public inputs frozen in the independent-audio protocol.

Usage: python3 tools/prepare_independent_forum_suite.py
Then run compare_online_suite.py with calibration/v019_independent_audio/manifest.csv.
Download responses must match hashes AND fully decode. No third-party audio/SysEx
is committed to the repository. Run from the repository root.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request

from inspect_reference_media import inspect


def main():
    protocol = json.loads(Path('calibration/v019_independent_audio/input_protocol.json').read_text())
    root = Path('reference_private/acquisition')
    root.mkdir(parents=True, exist_ok=True)
    inputs = [('forum_audio.wav', protocol['audio_url'], protocol['audio']['sha256']),
              ('forum_patch.syx', protocol['sysex_url'], protocol['sysex_sha256']),
              ('forum_filter_sweep.wav', protocol['sweep_audio_url'], protocol['sweep_audio']['sha256'])]
    for name, url, expected in inputs:
        path = root/name
        if path.is_file():
            data = path.read_bytes()
        else:
            with urllib.request.urlopen(url, timeout=30) as response:
                data = response.read()
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError(f'{name}: source changed or invalid response; refusing to overwrite')
        if not path.exists():
            path.write_bytes(data)
        if path.suffix == '.wav':
            inspect(path, expected_duration=protocol['audio' if name == 'forum_audio.wav' else 'sweep_audio']['decoded_duration_s'])
        elif len(data) != 1178 or data[:4] != b'\xf0\x01\x2d\x02' or data[-1:] != b'\xf7':
            raise ValueError('invalid Prophet-6 program dump')
    phrase = root/'forum_phrase.wav'
    subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-y', '-i', str(root/'forum_audio.wav'),
        '-t', '8.6', '-c:a', 'pcm_f32le', str(phrase)], check=True)
    # Container metadata may vary with ffmpeg version; decoded samples are the
    # comparable content. compare_online_suite records the actual excerpt hash.
    inspect(phrase, expected_duration=8.6, tolerance=.001)
    print('Three original inputs hash-verified; phrase decoded and ready.')


if __name__ == '__main__':
    main()
