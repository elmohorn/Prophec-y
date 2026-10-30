#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace p6 {

struct P6ReverbStereoSample {
    double left{};
    double right{};
};

class P6ReverbEffects {
public:
    enum class Type { Hall, Room, Plate, Spring };

    void prepare(double sampleRate);
    void reset() noexcept;
    // Realtime-safe logical clear. Old samples remain allocated but are ignored
    // until overwritten after the invalidation point.
    void invalidateRealtimeState() noexcept;

    [[nodiscard]] P6ReverbStereoSample process(double left, double right,
                                                Type type,
                                                double primary01,
                                                double secondary01) noexcept;

private:
    static constexpr std::size_t kLines = 4;

    struct Line {
        std::vector<double> buffer{};
        std::size_t write{};
        std::size_t validSamples{};
        double dampingState{};
    };

    [[nodiscard]] static double readDelay(const Line& line,
                                          std::size_t delaySamples) noexcept;
    static void writeDelay(Line& line, double value) noexcept;

    double sampleRate_{48000.0};
    std::array<Line, kLines> lines_{};
    std::vector<double> early_{};
    std::size_t earlyWrite_{};
    std::size_t earlyValidSamples_{};
};

} // namespace p6
