# v0.17 pre-implementation reference review

This document records the mandatory reuse/reference pass before changing the v0.17 control/performance runtime.

## Scope

v0.17 is limited to the control/performance contract: host/MIDI causal ordering, sustain pedal, LFO clock sync, and the basic performance CC map. Effects and bespoke UI are explicitly out of scope.

## Repositories and documentation reviewed

### joshjetson/phosphor — MIT

The Prophet-6 implementation remains the primary clean-room behavioral/DSP cross-check. For v0.17:

- Its current Prophet-6 engine does **not** implement Prophet-6 LFO Sync, so there is no P6-specific sync implementation to copy blindly.
- Its TEO-5 engine contains a mature generic clock-synced-LFO pattern: keep a separate sync flag/division, derive LFO Hz from shared tempo (`1 / (beats * 60/BPM)`), and keep the division table isolated from the oscillator implementation.
- Its Rhodes engine demonstrates the useful sustain-pedal ownership pattern: physical key-up is recorded independently of whether envelope release is deferred by the pedal. Prophec-y will adapt the architecture, not the instrument-specific code.

### NSeqArpKeys — MIT / HARP — Apache-2.0

Previously reviewed for v0.13. Their useful architectural lesson remains in force: note ownership/lifetime and transport timing stay outside the oscillator/DSP implementation. Sustain therefore belongs in the performance/voice lifetime path, not in the ADSR primitive itself.

### JUCE 9.0.4 / pluginval

No new JUCE subsystem is required for sustain or LFO Sync. MIDI CCs should continue through the JUCE-independent `P6MidiRouter`; the wrapper remains an adapter.

### Sequential Prophet-6 manuals

Authoritative behavioral constraints used for v0.17:

- CC64 Damper holds envelopes in Sustain for values >= 64.
- CC4 Foot Controller and CC7 Volume are received performance controllers; CC7 combines with the instrument's volume controls rather than replacing program state.
- LFO Sync synchronizes the LFO to the arpeggiator/sequencer/MIDI clock domain.
- With LFO Sync enabled, a new first key resets the LFO cycle; pressing an additional key while another key is already held does not reset it.
- Internal BPM affects synced LFO rate; when external MIDI clock is used, the internal BPM setting does not determine the external tempo.

## Implementation decisions

1. **Sustain:** adapt the established deferred-release pattern. A physical Note Off clears key ownership immediately; while CC64 is down the voice is marked pedal-held instead of entering release. Pedal-up releases only voices whose physical keys are already up. This preserves voice stealing and does not confuse sustain with Arp Hold.
2. **LFO Sync:** reuse the existing `P6Clock` tempo/source domain and the existing `P6LFO`; do not create a second transport clock. The exact Prophet-6 raw-frequency-to-sync-division quantisation remains isolated until independently evidenced.
3. **Host/MIDI ordering:** use fixed-size monotonic event epochs rather than locks or allocating queues. A delayed MIDI-to-host mirror must never overwrite newer host automation.
4. **CC map:** route CCs into existing performance or NRPN semantics instead of introducing parallel parameter implementations.

## Evidence boundary

The Prophet-6 manual and public controller maps establish NRPN 88 (LFO Frequency, 0..254) and NRPN 91 (LFO Sync, 0..1), but they do not publish the complete raw-frequency-to-rhythmic-division threshold table. Public tutorials confirm that the Frequency knob selects rhythmic values (for example 16th notes) while sync is enabled, but not the full 0..254 bin boundaries. Until those thresholds are measured or independently documented, they must remain explicitly provisional rather than being presented as verified hardware law.
