#pragma once
#include <array>
#include <vector>

namespace p6plugin::ui {
enum class PreviewKind { Position, Oscillator, Envelope };
struct PreviewSpec {
    PreviewKind kind{PreviewKind::Position};
    // Oscillator: shape, final duty, normalization. Envelope: A,D,S,R,gate hold.
    std::array<double,5> values{};
    bool operator==(const PreviewSpec&) const = default;
};
struct PreviewPoint { double x{},y{}; };
struct PreviewCurve {
    std::vector<PreviewPoint> points;
    double duration{1.},amplitude{1.},attackEnd{-1.},decayEnd{-1.},noteOff{-1.};
    bool valid{},truncated{};
};
inline constexpr double kEnvelopePreviewSampleRate=8000.;
// UI-side deterministic model preview. Never connects to an active voice.
PreviewCurve makeContextPreview(const PreviewSpec&);
} // namespace p6plugin::ui
