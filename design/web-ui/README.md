# Prophec-Y — Web UI design sandbox

This branch contains **design prototypes only**. The native JUCE VST3 editor and DSP remain unchanged on `main`.

## Files

- [Component Lab v1.2](component-lab-v1.2.html) — approved standalone component sandbox, with self-contained textures/assets.
- [Component design specification](COMPONENT_LAB_SPEC_V1_2.md) — material, lighting, behavior, and JUCE parameter binding reference.
- [Full Web Editor Alpha v1](prophec-y-full-editor-alpha-v1.html) — first standalone complete modular interface, reusing the component visual language and approved horizontal layout.

## Full editor alpha scope

The alpha displays 32 rotary main synthesizer controls, 4 mixer faders (OSC 1, OSC 2, SUB, NOISE) and the 18 known JUCE synthesizer switches. It includes FX A/B, a classic arpeggiator plus Character controls, the six Character types (None/Tube/Plasma/Optical Spring/Ferrofluid/Neon Wavefolder), a simplified preset browser and Studio inspector.

Controls are interactive locally in the browser, including pointer/keyboard changes, switches, presets, and Character switching with per-device values retained. The right Character panel can be hidden independently of its bypass/None state; the arp panel can be contracted. There is a single Character slot (not parallel instances).

### Not yet implemented / not claims of parity

- **No audio**, no DSP telemetry, MIDI, APVTS, SysEx, host automation, persistence or real factory preset loading.
- Demonstration values and illustrative waveform/meter/camera must **not** be treated as accurate sound behavior.
- The brushed-metal background and knob graphics remain **reference-render-derived** assets, not approved CC0 production PBR resources.
- Parameter identifiers are *mapping references*, not finished semantic range/format conversions; normalized 0–100% values are illustrative. FX types and visible switches are not connected to the core.
- The layout is usable at 1280×720 without vertical page scroll in the current browser snapshots, but some small text still needs a DPI/legibility pass.
- Character visuals must be replaced or connected to accurate DSP telemetry before shipping.

## Smoke checks

Manual/headless Chromium Playwright at **1600×900**, **1280×720**, **1024×700**: page mounted without JavaScript errors, no page overflow. Verified preset chooser, Character type switching and editing after switching, hiding Character, collapsing arp, Studio inspector navigation. These are only browser UI smoke checks, not native-plugin tests.

## Integration gate

Do not merge this directly as a replacement for `PluginEditor.cpp`. Build native JUCE reusable widgets with source parameter identifiers and snapshots; keep audio-thread rendering isolated. Compare against the approved component lab and verify host DPI scaling, parameter ranges, preset/state compatibility, pluginval and screenshots in the same commit.

Branch: `design/component-lab-v12-web-ui-alpha`
