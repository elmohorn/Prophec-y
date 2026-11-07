#!/usr/bin/env python3
"""Measure stable tone plateaus in an independently recorded self-oscillating filter.

This does not recover the knob trajectory or the absolute cutoff transfer law.
No synth parameter is fitted to this recording. Requires numpy, scipy, ffmpeg.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.ndimage import median_filter
from scipy.signal import stft

from compare_online_suite import decode, SR


def analyze(x):
    nfft, hop = 2048, 120
    f, t, z = stft(x.mean(axis=1), SR, nperseg=nfft,
                    noverlap=nfft-hop, boundary=None, padded=False)
    p = abs(z)**2
    ix = np.flatnonzero((f >= 700) & (f <= 7000))
    k = ix[np.argmax(p[ix], axis=0)]
    j = np.arange(len(t))
    log = np.log(np.maximum(p, 1e-30))
    den = log[k-1, j] - 2*log[k, j] + log[k+1, j]
    delta = np.clip(.5*(log[k-1, j]-log[k+1, j]) /
                    np.where(abs(den) > 1e-15, den, 1), -.5, .5)
    hz = (k+delta)*SR/nfft
    pitch = 69 + 12*np.log2(hz/440)
    smooth = median_filter(pitch, 5)
    share = (p[k-1, j]+p[k, j]+p[k+1, j])/np.maximum(p.sum(axis=0), 1e-30)
    good = (share > .8) & (hz >= 800) & (hz <= 6500) & (abs(np.gradient(smooth, t)) < 2)
    starts = np.flatnonzero(good & ~np.r_[False, good[:-1]])
    ends = np.flatnonzero(good & ~np.r_[good[1:], False])+1
    plateaus = []
    for a, b in zip(starts, ends):
        if t[b-1]-t[a] >= .035:
            plateaus.append(dict(start_s=float(t[a]), end_s=float(t[b-1]),
                pitch_semitones=float(np.median(pitch[a:b])),
                frequency_hz=float(np.median(hz[a:b]))))
    steps = []
    for a, b in zip(plateaus[:-1], plateaus[1:]):
        distance = abs(100*(b['pitch_semitones']-a['pitch_semitones']))
        if b['start_s']-a['end_s'] < .25 and 40 < distance < 250:
            steps.append(dict(start_s=a['end_s'], end_s=b['start_s'], distance_cents=distance))
    return t, hz, pitch, share, good, plateaus, steps


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('audio', type=Path)
    ap.add_argument('--output', required=True, type=Path)
    a = ap.parse_args()
    x = decode(a.audio)
    t, hz, pitch, share, good, plateaus, steps = analyze(x)
    if not steps:
        ap.error('no eligible plateau transitions; do not infer a step size')
    a.output.mkdir(parents=True, exist_ok=True)
    for name, rows in [('plateaus', plateaus), ('transitions', steps)]:
        with (a.output/f'{name}.csv').open('w') as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0]))
            w.writeheader(); w.writerows(rows)
    with (a.output/'frequency_trace.csv').open('w') as f:
        w = csv.writer(f); w.writerow(['time_s', 'frequency_hz', 'pitch_semitones', 'three_bin_power_fraction', 'stable'])
        w.writerows(zip(t, hz, pitch, share, good.astype(int)))
    distances = np.array([r['distance_cents'] for r in steps])
    single = distances[(distances >= 70) & (distances <= 130)]
    report = dict(source_sha256=hashlib.sha256(a.audio.read_bytes()).hexdigest(),
        sample_rate=SR, nfft=2048, hop=120, interpolation='bounded log-power parabolic',
        analysis_frequency_range_hz=[800, 6500], minimum_three_bin_power_fraction=.8,
        maximum_stable_slope_semitones_per_second=2, minimum_plateau_s=.035,
        maximum_transition_gap_s=.25, transition_range_cents=[40, 250],
        plateau_count=len(plateaus), transition_count=len(steps),
        transition_median_cents=float(np.median(distances)),
        near_semitone_transition_count=len(single),
        near_semitone_median_cents=float(np.median(single)) if len(single) else None,
        near_semitone_min_cents=float(min(single)) if len(single) else None,
        near_semitone_max_cents=float(max(single)) if len(single) else None,
        near_whole_tone_transition_count=int(np.sum((distances >= 170) & (distances <= 230))),
        full_file_near_fullscale_fraction=float(np.mean(abs(x) >= .999)),
        limits=['Unknown knob positions, sweep speed, firmware and patch: no absolute Hz/raw law inferred.',
            'Near-semitone category is descriptive, using the stated 70..130-cent interval.',
            'A skipped observed plateau can create a two-semitone transition.',
            'Short-window FFT estimates, analog drift and clipping limit precision.',
            'No renderer waveform, filter-envelope depth or parameter default fitted.'])
    (a.output/'selfosc_summary.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
