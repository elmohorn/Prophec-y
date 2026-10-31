# v0.19 — new references and frozen filter-depth holdouts

Date: 31 October 2026. Engine baseline: `23dc4d7d0e641f5fcdd7aa25738bf30c5c3a1bde`.
Status: **seven official audio files acquired and decoded; quantitative holdouts registered, not run**.

## What was acquired

The current [Sequential Prophet-6 page](https://sequential.com/modern-analog/prophet-6/) provides six individual examples: Buzzy Pad, Polymod Pad, Big Bass, Classic Sync Lead, Detune Unison Lead and Big Pad. All six downloaded successfully. The Peter Dyer song on the same page also downloaded successfully; Sequential identifies all its sounds, including drums, as Prophet-6.

`calibration/official_reference_media_v019.json` records their actual file sizes, SHA-256 hashes, decoded durations, channels, sample rates, decoded PCM hashes and signal statistics. Every file was decoded in full with ffmpeg error checking. The audio stays in the private reference directory; the manifest contains public source URLs for reacquisition. RMS and peak describe these distributed files, not calibrated hardware output gain.

These are additional real-instrument references for listening and behavior checks. Neither the page nor the acquired files establishes an exact SysEx/MIDI pairing. A name such as "Big Bass", or `nofx` in a filename, does not verify factory-program identity or an effects setting. They therefore **do not validate a 12-octave filter law**, and no blind numerical fitting is performed against them. The song is especially unsuitable for component fitting because it combines musical parts.

## Acquisition failure caught

YouTube metadata for `HQe1QxexfSs` was accessible, identifying Wolfgang S Vukan's factory-preset demo and a nominal duration of 2210 seconds. Its apparent media download, however, was a **195-byte HTML "Site Unavailable" page**. It fails both format probing and decoding. No extra Reference-A programs were recovered from that response. Fixing the downloader's trust-store selection did not recover the media; TLS verification remained enabled.

`tools/inspect_reference_media.py` now requires an audio stream, full decode, finite samples, nonzero signal and minimum duration. An optional expected-duration gate catches valid but incomplete/wrong-duration media. A successful HTTP status, file suffix or downloader exit code is insufficient. This verifies file integrity only; authenticity, patch identity and performance remain separate evidence gates.

Example:

```sh
python3 tools/inspect_reference_media.py reference_private/official_demo.mp3 --min-duration 120
python3 tools/inspect_reference_media.py reference_private/reference_a_full.webm --expected-duration 2210 --duration-tolerance 1
```

The second command must reject the failed response. Synthetic regressions exercise a disguised error page, a complete tone decode and an expected-duration mismatch. CI installs ffmpeg and runs these checks.

## Freeze the next test before seeing its scores

`calibration/filter_depth_holdout_v019.json` registers eight previously unused factory programs: **002, 006, 036, 043, 069, 115, 177 and 293**. Selection used their original SysEx fields, without new reference audio or candidate measurements: effects off, distortion off, polyphonic, sync off, low noise/slop and a nonzero filter-envelope amount. The bank hash, individual program hashes and relevant raw fields are frozen with the selection.

The comparison is fixed to `current` versus the existing `filter_env_depth12_v019`, at 2x oversampling and four shared pre-rolls (0, 0.5, 1.5, 3 seconds): **64 planned renders if all eight programs qualify**. These are planned, not completed renders. Four pre-rolls of one program do not count as four independent holdouts.

Before rendering either model, verify segment boundaries and patch identity, freeze reference hashes and performance events, and record ambiguity, unknown velocity/gate limits and any exclusions. The historical coarse time grid is a locator, not a verified boundary. The monophonic tracker cannot justify a polyphonic transcription. Exclusions must precede candidate scores; do not replace a disappointing holdout or shorten gates to improve its score.

Publish paired spectral, broad-band-shape and envelope results for every eligible program, including sensitivity across pre-rolls. No further depth/resonance search is allowed on this set. These new programs would test generalization across patches within Reference A; independent controlled recordings with known states/performance are still required before changing defaults. The newly acquired official demos do not meet that requirement.

## Next executable dependency

The quantitative test is blocked on a valid source recording or matching excerpts plus verifiable patch/performance information. It is not blocked on additional parameter sweeps. The existing five-excerpt calibration and the Unison fix remain documented in `V0_19_UNISON_FIX_AND_RECALIBRATION.md`; no DSP defaults were changed in this step. Once eligible audio is available, run the frozen two-profile comparison, then return to the separately scoped Sync Stack mix/filter investigation.

## Validation

[CI run 41210046396](https://github.com/elmohorn/Prophec-y/actions/runs/41210046396), for the baseline commit above, completed successfully: Linux and Windows builds/tests/VST3 validation, Linux ASan+UBSan, and online-reference guards. This confirms the previous functional fix and recalibration baseline; it does not claim that the new files were covered by that older run.

Locally, all 13 analysis regressions passed (five measurement, four Fourier-oracle, two event-uncertainty and two media-integrity tests). All seven acquired references passed full decoding. The next CI run includes the new media tests. No C++ engine or mapper code was modified here.
