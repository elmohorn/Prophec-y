# Prophec-Y v3.0 WebView integration — in progress

This branch contains an **opt-in** JUCE 9 `WebBrowserComponent` editor and APVTS bridge. The native editor remains the default. `main` is unchanged.

**Required before enabling `P6_USE_WEB_UI=ON`:** copy the approved, instrumented HTML file from the supplied integration ZIP into `plugin/ui/web/ProphecY_v3_0.html`. This 226 KB source asset is not yet committed to GitHub. The opt-in WebView build will fail until it is present.

The HTML package preserves the approved smoked-glass/metal v3.0 design, sends UI gestures and values to existing parameter IDs, receives host automation/state updates, and reports Character visibility for window resizing.

Build after copying the HTML:

```bash
cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DP6_BUILD_PLUGIN=ON -DP6_FETCH_JUCE=ON -DP6_USE_WEB_UI=ON
cmake --build build-web --config Release --target P6CorePlugin_VST3 -j2
```

Not yet connected to DSP: visual 64-step sequencer, Character knobs, FX types, preset navigation, meter/waveform visualizations and host tempo. `RESET UI` is disabled in hosted mode. No VST3 compilation or DAW testing has been performed. The native DSP and preset/state formats were not modified.
