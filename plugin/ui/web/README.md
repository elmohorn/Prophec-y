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
- Arp BPM, Character rhythm and motion.
- Native preset-library previous/next navigation and host preset-name display.
- Bidirectional APVTS parameter synchronization, with host gestures and host-state refresh on editor visibility.

## Known limitations (not falsely represented as implemented)

- The 64-step HTML sequencer is a visual editor only; it does not modify the engine's sequence data, nor does it follow host transport. Fake playhead animation is suppressed when hosted.
- Glass meters/waveform are not connected to DSP telemetry. Fake animation is suppressed and the display is marked as disconnected in hosted mode.
- The Rotary Ensemble module has no DSP equivalent and is disabled when hosted.
- Character uses one selected effect at a time, not a three-effect parallel chain.
- Full DAW runtime, pluginval, macOS WebKit, and multi-instance performance tests remain to be run on their respective hosts.

## CI

`tests/webview_v3_integrity.py` checks HTML scripts with Node, control counts, and JS/C++ parameter contracts. GitHub Actions compiles Windows VST3, runs existing DSP/state/realtime regression suites, and checks the approved HTML sentinel inside the binary before publishing the artifact. The native editor remains the default outside `P6_USE_WEB_UI=ON`.
