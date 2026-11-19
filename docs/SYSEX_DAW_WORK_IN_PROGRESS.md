# DAW SysEx receive — work in progress

The plugin does not yet apply complete SysEx events received from the host.
The fixed-size decoder in `include/p6/RealtimeSysexDecoder.h` validates and
unpacks complete Prophet-6 program/edit-buffer frames without allocation;
it is intentionally not connected to the audio processor yet.

Before connecting it, the design must preserve the ordering between a dump,
subsequent MIDI NRPN events and DAW automation/state restoration. A delayed
message-thread import using the existing `loadSysex()` can discard later NRPN
edits or overwrite newer host state. A safe implementation needs bounded
handoff, explicit invalidation/ordering and deterministic controlled-race
coverage, including dump followed by edits in the same buffer. Do not treat a
passing decoder test as evidence of working DAW reception.

The Prophet-6 manual describes whole-step ties but does not specify the raw
velocity high-bit encoding. Preserve the existing raw bit until a documented
specification or a paired hardware capture establishes its meaning. A
Prophec-Y-only gate feature needs a separate versioned state representation;
its data must not be silently mapped onto the unknown Sequential bit.
