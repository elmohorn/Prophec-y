#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace p6 {

struct P6ModulationStereoSample {
    double left{};
    double right{};
};

// Clean-room modulation-effect candidates used by the Prophet-style FX slots.
// The topology is intentionally replaceable: v0.18 establishes functional,
// realtime-safe behavior; v0.19 hardware captures may recalibrate or replace
// constants without changing canonical program state.
class P6ModulationEffects {
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    [[nodiscard]] P6ModulationStereoSample processPhaser(
        double left, double right, double rateHz, double depth01,
        int variant) noexcept;

    [[nodiscard]] P6ModulationStereoSample processFlanger(
        double left, double right, double rateHz, double depth01,
        bool highFeedback) noexcept;

    [[nodiscard]] P6ModulationStereoSample processRingMod(
        double left, double right, double modulatorHz) noexcept;

private:
    static constexpr std::size_t kPhaserStages = 6;

    struct PhaserChannel {
        std::array<double, kPhaserStages> state{};
        double feedback{};
    };

    [[nodiscard]] double phaserChannel(double input, PhaserChannel& state,
                                       double lfo, double depth01,
                                       int variant) noexcept;
    [[nodiscard]] static double tapLinear(const std::vector<double>& buffer,
                                          std::size_t write,
                                          double samplesBack) noexcept;

    double sampleRate_{48000.0};
    double phaserPhase_{};
    double flangerPhase_{};
    double ringPhase_{};
    PhaserChannel phaserLeft_{};
    PhaserChannel phaserRight_{};

    std::vector<double> flangerLeft_{};
    std::vector<double> flangerRight_{};
    std::size_t flangerWrite_{};
    double flangerFeedbackLeft_{};
    double flangerFeedbackRight_{};
};

} // namespace p6
