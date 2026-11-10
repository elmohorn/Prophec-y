#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace p6 {
// A fixed eight-step performance layer. No memory is allocated on the audio path.
class P6ArpCharacter {
public:
    struct Step { bool play; double velocityScale; std::uint8_t index; };
    P6ArpCharacter() noexcept { reset(); }
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    void setMask(std::uint8_t mask) noexcept { mask_ = mask; }
    void setRhythm(int pulses) noexcept { rhythm_ = std::clamp(pulses, 0, 8); }
    void setMotion(double motion) noexcept {
        motion_ = std::isfinite(motion) ? std::clamp(motion, 0.0, 1.0) : 0.0;
    }
    void setFreeze(bool freeze) noexcept { freeze_ = freeze; }
    void restoreVariationSeed(std::uint32_t seed) noexcept {
        rng_ = seed != 0 ? seed : 0x735a2d97u; fillVariations();
    }
    [[nodiscard]] std::uint32_t variationSeed() const noexcept { return variationSeed_; }
    void reset() noexcept {
        step_ = 0;
        if(!freeze_) { rng_ = 0x735a2d97u; fillVariations(); }
    }
    [[nodiscard]] Step next() noexcept {
        const auto index = static_cast<std::uint8_t>(step_ % 8u);
        if (index == 0 && step_ != 0 && !freeze_) fillVariations();
        ++step_;
        if (!enabled_) return {true, 1.0, index};
        return {((mask_ & rhythmMask(rhythm_)) & (1u << index)) != 0,
                1.0 + motion_ * variations_[index] * 0.25, index};
    }
    // Remainder pattern based on GenerativeMIDI EuclideanEngine (MIT);
    // see third_party/GenerativeMIDI-LICENSE.txt.
    [[nodiscard]] static std::uint8_t rhythmMask(int pulses) noexcept {
        pulses = std::clamp(pulses, 0, 8);
        std::uint8_t mask = 0;
        for (int i = 0; i < 8; ++i)
            if (pulses > 0 && ((i * pulses) % 8) < pulses)
                mask = static_cast<std::uint8_t>(mask | (1u << i));
        return mask;
    }
private:
    void fillVariations() noexcept {
        variationSeed_ = rng_;
        for (auto& value : variations_) {
            rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
            value = static_cast<double>(rng_) / 2147483647.5 - 1.0;
        }
    }
    bool enabled_{false}, freeze_{false};
    std::uint8_t mask_{0xffu};
    int rhythm_{8};
    double motion_{};
    std::uint32_t rng_{0x735a2d97u}, step_{};
    std::uint32_t variationSeed_{0x735a2d97u};
    std::array<double,8> variations_{};
};
} // namespace p6
