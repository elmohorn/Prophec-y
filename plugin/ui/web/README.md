# Prophec-Y v3.0 WebView integration

This experimental branch embeds the complete approved v3 HTML/CSS/JS interface in JUCE 9's WebBrowserComponent (WebView2 on Windows, WebKit on macOS). Native C++ DSP, patch/state codecs and the existing editor on main remain unchanged.

## Build

```sh
cmake -S . -B build-web -DP6_BUILD_PLUGIN=ON -DP6_FETCH_JUCE=ON -DP6_USE_WEB_UI=ON
cmake --build build-web --config Release --target P6CorePlugin_VST3 --parallel 2
```

Windows needs the Microsoft.Web.WebView2 NuGet SDK and `JUCE_WEBVIEW2_PACKAGE_LOCATION` (see `.github/workflows/webview-v3-ci.yml`). `JUCE_USE_WIN_WEBVIEW2=1` is enabled by CMake for the Web UI target. The resource provider must be available at compile time, and CI rejects a binary that does not contain the actual HTML.

## Connected controls

- 30 primary knobs, 4 mixer faders, 14 original switches, 3 arpeggiator switches, Master.
- 6 Character knobs: Tube drive/treble/mix and Optical Spring decay/tension/mix.
- Character Tube, Plasma, Spring, Ferrofluid, Neon and Rotary use a single-device selector and bypass. Rotary supports 8–384 kHz; above that the wet path falls back to dry and the editor warns.
- Arp BPM, Character rhythm and motion, mode/range/division, host/internal/MIDI clock, and LP keytrack.
- FX A/B algorithm selectors (real native effect types) and five native LFO waveform types.
- Eight native arpeggiator Character gate switches and the DSP playhead; editing a gate enables Character rhythm.
- Native preset-library previous/next navigation and host preset-name display.
- Bidirectional APVTS parameter synchronization, with host gestures and host-state refresh on editor visibility.

## Known limitations (not falsely represented as implemented)

- The six-track 64-step sequencer supports note and velocity editing. Its separate eight-step Arp Character mask is in Advanced. Variable gate length and the meaning of the imported high bit remain unverified.
- Effect and LFO selectors are now mapped to the verified native NRPN registry. Additional DSP controls not present in the v3 HTML remain outside this WebView bridge.
- Glass LED peak meters and VU-style RMS needles receive real native output telemetry at 20 Hz. The decorative waveform is unavailable in hosted mode because no real waveform stream exists.
- Rotary uses the experimental setBfree-based DSP; host and listening validation remain pending.
- Character uses one selected effect at a time, not a three-effect parallel chain.
- Full DAW runtime, pluginval, macOS WebKit, and multi-instance performance tests remain to be run on their respective hosts.

## CI

`tests/webview_v3_integrity.py` checks HTML scripts with Node, control counts, and JS/C++ parameter contracts. GitHub Actions compiles Windows VST3, runs existing DSP/state/realtime regression suites, and checks the approved HTML sentinel inside the binary before publishing the artifact. The native editor remains the default outside `P6_USE_WEB_UI=ON`.

## v3.5 unified smoked-glass preset library

The PRESETS capacitive control opens a full-height, contiguous smoked-glass drawer. Both the original left panel and the drawer use a shared reflection layer; the drawer's categories live only in the left filter column, and the loaded preset name is shown only in the original OLED. The approved synth layout and native DSP remain intact.

The WebView now requests real preset metadata from the existing `p6plugin::PresetLibrary` through `prophecyPresetCatalogRequest`/`prophecyPresetCatalog`. A single click only selects a row; double-click or LOAD SELECTED emits `prophecyPresetLoad` and applies the native musical preset. Favorites call `PresetLibrary::setFavorite` via `prophecyPresetFavorite`, using its existing persistent store. The current loaded preset is identified by stable ID in `prophecyPresetState`. Native category filtering uses the existing inferred category metadata, not invented tags; unclassified presets remain available under All patches. Standalone HTML shows a clearly labeled demo catalog, not DSP-connected presets.

The MANAGE button opens the existing preset manager for Save As and SysEx file import/export. Categories inferred from factory patch names are approximate. MIDI SysEx passed through the DAW is not yet applied to the current patch. Full DAW validation and cross-platform WebKit tests remain pending.

## v3.6 LUNA findings (2026-11-18)

Real UA LUNA testing confirms that the VST3 loads and produces sound without the previously reported random clicks (one session; not a long-term guarantee). Three UI regressions were identified and fixed on `fix/preset-library-regressions-v35` without changing the audio engine:

- The left panel and preset drawer use a **single parent-painted metal bezel and smoked-glass plane** when the drawer is open. The two child panels no longer draw their own independent horizontal border highlights. The common plane's geometry is measured from the mounted elements.
- Host-driven preset parameter hydration no longer triggers the temporary OLED parameter overlay (previously frequently displayed `MIX 35%`). User-originated knob edits still trigger the overlay.
- Native WebView exposes the **eight arpeggiator Character gates** as eight clickable ON/OFF steps, with verified `arp_step_1..8` events. The misleading 16/64-step note editor is hidden in the hosted view until it is actually connected. The JUCE-independent DSP already supports a separate six-track 64-step polyphonic sequencer; a full native editor and safe state bridge for its note/velocity data remain to be implemented.

Do not claim the 64-step sequencer is functional from the eight-gate UI. Do not merge or replace the user's working LUNA VST3 until the new CI passes and the user confirms the result.

## Native sequencer and Character integration (isolated feature branch)

- The native six-track, 64-step Prophet-6 sequence is now the source of truth. WebView requests `prophecySequenceState` (6×64 raw note, velocity and preserved bit-7 flag) and emits validated `prophecySequenceEdit` events for note/velocity changes.
- C++ edits canonical `patch_.rawPayload` under the existing non-realtime writer/patch guards. An atomic dirty bit publishes only the 768 sequence bytes to the audio thread; `scheduler_.updateSequence()` updates playback without restarting the oscillator/filter engine or resetting transport. Preset and DAW state serialization already includes the raw sequence payload.
- UI exposes track 1–6, four pages of 16 steps, REST/NOTE editing, MIDI note +/- (12–108), velocity, sequence length 1–64, PLAY/STOP and audio-thread playhead telemetry. Right-click toggles a step between REST and NOTE; primary click selects a step. The eight independent arpeggiator Character gates are in the Advanced section.
- Factory velocity bit 7 is retained losslessly but **not** interpreted as tie/gate until the firmware semantics are proven. Variable gate length is not supported by the current native player and its demo editor is hidden.
- Character FX is a **single DSP insert**, never a chain: Tube, Plasma, Optical Spring, Ferrofluid and Neon Wavefolder are mutually exclusive. A device picker selects the current module; its 7 native parameters (prominent dials plus secondary sliders) and pre/post routing are host-synchronized. Plasma Hybrid/Legacy is selectable. Rotary uses the experimental DSP; host and listening validation remain pending.
- These are integration changes awaiting validation in a real DAW. Do not merge into main or replace the user's known-good LUNA build solely on a successful compile.
