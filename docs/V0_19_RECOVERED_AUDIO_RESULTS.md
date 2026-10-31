# v0.19 — Fresh comparison with recovered Prophet-6 audio

Date: 31 October 2026. Status: **completed experiment; milestone ACTIVE**.

## What actually ran

The renderer was compiled from branch `v0.19-sound-matching`, upstream baseline `3cc9329f96a7f2029996700de124cda34f793eb7`, with GNU C++ 13.3, C++20 and Release optimisation. No synthesis-engine sources were modified in this round.

**180 new renders = five excerpts × nine calibration profiles × four pre-roll realizations.** The earlier 96-render pilot is superseded by this final matrix. Sample rate 48 kHz, oversampling 2x. Every candidate for a case uses the same pre-roll grid: 0, 0.5, 1.5 and 3 s. The published result is the median/range across that grid, not a best-phase selection.

Recovered real-hardware excerpts:

| ID | Reference | Duration | Interpretation |
|---|---|---:|---|
| RA000 | Brassed Off | 4.674 s | v0.8 listening-pack excerpt; original source range starts at **13.390 s**; use the matching v0.7 inferred event CSV |
| RA024 | Solid Bass | 4.550 s | Same factory bank and inferred event reconstruction as the recovered suite |
| RA067 | Carpenter Square | 5.150 s | Pulse-heavy priority anchor; inferred performance |
| RA266 | Sync Stack, note-41 stable region | 1.600 s | Mono reference duplicated to stereo for spectral metrics; single-note candidate, measured from candidate 0.3 s onward; no envelope fitting |
| RA099 | PolyModster, note-36 fragment | 0.520 s | Exploratory; synthetic single note at velocity 1, candidate window starts at 0.1 s; original timing/velocity not established |

These are **five excerpts from one recording source**, not five independent recording chains or hardware replications. PolyModster and Sync Stack are diagnostic C-trust cases; they are not training truth for a global law. The full MP3 was not recovered in this run, but the excerpts are real recovered files, not reconstructions of the reference.

The original v0.8/v0.9 listening packs normalized level. Consequently absolute hardware gain, input voltage, noise floor and analog headroom cannot be identified from these excerpts. Shape metrics use one explicit RMS gain match per comparison; that gain and both input RMS values remain in the data. This dataset cannot decide the absolute AC-level consequence of removing pulse normalization.

## Exact factory programs and events

The official [Sequential factory archive](https://sequential.com/wp-content/uploads/2015/07/P6-Factory-Programs-ReadMe.zip) was retrieved locally. Its `P6_Programs_v1.01.syx` contains 500 messages. Original message bytes for programs 000/024/067/099/266 were extracted without rewriting their payload. Decoded names for the three principal anchors were checked. The canonical program bytes were not tuned to fit the audio.

`recovered_asset_hashes_v019.json` records hashes of the local source inputs. `recovered_reference_suite_v019.csv` uses relative paths to private files and carries assumptions/trust. No third-party WAV, MP3 or factory SysEx is committed or redistributed.

## Measurements

- Bounded envelope alignment: at most ±150 ms in 5 ms steps, with no time stretching; only for the full-performance cases.
- Average L/R spectral power: antiphase stereo content is not canceled before spectral measurement.
- Multi-resolution STFT sizes 512/2048/8192, Hann windows, 75% overlap, 40 Hz–12 kHz analysis band.
- Log-power errors use a reference-relative −60 dB activity floor. This is our diagnostic objective, **not the exact DDSP/auraloss loss**.
- Broad 32-band log-frequency shape distance is reported separately because small pitch offsets can dominate bin-wise STFT distances. Equal-weight log bands can emphasize weak spectral regions; this metric is also not a perceptual oracle.
- RMS-envelope error and stereo side/mid behavior remain separate. Envelope values are not fitted or used to promote the short sync/modulation excerpts.
- Sync diagnostics estimate each signal's F0 near the expected master frequency and compare H1–H16 relative harmonic levels. This avoids confusing a loud H2/H4 with played pitch.
- No weighted composite or accuracy percentage is assigned. Numbers below cannot be compared numerically with the old v0.7 **composite** score.

## Current vs historical v0.9 candidate

Lower distance is better. Values are median across four pre-rolls.

| Case | Current MR log-spectrum MAE (dB) | v0.9 MAE (dB) | Current 32-band shape RMSE (dB) | v0.9 shape RMSE (dB) |
|---|---:|---:|---:|---:|
| Brassed Off | 6.262 | 7.684 | 2.170 | 4.986 |
| Solid Bass | 14.437 | 12.577 | 29.356 | 9.628 |
| Carpenter Square | 11.156 | 12.548 | 10.300 | 11.287 |
| Sync Stack, diagnostic | 10.400 | 13.679 | 13.885 | 25.076 |
| PolyModster, exploratory | 13.126 | 11.265 | 16.159 | 7.205 |

The historical candidate clearly helps Solid Bass and the exploratory PolyModster timbre, but is worse on the other three excerpts under both reported spectral summaries. Solid Bass's envelope MAE also remains large (current 0.402, v0.9 0.411), so a spectral win does not establish correct performance/envelope behavior.

This does not invalidate the historical eight-case fit. Its source subset, engine revision and composite metric were different. It does invalidate treating the old “−35.6% score” as a current global accuracy claim or automatically promoting v0.9 everywhere.

## PW and pulse-normalization decision

The isolated Phosphor direct-PW prior on the **current** mapper changes Carpenter Square MR MAE from **11.156 to 11.144 dB**. Its pre-roll ranges overlap (current 11.112–11.174; PW prior 11.101–11.155). This is too small relative to nuisance sensitivity and limited source coverage to promote a new endpoint law.

The same PW prior on v0.9 helps the exploratory PolyModster fragment (11.265 → 11.198 dB) but is effectively neutral/slightly worse on Carpenter Square (12.548 → 12.549 dB). The historical 15% inset is retained as an exploratory profile, not promoted.

Removing extra pulse peak normalization on v0.9 slightly improves Solid Bass MR distance (12.577 → 12.468) but worsens PolyModster (11.265 → 11.824). Level-normalized listening assets cannot establish its physical gain law. Do not promote it globally.

## Hard-sync finding

The reference's **H5−H4 is −2.843 dB**. Current engine median is **+7.734 dB**, and the historical v0.9 mapper is **+12.567 dB**. Thus the relative H4/H5 inversion remains, independently of a global brightness score. H1–H16 RMSE is 12.501 dB for current and 18.647 dB for v0.9 in this new own-F0 diagnostic. These are not numerically interchangeable with the older fixed-F0/window diagnostic.

The step+slope profile is byte-identical to v0.9 for the four principal/diagnostic cases at each matching pre-roll (16/16 pairs). PolyModster changes slightly: waveform-difference RMS is about 3–4 × 10^-6 over the full raw render, while its MR distance changes from 11.2647328 to 11.2647354 dB. These excerpts provide **no meaningful positive acoustic evidence for promoting that hook**; this is not evidence that BLAMP is generally useless. The existing subsystem harmonic probe remains necessary for morph positions/ratios where slope jumps occur. No coarse-frequency or reset-phase law was changed to force H4/H5 agreement.

## Result and next concrete experiments

**Promotion: none.** The current engine stays the product baseline; v07/v08/v09 and external variants remain explicit candidates.

1. Use dry/simple additional references from the already inventoried oscillator/filter walkthroughs to break the filter-versus-source ambiguity.
2. Prioritize a model family that can explain Solid Bass without destroying Brassed Off/Carpenter Square; split static cutoff, key-tracking pivot, filter-envelope depth and time laws instead of adopting the whole historical profile.
3. Build a static independent Fourier/high-rate sync oracle from the reviewed literature and compare it with the subsystem probe; then return to the real H4/H5 target.
4. Recover/annotate more performances, particularly additional pulse and sync programs. Keep the 0.52 s PolyModster fragment exploratory.
5. Only after source/performance uncertainty is bounded, fit limited parameters with held-out programs and eventually independent recording sources.

## Reproduce

Install Python packages from `analysis/requirements.txt` and provide ffmpeg.
`tools/prepare_recovered_online_suite.py` restores only the named WAV members
and exact factory messages from the existing v0.8/v0.9 listening archives:

```bash
python3 tools/prepare_recovered_online_suite.py P6_Programs_v1.01.syx \
  p6_referenceA_listen_v0.8.zip p6_referenceA_listen_v0.9.zip
```

The original bank SHA-256 is `b46ee1b907b77fdfa17a358e9541722b5a1e021b0a5512d53b0fe9d50c620f19`.
Inferred events are included as text under `calibration/recovered_events/`.
Then build the core tools and run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python3 tests/online_metrics_test.py
python3 tools/compare_online_suite.py calibration/recovered_reference_suite_v019.csv \
  --renderer build/p6_render_events_cal \
  --profiles current,reference_a_v07,reference_a_v08,reference_a_v09,phosphor_pw_v019,reference_a_v09_plus_phosphor_pw,reference_a_v09_no_pulse_norm,reference_a_v09_pw015,reference_a_v09_sync_slope \
  --source-sha 3cc9329f96a7f2029996700de124cda34f793eb7 \
  --output reference_private/v019_reproduction
```

This produces 180 renders for the five-row manifest. Current published results were computed in two calls (four principal/diagnostic cases plus the exploratory case); both provenance records are retained.

Validation in this run: Release build succeeded; CTest **8/8** passed; Python measurement tests **5/5** passed; recovered generic analysis scripts compile. Windows/Linux VST3 and pluginval were not re-run locally in this research-only change. The release still requires the repository's existing CI gates.
