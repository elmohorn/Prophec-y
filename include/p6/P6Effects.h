#pragma once

#include "p6/P6ModulationEffects.h"
#include "p6/P6ReverbEffects.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace p6 {

enum class P6EffectType : std::uint8_t {
    Off = 0,
    BbdDelay = 1,
    DigitalDelay = 2,
    Chorus = 3,
    Phaser1 = 4,
    Phaser2 = 5,
    Hall = 6,
    Room = 7,
    Plate = 8,
    Spring = 9,
    Phaser3 = 32,
    RingMod = 33,
    Flanger1 = 34,
    Flanger2 = 35,
    Unknown = 255,
};

struct P6EffectSlotSettings {
    P6EffectType type{P6EffectType::Off};
    double mix{};
    double primary{};
    double secondary{};
};

struct P6EffectsSettings {
    bool enabled{};
    P6EffectSlotSettings a{};
    P6EffectSlotSettings b{};
};

struct P6EffectsStereoSample {
    double left{};
    double right{};
};

class P6Effects {
public:
    void prepare(double sampleRate);
    // Full lifecycle reset; may clear preallocated storage and is intended for
    // prepare/non-realtime use.
    void reset() noexcept;
    // Realtime-safe logical clear for CC120 and type transitions. This is O(1)
    // with respect to delay/reverb buffer length: old ring-buffer samples are
    // marked invalid instead of being bulk-filled on the audio thread.
    void clearRealtimeState() noexcept;
    void setSettings(const P6EffectsSettings& settings) noexcept;

    [[nodiscard]] const P6EffectsSettings& settings() const noexcept { return settings_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    [[nodiscard]] P6EffectsStereoSample process(double left, double right) noexcept;

private:
    struct Slot {
        std::vector<double> delay;
        std::vector<double> chorusLeft;
        std::vector<double> chorusRight;
        std::size_t delayWrite{};
        std::size_t delayValidSamples{};
        std::size_t chorusWrite{};
        std::size_t chorusValidSamples{};
        double loopLowPass{};
        double chorusPhase{};
        double wowPhase{};
        double wowFrom{};
        double wowTo{};
        std::uint32_t rng{0x5a5a1234u};
        P6ModulationEffects modulation{};
        P6ReverbEffects reverb{};

        void prepare(double sampleRate, std::uint32_t seed);
        void reset() noexcept;
        void invalidateRealtimeState() noexcept;
        [[nodiscard]] P6EffectsStereoSample process(double left, double right,
                                                     const P6EffectSlotSettings& settings,
                                                     double sampleRate) noexcept;

    private:
        [[nodiscard]] double randomBipolar() noexcept;
        [[nodiscard]] double wander(double sampleRate) noexcept;
        [[nodiscard]] static double tapLinear(const std::vector<double>& buffer,
                                              std::size_t write,
                                              std::size_t validSamples,
                                              double samplesBack) noexcept;
        [[nodiscard]] static double tapCubic(const std::vector<double>& buffer,
                                             std::size_t write,
                                             std::size_t validSamples,
                                             double samplesBack) noexcept;
    };

    double sampleRate_{48000.0};
    P6EffectsSettings settings_{};
    Slot slotA_{};
    Slot slotB_{};
};

} // namespace p6
