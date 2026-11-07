# Sync/Poly Mod and Plasma threshold follow-up — 7 November 2026

## Voice sync and Poly Mod

`p6_voice_sync_polymod_probe` exercises the production P6Voice path with filters
active: 44.1/48/96 kHz, 1x/2x/4x, MIDI 36/60/96/119, Osc2 transpose -12/0/+12,
three seeds, and five modes (sync with no Poly Mod; Osc2 to frequency, shape,
PWM; and a combined Osc2 plus filter-envelope route). Note-on for 70 ms is
followed by 30 ms of release. The combined modulation source reaches 1.7418.

All 1,620 cases completed with finite audio and diagnostic controls. The largest
observed dry sample was 1.25321 and the largest pre-filter sample 2.50885. The
thresholds of 20/100 in this probe detect explosions, not musical quality or
limits on output level. A build using the old oscillator also passed these
same 1,620 cases (dry peak 1.25322), so they are not an isolated reproducer of
the original edge problem. They are broader integration evidence, not proof
of hardware fidelity, complete automation coverage, immunity to all parameter
combinations, or a substitute for DAW listening. The opt-in StepAndSlope mode
remains unvalidated.

## Plasma ignition and mix

`p6_plasma_threshold_probe > plasma.csv` passes a steady stereo 440 Hz sine into
CharacterFx at 48 kHz for one second, measuring the final 750 ms. It spans four
Ignition values (0/.25/.5/.75), three Mix values (.25/.5/1), and nine input
sample peaks (.005 through .5): 108 cases. Other Plasma controls are at their
defaults. The measured relative dB is output RMS / input RMS; a blank denotes
zero output, not a finite -300 dB. Telemetry is an averaged UI conduction
indicator. This is a controlled threshold test rather than musical loudness,
and results include dry/wet phase and filtering.

With default Ignition .25 and Mix 1, the .04-peak sine produced zero output,
whereas .05 produced output RMS .28884, +18.24 dB relative to its input RMS.
At Mix .25 the .04-peak input produced output RMS .02121 (-2.50 dB); at .05
output RMS was .09862 (+8.91 dB). These are steady-state values at two levels,
not a measurement of a time-domain click. Lower Mix retains a dry component but
does not eliminate a large change across this threshold.

The present law computes voltage=x*drive, uses a fixed threshold with
hysteresis/hold to set a binary lit state, and saturates the lit signal. At
current defaults drive is about +12.6 dB and threshold about 0.199 in the
oversampled domain. A fixed output trim cannot remove the off/on transition;
reducing Mix alone also does not solve it. Candidate approaches for a future
isolated A/B experiment: level-dependent soft knee for conduction, a regulated
wet output, or revised defaults for *new* instances. Evaluate each with slow
and fast amplitude ramps, harmonics, transients, patches and loudness-matched
listening. Preserve legacy project/preset behavior via an explicit versioned
mode if a DSP law changes. Do not present a physically faithful model or claim
an optimal gain law from this test. No production audio changes are made here.
