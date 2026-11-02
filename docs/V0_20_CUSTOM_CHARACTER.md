# Four custom Character devices — native prototype

Implemented together with the existing recessed Tube, in one six-position
carousel: None, Tube, Plasma, Optical Spring, Ferrofluid, Neon Wavefolder.
The dark inset chamber and separate aluminum faceplate remain the common
hardware language. One Character insert is active at a time, before or after
the complete standard FX chain. All five devices retain independent values.

| Device | Audio behavior | Seven controls | Window meaning |
|---|---|---|---|
| Plasma | Stateful ignition threshold, hysteresis, hold, nonlinear conduction and tone filter | Drive, Ignition, Sustain, Character, Tone, Mix, Output | Discharge trace from actual wet samples on the dominant input channel; brightness/conduction percentage follows activity across both channels |
| Optical Spring | Dispersive six-stage allpass input into the existing spring FDN; three internal delay-line pickups and stereo blend | Drive, Decay, Tension, Pickup, Damping, Mix, Output | Three coils driven by measured pickup energy; pickup position illustrates the blend |
| Ferrofluid | Envelope-following resonant low-pass; bipolar envelope sensitivity | Cutoff, Resonance, Sensitivity, Response, Polarity, Mix, Output | Spike height follows measured input envelope; frequency readout is the actual cutoff for the dominant input channel |
| Neon Wavefolder | Asymmetric multi-fold transfer, bias, tone filter | Drive, Folds, Symmetry, Bias, Tone, Mix, Output | Exact nominal folding law before tone/DC/output plus separate measured wet trace |

These are software designs, not verified physical emulations of commercial
Plasma or optical spring pedals, xenon discharge physics, magnetic fluid or
neon tubes. Spring visuals illustrate actual delay-line energy, not a measured
mechanical displacement. Ferrofluid's silhouette is an envelope indicator,
not a fluid simulation. Neon transfer remains visible when inactive because
it represents a control law, not emitted signal energy. Motion pauses live traces and Tube illumination; bypass clears activity. Drawing is native JUCE, on the message thread.

## DSP and compatibility

- Original Plasma state machine; DaisySP MIT Wavefolder/SVF/DCBlock kernels;
  DSPark MIT oversampling: 2x High for Plasma/Spring/Ferrofluid, 4x Medium
  for Neon. See `THIRD_PARTY_NOTICES.md`.
- Fixed 71-sample wrapper latency, including None, bypass and Mix=0. Custom
  wet paths use the 64-sample oversampler plus 7 samples of alignment; Neon
  uses 48 samples plus 23 of alignment. Spring
  tank delay and filter phase remain intentional effect behavior.
- Device processing parameters smooth over 20 ms; wet/dry uses a 10 ms
  full-scale ramp. Module/route changes fade the complete output
  down over 10 ms, reset at zero, prime latency and fade back in. Changing
  device clears its tail. Processing allocates no memory in the audio callback;
  the state-publication gate is only attempted without waiting.
- Nonlinear effects use 2x or 4x oversampling; this reduces aliasing, but extreme
  wavefolding is not claimed alias-free. Output can exceed full scale; use
  device Output and final Master, with the existing clip indicator.
- All 28 added controls and the custom selector are appended host IDs. The
  legacy `character_type` remains exactly None/Tube, preserving its normalized
  automation. `character_custom_type` overrides it only when nonzero.
- Wrapper codec v4 appended the four independent parameter arrays and the
  underlying legacy selector. Codec v5 appends the global MIDI input channel;
  v1/v2/v3/v4 still load; old None/Tube, output gain, unknown payload bytes and SysEx are
  preserved. New fields belong only to the software wrapper, never Prophet
  SysEx. Old plugin versions cannot read v5 sessions.

## Validation

Core regression suites cover every module at 44.1/48/96/192 kHz, deterministic
output, finite hot-input behavior, zero ordinary-new allocation during rapid
module/route/parameter changes, silent-right isolation except the coupled
spring tank, exact 71-sample dry null for bypass/Mix=0, and spring impulse-tail
decay. State tests cover all six choices, inactive parameter arrays, previous
codec versions and permanent historical fixtures.

Native smoke checks all 28 visible control bindings, complete wrapper restore,
unchanged exported SysEx, current selection on editor reopen, and captures
all four native windows. Linux/Windows VST3s, pluginval 10 and sanitizer checks
must pass before the release job publishes downloads. No hardware matching or
subjective listening acceptance is inferred from these engineering tests.
