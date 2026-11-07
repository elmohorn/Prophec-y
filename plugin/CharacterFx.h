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

inline constexpr int kCharacterTypes=6;
inline constexpr const char* kCustomTypeID="character_custom_type";
inline constexpr std::array<const char*,6> kCharacterNames{"None","Tube","Plasma","Optical Spring","Ferrofluid","Neon Wavefolder"};
inline constexpr std::array<std::array<const char*,7>,4> kCustomIDs{{
    {"character_plasma_drive","character_plasma_ignition","character_plasma_sustain","character_plasma_character","character_plasma_tone","character_plasma_mix","character_plasma_output"},
    {"character_spring_drive","character_spring_decay","character_spring_tension","character_spring_pickup","character_spring_damping","character_spring_mix","character_spring_output"},
    {"character_ferro_cutoff","character_ferro_resonance","character_ferro_sensitivity","character_ferro_response","character_ferro_polarity","character_ferro_mix","character_ferro_output"},
    {"character_neon_drive","character_neon_folds","character_neon_symmetry","character_neon_bias","character_neon_tone","character_neon_mix","character_neon_output"}
}};
inline constexpr std::array<std::array<const char*,7>,4> kCustomNames{{
    {"Drive","Ignition","Sustain","Character","Tone","Mix","Output"},
    {"Drive","Decay","Tension","Pickup","Damping","Mix","Output"},
    {"Cutoff","Resonance","Sensitivity","Response","Polarity","Mix","Output"},
    {"Drive","Folds","Symmetry","Bias","Tone","Mix","Output"}
}};
inline constexpr std::array<std::array<float,7>,4> kCustomDefaults{{
    {.35f,.25f,.4f,.35f,.65f,1.f,.5f},
    {.5f,.5f,.5f,.5f,.5f,.35f,.5f},
    {.55f,.3f,.5f,.4f,1.f,1.f,.5f},
    {.3f,.3f,.5f,.5f,.65f,1.f,.5f}
}};
struct DeviceTelemetry {
    std::array<float,8> values{};
    std::array<float,32> input{},wet{};
};

struct CharacterState {
    int type{}; // None, Tube, Plasma, Optical Spring, Ferrofluid, Neon.
    int legacyType{}; // underlying None/Tube automation while a custom device is selected
    int position{1}; // 0 start; 1 end
    bool bypass{};
    std::array<float, 7> tube{kTubeDefaults};
    std::array<std::array<float,7>,4> custom{kCustomDefaults};
};

// Audio-thread snapshot. Levels are linear RMS, not dB or a THD estimate.
// Change compares the mixed output with the latency-aligned dry waveform.
struct CharacterTelemetry {
    float inputRms{}, outputRms{}, changeRms{};
    bool active{};
    int type{};
    DeviceTelemetry device{};
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
    CharacterTelemetry telemetry() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace p6plugin

