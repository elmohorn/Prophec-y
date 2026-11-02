"""Convert tube_plasma_audition raw renders into transparent listening comparisons."""
from pathlib import Path
import argparse
import csv
import math
import numpy as np
from scipy.io import wavfile

parser = argparse.ArgumentParser()
parser.add_argument('folder', type=Path)
args = parser.parse_args()
names = ('dry', 'tube_default', 'tube_driven', 'plasma_default', 'plasma_driven')
audio = {n: np.fromfile(args.folder / (n+'.f32'), dtype='<f4').reshape(-1, 2) for n in names}
if any(x.shape != (48000*8, 2) or not np.isfinite(x).all() for x in audio.values()):
    raise SystemExit('Invalid render')
metrics = {n: (float(np.max(np.abs(x))), float(np.sqrt(np.mean(x.astype(np.float64)**2)))) for n, x in audio.items()}
if any(rms < 1e-9 for _, rms in metrics.values()):
    raise SystemExit('Silent render; cannot make a level-matched comparison')
ref = metrics['dry'][1]
level = {n: ref / metrics[n][1] for n in names}
common = min(1.0, .85 / max(p for p, _ in metrics.values()))
matched_peak = max(metrics[n][0] * level[n] for n in names)
matched_common = min(1.0, .85 / matched_peak)
for n in names:
    wavfile.write(args.folder / (n+'_original.wav'), 48000, (audio[n]*common).astype('<f4'))
    wavfile.write(args.folder / (n+'_matched.wav'), 48000, (audio[n]*level[n]*matched_common).astype('<f4'))
with (args.folder / 'metrics.csv').open('w', newline='') as f:
    w = csv.writer(f);w.writerow(('setting','peak_linear_before_packaging','rms_linear_before_packaging','rms_relative_to_dry_db','matching_gain_db'))
    for n in names:
        peak,rms=metrics[n];w.writerow((n, peak, rms, 20*math.log10(rms/ref), 20*math.log10(level[n])))
print('common output gain:', common, 'matched common gain:', matched_common)
for n in names:
    print(n, 'peak', round(metrics[n][0],4), 'RMS relative dry', round(20*math.log10(metrics[n][1]/ref),2),'dB')
