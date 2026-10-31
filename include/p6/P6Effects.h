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
    bool tempoSync{};
    std::uint8_t syncDivision{5};
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

[[nodiscard]] double syncedEffectDelaySeconds(std::uint8_t division,
                                              double bpm) noexcept;

class P6Effects {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void clearRealtimeState() noexcept;
    void setSettings(const P6EffectsSettings& settings) noexcept;
    void setRuntimeBpm(double bpm) noexcept;

    [[nodiscard]] const P6EffectsSettings& settings() const noexcept { return settings_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] double runtimeBpm() const noexcept { return runtimeBpm_; }

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

        // Five-millisecond one-pole smoothing is deliberately an implementation
        // safety policy rather than a claim about Sequential firmware. The
        // first value after prepare/type-reset snaps exactly to its target;
        // subsequent automation on the same topology is smoothed.
        double smoothingCoefficient_{1.0};
        bool smoothingInitialised_{};
        double smoothedMix_{};
        double smoothedPrimary_{};
        double smoothedSecondary_{};

        void prepare(double sampleRate, std::uint32_t seed);
        void reset() noexcept;
        void invalidateRealtimeState() noexcept;
        [[nodiscard]] P6EffectsStereoSample process(double left, double right,
                                                     const P6EffectSlotSettings& settings,
                                                     double sampleRate,
                                                     double runtimeBpm) noexcept;

    private:
        void updateSmoothedSettings(double targetMix,
                                    double targetPrimary,
                                    double targetSecondary) noexcept;
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
    double runtimeBpm_{120.0};
    P6EffectsSettings settings_{};
    Slot slotA_{};
    Slot slotB_{};
};

} // namespace p6
