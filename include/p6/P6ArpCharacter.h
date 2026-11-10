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
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    void setMask(std::uint8_t mask) noexcept { mask_ = mask; }
    void setMotion(double motion) noexcept {
        motion_ = std::isfinite(motion) ? std::clamp(motion, 0.0, 1.0) : 0.0;
    }
    void setFreeze(bool freeze) noexcept { freeze_ = freeze; }
    void reset() noexcept { step_ = 0; rng_ = 0x735a2d97u; fillVariations(); }
    [[nodiscard]] Step next() noexcept {
        const auto index = static_cast<std::uint8_t>(step_ % 8u);
        if (index == 0 && step_ != 0 && !freeze_) fillVariations();
        ++step_;
        if (!enabled_) return {true, 1.0, index};
        return {(mask_ & (1u << index)) != 0,
                1.0 + motion_ * variations_[index] * 0.25, index};
    }
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
        for (auto& value : variations_) {
            rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
            value = static_cast<double>(rng_) / 2147483647.5 - 1.0;
        }
    }
    bool enabled_{false}, freeze_{false};
    std::uint8_t mask_{0xffu};
    double motion_{};
    std::uint32_t rng_{0x735a2d97u}, step_{};
    std::array<double,8> variations_{};
};
} // namespace p6
