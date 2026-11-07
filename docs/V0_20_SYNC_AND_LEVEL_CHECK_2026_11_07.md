# Hard-sync and Character level check — 7 November 2026

## Hard-sync candidate

In the default LegacyStepOnly oscillator, a hard reset can interrupt an edge
before its matching closing BLAMP corner. Clearing that pending corner left a
large unmatched correction. The candidate closes each pending ramp at the
reset fraction before dropping its state. The opt-in StepAndSlope experiment
is not changed; its own stress failures remain unresolved.

The earlier isolated probe reproduced a 55,555.6 peak at 48 kHz, 8 kHz,
shape 1, width 0.5, ratio 1 and phase 0.25. The default-mode candidate passed
the original 165-case/1.5-second grid (absolute threshold 100), an expanded
1,530-case/0.1-second grid at 44.1/48/96 kHz and phases 0, .25, .9999999
(absolute threshold 10), and the permanent regression at 1.5 seconds for 36
combinations. This is boundedness evidence, not a proof of alias quality or
hardware parity. The eight-case, 20-seed P6Voice diagnostic at 48 kHz and
1x/2x also remained within its dry/pre-filter limits. The existing full DSP
fingerprints were identical for the scenarios they cover; those fingerprints
are not proof that sync sound is unchanged. Test music, modulation and sync
sweeps, especially waveform changes and boundary timing, before release.

## Character default levels

Run `p6_character_level_matrix > character_levels.csv`. Its 72 rows cover a
440 Hz sine and single-note/four-note P6Engine performances, four input scales,
and None plus all five default devices. The three-second RMS window includes
attack and release and uses electrical RMS, not perceptual loudness. Inputs
come from this implementation, not matched Prophet hardware. All values and
peaks must be interpreted in that context. The committed CSV was rendered at
48 kHz with the integration candidate.

| Device | RMS difference vs input across cases | Maximum output sample peak |
|---|---:|---:|
| None | approximately 0 dB | 0.860 |
| Tube | -8.18 to -5.29 dB | 0.437 |
| Plasma | -39.71 to +16.01 dB | 0.583 |
| Optical Spring | -4.66 to -4.08 dB | 0.542 |
| Ferrofluid | -3.53 to -1.13 dB | 0.571 |
| Neon | -1.63 to +10.99 dB | 0.534 |

Plasma's broad input-dependent range can include threshold/ignition behaviour;
it is not evidence that a fixed output gain alone fixes the issue. No gain law
or existing state was changed. Further tests should include the full plugin
routing, varied patches and dynamics, both RMS- and loudness-matched listening,
automation, and project migration. No limiter is implied by these peaks.
