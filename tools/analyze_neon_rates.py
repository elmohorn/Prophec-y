#!/usr/bin/env python3
"""Compare Neon at 48 kHz with a 192 kHz render, aligned in time.

This measures total waveform difference, not aliasing alone. Results also
contain filter/discretization and resampling differences. Requires numpy/scipy.
"""
import argparse
from pathlib import Path
import numpy as np
from scipy.ndimage import shift
from scipy.optimize import minimize_scalar
from scipy.signal import resample_poly

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
args = parser.parse_args()
print('frequency_hz,reference_rms,raw_difference_db,best_lag_samples,aligned_difference_db')
for freq in (251, 997, 4001):
    a = np.fromfile(args.directory / f'neon_48000_{freq}.f32', dtype='<f4')
    b = np.fromfile(args.directory / f'neon_192000_{freq}.f32', dtype='<f4')
    if len(a) != 48000 or len(b) != 192000:
        raise SystemExit('Unexpected render length')
    a = a[9600:-100].astype(float)
    b = resample_poly(b, 1, 4)[9600:-100].astype(float)
    reference = np.mean(b*b)
    def loss(delay):
        aligned = shift(b, delay, order=3, mode='nearest')
        return np.mean((a[100:-100] - aligned[100:-100])**2)
    result = minimize_scalar(loss, bounds=(-2, 2), method='bounded')
    raw = np.mean((a-b)**2)
    print(f'{freq},{np.sqrt(reference):.6f},{10*np.log10(raw/reference):.2f},'
          f'{result.x:.3f},{10*np.log10(result.fun/reference):.2f}')
