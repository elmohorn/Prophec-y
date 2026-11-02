# Tube and Plasma — quality focus

The current focus is the sound and visual quality of Tube and Plasma. The other
Character devices remain available for compatibility, but their refinement is
postponed. No physical-hardware parity is claimed.

## Reproducible first listening set

`tools/tube_plasma_audition.cpp` renders an identical eight-second core synth
phrase through None, Tube and Plasma at two parameter settings. Build it with
p6character and p6core, then run `tube_plasma_audition OUTPUT_DIRECTORY`.
`tools/package_tube_plasma_audition.py OUTPUT_DIRECTORY` converts the raw output
to WAV and produces original-level and global-RMS-matched comparisons. Requires
NumPy and SciPy. None uses the same Character latency. Both sets are generated
from the same renders; RMS matching is not perceptual loudness matching and can
mask dynamic differences. No limiter or per-file normalization is applied to
original-level files.

On one local 48 kHz run, this exact input and settings yielded:

| Setting | Peak (linear) | RMS vs dry |
|---|---:|---:|
| Dry | 0.190 | 0 dB |
| Tube default | 0.097 | -6.16 dB |
| Tube driven | 0.154 | -1.09 dB |
| Plasma default | 0.570 | +14.78 dB |
| Plasma driven | 0.578 | +17.38 dB |

This exposes a potential gain-staging usability issue, not a finding that all
Plasma presets are too loud or that a gain-law change has been decided. The
one short synthetic phrase is not sufficient to establish musical quality,
perceived loudness, hardware accuracy, aliasing or automation continuity.
Both original-level and matched samples must be auditioned before revising gain.

## Visual and runtime quality still needed

The experimental branch has an audio-reactive procedural Plasma cartridge. It
is an initial 2D view, not a physically simulated discharge; the Ferrofluid work
on that branch is postponed. The existing Tube uses a recessed image and
telemetry-driven illumination. Both still require owner visual approval, motion
review in a DAW, CPU/GPU measurements with editor open/closed, and a fallback for
constrained machines. Repositories discussed for animation have been researched
as references but no code from them has been integrated in this experiment.

Before considering either effect finished: compare several real synth patches
and dynamics; check steady-state and transient levels, aliasing, controls and
automation; listen at matched loudness; validate real DAWs; and seek independent
listener feedback. Preserve saved states and automation if revising sound.
