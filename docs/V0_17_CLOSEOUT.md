# v0.17 closeout — control / performance contract

v0.17 freezes the control, performance and realtime ownership contract before digital FX and custom UI work begin.

## Release baseline

Canonical branch: `v0.17-control-contract`

Validated implementation baseline before documentation closeout: `eecab18fa9fffd70ef361a90acaee13a57cf5e4d`

Version: `0.17.0`

## Completed scope

- Versioned lock-free host/MIDI edit ordering so stale MIDI mirrors cannot overwrite newer host edits.
- Sustain/Damper CC64 with physical-key state separated from sounding/sustained voice lifetime.
- MIDI Volume CC7 as runtime gain, independent of the saved 1024-byte Prophet program state.
- Reset All Controllers CC121 for the controller classes modeled by Prophec-y.
- MIDI channel-mode semantics including All Sound Off CC120 and All Notes Off CC123.
- Realtime allocation regression coverage.
- Removal of heap-allocation risk from arpeggiator sorting by using fixed-size deterministic sorting.
- Evidence-backed synced-LFO first-key phase reset behavior.
- Preservation of unknown Prophet-6 semantics instead of inventing mappings.

## Deliberately deferred / opaque

These are not release blockers for v0.17:

- Exact Prophet-6 synced-LFO raw-frequency-to-rhythmic-division law. Public evidence is insufficient for a trustworthy `0..254` mapping, so no guessed table is shipped.
- Foot Controller CC4 mapping until the Prophet-6 behavior is established strongly enough to avoid duplicating or misrouting existing parameter semantics.
- Chord-memory serialized storage location.
- Sequence velocity bit 7 semantics.
- Sequence Mode / Play Mode edge semantics.
- Realtime SysEx decode remains intentionally outside the audio callback; further handoff work can continue in a later milestone without blocking the stable non-SysEx realtime path.

## Final validation gate

The final functional baseline passed the complete release gate on GitHub Actions:

- Windows Release VST3 build: PASS
- Windows core/state/control/realtime tests: PASS
- Windows `pluginval --strictness-level 10`: PASS
- Linux Release VST3 build: PASS
- Linux core/state/control/realtime tests: PASS
- Linux `pluginval --strictness-level 10`: PASS
- Linux ASan + UBSan tests: PASS

Validated workflow run: `41139022711`.

## Branch note

`v0.17-control-performance` was a parallel experimental line. The canonical release line is `v0.17-control-contract`, which already contains equivalent or stronger coverage for sustain, MIDI Volume, controller reset, channel-mode behavior, causal edit ordering, LFO first-key sync reset and realtime allocation safety. The experimental branch is therefore not merged into the release line.

## Exit decision

v0.17 is complete when this documentation closeout is integrated with the canonical code line and the post-integration CI gate remains green. No digital effects, custom UI, speculative MIDI mappings or new synthesis features belong in this release.

Next active milestone: **v0.18 — digital effects**.
