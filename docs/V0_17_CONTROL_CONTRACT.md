# v0.17 — control / performance contract

v0.17 is deliberately limited to controller semantics, realtime ownership and the remaining performance-control contract. It does **not** start the digital-effects or custom-UI milestones.

## Reuse-first audit before implementation

The project rule for this and subsequent milestones is: inspect mature/reference implementations first, verify licensing, then decide whether to reuse, adapt, or implement locally.

### `joshjetson/phosphor` — MIT

Re-reviewed before v0.17 work. It remains the strongest open Prophet-6/DSP reference and may be adapted with attribution.

Relevant findings for this milestone:

- Its broader input/recording architecture treats sustain as controller state that changes note lifetime rather than pretending a released key remains physically held. Prophec-y follows the same architectural separation, while implementing its own P6 engine integration.
- `phosphor` defines standard sustain-controller behavior (`CC64`) elsewhere in the project and contains sustain-note-lifetime tests, useful as a mature pattern rather than Prophet-6-specific source to copy.
- The Prophet-6 module does not provide a sufficiently proven P6 LFO-sync raw-frequency-to-note-division table.
- The TEO-5 module contains a mature synced-LFO architecture, useful as an architectural reference, but TEO-5 division constants/ranges must not be transplanted into the Prophet-6.

### `ncg777/NSeqArpKeys` — MIT

Previously reviewed and still relevant for ordered musical-event scheduling and note ownership. No source is copied for v0.17; the existing scheduler already incorporates the useful architectural lessons.

### `kschzt/harp` — Apache-2.0

Previously reviewed for host-PPQ/sample anchoring and separation between timing and the audio kernel. The existing `P6Clock`/scheduler architecture already follows that separation.

### `synthmutt/CtrlrPanelSequential` — GPL-3.0

Re-reviewed as a Prophet-6 protocol/editor reference. It is **reference-only** under the current Prophec-y licensing strategy; GPL source is not copied into the project.

### JUCE / pluginval

JUCE remains the maintained VST3/host wrapper rather than custom VST3 glue. `pluginval --strictness-level 10` remains the Windows/Linux release gate.

## Evidence-backed v0.17 decisions

### Sustain / Damper (CC64)

The Prophet-6 receives CC64 as Damper; values `>= 64` engage sustain. v0.17 models it inside `P6Engine`, not in the wrapper:

- physical Note Off removes the key from the held-key set immediately;
- if sustain is down, the live voice's envelope release is deferred;
- pedal-up releases voices whose keys are no longer down;
- generated arp/sequencer note ownership is not extended by the damper pedal, preventing Damper from becoming a second Arp HOLD implementation;
- Unison is sustained/released as one keyboard stack.

This keeps Chord Memory and Low/High/Last key priority based on actual physical key state.

### MIDI Volume (CC7)

CC7 is runtime performance gain and remains separate from the saved program Voice Volume byte. It multiplies the existing program output gain and is reset to maximum by CC121. Receiving CC7 never modifies `P6PatchState`.

### Reset All Controllers (CC121)

For controller classes currently modeled by Prophec-y, CC121 restores:

- Mod Wheel -> 0;
- Sustain -> off;
- MIDI Volume -> maximum.

Channel pressure and pitch bend are separate MIDI message classes and are not silently reinterpreted as CC state.

### Host automation vs MIDI NRPN ordering

v0.16 used fixed atomic value mailboxes. That was realtime-safe but could allow a delayed MIDI mirror to overwrite a newer DAW edit.

v0.17 adds the JUCE-independent `VersionedEditMailbox<N>`:

- every MIDI edit receives a monotonically increasing generation;
- a host edit advances the same parameter's generation;
- canonical/UI consumers accept a queued MIDI edit only if its generation is still current;
- value and generation are packed into one 64-bit atomic to avoid torn pairs;
- no allocation, lock or OS message is required on the publishing/audio path.

A dedicated regression test covers MIDI -> host (host wins), host -> MIDI (MIDI wins), repeated MIDI collapse and independent parameters.

## Deliberately not guessed: LFO Sync division law

Public evidence establishes that Prophet-6 LFO Sync locks the LFO to the arpeggiator/sequencer/MIDI clock and that the LFO Frequency control selects rhythmic values while synced. However, this audit did **not** find a sufficiently authoritative raw `0..254` -> rhythmic-division mapping.

Therefore v0.17 must not copy TEO-5 values or invent thresholds merely to make the control appear complete. The parity-matrix row remains `CONNECTED / INCOMPLETE` until the exact P6 law is established from controlled hardware measurements or a strong independently verified implementation.

A smaller documented behavior — resetting the synced LFO on the first key of a newly started physical chord, but not on overlapping additional keys — may be implemented separately because it does not require guessing the division table.

## Current v0.17 tests

`p6_control_contract_tests` uses always-on checks in both Debug and Release and covers:

- versioned last-writer-wins mailbox behavior;
- physical-key vs sustained-voice separation;
- generated-note ownership not being extended by sustain;
- Unison sustain as a single stack;
- exact CC64 threshold at 64;
- CC7 runtime volume;
- CC121 controller reset;
- proof that CC7/CC64/CC121 do not mutate the saved 1024-byte program payload.

The suite is included in Windows/Linux Release CI and Linux ASan+UBSan CI.
