#pragma once
#include "p6/P6Engine.h"
#include <array>
#include <cstddef>
#include <memory>

namespace p6plugin {
// Stable host IDs belong to the device, never to carousel position.
inline constexpr std::array<const char*, 7> kTubeIDs {
    "character_tube_drive", "character_tube_sag", "character_tube_bass",
    "character_tube_middle", "character_tube_treble", "character_tube_mix", "character_tube_output" };
inline constexpr std::array<const char*, 7> kTubeNames {
    "Drive", "Sag", "Bass", "Middle", "Treble", "Mix", "Output" };
inline constexpr std::array<float, 7> kTubeDefaults { .25f, .3f, .5f, .5f, .5f, 1.f, .5f };
inline constexpr const char* kCharacterTypeID = "character_type";
inline constexpr const char* kCharacterPositionID = "character_position";
inline constexpr const char* kCharacterBypassID = "character_bypass";

struct CharacterState {
    int type{}; // 0 None; 1 Tube. Future types append, never reorder.
    int position{1}; // 0 start; 1 end
    bool bypass{};
    std::array<float, 7> tube{kTubeDefaults};
};

// Setup on the host prepare thread; all subsequent calls belong to the stream
// owner. The one tube is retained, not allocated when the carousel changes.
class CharacterFx {
public:
    CharacterFx();
    ~CharacterFx();
    void prepare(double sampleRate);
    void setState(const CharacterState&) noexcept;
    p6::StereoSample process(p6::StereoSample) noexcept;
    // Route changes fade the complete output to silence, swap/reset at zero,
    // prime latency with zeros, then fade in. Call once per rendered sample.
    bool advanceRoute() noexcept;
    bool beforeEffects() const noexcept;
    float routeGain() const noexcept;
    int latency() const noexcept;
    float excitation() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace p6plugin
