# Controlled Prophet-6 reference captures

This directory defines the **source-of-truth capture contract** for v0.19 sound
matching. The authoritative policy and test matrix live in
`docs/V0_19_SOUND_MATCHING.md`.

The tracked files here are intentionally lightweight:

- `manifest.schema.json` — formal metadata contract;
- `examples/P0A_manifest.example.json` — filled template for the first
  oscillator/pulse-width capture family;
- raw WAV/SysEx capture sets are added only when they have been acquired and
  their storage/versioning policy is explicit.

A materialized capture set should look like:

```text
capture-set-YYYYMMDD-A/
  manifest.json
  sysex/
    ...
  audio/
    ...
```

The exact SysEx is the complete patch-state authority. `raw_parameters` in the
manifest is only a human-auditable subset of the controls relevant to a test.

## Validation

The validator uses only the Python standard library:

```bash
python tools/validate_reference_manifest.py \
  reference_hardware/capture-set-YYYYMMDD-A/manifest.json
```

For templates or metadata review before WAV/SysEx files exist:

```bash
python tools/validate_reference_manifest.py \
  reference_hardware/examples/P0A_manifest.example.json \
  --metadata-only
```

A `calibration` capture is rejected unless its manifest declares stereo
uncompressed PCM at 48 or 96 kHz and at least 24-bit depth. The validator also
checks that referenced files cannot escape the capture-set directory, that
SysEx files are framed by `F0 ... F7`, and that WAV channel/rate/bit-depth
metadata agrees with the manifest.

Passing validation does **not** prove that a capture is acoustically suitable.
Clipping, wrong hardware globals, wrong patch state, DAW processing, unstable
gain, or a mislabeled test can still invalidate a take. Those conditions are
part of the capture procedure and later analysis report.

## IDs

- `test_id` identifies the control condition and may recur in later re-captures.
- `capture_id` identifies one concrete recorded take and must be unique within a
  manifest.
- `unit_id` is pseudonymous (`P6-A`, `P6-B`, ...). Do not store the hardware
  serial number or personally identifying metadata.

Never overwrite an accepted source WAV to make a fit look better. Re-capture
under a new `capture_id` and preserve the original.
