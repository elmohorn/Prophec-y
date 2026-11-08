# v0.20 — Character hardware mockups as native JUCE code

**Status:** Experimental UI implementation, not an audio effect release.
**Branch:** `ui-character-hardware-mockups` (based on `v0.20-ui`).
**Scope:** Approved visual direction for Tube, Optical Tremolo and Optical Spring.

## Delivered

- `ui/fx/CharacterHardwareWindow.h`: single reusable native JUCE component renders a large recessed hardware window, machined metal bezel, fasteners, internal glass, lamp/LDR and three metal springs. The rendering is independent of the audio thread and graphics require no web runtime.
- **Tube:** uses the existing bundled transparent glass artwork plus gradient/filament glow. Receives the **actual** existing `CharacterTelemetry` input/output/change levels. UI only; no change to the Koren/DSPark algorithm.
- **Optical Spring:** renders three springs from the existing three pickup-energy telemetry channels, preserving the real Character spring controls and the older FDN DSP. The spring deformation is illustrative, not a calibrated mechanical measurement.
- **Optical Tremolo:** a button labelled `OPTO · PREVIEW` opens a **design-only** lamp/LDR chamber with nonfunctional painted knobs. The UI prominently states **NO AUDIO PROCESSING**, and it never registers a fake Character ID or modifies effect/audio state. Its pulsing illumination is a clearly identified synthetic UI preview, not a signal meter.
- Header controls and panoramic chamber adopt the approved horizontal layout (left window, five real controls above, two below) within the existing 1460×930 expanded editor. Existing APVTS bindings, saved state, instrument DSP, factory presets, FX A/B, Start/End and MIDI remain untouched.
- None, Plasma, Ferrofluid and Neon keep their existing audio selections; those effects reuse `CharacterDeviceView`. The original TubeChamber remains in the tree for historical regressions but is hidden by the new Tube art.
- `tests/plugin_ui_smoke.cpp` now asserts that hiding/expanding the panoramic window does not modify the plugin state, that the optical preview cannot alter the state blob, and emits a `module_optical_tremolo_preview.png` screenshot when a capture folder is supplied.

## Deliberate boundaries

**This is UI code only.** A sound-generating Optical Tremolo still requires the separate DSP candidate comparison (DSPark baseline vs calibrated Patina LDR), new host IDs and versioned state migration in accordance with [the technical audit](V0_20_TUBE_OPTICAL_TREMOLO_TECH_REVIEW.md). This PR does not claim it exists.

This design uses a single Character device with selection Start/End as before. Three visual concepts **do not mean** three simultaneous audio effects.

## Validation to run before merge

1. Build the JUCE VST3 and smoke target:
   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DP6_BUILD_PLUGIN=ON -DP6_BUILD_UI_SMOKE=ON
   cmake --build build --target P6CorePlugin_VST3 P6UiSmoke --parallel 2
   xvfb-run -a ./build/plugin/P6UiSmoke_artefacts/Release/P6UiSmoke ./build/character_captures
   ```
   Adjust the binary path for local generator/platform.
2. Check `module_tube.png`, `module_custom_3.png` (Optical Spring) and `module_optical_tremolo_preview.png` at 1240/1460/1780 width, with active/bypass and reduced motion.
3. Host test in a DAW: no parameter changes on visual preview, preset/save/restore, bypass/position, click-free changes. Confirm the original dry/PDC and SysEx invariance.
4. Test CI Windows/Linux VST3, pluginval and sanitizer gates before merging into `v0.20-ui`.

**Not verified here:** actual JUCE compilation, real host/Windows runtime, visual pixel-perfect comparison, live trémolo audio or new acoustic measurements.
