#!/usr/bin/env python3
"""Acquire the v0.19 sync references; verify bytes, decode, and channel identity.

These are incompletely documented references, not known-state calibration data.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request

import numpy as np
from scipy.io import wavfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path,
                        default=Path('calibration/v019_sync_sources/acquisition.json'))
    parser.add_argument('--output', type=Path,
                        default=Path('reference_private/sync_search'))
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    args.output.mkdir(parents=True, exist_ok=True)
    for item in manifest['files']:
        dest = args.output / item['filename']
        if dest.exists():
            data = dest.read_bytes()
        else:
            with urllib.request.urlopen(item['url'], timeout=30) as response:
                data = response.read(50 * 1024 * 1024 + 1)
        if len(data) != item['bytes'] or sha(data) != item['sha256']:
            raise ValueError(f'Original size/hash mismatch: {dest}')
        if not dest.exists():
            dest.write_bytes(data)
        raw = subprocess.check_output([
            'ffmpeg', '-v', 'error', '-i', str(dest), '-f', 'f32le',
            '-acodec', 'pcm_f32le', 'pipe:1'])
        if sha(raw) != item['decoded_pcm_f32le_sha256']:
            raise ValueError(f'Decoded PCM changed (possibly decoder version): {dest}')
        samples = np.frombuffer(raw, dtype='<f4').reshape(-1, item['channels'])
        if len(samples) != item['decoded_frames'] or not np.isfinite(samples).all():
            raise ValueError(f'Invalid decoded signal: {dest}')
        if 'derived' in item:
            # Deliberately do not average the OB-6 left and Prophet-6 right.
            derivative = args.output / item['derived']['filename']
            wavfile.write(derivative, item['sample_rate'], samples[:, 1])
            sr, check = wavfile.read(derivative)
            if sr != item['sample_rate'] or not np.array_equal(check, samples[:, 1]):
                raise ValueError(f'Right-channel extraction failed: {derivative}')
            if sha(derivative.read_bytes()) != item['derived']['sha256']:
                raise ValueError(f'Derivative container changed: {derivative}')
        print(f'Verified: {dest.name}')


if __name__ == '__main__':
    main()
