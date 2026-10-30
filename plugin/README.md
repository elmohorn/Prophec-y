# JUCE/VST3 wrapper — v0.15

v0.15 keeps the wrapper deliberately thin: JUCE owns the generic VST3 host contract, while all Prophet-6 state, MIDI semantics, performance scheduling and DSP remain in the clean-room core.

## Dependency strategy

The plug-in is **not** a from-scratch VST3 implementation and it does not copy another synthesizer wrapper wholesale.

Reused maintained infrastructure:

- JUCE 9.0.4 `juce_add_plugin` and VST3 client glue;
- `AudioProcessor` lifecycle/bus negotiation;
- `MidiBuffer` sample offsets;
- `AudioPlayHead::PositionInfo` for host BPM/PPQ;
- `AudioProcessorValueTreeState` as the host parameter registry;
- `GenericAudioProcessorEditor` as the temporary diagnostic UI;
- Tracktion `pluginval` in CI;
- Pamplejuce-style three-platform build/validation strategy (reference only, not a runtime dependency).

P6-specific code remains ours:

- lossless `P6PatchState` / SysEx / NRPN state;
- `P6MidiRouter`;
- `P6PerformanceScheduler` clock/arp/sequencer;
- `P6Engine` and all voice/DSP code;
- `PluginStateCodec`.

## v0.15 realtime/state hardening

### Two patch domains

`patch_` is the host/UI canonical state. `audioPatch_` belongs to the audio thread. Incoming MIDI NRPN is applied to `audioPatch_` at the MIDI event's exact sample offset, then mirrored asynchronously back to the canonical/host view.

This prevents an audio-thread MIDI edit from taking `patchLock_`, and prevents UI/state work from mutating the patch that is currently rendering.

### Lock-free MIDI -> UI mirror

The audio callback only writes the latest NRPN values into fixed atomic arrays. A JUCE `Timer` running on the message thread drains those arrays at 30 Hz and mirrors exposed hardware parameters to APVTS.

`AsyncUpdater` is intentionally not used from the audio callback: JUCE 9.0.4 documents that `triggerAsyncUpdate()` may block because it posts to the OS message queue.

The mirror is intentionally a coalescing UI/state mirror. Audio still receives every MIDI NRPN at its sample position; if multiple values for one parameter arrive between UI timer ticks, only the latest one needs to be reflected into the editor/host-facing value.

### Host automation merge

Host automation is applied as individual NRPN changes to **both** canonical and audio patches. The wrapper never replaces the complete audio patch merely because one host parameter changed. This protects newer sample-accurate MIDI edits that have not yet reached the message-thread mirror.

### APVTS synchronisation fix

Full state/SysEx loads use `setValueNotifyingHost()` rather than plain `setValue()`. In JUCE 9, plain `AudioParameterInt::setValue()` does not notify APVTS's `ParameterAdapter`, so `getRawParameterValue()` can otherwise remain stale.

### Runtime NRPN 1088

The official Prophet-6 control NRPN `1088` (Seq Play/Stop, 0–1) is routed as runtime state. It starts/stops `P6PerformanceScheduler` without being written into the 1024-byte program payload. Data Increment/Decrement work against the current runtime state as well.

### Realtime allocation policy

The audio patch reserves its raw-parameter vector before playback. First-time sequence/name NRPNs therefore do not cause vector growth in `processBlock()`. Unknown/global NRPN selections are not inserted into program state until their exact semantics/storage domain are implemented.

### SysEx realtime policy

SysEx in a host MIDI buffer is recognised but not parsed on the audio thread. Program/edit-buffer import remains available through the non-realtime `loadSysex()` API. A future fixed-capacity RT->message-thread SysEx queue can be added when the custom preset browser is built.

## Host state

`PluginStateCodec` is JUCE-free and versioned. It stores:

- exact 1024-byte Prophet-6 payload;
- bank/program identity;
- NRPN-only sequence controls 168..171;
- wrapper/runtime settings.

Arbitrary unknown bytes are round-tripped exactly in tests. APVTS remains a host-facing surface, not the canonical preset format.

## Build

Core only, no JUCE:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

VST3, fetching pinned JUCE 9.0.4 when needed:

```bash
cmake -S . -B build-plugin \
  -DCMAKE_BUILD_TYPE=Release \
  -DP6_BUILD_PLUGIN=ON \
  -DP6_FETCH_JUCE=ON
cmake --build build-plugin --config Release --target P6CorePlugin_VST3 -j2
```

VST3 is the default wrapper target in v0.15. To also build JUCE Standalone:

```bash
-DP6_BUILD_STANDALONE=ON
```

A local JUCE checkout can be supplied with `-DP6_JUCE_PATH=/path/to/JUCE`; an installed JUCE CMake package is checked before FetchContent.

## CI / first real binary

`.github/workflows/plugin-ci.yml` builds the VST3 on Linux, macOS and Windows, runs the core/state tests, validates each VST3 with Tracktion `pluginval --strictness-level 10`, and uploads the validated VST3 as an artifact.

This execution container has no JUCE checkout and cannot resolve external Git hosts, so a linked VST3 is still **not claimed locally**. The source adapter has been API-reviewed against JUCE 9.0.4 and syntax-checked with a minimal interface shim based on the used 9.0.4 declarations.

## Licensing

JUCE 9 is an optional external dependency and is not vendored here. JUCE is available under AGPLv3 or an applicable commercial JUCE licence; distribution must comply with the chosen licence.
