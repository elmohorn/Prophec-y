#pragma once
#include "../ContextPreview.h"
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
    const Content& content() const noexcept {return content_;}
    const p6plugin::ui::PreviewCurve& curve() const noexcept {return curve_;}
    void paint(juce::Graphics&) override;
private:
    Content content_;
    p6plugin::ui::PreviewCurve curve_;
};
