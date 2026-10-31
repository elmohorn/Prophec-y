# v0.18 FX audit and implementation boundary — CLOSED

v0.18 closes the Prophet-6-style digital-effects milestone. The effects engine is JUCE-independent, shares the existing engine/scheduler clock, preserves the canonical 1024-byte hardware state, and does not invent later-OS selector encodings that are not established by evidence.

## Evidence boundary

Primary hardware references remain the Sequential Prophet-6 Operation Manual 2.1 and the OS 1.3.1 effect addendum. They establish:

- Effect A feeds Effect B in series.
- EFFECTS ON/OFF is a true bypass of the digital-effects path.
- The hardware effects subsystem is described as dual 24-bit / 48 kHz effects.
- A/B include BBD and digital delays, chorus and phasers; the later effect list adds PH3, ring modulation and flangers, while Effect B also includes Hall, Room, Plate and Spring reverbs.
- The published MIDI appendix still exposes the legacy selector ranges `FX 1 Select = 0..5` and `FX 2 Select = 0..9` despite the larger later panel effect list.

Therefore v0.18 does not widen the canonical selector domain or serialize guessed later-OS values. Unknown selectors remain losslessly preserved and pass dry at the DSP boundary.

## Reuse audit

### joshjetson/phosphor — MIT

Pinned review commit: `33b1073fa1b56315d0ca877b0210efec40f68f4d`.

Phosphor was the strongest clean-room Prophet-specific implementation reference for BBD delay, digital delay and chorus. Prophec-y reused/adapted compatible implementation ideas such as preallocated delay memory, bounded feedback, BBD repeat loss/wander, interpolated read heads and quadrature chorus modulation. Candidate constants remain calibration-replaceable and are not claimed as Sequential firmware/circuit constants.

### ShmKnd/Patina — MIT

Reviewed as a secondary architecture/topology reference for realtime phaser, flanger and reverb families. Prophec-y's implementations are separate clean-room DSP using standard topologies. GPL/AGPL projects remain behavioral/reference-only unless explicitly documented otherwise.

## Closed v0.18 architecture

```text
six-voice synth sum
      |
program volume × MIDI volume
      |
analog-distortion candidate
      |
   FX slot A
      |
   FX slot B
      |
    output
```

Contract:

1. A -> B is the only global digital-effects order.
2. Global bypass is bit-transparent at the DSP boundary.
3. Long effect buffers are allocated in `prepare()` and never resized in realtime.
4. CC120 uses logical realtime invalidation rather than bulk clearing long buffers on the audio thread.
5. Effect type changes invalidate the old algorithm state so a frozen tail cannot reappear when returning to a previous type.
6. FX tempo sync consumes the common runtime clock owned by the scheduler/host/MIDI-clock path; there is no second FX clock.
7. Delay/rate/depth/feedback changes use deterministic minimum smoothing to avoid obvious automation discontinuities.
8. Unknown hardware selector values remain preserved and are not coerced to known effects.

## Implemented patch-selectable legacy domain

Effect A (`0..5`): Off, BBD Delay, Digital Delay, Chorus, PH1, PH2.

Effect B (`0..9`): Off, BBD Delay, Digital Delay, Chorus, PH1, PH2, Hall, Room, Plate, Spring.

All of these have audible runtime DSP and regression coverage.

## Later-OS candidates — implemented internally, not serialized

PH3, Ring Mod, FL1 and FL2 have executable internal candidate DSP and regression coverage. They intentionally use internal identifiers outside the verified raw selector range. Ring Mod low-note tracking is also deferred until its state/control identity is established.

These are **OPAQUE / MEASURE**, not incomplete v0.18 release blockers.

## Hardening and performance gate — completed

v0.18 additionally closed the following correctness/performance work:

- stale effect-tail resurrection on effect-type changes;
- bounded-time / logical CC120 effect-state invalidation;
- common-clock FX sync for internal, host and MIDI clock operation;
- deterministic minimum parameter smoothing;
- deterministic rendered-audio fingerprints;
- a repeatable performance benchmark covering 1/6 voices, FX off/on, 48/96 kHz, 1x/2x/4x oversampling and worst-block timing;
- reverb coefficient caching outside the sample loop, including explicit cache invalidation when sample rate changes;
- exact zero-history FIR convolution skip while VCO/slop/sync/filter state continues free-running;
- base MIDI-note-to-Hz conversion caching keyed by effective glide + bend pitch while Slop/LFO/aftertouch/Poly Mod/coarse/fine remain downstream;
- Pan Spread gain caching so the six fixed per-voice `sin/cos` gains are recalculated only when spread changes.

The Pan Spread optimisation was validated with a dedicated nonzero-spread fingerprint. Baseline and cached implementations produced the same hash and acoustic metrics:

`pan_2x = d0332e325eb4376a`

This preserves output bit-for-bit for that controlled render while removing per-sample trigonometric work.

A separate cutoff-law cache experiment was deliberately not integrated because its benefit was not convincing enough to justify expanding the change surface before v0.19 profiling/calibration.

## Release-gate coverage

The v0.18 suite covers:

- true/global bypass and per-slot Off transparency;
- serial A -> B timing;
- BBD/digital delay and synced-delay timing;
- chorus stereo modulation;
- PH1/PH2 finite audible output;
- Hall/Room/Plate/Spring tails;
- internal PH3/Ring/FL1/FL2 candidate audibility;
- unknown-selector dry behavior;
- type-transition state invalidation;
- CC120 effect-tail clearing;
- shared scheduler/FX clock ownership;
- reverb cache correctness across sample-rate re-prepare;
- no ordinary heap allocation after `prepare()` on active FX paths;
- deterministic DSP fingerprints and performance snapshots.

Windows and Linux release builds, CTest, `pluginval --strictness-level 10`, and Linux ASan+UBSan are mandatory release gates.

## Deferred / opaque after v0.18

These items are intentionally not guessed and do not reopen v0.18:

1. Exact later-OS raw selector/control encoding for PH3, Ring, FL1 and FL2.
2. Ring Mod low-note tracking control identity/storage.
3. Exact hardware behavior of internal tails while global FX bypass is engaged.
4. Acoustic matching of candidate effect constants/topologies to controlled Prophet-6 captures.

Items 1–3 require stronger public evidence or differential hardware/SysEx capture. Item 4 belongs to v0.19 sound matching.

## Exit decision

**v0.18 Digital Effects: CLOSED.**

The release boundary is a complete, realtime-safe, testable A -> B effects subsystem for every verified legacy selector, with later-OS unknowns preserved rather than fabricated. v0.19 becomes the active milestone for sound matching and profiling-driven optimisation.
