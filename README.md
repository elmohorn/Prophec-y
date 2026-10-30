# P6 Core — phase 1 / v0.17

Compatibility-first C++20 core for a Prophet-6-style software synthesizer. The 1024-byte hardware program state remains canonical and lossless; DSP blocks are isolated so measured calibration can replace assumptions without breaking SysEx/NRPN compatibility.

## v0.17 headline — close the control/performance and realtime contract

v0.17 freezes the non-FX control/performance contract before digital effects and custom UI work begin.

- Host automation and MIDI NRPN edits now use a versioned lock-free last-writer-wins contract so stale MIDI mirrors cannot overwrite newer host edits.
- Sustain/Damper CC64 is modeled in the engine with physical-key state separated from sustained voice lifetime; generated arp/sequencer notes are not accidentally turned into a second HOLD mode.
- MIDI Volume CC7 is runtime gain only and never rewrites the saved 1024-byte Prophet program payload.
- CC121 Reset All Controllers restores modeled controller state; CC120 All Sound Off and CC123 All Notes Off have distinct channel-mode behavior.
- Realtime allocation tests are part of CI, and arpeggiator ordering no longer depends on heap-allocation-capable sorting.
- Synced-LFO first-key phase reset is implemented where evidence is strong. The exact Prophet-6 raw LFO-frequency-to-rhythmic-division law remains intentionally unguessed.
- Windows/Linux VST3, core/state/control/realtime tests, `pluginval --strictness-level 10`, and Linux ASan+UBSan pass the v0.17 gate.

See `docs/V0_17_CONTROL_CONTRACT.md`, `docs/V0_17_CLOSEOUT.md` and `docs/PROPHET6_PARITY_MATRIX.md`.

## v0.16 headline — wrapper and release stability gate

v0.16 closed the infrastructure/stability phase before new musical behavior resumed.

- Windows and Linux VST3 builds were validated through the same release CI path.
- `pluginval --strictness-level 10` passed on both release targets.
- Linux ASan+UBSan remained green.
- Release-mode tests were hardened so checks do not disappear under `NDEBUG`.
- Chord-memory held-key behavior and serialized parameter layout/cardinality regressions were fixed and covered by tests.

See `docs/V0_16_STABILITY.md`.

## v0.15 headline — realtime-safe host/MIDI bridge before the first linked VST3

v0.15 hardens the v0.14 JUCE adapter rather than adding another synthesis subsystem. A new JUCE-independent `P6MidiRouter` owns standard Prophet NRPN semantics and the wrapper now has explicit host/UI vs audio-thread patch ownership.

- Complete NRPN streams (CC99/98 + CC6/38), Data Increment/Decrement (CC96/97), performance MIDI and system realtime are routed through one core component.
- MIDI NRPN edits reach `audioPatch_` at their exact MIDI sample position; fixed atomic mailboxes mirror the final values back to canonical/UI state.
- JUCE `AsyncUpdater` was removed from the audio path after confirming JUCE's own warning that posting its message may block. A message-thread `Timer` polls the mirror instead.
- Host automation applies individual NRPN changes to both patch domains; it no longer replaces the whole audio patch and therefore cannot erase a newer MIDI edit that has not yet reached the UI mirror.
- Full state/SysEx loads use `setValueNotifyingHost()` so APVTS's denormalised `getRawParameterValue()` state remains coherent.
- Official control NRPN 1088 (`Seq Play/Stop`) is implemented as runtime state and never written into the 1024-byte program payload.
- Live sequence note/velocity NRPN edits refresh the active sequencer while preserving step and transposition.
- VST3 is now the default plug-in format; Standalone is opt-in with `P6_BUILD_STANDALONE=ON`. The wrapper links the narrower `juce_audio_processors` module.
- CI builds Linux/macOS/Windows VST3 targets, runs CTest + `pluginval --strictness-level 10`, and uploads validated artifacts.

See `plugin/README.md`, `docs/WRAPPER_0_15.md`, `docs/V0_15_CHANGELOG.md` and `docs/VALIDATION_0_15.txt`.

## v0.14 headline — thin JUCE/VST3 host adapter, not a second synth engine

v0.14 adds an optional JUCE wrapper around the existing core instead of implementing VST3 platform glue from scratch. The wrapper follows JUCE 9.0.4's maintained CMake/`AudioProcessor` architecture; `P6PatchState`, `P6PerformanceScheduler` and `P6Engine` remain the only synthesis/timing sources of truth.

- `P6_BUILD_PLUGIN=OFF` remains the default: the core has no JUCE dependency.
- `P6_BUILD_PLUGIN=ON` builds VST3 + Standalone through `juce_add_plugin`. JUCE can come from `P6_JUCE_PATH`, an installed CMake package, or pinned FetchContent (`P6_JUCE_TAG=9.0.4`).
- MIDI note/CC/channel-pressure/pitch-bend/system-realtime messages are consumed at their `MidiBuffer` sample offsets. Host BPM/PPQ is read once per block and fed into the v0.13 host-anchor clock; the wrapper does not implement another clock.
- Host parameters expose verified hardware-domain integers plus a few wrapper/runtime controls. APVTS is a host-facing registry only; it never replaces the canonical 1024-byte hardware state.
- Host state uses a small JUCE-independent, versioned binary codec. Regression tests prove that arbitrary/unknown bytes survive save -> restore bit-for-bit.
- Runtime chord memory survives ordinary host automation updates; a true program/preset load still clears it, matching the distinction introduced in v0.12.
- Oversampling selection is persisted as a setup/quality preference but is only applied during `prepareToPlay()`, because rebuilding the decimator may allocate and therefore must not happen in the realtime callback.
- The temporary UI is JUCE's `GenericAudioProcessorEditor`; bespoke UI work is deliberately deferred until the host contract is stable.

JUCE/VST3 integration details, limitations, and licensing are documented in `plugin/README.md` and `docs/WRAPPER_0_14.md`.

## v0.13 headline — one performance scheduler for internal, host and MIDI-clock playback

v0.13 adds the Prophet-6 clock/arpeggiator/stored-sequencer layer without moving musical timing into the DSP or inventing unknown firmware behavior. The same `P6PerformanceScheduler` sits above `P6Engine` for standalone use and for the future VST wrapper.

- All ten documented CLOCK/VALUE divisions are represented. Internal timing uses a fractional sample accumulator, host timing can re-anchor from PPQ at a block boundary, and MIDI Timing Clock uses the MIDI-standard 24 pulses per quarter note. Start/Continue/Stop are exposed without hard-coding the Prophet Global `In` versus `In, No Start/Stop` policy into the clock itself.
- The arpeggiator implements Up, Down, Up+Down, Random and Assign plus 1/2/3-octave range and the documented HOLD relatch behavior. Assign has a separate 32-press event scratchpad so repeated pitches are not lost by the held-key set. The exact Prophet Random RNG is not claimed.
- `P6SequencerPlayer` plays the decoded six-track / 64-step payload, uses middle C as the transposition reference, honours the available sequence-length NRPN state, and allows live notes to play alongside the sequence. Sequence playback suppresses arpeggiator generation as documented.
- Generated notes carry source ownership (`live`, `ARP1`, `SEQ1`) so a sequence/arp note-off cannot kill an identically pitched live note. This follows the note-ownership pattern used by mature schedulers rather than relying on pitch-only note-off matching.
- Arp Beat Sync is implemented as the later Prophet Global behavior: OFF retriggers immediately from the key/chord change; ON waits for the current clock grid.

Several values remain intentionally **opaque rather than guessed**: the 8th/16th swing long:short ratio, sequence velocity bit 7, sequence mode/play-mode semantics, and the exact Random generator. Swing selections currently preserve their documented average division but use straight spacing until measured hardware timing is available. See `docs/TIMING_SEQUENCER_0_13.md`.

## Compatibility / state

- Full 1024-byte Prophet-6 program payload preserved losslessly.
- Correct Program Data `0x02` vs Edit Buffer Data `0x03` handling.
- Sequential 7-bit pack/unpack and concatenated-bank scanner.
- Known serialized one-byte parameters mapped to their NRPN identities while unsupported bytes remain untouched.
- Stateful NRPN decoder.
- **NRPN edits now update the canonical raw payload**, so decode -> edit -> encode -> decode retains the edit.
- Program-name NRPNs `236..255` update payload bytes `107..126` and `P6PatchState::name`.
- Sequence note/velocity NRPNs `256..1023` update the sequence bytes in-place.
- Sequence velocity edits preserve the hardware's currently-unidentified high-bit metadata.
- Standard Slop vs Vintage remains a separate Global choice because that mode is not stored in an individual program SysEx.

## Stored sequencer

The raw program payload contains the 64-step / six-track sequence at bytes `128..895`:

```text
track 1: notes 128..191, velocity/flags 192..255
track 2: notes 256..319, velocity/flags 320..383
...
track 6: notes 768..831, velocity/flags 832..895
```

`P6Sequence` decodes all 384 cells without destroying unknown metadata. Each stored velocity byte is exposed as:

- low seven bits: MIDI velocity candidate;
- high bit: preserved opaque hardware flag.

Factory-bank inspection shows that the high bit is meaningful, but its exact tie/retrigger semantics are not yet independently established. v0.6 therefore **preserves it rather than guessing**.

The official sequence note/velocity NRPN namespaces are mapped as well. Sequence controls `168..171` are retained as NRPN state but are not written into guessed SysEx offsets until those offsets are independently verified.

## DSP core inherited from v0.4

- Continuous triangle -> saw -> pulse trapezoid-family oscillator morph.
- Two-sided polyBLAMP corner correction.
- Sub-sample hard sync, Osc2 master -> Osc1 slave.
- Free-running VCOs.
- Sub oscillator follows the actually-modulated Osc1 frequency.
- Independent slow random-walk Slop per VCO/per voice.
- Four-pole SSM2040-lineage candidate LPF with self-oscillation.
- Resonant TPT two-pole HPF.
- Candidate path: mixer -> HPF -> LPF -> VCA.
- RC-style exponential ADSR.
- Global audio-rate-capable LFO and channel aftertouch routing.
- Default 2x internal oversampling of the per-voice nonlinear/audio-rate path with the original 31-tap clean-room FIR decimator; selectable 1x/4x/8x diagnostic modes were added in v0.10.
- Post-sum analog-distortion candidate.
- Separate Standard Slop and Vintage behavior.

## Serialized mapping corrections in v0.5

- Effect A stored type domain tightened to `0..5`; Effect B to `0..9`, matching the factory-bank validation domain.
- Serialized aftertouch destination byte order corrected: NRPN 114 (VCA) maps to raw byte 74 and NRPN 113 (HPF) maps to raw byte 75. This intentionally differs from a naive assumption that serialized byte order must equal printed NRPN order.
- Oscillator coarse remains `raw - 24` semitones; raw 24 is the current evidence-backed 8'/reference position.

## Reference-A calibration harness

The `analysis/` directory now contains the reference index/comparator plus patch-aware inference and suite automation:

```bash
python3 analysis/reference_index.py reference.mp3 \
  --names data/factory_program_names_2015.txt \
  --csv segments.csv --json segments.json

python3 analysis/compare_audio.py reference_segment.wav candidate.wav \
  --out comparison.json

python3 analysis/compare_program.py 7 reference.mp3 candidate.wav \
  --segments segments.csv --out comparison.json

python3 analysis/infer_performance.py reference.mp3 patch.syx events.csv \
  --segments segments.csv --program 24 --report-json inference.json

python3 analysis/run_calibration_suite.py reference.mp3 manifest.csv out/ \
  --renderer build/p6_render_events_cal
```

The comparison deliberately avoids a misleading single "accuracy percentage". It reports separate measurements including log-band spectral error, spectral-shape cosine, centroid, roll-off, envelope shape, duration, crest factor and stereo width/correlation.

The reference indexer fits a 500-program time grid to an online factory-preset recording, then locally refines boundaries using acoustic novelty. The resulting boundaries are **candidate alignment points**, not assumed ground truth; important diagnostic presets should be manually refined before fitting DSP constants.

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
```

`p6_render_events` accepts rows of:

```csv
time_s,midi_note,velocity_0_to_1,duration_s
0.000,60,1.0,2.250
2.262,63,1.0,1.088
```

This lets a reference performance be reconstructed without changing the synth DSP just to fit one recording.

## Reference-A status

The v0.9 named calibration starts from the v0.8 LPF/envelope + independent HPF candidate and adds the current Poly-Mod span hypotheses:

```text
LPF base:                   88.97695 Hz
LPF raw slope:               0.37 semitone/raw
filter envelope depth:      13 octaves
filter envelope exponent:    3.0
filter ADSR max:              2.5 s
LP resonance scale:           0.70

HPF candidate base:          40 Hz
HPF candidate top:           20 kHz
HPF candidate curve exp:      4.0

Poly Mod -> Osc1 pitch:       4 octaves
Poly Mod -> Osc1 shape:       1.0 morph unit
Poly Mod -> Osc1 PW:          0.5 duty
Poly Mod -> LPF/HPF:          7 octaves
```

This is exposed as `referenceACandidateV09()` / `--reference-a-v09`. The experimental hard-sync reset/trigger-phase and Osc2-polarity controls remain at their legacy values in that preset because the Reference-A sweeps did not identify a stable improvement.

The hard-sync calibration renderer accepts `--sync-reset-phase`, `--sync-trigger-phase`, and `--osc2-mixer-polarity` for controlled experiments. `analysis/sync_diagnostics.py` estimates the master period by autocorrelation and reports per-harmonic differences so a broadband EQ/filter change cannot masquerade as a corrected sync waveform.

The general calibration renderers also accept `--pre-roll-seconds`, `--pulse-normalization`, `--pw-inset`, `--pw-exponent`, `--mixer-exponent`, `--sub-gain`, `--noise-gain`, the LPF/HPF/envelope/Poly-Mod overrides, and the v0.10 isolation/oversampling flags documented by the usage text.

## Build and test

Core only (no JUCE dependency):

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

VST3 (JUCE 9.0.4 fetched automatically if no local/installed JUCE is found):

```bash
cmake -S . -B build-plugin -DP6_BUILD_PLUGIN=ON -DP6_FETCH_JUCE=ON
cmake --build build-plugin --config Release --target P6CorePlugin_VST3 -j2
ctest --test-dir build-plugin -C Release --output-on-failure
```

The test suite covers SysEx round trips, canonical NRPN edits, sequence decode/encode, preservation of sequence high-bit metadata, serialized mapping identities, oscillator/sync behavior, Slop, filters, LFO, aftertouch, voice allocation, pitch bend, Mod Wheel depth, the six Unison key/retrigger modes, runtime chord-memory transposition, all ten clock divisions, host-PPQ and MIDI-clock timing, arpeggiator patterns/HOLD/Assign repeats, conservative sequence playback and source-owned generated notes. v0.17 additionally gates causal host/MIDI ordering, sustain lifetime, MIDI Volume/reset semantics, MIDI channel-mode behavior, synced-LFO first-key reset and realtime no-allocation behavior.

## Priorities after v0.17

1. Start v0.18 with one shared realtime-safe serial FX A -> B architecture using the existing common clock for tempo-synced effects.
2. Reuse/adapt mature license-compatible delay/chorus concepts where appropriate; do not duplicate timing or state infrastructure already present in the core.
3. Keep LFO Sync division mapping, CC4, chord-memory storage, swing ratio and sequence opaque semantics unguessed until stronger Prophet-6 evidence or controlled hardware captures exist.
4. Preserve the generic JUCE editor as a diagnostic surface; bespoke UI and preset-browser work remain a later milestone after FX and sound-matching contracts stabilize.
5. Continue real-host DAW soak testing and project save/reload coverage as beta-hardening work without reopening the v0.17 feature boundary.
6. Return to hard-sync/Poly-Mod/filter/envelope calibration when dry hardware captures with known settings are available.

## Licensing

The project contains adapted MIT-licensed implementation ideas/code from `joshjetson/phosphor`; attribution is in `THIRD_PARTY_NOTICES.md`. GPL projects were used only as architectural/protocol references unless explicitly stated otherwise. Factory SysEx and third-party reference audio are **not redistributed** by this source package.