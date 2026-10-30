# P6 Core — phase 1 / v0.15

Compatibility-first C++20 core for a Prophet-6-style software synthesizer. The 1024-byte hardware program state remains canonical and lossless; DSP blocks are isolated so measured calibration can replace assumptions without breaking SysEx/NRPN compatibility.

## v0.15 headline — realtime-safe host/MIDI bridge before the first linked VST3

v0.15 hardens the v0.14 JUCE adapter rather than adding another synthesis subsystem. A new JUCE-independent `P6MidiRouter` owns standard Prophet NRPN semantics and the wrapper now has explicit host/UI vs audio-thread patch ownership.

- Complete NRPN streams (CC99/98 + CC6/38), Data Increment/Decrement (CC96/97), performance MIDI and system realtime are routed through one core component.
- MIDI NRPN edits reach `audioPatch_` at their exact MIDI sample position; fixed atomic mailboxes mirror the final values back to canonical/UI state.
- JUCE `AsyncUpdater` was removed from the audio path after confirming JUCE's own warning that posting its message may block. A message-thread `Timer` polls the mirror instead.
- Host automation applies individual NRPN changes to both patch domains; it no longer replaces the whole audio patch and therefore cannot erase a newer MIDI edit that has not yet reached the UI mirror.
- Full state/SysEx loads use `setValueNotifyingHost()` so APVTS's denormalised `getRawParameterValue()` state remains coherent.
- Official control NRPN 1088 (`Seq Play/Stop`) is implemented as runtime state and never written into the 1024-byte program payload.
- Live sequence note/velocity NRPN edits refresh the active sequencer while preserving step and transposition.
- VST3 is now the default plug-in format; Standalone is opt-in with `P6_BUILD_STANDALONE=ON`. The wrapper links the narrower `juce_audio_processors` module.
- CI builds Linux/macOS/Windows VST3 targets, runs CTest + `pluginval --strictness-level 10`, and uploads validated artifacts.

See `plugin/README.md`, `docs/WRAPPER_0_15.md`, `docs/V0_15_CHANGELOG.md` and `docs/VALIDATION_0_15.txt`.

## Build and test

Core only (no JUCE dependency):

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

VST3 (JUCE 9.0.4 fetched automatically if no local/installed JUCE is found):

```bash
cmake -S . -B build-plugin -DP6_BUILD_PLUGIN=ON -DP6_FETCH_JUCE=ON
cmake --build build-plugin --config Release --target P6CorePlugin_VST3 -j2
ctest --test-dir build-plugin -C Release --output-on-failure
```

## Licensing

The project contains adapted MIT-licensed implementation ideas/code from `joshjetson/phosphor`; attribution is in `THIRD_PARTY_NOTICES.md`. GPL projects were used only as architectural/protocol references unless explicitly stated otherwise. Factory SysEx and third-party reference audio are **not redistributed** by this source package.

JUCE is not vendored in this repository. Building the optional JUCE/VST3 wrapper is subject to JUCE's own licensing terms.
