# v0.18 FX audit and implementation boundary

This document records the evidence and source-code review used for the Prophet-6-style digital-effects milestone. The goal is to reuse license-compatible mature work where it genuinely helps, while keeping Prophet-specific behavior, raw-state compatibility and uncertain hardware semantics explicit.

## Authoritative hardware behavior

Primary behavior source: **Sequential Prophet-6 Operation Manual 2.1** plus the OS 1.3.1 effect addendum.

Verified points used by v0.18:

- Effect A feeds Effect B in **series**.
- The panel EFFECTS ON/OFF control is a **true bypass** of the digital-effects path.
- The hardware effects subsystem is specified as dual **24-bit / 48 kHz** digital effects.
- Effect A includes BBD delay, digital delay, chorus, PH1, PH2, PH3, ring modulation, FL1 and FL2.
- Effect B includes BBD delay, digital delay, chorus, PH1, PH2, PH3, ring modulation and the reverb family (Hall, Room, Plate, Spring).
- BBD/DDL use time + feedback; Chorus/PH1/PH2/PH3/FL1/FL2 use rate + depth; Hall/Room/Plate use time + early-reflection amount; Spring uses decay + tone. Ring modulation uses modulation frequency plus a low-note tracking mode.
- The v2.1 MIDI appendix still documents legacy NRPN select domains `FX 1 Select = 0..5` and `FX 2 Select = 0..9`, despite the later panel effect list being larger. Prophec-y therefore does **not** widen or reinterpret serialized/NRPN hardware values without independent evidence.

This last mismatch is important: v0.18 implements the later effect families as internal DSP identifiers, but they are not written into the canonical 1024-byte hardware payload and an unknown raw selector is never coerced into another known effect.

## Primary reusable reference: joshjetson/phosphor

Repository: `joshjetson/phosphor`
License: MIT
Pinned review commit: `33b1073fa1b56315d0ca877b0210efec40f68f4d`

Phosphor remains the strongest clean-room implementation reference because its Prophet-6 module combines factory-bank/raw-state analysis with instrument-specific DSP behavior. Its current Prophet-6 effects implementation renders:

- BBD delay,
- digital delay,
- chorus,

while preserving unsupported selectors dry.

The Prophec-y v0.18 delay/chorus core intentionally follows/adapts the same class of implementation decisions:

- one mono delay line feeding both stereo wet outputs while preserving stereo dry,
- 1 second maximum delay,
- logarithmic/free delay-time mapping in the hardware mapper,
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
- chorus maximum sweep: 4.8 ms,
- free delay: 2 ms .. 1 s,
- feedback maximum candidate: 0.92,
- modulation-rate candidate: 0.05 .. 8 Hz.

These values are **not** claimed as Sequential circuit/firmware constants. They are public-model/calibration candidates and can be replaced when controlled Prophet-6 measurements support a better fit.

## Secondary reusable reference: ShmKnd/Patina

Repository: `ShmKnd/Patina`
License: MIT
Language/dependencies: C++17 standard library, no JUCE required.

Patina was reviewed for realtime-oriented architecture in effect families Phosphor's Prophet-6 module does not yet render:

- multi-stage all-pass phaser,
- short modulated-delay chorus/flanger concepts,
- spring and plate reverb networks,
- BBD implementation patterns,
- analog-modeling/filter primitives.

The Prophec-y phaser/flanger/reverb code is a separate clean-room implementation using standard DSP topologies rather than copied Patina source. Patina therefore remains a design/reference source in `THIRD_PARTY_NOTICES.md`; if later code is materially adapted, its full MIT attribution must be added before merge.

## Reference-only copyleft projects

GPL/AGPL projects remain useful for behavioral comparison and test ideas, but source is not copied into Prophec-y's clean-room core. This includes previously reviewed analog-synth / Prophet-inspired projects and generic effect implementations discovered during the v0.18 review.

## v0.18 architecture

The effects subsystem is a JUCE-independent `P6Effects` object with two fixed slots:

```text
stereo synth sum
      |
program + MIDI volume
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
3. All delay/modulation/reverb memory is allocated in `prepare()`.
4. `process()`, `setSettings()` and tail reset perform no vector resize/allocation.
5. `P6Engine` owns the single global effects path; scheduler and JUCE wrapper do not create parallel effects engines.
6. CC120 All Sound Off clears both voice output and stored FX energy; ordinary Note Off / CC123 may leave musical tails.
7. The canonical hardware program remains untouched; `P6EffectMapper` converts only verified raw controls into DSP-domain settings.
8. Internal identifiers for later-OS effects are not serialized until the legacy-NRPN/newer-panel discrepancy is resolved.
9. Unknown hardware selector values pass dry at the DSP boundary while remaining losslessly preserved in `P6PatchState`.

## Implemented families

### Verified legacy selector domain — patch selectable now

The complete currently verified legacy raw domain has audible DSP:

**Effect A (`0..5`)**

- Off
- BBD delay
- Digital delay
- Chorus
- PH1
- PH2

**Effect B (`0..9`)**

- Off
- BBD delay
- Digital delay
- Chorus
- PH1
- PH2
- Hall
- Room
- Plate
- Spring

PH1/PH2 use a clean-room six-stage all-pass cascade with different resonance voicing. Hall/Room/Plate/Spring use a preallocated four-line stereo feedback-delay network with type-specific delay, damping and early-reflection voicing. These algorithms are functional/provisional candidates for later hardware calibration, not claims of Sequential's internal implementation.

### Later-OS families — DSP implemented, raw selector mapping intentionally unresolved

The following types have executable internal DSP and regression coverage:

- PH3 — six-stage phaser with separate Oberheim-style candidate voicing,
- Ring Mod — sine multiplication with fixed-frequency mode,
- FL1 — short through-zero-style flanger candidate with feedback/high resonance,
- FL2 — short through-zero-style flanger candidate without feedback/lower resonance.

They are **not yet selectable from a hardware raw FX selector**. Their internal enum values live outside the verified raw range specifically so no accidental state serialization can occur.

Ring Mod low-note tracking is also still pending: official behavior is known, but it should only be wired once the later-OS selector/control encoding is established so the runtime/state contract is not split across guessed identities.

## Mapping behavior

`P6EffectMapper` currently maps:

- FX On/Off NRPN 135 -> global true-bypass state,
- A selector/mix/P1/P2/sync/division -> NRPN 119..124,
- B selector/mix/P1/P2/sync/division -> NRPN 127..132,
- Clock BPM NRPN 167 -> synced-delay timing,
- BBD/DDL free time -> logarithmic 2 ms .. 1 s candidate,
- delay feedback -> bounded 0 .. 0.92 candidate,
- Chorus/PH1/PH2 P1 -> 0.05 .. 8 Hz modulation-rate candidate,
- Chorus/PH1/PH2 P2 -> depth 0..1,
- Hall/Room/Plate P1/P2 -> normalised time / early reflections,
- Spring P1/P2 -> normalised decay / tone.

Synced delay uses the documented beat-division set and repeatedly halves delay times longer than the one-second engine maximum.

## Regression / release-gate coverage

v0.18 adds explicit tests for:

- global true bypass,
- per-slot Off transparency,
- serial A -> B timing,
- digital-delay timing,
- BBD silence/stability,
- chorus stereo modulation,
- PH1/PH2 audible finite output,
- Hall/Room/Plate/Spring finite tails,
- PH3/Ring/FL1/FL2 internal candidate audibility,
- unknown-selector dry behavior,
- mapping laws and legacy-domain boundaries,
- engine ownership of the FX path,
- CC120 clearing FX tails,
- no ordinary heap allocation after `prepare()` with active delay, phaser, flanger and reverb processing.

## Remaining v0.18 work

1. Establish the actual Prophet-6/OB-6 later-OS raw selector/control encoding for PH3, Ring, FL1 and FL2 from strong independent evidence or differential SysEx/hardware captures; do not guess it.
2. Wire Ring Mod low-note tracking once that control identity is established.
3. Confirm exact FX bypass/tail behavior on hardware (whether internal tails advance while global true bypass is engaged).
4. Run the Windows/Linux VST3 + `pluginval --strictness-level 10` + Linux ASan/UBSan gate with the complete FX test set.
5. Update the parity matrix only after the complete gate is green; acoustic matching of the effect constants remains a v0.19 task.
