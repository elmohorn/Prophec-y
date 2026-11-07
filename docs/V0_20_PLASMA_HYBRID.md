# Plasma Hybrid — v0.20 integration candidate

Plasma now offers two algorithms. New instances and Factory Init use **Hybrid**
with Mix 50%, matching the audition candidate selected by the owner. Legacy
preserves the historical Plasma DSP. The mode selector is visible when Plasma
is selected; it is also appended as an independent host parameter
`character_plasma_mode` (0 Legacy, 1 Hybrid). Existing parameter IDs/indices,
Prophet SysEx and the 71-sample Character latency remain unchanged.

Hybrid uses the evaluated 2 ms attack / 30 ms release voltage envelope, smooth
ignition knee and parameter-dependent wet gain compensation. The envelope
coefficients are calculated during prepare rather than each sample. Changing
mode while Plasma is active uses the existing route fade/reset/latency prime.
This is a creative audio design, not a verified physical Plasma model.

## Compatibility

Wrapper codec v6 adds one byte after v5 for the mode. Old v1–v5 states decode as
Legacy, preserving their saved Plasma parameters and sound. New v6 saves retain
the choice; old plugin versions do not understand v6, so keep backups before
opening and saving sessions with this candidate. An independent v5 fixture
emitted by the historical encoder is checked by the compatibility test. If an
older project did not use Plasma but its mode displays Legacy, select Hybrid in
the Plasma panel and save a copy. Preset mode is part of the musical state.

## Verification and limitations

Local tests cover 44.1/48/96 kHz algorithm transition, finite audio, no dynamic
allocations during mode changes, state v1–v6 migration and rejection. For one
coherent 48 kHz stimulus the legacy and hybrid renders were each bit-identical
with their corresponding pre-integration baseline. This is not a proof of
identity for every parameter, rate or host. Full Windows/Linux VST3, native UI,
pluginval, sanitizers, waveform/parameter automation, prolonged DAW sessions,
listening to multiple patches, CPU and independent alias tests remain required
on the final candidate. The owner's preferred Hybrid listening sample informed
selection; no blind comparison or Prophet hardware equivalence is claimed.
