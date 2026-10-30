# v0.16 stability gate — Windows + Linux

This milestone is intentionally about correctness and host portability rather than new synthesis features.

## Fixed before CI

- Release test executables explicitly undefine `NDEBUG`, so the existing `assert()` regression checks remain active instead of compiling away.
- Chord-memory capture now tracks physically held live keys in normal polyphonic mode; generated arp/sequencer notes do not enter the capture set.
- The plug-in uses an explicit valid bundle identifier: `com.elmohorn.p6core`.
- The CI matrix is reduced to the two target platforms requested for distribution: Windows and Linux.
- Windows uses the pinned JUCE default from CMake rather than passing a dotted tag through PowerShell, avoiding the previous `9` / `.0.4` argument split.
- CI explicitly builds `p6core_tests`, `p6_plugin_state_tests`, and the VST3 target before invoking CTest.
- Linux adds a Debug ASan+UBSan job in addition to the Release VST3 + pluginval validation.
- The obsolete Linux smoke workflow was removed so one workflow is the source of truth.
- `ProgramLayout` now has the correct 87-entry cardinality. The former 88-entry declaration silently created a default trailing entry that duplicated NRPN 0 with a zero-width range, producing a `NaN` host parameter and failing pluginval state restoration.
- A regression test now checks serialized-layout cardinality, non-empty names, valid ranges, unique NRPNs, unique SysEx offsets, and registry coverage.

## Local validation before CI

- Debug CTest: 2/2 pass.
- Release CTest with assertions active: 2/2 pass.
- Debug ASan+UBSan: 2/2 pass.
- Deterministic hard-sync regression remains byte-identical to v0.15 for the controlled probe:
  `766936d0c6467c527cce32f88d933c46de2660819886e8f779deae74b7657e9b`.

## Windows/Linux validation

The validated branch run (`41105413609`) completed successfully on both requested targets:

- Linux Release VST3: build PASS, CTest 2/2 PASS, pluginval strictness 10 PASS.
- Windows Release VST3: build PASS, CTest 2/2 PASS, pluginval strictness 10 PASS.
- Linux Debug ASan+UBSan: PASS.

The resulting VST3 artifacts are retained by GitHub Actions. v0.16 was then promoted to `main`; the promotion commit exists to trigger the same CI gate on the canonical branch.

## Gate for subsequent releases

Future changes should not be considered release-ready unless Windows and Linux both build the VST3, run the core/state tests, and pass `pluginval --strictness-level 10`. The Linux sanitizer job must remain green as an additional correctness gate.
