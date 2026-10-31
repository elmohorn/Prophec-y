# v0.19 — Sync Stack mixture, filter and ensemble isolation

Date: 31 October 2026. Core baseline: `34135bbc4334269b6b358065b45167557f676b50`.
Status: **132 new diagnostic/reference renders; no justified global correction identified**.
No engine, mapper, factory program or runtime sound default was modified.

## Findings

The H4/H5 discrepancy survives removal of the filters. It also survives ordinary changes to mixer law, cutoff, resonance, voice count and Slop. Opening the LPF helps the aggregate spectrum but does not fix this harmonic relationship. The most tempting aggregate improvement regresses another anchor and is rejected as a global replacement.

The stereo-safe H5-minus-H4 ratio in the reference is **-2.843 dB**, compared with a four-pre-roll median of **+8.141 dB** in the current complete model. First and second reference halves give -3.302 and -2.856 dB, so the sign is not confined to a small transition. Removing the filters gives **+8.140 dB**: the filter hypothesis explains brightness differences, but not this local inversion under the tested model.

The current complete model and the model with Poly Mod pitch disabled are different experiments. The stored program has a small positive Poly Mod envelope source (raw 131 vs neutral 127), routed to Osc1 pitch. With the current 4-octave destination range, its maximum excursion is **1.512 semitones**. That is sufficient to change sync harmonics substantially. The static +28-semitone Fourier oracle validates its declared oscillator family; it is not the entire modulated, six-voice hardware patch.

## Isolation method and controls

The original program uses Osc1/Osc2 coarse values 52/24, saw shapes 127/127, mixer levels 100/94, zero sub/noise, six Unison voices, Slop 67, Pan Spread 127, distortion zero and FX off. Program bytes and inferred note 41 remain unchanged in the main stage study. Reference window: 1.6 seconds; candidate window starts at 0.3 seconds. All profiles use 2x oversampling and the same four pre-rolls: 0, 0.5, 1.5 and 3 seconds.

The existing calibration renderer automatically centers pan and disables distortion for diagnostic bypasses. A tooling-only `--preserve-output-controls` option now allows component isolation while retaining the original output controls. The default behavior remains unchanged.

Checks:

- Current and the otherwise-identical preservation control produce **bit-identical WAVs in 4/4 pre-rolls**.
- One old-binary/current-binary default render is also bit-identical. The saved old binary hash exactly matches the corrected baseline's archived provenance.
- Reconstructing the filter-bypassed mix from isolated Osc1 and Osc2 using the original 100/127 and 94/127 gains gives relative RMS error **3.49–3.51 × 10^-8** across four pre-rolls. Timing, stack, panorama and the linear mixture are therefore preserved to float-WAV precision for this program. This does not validate analog hardware mixer laws in general.
- Harmonic powers are summed independently across channels, with F0 estimated from summed-channel autocorrelations. Synthetic tests confirm that an antiphase partial is retained and that silence is rejected. Legacy mono-fold measurements remain separately identifiable.

## Stage results

Medians over four pre-rolls. MR spectrum is the existing multi-resolution log-spectral MAE; harmonic RMSE uses the new independent-channel-power H1–H16 profile at each signal's own F0. Lower errors are better. A component render is a model intervention, **not an isolated hardware reference or a candidate preset**.

| Intervention | MR spectrum MAE (dB) | H5−H4 (dB) | H1–H16 power RMSE (dB) |
|---|---:|---:|---:|
| Current complete patch | 10.400 | +8.141 | 12.480 |
| Existing depth12 candidate | 10.384 | +8.142 | 12.471 |
| Filters bypassed, original mixture | 9.457 | +8.140 | 11.946 |
| Osc1 only, filters bypassed, Poly Mod retained | 10.347 | +10.899 | 10.393 |
| Osc2 only, filters bypassed | 7.769 | -1.596 | 12.489 |
| Osc1 sync only, filters and Poly Mod bypassed | 10.643 | +22.597 | 14.100 |
| Poly Mod pitch range zero | 9.851 | +12.797 | 11.705 |
| Mixer exponent 0.5 | 10.105 | +8.037 | 12.489 |
| Mixer exponent 2 | 10.505 | +8.420 | 12.477 |
| LP static cutoff ×0.5, HP law held | 12.527 | +9.134 | 12.718 |
| LP static cutoff ×2, HP law held | 9.089 | +8.098 | 11.869 |
| LP resonance ×0.7 | 10.487 | +8.220 | 12.245 |
| LP resonance ×1.3 | 9.979 | +8.156 | 12.422 |

Osc2 alone illustrates why fitting one partial or score is unsafe: its H4/H5 sign and aggregate MR error are closer, but its complete harmonic error does not improve, and it removes the defining synced oscillator. It is not a proposed repair.

## Cross-preset check

The four whole-patch interventions that improve Sync Stack's aggregate MR score were checked against Brassed Off, Solid Bass, Carpenter Square and the exploratory PolyModster excerpt. The baseline is reused from the corrected-engine matrix, not the historical pre-Unison-fix results. Its binary hash is verified; no duplicate baseline renders are counted as new work.

Median **paired** MR differences versus current, in dB; negative means improvement. Paired-difference medians need not equal differences between separate medians.

| Intervention | Brassed Off | Solid Bass | Carpenter Square | PolyModster |
|---|---:|---:|---:|---:|
| Mixer exponent 0.5 | +0.119 | +0.008 | 0.000 | +0.045 |
| LP static cutoff ×2 | +0.884 | -0.503 | -2.294 | -2.226 |
| LP resonance ×1.3 | +0.077 | +0.115 | -0.337 | -0.041 |
| Poly Mod pitch range zero | 0.000 | 0.000 | 0.000 | +0.856 |

The doubled cutoff additionally worsens Brassed Off's broad-band shape by **+1.560 dB**. The resonance increase worsens Solid Bass envelope MAE by a paired median **+0.0083**. Disabling Poly Mod pitch damages PolyModster's spectral shape and envelope. None passes as a global correction, even though some improve Sync Stack or other individual presets.

## Ensemble and mono-recording limits

Twelve additional renders use explicit private copies for one voice, Slop zero, and both. The original SysEx is preserved and all changed byte offsets/hashes are recorded. Median H5−H4 remains positive: **+7.766**, **+13.517** and **+7.759 dB**, respectively. These simple nuisance changes do not explain the reference inversion.

The recovered Sync Stack reference has identical L/R channels, unlike the other four recovered excerpts. The available listening-pack README does not establish whether the original source was mono or was downmixed during extraction. No original stereo field can be inferred from that file. Folding the model to mono still gives a positive H5−H4 median of **+7.709 dB**; channel-power measurement was not the cause of the sign mismatch.

The reference also has a very weak first harmonic: -33.50 dB relative to its strongest harmonic, versus 0 to -3.76 dB in the current stereo-power model. Removing Slop changes the model's first harmonic dramatically (median -21.31 dB) while leaving H5−H4 wrong. This demonstrates the sensitivity of a six-voice ensemble spectrum to phase/coherence; it does not identify which hardware or recording condition produced the reference. We do not phase-fit voices, retune the SysEx interval or alter reset geometry to force this excerpt to match.

The remaining possibilities include oscillator waveform/reset behavior, ensemble phase/Slop behavior, and exact patch/performance/recording identity. The current evidence **does not uniquely identify a DSP defect** in any one of those. The mathematical oscillator oracle remains useful, but cannot establish hardware fidelity. An invented high-cutoff knee that changes only Sync Stack while leaving the other four anchors untouched would not provide independent validation and was not introduced.

## Data, validation and reproduction

`calibration/v019_sync_stages/` contains the preregistered profiles/protocol, 56 stage comparisons, 64 cross-preset comparisons, 12 ensemble probes, full power harmonic profiles and half-window checks, mono-fold audit, isolation checks and reuse provenance. An additional old-binary render is a compatibility check, not a calibration trial. Four pre-rolls are sensitivity realizations, not independent hardware recordings. The previously frozen eight-program holdout remains untouched.

```sh
cmake --build build-check --target p6_render_events_cal
python3 tools/compare_online_suite.py calibration/recovered_reference_suite_v019.csv --renderer build-check/p6_render_events_cal --profiles-file calibration/v019_sync_stages/profiles.json --profiles current,control_preserve,depth12,raw_mix,osc1,osc2,sync_only,pm_pitch_zero,mix_sqrt,mix_squared,lp_half,lp_double,res070,res130 --cases RA266 --source-sha 34135bbc4334269b6b358065b45167557f676b50 --output reference_private/v019_sync_stages
python3 tools/analyze_sync_stages.py reference_private/v019_sync_stages --reference reference_private/online/266_reference.wav --output reference_private/sync_stage_analysis
python3 tools/compare_online_suite.py calibration/recovered_reference_suite_v019.csv --renderer build-check/p6_render_events_cal --profiles-file calibration/v019_sync_stages/profiles.json --profiles mix_sqrt,lp_double,res130,pm_pitch_zero --cases RA000,RA024,RA067,RA099 --source-sha 34135bbc4334269b6b358065b45167557f676b50 --output reference_private/v019_sync_crosscheck
python3 tools/probe_sync_ensemble.py --renderer build-check/p6_render_events_cal --output reference_private/v019_sync_ensemble --results reference_private/sync_stage_analysis
```

The audition pack labels components and rejected interventions explicitly. It includes a Brassed Off A/B to expose the doubled-cutoff regression. All audition files are RMS-matched, with no waveform alignment or time stretching. The current sound and depth12 status remain unchanged. The complete local core build, 8/8 CTest checks and all 17 analysis tests pass.
