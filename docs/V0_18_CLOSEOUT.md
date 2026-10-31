# v0.18 closeout — Digital Effects

Status: **CLOSED**

Functional code boundary before closeout documentation: `4ffacedebad58960b7ee0b23bd19c191bd8c5741` on `v0.18-digital-effects`.

## Release scope completed

v0.18 delivers one JUCE-independent, realtime-safe global digital-effects path in `P6Engine`:

`voice sum -> program/MIDI volume -> analog-distortion candidate -> FX A -> FX B -> output`

The verified legacy Prophet-6 selector domains now have audible runtime behavior:

- Effect A: Off, BBD Delay, Digital Delay, Chorus, PH1, PH2.
- Effect B: Off, BBD Delay, Digital Delay, Chorus, PH1, PH2, Hall, Room, Plate, Spring.

PH3, Ring Mod, FL1 and FL2 also have internal candidate DSP and tests, but remain outside the serialized hardware selector domain until their later-OS raw encoding is independently established.

## Correctness/realtime hardening completed

- true global bypass and per-slot Off transparency;
- deterministic A -> B serial ordering;
- effect-type changes invalidate old algorithm state so frozen tails cannot reappear;
- CC120 clears audible FX state using bounded/logical realtime invalidation rather than bulk clearing long buffers;
- FX sync uses the same internal/host/MIDI-clock authority as the scheduler;
- minimum deterministic smoothing for delay/rate/depth/feedback automation;
- all long FX buffers allocated in `prepare()`; no vector resize on the realtime path;
- sample-rate-dependent reverb coefficient cache explicitly invalidated by `prepare()` and regression-tested.

## Performance/regression infrastructure completed

v0.18 adds permanent diagnostics rather than relying on intuition:

- deterministic DSP fingerprints;
- a performance benchmark covering 1/6 voices, FX off/on, 48/96 kHz, 1x/2x/4x oversampling, average block cost and worst-block cost;
- CI execution of the v0.18 FX suite alongside core/state/control/realtime tests.

Safe optimisations integrated:

1. Reverb coefficient caching outside the per-sample loop.
2. Exact zero-history FIR convolution skip while VCO/slop/sync/filter state continues free-running.
3. Base note-to-Hz conversion cache keyed by effective glide + pitch-bend base pitch; coarse/fine, Slop, LFO, aftertouch and Poly Mod remain downstream and dynamic.
4. Pan Spread gain caching: six fixed `sin/cos` gains are recalculated only when spread changes.

A dedicated maximum-spread render proves the Pan change is bit-identical before/after optimisation:

`pan_2x = d0332e325eb4376a`

A separate cutoff-law cache experiment was intentionally **not** integrated because its measured benefit was not convincing enough to expand the change surface before v0.19 profiling/calibration.

## Validation boundary

The v0.18 release gate requires:

- Windows Release VST3 build;
- Linux Release VST3 build;
- all core/state/control/realtime/v0.18 FX tests;
- deterministic DSP fingerprint snapshot;
- `pluginval --strictness-level 10` on Windows and Linux;
- Linux ASan+UBSan;
- validated VST3 artifacts.

The functional tree containing the final Pan optimisation passed this gate on the experiment path, and the canonical branch will be gated once more after this closeout documentation commit before merge/release.

## Explicitly deferred / opaque

The following do **not** reopen v0.18:

- later-OS raw selector/control encoding for PH3, Ring, FL1 and FL2;
- Ring Mod low-note tracking storage/control identity;
- exact hardware evolution of internal FX tails while global bypass is active;
- final acoustic matching of FX constants/topologies;
- exact LFO sync division law, CC4, swing ratio, sequence flag/mode semantics and chord-memory storage already tracked elsewhere in the matrix.

These require controlled hardware evidence or belong to v0.19 calibration.

## UI/visual concept deferred

A future **reactive character FX visual** concept has been added to the master parity/backlog matrix, intentionally outside v0.18. Candidate visuals include glowing tube saturation, plasma discharges, vibrating spring reverb, spinning tape reels, optical tremolo light, ring-mod oscilloscope, ferrofluid distortion and shimmer particles.

This work is deferred to v0.20+ UI exploration, after the editor's visual language, accessibility/reduced-motion policy and UI performance budget are defined. The future architecture must consume bounded low-cost telemetry on the UI side and must never move decorative rendering or expensive analysis onto the realtime audio thread.

## Next milestone

**v0.19 — Sound matching and optimisation** becomes active after the v0.18 release merge.

Primary goals:

- controlled Prophet-6 hardware capture corpus;
- oscillator/PW/mixer and hard-sync matching;
- HPF/LPF and envelope calibration;
- Slop/Vintage/distortion/modulation depth matching;
- FX acoustic calibration;
- profiling-driven CPU work protected by Reference-A and DSP fingerprints.
