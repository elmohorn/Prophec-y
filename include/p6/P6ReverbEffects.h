#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace p6 {

struct P6ReverbStereoSample {
    double left{};
    double right{};
};

// Small stereo FDN-style reverb family. v0.18 uses one preallocated network
// with type-specific delay/decay/damping voicings so Hall/Room/Plate/Spring are
// all audible and realtime safe. Constants remain calibration candidates.
class P6ReverbEffects {
public:
    enum class Type { Hall, Room, Plate, Spring };

    void prepare(double sampleRate);
    void reset() noexcept;

    // primary01: Hall/Room/Plate time, Spring decay
    // secondary01: Hall/Room/Plate early-reflection amount, Spring tone
    [[nodiscard]] P6ReverbStereoSample process(double left, double right,
                                                Type type,
                                                double primary01,
                                                double secondary01) noexcept;

private:
    static constexpr std::size_t kLines = 4;

    struct Line {
        std::vector<double> buffer{};
        std::size_t write{};
        double dampingState{};
    };

    [[nodiscard]] static double readDelay(const Line& line,
                                          std::size_t delaySamples) noexcept;
    static void writeDelay(Line& line, double value) noexcept;

    double sampleRate_{48000.0};
    std::array<Line, kLines> lines_{};
    std::vector<double> early_{};
    std::size_t earlyWrite_{};
};

} // namespace p6
