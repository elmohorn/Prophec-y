#!/usr/bin/env python3
"""Validate an acquired reference by probing AND fully decoding its first audio stream.

An HTTP/download success is not evidence of an audio acquisition. Reports record
container and decoded PCM hashes, without copying copyrighted media into git.
Requires ffmpeg/ffprobe and numpy. This checks integrity, not provenance or patch identity.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile

import numpy as np


def inspect(path, min_duration=0.1, expected_duration=None, tolerance=0.25):
    if not math.isfinite(min_duration) or min_duration <= 0:
        raise ValueError('minimum duration must be positive and finite')
    if not math.isfinite(tolerance) or tolerance < 0:
        raise ValueError('tolerance must be finite and nonnegative')
    if expected_duration is not None and (not math.isfinite(expected_duration) or expected_duration <= 0):
        raise ValueError('expected duration must be positive and finite')
    path = Path(path)
    with path.open('rb') as f:
        head = f.read(512).lstrip().lower()
    if head.startswith((b'<html', b'<!doctype html', b'<?xml')):
        raise ValueError('received a document instead of audio')
    probe = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'a:0',
        '-show_entries', 'stream=codec_name,sample_rate,channels', '-of', 'json', str(path)],
        capture_output=True, text=True, check=True)
    streams = json.loads(probe.stdout).get('streams', [])
    if not streams:
        raise ValueError('no audio stream')
    stream = streams[0]
    rate, channels = int(stream['sample_rate']), int(stream['channels'])
    if rate <= 0 or not 1 <= channels <= 32:
        raise ValueError('unsupported audio geometry')
    pcm_hash = hashlib.sha256()
    count, energy, peak = 0, 0., 0.
    # Keep memory bounded; the full decode also detects errors beyond the header.
    with tempfile.TemporaryFile() as pcm:
        subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-xerror', '-i', str(path),
            '-map', '0:a:0', '-vn', '-acodec', 'pcm_f32le', '-f', 'f32le', '-'],
            stdout=pcm, stderr=subprocess.PIPE, check=True)
        pcm.seek(0)
        while chunk := pcm.read(4 * channels * 65536):
            if len(chunk) % (4 * channels):
                raise ValueError('incomplete decoded frame')
            pcm_hash.update(chunk)
            samples = np.frombuffer(chunk, dtype='<f4').astype(np.float64)
            if not np.all(np.isfinite(samples)):
                raise ValueError('nonfinite decoded samples')
            count += samples.size
            energy += float(np.dot(samples, samples))
            peak = max(peak, float(np.max(np.abs(samples))))
    duration = count / channels / rate
    if duration < min_duration:
        raise ValueError('decoded audio shorter than required minimum')
    if expected_duration is not None and abs(duration - expected_duration) > tolerance:
        raise ValueError('decoded duration does not match expected source duration')
    if peak == 0:
        raise ValueError('decoded audio is silent')
    with path.open('rb') as f:
        source_hash = hashlib.file_digest(f, 'sha256').hexdigest()
    return {'file': path.name, 'bytes': path.stat().st_size, 'sha256': source_hash,
        'codec': stream['codec_name'], 'sample_rate': rate, 'channels': channels,
        'decoded_frames': count // channels, 'decoded_duration_s': duration,
        'pcm_f32le_sha256': pcm_hash.hexdigest(), 'peak': peak,
        'rms': math.sqrt(energy / count), 'integrity': 'full-decode-passed',
        'patch_identity_verified': False}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input', type=Path)
    ap.add_argument('--min-duration', type=float, default=0.1)
    ap.add_argument('--expected-duration', type=float)
    ap.add_argument('--duration-tolerance', type=float, default=0.25)
    a = ap.parse_args()
    try:
        result = inspect(a.input, a.min_duration, a.expected_duration, a.duration_tolerance)
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        ap.exit(1, f'rejected {a.input.name}: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
