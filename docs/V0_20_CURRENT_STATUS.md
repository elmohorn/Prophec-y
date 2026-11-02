# v0.20 — Current implementation status

## Custom Character priority — four native prototypes implemented

Plasma, Optical Spring, Ferrofluid and Neon Wavefolder now join Tube in the
same recessed bay. Each has real DSP, seven independent persistent controls,
Start/End routing and an audio-informed native visualization. See
[V0_20_CUSTOM_CHARACTER.md](V0_20_CUSTOM_CHARACTER.md) for the exact scope,
source reuse, compatibility strategy and validation. MIDI input selection and its monitor are implemented; custom Studio remains pending.


Status: **active prototype / engineering checkpoint**, 2 November 2026, preview 282.

This file is the current short-form ledger for the `v0.20-ui` branch. It records what is implemented, what has been measured, and what is still intentionally unresolved. It complements the design/history documents rather than replacing them.

## Audit follow-up matrix — preview 282

This is the live status of the audit; historical findings and detailed evidence
are retained in [the audit](AUDITORIA_GLOBAL_2026_11_02.md). “Tested” means
the stated automated scope, not musical, hardware or visual approval.

| Finding | Implemented and tested | Still open |
|---|---|---|
| A01 Neon discontinuity | Continuous law, zero-crossing tests and 4× oversampling | Independent spectral alias measurement and listening |
| A02 sample-rate response | Sample-rate-derived Plasma/Ferrofluid timing; Ferrofluid attack measured at four rates | Broader acoustic comparison between rates |
| A03 MIDI channels | Omni/1–16, monitor, per-channel NRPN and note-ownership tests | FA-08/DAW checks; independent channel sustain and MPE |
| A04 regression gates/CPU | Linux core metric gate and preliminary six-voice Character benchmark | Wrapper/editor/standard FX CPU, percentiles and meaningful budgets |
| A05 Character acoustics | Continuity, impulse, timing and stereo telemetry tests; exploratory Neon rate comparison | Independent acoustic/alias tests, automation and musical listening |
| A06 concurrent restore | Non-blocking audio snapshot gate; controlled interleaving, concurrent save and two-writer probe in CI | Longer stress, host reentrancy and real DAW sessions |
| A07 control contract / Studio | Common contract for hardware-described controls | Character parity, accessibility and contextual Inspector |
| A08 visual telemetry | Stereo-aware activity and consistent Tube Motion behavior | Owner visual review; views remain illustrative, not physical measurements |
| A09 version migration | Independent synthetic v3/v4 fixtures; codec v5 reads old states | Real projects in multiple DAWs, automation and library association |
| A10 product evidence | This matrix and linked evidence | Real Prophet-6 comparison, listening and final acceptance |

The instrument and Character names do not establish verified hardware or pedal
fidelity. Preview 282 is not an RC. Older plugin versions cannot read v5 projects
or presets; retain backups before testing.

## Current branch scope

Prophec-y v0.20 keeps the Prophet-6-compatible raw/SysEx state as the canonical musical state while replacing the generic editor with a custom aluminum Module surface and adding optional Prophec-y Character FX. The core DSP/state contract from v0.19 is preserved unless a change is explicitly documented below.

## UI implemented

- Native JUCE aluminum Module editor based on the owner's desktop-module reference; final visual approval remains pending.
- Oscillator, mixer, filter, Poly Mod, LFO, filter envelope, amplifier envelope, performance and output controls connected to the existing APVTS/raw parameter contract.
- Standard Prophet-style FX type selectors are now visible in Module:
  - FX A: Off, BBD Delay, Digital Delay, Chorus, Phaser 1, Phaser 2.
  - FX B: the same plus Hall, Room, Plate and Spring.
  - The existing global `FX ON` control remains the standard-chain master bypass.
- Character FX is a separate expandable/collapsible bay with Start/End insertion relative to the standard FX chain. Implemented types are Tube, Plasma, Optical Spring, Ferrofluid and Neon Wavefolder.
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
- Other controls retain a labelled position bar. The decorative falling filter curve has been removed; an actual filter response and LFO/FX contextual views remain pending. Separate output meters are implemented.

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

Implemented: [Master Output and L/R peak/RMS/clip metering](V0_20_MASTER_OUTPUT.md). New instances use -18 dB; legacy v1/v2 wrapper sessions migrate at unity (0 dB). The canonical Program Volume, voice sum and effect gain laws are preserved. State codec v3 introduced the appended global Master independently of SysEx; the current codec is v5.

The measured +16.91 dBFS no-FX peak becomes -1.09 dBFS at -18 dB. PRE and final clip indicators expose hotter cases; no limiter guarantees clipping prevention for arbitrary effect settings. Manual musical listening and DAW migration checks remain open.

## Permanent diagnostics added

- Full 14-bit pitch-wheel mapping/reset regression coverage.
- Live RAW14 pitch-bend display in the editor.
- Polyphonic/Unison 1–6 voice deterministic headroom table in `p6_dsp_fingerprint`.
- Focused GitHub Actions workflow `headroom-probe.yml` to build and run the headroom diagnostic quickly without waiting for the complete plugin matrix.

## Current unresolved v0.20 items

1. Contextual Studio/Inspector; manual headroom and listening checks remain open. MIDI input channel selection and monitoring are implemented.
2. Complete the remaining filter/LFO/FX contextual views; OSC/ADSR models, live pitch bend and separate output meters are implemented.
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

Preview 282 passed Windows and Linux CI, including the restore probe; the focused headroom workflow also passed for its diagnostic implementation. Full Windows/Linux plugin CI remains the release gate: core/state/control/realtime tests, native editor/audio smoke, pluginval strictness 10, Linux ASan+UBSan and analysis guards must pass before a preview is published.

## Product review and proposed next steps

The owner's comparison with Surge XT, Vital, Dexed, Gearmulator and Odin 2 has been reviewed against this checkpoint. Concrete formatter/calibration and MIDI-channel findings, references and proposed acceptance criteria are recorded in [V0_20_PRODUCT_REVIEW.md](V0_20_PRODUCT_REVIEW.md). The review remains design history; implemented deliveries and remaining work are tracked above.

## Parameter foundation checkpoint

The first compatibility/common-control slice is implemented in [V0_20_PARAMETER_FOUNDATION.md](V0_20_PARAMETER_FOUNDATION.md): historical-encoder fixtures, a JUCE-independent presentation model, effective-profile envelope formatting and common hardware controls, validated first on Low Pass then extended to the 37 synthesis-control instances. Existing state formats and DSP fingerprints are preserved. The complete widget migration, Inspector and manual DAW/screen-reader checks remain pending.

## Additional restore stress — 2 November 2026

The locally built preview-282 restore probe was executed 50 times, each with its
controlled paused restore, simultaneous save and 24 competing-writer rounds:
50/50 executions passed, for 1,200 competing-writer rounds. The executable's
SHA-256 was `1835d6148141e330bebed0e3e675f8825203d71a34bfdb08c5251cb1936aec09`.
These were 50 short independent runs on Linux, not one continuous long soak,
not a TSan run, and not evidence about all host callbacks or DAWs.

## Manual listening and FA-08 check — preview 282

1. Back up existing projects and presets. Install preview 282, rescan the VST3,
   create a new project and save a copy under a new name.
2. Play notes and pitch bend on the FA-08 in Omni. Watch the MIDI monitor and
   RAW14 display while moving the wheel slowly through its full travel; note
   channels, filtered markers, any discontinuity and whether sound follows it.
3. Set an input channel and compare matching and nonmatching FA-08 zones. When
   changing the channel, expect held notes to stop and standard FX tails to clear.
4. Listen to Tube, Plasma, Optical Spring, Ferrofluid and Neon at moderate volume;
   vary Mix and the main controls. Note any unwanted clicks, abrupt jumps or
   excessive levels and the exact settings. These are prototypes to audition.
5. Save the DAW project, close and reopen it, then check sound, Character values,
   master level, MIDI channel and any host automation. Try an old project only
   on a backup: earlier plugin versions cannot read v5 saves.
6. Report OS, DAW/version, audio sample rate and buffer size, plugin preview,
   FA-08 zone/channel, reproduction steps and what you expected versus heard.
   A brief recording or screenshot is useful if the issue is hard to describe.
