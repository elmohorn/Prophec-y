# Historical state-format compatibility fixtures

These are **synthetic fixtures emitted by the unchanged historical encoders**, not captured DAW projects and not current-state bytes relabelled as old releases. v0.17/v0.18 tags and the v0.19 validated commit contain the same v1 encoder blob. The pinned v0.20 checkpoint emits v2, including Character state. A separate v3 fixture was emitted by the unchanged preview 275 encoder and includes a -23.4 dB Master. A second fixture comes from the unchanged preview 278 v4 encoder and tests migration to the v5 Omni default. Each uses different seeded payload data, including opaque/high-bit bytes, to catch destructive normalization.

`manifest.json` records source refs, the original encoder blob/SHA-256, seed, format, size and fixture SHA-256. Release names and serialized format versions are deliberately separate: three product releases share format v1.

Reproduction: fetch `plugin/PluginStateCodec.cpp` and `.h` at the recorded source ref into an isolated directory, without modifying them. Compile them with `generate_fixture.cpp`, setting `P6_FIXTURE_SEED` to the manifest seed and adding `P6_FIXTURE_CHARACTER` for the v2/v3/v4 fixtures and `P6_FIXTURE_MASTER` for v3/v4. The original header directory must precede the current plugin include directory. Current core support symbols/types can be linked: the historical `encodeState` path only writes the explicitly seeded state. It does not call the mapper or decoder. Compare emitted bytes against the recorded SHA-256; never overwrite a fixture merely to make a regression pass.

The C++ compatibility test reads the committed bytes, verifies all 1024 payload bytes, wrapper extras, bank/program/name, sequence controls and Character defaults/migration, then rejects every truncated prefix and an unknown future version without modifying the caller's state. It does not regenerate its own test inputs.

These fixtures establish serialized-format compatibility. Captured projects and multi-host session migration remain separate DAW validation work.
