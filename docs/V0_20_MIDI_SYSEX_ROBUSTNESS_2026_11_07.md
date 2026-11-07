# MIDI mailbox and SysEx robustness — 7 November 2026

This branch is based on the v0.20 integration candidate (#9). It does not
change audio DSP, parameter IDs, the plugin state codec or SysEx encoding.

## Mailbox

The previous implementation could overwrite a newer pending MIDI publication
with a delayed older producer. `clear()` could also erase an edit published
after its barrier generation was allocated. Queue slots now use a generation-
ordered CAS, and clear invalidates old entries with a generation barrier rather
than zeroing the slots. Old entries are discarded lazily when consumed.

Test-only hooks compile solely when P6_MAILBOX_TEST_HOOK is defined. The control
contract test forces both interleavings and verifies both canonical and mirror
queues preserve the newer edit. The same first test fails against the previous
implementation with the identical instrumentation. The CAS loops are lock-free
on supported targets but not claimed wait-free. No allocations or mutexes were
added to the audio path. Concurrent event order during real host restore remains
subject to the wrapper's state contract; a DAW stress test is still required.

## SysEx and NRPN

The packed 7-bit Prophet-6 payload rejects bytes with the high bit set instead
of silently masking them. A malformed program followed by a valid program is
reported as an error while the second program is recovered for 0x80, 0xf0 and
0xf7 corruptions. Encoded valid messages are unchanged. This is strict input
validation for complete contiguous dumps; real-time MIDI status bytes inserted
inside a live stream are not newly supported.

NRPN normalization now clamps defensively inside upsertRawParameter; existing
call sites already clamp incoming values, so legal or existing out-of-range MIDI
handling is unchanged. Repeated CC38 behavior is retained and covered; RPN Null
is already implemented. No assumptions are made about unverified controller
behavior.

## Release test assertions

The two executables that rely on assert (`test_main` and v0.19 reference
isolation) now fail compilation if NDEBUG is set. Their existing CMake compiler
options remain unchanged. Other suites use explicit P6_CHECK-style checks.

## Verification and limits

Locally built using g++ 13: core test, parameter-domain audit, control contract,
realtime allocation contract, plugin state codec, historical state fixtures,
and reference isolation passed. A compile with -DNDEBUG was intentionally
rejected, and the existing DSP fingerprint is byte-identical on its scenarios.
These checks do not replace full Windows/Linux VST3 + pluginval, sanitizer CI,
thread sanitizer, stress with actual host callbacks, real hardware or DAW tests.
