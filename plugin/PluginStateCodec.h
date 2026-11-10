#pragma once

#include "p6/P6PatchState.h"
#include "CharacterFx.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace p6plugin {

struct WrapperState {
    p6::P6PatchState patch;
    std::array<std::uint16_t, 4> sequenceControls{}; // NRPN 168..171
    std::array<std::uint8_t, 8> extras{};            // wrapper/runtime controls
    float masterOutputDb{-18.f};                 // global output; absent legacy states use unity
    std::uint8_t midiInputChannel{}; // 0 Omni, 1..16 single MIDI channel
    bool arpCharacterEnabled{};
    bool arpCharacterFreeze{};
    std::uint8_t arpCharacterMask{255};
    std::uint8_t arpCharacterRhythm{8};
    float arpCharacterMotion{};
    std::uint32_t arpVariationSeed{0x735a2d97u};
    CharacterState character{};                   // never part of Prophet SysEx
};

inline constexpr std::array<std::uint8_t, 4> kStateMagic { 'P', '6', 'V', '1' };
inline constexpr std::uint16_t kStateVersion = 7;
inline constexpr std::size_t kV1StateBytes = 4 + 2 + 2 + 2 + 1024 + 8 + 8;
inline constexpr std::size_t kV2StateBytes = kV1StateBytes + 3 + 14;
inline constexpr std::size_t kV3StateBytes = kV2StateBytes + 2;
inline constexpr std::size_t kV4StateBytes = kV3StateBytes + 57;
inline constexpr std::size_t kV5StateBytes = kV4StateBytes + 1;
inline constexpr std::size_t kV6StateBytes = kV5StateBytes + 1;
inline constexpr std::size_t kEncodedStateBytes = kV6StateBytes + 10;
inline constexpr const char* kMidiInputChannelID = "wrapper_midi_input_channel";

std::vector<std::uint8_t> encodeState(const WrapperState& state);
bool decodeState(std::span<const std::uint8_t> bytes, WrapperState& state);

} // namespace p6plugin

