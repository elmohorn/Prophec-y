#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Pure, allocation-free, UI-thread-only (30 Hz) mapping of measured DSP
// telemetry to physical-looking Character camera animation. Never called from
// processBlock; no parameters or musical state are changed.
namespace p6plugin::ui
{
class CharacterVisualDynamics final
{
public:
    struct Frame
    {
        float heater{};                      // tube glow: actual signal + effect change
        std::array<float, 2> springEnergy{}; // two *visible* springs, three DSP taps
        std::array<float, 2> springShift{};  // displacement in design-space pixels
        float lamp{};                        // illuminated optical bulb
        float ldr{};                         // deliberately slower LDR response
    };

    [[nodiscard]] const Frame& frame() const noexcept { return frame_; }

    void reset() noexcept
    {
        frame_ = {};
        phase_ = {};
    }

    // RMS and tapRms are genuine measurements published by the DSP.
    // previewLamp is an *explicit visual design preview* signal; it is not
    // represented as an actual optical effect modulation.
    void advance(bool enabled, bool motion, float inputRms, float changedRms,
                 const std::array<float, 3>& tapRms, float previewLamp,
                 float previewResponse = .5f) noexcept
    {
        if (!enabled) { reset(); return; }
        if (!motion) return; // exact freeze including lamp decay and spring phase

        const float in = std::sqrt(positive(inputRms));
        const float delta = std::sqrt(positive(changedRms));
        const float heat = unit(in * 1.75f + delta * 1.15f);
        frame_.heater = follow(frame_.heater, heat, .46f, .12f);

        // The Spring DSP publishes three RMS delay-line pickups. The photo
        // depicts TWO helical springs; do not paint a fictitious third coil.
        // Map all three taps into two correlated but independently moving
        // bodies, preserving the real late-reverb tail.
        const std::array<float, 3> tap{
            1.f - std::exp(-26.f * positive(tapRms[0])),
            1.f - std::exp(-26.f * positive(tapRms[1])),
            1.f - std::exp(-26.f * positive(tapRms[2]))
        };
        const std::array<float, 2> target{
            unit(.78f * tap[0] + .22f * tap[1]),
            unit(.22f * tap[1] + .78f * tap[2])
        };
        constexpr float twoPi = 6.2831853071795864769f;
        // In the absence of envelope energy there is no vibration. With
        // energy, a damped low-frequency mode yields a plausible sub-pixel
        // shimmer in a 30-fps GUI. It is NOT a sample-accurate visualization
        // of the 44.1/48 kHz coil displacement.
        constexpr std::array<float, 2> hz{4.3f, 5.6f};
        for (std::size_t i = 0; i < 2; ++i)
        {
            frame_.springEnergy[i] =
                follow(frame_.springEnergy[i], target[i], .40f, .105f);
            phase_[i] = std::fmod(phase_[i] + hz[i] * twoPi / 30.f, twoPi);
            frame_.springShift[i] =
                (2.8f + 1.4f * frame_.springEnergy[i]) *
                frame_.springEnergy[i] * std::sin(phase_[i]);
        }

        const float response=unit(previewResponse);
        // User's UI-only RESPONSE knob changes the thermal and photoresistive
        // inertia of the conceptual bulb. No effect audio parameter is saved.
        frame_.lamp = follow(frame_.lamp, unit(previewLamp),
                             .24f+.60f*response, .08f+.34f*response);
        // The photoresistor is deliberately slower than the filament.
        frame_.ldr = follow(frame_.ldr, frame_.lamp,
                            .06f+.26f*response, .025f+.17f*response);
    }

private:
    static float positive(float x) noexcept
    {
        return std::isfinite(x) ? std::max(0.f, x) : 0.f;
    }

    static float unit(float x) noexcept
    {
        return std::clamp(positive(x), 0.f, 1.f);
    }

    static float follow(float oldValue, float target, float attack, float release) noexcept
    {
        return oldValue + (target > oldValue ? attack : release) * (target - oldValue);
    }

    Frame frame_{};
    std::array<float, 2> phase_{};
};
}
