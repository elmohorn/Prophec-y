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
- Character Tube/Spring single-device selection and bypass, reflecting the **actual single-device** DSP architecture. Rotary is disabled because no rotary DSP exists.
- Arp BPM, Character rhythm and motion, mode/range/division, host/internal/MIDI clock, and LP keytrack.
- Eight native arpeggiator Character gate switches and the DSP playhead; editing a gate enables Character rhythm.
- Native preset-library previous/next navigation and host preset-name display.
- Bidirectional APVTS parameter synchronization, with host gestures and host-state refresh on editor visibility.

## Known limitations (not falsely represented as implemented)

- The engine's **64-step six-track polyphonic sequencer** is not yet editable through WebView. The visible eight switches control the *separate* native eight-step Arp Character mask; other pages, per-step notes, velocities, gate lengths and tie states are disabled in hosted mode. The Arp Character playhead is sourced from DSP.
- Native FX selectors and LFO shape selectors still need verified effect/waveform mapping and bridge wiring.
- Glass LED peak meters and VU-style RMS needles receive real native output telemetry at 20 Hz. The decorative waveform is unavailable in hosted mode because no real waveform stream exists.
- The Rotary Ensemble module has no DSP equivalent and is disabled when hosted.
- Character uses one selected effect at a time, not a three-effect parallel chain.
- Full DAW runtime, pluginval, macOS WebKit, and multi-instance performance tests remain to be run on their respective hosts.

## CI

`tests/webview_v3_integrity.py` checks HTML scripts with Node, control counts, and JS/C++ parameter contracts. GitHub Actions compiles Windows VST3, runs existing DSP/state/realtime regression suites, and checks the approved HTML sentinel inside the binary before publishing the artifact. The native editor remains the default outside `P6_USE_WEB_UI=ON`.
