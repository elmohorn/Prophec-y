# v0.19 — Sound Matching and Optimisation

Unison first-key/release-tail defect fixed in `23910ba4...`. See `V0_19_UNISON_FIX_AND_RECALIBRATION.md` before using the older Solid Bass calibration rows. A fresh 360-render local grid and 48-render event sensitivity test use the corrected engine; filter defaults remain unchanged.


Status: **CLOSED AS ENGINEERING/RESEARCH CHECKPOINT**

Final decision and explicit acoustic limitations: [V0_19_CLOSEOUT.md](V0_19_CLOSEOUT.md). [Final filter/envelope evaluation](V0_19_FINAL_CALIBRATION.md) freezes Default after 100 new renders. The sections below retain the historical plan and intermediate checkpoints; their open-work wording is superseded by the closeout ledger, not silently treated as completed hardware calibration.

## Historical pre-closeout checkpoint — 31 October 2026 (Santiago)

See [V0_19_PLAN_REVIEW.md](V0_19_PLAN_REVIEW.md) for the reconciliation against the master matrix and completed reports. v0.19 remains open. The next agreed work is static LP/HP cutoff analysis on the simplest usable references, then envelope timing/contour with the static law frozen. Any unidentifiable law stays provisional. This does not complete the broader modulation/FX/performance scope by implication. The eight-program holdout remains unrun, and representative coverage must be accounted for before release. Controlled owner-provided hardware recordings remain optional under section 1.

The last audit passed nine local CTest targets, but CI run41255778521 failed because its explicit build lists omitted the new audit executable and the media integrity utility used Python3.11-only hashing on Python3.10. The build lists and streaming hash have now been corrected; a successful full follow-up CI is still a release gate, not implied by local tests.

Follow-on checkpoint: `V0_19_FILTER_AND_SYNC_RESULTS.md` documents 320
filter-factorial real-reference renders and 480 synthetic Fourier-oracle
comparisons. Two filter-envelope audition profiles remain unpromoted; the
step+slope sync profile is explicitly rejected for promotion after pure-pulse
endpoint failures. Static harmonic agreement with a mathematical family does
not establish Prophet-6 waveform fidelity.

Research/comparison checkpoint (31 October 2026): see
`V0_19_RESEARCH_REVIEW.md` and `V0_19_RECOVERED_AUDIO_RESULTS.md`.
Five recovered real-hardware excerpts have now been re-rendered under nine
candidate profiles and four common pre-rolls (180 renders). The data support
keeping the current defaults while separating static-filter, envelope and
hard-sync questions. The seven generic native analysis tools missing from the
repository were recovered from the v0.15 project archive. Full-source MP3
recovery and independent online-source annotation remain outstanding.

v0.19 improves acoustic fidelity **without requiring access to a physical Prophet-6**. Its primary evidence is real Prophet-6 audio already available online or supplied by the project owner, combined with known program state, factory SysEx, documented controls, performance-event reconstruction and cross-source consistency.

The v0.18 DSP fingerprints remain the anti-regression guard. They answer “did Prophec-y change?” Reference-A and other real Prophet-6 recordings answer the separate question “did Prophec-y move closer to the hardware?”

## 1. Core constraint

The development plan must not depend on the project owner obtaining, borrowing or recording a Prophet-6. A controlled hardware capture would be useful if one ever becomes available independently, but it is **optional evidence**, not a milestone prerequisite.

v0.19 therefore works from:

1. the existing Reference-A corpus;
2. additional public Prophet-6 recordings with useful provenance;
3. factory/program SysEx and documented patch settings where available;
4. reconstructed MIDI/performance events when the original MIDI is unavailable;
5. official Sequential documentation;
6. independent software implementations as **calibration priors and hypothesis generators**, never as acoustic truth.

## 2. Reference-A

Reference-A is the current principal acoustic anchor: the user-supplied extraction of the online recording **“Sequential Prophet-6 - Factory Presets Demo (Sounds Only)” by Wolfgang S Vukan**.

Working source characteristics already established in the project:

- full 500-program factory-preset performance;
- local/user-supplied extraction: MP3 stereo, 44.1 kHz, 320 kb/s, approximately 36:50;
- source description indicates direct Prophet-6 recording to a Tascam DR-05X at 24-bit / 48 kHz before online distribution/compression;
- program boundaries and performance events are reconstructed/inferred and therefore carry confidence grades rather than being treated as ground truth.

Reference-A is suitable for **relative** calibration of:

- pitch/fundamental relationships;
- broad harmonic balance and brightness;
- envelope timing and contour when the performance is sufficiently clear;
- stereo spread/relative channel behavior;
- large filter/resonance differences;
- gross gain relationships after explicit normalization;
- ranking competing DSP candidates against the same real-hardware recording.

Reference-A is **not** authoritative for:

- ultrasonic content or alias energy above the effective online/audio-codec bandwidth;
- tiny converter/noise-floor differences;
- sample-exact waveform phase;
- absolute amplitude without a known recording gain chain;
- subtle transient behavior when codec smearing or unknown performance timing dominates;
- exact knob laws from a single factory performance where the control trajectory is unknown.

The clearest existing validated anchor remains program 000 **Brassed Off**, where previous analysis identified approximately 65 Hz sub content, ~130 Hz Osc1 content and a reconstructed C4 → Eb4 → F4 performance. It remains a sanity check, not the entire calibration corpus.

`calibration/reference_a_inventory_v019.csv` is the current compact inventory of trusted and candidate Reference-A segments. It deliberately separates boundary confidence, performance-analysis trust and usable DSP domains.

## 3. Evidence hierarchy

Use the strongest available evidence per parameter, rather than pretending all online recordings or software projects are equal.

### Grade A — high-value public hardware reference

Real Prophet-6 audio where several of the following are known:

- exact program/preset identity;
- corresponding SysEx/program data;
- dry/FX state;
- demonstrated knob positions;
- isolated oscillator/filter/effect behavior;
- direct/high-quality audio path;
- stable note or explicitly shown performance.

### Grade B — useful real-hardware reference

Real Prophet-6 audio with identifiable patch/performance but incomplete capture-chain or MIDI information. Reference-A mostly belongs here, with individual segments promoted/demoted by confidence.

### Grade C — qualitative hardware evidence

Real Prophet-6 audio where patch/performance is only partly recoverable. Use for broad timbral ranking and falsifying obviously wrong candidates, not for fitting precise constants.

### Grade D — external calibration priors / hypothesis material

Independent implementations may contain useful calibration work. v0.19 records those hypotheses in `docs/V0_19_EXTERNAL_PRIORS.md` and machine-readable profiles in `calibration/external_priors.json`.

Current classes include:

- **P6-PERMISSIVE** — Prophet-6-specific permissive implementations such as Phosphor; strongest software prior;
- **RELATED-MEASURED** — related Sequential/analog models with reported measurement work; weak-to-medium prior only;
- **DOMAIN-PERMISSIVE** — generic DSP libraries such as Patina; architecture/numerics, not P6 calibration;
- **COPYLEFT-REFERENCE** — GPL/AGPL projects; behavior/reference only, no implementation copying into Prophec-y.

External consensus narrows the search space but does not establish hardware truth. A real Prophet-6 recording can override a software consensus. If external projects disagree, keep separate candidates rather than averaging arbitrary constants.

## 4. Online-reference corpus

Do not require new hardware recordings. Build a versioned corpus around material we can legally inspect and analyze without redistributing copyrighted factory audio in releases.

For each reference segment record as much as is known:

```text
reference_id
source title / creator
source URL or user-supplied source identifier
local source properties (codec / rate / channels)
program or patch identity
matching SysEx / raw payload when available
segment start/end
reconstructed MIDI/performance events
FX / distortion / pan / slop confidence
alignment confidence
usable domains (pitch / spectrum / envelope / stereo / etc.)
known limitations
```

Generated derivatives such as event files, measurements, fingerprints and fit reports may be stored in the development corpus. Do not commit or redistribute third-party audio unless licensing explicitly allows it.

## 5. Performance reconstruction

Original MIDI will often be unavailable. v0.19 therefore treats performance reconstruction as a first-class engineering problem.

The existing event renderer accepts timed note/velocity/duration events and should be used to recreate the most plausible performance against a known patch.

Rules:

1. infer events from pitch/onset evidence plus patch constraints;
2. keep alternative event hypotheses when ambiguous;
3. flag polyphonic, hard-sync, arp/sequencer and FX-heavy passages as lower confidence;
4. never use factory sequencer bytes as if they were necessarily the demo performance;
5. optimize event timing separately from DSP constants so a timing error is not “fixed” by changing the synth model;
6. report confidence per segment.

## 6. External-prior workflow

External projects are used before brute-force fitting to generate defensible candidate laws.

The machine-readable registry `calibration/external_priors.json` stores sources, licenses, exact hypotheses and renderer arguments. `tools/render_external_prior_suite.py` renders the same local SysEx/event suite under multiple named profiles without changing engine defaults.

Initial executable profiles include:

- `current`;
- `reference_a_v07/v08/v09`;
- `phosphor_pw_v019`;
- `reference_a_v09_plus_phosphor_pw`;
- `phosphor_poly_mod_v019`.

The first real divergence selected for testing is the PW control law. Prophec-y currently maps raw 0..255 to nominal duty 0..100% and then clamps the oscillator to 4..96%, creating small dead zones at both ends. Phosphor distributes the entire raw range directly over 4..96%. The candidate is exactly representable with `pulseWidthEndInset=0.04`, so it can be A/B tested without architecture changes.

Where Prophec-y already matches an external law (for example linear mixer raw/127 or the existing narrow-pulse normalization family), no duplicate candidate is created merely to claim consensus.

## 7. v0.19 calibration order

Because available online references are uneven, the order is driven by **evidence quality**, with external priors used to reduce candidate search space.

### P0-A — Oscillator / mixer

Use the strongest online examples and factory-patch matches to test:

- coarse/fine pitch law;
- Triangle → Saw → Pulse morph geometry;
- raw → pulse-width law;
- narrow-pulse mean/level normalization;
- Osc1/Osc2/Sub/Noise relative mixer levels;
- polarity and phase-sensitive relationships only where evidence is strong enough.

Two especially important hypotheses already identified:

1. Prophec-y and Phosphor share essentially the same trapezoidal morph family and 4% minimum duty assumption. Agreement between implementations is **not independent hardware evidence**.
2. Prophec-y currently inherits Phosphor-style narrow-pulse mean removal plus peak normalization. This intentionally reduces narrow-pulse AC level substantially at the extreme and must remain a calibration candidate, not be treated as proven Prophet behavior.

The first external-prior A/B is `phosphor_pw_v019`, preferably judged on Carpenter Square and Poly Pulse before broader promotion.

### P0-B — Hard sync

Prefer dry/low-FX examples with known oscillator intervals. Compare:

- perceived/fundamental pitch;
- harmonic distribution;
- spectral centroid;
- reset transient character;
- alias-region energy only inside the reliable bandwidth of the source.

DeepSyncLead is currently ambiguous; Saw Sync Lead is retained as an independent cross-check. Prophet-6-inspired projects with contradictory master/slave topology are not treated as calibration votes.

### P0-C — LPF / HPF

Use factory programs and demonstrations where one filter dominates and envelope movement is limited or reconstructable. Fit LPF and HPF independently. The historical Reference-A cutoff candidates remain hypotheses, not immutable truth.

Notably, Phosphor's P6 cutoff-coordinate prior is close to the old one-semitone/raw family, while Reference-A v0.7 substantially preferred the shallower fitted law across the weighted corpus. This is a concrete example where real audio should outrank software consensus.

### P0-D — envelopes

Use segments with clean attacks/releases and repeated notes when available. Estimate event onset first, then compare time-domain envelopes. Do not fit codec pre-ringing or unknown key timing as envelope behavior. Related Sequential projects can seed timing ranges but cannot establish P6 timing constants.

### P0-E — modulation

Use examples with recognizable LFO/Poly Mod/aftertouch behavior, preferably when settings or program identity are known. Measure depth/frequency relationships only to the precision supported by the recording. Phosphor-derived Poly Mod spans already carried by Reference-A v0.9 remain candidate priors until the online corpus confirms them.

### P0-F — Distortion / Slop / Vintage

Aggregate multiple programs and longer passages where needed. Stochastic behavior is assessed statistically across recordings, not sample-for-sample. Generic analog-drift implementations are architecture references only.

### P0-G — digital FX

Use dry vs wet demonstrations, isolated effect demos and matching factory programs when available. Phosphor and Patina can seed DSP hypotheses where applicable, but acoustic calibration remains online-hardware driven. Later-OS PH3/Ring/FL1/FL2 state encoding remains an independent evidence problem and must not be guessed.

## 8. Metrics appropriate for online audio

No single score is authoritative. Each comparison reports the metrics the source can actually support.

Preferred metrics:

- pitch error in cents;
- aligned short-time RMS/envelope error;
- H2–H16 relative harmonic error inside reliable bandwidth;
- spectral-envelope / log-magnitude distance;
- spectral centroid/rolloff differences;
- transient timing after event alignment;
- stereo correlation and relative channel energy;
- FX delay/rate/decay estimates where identifiable.

Absolute high-frequency alias metrics, tiny level differences and sample-wise correlation are down-weighted or disabled for lossy/unknown-chain sources.

## 9. Candidate promotion rule

Existing `referenceACandidateV07/V08/V09()` are historical candidates and remain versioned.

New v0.19 hypotheses first live as named renderer profiles. They need not become C++ calibration functions until real-audio testing justifies promoting them closer to the engine.

Promotion flow:

```text
external prior and/or real Prophet evidence
  -> performance/event hypothesis
  -> named isolated candidate
  -> before/after multi-segment report
  -> v0.18 fingerprint review
  -> unrelated-reference regression review
  -> promote / reject
```

A candidate must improve more than the one clip or software project that inspired it. Overfitting one compressed demo segment or copying another emulator's assumptions is a failure mode, not progress.

## 10. Search strategy for additional evidence

v0.19 should actively seek additional public Prophet-6 material that fills Reference-A gaps, especially:

- dry oscillator/PW sweeps;
- filter cutoff/resonance sweeps;
- envelope demonstrations;
- hard-sync demonstrations;
- Poly Mod demonstrations;
- isolated FX examples;
- videos where knob positions are visible and audio is direct.

Prefer multiple independent real-hardware sources for the same law. A new source is valuable even when lower quality if it constrains a parameter that Reference-A cannot identify.

External software searches continue too, but repos are classified before their constants are allowed into candidate generation. “Prophet-6-inspired” is not sufficient evidence by itself.

## 11. Tooling

Keep and extend tooling that is useful for online-reference analysis:

- 48/96 kHz Prophec-y reference renderer;
- event/performance renderer;
- calibration renderer;
- DSP fingerprints;
- spectral/timing probes;
- `calibration/reference_a_inventory_v019.csv` for confidence/domain tracking;
- `calibration/external_priors.json` for machine-readable external hypotheses;
- `tools/render_external_prior_suite.py` for multi-profile candidate renders;
- automated before/after comparison reports.

The controlled-capture manifest/schema and generated P0-A capture-pack tooling are **not v0.19 prerequisites**. They may remain as optional future infrastructure, but no roadmap item may depend on the user producing new Prophet-6 recordings.

## 12. Performance optimisation

Performance work continues under two guards:

- deterministic DSP fingerprints for accidental regressions;
- online real-hardware reference metrics for fidelity-sensitive changes.

Prefer algebraically exact optimisations and moving invariant work outside sample loops. Precision changes, approximations, SIMD substitutions or reduced update cadence require profiling evidence plus reference regression testing.

GitHub-hosted CPU percentages remain directional only; they are not absolute product CPU claims.

## 13. Immediate work order

1. Use `calibration/reference_a_inventory_v019.csv` to materialize the existing trusted A/B suite from the recovered Reference-A analysis assets.
2. Run `current`, `reference_a_v07/v08/v09`, `phosphor_pw_v019` and the combined v09+PW profile over the same anchors.
3. Prioritize Carpenter Square and Poly Pulse for the first PW-law decision; include Brassed Off/Solid Bass as unrelated-domain guards.
4. Re-score the historical Reference-A candidates across the expanded inventory instead of relying only on the old six/eight-case summaries.
5. Search for additional online Prophet-6 captures specifically targeting gaps in PW, sync, cutoff/resonance, envelopes and Poly Mod.
6. Add new external projects to the prior matrix only after license, instrument applicability and architecture are audited.
7. Promote no acoustic change unless it improves the weighted multi-source real-hardware score without unacceptable regressions.
8. Profile and integrate only fingerprint-safe performance improvements alongside sound matching.

The custom/reactive FX visual concept remains **DEFERRED / UI** in the master matrix and is outside v0.19 sound-matching scope unless a later UI milestone deliberately pulls it forward.


