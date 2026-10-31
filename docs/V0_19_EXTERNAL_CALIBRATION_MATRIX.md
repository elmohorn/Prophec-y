# v0.19 — External calibration prior matrix

Status: **ACTIVE SUPPORT DOCUMENT**

This document makes external implementations an explicit part of v0.19 sound matching. They are used as **priors and candidate generators**, not as automatic hardware truth. Real Prophet-6 audio (Reference-A plus other usable online captures) remains the acoustic arbiter.

## 1. Evidence roles

| Source | Instrument match | Evidence type | Licence / reuse rule | v0.19 weight |
|---|---|---|---|---|
| `joshjetson/phosphor` | Prophet-6 direct | full implementation; factory-bank-informed laws; manual-derived behavior | MIT; code/ideas may be adapted with attribution | **High prior** for P6-specific hypotheses, but constants explicitly labelled judgment remain unproven |
| Prophec-y Reference-A candidates v0.7/v0.8/v0.9 | Prophet-6 direct | fit to user-supplied real P6 factory-demo audio | project-native | **High acoustic prior** within the limits of compressed/inferred-performance audio |
| `keremimo/melodee` PROPHET | Prophet-5 Rev 4, not P6 | 75-program demo fit plus official bank | GPL; reference only, no source copying | **Medium/low cross-family prior** for envelopes, tracking, modulation and Vintage concepts |
| `straplocked/analog-synth` | Prophet-6 inspired, not calibrated clone | generic VA implementation and documented DSP choices | GPL; reference only, no source copying | **Low prior**; useful mainly for alternative algorithms and sanity bounds |
| `ShmKnd/Patina` | generic analog/effects | BBD, modulation, spring/plate, ring-mod, tape etc. | MIT | **Architecture prior for FX**, not Prophet-6 calibration truth |
| `wnetzel/all_in_the_multitouch` | P6-inspired | Pure Data architecture/behavior reference | GPL | **Architecture only** unless a parameter is independently evidenced |

A lower-weight project can still be useful when it supplies a qualitatively different candidate that Reference-A can discriminate.

## 2. Promotion rule

For every uncertain parameter:

```text
external prior(s)
  -> explicit Prophec-y candidate
  -> render same patch/performance
  -> compare against Reference-A / other P6 audio
  -> multi-segment regression check
  -> promote / retain as alternate / reject
```

Agreement between two software projects is **not independent hardware evidence** when one influenced the other, when both derive from the same documentation, or when both simply chose the same conventional DSP law.

## 3. Current calibration matrix

### 3.1 Oscillator pitch and shape

| Parameter | Prophec-y current | Phosphor | Other-project prior | P6 audio/document evidence | v0.19 action |
|---|---|---|---|---|---|
| Osc coarse centre | raw 24 = unison | raw 24 = unison | Prophet-family projects use semitone detents | factory-bank distribution + manual span strongly support it | **KEEP**, high confidence |
| Osc2 fine | ±50 cents | ±50 cents | Prophet-family Rev4 also uses fine cents around centre | manual explicitly states quarter-tone each way | **KEEP**, very high confidence |
| Shape raw law | `raw/254`, 0 triangle, midpoint saw, 254 pulse | same linear morph position | generic VA projects use discrete/independent waves, so little calibration value | manual describes continuous Triangle→Saw→Pulse morph | **KEEP candidate**, medium-high confidence |
| Morph geometry | continuous trapezoidal Triangle→Saw→Pulse | essentially same trapezoidal family | AnalogSynth uses separate standard waveforms, not comparable | no online P6 sweep yet strong enough to settle exact geometry | **MEASURE FROM ONLINE AUDIO**; do not infer confidence from Phosphor agreement alone |

### 3.2 Pulse width and pulse level

| Parameter | Prophec-y current | Phosphor | Other-project prior | Evidence status | v0.19 action |
|---|---|---|---|---|---|
| Minimum duty | oscillator clamp 4% / 96% | `DUTY_MIN = 0.04` | generic VA projects usually choose safety clamps rather than hardware-calibrated endpoints | manual only says “very narrow” at both ends | **OPEN**; 4% is a strong software prior, not proven P6 constant |
| raw→PW law | centred symmetric curve; default exponent 1; mapper endpoints reach 0/1 then oscillator clamps to 4/96, creating small flat endpoint zones | raw 0..255 is distributed directly across 4..96% | generic alternatives vary | Reference-A has pulse-oriented patches but not a controlled knob sweep | **HIGH-PRIORITY A/B**: legacy clamp-vs-direct-4..96 mapping |
| pulse DC removal | always remove waveform mean | removes pulse mean | conventional analog/digital implementations vary | AC-coupled recording makes DC itself hard to infer | likely keep DC-safe path; assess audible harmonic/level consequences separately |
| narrow-pulse peak normalization | amount=1 default: scale `(1+|mean|)^-1`; nearly 6 dB AC attenuation at extreme PW | same concept; Phosphor explicitly notes this is a rack/headroom choice and an analog pulse need not behave this way | no useful direct P6 calibration from generic projects | current Reference-A evidence has not independently proved it | **TOP P0-A QUESTION**: compare normalization amount 0..1 across pulse-heavy presets |

### 3.3 Mixer and gain

| Parameter | Prophec-y current | Phosphor | AnalogSynth / other | Evidence status | v0.19 action |
|---|---|---|---|---|---|
| Osc mixer law | linear raw/127 amplitude | linear raw/127 | conventional VA mixers vary; AnalogSynth is not P6-calibrated | factory audio can constrain relative levels but performance/filter interactions confound it | test exponent candidates around 1.0 on dry/high-confidence presets |
| Sub gain | 1.0 relative hook | direct linear level in P6 raw law | generic projects use design-specific sub gains | Brassed Off gives useful ~65/130 Hz separation | **MEASURE** from Brassed Off + other sub-heavy dry patches |
| Noise gain | 1.0 relative hook | direct linear raw law | generic | online noise-heavy patches can provide broad estimate | lower priority than oscillator/sub |
| Output trim / voice knee | plugin output architecture, not copied from Phosphor | Phosphor uses fixed output trim 0.034 plus internal voice limiting chosen for rack-level consistency | unrelated to P6 hardware output chain | Reference-A absolute recording gain is unknown | **DO NOT CALIBRATE P6 VOICE FROM PHOSPHOR OUTPUT TRIM** |

### 3.4 Filters

| Parameter | Prophec-y current/default | Reference-A candidate | Phosphor | Prophet-family external prior | v0.19 action |
|---|---|---|---|---|---|
| LP cutoff law | base 8.1758 Hz, 1 semitone/raw | v0.7: base 88.97695 Hz, 0.37 semitone/raw | interprets raw cutoff directly as MIDI-note/semitone control | Melodee P5 demo fit finds a much shallower effective knob law for its Curtis path | **RE-RANK default/v0.7/nonlinear candidates on broader P6 corpus** |
| HP cutoff law | inherits LP law by default | v0.8: 40 Hz base, convex exponent 4, top 20 kHz | same raw note-space concept as LP | generic P6-inspired filters are not useful physical calibration | keep v0.8 as explicit candidate; seek HP-dominant Reference-A programs |
| LP resonance scale | 1.0 default | v0.7 = 0.70 | raw 0..255 maps linearly to travel, followed by Phosphor filter model | generic projects have model-specific Q/res feedback | **Reference-A decides**; never compare raw resonance coefficients across different filter topologies directly |
| Filter env depth | default 127 semitones = 10.583 oct | v0.7 = 13 oct | 127 semitones | Melodee P5 demo fit ≈0.046 oct/raw for its hardware, ~5.84 oct full unipolar travel | strong family disagreement means **do not average constants**; compare P6 patches directly |
| Keyboard tracking | 0 / 0.5 / 1.0 | same | same conceptual semitone/quarter-tone tracking | Melodee P5 fit required correct keyboard pivot but full/half slopes remain 1 / 0.5 | **KEEP slopes**, verify pivot/implementation via P6 audio if needed |

### 3.5 Envelopes

| Parameter | Prophec-y current/default | Reference-A candidate | Phosphor | Melodee P5 fitted prior | v0.19 action |
|---|---|---|---|---|---|
| A/D/R max | 10 s | v0.7 filter A/D/R max = 2.5 s; amp remains 10 s | 10 s | P5 demo fit uses a different time-constant interpretation (`10000^(raw/127)` ms) | **OPEN**; fit amp and filter independently on high-confidence P6 segments |
| time-law exponent | 3.5 | v0.7 filter exponent 3.0 | 3.5 | different exponential/time-constant law | candidate family should include 3.0, 3.5 and time-constant-style mapping |
| minimum | 0.5 ms | same helper | 0.5 ms | not directly transferable | likely inaudible in lossy Reference-A at extreme minimum; avoid overfitting |
| filter vs amp law | independently calibratable hooks | v0.7 changes filter only | same raw law initially | P5 Rev1/2 even needed different envelope shape behavior | **KEEP INDEPENDENT**; never force both envelopes to one law for convenience |

### 3.6 LFO, Slop/Vintage and glide

| Parameter | Prophec-y current | Phosphor | Melodee P5 / AnalogSynth prior | v0.19 action |
|---|---|---|---|---|
| LFO range | 0.03–500 Hz geometric | 0.03–500 Hz geometric | Melodee P5 measured midrange `ln Hz = -3.10 + 0.0539·raw`, endpoints bent to 0.022–500 Hz; AnalogSynth generic | P5 fit is a useful curve-shape prior but **not transferable as a P6 constant**; use if Reference-A has identifiable LFO patches |
| Slop max | 100 cents with squared control law | 100 cents squared; described as judgment | AnalogSynth uses three slow drift sines plus per-voice tolerances, not P6-calibrated | evaluate statistically across sustained/repeated P6 examples; software agreement is weak evidence |
| Vintage | separate Standard/Vintage behavior | P6 implementation-specific random/tolerance behavior | Melodee P5 maps Rev4 stable→Rev1 loose and fits Prophet-family recordings; AnalogSynth uses ±5% component tolerances | useful qualitative prior only; avoid copying P5 distributions into P6 |
| Glide | 1 ms–10 s geometric | same | family projects vary | low priority until clean online examples are identified |

### 3.7 Poly Mod and sync

| Parameter | Prophec-y current | v0.9 candidate | Phosphor / external | Evidence status | v0.19 action |
|---|---|---|---|---|---|
| Poly Mod → Osc1 pitch | ±4 oct | ±4 oct | Phosphor supports same scale class; Melodee P5 demo measured different source-specific depths | P6 dry Poly-Mod corpus sparse | retain versioned candidate; seek online Poly-Mod demos |
| Poly Mod → shape | 0.5 morph travel | 1.0 | v0.9 changed after Phosphor cross-check | not independently verified acoustically | **OPEN** |
| Poly Mod → PW | 0.48 | 0.5 | Phosphor cross-check | not independently verified | low-risk candidate but still needs P6 evidence |
| Poly Mod → filters | 6 oct | 7 oct | Phosphor cross-check; P5 prior uses another range | not independently verified | **OPEN** |
| Hard-sync direction | Osc2 master → Osc1 slave | same | Phosphor agrees; manual/behavior support it | strong structural confidence | **KEEP** |
| Sync reset/slope correction | reset phase 0; slope correction opt-in | same candidate hooks | external implementations differ in anti-alias strategy | alias treatment is implementation detail; acoustic spectrum is arbiter | test candidates against sync-heavy online references, not source-code consensus |

### 3.8 Digital effects

| Domain | Prophec-y | Phosphor | Patina | v0.19 treatment |
|---|---|---|---|---|
| BBD / DDL / Chorus | v0.18 clean-room implementation with Phosphor-derived starting ideas | direct P6 effect architecture for those types | generic high-quality analog-effect reference | Phosphor = strongest structural prior; Patina = alternate implementation prior; online P6 FX audio decides constants |
| Phaser/Reverbs | clean-room candidates | legacy Phosphor P6 support incomplete in reviewed snapshot | strong generic architecture coverage | use Patina for plausible DSP families, not Prophet constants |
| Ring/Flanger later-OS | internal candidates; hardware serialization intentionally unresolved | not authoritative for later-OS raw encoding | generic architecture only | acoustic tuning and raw-state encoding remain separate problems |

## 4. Immediate experiments generated by this matrix

Prioritize experiments that discriminate between credible existing candidates rather than inventing new laws.

### P0-A1 — Pulse-width mapping

Render the same pulse-heavy factory patches under:

1. current Prophec-y mapper: raw endpoints → 0/1, then oscillator clamps to 4/96%;
2. direct Phosphor-style raw mapping: raw 0..255 → 4..96% with no endpoint plateau;
3. optional curved-centre variants only if 1 vs 2 is insufficient.

Primary targets: `Carpenter Square`, `Busted Squares`, `Harpsichord` and other high-confidence pulse-oriented Reference-A segments.

### P0-A2 — Narrow-pulse normalization

Sweep `pulseMeanNormalization` through at least:

- 0.0 — DC removal only;
- 0.5 — partial peak normalization;
- 1.0 — current/Phosphor behavior.

Judge level-normalized harmonic shape **and** unnormalized AC level. Do not allow normalization of the comparison to hide the ~6 dB candidate level difference.

### P0-A3 — Mixer law

Compare mixer exponent candidates around 1.0 using dry/simple patches. Fit oscillator, sub and noise relative gains separately rather than letting one global exponent absorb all differences.

### P0-C — Filter candidate rerank

Re-run at least:

- current default;
- Reference-A v0.7;
- v0.8 HP law where relevant;
- existing nonlinear/two-slope experimental families;
- Phosphor raw-note law as its own explicit baseline rather than conflating it with Prophec-y default.

The goal is not to average P6 and P5 constants. Melodee's P5 fit is used only to demonstrate plausible law shapes and to guard against treating one software convention as inevitable.

### P0-D — Envelope model family

Test separate amp/filter families:

- exponent 3.5 / max 10 s;
- v0.7 filter exponent 3.0 / max 2.5 s;
- time-constant-style curve inspired by the independently fitted P5 project.

Promote only if several P6 segments agree.

## 5. Legal/source discipline

- MIT sources (Phosphor, Patina): implementation may be adapted where useful, with required attribution/licence notice.
- GPL/AGPL sources (AnalogSynth, Melodee, `all_in_the_multitouch`, protocol/editor references): inspect behavior, equations, constants and architecture as research evidence; do **not** copy source into Prophec-y.
- Public protocol facts and official manuals are facts/documentation, not source-code reuse.
- Every future external prior added to this matrix must record repository, pinned commit where practical, licence and whether its value was measured, documented, bank-inferred or simply chosen by the author.

## 6. Confidence labels

Use these labels in future sound-matching reports:

- **DOC** — directly documented by Sequential.
- **BANK** — strongly constrained by the official factory bank / raw ranges.
- **P6-AUDIO** — supported by real Prophet-6 audio.
- **P6-PRIOR** — direct Prophet-6 software implementation but not independently measured.
- **FAMILY-PRIOR** — measured or reasoned on Prophet-5/other related hardware.
- **GENERIC-PRIOR** — conventional DSP or P6-inspired software without hardware calibration.
- **OPAQUE** — insufficient evidence; retain candidate hooks and do not promote.

The desired end state is not maximal agreement with another emulator. It is a Prophec-y calibration for which the important promoted constants are increasingly supported by **DOC/BANK/P6-AUDIO**, with external implementations used to reduce the search space.