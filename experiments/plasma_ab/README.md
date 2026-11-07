# Plasma: independent alternatives, 7 November 2026

This is an offline experiment using the actual CharacterFx and CustomCharacter
code from its parent branch. It does not modify production DSP, defaults,
parameters, preset codec, or SysEx. `run.py` creates temporary copies of
CustomCharacter.cpp and checks each expected replacement appears exactly where
expected. Existing sessions are not affected by running or keeping this branch.

## Candidates

- **legacy:** existing algorithm and default state.
- **soft_knee:** input-voltage envelope, 2 ms attack/30 ms release and a
  smoothstep from 0.5x to 1.5x the threshold controls conduction. Illustrative
  only; this bypasses the legacy binary latch's effect on conduction.
- **gain_comp:** divides wet output by the larger of 1 and the nominal
  small-signal gain (drive × saturator slope × output). This is fixed
  parameter-dependent gain compensation, not an AGC or perceptual match.
- **new_defaults:** legacy DSP but ignition .10, mix .5 and output .35 instead
  of .25, 1 and .5; experimental values, not an approved new default.

The threshold replacement is in the actual oversampled production path. At
48 kHz, 2-second static 440 Hz sinusoids, a 0→.12 peak amplitude sine ramp,
and synthetic single-note/chord phrases were rendered. The same matrix ran at
44.1 and 96 kHz. The first 250 ms was excluded from the scalar metrics; raw
48 kHz float stereo renders include the entire phrase. `results.csv` contains
84 rows (4 variants × 3 rates × 7 scenarios). Peak and sample-to-sample step
are not true-peak or click measurements. RMS ratios do not equal perceived
loudness. The ramp has no independently defined reference click metric.

## 48 kHz observations

| candidate | steady sine .04 peak | sine .05 peak | chord RMS vs input |
|---|---:|---:|---:|
| legacy | silent | +18.24 dB | +15.97 dB |
| soft knee | +4.02 dB | +11.28 dB | +15.89 dB |
| gain compensation | silent | -1.98 dB | -4.26 dB |
| alternative defaults | -6.02 dB at .02 peak; +9.07 dB at .04 | +8.60 dB | +6.77 dB |

Soft knee does not address excessive gain when fully lit; gain compensation
alone does not prevent complete silence below ignition; alternative defaults
reduce but do not eliminate large gain changes. None is a production-ready
winner. A soft-knee + gain strategy could be investigated next, but listening,
transient timing, aliasing, CPU and legacy-state compatibility remain open.
The oscillator sync and the source synth patch affect this experiment's chord.

## Reproduce

After building the JUCE-independent p6core static library with CMake and
installing g++ and Python 3:

```
python3 experiments/plasma_ab/run.py /tmp/prophec-plasma-ab build/libp6core.a
```

The output contains CSV metrics and raw float32 interleaved stereo audition
files at 48 kHz. The script builds four temporary binaries. The chosen library
must match the sources being compared. Builds and figures in this document use
the #9 integration candidate, not an independent Prophet-6 capture.
