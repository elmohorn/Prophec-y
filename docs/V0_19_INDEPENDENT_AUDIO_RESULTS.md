# v0.19 — actual comparisons with newly acquired independent audio

Date: 31 October 2026. Renderer baseline: `ab5170d2de9463a4ee4b9bb31917108bf48c944a`.
Status: **two independent-source WAVs acquired; 24 new renderer comparisons completed**.
No engine or parameter-mapping defaults changed.

Validation: all 15 local analysis regressions passed, including full-decode/error-page guards and the new synthetic plateau/constant-tone tests. The preparation command also verified the acquired originals against their frozen hashes and decoded both WAVs. Audition WAVs were checked at 48 kHz, 8.6 seconds, equal RMS of -24 dBFS, with peaks below 0.52 (no clipping).

## Result

The existing `filter_env_depth12_v019` candidate improves both spectral metrics against the newly acquired customized Dark Octave Bass recording in every paired pre-roll/gate realization. This is a small, consistent improvement on **one additional recording and patch**. It is not a controlled calibration or eight new held-out presets. The author posted the recording to investigate possible intermittent hardware glitches; no normal-hardware transfer law is inferred from that material.

All numbers below are medians over four shared pre-rolls; smaller is better. The three gates are explicit uncertainty hypotheses for the same six attacks, not three independent performances. No gate was selected by its score.

| Assumed gate | MR spectral MAE, current → depth12 (dB) | Broad-band shape RMSE, current → depth12 (dB) | Envelope MAE, current → depth12 |
|---|---:|---:|---:|
| 250 ms | 11.1845 → 11.0574 | 17.6875 → 15.9860 | 0.1789 → 0.1797 |
| 500 ms | 11.0032 → 10.8754 | 17.7370 → 16.0113 | 0.1809 → 0.1812 |
| 750 ms | 10.8731 → 10.7649 | 17.8405 → 16.0912 | 0.1857 → 0.1857 |

Across all **12 paired comparisons / 24 renders**, MR spectral error improves 12/12 and broad-band shape improves 12/12. Maximum paired envelope-MAE worsening is 0.000918. These rows are sensitivity checks on one source, not independent statistical successes. Scores are not fidelity percentages. Overall spectral/envelope mismatches remain substantial, and unknown performance/FX history prevents attributing the residual specifically to the filter law.

## New audio and exact patch identity

1. [Sequential forum: customized Dark Octave Bass](https://forum.sequential.com/index.php/topic,2772.0.html), author `prophetic`, 2018. Original WAV: 22.638865 s, stereo, PCM24 at 96 kHz, 13,057,746 bytes. The author supplies the associated 1178-byte Prophet-6 SysEx. The decoded patch retains the Dark Octave Bass name but differs from the official factory program in **161 raw bytes**; it includes sync, glide and effects. The exact attachment was used, with no synth edits to improve its match. The first 8.6 seconds contain six clear attacks used for the comparison. No specific attack was excluded because of its comparison score or alleged glitch.
2. [Sequential forum: self-oscillating filter sweep](https://forum.sequential.com/index.php?topic=274.0), author `Christopher`, 2016. Original WAV: 19.490612 s, mono, PCM16 at 44.1 kHz, 1,719,838 bytes. The source identifies it as a Prophet-6 manual cutoff sweep. There is no accompanying synchronized knob trajectory or SysEx, so it is used for relative frequency-step analysis only.

Original-file, decoded-PCM, excerpt and SysEx hashes plus the exact attachment URLs are in `calibration/v019_independent_audio/input_protocol.json`. Both WAVs passed complete decoding. Third-party WAV/SysEx bytes remain outside the repository.

## Performance and controls: unresolved uncertainty is retained

Inputs were frozen before rendering: six approximately located onsets at 0, 1.73, 3.00, 4.35, 5.99 and 7.33 s; repeated MIDI 60; assumed velocity 1; and three gate durations (250/500/750 ms). The patch-aware pitch tracker supported C4 through the dominant regions, but merged repeated attacks and release/FX tails into long held events. Its full-file event output was therefore rejected as MIDI ground truth. The first onset is truncated by the source file boundary; onset timing, gates, physical key velocity and prior FX-buffer history are unknown.

The comparison retains original patch bytes, the same pre-rolls (0, 0.5, 1.5, 3 seconds), 2x oversampling, stereo-power analysis, bounded envelope alignment and RMS matching. Neither time stretching nor best-phase selection is used. The original WAV was not RMS-normalized; its interface/master gain is unknown, so absolute output-gain fitting is still unjustified. Envelope scores here cannot establish envelope timing accuracy.

This is **trust C, exploratory evidence**. It neither discharges the preregistered eight-program holdout nor satisfies the independent controlled-recording requirement for promotion. It does add a real comparison beyond the original five excerpts and recording.

## What the self-oscillation recording shows

The analysis finds **43 stable plateaus** and 31 eligible adjacent-plateau transitions in the 800–6500 Hz analysis region. Of these, 27 are near one semitone (median **101.30 cents**, range 95.20–106.57 cents) and four near a whole tone. The latter can include skipped observed plateaus. The windows, quality thresholds, full frequency trace and transitions are published with the data.

This supports approximately semitone-spaced observable cutoff plateaus in this recording. Without raw knob values or their timing, it cannot establish the absolute Hz/raw slope, smoothing time constant, complete cutoff range, filter-envelope depth or behavior of other firmware versions. No synth parameter was fitted to this sweep. The tracker was independently checked against eight synthetic semitone plateaus (all seven intervals within 2 cents) and a constant tone (no invented transitions).

## Reproduce

Run from the repository root with numpy/scipy and ffmpeg/ffprobe:

```sh
python3 tools/prepare_independent_forum_suite.py
python3 tools/compare_online_suite.py calibration/v019_independent_audio/manifest.csv --renderer build-check/p6_render_events_cal --profiles current,filter_env_depth12_v019 --source-sha ab5170d2de9463a4ee4b9bb31917108bf48c944a --output reference_private/independent_forum_comparison
python3 tools/analyze_selfosc_recording.py reference_private/acquisition/forum_filter_sweep.wav --output reference_private/selfosc_analysis
```

The preparation tool rejects changed/error responses using original hashes and full decoding. ffmpeg container metadata may differ across versions; comparison provenance records actual regenerated excerpt hashes. Published `comparison_provenance.json` corrects the older harness's recovery-specific generic caveats to describe these original, non-normalized recordings accurately. Measurements and all twelve paired deltas are in `calibration/v019_independent_audio/`.

## Acquisition limits encountered

Library search/inventory did not recover the old complete Reference-A MP3. The previous YouTube media response was HTML, not audio. Hazy Prophecies publicly advertises a free sound bank and matching demos, but its package download returned HTTP 403; the older `programmed patch` Drive links returned not found. PresetPatch file access also returned HTTP 403. None of these failed acquisitions is counted as a comparison, and mixed-instrument VCO packs were excluded. The seven already verified official demos remain useful for listening but lack exact SysEx/MIDI pairings.

## Decision

Keep depth12 as the leading audition candidate; the new exploratory source corroborates a small spectral improvement but does not justify promotion. Keep the eight genuinely unused programs frozen until verified matching audio is available. The functional Unison fix remains separate and validated. Next DSP investigation can isolate Sync Stack's mixer/filter contributions using the existing trustworthy excerpt; these new recordings do not justify a global hard-sync or filter rewrite.
