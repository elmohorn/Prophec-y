# v0.19 — Sound Matching and Optimisation

Status: **ACTIVE**

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
6. independent software implementations only as hypothesis/reference material, never as acoustic truth.

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

## 3. Evidence hierarchy

Use the strongest available evidence per parameter, rather than pretending all online recordings are equal.

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

### Grade D — hypothesis/reference only

- Phosphor and other compatible implementations;
- factory-bank statistics without corresponding audio;
- forum descriptions;
- compressed demos with unknown patch identity.

Grade D can suggest experiments but cannot independently establish Prophet-6 behavior.

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

## 6. v0.19 calibration order

Because available online references are uneven, the order is driven by **evidence quality**, not by an imaginary controlled capture session.

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
2. Prophec-y currently inherits Phosphor-style narrow-pulse mean removal plus peak normalization. This intentionally reduces narrow-pulse AC level by nearly 6 dB at the extreme and must remain a calibration candidate, not be treated as proven Prophet behavior.

Do not change either default until online hardware evidence favors a candidate over the existing model.

### P0-B — Hard sync

Prefer dry/low-FX examples with known oscillator intervals. Compare:

- perceived/fundamental pitch;
- harmonic distribution;
- spectral centroid;
- reset transient character;
- alias-region energy only inside the reliable bandwidth of the source.

### P0-C — LPF / HPF

Use factory programs and demonstrations where one filter dominates and envelope movement is limited or reconstructable. Fit LPF and HPF independently. The historical Reference-A cutoff candidates remain hypotheses, not immutable truth.

### P0-D — envelopes

Use segments with clean attacks/releases and repeated notes when available. Estimate event onset first, then compare time-domain envelopes. Do not fit codec pre-ringing or unknown key timing as envelope behavior.

### P0-E — modulation

Use examples with recognizable LFO/Poly Mod/aftertouch behavior, preferably when settings or program identity are known. Measure depth/frequency relationships only to the precision supported by the recording.

### P0-F — Distortion / Slop / Vintage

Aggregate multiple programs and longer passages where needed. Stochastic behavior is assessed statistically across recordings, not sample-for-sample.

### P0-G — digital FX

Use dry vs wet demonstrations, isolated effect demos and matching factory programs when available. Later-OS PH3/Ring/FL1/FL2 state encoding remains an independent evidence problem and must not be guessed.

## 7. Metrics appropriate for online audio

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

## 8. Candidate promotion rule

Existing `referenceACandidateV07/V08/V09()` are historical candidates and remain versioned.

New v0.19 candidates should use an explicit family such as `referenceCandidateV019A`, never silently mutate historical constants.

Promotion flow:

```text
real Prophet online evidence
  -> performance/event hypothesis
  -> isolated DSP candidate
  -> before/after multi-segment report
  -> v0.18 fingerprint review
  -> unrelated-reference regression review
  -> promote / reject
```

A candidate must improve more than the one clip that inspired it. Overfitting one compressed demo segment is a failure mode, not progress.

## 9. Search strategy for additional evidence

v0.19 should actively seek additional public Prophet-6 material that fills Reference-A gaps, especially:

- dry oscillator/PW sweeps;
- filter cutoff/resonance sweeps;
- envelope demonstrations;
- hard-sync demonstrations;
- Poly Mod demonstrations;
- isolated FX examples;
- videos where knob positions are visible and audio is direct.

Prefer multiple independent sources for the same law. A new source is valuable even when lower quality if it constrains a parameter that Reference-A cannot identify.

## 10. Tooling

Keep and extend tooling that is useful for online-reference analysis:

- 48/96 kHz Prophec-y reference renderer;
- event/performance renderer;
- calibration renderer;
- DSP fingerprints;
- spectral/timing probes;
- source/segment manifests and confidence metadata;
- automated before/after comparison reports.

The recently added controlled-capture manifest/schema and generated P0-A capture-pack tooling are **not v0.19 prerequisites**. They may remain as optional future infrastructure, but no roadmap item may depend on the user producing new Prophet-6 recordings.

## 11. Performance optimisation

Performance work continues under two guards:

- deterministic DSP fingerprints for accidental regressions;
- online real-hardware reference metrics for fidelity-sensitive changes.

Prefer algebraically exact optimisations and moving invariant work outside sample loops. Precision changes, approximations, SIMD substitutions or reduced update cadence require profiling evidence plus reference regression testing.

GitHub-hosted CPU percentages remain directional only; they are not absolute product CPU claims.

## 12. Immediate work order

1. Restore/reference the existing Reference-A reports, segments and inferred event files as the primary v0.19 dataset.
2. Inventory which of the 500-preset recording segments are high enough confidence for oscillator, filter, envelope, sync, modulation and FX work.
3. Search for additional online captures specifically targeting gaps in Reference-A.
4. Build a confidence-weighted automated comparison report across usable segments.
5. Re-run the existing v0.7/v0.8/v0.9 candidates through the larger reference set to establish the current baseline.
6. Attack P0-A hypotheses first where evidence supports them: pulse-width law, narrow-pulse normalization, shape morph and mixer law.
7. Promote no acoustic change unless it improves the weighted multi-source reference score without unacceptable regressions.
8. Profile and integrate only fingerprint-safe performance improvements alongside sound matching.

The custom/reactive FX visual concept remains **DEFERRED / UI** in the master matrix and is outside v0.19 sound-matching scope unless a later UI milestone deliberately pulls it forward.
