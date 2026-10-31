# P6 Core — phase 1 / v0.18

Unison correction checkpoint: [fixed first-key release-tail behavior and corrected calibration](docs/V0_19_UNISON_FIX_AND_RECALIBRATION.md). The fix restores a missing Solid Bass note; 360 corrected local-grid renders identify a balanced 12-octave audition profile. Earlier affected Unison calibration rows remain historical only.


Compatibility-first C++20 core for a Prophet-6-style software synthesizer. The 1024-byte hardware program state remains canonical and lossless; DSP/calibration layers are replaceable so measured sound matching can evolve without breaking SysEx/NRPN compatibility.

## v0.19 recovered-audio research checkpoint — active branch

The [next filter/sync experiment](docs/V0_19_FILTER_AND_SYNC_RESULTS.md) adds
320 isolated-filter-factor renders and 480 independent Fourier-oracle probe
renders. Filter-envelope depth is now a narrower audition candidate; the
experimental slope correction is rejected for promotion after pulse-endpoint
failures. Production DSP/defaults remain unchanged.

The active branch includes a [primary-source research review](docs/V0_19_RESEARCH_REVIEW.md)
and a [fresh 180-render comparison](docs/V0_19_RECOVERED_AUDIO_RESULTS.md) against
five recovered real Prophet-6 excerpts. Nine named candidate profiles share the
same four pre-roll realizations; spectral, envelope, stereo and sync-harmonic
metrics remain separate. No acoustic defaults were promoted from this experiment.
The missing generic `analysis/` tools were recovered from the existing v0.15
project archive. Third-party audio and factory SysEx remain private inputs.

## v0.18 headline — digital effects closed

v0.18 adds and hardens one global realtime-safe serial digital-effects path:

`voice sum -> program/MIDI volume -> analog-distortion candidate -> FX A -> FX B -> output`

- Verified legacy Effect A selectors now render Off, BBD Delay, Digital Delay, Chorus, PH1 and PH2.
- Verified legacy Effect B selectors now render Off, BBD Delay, Digital Delay, Chorus, PH1, PH2, Hall, Room, Plate and Spring.
- PH3, Ring Mod, FL1 and FL2 have internal candidate DSP and regression coverage, but their later-OS raw selector encoding is deliberately not guessed or serialized.
- FX tempo sync uses the existing internal/host/MIDI common clock rather than introducing another timing authority.
- Global bypass is bit-transparent at the DSP boundary.
- Effect-type changes invalidate stale tails; CC120 uses bounded/logical realtime invalidation rather than sweeping long buffers in the audio callback.
- Minimum deterministic parameter smoothing protects delay/rate/depth/feedback automation from obvious discontinuities.
- DSP fingerprints and a performance benchmark are permanent CI diagnostics.
- Safe optimisations now include reverb coefficient caching, exact zero-history FIR skipping, base-pitch conversion caching and Pan Spread gain caching.
- The dedicated nonzero-Pan render is bit-identical before/after Pan caching: `d0332e325eb4376a`.
- Windows/Linux VST3, CTest, `pluginval --strictness-level 10`, and Linux ASan+UBSan remain release gates.

See `docs/V0_18_FX_AUDIT.md`, `docs/V0_18_CLOSEOUT.md` and `docs/PROPHET6_PARITY_MATRIX.md`.

## v0.17 — control/performance contract

v0.17 closed the non-FX control/performance boundary before digital effects:

- versioned lock-free last-writer-wins host/MIDI edit ordering;
- sustain/Damper CC64 with physical-key state separated from sustained voice lifetime;
- MIDI Volume CC7 independent of saved program volume;
- distinct CC120/121/123 semantics;
- realtime allocation regression tests and allocation-free arp ordering;
- evidence-backed synced-LFO first-key phase reset while the exact rhythmic-division law remains unguessed.

See `docs/V0_17_CONTROL_CONTRACT.md` and `docs/V0_17_CLOSEOUT.md`.

## v0.16 — wrapper/release stability gate

v0.16 established the cross-platform release baseline: Windows/Linux VST3, strict pluginval, Linux sanitizers, release-mode tests, chord-memory held-key fixes and serialized-layout/cardinality integrity.

See `docs/V0_16_STABILITY.md`.

## Core architecture

```text
SysEx / MIDI NRPN / host automation
              |
      P6PatchState
   canonical 1024 bytes
              |
 ParameterMapper / calibration
              |
        P6Engine / DSP
              |
            Audio
```

The core remains JUCE-independent. JUCE 9.0.4 is an optional thin VST3 host adapter; `P6PatchState`, `P6PerformanceScheduler` and `P6Engine` remain the musical/state sources of truth.

## Compatibility / state

- Full 1024-byte Prophet-6 program payload preserved losslessly.
- Correct Program Data `0x02` vs Edit Buffer Data `0x03` handling.
- Sequential 7-bit pack/unpack and concatenated-bank scanning.
- Known serialized parameters map to verified NRPN identities while unsupported bytes remain untouched.
- Stateful NRPN decoder with Data Increment/Decrement support.
- NRPN edits update the canonical raw payload.
- Program-name NRPNs `236..255` update payload bytes `107..126` and visible name state.
- Sequence note/velocity NRPNs `256..1023` update stored sequence bytes in place.
- Unknown sequence high-bit metadata is preserved rather than interpreted by guesswork.
- Host state uses a versioned JUCE-independent codec and preserves unknown hardware bytes.

## Performance scheduler

- Internal sample-domain clock.
- Host BPM/PPQ anchoring.
- MIDI Clock 24 PPQN with Start/Continue/Stop behavior.
- Ten documented clock divisions.
- Arp Up, Down, Up+Down, Random and Assign, 1–3 octave range, HOLD and Beat Sync.
- Stored six-track / 64-step sequencer playback with middle-C transposition reference.
- Source ownership for live/ARP/SEQ notes so generated note-offs do not kill coincident live notes.
- Opaque swing ratio, sequence flag/mode semantics and exact Random generator remain explicitly unguessed.

## DSP core

- Six free-running voices.
- Continuous triangle -> saw -> pulse trapezoid-family oscillator morph with anti-alias correction.
- Sub-sample Osc2 -> Osc1 hard sync.
- Osc1 sub oscillator and noise source.
- Independent per-oscillator/per-voice slow Slop.
- Resonant two-pole HPF candidate.
- Resonant four-pole SSM2040-lineage LPF candidate.
- Mixer -> HPF -> LPF -> VCA voice path.
- Independent exponential amp/filter envelopes.
- Audio-rate-capable global LFO and aftertouch routing.
- Audio-rate Poly Mod from Filter Env/Osc2 to Osc1 pitch/shape/PW and LPF/HPF.
- 1x/2x/4x/8x oversampling infrastructure; 2x default.
- Post-sum analog-distortion candidate.
- Standard Slop and Vintage variation modes.
- Global serial FX A -> B subsystem from v0.18.

## Stored sequencer layout

The raw program payload contains the 64-step / six-track sequence at bytes `128..895`:

```text
track 1: notes 128..191, velocity/flags 192..255
track 2: notes 256..319, velocity/flags 320..383
...
track 6: notes 768..831, velocity/flags 832..895
```

`P6Sequence` decodes all 384 cells while preserving unknown metadata. The low seven velocity bits are exposed as the current MIDI-velocity candidate; the high bit remains opaque until hardware evidence establishes its exact meaning.

## Reference-A calibration harness

The `analysis/` directory contains the reference index/comparator, patch-aware inference and suite automation. The comparison intentionally reports separate spectral, envelope, timing, crest and stereo metrics rather than a misleading single “accuracy percentage”. Reference-A is useful diagnostic evidence, but controlled dry Prophet-6 captures remain the required calibration truth for v0.19.

Representative tools:

```bash
python3 analysis/reference_index.py reference.mp3 --names data/factory_program_names_2015.txt --csv segments.csv --json segments.json
python3 analysis/compare_audio.py reference_segment.wav candidate.wav --out comparison.json
python3 analysis/compare_program.py 7 reference.mp3 candidate.wav --segments segments.csv --out comparison.json
python3 analysis/run_calibration_suite.py reference.mp3 manifest.csv out/ --renderer build/p6_render_events_cal
```

## Command-line tools

```bash
p6_inspect bank_or_patch.syx
p6_sequence_dump bank_or_patch.syx [message-index]
p6_render patch.syx output.wav [midi-note] [slop|vintage]
p6_render_events patch_or_bank.syx events.csv output.wav [message-index] [slop|vintage]
p6_render_cal patch.syx output.wav midi-note [calibration options]
p6_render_events_cal patch.syx events.csv output.wav [calibration options]
p6_sync_probe output.wav [sync diagnostic options]
p6_polymod_probe output.wav [Poly-Mod diagnostic options]
p6_sync_microscope trace.csv [cycle-level hard-sync options]
p6_timing_probe output_directory
p6_dsp_fingerprint
p6_perf_benchmark
```

## Build and test

Core only:

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

VST3 with JUCE 9.0.4 fetched automatically:

```bash
cmake -S . -B build-plugin -DP6_BUILD_PLUGIN=ON -DP6_FETCH_JUCE=ON
cmake --build build-plugin --config Release --target P6CorePlugin_VST3 -j2
ctest --test-dir build-plugin -C Release --output-on-failure
```

The release CI builds Windows and Linux VST3 targets, runs core/state/control/realtime/v0.18 FX tests, emits DSP fingerprints and a Linux performance snapshot, runs Linux ASan+UBSan, validates the plug-in with `pluginval --strictness-level 10`, and uploads validated artifacts.

## Priorities after v0.18

1. **v0.19 sound matching:** obtain controlled Prophet-6 hardware captures and use them as primary calibration evidence.
2. Calibrate oscillators/PW/mixer, hard sync, HPF/LPF, ADSRs, glide, Slop/Vintage, distortion, modulation depths and the v0.18 FX candidates.
3. Continue CPU optimisation only from profiling data and protect every material change with DSP fingerprints and Reference-A/hardware comparisons.
4. Keep later-OS FX selector encoding, exact LFO Sync divisions, CC4, swing ratio, chord-memory storage and sequence opaque semantics unguessed until stronger evidence exists.
5. Keep the Generic JUCE editor as a diagnostic fallback. Bespoke UI/preset workflow is v0.20 work.
6. The proposed reactive/character FX visuals — glowing tube, plasma discharge, vibrating springs, tape reels, optical lamp, ring-mod scope, ferrofluid and shimmer particles — are recorded in the parity matrix as **deferred UI exploration for v0.20+**, after the visual language and UI performance/accessibility contract are defined.

## Licensing

The project uses/adapts MIT-licensed implementation ideas/code from `joshjetson/phosphor`; attribution is maintained in `THIRD_PARTY_NOTICES.md`. `ShmKnd/Patina` was reviewed as an MIT-licensed topology/architecture reference for additional effects. GPL/AGPL projects are reference-only unless explicitly documented otherwise. Factory SysEx and third-party reference audio are **not redistributed** by this source package.

