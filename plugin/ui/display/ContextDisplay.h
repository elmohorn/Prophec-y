#pragma once
#include "../ContextPreview.h"
#include "LiveSpectrum.h"
#include <juce_gui_basics/juce_gui_basics.h>

class ContextDisplay final : public juce::Component {
public:
    struct Content {
        juce::String preset,title,value,caption;
        p6plugin::ui::PreviewSpec spec;
        double position{};
        bool pitchBend{};
        int pitchRaw{8192};
        bool operator==(const Content&) const = default;
    };
    ContextDisplay();
    void setContent(Content);
    void setLiveFrame(const p6plugin::ui::LiveAudioFrame&);
    void clearLiveFrame();
    void tick();
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    const Content& content() const noexcept {return content_;}
    const p6plugin::ui::PreviewCurve& curve() const noexcept {return curve_;}
    void paint(juce::Graphics&) override;
private:
    void advanceView();
    Content content_;
    p6plugin::ui::PreviewCurve curve_;
    p6plugin::ui::LiveAudioFrame liveFrame_{};
    std::array<float,p6plugin::ui::kSpectrumBins> liveSpectrum_{};
    enum class View { Spectrum, Waveform, Context };
    View view_{View::Spectrum};
    bool haveLiveFrame_{};
    int contextTicks_{};
};
