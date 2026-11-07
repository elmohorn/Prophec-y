#!/usr/bin/env python3
"""Analyze Sync Stack stage interventions without folding stereo to mono.

The reference is a complete mixed hardware patch, not isolated oscillator truth.
Requires numpy/scipy/ffmpeg. Historical mono-fold metrics remain separate.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.signal import welch

from compare_online_suite import decode, SR


def power_harmonics(x, expected=88., count=16):
    if x.ndim != 2 or x.shape[1] != 2 or len(x) < 1024 or not np.isfinite(x).all():
        raise ValueError('finite stereo audio of at least 1024 frames required')
    if not np.isfinite(expected) or expected <= 0 or expected*count >= SR/2:
        raise ValueError('invalid expected fundamental/harmonic range')
    n = min(16384, len(x))
    ac = np.zeros(n)
    frames = 0
    # Sum channel autocorrelation powers; an antiphase channel cannot erase F0.
    for start in range(0, len(x)-n+1, max(1, n//4)):
        a = x[start:start+n]
        a = (a-a.mean(axis=0))*np.hanning(n)[:, None]
        spectrum = np.fft.rfft(a, n=2*n, axis=0)
        corr = np.fft.irfft(np.sum(abs(spectrum)**2, axis=1), n=2*n)[:n]
        if corr[0] > 1e-20:
            ac += corr/corr[0]
            frames += 1
    if not frames:
        raise ValueError('silent signal has no measurable harmonics')
    ac /= frames
    lo = max(1, int(SR/(expected*2**(50/1200))))
    hi = min(n-1, int(SR/(expected/2**(50/1200)))+1)
    if hi <= lo:
        raise ValueError('fundamental period exceeds analysis window')
    k = lo+int(np.argmax(ac[lo:hi]))
    den = ac[k-1]-2*ac[k]+ac[k+1]
    delta = float(np.clip(.5*(ac[k-1]-ac[k+1])/den, -.5, .5)) if abs(den) > 1e-20 else 0.
    f0 = SR/(k+delta)
    f, p = welch(x-x.mean(axis=0), fs=SR, nperseg=n, axis=0)
    powers = []
    for h in range(1, count+1):
        target = h*f0
        width = max(SR/n, target*(2**(35/1200)-1))
        powers.append(p[abs(f-target) <= width].sum(axis=0))
    powers = np.asarray(powers)
    combined = powers.mean(axis=1)
    relative = 10*np.log10(np.maximum(combined/max(float(combined.max()), 1e-30), 1e-24))
    result = dict(power_f0_hz=float(f0), f0_search_edge=bool(k == lo or k == hi-1),
        power_h5_minus_h4_db=float(relative[4]-relative[3]))
    for channel, label in enumerate(['left', 'right']):
        result[label+'_h5_minus_h4_db'] = float(10*np.log10(max(powers[4, channel], 1e-30)/max(powers[3, channel], 1e-30)))
    result.update({f'power_h{i+1}_db': float(v) for i, v in enumerate(relative)})
    return result, relative


def write_csv(path, rows):
    with path.open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator='\n')
        w.writeheader(); w.writerows(rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('render_results', type=Path)
    ap.add_argument('--reference', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    reference = decode(a.reference)
    rows = []
    source_rows = list(csv.DictReader((a.render_results/'comparisons.csv').open()))
    for r in source_rows:
        pre = float(r['pre_roll_s'])
        path = a.render_results/'renders'/r['profile']/f'RA266_pre{pre:g}.wav'
        candidate = decode(path)[round(.3*SR):round(.3*SR)+len(reference)]
        if len(candidate) != len(reference):
            raise ValueError('candidate window too short')
        for label, start, end in [('whole', 0, len(reference)), ('first_half', 0, len(reference)//2), ('second_half', len(reference)//2, len(reference))]:
            rr, rh = power_harmonics(reference[start:end])
            cc, ch = power_harmonics(candidate[start:end])
            use = rh >= -50
            metrics = {'reference_'+k: v for k, v in rr.items()}
            metrics.update({'candidate_'+k: v for k, v in cc.items()})
            metrics['power_harmonic_rmse_db'] = float(np.sqrt(np.mean((ch[use]-rh[use])**2)))
            rows.append(dict(profile=r['profile'], pre_roll_s=pre, window=label, **metrics))
    write_csv(a.output/'power_harmonics.csv', rows)
    summary = []
    for profile in dict.fromkeys(r['profile'] for r in rows):
        for window in ['whole', 'first_half', 'second_half']:
            rr = [r for r in rows if r['profile'] == profile and r['window'] == window]
            result = dict(profile=profile, window=window, realizations=len(rr))
            for key in ['candidate_power_f0_hz', 'candidate_power_h5_minus_h4_db', 'power_harmonic_rmse_db']:
                vals = [r[key] for r in rr]
                result.update({key+'_median':float(np.median(vals)), key+'_min':min(vals), key+'_max':max(vals)})
            summary.append(result)
    write_csv(a.output/'power_summary.csv', summary)
    # Verify isolation kept timing, levels, stack and pan identical. The source
    # program has zero sub/noise, zero distortion and FX off.
    checks = []
    for pre in [0, .5, 1.5, 3]:
        base = a.render_results/'renders'
        paths = {p: base/p/f'RA266_pre{pre:g}.wav' for p in ['current', 'control_preserve', 'raw_mix', 'osc1', 'osc2']}
        identical = paths['current'].read_bytes() == paths['control_preserve'].read_bytes()
        mix, one, two = [decode(paths[p]) for p in ['raw_mix', 'osc1', 'osc2']]
        reconstructed = (100/127)*one+(94/127)*two
        relative_error = float(np.sqrt(np.mean((mix-reconstructed)**2))/max(np.sqrt(np.mean(mix**2)), 1e-30))
        if not identical or relative_error > 1e-6:
            raise ValueError('stage isolation failed no-op or linear reconstruction check')
        checks.append(dict(pre_roll_s=pre, no_op_bit_identical=identical, raw_mix_reconstruction_relative_rms=relative_error))
    (a.output/'isolation_checks.json').write_text(json.dumps(dict(checks=checks,
        reference_sha256=hashlib.sha256(a.reference.read_bytes()).hexdigest(),
        harmonic_method='stereo-power-v1; summed-channel autocorrelation; own F0; 16 harmonics',
        limits=['Whole hardware patch is compared with model interventions, not hardware-isolated components.',
            'Four phase/Slop pre-rolls are sensitivities, not independent hardware recordings.',
            'First/second halves test stationarity only; key timing remains inferred.']), indent=2)+'\n')
    print(json.dumps({'profiles':len(summary)//3, 'windows':len(rows), 'isolation_checks':'passed'}))


if __name__ == '__main__':
    main()
