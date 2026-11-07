# Optional controlled-capture tooling

This directory contains infrastructure for validating and analyzing a future controlled Prophet-6 capture **if one becomes available independently**. It is **not** the source-of-truth requirement for v0.19 and the project owner is not expected to obtain, borrow or record a Prophet-6.

The active v0.19 authority and roadmap live in `docs/V0_19_SOUND_MATCHING.md`. v0.19 is built around existing and newly discovered **online real-Prophet-6 recordings**, beginning with Reference-A, plus program/SysEx information and reconstructed performance events.

The files here are retained because several components are reusable:

- `manifest.schema.json` — generic metadata ideas for reference audio;
- `examples/P0A_manifest.example.json` — a controlled-capture example only;
- generated P0-A capture-pack tooling — optional future utility, not a milestone dependency;
- validation/report code — reusable concepts for online-source manifests and model-vs-reference metrics.

No v0.19 task may block waiting for a user-recorded hardware take.

## Why this directory still exists

A controlled physical capture would provide stronger evidence than a compressed web recording, so the infrastructure remains useful for contributors or future opportunities. Keeping it does **not** change the evidence available to the current project.

For current development:

1. use Reference-A and other public Prophet-6 recordings;
2. store source/segment provenance and confidence metadata rather than pretending unknown settings are known;
3. reconstruct performance events separately from DSP fitting;
4. compare Prophec-y renders with metrics appropriate to the source quality;
5. never redistribute third-party factory audio unless its license explicitly permits it.

The 88-take P0-A generator and the strict 48/96 kHz WAV contract are therefore documentation for an optional future controlled session, not instructions for the project owner.
