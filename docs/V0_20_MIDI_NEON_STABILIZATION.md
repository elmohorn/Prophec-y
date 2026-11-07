# v0.20 — MIDI input and Neon stabilization

## MIDI input

The Module header provides `MIDI: OMNI` / `MIDI: 1`–`MIDI: 16`.
For a selected channel, channel-voice messages from other channels are ignored;
MIDI clock/start/continue/stop are system messages and remain available.
Changing the selection silences currently held notes, clears standard FX tails,
resets sustain/pitch/modulation/volume and clears NRPN selection. This prevents
filtered note-offs or stale controllers leaving notes stuck. The active Channel
is a global DAW-session setting, not part of a musical preset or Prophet SysEx.
Legacy v1–v4 sessions default to Omni. Wrapper state v5 appends one byte; old
plugin versions cannot read the newer state. Host parameter IDs are appended.

The compact two-event monitor shows incoming `CH n PB raw14` and `CH n CC x y`.
An asterisk means the selected channel filtered the event. It records input
before the channel filter and is a UI diagnostic; rapid bursts can overwrite
old events. A single-channel setting avoids cross-channel performance control.
In Omni, equal-pitch notes from different channels coalesce into one live note;
it is released after the last owning channel sends Note Off. Performance
controllers and CC120/CC123 remain global panic actions. Per-channel sustain,
separate same-pitch voices and MPE behavior are not implemented.

## Neon

Neon applies asymmetric slope to the zero-centered input before adding Bias,
so the transfer stays continuous at zero. It uses 4x Medium oversampling with
48 samples FIR delay plus 23 samples alignment, preserving the existing 71
sample wrapper latency. The other Character devices retain 2x High. Device traces use the dominant input channel; Plasma conduction tracks activity across both channels. Tube illumination freezes with Motion off. This
changes the sound of Neon presets from earlier previews and increases its CPU
cost. The UI transfer curve uses the same nonlinear law before tone/DC stages.

`tools/neon_rate_probe.cpp` and `tools/analyze_neon_rates.py` reproduce the
convergence experiment described in `AUDITORIA_GLOBAL_2026_11_02.md`. A
4x-higher host rate is an approximation, not an exact hardware reference;
this comparison contains filter and discretization differences in addition to
aliasing. No claim of alias-free sound or external listening acceptance is made.

## Validation and outstanding work

Core tests cover all channel values, channel filtering, reset on channel change,
interleaved NRPN, historical v1–v4 codec fixtures, unchanged preset dirty state,
Neon near-zero continuity, wet impulse alignment and Ferrofluid's 10–90 % rise
across four rates. Native smoke covers selector binding and session/preset
migration. CI and release checks are needed on the exact commit before binaries
are considered validated. Manual FA-08/DAW tests, per-channel sustain and MPE, full
transactional restore, Character wrapper/UI CPU, deeper accessibility and
subjective effect audition remain pending.

## State restore: controlled concurrency

A controlled listener pause reproduced preview 280 rendering the new Character
with the previous Master during a state restore. The current correction marks
state publication in progress, waits on the writer side for an in-flight short
control snapshot to finish, then updates the state. `processBlock` only tries a
non-blocking gate and continues with the prior applied controls if a restore is
in progress or the patch is busy. A recursive writer mutex serializes state
restores/imports and state/SysEx reads; host notifications are not made while
holding the audio gate. MIDI events continue to be processed using the last
applied calibration, and the UI MIDI mirror waits until the writer finishes.
The probe checks a paused restore with incoming NRPN against an untouched audio
reference, simultaneous state save, correct application after resume and
24 competing-writer rounds while audio is processed.

This covers the tested interleavings; it does not prove all host reentrancy,
concurrent prepare/release or MIDI automation semantics. State save/load APIs
remain non-realtime operations and can wait for another state writer. Manual DAW
and longer stress tests remain necessary.
