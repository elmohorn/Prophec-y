# Prophet-6 parity matrix and master development plan

This file is the **master scope document** for Prophec-y. It exists to prevent local fixes, UI work, or interesting experiments from displacing the actual goal: a stable Windows/Linux VST3 whose musical behavior and sound approach the Sequential Prophet-6 as closely as clean-room evidence allows.

## Status legend

- **VALIDATED** — implemented and covered by tests / CI / deterministic regression where applicable.
- **FUNCTIONAL / PROVISIONAL** — behavior exists and is usable, but calibration or exact hardware law remains provisional.
- **CONNECTED / INCOMPLETE** — state/parameter is present, but the complete musical/DSP behavior is not yet implemented.
- **MISSING** — not implemented in the runtime path.
- **OPAQUE / MEASURE** — hardware semantics or storage are not sufficiently established; preserve state and do not guess.

## Project rules

1. The 1024-byte Prophet-6 program payload remains the canonical hardware-compatible state. Unknown bytes are preserved.
2. Do not invent undocumented SysEx offsets or firmware semantics to make a UI control appear complete.
3. Do not change a hardware-domain mapping to fit a single preset. Calibration belongs in replaceable DSP/calibration layers.
4. Reuse mature, license-compatible work before implementing generic infrastructure from scratch.
5. Windows and Linux are the release targets. A milestone is not release-ready unless both VST3 builds pass tests and `pluginval --strictness-level 10`; Linux sanitizers must also remain green.
6. Custom UI work must not redefine synth behavior. The engine/parameter contract comes first.
7. New ideas that do not close a row in this matrix go to backlog instead of interrupting the active milestone.

## Snapshot after v0.16

The project is **not a finished Prophet-6 emulation yet**. It is a mature synthesis/compatibility core wrapped in a now-valid Windows/Linux VST3. The strongest areas are state compatibility, the six-voice engine architecture, hard-sync/Poly-Mod infrastructure, performance scheduling, NRPN/SysEx handling, and host integration. The largest functional gaps are digital effects, LFO clock sync, several performance MIDI controls, exact sequencer edge semantics, chord-memory persistence, and final acoustic calibration.

The current JUCE generic editor is diagnostic only. A control being visible there means the host parameter exists; it does **not** imply that the corresponding hardware behavior is complete.

---

## A. State, SysEx and parameter contract

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| 1024-byte canonical program payload | VALIDATED | `P6PatchState` preserves full raw payload | Keep immutable contract | Done / regression gate |
| Program vs Edit Buffer SysEx | VALIDATED | Correct `0x02` / `0x03` handling | Maintain tests | Done |
| Sequential 7-bit pack/unpack | VALIDATED | Codec + concatenated-bank scanning | Fuzz malformed streams further | v0.17 |
| Known serialized parameter mapping | VALIDATED | 87 verified entries after v0.16 cardinality fix | Extend only with evidence | Ongoing |
| Unknown serialized bytes | VALIDATED | Preserved losslessly | Never reinterpret without evidence | Ongoing |
| Program name NRPN 236..255 | VALIDATED | Updates payload and visible name | UI text editing later | v0.20 |
| Sequence note/velocity NRPN 256..1023 | VALIDATED | Updates stored sequence bytes in place | Preserve unknown metadata | Done |
| Sequence controls NRPN 168..171 | FUNCTIONAL / PROVISIONAL | Kept as NRPN state; no guessed SysEx offsets | Identify storage offsets only from evidence | v0.17+ |
| Sequence velocity bit 7 | OPAQUE / MEASURE | Preserved untouched | Determine tie/retrigger/other semantics from hardware or strong public evidence | v0.17/v0.19 |
| Chord-memory program storage | OPAQUE / MEASURE | Runtime chord exists; program load clears it | Differential real-hardware SysEx captures to locate storage | v0.17/v0.19 |
| Host state codec | VALIDATED | Versioned JUCE-independent codec, 1024-byte round-trip | Keep backward compatibility when format evolves | Ongoing |

## B. Voice engine and analog signal path

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Six-voice polyphony | VALIDATED | Six independent `P6Voice` slots | DAW stress/voice-steal soak | v0.21 |
| Voice allocation / steal | FUNCTIONAL / PROVISIONAL | Free voice, released voice, then oldest voice | Compare subtle hardware stealing behavior if evidence appears | v0.19 |
| Oscillator 1/2 pitch | FUNCTIONAL / PROVISIONAL | Hardware raw domains mapped; free-running VCOs | Final pitch/slop calibration | v0.19 |
| Continuous Tri -> Saw -> Pulse morph | FUNCTIONAL / PROVISIONAL | Anti-aliased trapezoid-family oscillator | Hardware spectral calibration across shape sweep | v0.19 |
| Pulse width / PWM | FUNCTIONAL / PROVISIONAL | Continuous duty control, modulation support | Endpoint law and normalization calibration | v0.19 |
| Osc2 fine tune | FUNCTIONAL / PROVISIONAL | +/-50-cent physical interpretation | Verify exact control law | v0.19 |
| Osc2 low-frequency mode | FUNCTIONAL / PROVISIONAL | Implemented | Hardware range calibration | v0.19 |
| Osc2 keyboard tracking off | VALIDATED | Fixed reference pitch while bend remains active | Behavioral A/B if hardware available | v0.19 |
| Osc1 sub oscillator | FUNCTIONAL / PROVISIONAL | Follows modulated Osc1 frequency | Gain/waveform calibration | v0.19 |
| Noise source | FUNCTIONAL / PROVISIONAL | White noise path | Level/color calibration | v0.19 |
| Mixer levels | FUNCTIONAL / PROVISIONAL | Independent oscillator/sub/noise levels | Gain taper / drive interaction calibration | v0.19 |
| Hard sync Osc2 -> Osc1 | FUNCTIONAL / PROVISIONAL | Sub-sample sync + anti-alias correction diagnostics | **Known spectral mismatch** on Sync Stack; likely analog reset/recovery behavior | v0.19 |
| HPF | FUNCTIONAL / PROVISIONAL | Resonant TPT 2-pole candidate | Cutoff/resonance/key-track/env calibration | v0.19 |
| LPF | FUNCTIONAL / PROVISIONAL | Resonant 4-pole SSM2040-lineage candidate | Cutoff law, resonance, saturation, self-oscillation calibration | v0.19 |
| Filter ordering | FUNCTIONAL / PROVISIONAL | Mixer -> HPF -> LPF -> VCA | Confirm edge interactions with hardware captures | v0.19 |
| Amp ADSR | FUNCTIONAL / PROVISIONAL | Exponential envelope, separate calibration hooks | Exact attack/decay/release law | v0.19 |
| Filter ADSR | FUNCTIONAL / PROVISIONAL | Independent envelope/taper hooks | Exact timing and depth law | v0.19 |
| Velocity -> amp/filter | FUNCTIONAL / PROVISIONAL | Mapped and routed | Hardware depth calibration | v0.19 |
| VCA / program volume | FUNCTIONAL / PROVISIONAL | Working output gain path | Final gain staging calibration | v0.19 |
| Pan Spread | FUNCTIONAL / PROVISIONAL | Per-voice stereo positions | Exact hardware spread law | v0.19 |
| Analog distortion | FUNCTIONAL / PROVISIONAL | Post-sum asymmetric candidate + DC blocker | Transfer-curve / gain staging calibration | v0.19 |
| Standard Slop | FUNCTIONAL / PROVISIONAL | Independent slow pitch drift | Amount/rate distribution calibration | v0.19 |
| Vintage behavior | FUNCTIONAL / PROVISIONAL | Per-voice pitch/filter/envelope/gain variation candidate | Hardware/global behavior calibration | v0.19 |

## C. Modulation

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Global LFO waveforms | FUNCTIONAL / PROVISIONAL | Triangle, saw, reverse saw, square, random/noise behavior | Rate/waveform calibration | v0.19 |
| LFO rate | FUNCTIONAL / PROVISIONAL | Raw-to-Hz mapping | Final hardware law | v0.19 |
| LFO Initial Amount | VALIDATED behavior / provisional depth | Routed | Depth calibration | v0.19 |
| Mod Wheel -> LFO depth | VALIDATED behavior | `initial + wheel*(1-initial)` model | Hardware A/B only | v0.19 |
| LFO destinations | FUNCTIONAL / PROVISIONAL | Osc1/2 pitch, PW, amp, LPF, HPF routing | Destination depth calibration | v0.19 |
| **LFO Sync** | CONNECTED / INCOMPLETE | Parameter decoded and retained as `lfoClockSync_` | Connect LFO phase/rate to common Internal/Host/MIDI clock | **v0.17 P0** |
| Channel pressure amount/destinations | FUNCTIONAL / PROVISIONAL | Pressure routing including LFO amount | Final depth calibration | v0.19 |
| Poly Mod filter-envelope source | FUNCTIONAL / PROVISIONAL | Audio-rate-capable source/dest path | Range calibration | v0.19 |
| Poly Mod Osc2 source | FUNCTIONAL / PROVISIONAL | Audio-rate modulation | Range and analog bandwidth calibration | v0.19 |
| Poly Mod -> Osc1 pitch | FUNCTIONAL / PROVISIONAL | 4-octave candidate span | Hardware sweep | v0.19 |
| Poly Mod -> Osc1 shape/PW | FUNCTIONAL / PROVISIONAL | Full morph / duty candidates | Hardware sweep | v0.19 |
| Poly Mod -> LPF/HPF | FUNCTIONAL / PROVISIONAL | 7-octave candidate span | Hardware sweep | v0.19 |

## D. Performance behavior

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Pitch bend | VALIDATED behavior | Bipolar bend, program range capped physically to 12 semitones while raw state is preserved | Verify range edge cases | v0.17/v0.19 |
| Unison voice count | VALIDATED behavior | 1..6 stack, CHD selects six | Final hardware behavior A/B | v0.19 |
| Key modes Low/High/Last | VALIDATED behavior | Implemented | Hardware A/B | v0.19 |
| Retrigger variants | VALIDATED behavior | Low/High/Last retrigger from zero | Hardware A/B | v0.19 |
| Chord memory runtime | VALIDATED behavior | Capture up to six intervals; high/low anchoring | Program persistence unknown | v0.17/v0.19 |
| Sustain pedal CC64 | MISSING | No sustain state in MIDI router/engine | Implement pedal-held release semantics | **v0.17 P0** |
| Foot Controller CC4 | MISSING | Not routed | Map documented behavior without duplicating parameter logic | v0.17 |
| MIDI Volume CC7 | MISSING | Not routed | Decide hardware-compatible placement vs host output gain | v0.17 |
| Full panel MIDI CC map | MISSING / PARTIAL | NRPN path is mature; CC panel translation incomplete | Translate CC -> existing NRPN semantics | v0.17 |

## E. Clock, arpeggiator and sequencer

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Internal clock | VALIDATED | Sample-domain scheduler | Long-duration drift soak | v0.21 |
| Host/DAW PPQ clock | VALIDATED | Host PPQ anchor, no second wrapper clock | Multi-host reposition/loop tests | v0.21 |
| MIDI Clock 24 PPQN | VALIDATED | Start/Continue/Stop + no-start/stop policy support | External hardware soak | v0.17/v0.21 |
| 10 clock divisions | VALIDATED | All documented divisions represented | Keep regression tests | Done |
| Swing divisions | OPAQUE / MEASURE | Correct average division; straight fallback | Measure actual long:short ratio | v0.19 |
| Arp Up / Down / Up-Down | VALIDATED behavior | Sample-accurate scheduler | Hardware A/B endpoints | v0.19 |
| Arp Random | FUNCTIONAL / PROVISIONAL | Random mode works | Exact Prophet RNG/order not claimed | v0.19 |
| Arp Assign | FUNCTIONAL / PROVISIONAL | Dedicated 32-press scratchpad; repeated pitches preserved | Hardware sequence limits/edge behavior | v0.19 |
| Arp Range 1..3 octaves | VALIDATED behavior | Implemented | Hardware A/B | v0.19 |
| Arp Hold / relatch | VALIDATED behavior | Implemented | Host/UI exposure later | v0.20 |
| Arp Beat Sync | VALIDATED behavior | Immediate vs next-grid behavior | Hardware A/B | v0.19 |
| Stored sequencer decode 6x64 | VALIDATED | 384 cells decoded losslessly | Preserve unknown flag | Done |
| Sequencer playback | FUNCTIONAL / PROVISIONAL | Six tracks, length, velocity low-7-bit, transposition | Tie/retrigger and mode semantics incomplete | v0.17/v0.19 |
| Sequencer transpose | VALIDATED behavior | Middle-C reference | Hardware A/B | v0.19 |
| Sequencer Mode NRPN 170 | OPAQUE / MEASURE | State preserved | Determine exact behavior | v0.17/v0.19 |
| Sequencer Play Mode NRPN 171 | OPAQUE / MEASURE | State preserved | Determine exact behavior | v0.17/v0.19 |
| Seq Play/Stop NRPN 1088 | VALIDATED | Runtime-only, not serialized into program | Keep state/host regression | Done |
| Generated MIDI output from arp/seq | MISSING | Internal generated notes only | Optional MIDI output parity after core behavior is stable | v0.18+ |

## F. Effects

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| FX A/B program state | VALIDATED state | Types, mix, params, sync/rate domains mapped and preserved | Runtime DSP absent | v0.18 |
| FX global on/off | VALIDATED state / MISSING DSP | Parameter exists | Wire true effect bypass once DSP exists | v0.18 |
| Delay / BBD-style effects | MISSING | No digital FX processing in engine | Reuse/adapt license-compatible mature patterns before writing from zero | **v0.18** |
| Chorus | MISSING | No runtime DSP | Same | v0.18 |
| Phaser | MISSING | No runtime DSP | Implement/calibrate | v0.18 |
| Flanger | MISSING | No runtime DSP | Implement/calibrate | v0.18 |
| Ring modulation | MISSING | No runtime DSP | Implement/calibrate | v0.18 |
| Reverbs | MISSING | No runtime DSP | Implement/calibrate | v0.18 |
| FX serial ordering A -> B | MISSING runtime | State exists only | Build one shared realtime-safe FX chain | v0.18 |
| FX tempo sync | MISSING runtime | Sync controls mapped | Use common scheduler/clock; do not invent another clock | v0.18 |

## G. MIDI and realtime host integration

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Note On/Off sample offsets | VALIDATED | JUCE MIDI consumed at sample positions | DAW soak | v0.21 |
| Mod Wheel / pitch bend / channel pressure | VALIDATED | Unified `P6MidiRouter` | Keep regression | Done |
| Complete NRPN stream | VALIDATED | CC99/98 + CC6/38 | Fuzz malformed/reordered streams | v0.17 |
| Data Increment/Decrement CC96/97 | VALIDATED | Implemented | Edge-domain tests | v0.17 |
| RPN reset 127/127 | VALIDATED | Correct pair semantics | Keep regression | Done |
| Host automation -> raw patch | VALIDATED basic path | Individual NRPN updates, no whole-patch overwrite | More conflict tests | v0.17 |
| MIDI NRPN -> audio patch | VALIDATED basic path | Sample-accurate application | More conflict tests | v0.17 |
| **Host automation vs pending MIDI mirror ordering** | CONNECTED / INCOMPLETE | Separate audio/canonical patch domains and atomic mailbox exist | Add explicit causal ordering/generation test so stale UI mirror can never beat a newer host edit | **v0.17 P0** |
| SysEx received inside audio callback | CONNECTED / INCOMPLETE | Recognised but deliberately not decoded realtime | Fixed RT->message-thread queue | v0.17 |
| Realtime allocation proof | FUNCTIONAL / PROVISIONAL | Major known allocations moved out of callback; buffers reserved | Add explicit no-allocation / realtime guard test | **v0.17 P0** |
| Realtime lock behavior | FUNCTIONAL / PROVISIONAL | `tryEnter()` avoids blocking audio thread | Stress preset/state edits while processing | v0.17/v0.21 |

## H. VST3 wrapper and platform support

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| JUCE isolation from core | VALIDATED | JUCE optional; core remains C++20-only | Maintain boundary | Ongoing |
| Windows VST3 build | VALIDATED | GitHub Actions build PASS | Real DAW matrix | v0.21 |
| Linux VST3 build | VALIDATED | GitHub Actions build PASS | Real DAW matrix | v0.21 |
| `pluginval` strictness 10 Windows | VALIDATED | PASS in v0.16 gate | Keep release gate | Ongoing |
| `pluginval` strictness 10 Linux | VALIDATED | PASS in v0.16 gate | Keep release gate | Ongoing |
| Linux ASan+UBSan | VALIDATED | PASS | Keep release gate | Ongoing |
| State restoration through VST3 | VALIDATED | pluginval + independent codec tests | DAW project restore matrix | v0.21 |
| Host tempo/PPQ | VALIDATED basic path | JUCE playhead -> common clock | Multi-host loops/reposition tests | v0.21 |
| Oversampling quality setting | VALIDATED host behavior | Applied only from `prepareToPlay()` to avoid RT reallocations | Better UX/restart semantics later | v0.20 |
| Standalone | SECONDARY | Build option retained | Not a Windows/Linux VST3 release blocker | Backlog |

## I. UI, preset workflow and user-facing instrument

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Diagnostic parameter UI | VALIDATED | JUCE `GenericAudioProcessorEditor` | Retain as debug option | Ongoing |
| Custom Prophet-style editor | MISSING | None | Build only after parameter/behavior contract stabilises | **v0.20** |
| Panel controls connected to real DSP | PARTIAL by definition | Generic editor exposes host parameters | Matrix rows above decide whether each control is truly functional | v0.17-v0.20 |
| Preset browser | MISSING | Host state and SysEx APIs exist underneath | Browser, bank/program navigation, favorites/search later | v0.20 |
| SysEx import/export UI | MISSING | Non-realtime APIs exist | File picker + validation + error UX | v0.20 |
| Init / compare / A-B workflow | MISSING | Core can load/update states | Design after state contract is frozen | v0.20 |
| Sequencer visual editor | MISSING | Sequence data/runtime exist | UI only after semantics are clearer | v0.20 |
| Calibration/diagnostic UI | MISSING / OPTIONAL | CLI diagnostics are mature | Keep developer-only unless genuinely useful | Backlog |

## J. Acoustic validation and performance optimisation

| Feature | Status | Current implementation | Remaining work / proof | Target |
|---|---|---|---|---|
| Deterministic DSP regression | VALIDATED | Controlled hard-sync probe remains byte-identical across wrapper/stability work | Keep immutable golden probes | Ongoing |
| Reference-A comparison harness | FUNCTIONAL / PROVISIONAL | Spectral/envelope/stereo metrics and patch-aware inference | Source is compressed web audio, not calibration truth | v0.19 |
| Real hardware dry capture corpus | MISSING | Capture protocol defined | Obtain controlled oscillator/filter/env/sync sweeps | **v0.19 P0** |
| Hard-sync residual analysis | FUNCTIONAL / PROVISIONAL | v0.11 microscope excludes several simple causes | Model finite analog reset/discharge/recovery only with evidence | v0.19 |
| Filter calibration | FUNCTIONAL / PROVISIONAL | Several replaceable laws/candidates | Hardware sweep fit | v0.19 |
| Envelope calibration | FUNCTIONAL / PROVISIONAL | Separate amp/filter exponents and maxima | Hardware timing captures | v0.19 |
| Glide calibration | FUNCTIONAL / PROVISIONAL | Four modes implemented | Exact rate/time law | v0.19 |
| 1x/2x/4x/8x oversampling | VALIDATED infrastructure | 2x default; higher modes diagnostic | Decide adaptive 4x only if quality/CPU data justify it | v0.19 |
| CPU profiling | FUNCTIONAL | Oscillator path identified as main hotspot | Optimise only after v0.18 feature set stabilises | v0.19 |
| Oscillator fast paths | MISSING optimisation | Current general path always used | Cache static shape/PW geometry; preserve hard-sync/Poly-Mod fidelity | v0.19 |
| Silent-voice cheap advance | MISSING optimisation | Full voice path currently used | Advance free-running state without unnecessary audio work | v0.19 |
| Pan coefficient caching | MISSING optimisation | Candidate micro-optimisation | Do only after profiling confirms value | v0.19 |

---

# Milestone plan

## v0.16 — Stability gate — **CLOSE NOW**

Scope only:

- Windows VST3 build + CTest + pluginval strictness 10.
- Linux VST3 build + CTest + pluginval strictness 10.
- Linux ASan+UBSan.
- Real Release-mode checks (not tests compiled away by `NDEBUG`).
- Chord-memory held-key bug fixed.
- Serialized parameter registry/layout integrity fixed and regression-tested.
- Valid bundle ID and one canonical CI workflow.

**Definition of done:** final closeout commit passes the same three jobs. No new synth features are added to v0.16.

## v0.17 — Complete the control/performance contract

Priority order:

1. Causal ordering for host automation vs MIDI NRPN/UI mirroring.
2. Explicit realtime no-allocation/stress tests.
3. Sustain pedal CC64.
4. LFO Sync connected to the common clock.
5. Remaining documented performance CC translation through the existing NRPN/source-of-truth path.
6. Realtime-safe SysEx handoff to message thread.
7. Investigate — but do not guess — chord-memory storage and sequencer opaque semantics.

**Exit criterion:** all non-FX musical controls intended for the first beta either work, are explicitly marked hardware-unknown, or are deliberately deferred with reason.

## v0.18 — Digital effects

- One realtime-safe serial FX A -> B architecture.
- Reuse/adapt mature license-compatible delay/chorus concepts where available instead of reinventing them.
- Complete Prophet effect family: delay/BBD, chorus, phaser, flanger, ring modulation, reverbs.
- Common tempo sync source shared with the existing clock.
- True/off bypass semantics and state regression.

**Exit criterion:** every effect control exposed to the host has audible runtime behavior or is intentionally unsupported and hidden from release UI.

## v0.19 — Sound matching and optimisation

- Controlled Prophet-6 hardware captures become primary calibration evidence.
- Oscillators/PW/mixer gain staging.
- Hard-sync residual.
- HPF/LPF cutoff, resonance, saturation and envelope behavior.
- ADSR timing/depth, glide, Slop/Vintage and analog distortion.
- Poly-Mod/LFO/pressure depth calibration.
- CPU optimisation driven by profiling, not intuition.
- Decide adaptive oversampling only from measured quality/CPU tradeoff.

**Exit criterion:** a documented listening/measurement suite covers representative basic, sync, PWM, filter, modulation, unison and FX patches with no regression outside agreed tolerances.

## v0.20 — Custom UI and preset workflow

Only after the parameter/musical contract above is stable:

- Bespoke editor.
- Clear indication of panel vs Global/wrapper controls.
- Preset/bank browser.
- SysEx import/export.
- Sequence editor.
- Init / compare / A-B workflow.
- Keep Generic JUCE editor available as a diagnostic fallback.

**Exit criterion:** every visible user control maps to a matrix row that is functional or explicitly documented; no decorative dead controls.

## v0.21 — Public beta hardening

- Windows DAW matrix.
- Linux DAW matrix.
- Project save/reload and automation regression in real hosts.
- Long-running MIDI/clock/voice-steal soak tests.
- CPU/load tests at 44.1/48/96 kHz and common block sizes.
- Packaging/install instructions and license review.

---

# Active milestone discipline

At any point there should be **one active milestone**. Work may only enter it when it closes a matrix row assigned to that milestone or fixes a regression blocking its exit criterion. Everything else stays in backlog.

The next active milestone after the v0.16 closeout gate is **v0.17: Complete the control/performance contract**. Digital FX, acoustic calibration and custom UI are explicitly later milestones so the project does not oscillate between unrelated subsystems.
