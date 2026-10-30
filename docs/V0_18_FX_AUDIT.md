# v0.18 FX audit and implementation boundary

This document records the evidence and source-code review used to start the Prophet-6-style digital-effects milestone. The goal is to reuse license-compatible mature work where it genuinely helps, while keeping Prophet-specific behavior, raw-state compatibility and uncertain hardware semantics explicit.

## Authoritative hardware behavior

Primary behavior source: **Sequential Prophet-6 Operation Manual 2.1**.

Verified points used by v0.18:

- Effect A feeds Effect B in **series**.
- The panel EFFECTS ON/OFF control is a **true bypass** of the digital-effects path.
- The hardware effects subsystem is specified as dual **24-bit / 48 kHz** digital effects.
- Effect A includes BBD delay, digital delay, chorus, PH1, PH2, PH3, ring modulation, FL1 and FL2.
- Effect B includes BBD delay, digital delay, chorus, PH1, PH2, PH3, ring modulation and the reverb family (Hall, Room, Plate, Spring).
- The v2.1 MIDI appendix still documents legacy NRPN select domains `FX 1 Select = 0..5` and `FX 2 Select = 0..9`, despite the later panel effect list being larger. Prophec-y therefore does **not** widen or reinterpret serialized/NRPN hardware values without independent evidence.

This last mismatch is important: v0.18 may use additional internal effect identifiers, but they are not written into the canonical 1024-byte hardware payload until their actual serialized identities are established.

## Primary reusable reference: joshjetson/phosphor

Repository: `joshjetson/phosphor`
License: MIT
Pinned review commit: `33b1073fa1b56315d0ca877b0210efec40f68f4d`

Phosphor remains the strongest clean-room implementation reference because its Prophet-6 module combines factory-bank/raw-state analysis with instrument-specific DSP behavior. Its current Prophet-6 effects implementation already renders:

- BBD delay,
- digital delay,
- chorus,

while preserving unsupported selectors dry.

The initial Prophec-y v0.18 core intentionally follows/adapts the same class of implementation decisions:

- one mono delay line feeding both stereo wet outputs while preserving stereo dry,
- 1 second maximum delay,
- logarithmic/free delay-time mapping to be applied by the hardware mapper,
- bounded feedback,
- BBD repeat low-pass loss and slow clock wander,
- cubic interpolation for the wandering BBD read head,
- stereo chorus based on two quadrature modulated delay taps,
- all delay/chorus buffers allocated in `prepare()` and never resized by the realtime process path.

The current candidate constants inherited as implementation starting points are deliberately calibration-replaceable:

- BBD repeat loss corner: 2.6 kHz,
- BBD fractional clock wander: 0.25%,
- BBD wander target rate: 0.6 Hz,
- chorus centre delay: 7.2 ms,
- chorus maximum sweep: 4.8 ms.

These values are **not** claimed as Sequential circuit/firmware constants. They are an MIT-licensed public model and will be replaced or adjusted if controlled Prophet-6 measurements support a better fit.

## Secondary reusable reference: ShmKnd/Patina

Repository: `ShmKnd/Patina`
License: MIT
Language/dependencies: C++17 standard library, no JUCE required.

Patina is a useful secondary implementation source because it already provides realtime-oriented C++ engines for:

- BBD delay,
- spring and plate reverb,
- phaser/chorus modulation,
- ring modulation examples,
- lower-level analog-modeling/filter primitives.

For v0.18 it is primarily a design/reference candidate for the effect families Phosphor's Prophet-6 module does not yet render. Any code materially adapted from Patina must receive its own attribution in `THIRD_PARTY_NOTICES.md` before merge.

## Reference-only copyleft projects

GPL/AGPL projects remain useful for behavioral comparison and test ideas, but source is not copied into Prophec-y's clean-room core. This includes previously reviewed analog-synth / Prophet-inspired projects and newer generic effect implementations discovered during the v0.18 review.

## v0.18 architecture decision

The effects subsystem is a JUCE-independent `P6Effects` object with two fixed slots:

```text
stereo synth sum
      |
analog-distortion candidate
      |
   FX slot A
      |
   FX slot B
      |
    output
```

Rules:

1. Global bypass is bit-transparent at the DSP boundary.
2. A and B always process in that order.
3. All delay/modulation memory is allocated in `prepare()`.
4. `process()` performs no vector resize/allocation.
5. Unsupported effect families pass dry rather than using a misleading generic placeholder.
6. The canonical hardware program remains untouched; the future `ParameterMapper` layer converts verified raw values into DSP-domain settings.
7. Internal identifiers for later-OS effects are not serialized until the legacy-NRPN/newer-panel discrepancy is resolved.

## First implementation slice

The first executable v0.18 slice implements and regression-tests:

- global true bypass,
- per-slot off/bypass behavior,
- serial A -> B ordering,
- digital delay,
- BBD-style delay candidate,
- stereo chorus candidate,
- deterministic silence/stability behavior.

Next integration slice: map the existing verified FX NRPN/state fields into DSP-domain settings and place `P6Effects` after the current stereo distortion stage in `P6Engine`. Phaser, ring-mod, flanger and reverb families follow behind the same slot contract rather than creating parallel effect paths.
