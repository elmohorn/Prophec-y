# v0.19 — External calibration priors

Status: **ACTIVE SUPPORTING EVIDENCE**

This matrix records calibration hypotheses from independent synth projects so v0.19 does not re-invent every transfer law from scratch. External implementations are **priors**, not Prophet-6 truth. Promotion into Prophec-y still requires compatibility with real Prophet-6 audio (Reference-A and other online hardware recordings) and regression review.

## Evidence classes

| Class | Meaning | Use |
|---|---|---|
| P6-PERMISSIVE | Prophet-6-specific implementation under a permissive license | Strong external prior; may seed candidates, but does not override real-hardware audio |
| RELATED-MEASURED | Related Sequential/analog implementation reported as calibrated to recordings | Weak-to-medium prior for plausibly shared behavior only |
| DOMAIN-PERMISSIVE | Generic analog/FX implementation under a permissive license | Architecture/algorithm prior, not Prophet-6 calibration |
| COPYLEFT-REFERENCE | GPL/AGPL implementation | Behavior/range/reference only; no code copying into Prophec-y |
| OFFICIAL | Sequential documentation | Authority for documented ranges/semantics, not undocumented circuit laws |
| REAL-AUDIO | Real Prophet-6 recording | Acoustic decision evidence, weighted by source/patch/performance confidence |

External agreement raises the prior probability of a hypothesis, but consensus between software implementations is not independent hardware measurement.

## Pinned sources

### Phosphor — `joshjetson/phosphor`

- license: MIT;
- pinned audit commit: `33b1073fa1b56315d0ca877b0210efec40f68f4d`;
- dedicated Prophet-6 engine with the 500-program ROM and extensive deterministic tests;
- class: **P6-PERMISSIVE**.

Phosphor is the strongest currently known external Prophet-6 prior. Several Prophec-y behaviors already deliberately overlap with it, so agreement between the two cannot always be counted as independent evidence.

### Patina — `ShmKnd/Patina`

- license: MIT;
- generic audio/DSP library with drive, delay, modulation, reverb and analog-style building blocks;
- class: **DOMAIN-PERMISSIVE**.

Useful for algorithm architecture and numerical/realtime practices, not for Prophet-6 oscillator/filter transfer laws.

### melodee — `keremimo/melodee`

- license: GPLv3;
- includes a Prophet-5 Rev4-oriented engine and reports fitting some control behavior to recordings;
- different instrument generation and filter topology from the Prophet-6;
- class: **COPYLEFT-REFERENCE / RELATED-MEASURED**.

Use only as a weak behavioral prior for plausibly shared Sequential conventions. Do not copy implementation code or promote Prophet-5 constants merely because they exist there.

### AnalogSynth — `straplocked/analog-synth`

- license: GPL-3.0-or-later;
- describes itself as inspired by the Prophet-6, but is a broader virtual-analog design rather than a P6 state-compatible emulator;
- its documented hard-sync topology uses Osc1 as master and Osc2 as slave, which conflicts with the Prophet-6 contract used by Prophec-y;
- class: **COPYLEFT-REFERENCE / ARCHITECTURE-ONLY**.

This is useful as a counterexample: a project can be Prophet-6-inspired without being a trustworthy calibration source. Its generic PolyBLEP, oversampling, drift and FX choices may inform engineering experiments, but its constants are not counted as Prophet-6 votes.

## Cross-calibration matrix

`Current` means the unqualified Prophec-y mapper unless otherwise stated. `RA` means the existing Reference-A candidates.

| Domain | Prophec-y current / RA | Phosphor prior | Other prior | Real-audio status | v0.19 confidence / action |
|---|---|---|---|---|---|
| Osc coarse center | raw 24 = unison | same Prophet program interpretation family | — | Brassed Off pitch reconstruction is consistent with current interpretation | **High enough to retain** |
| Osc2 fine | ±50 cents candidate | P6-specific fine mapping should be cross-checked numerically | related synths vary | Reference-A not precise enough yet | **Medium; audit exact Phosphor law** |
| Shape domain | raw 0..254 → 0..1 | Triangle→Saw→Pulse continuous trapezoid family | related analog morphs not authoritative | factory clips can rank broad shape mismatch | **Medium prior; do not change from consensus alone** |
| Shape geometry | continuous trapezoid/polyBLAMP-inspired | one moving trapezoid, not simple crossfade | AnalogSynth uses discrete conventional wave generators, so it is not an independent vote | needs dry/low-filter online examples | **Medium prior, low hardware certainty** |
| PW minimum | oscillator clamps 4%..96% | `DUTY_MIN = 0.04` | instrument-specific elsewhere | not yet isolated in Reference-A | **Medium prior** |
| raw→PW law | raw maps to nominal 0..100%, then oscillator clamps 4..96%; creates endpoint dead zones | entire raw 0..255 is distributed linearly across 4..96% | — | Carpenter Square / pulse-oriented presets are best current targets | **Important divergence: test candidate `endInset=0.04`** |
| Pulse DC/level normalization | DC removal + Phosphor-derived mean/peak normalization (`pulseMeanNormalization=1`) | mean removed, scale `1/(1+abs(mean))` | analog implementations differ | online corpus has not isolated narrow-pulse level law | **Low hardware confidence; keep candidate, not truth** |
| Mixer level | linear raw/127 by default | linear raw/127 | generic synths vary | Reference-A can test relative level only in suitable presets | **Medium prior** |
| Sub/noise gain | 1.0 relative candidate | audit required | generic implementations not P6 evidence | needs suitable dry examples | **Low** |
| LP cutoff control | default: 1 semitone/raw from 8.1758 Hz; RA-v07: base 88.97695 Hz, slope 0.37 | raw cutoff coordinate is directly 0..164 semitone-like steps | Prophet-5 laws are topology/instrument dependent; AnalogSynth offers multiple unrelated filter models | Reference-A strongly preferred RA-v07 over legacy in multi-patch fit | **Real audio outranks external prior; keep RA family active** |
| LP resonance raw law | default linear raw/255; RA-v07 scale 0.70 | linear raw/255 before model | filter implementation differs by project | RA-v07 improved corpus fit | **Medium; model and control law must be judged separately** |
| HP cutoff | RA-v08 uses 40 Hz + convex candidate to 20 kHz | audit exact P6 law separately | generic HPF references not P6 calibration | Reference-A evidence exists but incomplete | **Low-medium** |
| Envelope control law | power-law candidate; default max 10 s, exponent 3.5; RA-v07 filter max 2.5 s, exponent 3.0 | audit exact P6 envelope law | melodee can provide weak related-instrument timing prior; AnalogSynth uses its own generic exponential ADSR | Reference-A filter/envelope fitting is coupled but informative | **Medium; external priors seed grid only** |
| Filter-env depth | default 10.583 oct; RA-v07 13 oct | audit raw `ENV_SEMITONES` constant | Prophet-5 not assumed equivalent | RA-v07 multi-patch fit supports larger range | **Medium** |
| Poly Mod Osc1 pitch | RA-v09 4 oct | 4 oct prior | related models non-authoritative | dry examples sparse | **Medium external, low audio confirmation** |
| Poly Mod shape | RA-v09 full morph = 1.0 | full morph prior | — | sparse online evidence | **Medium external** |
| Poly Mod PW | RA-v09 0.5 duty span | 0.5 prior | — | sparse online evidence | **Medium external** |
| Poly Mod filters | RA-v09 7 oct | 7 oct prior | — | sparse online evidence | **Medium external** |
| Hard-sync topology | Osc2 master → Osc1 slave | same P6 family must be verified in pinned source | AnalogSynth documents the opposite topology, so it is rejected as a P6 calibration vote | DeepSyncLead/Saw Sync Lead remain useful acoustic tests | **Topology comes from P6 evidence, not generic inspiration** |
| LFO rate law | geometric 0.03..500 Hz candidate | audit exact P6 mapping | related Sequential engines may help only weakly | factory demo usually insufficient for exact endpoints | **Low** |
| Glide law | 1 ms..10 s geometric candidate | audit exact P6 mapping | related Sequential prior possible | demo performance confounds note timing | **Low** |
| Slop/Vintage | custom stochastic model | Phosphor implementation is useful as P6 prior | Prophet-5 Vintage behavior is related but not identical; generic drift models are architecture-only | requires statistical multi-preset/long-note evidence | **Low-medium** |
| BBD / DDL / Chorus | clean-room v0.18 candidates | Phosphor was already audited as MIT P6 reference | Patina useful only for general DSP practice | online FX clips can refine time/loss/stereo | **Medium external; acoustic refinement later** |
| Phaser / Reverb | clean-room v0.18 candidates | limited/unsupported legacy coverage in Phosphor | Patina architecture useful | online isolated FX evidence needed | **Low P6 calibration confidence** |

## First executable prior profile: Phosphor PW

The machine-readable registry `calibration/external_priors.json` contains a deliberately narrow `phosphor_pw_v019` profile. It changes only a mapping for which the external P6 implementation gives a clear, representable alternative:

```text
pulseWidthCurveExponent = 1.0
pulseWidthEndInset      = 0.04
```

This maps raw PW 0..255 continuously to 4%..96% instead of mapping to 0..100% and relying on the oscillator's later 4% clamp. It removes the small endpoint dead zones while preserving square at the center and the same final duty limits.

`tools/render_external_prior_suite.py` can render this profile, the current mapper, historical Reference-A candidates and combinations such as `reference_a_v09_plus_phosphor_pw` over the same local SysEx/event suite. The profile does **not** change engine defaults.

The registry does **not** automatically import Phosphor's filter, envelope, oscillator or FX implementation. Where Prophec-y already matches a Phosphor law, no candidate change is needed. Where Reference-A already contradicts or improves on a software prior (notably the LP cutoff family), the real-audio candidate remains the more important test.

## Candidate promotion policy

For each external-prior candidate:

1. record source, license, commit and exact hypothesis here;
2. implement it as a named non-default renderer profile;
3. render the same Reference-A event/patch set with baseline and candidate;
4. compare pitch, spectrum, envelope and stereo metrics appropriate to each source;
5. require improvement on more than the motivating clip;
6. run deterministic DSP fingerprints and unrelated-domain regressions;
7. promote only if real Prophet-6 evidence supports the change.

If two external projects disagree, both become candidates. We do not average arbitrary constants.

## License boundary

Permissive projects may provide implementation ideas/code when attribution and license terms are satisfied. GPL/AGPL projects are reference-only for Prophec-y: inspect behavior, published ranges and measurements, but do not copy their implementation into this codebase.
