# v0.15 changelog

## Host / wrapper hardening

- Added JUCE-independent `P6MidiRouter` for performance MIDI, system realtime and Prophet-6 NRPN streams.
- Added complete NRPN selection/data-entry handling (CC99/98 + CC6/38), Data Increment/Decrement (CC96/97), and stricter RPN-null reset semantics.
- Added NRPN 1088 `Seq Play/Stop` as runtime-only control; it is never written into the 1024-byte program payload.
- Split canonical/UI program state from audio-thread-owned `audioPatch_`.
- Host automation is merged per NRPN into both state domains instead of overwriting the complete audio patch.
- Removed `AsyncUpdater` from realtime paths; a message-thread timer mirrors atomic realtime edits back to canonical/UI state.
- Corrected APVTS synchronisation after full state/SysEx loads by using host-notifying parameter updates off the audio thread.
- Reserved raw-parameter storage before playback and stopped unsupported/global NRPNs from becoming synthetic program entries.
- Sequence NRPN edits preserve current playback step and transposition.

## Plug-in build

- JUCE remains optional and outside the core.
- JUCE version pinned to 9.0.4 when fetched by CMake.
- VST3 is the default plug-in target; Standalone is opt-in.
- Reduced wrapper linkage to the JUCE modules required by the current editor/processor path.
- Added cross-platform GitHub Actions build/test/pluginval workflow for Linux, macOS and Windows.

## Regression status

The deterministic hard-sync regression render remains byte-identical to v0.13/v0.14:

`d5701a38fdc467104a602412a5a197139700527dee7975d9c0840968457a0487`

The clean v0.15 core build passes both registered CTest tests.
