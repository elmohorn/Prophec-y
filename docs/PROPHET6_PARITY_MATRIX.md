# Prophet-6 parity matrix and master development plan

This is the **master scope document** for Prophec-y. The project goal is a stable Windows/Linux VST3 whose musical behavior and sound approach the Sequential Prophet-6 as closely as clean-room evidence allows, while preserving the hardware program/state contract and refusing to invent unknown firmware semantics.

## Status legend

- **VALIDATED** — implemented and covered by tests / CI / deterministic regression where applicable.
- **FUNCTIONAL / PROVISIONAL** — behavior exists and is usable, but calibration or exact hardware law remains provisional.
- **CONNECTED / INCOMPLETE** — state/parameter exists, but complete behavior is not implemented.
- **MISSING** — not implemented in the runtime path.
- **OPAQUE / MEASURE** — hardware semantics/storage are not sufficiently established; preserve state and do not guess.
- **DEFERRED / UI** — deliberate product/UI backlog; not a DSP or release blocker for the active milestone.

## Project rules

1. The 1024-byte Prophet-6 program payload remains canonical and lossless. Unknown bytes are preserved.
2. Do not invent undocumented SysEx offsets, NRPN meanings or firmware behavior to make a feature appear complete.
3. Hardware-domain state and calibration/DSP layers stay separate.
4. Reuse mature license-compatible work before rebuilding generic infrastructure.
5. Windows/Linux VST3, `pluginval --strictness-level 10`, core tests and Linux sanitizers are release gates.
6. UI work must not redefine synth behavior or place realtime work on the audio thread.
7. New ideas that do not close the active milestone go to backlog.

## Snapshot after v0.18

v0.18 closes the digital-effects milestone. Prophec-y now has one global realtime-safe serial FX A -> B path, audible DSP for every effect in the verified legacy selector domains, common-clock tempo sync, deterministic smoothing, bounded realtime state invalidation, DSP fingerprints and a repeatable performance benchmark.

The project remains deliberately conservative where the Prophet-6 documentation is contradictory or incomplete. Later-OS PH3/Ring/FL1/FL2 DSP exists internally, but their raw selector/control encoding is still **OPAQUE / MEASURE** and is not serialized by guesswork.

v0.18 also introduced safe measured optimisations: reverb coefficient caching, exact zero-history FIR skipping, base-pitch conversion caching and Pan Spread gain caching. The dedicated maximum-spread render remained bit-identical before/after Pan caching (`d0332e325eb4376a`).

At the v0.18 snapshot, the active milestone became v0.19. Current status: v0.19 engineering/research closeout is documented in `V0_19_CLOSEOUT.md`; acoustic laws remain provisional. v0.20 UI/preset workflow enters planning under `V0_20_UI_PLAN.md`.

---

## A. State, SysEx and parameter contract

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| 1024-byte canonical program payload | VALIDATED | Full raw payload preserved | Keep immutable contract | Ongoing gate |
| Program vs Edit Buffer SysEx | VALIDATED | Correct `0x02` / `0x03` handling | Maintain tests | Done |
| Sequential 7-bit pack/unpack | VALIDATED | Codec + bank scanning | Fuzz malformed streams further | Ongoing |
| Known serialized parameter mapping | VALIDATED | Verified entries only | Extend only with evidence | Ongoing |
| Unknown serialized bytes | VALIDATED | Preserved losslessly | Never reinterpret without evidence | Ongoing |
| Program name NRPN 236..255 | VALIDATED | Payload/name updates | UI editing later | v0.20 |
| Sequence note/velocity NRPN 256..1023 | VALIDATED | Stored bytes updated in place | Preserve opaque metadata | Done |
| Sequence controls NRPN 168..171 | FUNCTIONAL / PROVISIONAL | State retained; no guessed SysEx offsets | Identify offsets only from evidence | v0.19+ |
| Sequence velocity bit 7 | OPAQUE / MEASURE | Preserved | Determine tie/retrigger semantics | v0.19+ |
| Chord-memory program storage | OPAQUE / MEASURE | Runtime chord works; program load clears runtime capture | Differential hardware SysEx captures | v0.19+ |
| Host state codec | VALIDATED | Versioned, JUCE-independent, 1024-byte round trip | Keep backward compatibility | Ongoing |

## B. Voice engine and analog signal path

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Six-voice polyphony | VALIDATED | Six independent `P6Voice` slots | DAW stress/steal soak | v0.21 |
| Voice allocation / steal | FUNCTIONAL / PROVISIONAL | Free, released, then oldest | Hardware edge behavior | v0.19 |
| Oscillator pitch / coarse / fine | FUNCTIONAL / PROVISIONAL | Free-running VCOs and mapped raw domains | Final calibration | v0.19 |
| Tri -> Saw -> Pulse morph | FUNCTIONAL / PROVISIONAL | Anti-aliased trapezoid-family oscillator | Hardware spectral sweep | v0.19 |
| Pulse width / PWM | FUNCTIONAL / PROVISIONAL | Continuous duty + modulation | Endpoint/law calibration | v0.19 |
| Osc2 LF / key-track-off | FUNCTIONAL / PROVISIONAL | Implemented; fixed reference when key track off | Range/A-B calibration | v0.19 |
| Sub oscillator | FUNCTIONAL / PROVISIONAL | Follows modulated Osc1 | Gain/waveform calibration | v0.19 |
| Noise / mixer levels | FUNCTIONAL / PROVISIONAL | Working paths | Level/color/taper calibration | v0.19 |
| Hard sync Osc2 -> Osc1 | FUNCTIONAL / PROVISIONAL | Sub-sample sync + anti-alias correction | Known residual spectral mismatch | v0.19 |
| HPF | FUNCTIONAL / PROVISIONAL | Resonant TPT 2-pole candidate | Cutoff/resonance calibration | v0.19 |
| LPF | FUNCTIONAL / PROVISIONAL | Resonant 4-pole SSM2040-lineage candidate | Law/resonance/saturation calibration | v0.19 |
| Filter ordering | FUNCTIONAL / PROVISIONAL | Mixer -> HPF -> LPF -> VCA | Edge A/B | v0.19 |
| Amp / Filter ADSR | FUNCTIONAL / PROVISIONAL | Independent exponential/taper hooks | Exact timing/depth laws | v0.19 |
| Velocity -> amp/filter | FUNCTIONAL / PROVISIONAL | Routed | Depth calibration | v0.19 |
| VCA / program volume | FUNCTIONAL / PROVISIONAL | Working gain path | Final gain staging | v0.19 |
| Pan Spread | FUNCTIONAL / PROVISIONAL | Six fixed per-voice positions; gains cached outside sample loop | Exact hardware spread law | v0.19 |
| Analog distortion | FUNCTIONAL / PROVISIONAL | Post-sum asymmetric candidate + DC blocker | Transfer/gain calibration | v0.19 |
| Standard Slop / Vintage | FUNCTIONAL / PROVISIONAL | Drift and per-voice variation candidates | Distribution/global calibration | v0.19 |

## C. Modulation

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Global LFO waveforms/rate | FUNCTIONAL / PROVISIONAL | Triangle, saw, reverse saw, square, random/noise; audio-rate capable | Final waveform/rate calibration | v0.19 |
| LFO Initial Amount | VALIDATED behavior / provisional depth | Routed | Depth calibration | v0.19 |
| Mod Wheel -> LFO depth | VALIDATED behavior | Remaining-range model | Hardware A/B | v0.19 |
| LFO destinations | FUNCTIONAL / PROVISIONAL | Osc pitch/PW/VCA/filters | Depth calibration | v0.19 |
| LFO Sync | CONNECTED / INCOMPLETE | Evidence-backed first-key phase reset | Exact raw-frequency -> rhythmic division law | OPAQUE / v0.19+ |
| Channel pressure | FUNCTIONAL / PROVISIONAL | Destinations including LFO amount | Depth calibration | v0.19 |
| Poly Mod sources/destinations | FUNCTIONAL / PROVISIONAL | Filter Env + Osc2; Osc1 pitch/shape/PW + LPF/HPF | Range/bandwidth calibration | v0.19 |

## D. Performance behavior

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Pitch bend | VALIDATED behavior | Bipolar, program range | Edge A/B | v0.19 |
| Unison / key modes / retrigger | VALIDATED behavior | 1..6 voices, Low/High/Last variants | Hardware A/B | v0.19 |
| Chord memory runtime | VALIDATED behavior | Up to six intervals | Persistence remains opaque | v0.19+ |
| Sustain CC64 | VALIDATED | Physical-key and sustained-voice lifetime separated | Edge A/B | Regression gate |
| Foot Controller CC4 | OPAQUE / MEASURE | Not guessed | Establish mapping from evidence | v0.19+ |
| MIDI Volume CC7 | VALIDATED | Runtime gain independent of saved Voice Volume | Gain law if needed | Regression gate |
| CC120 / CC121 / CC123 | VALIDATED | Distinct channel-mode/controller semantics | Maintain regression | Done |
| Full panel MIDI CC map | MISSING / PARTIAL | Mature NRPN + evidence-backed CCs | Extend only with evidence | Ongoing |

## E. Clock, arpeggiator and sequencer

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Internal / host PPQ / MIDI Clock | VALIDATED | One sample-domain scheduler; 24 PPQN and transport | Long real-host/hardware soak | v0.21 |
| 10 clock divisions | VALIDATED | All represented | Maintain tests | Done |
| Swing ratio | OPAQUE / MEASURE | Correct average division, straight fallback | Measure long:short ratio | v0.19+ |
| Arp modes/range/HOLD/Beat Sync | VALIDATED or FUNCTIONAL / PROVISIONAL | Up/Down/Up+Down/Random/Assign; 1..3 octaves | RNG/edge A-B where unknown | v0.19 |
| Arp realtime ordering | VALIDATED | Fixed-array deterministic ordering | Keep allocation gate | Done |
| Stored sequencer 6x64 | VALIDATED decode | 384 cells preserved | Unknown flags remain opaque | Done |
| Sequencer playback/transposition | FUNCTIONAL / PROVISIONAL | Six tracks, velocity low 7 bits, middle-C transpose | Tie/mode/play-mode semantics | v0.19+ |
| Seq Mode / Play Mode | OPAQUE / MEASURE | State preserved | Hardware evidence | v0.19+ |
| Seq Play/Stop NRPN 1088 | VALIDATED | Runtime-only | Maintain tests | Done |
| Generated MIDI output | MISSING / OPTIONAL | Internal generated notes only | Optional after core parity | Backlog |

## F. Effects — v0.18 CLOSED

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| FX A/B program state | VALIDATED | Verified raw domains preserved/mapped | Later-OS selectors only with evidence | Ongoing |
| FX global on/off | VALIDATED | Bit-transparent true bypass at DSP boundary | Exact bypass-tail evolution on hardware | OPAQUE / MEASURE |
| Serial A -> B architecture | VALIDATED | One global realtime-safe chain in `P6Engine` | Maintain tests | Done |
| BBD delay | FUNCTIONAL / PROVISIONAL | Audible DSP; bounded feedback, repeat loss/wander | Acoustic hardware calibration | v0.19 |
| Digital delay | FUNCTIONAL / PROVISIONAL | Audible DSP | Acoustic/time-law calibration | v0.19 |
| Chorus | FUNCTIONAL / PROVISIONAL | Stereo modulated-delay candidate | Acoustic calibration | v0.19 |
| PH1 / PH2 | FUNCTIONAL / PROVISIONAL | Audible all-pass candidates | Acoustic calibration | v0.19 |
| Hall / Room / Plate / Spring | FUNCTIONAL / PROVISIONAL | Audible preallocated stereo FDN candidates | Acoustic calibration | v0.19 |
| PH3 / Ring / FL1 / FL2 internal DSP | FUNCTIONAL / PROVISIONAL | Executable internal candidates + tests | Raw selector/control encoding unknown | OPAQUE / MEASURE |
| Ring low-note tracking | OPAQUE / MEASURE | Not wired to guessed state | Establish control identity | Hardware evidence |
| FX tempo sync | VALIDATED | Uses existing internal/host/MIDI common clock | Exact hardware timing A/B | v0.19 |
| FX parameter smoothing | VALIDATED contract | Deterministic click-safe interpolation | Hardware smoothing law optional | v0.19 |
| FX state transitions | VALIDATED | Old algorithm state invalidated on type change | Hardware-tail nuance only | Done |
| CC120 effect clear | VALIDATED | O(1)/bounded logical invalidation; immediate silence | Maintain realtime regression | Done |

## G. MIDI and realtime host integration

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Sample-offset MIDI | VALIDATED | JUCE MIDI applied at sample positions | DAW soak | v0.21 |
| Mod Wheel / bend / pressure | VALIDATED | Unified router | Maintain regression | Done |
| NRPN + Data Inc/Dec | VALIDATED | Complete modeled stream | Fuzz more malformed ordering | Ongoing |
| Host automation <-> raw patch | VALIDATED | Per-NRPN edits | Real-host conflict soak | v0.21 |
| Host-vs-MIDI causal ordering | VALIDATED | Versioned monotonic last-writer-wins mailbox | Maintain cross-thread test | Done |
| Realtime allocation proof | VALIDATED | Explicit contract tests + fixed-array arp ordering | Extend with new subsystems | Ongoing |
| SysEx inside audio callback | CONNECTED / INCOMPLETE | Recognized but not decoded realtime | RT->message handoff only if required | Backlog |
| Realtime locks | FUNCTIONAL / PROVISIONAL | Nonblocking/try-enter strategy | Stress state edits in hosts | v0.21 |

## H. VST3 wrapper and platform support

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| JUCE isolation from core | VALIDATED | Optional wrapper; C++20 core independent | Maintain boundary | Ongoing |
| Windows VST3 | VALIDATED | Release CI + pluginval 10 | DAW matrix | v0.21 |
| Linux VST3 | VALIDATED | Release CI + pluginval 10 | DAW matrix | v0.21 |
| Linux ASan+UBSan | VALIDATED | Release gate | Keep green | Ongoing |
| VST3 state restoration | VALIDATED | pluginval + codec tests | Real project matrix | v0.21 |
| Host tempo/PPQ | VALIDATED basic path | Common clock | Multi-host loop/reposition tests | v0.21 |
| Oversampling setting | VALIDATED host behavior | Applied only during prepare | UX/restart semantics | v0.20 |
| Standalone | SECONDARY | Optional build | Not a VST3 release blocker | Backlog |

## I. UI, preset workflow and visual identity

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Diagnostic parameter UI | VALIDATED | JUCE Generic editor | Keep debug fallback | Ongoing |
| Custom editor | MISSING | None | Design after sound/parameter contract settles | v0.20 |
| Preset browser / SysEx UI / Init-Compare-A-B | MISSING | Core APIs exist underneath | User-facing workflow design | v0.20 |
| Sequencer visual editor | MISSING | Runtime/data exist | Build after semantics stabilize | v0.20 |
| **Reactive character FX visuals** | **DEFERRED / UI** | Concept backlog only; no DSP coupling yet | Refine visual language and UI architecture before implementation | **v0.20+ / UI exploration** |

### Reactive character FX visual backlog

The following ideas are intentionally parked until the custom UI language is better defined:

- tube saturation with a visible glowing valve/heat response;
- plasma-style effect/pedal with visible electrical discharges;
- spring reverb with animated vibrating springs;
- tape echo with rotating reels/tape motion;
- optical tremolo with a pulsing lamp/photo-cell metaphor;
- ring modulation with a miniature oscilloscope/carrier visual;
- ferrofluidic distortion with magnetic-liquid deformation;
- shimmer reverb with refracting/dispersing light particles in a translucent surface.

Architecture rule for this future work: visuals must consume low-cost telemetry copied from DSP/state, render on the UI thread at a bounded frame rate, support reduced-motion/visual-off modes, and never alter the audio contract or add expensive analysis solely to drive decoration.

## J. Acoustic validation, regression and performance optimisation

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| DSP fingerprints | VALIDATED | Dry, sync/poly, FX, 1x and nonzero-Pan deterministic snapshots | Expand around calibration changes | Ongoing |
| Reference-A harness | FUNCTIONAL / PROVISIONAL | Spectral/envelope/stereo comparison suite | Compressed web source is not calibration truth | v0.19 |
| Real Prophet-6 acoustic corpus | FUNCTIONAL / PROVISIONAL | Reference-A plus independent online captures; optional controlled-capture tools | Extend usable patch/event coverage; no requirement for owner-provided hardware | v0.19; see V0_19_SOUND_MATCHING.md |
| Performance benchmark | VALIDATED infrastructure | 1/6 voices, FX off/on, 48/96 kHz, oversampling, avg/worst block | Add stable local-machine baselines | v0.19 |
| Reverb coefficient cache | VALIDATED optimisation | Parameter-invariant math outside sample loop; sample-rate invalidation tested | Maintain fingerprint | Done |
| Zero-history FIR skip | VALIDATED optimisation | Exact zero convolution shortcut; free-running state retained | Maintain fingerprint | Done |
| Base pitch conversion cache | VALIDATED optimisation | Reuses Hz conversion only when effective base pitch unchanged | Maintain modulation regression | Done |
| Pan gain cache | VALIDATED optimisation | Six `sin/cos` gains recomputed only on spread changes | Exact hardware spread law remains calibration work | Done |
| Cutoff cache experiment | NOT INTEGRATED | Experimented separately | Reconsider only after profiling/calibration | v0.19 |
| Further CPU optimisation | DEFERRED / PROFILE | Final cloud snapshots recorded; no new approximation promoted | Dedicated-machine profiling and regression before any further change | Follow-up / v0.21 load testing |

---

# Milestone plan

## v0.16 — Stability gate — **CLOSED**

Windows/Linux VST3, pluginval strictness 10, Linux sanitizers, release-mode tests, chord-memory fix and serialized-layout integrity closed.

## v0.17 — Control/performance contract — **CLOSED**

Host/MIDI causal ordering, realtime allocation regression, sustain, MIDI Volume/channel-mode CC behavior, evidence-backed synced-LFO phase reset and realtime arp ordering closed. Unknown hardware semantics remained explicit rather than fabricated.

See `V0_17_CONTROL_CONTRACT.md` and `V0_17_CLOSEOUT.md`.

## v0.18 — Digital effects — **CLOSED**

Completed release scope:

1. One realtime-safe global serial FX A -> B architecture.
2. Audible DSP for every effect in the verified legacy selector domains.
3. Internal candidate DSP for PH3/Ring/FL1/FL2 without guessed serialization.
4. Common-clock tempo sync.
5. True bypass, deterministic parameter smoothing and state-transition hardening.
6. Bounded-time CC120 effect invalidation.
7. Deterministic DSP fingerprints and repeatable performance benchmarking.
8. Safe measured optimisations: reverb coefficients, zero-history FIR, base pitch conversion and Pan Spread gains.
9. Windows/Linux VST3, CTest, pluginval strictness 10 and Linux ASan+UBSan release gates.

**Exit criterion met:** every verified legacy effect control exposed to the host has runtime behavior; unsupported/unknown later-OS selector semantics remain explicitly opaque rather than guessed.

See `V0_18_FX_AUDIT.md` and `V0_18_CLOSEOUT.md`.

## v0.19 — Sound matching and optimisation — **CLOSED: ENGINEERING CHECKPOINT**

Closeout: [V0_19_CLOSEOUT.md](V0_19_CLOSEOUT.md), with [final cutoff/envelope results](V0_19_FINAL_CALIBRATION.md). The completed final phase retained Default after static-reference identifiability review and 100 envelope renders. All remaining acoustic uncertainty stays explicit; no provisional row becomes validated merely because this milestone closes. The original scope is preserved below for traceability.
- Available real Prophet-6 recordings as primary evidence: Reference-A and independently sourced online audio. Controlled hardware captures are optional; the owner is not required to obtain a unit.
- Oscillator/PW/mixer gain staging and hard-sync residual.
- HPF/LPF cutoff/resonance/saturation/envelope behavior.
- ADSR, glide, Slop/Vintage and analog distortion.
- Poly-Mod/LFO/pressure depth calibration.
- FX acoustic calibration.
- CPU optimisation driven by profiling and protected by fingerprints/Reference-A.
- Adaptive oversampling only if measured quality/CPU data justify it.

**Original acoustic exit criterion (not fully established; retained for future sound-matching work):** documented listening/measurement coverage of representative basic, sync, PWM, filter, modulation, unison and FX patches with no regression outside agreed tolerances.

**Engineering closeout adopted for the requested transition to UI:** freeze the tested sound/state contract, record rejected candidates and unidentifiable laws, complete the final cutoff/envelope investigation and regression/CPU snapshots, and pass Windows/Linux VST3, pluginval10, sanitizers and analysis gates. This is an explicit bounded research checkpoint, not a full acoustic-parity certification. Calibration rows with v0.19 targets above remain evidence/backlog records where their remaining-proof column is unresolved; see the closeout ledger.

## v0.20 — Custom UI and preset workflow — **PLANNING**

See [V0_20_UI_PLAN.md](V0_20_UI_PLAN.md). Proposed hybrid visual direction is awaiting user design review; no bespoke editor is implemented yet.

- Bespoke editor and final visual language.
- Preset/bank browser and SysEx import/export.
- Sequence editor.
- Init / compare / A-B workflow.
- Evaluate the deferred reactive/character FX visual concepts only after core UI architecture, accessibility/reduced-motion behavior and performance budget are defined.
- Keep Generic JUCE editor as diagnostic fallback.

**Exit criterion:** every visible user control maps to a functional/explicitly documented matrix row; no decorative dead controls and no UI work on the realtime audio thread.

## v0.21 — Public beta hardening

- Windows/Linux DAW matrix.
- Project save/reload and automation regression in real hosts.
- Long-running MIDI/clock/voice-steal soak tests.
- CPU/load tests at 44.1/48/96 kHz and common block sizes.
- Packaging/install instructions and license review.

---

# Active milestone discipline

There is one active milestone at a time. Work only enters it when it closes an assigned matrix row or fixes a release-blocking regression. Everything else remains backlog.

**Active milestone after v0.19 engineering closeout: v0.20 — Custom UI and preset workflow (planning).**

