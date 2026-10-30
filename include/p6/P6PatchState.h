#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace p6 {

// Canonical state is intentionally hardware-oriented. DSP/UI layers map from this
// representation instead of becoming the source of truth.
struct RawParameter {
    std::uint16_t id{};          // Sequential/NRPN identifier when known
    std::uint16_t raw{};         // native integer value, lossless
    float normalized{};          // 0..1 convenience value for hosts/UI
    bool implemented{};          // false => preserve value even if DSP ignores it
};

enum class WaveShape : std::uint8_t {
    Triangle,
    TriangleToSaw,
    Saw,
    SawToPulse,
    Pulse
};

struct OscillatorState {
    float coarseSemitones{};
    float fineCents{};
    float shape{};               // canonical continuous 0..1 shape position
    float pulseWidth{0.5f};
    float level{};
    bool sync{};
    bool keyTrack{true};
};

struct EnvelopeState {
    float attack{};              // normalized raw-domain placeholder until calibration
    float decay{};
    float sustain{};
    float release{};
    float amount{};
};

struct FilterState {
    float cutoff{};              // normalized raw-domain placeholder until calibrated mapping
    float resonance{};
    float keyTrack{};
    EnvelopeState env{};
};

struct PolyModState {
    float filterEnvSource{};
    float osc2Source{};
    float osc1FreqDest{};
    float osc1ShapeDest{};
    float osc1PulseWidthDest{};
    float lowPassDest{};
    float highPassDest{};
};

struct LfoState {
    float frequency{};
    float amount{};
    float shape{};
};

struct PerformanceState {
    float slop{};
    float vintage{};
    bool unison{};
    std::uint8_t unisonVoices{1};
    float panSpread{};
};

struct P6PatchState {
    std::string name;
    int bank{-1};                // -1 for edit-buffer dump
    int program{-1};             // -1 for edit-buffer dump

    OscillatorState osc1{};
    OscillatorState osc2{};
    float subLevel{};
    float noiseLevel{};
    float mixerDrive{};

    FilterState highPass{};
    FilterState lowPass{};
    EnvelopeState ampEnv{};
    PolyModState polyMod{};
    LfoState lfo{};
    PerformanceState performance{};

    // Always preserve the exact unpacked Prophet-6 payload, even before all
    // parameter offsets are understood. This makes imports forward-compatible.
    std::array<std::uint8_t, 1024> rawPayload{};
    std::vector<RawParameter> rawParameters;
};

} // namespace p6
