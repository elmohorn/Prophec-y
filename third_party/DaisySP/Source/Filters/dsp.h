#pragma once
// Minimal adapter for the two symbols used by the unmodified DaisySP SVF.
// Avoids importing unrelated DSP utilities. Prophec-y authors, 2026.
#include <algorithm>
namespace daisysp {
inline constexpr float PI_F=3.14159265358979323846f;
inline float fclamp(float x,float lo,float hi){return std::clamp(x,lo,hi);}
}
