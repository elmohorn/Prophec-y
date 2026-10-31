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
  model/
    ...
```

The exact SysEx is the complete patch-state authority. `raw_parameters` in the
manifest is only a human-auditable subset of the controls relevant to a test.

## Generate the P0-A capture pack

After saving one real Prophet-6 Program/Edit Buffer dump as the baseline, the
C++ generator builds a deterministic **88-take P0-A pack**. Every generated
SysEx is an Edit Buffer message, so the sweep does not target stored program
memory. Unknown bytes from the supplied baseline are preserved; only verified
serialized controls are changed.

```bash
./p6_generate_p0a_capture_pack baseline.syx capture-set-YYYYMMDD-A \
  --capture-set-id P6-A-YYYYMMDD-A \
  --unit-id P6-A \
  --firmware RECORD_EXACT_VERSION \
  --sample-rate 96000
```

The pack contains one SysEx and one planned WAV path per capture condition,
plus `manifest.json` and capture instructions. The 88 tasks cover Osc1 pitch,
Osc2 fine tune, shape anchors, the full PW sweep used by v0.19, individual
mixer-source laws and upper-level two-oscillator interaction probes.

The generator deliberately does not invent hardware globals or interface
metadata. Replace all `RECORD_*` placeholders and inspect the resulting
isolation state before recording.

### Hardware WAV timing convention

The hardware WAV is the **untouched full take**: start recording before the
manifest's `pre_roll_seconds`, retain that pre-roll in the file, then retain the
whole note and release. Do not trim the hardware take. This is intentional:
free-running oscillator state and pre-roll duration remain auditable.

`p6_render_reference` advances through the same pre-roll internally but writes
its model WAV starting at Note On. The P0-A report tool accounts for this
explicitly; it never tries to align the two files by guessing.

### Analog voice variance

Do not treat a single analog voice as exact ground truth. The Prophet-6 can
cycle repeated notes across its six analog voices, and voice-to-voice tuning,
filter and level differences can be comparable to a proposed calibration
change. During the first real session, record repeated anchor notes before and
after the P0-A sweep without changing gain. Those repeats are used to estimate
hardware repeatability; model improvements smaller than that uncertainty are
not promoted as measured wins.

The 88 generated conditions remain the deterministic transfer-function matrix;
repeat anchors are uncertainty controls, not 6x duplication of the whole pack.

## Validate a capture set

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

## Render the same P0-A matrix in Prophec-y

The batch renderer reads note, velocity, sample rate and timing directly from
the manifest and invokes the standardized v0.19 renderer with dry isolation:

```bash
python tools/render_reference_manifest.py \
  reference_hardware/capture-set-YYYYMMDD-A/manifest.json \
  --renderer ./build/p6_render_reference \
  --output-dir reference_hardware/capture-set-YYYYMMDD-A/model \
  --oversampling 2
```

Use `--dry-run` to inspect every command without rendering. Historical
Reference-A candidates can be selected explicitly with `--calibration v07`,
`v08` or `v09`; the default is the current unqualified mapper, so candidate
comparisons remain deliberate rather than implicit.

## Measure one pair

`reference_audio_metrics.py` reads both 24-bit hardware PCM and the float32 WAV
written by the model renderer. It reports DC, AC RMS, peak, positive-duty
fraction, frequency and H1–H16 relative levels without external Python
packages.

```bash
python tools/reference_audio_metrics.py hardware.wav \
  --compare model.wav \
  --start-seconds 2.5 \
  --compare-start-seconds 0.5 \
  --duration-seconds 1 \
  --pretty
```

Harmonic error ignores terms below -70 dBc in both signals, while a harmonic
present above that threshold in only one signal is penalized rather than being
silently dropped. This avoids letting numerical/noise-floor harmonics dominate
the score without hiding a genuinely missing or spurious partial.

## Build the P0-A report

For a complete capture set, the report tool chooses the same steady-state
window in hardware and model automatically, accounting for the retained
hardware pre-roll:

```bash
python tools/analyze_p0a_manifest.py \
  reference_hardware/capture-set-YYYYMMDD-A/manifest.json \
  --model-dir reference_hardware/capture-set-YYYYMMDD-A/model \
  --output p0a-report.json \
  --pretty
```

Absolute interface gain is not treated as a synth error. The report uses one
global level anchor (`MIX_OSC1_C4_RAW127` by default), then reports all other
RMS deviations relative to that single anchor. Pitch error, pulse duty and
relative H2–H16 spectra do not use that gain normalization. Per-file
normalization is deliberately forbidden because it would hide mixer/level-law
errors.

The report also applies metrics by test role: pitch sweeps are not scored on
harmonics, noise/mixed-source level tests are not assigned a fake pitch, and
shape/PW tests retain their spectral/duty metrics. A manifest may later specify
`analysis_metrics` explicitly; otherwise the stable P0-A test-ID convention is
used.

## IDs

- `test_id` identifies the control condition and may recur in later re-captures.
- `capture_id` identifies one concrete recorded take and must be unique within a
  manifest.
- `unit_id` is pseudonymous (`P6-A`, `P6-B`, ...). Do not store the hardware
  serial number or personally identifying metadata.

Never overwrite an accepted source WAV to make a fit look better. Re-capture
under a new `capture_id` and preserve the original.
