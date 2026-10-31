# v0.19 — Isolated filter factors and independent hard-sync oracle

> Historical measurement caveat: the Unison first-key/release-tail bug discovered later in v0.19 affected Solid Bass multi-note renders. Preserve these numbers as historical results, but use `V0_19_UNISON_FIX_AND_RECALIBRATION.md` and the corrected-engine measurements for new calibration decisions. The independent static Fourier-oracle results are unaffected.

Date: 31 October 2026. Engine baseline: `0bc3c5e962d35c9952172c6aa00d0e37954b942c`.

**800 new final renders:** 320 real-reference candidate renders plus 480 synthetic sync-probe renders. The earlier 240-render single-register oracle pilot is superseded by the two-register matrix. No engine implementation, serialized state or product defaults changed.

## 1. Separate the historical calibration into four factors

The prior whole-profile replacement improved Solid Bass but damaged other programs. This experiment separates the hypotheses instead of adopting the bundle.

| Factor bit, in order | 0: current | 1: historical alternative |
|---|---|---|
| LP static cutoff law | base 8.1757989156 Hz; 1 semitone/raw | base 88.9769483193 Hz; .37 semitone/raw |
| Shared filter-envelope depth | 10.5833333333 octaves | 13 octaves |
| Shared filter-envelope timing | exponent 3.5; A/D/R maximum 10 s | exponent 3; A/D/R maximum 2.5 s |
| LP resonance scale | 1 | .7 |

All 16 combinations were tested on the same five recovered references and four matching pre-rolls: **16 × 5 × 4 = 320**. The amplifier envelope and static HPF law stay at current values. Explicitly holding the HPF base/slope at 8.1757989156/1 prevents an LP-law change from implicitly moving HPF cutoff through its normal inherited mapper settings. Shared filter-envelope depth/timing can still affect both filters where their program amounts are non-neutral.

`factorial_0000` is bit-identical to the preceding current-profile renders in **20/20 complete WAV pairs**, confirming that fixing the HPF law explicitly did not alter the baseline.

Data: `calibration/filter_factorial_profiles_v019.json` and `calibration/v019_filter_factorial/`. The new analyzer reports matched-pre-roll differences and conditional factor contrasts; these are numerical interventions on the model, not statistical estimates from independent hardware observations.

## 2. A narrower candidate improves all five MR spectral distances

`factorial_0100` changes **only filter-envelope depth to 13 octaves**, retaining current static cutoff, timing and resonance. Its median multi-resolution log-spectrum distance improves in all five excerpts. All 20 matched-pre-roll MR differences have the same improving sign.

| Reference | Current MR MAE, dB | Depth 13 | Depth 13 + resonance .7 |
|---|---:|---:|---:|
| Brassed Off | 6.262 | 6.070 | 5.876 |
| Solid Bass | 14.437 | 14.058 | 14.212 |
| Carpenter Square | 11.156 | 10.456 | 10.865 |
| Sync Stack, diagnostic | 10.400 | 10.373 | 10.460 |
| PolyModster, exploratory | 13.126 | 11.866 | 11.933 |

The two-factor variant `factorial_0101` improves **both MR and broad-band spectral distances on all three principal anchors**, for every matched pre-roll. It nevertheless worsens the diagnostic Sync Stack broad-band metric from 13.885 to 15.548 dB, so it is not a universal improvement.

The depth-only candidate also worsens Brassed Off's broad-band shape RMSE from 2.170 to 2.418 dB despite improving its MR distance. Solid Bass envelope MAE rises from .402 to .438 (depth-only) or .423 (depth + resonance). These conflicting metrics matter; a favorable spectrum cannot establish correct envelope/performance behavior.

These are useful, **not yet promoted**, profiles:

- `filter_env_depth13_v019` — minimal depth-only hypothesis;
- `filter_env_depth13_res070_v019` — depth/resonance alternative for audition and further cross-checking.

The existing CLI can render them through the profile registry; no guessed patch bytes or new C++ calibration constants are required.

## 3. What the factorial rules out

Changing the LP cutoff law alone (`1000`) improves Solid Bass MR distance by about 1.57 dB, but worsens Brassed Off by 1.25 dB and Carpenter Square by .62 dB. The static-law replacement still fails as a global change.

Changing filter-envelope timing alone (`0010`) worsens the MR distance on all three principal anchors. The historical short-time law is therefore not the part to promote from this matrix.

`1100` (shallower LP static law + deeper envelope, current timing/resonance) makes Solid Bass substantially closer (14.437 → 12.539), and improves Carpenter Square slightly, but worsens Brassed Off MR distance. Its favorable average/broad-band results cannot justify silently overriding that regression.

Thus the concrete progress is identifying envelope depth as a more general candidate than the old bundled cutoff/timing profile. It is not evidence that 13 octaves is the uniquely correct hardware constant. The candidates were selected on these same excerpts; there is no independent holdout recording in this experiment.

## 4. Independent continuous-time hard-sync oracle

`tools/sync_fourier_oracle.py` computes Fourier coefficients of the declared **ideal piecewise-linear oscillator family**, reset once per master period. It does not execute the C++ oscillator, BLEP, BLAMP, filter or decimator.

For master-cycle coordinate u, ratio r = 2^(slave_semitones/12), and free waveform W:

    x(u) = W(frac(r*u)), 0 <= u < 1
    c[h] = integral_0^1 x(u) exp(-j*2*pi*h*u) du

The master period is split at each slave waveform vertex and wrap. Each linear segment is integrated analytically. A small-argument series avoids cancellation at the 10^-6-cycle ramps used by the geometry contract. The geometry intentionally matches the declared trapezoidal family; **independent numerical integration does not independently prove that this is the P6's physical waveform**.

Oracle self-tests check:

- analytic triangle harmonic amplitudes and even-harmonic zeros;
- removal of non-multiple harmonics at an integer 4:1 ratio;
- noninteger reset-cycle integration against dense midpoint quadrature;
- invalid geometry/ratio rejection.

All **4/4** passed.

Bridge experiment: MIDI notes 41 and 65; six slave offsets (19/24/28/31/36/40 semitones); five shapes (0/.25/.5/.75/1); oversampling 1/2/4/8; legacy versus step+slope correction. Total **2 × 6 × 5 × 4 × 2 = 480** new C++ probe renders. Filter, Slop, Poly Mod, Osc2 audio and FX are excluded. Each render has .5 s warm-up and 1 s measurement.

Twenty harmonic magnitudes are measured at exact master harmonics. Only ideal partials above -50 dB relative to the strongest ideal partial enter RMSE, avoiding meaningless relative errors at mathematical spectral zeros.

## 5. Current oscillator converges to its declared model

Median / maximum H1–H20 error versus the oracle, over 30 shape/ratio cases per note and oversampling factor:

| Master note | Oversampling | Legacy median RMSE, dB | Legacy maximum RMSE, dB |
|---|---:|---:|---:|
| 41 | 1x | .01618 | .04494 |
| 41 | 2x | .01516 | .02039 |
| 41 | 4x | .00109 | .00278 |
| 41 | 8x | .00031 | .00068 |
| 65 | 1x | .26038 | .74838 |
| 65 | 2x | .08999 | .19450 |
| 65 | 4x | .01607 | .04491 |
| 65 | 8x | .00394 | .01118 |

This validates static in-band harmonic behavior on this grid. It is **not** a full alias-energy measurement, a proof over every frequency/duty/reset/modulation, or a claim of hardware accuracy.

At +28 semitones, the pure model itself predicts H5 stronger than H4 (approximately +26 to +28 dB across the tested shapes). The low-register legacy probe closely follows that prediction. Consequently the known factory Sync Stack H4/H5 mismatch cannot simply be attributed to a faulty low-order spectral calculation in the current reset implementation at this static ratio.

This does not directly compare pure Osc1 with the mixed/filtered hardware patch. The remaining investigation must account for actual waveform/reset behavior, Osc2 summation/fine tuning, filter response, modulation/Slop and patch/performance identity. Do not retune the raw semitone mapping to force agreement.

## 6. Experimental slope correction fails the pulse endpoint

The oracle exposes severe errors for `StepAndSlope` at pure-pulse shape 1. The worst tested case is note 65, +31 semitones, 2x: **41.411 dB** included-harmonic RMSE. The production/default `LegacyStepOnly` mode does not exhibit this failure; its maximum across the complete grid is .748 dB at 1x and .1945 dB at 2x.

Code inspection provides a plausible explanation, not yet a repaired derivation: at reset phase 0, a pulse endpoint begins on a 10^-6-cycle ramp, so the derivative-jump correction can act on an extremely steep, extremely short segment. Treating that local derivative in isolation can inject an excessive correction when neighboring corner contributions must be considered together.

Action taken: mark `reference_a_v09_sync_slope` **rejected-for-promotion** in the registry. Retain it as a diagnostic path for future derivation; do not enable it by default. A future fix needs the independent oracle across neighboring morph positions, pulse duties and reset phases, plus alias and real-reference tests. No speculative runtime fix is bundled here.

## 7. Reproduce and continue

```bash
python3 tools/compare_online_suite.py calibration/recovered_reference_suite_v019.csv \
  --renderer build/p6_render_events_cal \
  --profiles-file calibration/filter_factorial_profiles_v019.json \
  --profiles factorial_0000,factorial_0001,factorial_0010,factorial_0011,factorial_0100,factorial_0101,factorial_0110,factorial_0111,factorial_1000,factorial_1001,factorial_1010,factorial_1011,factorial_1100,factorial_1101,factorial_1110,factorial_1111 \
  --source-sha 0bc3c5e962d35c9952172c6aa00d0e37954b942c \
  --output reference_private/v019_filter_factorial
python3 tools/analyze_filter_factorial.py reference_private/v019_filter_factorial/comparisons.csv \
  --output calibration/v019_filter_factorial
python3 tests/sync_fourier_oracle_test.py
python3 tools/sync_fourier_oracle.py --probe build/p6_sync_probe --notes 41,65 \
  --source-sha 0bc3c5e962d35c9952172c6aa00d0e37954b942c \
  --output reference_private/v019_sync_oracle_two_registers
```

Both C++ executables already exist in the normal core build. Analysis needs NumPy/SciPy; online-reference decoding also needs ffmpeg. Only derived measurements/text are committed. Private factory programs and reference/rendered WAVs stay outside the repository.

Next highest-value work: cross-check envelope depth/resonance on additional programs or independently annotated online excerpts; resolve Solid Bass performance/envelope uncertainty; and isolate the actual Sync Stack source mixture against the now-validated static mathematical baseline. v0.19 remains **ACTIVE**, with two explicit audition candidates and one experimentally rejected sync alternative.
