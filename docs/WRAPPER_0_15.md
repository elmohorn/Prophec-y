# Wrapper architecture — v0.15

v0.15 keeps the Prophet-compatible synthesis core independent from JUCE. JUCE 9.0.4 is used only as the host-facing VST3/Standalone adapter.

## Ownership model

- `P6PatchState patch_`: canonical host/UI state.
- `P6PatchState audioPatch_`: audio-thread-owned state used for realtime rendering.
- `P6MidiRouter`: JUCE-independent MIDI/NRPN decoder and router.
- `P6PerformanceScheduler`: clock, arp, sequencer and generated-note scheduling.
- `P6Engine`: synthesis/DSP engine.

Host automation is applied parameter-by-parameter to both patch domains. The wrapper never replaces the whole audio patch after automation, because doing so could erase a newer MIDI NRPN edit that has not yet reached the canonical/UI mirror.

## Realtime policy

The audio callback does not use `AsyncUpdater`. JUCE documents that `triggerAsyncUpdate()` may block while posting to the system message queue, even though it is thread-safe. Realtime edits are mirrored through fixed atomic mailboxes and a message-thread `Timer` polls those values for UI/canonical-state synchronisation.

`audioPatch_` reserves enough raw-parameter capacity before playback so a first sequence NRPN does not allocate memory in the audio callback. Unsupported/global NRPNs are ignored rather than inserted into program state.

## MIDI / NRPN

The router supports:

- Note on/off
- Mod Wheel
- Pitch bend
- Channel pressure
- MIDI Clock / Start / Continue / Stop
- NRPN selection through CC99/98
- Data Entry through CC6/38
- Data Increment / Decrement through CC96/97
- RPN null reset only after the complete MSB=127 + LSB=127 pair
- NRPN 1088 `Seq Play/Stop` as runtime state only

Sequence note/velocity edits update active playback without resetting the current step or transposition.

## State

The complete 1024-byte Prophet program payload remains the canonical stored hardware state and is round-tripped without normalising unknown bytes. `PluginStateCodec` is JUCE-independent so host-state compatibility can be tested without the plug-in framework.

Full state/SysEx loads synchronise exposed JUCE parameters with `setValueNotifyingHost()` outside the audio thread so `AudioProcessorValueTreeState`'s denormalised raw values remain coherent.

## Build

Core only:

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

VST3 with JUCE 9.0.4 fetched automatically:

```bash
cmake -S . -B build-plugin \
  -DP6_BUILD_PLUGIN=ON \
  -DP6_FETCH_JUCE=ON \
  -DP6_JUCE_TAG=9.0.4
cmake --build build-plugin --config Release --target P6CorePlugin_VST3 -j2
ctest --test-dir build-plugin -C Release --output-on-failure
```

Standalone is optional with `P6_BUILD_STANDALONE=ON`.
