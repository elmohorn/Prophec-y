# DSP quality audit — 2026-11-07

Scope: evidence-backed response to five proposed directions (SIMD/SoA,
oversampling, automation smoothing, modulation matrix, formats and CI).
This branch **does not change the audio algorithm, parameter IDs, SysEx,
preset layout, runtime latency, or host automation contract**. Its first
implementation is expanded `p6_perf_benchmark` coverage (1x/2x/4x/8x).

## Current implementation, verified from source

- `P6Engine` owns **six** `P6Voice` instances, each with independent
  oscillator phase, glide, noise, envelope, filter and poly-mod state.
  Processing is scalar per voice and per sample.
- `P6Voice::prepare()` runs the *complete* voice signal path at selectable
  1x / 2x / 4x / 8x internal rates. `process()` uses a 31-tap 2x FIR
  decimator and windowed sinc FIR taps for 4x/8x. The plugin's default
  `DSP Oversampling` choice is **2x**. `CharacterFx` also runs Tube at
  2x oversampling. Adding an identical filter oversampling wrapper would
  duplicate work and change calibration.
- `P6Engine::applyCandidateDistortion()` is a **separate, global stereo
  nonlinear stage after summing six oversampled/decimated voices**. This
  particular stage remains at host sample rate and is the first meaningful
  *candidate* for an aliasing experiment.
- `PluginProcessor::processBlock()` processes MIDI events at
  `juce::MidiBuffer::Iterator::samplePosition`, but reads APVTS host
  parameters and updates mapped patches **once at block entry**.
  `OutputStage::setDecibels()` already ramps the master output over
  20 ms (tested by `p6_output_stage_tests`); Tube/Character also has
  parameter crossfade handling. Therefore a blanket 'no smoothing'
  diagnosis is false, but cutoff and aggressive host automation still
  merit dedicated tests.
- PolyMod/LFO and aftertouch destinations are explicit typed routes,
  not a general matrix. Hardware routing fidelity and SysEx mapping are
  intentional.
- `plugin/CMakeLists.txt` currently builds **VST3** (and optional
  Standalone). `plugin-ci` builds Windows and Linux, runs native UI,
  pluginval, real-time tests and DSP fingerprint; a macOS runner,
  Apple signing and CLAP are not yet configured.

## Decisions

| Proposal | Decision | Reason / gate |
| --- | --- | --- |
| SIMD / 4–8 lane SoA | **Benchmark first; do not rewrite** | Six heterogeneous voices and branch-heavy audio-rate PolyMod make 8-lane efficiency uncertain. Rewriting a proven scalar path risks changing phase/state semantics. Existing performance benchmark now includes dry/effected 4x/8x six-voice points. First run repeatable scalar baselines on x86_64 and ARM64 with identical audio fingerprints; then prototype xsimd/Highway on a *separate* branch, and accept only a reproducible net win with negligible regression. |
| 2x/4x oversampling around filter | **Already done; do not duplicate** | Complete voice oversampling 1/2/4/8 exists. Profile the low-pass filter at those factors, including resonance sweeps. |
| Oversampling around global distortion | **Investigate next** | Global `applyCandidateDistortion` is currently after downsampling. Measure its output aliasing at 44.1/48/96 kHz with 1–6 voices; compare off/2x/4x distortion-only candidates including real filter CPU and latency. No implicit extra PDC or unconditional tone change. |
| Smoothing / sample-accurate automation | **P1: test host ramps and fix selectively** | Master gain already ramps; MIDI offset is sample-granular. APVTS host automation is block-boundary in this wrapper. Build reproducible 64/128/512 sample host-automation capture with cutoff, resonance, drive and pitch. Add bounded per-sample smoothing to actual audible discontinuities (select tau by control type; never smear note gates or sample-accurate MIDI). Sample-accurate VST3 queues need host-wrapper event delivery, not merely a one-pole filter. |
| Generalized modulation matrix | **Defer / keep hardware graph fixed** | A universal dynamic matrix would complicate the calibrated Prophet-6 routes and SysEx contract. Introduce typed internal destinations only when a real second consumer needs them; keep extended modes isolated from hardware patches. |
| CLAP and macOS Universal signed releases | **Post-UI/engine stabilization** | JUCE 9.0.4 does not expose a native CLAP plugin client; evaluate `clap-juce-extensions` (MIT) and clap-validator as a separate spike. First add unsigned macOS x64/ARM64 smoke jobs when a runner is available, then AU validation. Signing/notarization requires valid Apple credentials/secrets; never invent signed binaries. |

## Concrete verification plan / stop conditions

1. **CPU baselines:** `p6_perf_benchmark` now measures 1x, 2x, 4x
   and 8x six-voice 48 kHz cases (dry vs FX) and existing 96 kHz cases.
   Capture at least three runs on matched hardware, compiler flags and
   thermals. Separate six-voice DSP cost from the shared stereo FX.
   Compare one-core CPU%, mean and worst-block CPU time; do not use
   uncalibrated GitHub Actions runners as a hard performance gate.
2. **Aliasing:** establish the base reference from a fixed patch and
   deterministic offline rendering before proposing distortion oversampling.
   Compare non-harmonic foldback (dBFS), passband response, gain, latency
   and transient ringing at 44.1/48/96 kHz. Any candidate must maintain
   prior regression suite and hardware-reference similarity.
3. **Automation:** replay time-stamped ramps into 64/512-sample host
   buffers; collect max inter-sample discontinuity, audible zipper sidebands
   and CPU overhead. Preserve the original host parameter ID/index and
   state codec. A DSP smoother is **not** equivalent to preserving a
   host-supplied VST3 sample-offset queue.
4. **Regression:** for every audio-affecting proposal run
   `p6_realtime_contract_tests`, `p6core_tests`, `p6_dsp_fingerprint`,
   full plugin-ci, pluginval, state/SysEx tests, and compare against actual
   Prophet-6 audio captures; no new heap allocation in process paths.

## Order of execution

1. Expand objective CPU data (done in this branch; no DSP change).
2. Add aliasing and automation experiments in independent feature branches.
3. Evaluate an optional **global distortion-only** oversampling path
   if the recordings support a material benefit.
4. Explore SIMD/SoA only if measured CPU bottlenecks justify its risk.
5. Expand CLAP/macOS/CI once the v0.20 visuals and engine behavior stabilize.

This audit is intentionally separate from PR #7 (the Character RGBA UI work).
Do not merge a DSP experiment into the photo/renderer PR.

## External context (research links)

- JUCE 9.0.4 change list: https://github.com/juce-framework/JUCE/blob/master/CHANGE_LIST.md
- JUCE VST3 sample-offset queue limitation discussion:
  https://forum.juce.com/t/sample-accurate-automation-hand-the-plugin-the-offset-of-every-automation-point-vst3-not-the-blocks-last-value/69503
- CLAP specification and validation: https://github.com/free-audio/clap
- JUCE CLAP extension: https://github.com/free-audio/clap-juce-extensions
