# v0.19 — independent single-oscillator waveform references

Date: 31 October 2026. Core baseline: `3e389b20a00a63e811a10f3e17d0978475cdd0eb`.
Status: **five new publisher audio files acquired; 40 direct waveform-probe renders completed**.
No production oscillator, mapper, filter, factory program or acoustic default was changed.

## The useful result

The current saw endpoint already tracks the new capture's low-order harmonic profile closely. The captures labeled triangle and square contain appreciable even harmonics that ideal symmetric endpoints lack. Crucially, fixed nearby settings of the **existing** morph/pulse controls reproduce much of that difference across registers. The nominal endpoint comparison alone therefore does not establish that a new oscillator/reset implementation is needed.

This is better constrained than the old six-voice Sync Stack excerpt, but it is **not a fully known-state hardware measurement**. The source identifies the oscillator and waveform classes, without publishing all analog-path controls or a paired SysEx/MIDI file. No isolated hard-sync recording with verified master/slave intervals was acquired. Hard-sync/reset identification and the eight-program holdout remain open.

## Source and acquisition

[Lasse Eilers, Bonedo, 24 August 2015](https://www.bonedo.de/artikel/dave-smith-instruments-sequential-prophet-6-test/) identifies a sequence of Oscillator 1 triangle, saw and square examples, plus a separate pulse-width demonstration. The accompanying LPF, HPF and self-oscillation examples were also recovered for future analysis.

| File | Codec | Duration | Use in this phase |
|---|---|---:|---|
| `Sequential_Prophet_6_Waves.mp3` | MP3, stereo 44.1 kHz | 38.133810 s | Twelve stable register windows; quantitative comparisons |
| `Sequential_Prophet_6_PW.mp3` | MP3, stereo 44.1 kHz | 15.878776 s | Acquired/inventoried; dynamic width not fitted |
| `Sequential_Prophet_6_LPF.mp3` | MP3 | See protocol | Acquired/inventoried; no cutoff-law fitting |
| `Sequential_Prophet_6_HPF.mp3` | MP3 | See protocol | Acquired/inventoried; no cutoff-law fitting |
| `Sequential_Prophet_6_Selfosc_1.wav` | PCM | 14.105442 s | Acquired/inventoried; no tuning-law fitting |

All five files passed original SHA-256 checks and complete ffmpeg decoding. Source URLs, durations, signal statistics and decoded-PCM hashes are in `calibration/v019_basic_waves/protocol.json`. The publisher page advertises malformed filesystem-style HQ waveform links; corrected sibling WAV URLs returned HTTP 403. They were not used or bypassed. The old Paul Dither single-oscillator SoundCloud link returned not found. The already rejected mixed-instrument VCO packs and unrelated synthesizers were not substituted.

MP3 decode can produce samples above full scale; the PW file has a decoded peak of 1.0218. That alone does not prove analog clipping. PW is not used to calibrate endpoint duty limits. The scored steady waveform windows have their actual peaks recorded separately.

## Freeze windows and exclude note tails before synthesizing

The three waveform groups were selected from the author's sequence. Notes near MIDI 48/60/72/84 (approximately 130/261/522/1043 Hz) were located from captured pitch, with 250 ms windows. Shape parameters were initially fixed to 0, 0.5 and 1; square duty 0.5. Comparison pitch follows each capture's measured F0; no timbre parameter is optimized per note.

Two initial locations failed pitch-quality checks **before the first candidate render**. Triangle n84 at 6.6 s and square n72 at 29.6 s contained previous-note tails: the suboctave was only -6.4 and +0.1 dBc respectively. Each start was moved forward by 200 ms and rechecked. Final acceptance requires no F0-search-boundary hit, less than 10 cents half-window drift, and suboctave power below -35 dBc. The revisions, final windows and quality measurements are retained. No window was rejected because of a model comparison score.

`p6_sync_probe --free-running` now renders a single free-running oscillator without accidentally testing a synced oscillator against an unsynced capture. Its default sync mode remains bit-identical to a binary compiled from the previous source under a compatibility probe. This is a tool option; it does not change the plugin.

## Comparisons and the control-uncertainty experiment

First, twelve windows × two oversampling factors (2x/8x) produced **24 nominal endpoint renders**. Harmonic powers are measured per channel and compared at each signal's own F0. Comparison partials must be below 10 kHz and at least -40 dBc in the capture.

Odd-harmonic error and even/odd power are reported separately. Symmetric model endpoints have mathematical even-harmonic zeros; an unfloored logarithmic error at those zeros would be dominated by floating-point numerical noise. The additional all-harmonic diagnostic floors levels at -60 dBc and is explicitly floor-dependent. It is not a fidelity percentage or a promotion score.

The lowest-register capture's H2/H1 suggests an equivalent triangle up-ramp fraction near 0.569, or its time-reversed counterpart; the existing model morph near 0.069 can produce that magnitude pattern. Likewise, square H2/H1 implies a duty near 0.515, or the mirrored duty. Fixed diagnostic controls **shape 0.07** and **square duty 0.515** were then registered and held across all four registers: **16 additional renders**. These are equivalent-model counterexamples, not recovered knob values or proposed global corrections. The nominal high-register scores had already been seen; the remaining-register check is not called a blind holdout.

Odd-harmonic RMSE at 2x, dB; lower means a closer profile:

| Nominal register | Triangle endpoint → fixed shape 0.07 | Saw endpoint | Square 50% → fixed 51.5% duty |
|---|---:|---:|---:|
| MIDI 48 | 4.206 → 0.229 | 0.068 | 1.204 → 0.089 |
| MIDI 60 | 4.477 → 0.496 | 0.141 | 1.755 → 0.582 |
| MIDI 72 | 4.962 → 0.981 | 0.846 | 3.782 → 2.611 |
| MIDI 84 | 5.692 → 1.712 | 1.218 | 3.026 → 2.598 |

The nominal triangle's captured even/odd power is -18.36 to -18.95 dB, while the perfectly symmetric digital endpoint has effectively zero even power. Fixed shape 0.07 yields -17.95 to -18.17 dB at 2x. The nominal square's even/odd power is -16.92 to -18.94 dB; fixed 51.5% duty reproduces much of it, with residual register variation. These discrepancies can reflect actual waveform shape, unknown controls, analog stages or voice differences; their origin is not identified from these files alone.

For the saw, the all-harmonic bounded error is 0.304–1.101 dB across the four registers. Changing 2x to 8x alters odd-error scores by at most 0.116 dB and bounded all-harmonic scores by at most 0.108 dB in the nominal set. Oversampling is not the dominant explanation for the observed triangle/square difference. Measured probe-to-reference F0 error is below 0.45 cents after pitch matching; this does not establish MIDI/master-tune accuracy on hardware.

## Decision and next dependency

Do **not** skew every triangle, offset every pulse center, or rewrite hard sync from this result. Nearby controls in the existing oscillator already supply a counterexample to the claim that a new waveform family is required. An exact endpoint-shape/duty capture with verified raw controls and analog-path settings is needed to distinguish a mapper correction from a source-setting difference. The square residual can also include voice-specific variation; no frequency-dependent duty law was fitted.

The next hard-sync test still needs known shape/PW, master/slave ratio, modulation, voice count and recording path. Sync Stack's unexplained mixed spectrum cannot supply those independently. The current production sound and provisional depth12 status remain unchanged. No whole-preset regression renders are claimed: no production sound code or default changed.

## Reproduce

```sh
python3 tools/prepare_basic_waveform_suite.py
cmake --build build-check --target p6_sync_probe
python3 tools/compare_basic_waveforms.py --probe build-check/p6_sync_probe --audio reference_private/acquisition/Sequential_Prophet_6_Waves.mp3 --windows calibration/v019_basic_waves/windows.csv --output reference_private/v019_basic_waves
python3 tools/compare_basic_waveforms.py --probe build-check/p6_sync_probe --audio reference_private/acquisition/Sequential_Prophet_6_Waves.mp3 --windows calibration/v019_basic_waves/control_uncertainty_windows.csv --output reference_private/v019_basic_wave_controls
```

`--analyze-only` reuses existing renders only when the stored audio/window/probe hashes match. The published data include all forty comparisons, both oversampling factors, quality ledgers, counterexample specifications and binary compatibility evidence. Audition files are level-matched and explicitly labeled as nominal or control-uncertainty examples.

## Validation

Full local C++ build and 8/8 CTest checks passed. All 17 existing Python analysis checks passed (5 online metrics, 4 Fourier oracle, 2 event uncertainty, 2 reference media, 2 self-oscillation, 2 stereo harmonic metrics). New scripts pass Python compilation and the patch passes whitespace validation. The original default sync probe produces byte-identical audio to the previous source. This phase does not claim a new VST3/pluginval run; no production DSP or mapper changed.
