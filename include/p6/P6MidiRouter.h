#pragma once

#include "p6/NrpnCodec.h"
#include "p6/P6PatchState.h"
#include "p6/P6PerformanceScheduler.h"
#include "p6/ParameterMapper.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <span>

namespace p6 {

// Result of routing one complete MIDI message through the performance layer.
// NRPN edits report the exact parameter/value so a wrapper may mirror the
// hardware-style edit onto its host-facing parameter surface off the RT thread.
struct MidiRouteResult {
    bool consumed{};
    bool patchChanged{};
    bool sysexDeferred{};
    std::optional<NrpnUpdate> nrpnUpdate;
    // Prophet-6 control NRPN 1088 is runtime Seq Play/Stop, not program data.
    std::optional<bool> sequencePlay;
};

// JUCE/VST-independent MIDI input router. Keeping message semantics here makes
// the plug-in wrapper a transport adapter rather than a second performance
// implementation. SysEx is only identified here; parsing remains explicitly
// non-realtime because SysexCodec allocates strings/vectors.
class P6MidiRouter {
public:
    void reset() noexcept
    {
        nrpn_.reset();
        pitchBendRaw14_.store(8192, std::memory_order_relaxed);
        pitchBendRevision_.fetch_add(1, std::memory_order_relaxed);
    }

    MidiRouteResult process(std::span<const std::uint8_t> message,
                            P6PatchState& patch,
                            P6PerformanceScheduler& scheduler,
                            const MapperCalibration& calibration = {});

    // UI/debug telemetry only. The audio path still consumes the decoded bend
    // directly in the scheduler/engine; these atomics simply let the editor
    // show exactly what the controller sent without touching RT-owned state.
    [[nodiscard]] int pitchBendRaw14() const noexcept
    {
        return pitchBendRaw14_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] unsigned pitchBendRevision() const noexcept
    {
        return pitchBendRevision_.load(std::memory_order_relaxed);
    }

private:
    static int rawValueFor(const P6PatchState& patch, std::uint16_t nrpn, int fallback = 0);
    static std::pair<int, int> rangeFor(std::uint16_t nrpn);
    MidiRouteResult applyNrpn(P6PatchState& patch,
                              P6PerformanceScheduler& scheduler,
                              const MapperCalibration& calibration,
                              NrpnUpdate update);

    NrpnDecoder nrpn_{};
    std::atomic<int> pitchBendRaw14_{8192};
    std::atomic<unsigned> pitchBendRevision_{};
};

} // namespace p6
