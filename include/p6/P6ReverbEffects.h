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

    struct CachedCoefficients {
        bool valid{};
        Type type{Type::Hall};
        double primary{};
        double secondary{};
        std::array<std::size_t, kLines> delaySamples{};
        std::array<double, kLines> feedbackGain{};
        std::size_t earlyDelaySamples{1};
        double dampingAlpha{};
        double diffusion{};
        double earlyAmount{};
        bool spring{};
    };

    [[nodiscard]] static double readDelay(const Line& line,
                                          std::size_t delaySamples) noexcept;
    static void writeDelay(Line& line, double value) noexcept;
    void updateCachedCoefficients(Type type,
                                  double primary,
                                  double secondary) noexcept;

    double sampleRate_{48000.0};
    std::array<Line, kLines> lines_{};
    std::vector<double> early_{};
    std::size_t earlyWrite_{};
    std::size_t earlyValidSamples_{};
    CachedCoefficients cached_{};
};

} // namespace p6
