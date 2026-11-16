# Prophec-Y UI v3.0 — design checkpoint (branch)

This branch is reserved for the approved **v3.0 HTML/CSS/JS UI design**. The existing native JUCE editor, DSP, presets and parameter IDs on `main` must remain unchanged until the web UI has been integrated and validated.

## Approved visual baseline

- Left panel: smoked black glass, red neon logo beneath the glass, unified multifunction screen, separate preset OLED, metal Master dial and capacitive buttons.
- Main synth: continuous dark brushed aluminum separated by thin machined grooves; no redundant top signal-flow headings.
- Two-row synth layout; 16-step sequencer page with contextual editing; optional right-hand Character module without reflow of the main synth.

## Integration contract

- Use JUCE 9 `WebBrowserComponent` (WebView2 on Windows, WebKit on macOS), with C++ DSP and APVTS remaining native.
- Map HTML controls to existing parameter IDs and host automation; avoid inventing DSP parameters from the visual prototype.
- Distinguish Character panel visibility from Character audio bypass.
- Validate WebView resizing, preset load/save, MIDI, host automation, state restoration and audio-thread safety before replacing the existing editor.

## Status

**Design baseline only.** The approved standalone HTML prototype is not yet committed to this branch, and the JUCE WebView integration is not yet implemented or compiled. Do not treat this document as a plugin build or a completed UI migration.
