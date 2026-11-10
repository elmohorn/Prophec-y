# Arpeggiator review — 2026-11-10

## Implementation reviewed

`P6Arpeggiator`, `P6Clock`, `P6PerformanceScheduler`, `P6ArpCharacter`, the JUCE MIDI/parameter bridge, the editor, core tests and plugin smoke tests. The visual reference is the approved arpeggiator mockup; the baseline capture was `ProphecY_arp_redisenado.png`.

## Corrected in this pass

1. Free-start key changes no longer restart the external MIDI pulse counter. Before this fix, playing between F8 messages shifted the next 16th-note edge by as many as six pulses. Internal clock still restarts its interval as designed.
2. Beat Sync releases the generated note immediately when the final physical key is released, even if the external clock stops before the next edge. HOLD continues to retain its latch.
3. Applying a patch change that leaves mode/range unchanged no longer resets the note order to the first step.
4. The octave range is now expressed as three physical choices and division as a Rate knob, connected to the existing NRPN parameters. The BPM and clock source retain their second row because these are functional controls with no approved alternate location yet.
5. Both panel collapse controls use a legible labelled rocker. Step buttons have stronger depth and a limited glow around the *playing* key; muted/rest steps remain dark, without an invented grey hardware state.

## Still provisional or missing

| Priority | Item | Present state | Next proof or implementation |
|---|---|---|---|
| High | External timing in real DAWs | Host PPQ and MIDI sample tests; no long host/hardware soak | Run transport seek, stop/continue, tempo automation, note input between F8 pulses at 44.1/48/96 kHz; compare recorded MIDI/audio onset grids. |
| High | UI visual acceptance | Working controls, now closer to mockup; no post-change plugin screenshot yet | Inspect CI screenshot at normal and reduced scale; check rate label, three octave lamps, rocker and active-step halo. |
| Medium | Sync transition semantics | Host fallback when stopped; reconnect on play | Test changing Internal/Host/MIDI while notes are held, missing PPQ, seeks and loop boundaries. Specify whether a transition restarts the musical phrase. |
| Medium | Audible note retrigger | Generated note-off and next note-on share a sample | Listen and measure the click envelope for repeated pitches, hard gates and short attack/release presets; use a bounded gate or voice policy only if evidence supports it. |
| Medium | Character pattern | Eight binary note/rest steps, rhythm count, velocity Motion, Freeze | If adding ratchets or humanization, specify timing and state migration first. Never treat these as Prophet-6 patch data. |
| Lower | Prophet swing and Random parity | Swing timing deliberately straight; Random deterministic but not a hardware clone | Measure Prophet-6 swing ratio and Random behavior before claiming parity; keep the UI tooltip explicit. |
| Lower | Generated MIDI output | Arp plays internally only | Decide whether DAW MIDI output is a product requirement, then implement host capability and event timing tests. |

## Design acceptance against the approved mockup

The current plugin keeps the arpeggiator as a chassis extension above Character FX, with no extra black-glass settings strip. The three octave choices, Rate knob, dimensional eight-step keys and labelled collapse rocker now follow the approved visual language. The mockup shows a single strongly lit current step; a real playing note must supply that highlight. All eight default steps are armed, so their standby lamps should remain subdued. The current panel retains BPM/clock source as an additional row. The Character FX panel must use the same rocker, with a larger reachable target; its controls remain part of the synthesizer chassis. Final acceptance needs a compiled screenshot and user comparison, not just code inspection.
