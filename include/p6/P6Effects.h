#pragma once

#include "p6/P6ModulationEffects.h"
#include "p6/P6ReverbEffects.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace p6 {

// The first ten values intentionally match the verified legacy Prophet-6
// program/NRPN domains already used by Prophec-y. Later-OS effects use
// internal-only identifiers outside that hardware range until their actual
// serialized/MIDI identities are established independently. Unknown hardware
// values must never be coerced into a different known effect.
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

    // Internal extension identifiers only; not valid serialized values yet.
    Phaser3 = 32,
    RingMod = 33,
    Flanger1 = 34,
    Flanger2 = 35,

    Unknown = 255,
};

// DSP-domain settings. The effect mapper is responsible for converting
// verified hardware raw values to these physical/normalised values.
//
// BbdDelay / DigitalDelay: primary=time seconds, secondary=feedback 0..1
// Chorus / Phaser / Flanger: primary=rate Hz, secondary=depth 0..1
// Hall / Room / Plate: primary=time 0..1, secondary=early reflections 0..1
// Spring: primary=decay 0..1, secondary=tone 0..1
// RingMod: primary=modulator frequency Hz; low-note tracking remains a separate
// evidence-gated runtime concern until the later-OS selector encoding is known.
struct P6EffectSlotSettings {
    P6EffectType type{P6EffectType::Off};
    double mix{};       // 0=dry, 1=wet
    double primary{};
    double secondary{};
};

struct P6EffectsSettings {
    bool enabled{};     // Prophet EFFECTS ON/OFF true-bypass control
    P6EffectSlotSettings a{};
    P6EffectSlotSettings b{};
};

struct P6EffectsStereoSample {
    double left{};
    double right{};
};

// JUCE-independent, allocation-free-after-prepare stereo FX chain.
//
// Signal order is always A -> B. prepare() owns every allocation; setSettings()
// and process() never resize their storage. Unknown types pass through dry while
// the canonical P6PatchState continues to preserve the raw selector losslessly.
class P6Effects {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
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
        std::size_t chorusWrite{};
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
        [[nodiscard]] P6EffectsStereoSample process(double left, double right,
                                                     const P6EffectSlotSettings& settings,
                                                     double sampleRate) noexcept;

    private:
        [[nodiscard]] double randomBipolar() noexcept;
        [[nodiscard]] double wander(double sampleRate) noexcept;
        [[nodiscard]] static double tapLinear(const std::vector<double>& buffer,
                                              std::size_t write,
                                              double samplesBack) noexcept;
        [[nodiscard]] static double tapCubic(const std::vector<double>& buffer,
                                             std::size_t write,
                                             double samplesBack) noexcept;
    };

    double sampleRate_{48000.0};
    P6EffectsSettings settings_{};
    Slot slotA_{};
    Slot slotB_{};
};

} // namespace p6
