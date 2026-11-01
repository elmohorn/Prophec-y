# Third-party notices

## joshjetson/phosphor

Portions of the oscillator morph/band-limiting approach, sub-sample sync strategy, Slop random-walk model, Prophet-6 filter candidates, envelope timing strategy, LFO behavior, distortion candidate, v0.12 performance-behavior cross-checks (key-priority enumeration, retrigger semantics, Mod-Wheel/LFO interaction and runtime chord-memory transposition), and the v0.18 initial BBD/digital-delay/chorus effects architecture were adapted from or informed directly by the MIT-licensed `joshjetson/phosphor` project. Hardware-facing behavior is kept separate from raw-state compatibility so later measurements or documentation can replace any inferred implementation details.

For v0.18 specifically, the adapted starting-point ideas include preallocated delay/modulation buffers, a mono wet delay line preserving stereo dry, BBD repeat low-pass loss plus slow clock wander, cubic interpolation for the wandering BBD read head, and quadrature stereo chorus taps. Numerical constants remain calibration candidates rather than claims about Sequential firmware or circuitry.

Source repository: `https://github.com/joshjetson/phosphor`

MIT License

Copyright (c) 2026 Josh Jetson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Reference-only projects

No source code was copied from the following GPL/AGPL projects in v0.4:

- `straplocked/analog-synth` — GPL-3.0-or-later. Used to compare JUCE organization, tests, hard-sync direction, generic filter choices and oversampling strategy.
- `wnetzel/all_in_the_multitouch` — GPL-3.0. Used as an independent behavioral/architecture reference for P6-inspired per-voice modulation in Pure Data.
- `christofmuc/KnobKraft-orm` — AGPL. Used only as an independent Prophet-6 SysEx/protocol cross-check.
- `synthmutt/CtrlrPanelSequential` — GPL-3.0. Used as a parameter serialization/protocol cross-check.

Public protocol facts are not treated as source-code reuse.

## v0.13 scheduling / arpeggiator architecture references (no copied source)

- `ncg777/NSeqArpKeys` — MIT. Inspected for sample-domain pattern scheduling, chronological event handling and note ownership/lifetime design. v0.13 implements these architectural ideas independently in the P6 core; no source or constants were copied.
- `kschzt/harp` — Apache-2.0. Inspected for PPQ/sample transport anchoring and separation of pure arpeggiator note selection from the audio kernel. v0.13 independently applies the same class of design to `P6Clock` and `P6PerformanceScheduler`; no source or constants were copied.

Sequential Prophet-6 manuals and MIDI Association reference tables are protocol/behavior documentation, not software dependencies.

## v0.14 wrapper/build references

- **JUCE 9.0.4** — external optional dependency for the VST3/Standalone wrapper. The wrapper follows JUCE's maintained `juce_add_plugin` / `AudioProcessor` / APVTS APIs and the official CMake AudioPlugin example. JUCE itself is **not** redistributed in this archive. JUCE 9 is offered under AGPLv3 or a commercial JUCE licence; distribution must comply with the chosen licence.
- **JUCE CMake AudioPlugin example** — ISC-licensed example used as the structural starting point for the wrapper target/lifecycle. P6-specific adapter/state/DSP code is original to this project.
- **sudara/pamplejuce** — inspected as a reference for modern cross-platform CMake CI, JUCE dependency handling and Tracktion pluginval validation. No Pamplejuce runtime/source files are copied into the plug-in.
- **Tracktion/pluginval** — planned external validation tool for the first linked VST3 builds; not bundled.

## v0.15 wrapper hardening references

- **JUCE 9.0.4 source/API documentation** — inspected to verify APVTS parameter notification semantics, `AsyncUpdater` realtime caveats, `Timer` message-thread behavior, `GenericAudioProcessorEditor` module location, and Linux module dependencies. JUCE remains an optional external dependency and is not copied into this archive.
- **Sequential Prophet-6 Operation Manual 2.1, Appendix C** — authoritative protocol reference for standard NRPN CC ordering, Data Increment/Decrement support, and runtime control NRPN 1088 (`Seq Play/Stop`).

No additional third-party runtime source was incorporated in v0.15.

## v0.18 effects references

- **ShmKnd/Patina** — MIT-licensed C++17 analog-modeling DSP library. Inspected as a secondary implementation reference for BBD, modulation, spring/plate reverb and ring-modulation architecture. No Patina source has been copied into the initial v0.18 effects slice. If later code is materially adapted, its MIT copyright/license text must be added here before merge.
- **Sequential Prophet-6 Operation Manual 2.1** — authoritative behavior reference for serial Effect A -> Effect B order, global true bypass, effect-family availability and the legacy MIDI/NRPN domains. The documented mismatch between the expanded post-OS-1.3 effect list and the legacy FX-select NRPN ranges is intentionally left unresolved rather than guessed.

## v0.20 isolated character-FX research harness

The optional research harness under `research/character_fx/` builds selected MIT-licensed sources from pinned Airwindows (Chris Johnson), DSPark (Cristian Moresi), Patina (ShmKnd), DaisySP (Electrosmith/credited contributors) and 0xFX (Dan Michael) revisions in a private source directory. Upstream LICENSE files are retained there. None of these new sources is linked into the production Prophec-y plugin by this research change. The SDK-free Tube2 shim and extraction of the 0xFX Valve Color kernel are research adapters. Any later vendoring/adoption must carry the corresponding complete notices, source provenance and dependency review. No GPL/LGPL kernel is used by this harness.
