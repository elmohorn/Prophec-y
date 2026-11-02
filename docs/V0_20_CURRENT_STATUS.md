# v0.20 — Current implementation status

## Custom Character priority — four native prototypes implemented

Plasma, Optical Spring, Ferrofluid and Neon Wavefolder now join Tube in the
same recessed bay. Each has real DSP, seven independent persistent controls,
Start/End routing and an audio-informed native visualization. See
[V0_20_CUSTOM_CHARACTER.md](V0_20_CUSTOM_CHARACTER.md) for the exact scope,
source reuse, compatibility strategy and validation. MIDI policy and custom
Studio remain postponed by request while these modules are auditioned.


Status: **active prototype / engineering checkpoint**, 1 November 2026.

This file is the current short-form ledger for the `v0.20-ui` branch. It records what is implemented, what has been measured, and what is still intentionally unresolved. It complements the design/history documents rather than replacing them.

## Current branch scope

Prophec-y v0.20 keeps the Prophet-6-compatible raw/SysEx state as the canonical musical state while replacing the generic editor with a custom aluminum Module surface and adding optional Prophec-y Character FX. The core DSP/state contract from v0.19 is preserved unless a change is explicitly documented below.

## UI implemented

- Native JUCE aluminum Module editor based on the owner's desktop-module reference; final visual approval remains pending.
- Oscillator, mixer, filter, Poly Mod, LFO, filter envelope, amplifier envelope, performance and output controls connected to the existing APVTS/raw parameter contract.
- Standard Prophet-style FX type selectors are now visible in Module:
  - FX A: Off, BBD Delay, Digital Delay, Chorus, Phaser 1, Phaser 2.
  - FX B: the same plus Hall, Room, Plate and Spring.
  - The existing global `FX ON` control remains the standard-chain master bypass.
- Character FX is a separate expandable/collapsible bay with Start/End insertion relative to the standard FX chain. The current implemented Character type is Tube; the architecture is intended for additional modules.
- Character now has an independent aluminum faceplate and a [recessed dark valve chamber](V0_20_RECESSED_CHARACTER.md), following the approved design.
- Tube graphics are driven by measured audio telemetry (input RMS, output RMS and latency-aligned waveform-change RMS), not random animation. The light display is cosmetic and does not alter DSP.
- Human-readable control formatting has begun replacing raw 0–127 / 0–255 numbers on the main Module surface. Internal values and SysEx/NRPN domains remain unchanged.

## Preset browser implemented

[Native local browser](V0_20_PRESET_BROWSER.md): Factory Init / Imported / User, search and filters, persistent favorites, Save/Save As, protected unsaved edits and program/bank SysEx import/export. Musical loads preserve global instance settings and opaque payload bytes. Library ID/alias association is transient across DAW reloads; full sound/session state remains supported.

## Human-readable parameter display

The Module editor now formats common controls into semantic units where the current mapping is known or intentionally defined: semitone/coarse pitch, cents, oscillator morph labels, pulse-width percentage, mixer/output percentages, bipolar percentages, LFO Hz and envelope/glide time. Raw values remain canonical internally and remain available for diagnostic/compatibility work.

This is a presentation change only. It does not rewrite stored Prophet state or change host parameter IDs.

## Pitch bend — implementation and diagnostic

The MIDI router uses the full standard 14-bit pitch-wheel domain:

- raw 0 -> full negative bend
- raw 8192 -> centre
- raw 16383 -> full positive bend

The engine applies the mapped bend range to all active voices. A regression test now covers full negative, centre, full positive and CC121 reset behavior.

`Reset All Controllers` (CC121) now explicitly recentres pitch bend and clears channel pressure in addition to the pre-existing controller reset behavior. This fixed a real stale-state hole but is not claimed to explain every subjective controller issue.

For hardware diagnosis, the central display now temporarily shows incoming pitch-bend telemetry including:

- bipolar wheel position,
- resulting semitone offset using the current bend range,
- exact incoming `RAW14` value.

This is specifically useful with the Roland FA-08: if the displayed RAW14 value itself returns toward 8192 mid-travel, the problem is upstream of the synth; if RAW14 remains monotonic while pitch returns, the bug is inside Prophec-y.

## Central display — first contextual models implemented

The first Vital-inspired slice is implemented in native JUCE, without reusing Vital code. Selecting or keyboard-focusing an oscillator or envelope control changes the OLED without editing the patch.

- OSC 1/2: nominal cycle from the core shape/PW law and active calibration. This is before oscillator level, sync, modulation and anti-alias correction; it is not an oscilloscope.
- Filter/amp envelope: isolated ADSR simulation using the core envelope and active mapped times, sustain, a labelled gate and time axis. This is before depth/velocity/modulation, not a live per-voice envelope.
- Pitch bend retains its real incoming RAW14 diagnostic.
- Other controls retain a labelled position bar. The decorative falling filter curve has been removed; an actual filter response, LFO/FX views and output meters remain pending.

The extracted `ContextDisplay` caches model curves by parameter content. Curve work runs only on the UI side when model inputs change. Tests compare ADSR samples against the core at 48 kHz, preserve fast attack peaks, bound preview work and verify that selection/rendering does not modify plugin state. See [scope and validation](V0_20_CONTEXT_DISPLAY.md).

## Studio — current limitation

Studio still embeds JUCE `GenericAudioProcessorEditor`. It is intentionally retained as a complete diagnostic/raw editing surface, but it is not the intended final Studio design. A custom Studio view remains a major v0.20 UI task.

## Headroom / clipping diagnosis

The current engine sums voice outputs directly into the stereo bus and then applies program/MIDI volume before distortion, standard FX and the optional Character insert. A dedicated final Master Output stage is now implemented after the complete chain; no automatic voice-count normalization or safety limiter is used.

A permanent deterministic headroom probe was added to `p6_dsp_fingerprint`. It renders controlled polyphonic and Unison stacks at 48 kHz / 2x oversampling and reports RMS, peak and peak dBFS.

### Measured `init_like` case

One oscillator level 110/127, Program Volume 110/127, open filters, distortion/FX off:

| Voices | Poly peak dBFS | Unison peak dBFS |
|---:|---:|---:|
| 1 | -2.58 | -2.58 |
| 2 | +2.58 | -2.05 |
| 3 | +5.53 | -0.37 |
| 4 | +6.86 | +0.18 |
| 5 | +8.84 | -2.46 |
| 6 | +9.97 | +1.64 |

### Measured `dual_osc_max` case

Both oscillators at maximum level, Program Volume 110/127, open filters, distortion/FX off:

| Voices | Poly peak dBFS | Unison peak dBFS |
|---:|---:|---:|
| 1 | +4.84 | +4.84 |
| 2 | +9.13 | +3.87 |
| 3 | +12.63 | +5.93 |
| 4 | +14.69 | +8.10 |
| 5 | +15.35 | +6.64 |
| 6 | +16.91 | +9.51 |

These results confirm that the engine can legitimately exceed +/-1.0 before the host output. They also show that the issue is **not uniquely an Unison problem**: ordinary polyphony can produce even larger peaks. Unison peaks vary non-monotonically because the free-running voice phases can reinforce or partially cancel.

## Headroom decision

No limiter, hard clamp or `1/N` / `1/sqrt(N)` voice normalization has been added. Doing that at the voice-sum stage would alter the relative dynamics and could erase behavior that belongs to the instrument model.

Implemented: [Master Output and L/R peak/RMS/clip metering](V0_20_MASTER_OUTPUT.md). New instances use -18 dB; legacy v1/v2 wrapper sessions migrate at unity (0 dB). The canonical Program Volume, voice sum and effect gain laws are preserved. State codec v3 stores the appended global Master independently of SysEx.

The measured +16.91 dBFS no-FX peak becomes -1.09 dBFS at -18 dB. PRE and final clip indicators expose hotter cases; no limiter guarantees clipping prevention for arbitrary effect settings. Manual musical listening and DAW migration checks remain open.

## Permanent diagnostics added

- Full 14-bit pitch-wheel mapping/reset regression coverage.
- Live RAW14 pitch-bend display in the editor.
- Polyphonic/Unison 1–6 voice deterministic headroom table in `p6_dsp_fingerprint`.
- Focused GitHub Actions workflow `headroom-probe.yml` to build and run the headroom diagnostic quickly without waiting for the complete plugin matrix.

## Current unresolved v0.20 items

1. Contextual Studio/Inspector. MIDI input channel selection/monitor is postponed by the owner; manual headroom/listening checks remain open.
2. Complete the remaining filter/LFO/FX/output contextual views; OSC/ADSR models and live pitch bend are already implemented.
3. Replace the generic Studio editor with the custom Studio design.
4. Continue semantic formatting/precise editing for controls that still expose raw-style values or ambiguous units.
5. Expand standard FX editing beyond type/mix into clear effect-specific controls where the hardware semantics are verified.
6. Browser follow-up: Compare/A-B, undo/redo, DAW library association and remaining v0.20 plan items.

## Recent implementation checkpoints

- `09de573` — standard FX selector controls added.
- `2ebb301` — standard FX A/B selectors exposed on Module.
- `0b9377e` — MIDI controller reset now recentres pitch bend.
- `231b113` — pitch-wheel full-travel/reset regression tests.
- `f86664f` — live pitch-bend diagnostic plus human-readable Module values.
- `fb729c6` / `7b412df` — deterministic headroom measurement added and reported by DSP fingerprint.
- `540268f` — focused headroom diagnostic workflow.

## CI / release policy

The focused headroom workflow passed for the current diagnostic implementation. Full Windows/Linux plugin CI remains the release gate: core/state/control/realtime tests, native editor/audio smoke, pluginval strictness 10, Linux ASan+UBSan and analysis guards must pass before a preview is published.

## Product review and proposed next steps

The owner's comparison with Surge XT, Vital, Dexed, Gearmulator and Odin 2 has been reviewed against this checkpoint. Concrete formatter/calibration and MIDI-channel findings, references and proposed acceptance criteria are recorded in [V0_20_PRODUCT_REVIEW.md](V0_20_PRODUCT_REVIEW.md). The review remains design history; implemented deliveries and remaining work are tracked above.

## Parameter foundation checkpoint

The first compatibility/common-control slice is implemented in [V0_20_PARAMETER_FOUNDATION.md](V0_20_PARAMETER_FOUNDATION.md): historical-encoder fixtures, a JUCE-independent presentation model, effective-profile envelope formatting and common hardware controls, validated first on Low Pass then extended to the 37 synthesis-control instances. Existing state formats and DSP fingerprints are preserved. The complete widget migration, Inspector and manual DAW/screen-reader checks remain pending.
