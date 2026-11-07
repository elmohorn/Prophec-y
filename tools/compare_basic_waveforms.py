#!/usr/bin/env python3
"""Compare published single-Osc1 examples with fixed current-model endpoints.

This does not isolate the hardware oscillator from its analog filters/VCA or
the MP3 chain. Only pitch and comparison level are matched, never timbre.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess

import numpy as np
from scipy.signal import welch

from analyze_sync_stages import power_harmonics, write_csv
from compare_online_suite import decode, compare, SR


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--probe', type=Path, required=True)
    ap.add_argument('--audio', type=Path, required=True)
    ap.add_argument('--windows', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--analyze-only', action='store_true', help='reuse renders only when their recorded input/probe hashes match')
    a = ap.parse_args()
    hashes = dict(source_audio_sha256=hashlib.sha256(a.audio.read_bytes()).hexdigest(),
        windows_sha256=hashlib.sha256(a.windows.read_bytes()).hexdigest(),
        probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest())
    if a.analyze_only:
        previous = json.loads((a.output/'provenance.json').read_text())
        if any(previous.get(k) != v for k, v in hashes.items()):
            raise ValueError('input/probe provenance mismatch; refusing stale renders')
    a.output.mkdir(parents=True, exist_ok=True)
    (a.output/'renders').mkdir(exist_ok=True)
    cases = list(csv.DictReader(a.windows.open()))
    source = decode(a.audio)
    references = {}
    specifications = []
    for c in cases:
        nominal = 440*2**((int(c['nominal_midi_note'])-69)/12)
        first = round(float(c['start_s'])*SR)
        x = source[first:first+round(float(c['duration_s'])*SR)]
        count = min(16, int(10000/(nominal*1.03)))
        metrics, h = power_harmonics(x, nominal, count)
        one, _ = power_harmonics(x[:len(x)//2], nominal, count)
        two, _ = power_harmonics(x[len(x)//2:], nominal, count)
        stability = 1200*math.log2(two['power_f0_hz']/one['power_f0_hz'])
        f, power = welch(x, SR, nperseg=len(x), axis=0)
        power = power.mean(axis=1)
        frequency = metrics['power_f0_hz']
        width = max(8., 2*SR/len(x))
        sub = 10*math.log10(max(float(power[abs(f-frequency/2) < width].sum()), 1e-30) /
                           max(float(power[abs(f-frequency) < width].sum()), 1e-30))
        if metrics['f0_search_edge'] or abs(stability) > 10 or sub > -35:
            raise ValueError(f"{c['case_id']}: captured pitch unstable/boundary; review before timbre comparison")
        references[c['case_id']] = (x, metrics, h)
        specifications.append(dict(c, measured_f0_hz=metrics['power_f0_hz'],
            half_window_pitch_drift_cents=stability, harmonics=count,
            suboctave_power_dbc=sub,
            decoded_peak=float(np.max(abs(x)))))
    # This pitch/quality-only ledger precedes synthesis and spectral comparisons.
    write_csv(a.output/'reference_windows.csv', specifications)
    def job(item):
        c, oversampling = item
        x, ref_metrics, rh = references[c['case_id']]
        note = int(c['nominal_midi_note'])
        f0 = ref_metrics['power_f0_hz']
        offset = 12*math.log2(f0/(440*2**((note-69)/12)))
        wav = a.output/'renders'/f"{c['case_id']}_os{oversampling}.wav"
        cmd = [str(a.probe.resolve()), str(wav), '--free-running', '--note', str(note),
            '--slave-semis', f'{offset:.12g}', '--shape1', c['shape'], '--pw1', c['pulse_width'],
            '--oversampling', str(oversampling), '--seconds', c['duration_s'], '--preroll', '.5']
        if not a.analyze_only:
            result = subprocess.run(cmd, check=True, capture_output=True, text=True)
            wav.with_suffix('.log').write_text(result.stdout+result.stderr)
        y = decode(wav)
        measured, ch = power_harmonics(y, f0, len(rh))
        # dBc is relative to each signal's fundamental, not its largest partial.
        ref = rh-rh[0]
        candidate = ch-ch[0]
        included = ref >= -40
        odd = np.arange(1, len(ref)+1) % 2 == 1
        used = included & odd
        row = dict(case_id=c['case_id'], shape=float(c['shape']), oversampling=oversampling,
            reference_f0_hz=f0, candidate_f0_hz=measured['power_f0_hz'],
            pitch_difference_cents=1200*math.log2(measured['power_f0_hz']/f0),
            included_harmonics=int(included.sum()),
            harmonic_rmse_floor60_db=float(np.sqrt(np.mean((np.maximum(candidate[included], -60)-np.maximum(ref[included], -60))**2))),
            odd_harmonic_rmse_db=float(np.sqrt(np.mean((candidate[used]-ref[used])**2))),
            **{key:value for key,value in compare(x, y, kind='steady').items() if key in ['mr_log_spectral_mae_db','log_band_shape_rmse_db']})
        for i in range(len(ref)):
            row[f'reference_h{i+1}_dbc'] = float(ref[i])
            row[f'candidate_h{i+1}_dbc'] = float(candidate[i])
        for label, h in [('reference', ref), ('candidate', candidate)]:
            power = 10**(h/10)
            row[label+'_even_over_odd_db'] = float(10*np.log10(max(float(power[~odd].sum()), 1e-24)/max(float(power[odd].sum()), 1e-24)))
        return row
    jobs = [(c, os) for c in cases for os in [2, 8]]
    with ThreadPoolExecutor(max_workers=4) as pool:
        rows = list(pool.map(job, jobs))
    # Variable harmonic counts at higher notes need a union of columns.
    fields = list(dict.fromkeys(k for r in rows for k in r))
    with (a.output/'comparisons.csv').open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator='\n')
        w.writeheader(); w.writerows(rows)
    (a.output/'provenance.json').write_text(json.dumps(dict(renders=len(rows),
        **hashes,
        method='stereo-power harmonics, own F0, relative to H1, reference >= -40 dBc, all-harmonic error floored at -60 dBc; primary odd error and even/odd ratio separate',
        new_renders=0 if a.analyze_only else len(rows),
        limits=['Published Osc1 endpoint labels, no exact SysEx/filter/VCA/drive state.',
            'Hardware recording includes analog signal path and lossy MP3 encoding.',
            'Free-running probe bypasses filters/Poly Mod and uses a single oscillator; no hard-sync truth acquired.',
            'Oversampling is a numerical sensitivity, not an independent hardware sample.',
            'A nonzero even partial can arise in more than the oscillator; do not fit an asymmetric VCO from this alone.']), indent=2)+'\n')
    print(json.dumps({'renders':len(rows), 'output':str(a.output)}))


if __name__ == '__main__':
    main()
