#!/usr/bin/env python3
"""Ablate Sync Stack voice count and Slop in explicitly labeled private copies.

No imported program is overwritten. These probes test model nuisance factors,
not proposed changes to the original preset or global sound defaults.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np

from analyze_sync_stages import power_harmonics, write_csv
from compare_online_suite import decode, SR


def unpack(packed):
    raw = bytearray()
    for start in range(0, len(packed), 8):
        block = packed[start:start+8]
        raw.extend(v | (((block[0] >> bit) & 1) << 7) for bit, v in enumerate(block[1:]))
    return raw


def pack(raw):
    result = bytearray()
    for start in range(0, len(raw), 7):
        block = raw[start:start+7]
        result.append(sum(((v >> 7) & 1) << bit for bit, v in enumerate(block)))
        result.extend(v & 127 for v in block)
    return bytes(result)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--renderer', required=True, type=Path)
    ap.add_argument('--output', required=True, type=Path)
    ap.add_argument('--results', required=True, type=Path)
    a = ap.parse_args()
    source = Path('reference_private/online/266.syx').read_bytes()
    if len(source) != 1178 or source[:4] != b'\xf0\x01\x2d\x02' or source[-1] != 247:
        ap.error('expected original single-program Prophet-6 dump')
    raw = unpack(source[6:-1])
    if len(raw) != 1024 or pack(raw) != source[6:-1]:
        ap.error('program must round-trip exactly before ablation')
    a.output.mkdir(parents=True, exist_ok=True)
    a.results.mkdir(parents=True, exist_ok=True)
    interventions = {'one_voice': {85: 0}, 'no_slop': {18: 0}, 'one_voice_no_slop': {85: 0, 18: 0}}
    provenance = {'source_sha256': hashlib.sha256(source).hexdigest(), 'interventions': {}, 'renders': 12,
        'renderer_sha256': hashlib.sha256(a.renderer.read_bytes()).hexdigest(),
        'events_sha256': hashlib.sha256(Path('calibration/recovered_events/RA266_events.csv').read_bytes()).hexdigest(),
        'source_sha': '34135bbc4334269b6b358065b45167557f676b50',
        'limits': ['Model interventions only; original source patch remains unchanged.',
            'No voice-count or Slop setting is inferred for the hardware from these ablations.']}
    for name, edits in interventions.items():
        candidate = raw.copy()
        for offset, value in edits.items():
            candidate[offset] = value
        actual = [i for i, (old, new) in enumerate(zip(raw, candidate)) if old != new]
        assert actual == sorted(edits)
        program = source[:6]+pack(candidate)+source[-1:]
        (a.output/f'{name}.syx').write_bytes(program)
        provenance['interventions'][name] = {'verified_raw_edits': {str(k): [raw[k], v] for k, v in edits.items()},
            'sha256': hashlib.sha256(program).hexdigest()}
    # Write intervention specification before any candidate score is measured.
    (a.results/'ensemble_protocol.json').write_text(json.dumps(provenance, indent=2)+'\n')
    ref = decode(Path('reference_private/online/266_reference.wav'))
    ref_metrics, ref_harmonics = power_harmonics(ref)
    def run(job):
        name, pre = job
        wav = a.output/f'{name}_pre{pre:g}.wav'
        command = [str(a.renderer.resolve()), str(a.output/f'{name}.syx'),
            'calibration/recovered_events/RA266_events.csv', str(wav),
            '--pre-roll-seconds', str(pre), '--oversampling', '2']
        subprocess.run(command, check=True, capture_output=True)
        candidate = decode(wav)[round(.3*SR):round(.3*SR)+len(ref)]
        metrics, h = power_harmonics(candidate)
        return dict(probe=name, pre_roll_s=pre, **metrics,
            power_harmonic_rmse_db=float(np.sqrt(np.mean((h-ref_harmonics)**2))))
    jobs = [(name, pre) for name in interventions for pre in [0, .5, 1.5, 3]]
    with ThreadPoolExecutor(max_workers=4) as pool:
        rows = list(pool.map(run, jobs))
    write_csv(a.results/'ensemble_metrics.csv', rows)
    assert Path('reference_private/online/266.syx').read_bytes() == source
    print(json.dumps({'renders':len(rows), 'original_preserved':True, 'reference':ref_metrics}))


if __name__ == '__main__':
    main()
