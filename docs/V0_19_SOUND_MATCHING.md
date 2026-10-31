# v0.19 — Sound Matching and Optimisation

Status: **ACTIVE**

v0.19 is the first milestone whose primary authority is **controlled audio captured from a real Prophet-6**, rather than factory-bank inference, compressed web demonstrations, or another software implementation. Existing Reference-A material remains useful for regression and hypothesis generation, but it is not calibration truth.

The v0.18 DSP fingerprints remain the anti-regression guard. They answer “did Prophec-y change?” Controlled hardware captures answer the separate question “did Prophec-y become more like the Prophet-6?”

## 1. Exit goals

v0.19 should leave the project with:

1. a reproducible, versioned real-hardware capture corpus;
2. measured oscillator, mixer, filter, envelope, hard-sync and modulation laws;
3. calibrated distortion, Slop/Vintage and digital-effects candidates where the corpus supports them;
4. objective before/after error reports rather than listening-only tuning;
5. profiling-driven CPU improvements protected by deterministic fingerprints and hardware-reference metrics;
6. no guessed SysEx/NRPN semantics introduced merely to improve a fit.

A calibration is promoted toward the default only when it improves the relevant controlled hardware tests without materially regressing unrelated controlled tests.

## 2. Evidence hierarchy

From strongest to weakest:

1. **Controlled Prophet-6 capture + exact patch SysEx + documented globals and performance conditions.**
2. Repeated controlled captures from another Prophet-6 / another voice where component variance matters.
3. Official Sequential documentation for documented ranges/behavior.
4. Lossless or high-quality public audio with sufficiently known patch/control conditions.
5. MIT-compatible Prophet-specific implementations such as Phosphor as hypothesis/reference material.
6. Compressed web demos / factory-performance inference.

Levels 5–6 may suggest a candidate but cannot, by themselves, establish a hardware calibration constant.

## 3. Capture contract

### 3.1 Audio

Preferred acquisition:

- 96 kHz / 24-bit PCM WAV;
- direct line output into a clean audio interface, with no DAW processing;
- no normalization, limiter, gate, EQ, compressor, dither or sample-rate conversion before archiving;
- fixed interface input gain for an entire related sweep;
- leave at least 6 dBFS peak headroom;
- capture stereo even for nominally mono tests so unexpected stereo behavior is observable;
- preserve the untouched original recording as the source of truth.

48 kHz / 24-bit lossless capture is acceptable when 96 kHz is unavailable. Lossy audio is diagnostic-only and must not become calibration truth.

### 3.2 Prophet state

For every test family archive:

- exact Program/Edit Buffer SysEx used for the capture;
- Prophet-6 OS version;
- relevant Global settings that are not stored in the 1024-byte program;
- MIDI note(s), velocity, channel pressure, wheel/bend values and pedal state where relevant;
- note-on duration, release duration and pre-roll;
- mono/stereo output routing and audio-interface path;
- whether one particular hardware voice was forced/identified, if possible;
- any warm-up/calibration procedure performed before capture.

Never reconstruct a patch later from a photograph when its SysEx can be retained at capture time.

### 3.3 Default isolation conditions

Unless the test explicitly targets one of these blocks:

- digital FX OFF;
- Distortion = 0;
- Pan Spread = 0;
- Slop at minimum / Vintage behavior disabled where the hardware/global permits;
- LFO destinations OFF;
- Poly Mod sources/destinations OFF;
- velocity modulation OFF;
- aftertouch modulation OFF;
- oscillator sync OFF;
- filters fully open/bypassed as far as the hardware permits for oscillator tests;
- one held note and one voice where practical.

The goal is not to create an artificial “best sounding” patch. It is to isolate one transfer function at a time.

## 4. Corpus layout

Recommended immutable source layout:

```text
reference_hardware/
  capture-set-YYYYMMDD-A/
    manifest.json
    sysex/
      OSC_SHAPE_001.syx
      ...
    audio/
      OSC_SHAPE_001.wav
      ...
```

Generated Prophec-y renders, resamples, plots and fit outputs belong outside the immutable source folders.

Each test has a stable `test_id`; the same ID always means the same control condition. A re-capture receives a new `capture_id`, not an overwritten WAV.

### Minimum manifest fields

```json
{
  "capture_id": "P6-A-2026-001",
  "test_id": "OSC1_SHAPE_C4_RAW127",
  "hardware": "Sequential Prophet-6",
  "firmware": "record exact version",
  "sample_rate_hz": 96000,
  "bit_depth": 24,
  "channels": 2,
  "sysex_file": "sysex/OSC1_SHAPE_C4_RAW127.syx",
  "audio_file": "audio/OSC1_SHAPE_C4_RAW127.wav",
  "midi_note": 60,
  "velocity": 127,
  "pre_roll_seconds": 2.0,
  "note_seconds": 3.0,
  "release_seconds": 2.0,
  "notes": "document globals, interface, gain and any deviation from the default isolation contract"
}
```

Do not store personally identifying or unnecessary equipment metadata in the corpus.

## 5. P0 capture matrix

The sequence below is intentional: downstream blocks should not be calibrated using an upstream oscillator/mixer model that is still moving substantially.

### P0-A — Oscillator and mixer

Capture Osc1 and Osc2 separately where possible.

**Pitch law**
- notes C2, C3, C4, C5, C6;
- coarse reference and several offsets across the usable range;
- Osc2 fine at center and symmetric negative/positive points.

**Shape morph**
- raw shape 0, 32, 64, 96, 127, 160, 192, 224, 254;
- C2, C4 and C6 for a subset sufficient to expose aliasing/frequency-dependent changes.

**Pulse width**
- raw PW 0, 16, 32, 64, 96, 127/128, 160, 192, 224, 240, 255;
- capture waveform duty, DC/mean behavior and level change.

**Mixer law**
- each oscillator independently at raw level 0, 16, 32, 48, 64, 80, 96, 112, 127;
- repeat combinations around the upper half to detect summing saturation/interactions;
- sub oscillator and noise independently at the same representative levels.

P0-A should be substantially stable before changing the default filter calibration.

### P0-B — Hard sync

- Osc1 isolated as audible slave;
- Osc2 master swept across ratios below, near and above Osc1;
- at least 1:2, 2:3, 1:1, 3:2, 2:1, 3:1 and non-integer intervals;
- repeat at C2/C4/C6 where practical;
- capture enough steady state for spectral comparison plus the note-on transition.

Metrics must distinguish fundamental/pitch correctness from reset-transient spectrum and alias energy.

### P0-C — LPF and HPF

Use a spectrally rich, stable source after oscillator/mixer calibration.

**Cutoff law**
- raw 0, 8, 16, 24, 32, 48, 64, 80, 96, 112, 128, 144, 156, 164;
- resonance minimum first;
- repeat selected points with 50% and 100% keyboard tracking.

**Resonance law**
- representative cutoff points in low/mid/high range;
- resonance raw 0, 32, 64, 96, 128, 160, 192, 224, 255;
- document onset of self-oscillation if observed.

LPF and HPF are fitted independently. Do not force a shared law for convenience.

### P0-D — Amp and filter envelopes

For Attack, Decay and Release independently:

- raw 0, 8, 16, 24, 32, 48, 64, 80, 96, 112, 127;
- repeat at least three captures at short/mid/long settings to estimate timing variability;
- Amp and Filter envelopes are fitted independently;
- for filter-envelope tests choose cutoff/resonance conditions that make envelope position measurable without clipping or self-oscillation.

Also capture Sustain raw 0, 16, 32, 64, 96, 112, 127 for level law.

### P0-E — Modulation depth

After the static destination laws are stable:

- LFO pitch, PW, LPF, HPF and VCA destinations;
- Poly Mod Filter Env and Osc2 sources into Osc1 pitch/shape/PW and filters;
- aftertouch destinations;
- mod-wheel contribution to LFO amount.

Use slow modulation for transfer-depth measurement and audio-rate cases separately for bandwidth/spectral behavior.

### P0-F — Distortion, Slop and Vintage

**Distortion**
- controlled sine/saw-like source at several input levels;
- Distortion raw sweep including minimum, quarter, half, three-quarter and maximum;
- measure level compensation, asymmetry and harmonic growth.

**Slop/Vintage**
- repeated long notes and repeated note-ons;
- per-voice and aggregate pitch statistics where possible;
- capture drift time scale separately from static per-voice offsets/level/filter/envelope variation.

Do not fit stochastic behavior to a single take.

### P0-G — Digital FX

FX calibration comes after the dry path is stable.

For each verified effect type capture:

- dry/wet relationship at several mix values;
- P1/P2 representative sweep;
- impulse/short transient and sustained tonal source;
- synced and free-time delay cases where applicable;
- stereo response and tail decay.

Later-OS PH3/Ring/FL1/FL2 state encoding remains an independent evidence problem and must not be guessed to enable this acoustic work.

## 6. Objective comparison metrics

No single score is authoritative. The report should expose component metrics so a better spectral fit cannot hide a worse envelope or gain fit.

Initial engineering targets:

| Domain | Initial acceptance target |
|---|---|
| steady oscillator pitch | < 2 cents |
| filter cutoff at measured knob points | < 5% frequency error where measurable |
| envelope A/D/R timing | < 10% at mid-range measured points |
| oscillator harmonic balance | mean H2–H16 error < 2–3 dB on controlled steady-state windows |
| static gain / preset level after defined normalization | within ±1 dB |
| hard-sync comparison | report harmonic/centroid/alias error separately; no single opaque scalar |
| Slop/Vintage | match distribution and drift time scale, not sample-by-sample randomness |
| FX | report wet gain, time/rate, stereo and decay/spectral metrics separately |

Targets are engineering gates, not claims about perceptual indistinguishability. They may be tightened after the first high-quality corpus establishes realistic repeatability of the hardware itself.

## 7. Analysis rules

1. Align captures using event/onset information; do not time-warp steady hardware behavior to make it fit.
2. Remove only documented capture latency/DC when required; preserve raw originals.
3. Compare at a common sample rate using one documented high-quality resampling path when native rates differ.
4. Window steady-state oscillator spectra away from note-on and note-off transitions.
5. Measure envelopes in the time domain before judging their spectral consequence.
6. Report gain normalization separately from absolute-gain error.
7. Repeat measurements when analog variance is comparable to the proposed model improvement.
8. Every promoted calibration change must identify which test IDs improved and which regressed.

## 8. Calibration versioning and promotion

Existing `referenceACandidateV07/V08/V09()` remain historical web/reference candidates and stay explicitly non-default.

Controlled-hardware calibration should use a separate versioned family (for example `hardwareCandidateV019A`) rather than silently mutating Reference-A constants. Promotion flow:

```text
hypothesis
  -> isolated candidate calibration
  -> controlled corpus report
  -> DSP fingerprint impact review
  -> unrelated-domain regression review
  -> promote / reject
```

A candidate that improves one patch but worsens the sweep for that subsystem is not a valid calibration.

## 9. Performance work in v0.19

Performance optimisation continues, but only after measurement shows a real hotspot and only under two guards:

- deterministic DSP fingerprints for accidental internal regressions;
- controlled-hardware metrics for fidelity-sensitive changes.

Preferred first optimisations are algebraically exact or move parameter-invariant work outside the sample loop. Changes to precision, approximations, SIMD math substitutions or state-update cadence require profiling evidence plus acoustic regression proof.

GitHub-hosted benchmark percentages are useful directional diagnostics but are too runner-sensitive to be treated as absolute product CPU claims. Stable local-machine baselines should be added before setting hard CPU budgets.

## 10. Immediate v0.19 work order

1. Make calibration rendering sample-rate selectable so 96 kHz hardware captures can be compared without first degrading the reference.
2. Define and ingest the first controlled capture manifest/corpus.
3. Establish P0-A oscillator/mixer baseline report.
4. Calibrate oscillator/PW/mixer only if the controlled data supports a change.
5. Move through hard sync -> filters -> envelopes -> modulation -> stochastic/drive -> FX.
6. Profile again after each major calibrated subsystem and integrate only measured, fingerprint-safe optimisations.

The custom/reactive FX visual concept remains **DEFERRED / UI** in the master matrix and is outside v0.19 sound-matching scope unless a later UI milestone deliberately pulls it forward.
