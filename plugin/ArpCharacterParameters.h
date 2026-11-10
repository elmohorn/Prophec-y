#pragma once
#include <array>
namespace p6plugin {
inline constexpr const char* kArpCharacterEnabledID = "arp_character_enabled";
inline constexpr const char* kArpCharacterMotionID = "arp_character_motion";
inline constexpr const char* kArpCharacterRhythmID = "arp_character_rhythm";
inline constexpr const char* kArpCharacterFreezeID = "arp_character_freeze";
inline constexpr std::array<const char*,8> kArpStepIDs {
 "arp_step_1","arp_step_2","arp_step_3","arp_step_4",
 "arp_step_5","arp_step_6","arp_step_7","arp_step_8"
};
}
