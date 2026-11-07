# Recovered native analysis tools

These seven project-native scripts were restored from the existing user artifact
`p6_vst_phase1_v0.15.zip` (SHA-256 recorded below), rather than replacing native
analysis with an unrelated implementation. They had been referenced by README
but were missing from the repository snapshot.

Restored: compare_audio.py, sync_diagnostics.py, reference_index.py,
infer_performance.py, compare_program.py, run_calibration_suite.py, joint_fit.py.

They retain historical metrics/assumptions. Their scores are not interchangeable
with the new `tools/compare_online_suite.py` metrics. The latter additionally
reports common-pre-roll nuisance ranges and independent stereo-channel power.

The full source MP3 and factory SysEx remain private local inputs. Restoration
of analysis code does not imply the full source audio has been recovered.
de217f74c95138d3750c39a0b86c0421bf41ae87f807792aaf0428efbee5b150  ../reference_assets/p6_vst_phase1_v0.15.zip
