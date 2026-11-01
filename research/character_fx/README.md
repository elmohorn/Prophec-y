# Character-FX comparison harness

Research only. No processor is linked into Prophec-y. Source revisions and license boundaries are in `calibration/prophecy_fx_sources_v020.json`. The downloader fetches only pinned MIT source selections for five candidates, keeping upstream notices in private scratch. No VST2 SDK, hardware SDK, GPL/LGPL module, GUI image or IR is imported into the plugin.

```sh
python research/character_fx/fetch_sources.py /path/to/private/fx_sources
python research/character_fx/build_probe.py /path/to/private/fx_sources
/path/to/private/fx_sources/fx_probe /path/to/private/probe_results
python research/character_fx/analyze_probe.py /path/to/private/probe_results --output /tmp/fx_metrics
python tests/character_fx_metrics_test.py
```

The harness compiles the original Airwindows Tube2 float processing body against a minimal SDK-free class shim. It extracts the original 0xFX Valve Color function and its two-state structure into a private build wrapper. DaisySP Overdrive is compiled from its original cpp; Patina and DSPark use their native headers. API adapters are not identical to the upstream plugin defaults: see `provenance.json` for pad, drive, stage count, sag and mix.

For the optional musical input, supply a stereo little-endian interleaved float32 file at48kHz as the second argument of `fx_probe`. The recorded source used a private modified factory000 with FX and distortion disabled, three notes48/52/55 at velocity0.8,2s gate and2s tail. Normalize that original Prophec-y render to RMS−18dBFS with peak at most−6dBFS before processing. No hardware recording is used for these auditions.

```sh
/path/to/private/fx_sources/fx_probe /path/to/private/probe_results /path/to/private/fx_sources/musical_source.f32
python research/character_fx/package_auditions.py /path/to/private/fx_sources /path/to/private/probe_results /tmp/auditions
```

`package_auditions.py` expects that input basename in the source directory. It trims declared DSPark latency and applies one constant gain per file to RMS−20dBFS, with a peak safeguard. This is not LUFS matching, matched distortion, a blind listening preference or proof of valve accuracy.

Sanitizers:

```sh
python research/character_fx/build_probe.py /path/to/private/fx_sources --sanitize
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 /path/to/private/fx_sources/fx_probe_asan /tmp/fx_sanitized /path/to/private/fx_sources/musical_source.f32
```

LeakSanitizer could not operate in the hosted ptrace environment. ASan/UBSan passed with leak checking disabled; do not claim a leak proof.

For quiet benchmarking, compile `fx_benchmark.cpp` with the same include directories and DaisySP overdrive.cpp used by `build_probe.py`, then run it after render/sanitizer jobs finish. It processes the same synthetic chord in two sequential48kHz stereo passes, blocks128, native dial0.5. Avoid CPU comparisons from concurrent runs. Ordinary `new` is counted; malloc, aligned allocations, locks, system calls and all realtime guarantees are not fully instrumented. Compiler warnings about mismatched allocation arise from this explicit malloc/free-backed global new/delete instrumentation; they are not evidence of an upstream allocation failure.

The native knob mappings differ substantially. Spectral residuals include noise and transient state, not only aliases. Do not use THD or CPU alone as a sonic quality ranking. Samples and reports are exploratory comparison data, not calibration against physical valves.
