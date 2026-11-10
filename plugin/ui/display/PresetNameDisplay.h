#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <array>
#include <cstdint>

// A small alphanumeric LED matrix, inspired by the Prophet-6's numeric
// program display. All animation and drawing remain on the message thread.
class PresetNameDisplay final : public juce::Component, public juce::SettableTooltipClient {
public:
    PresetNameDisplay() { setTitle("Current preset"); setDescription("Current preset name; long names scroll across the display."); }

    void setName(const juce::String& name) {
        if (name == name_) return;
        name_ = name;
        rendered_ = name.toUpperCase();
        offset_ = 0.f;
        dwell_ = 45;
        setTooltip(name);
        repaint();
    }
    [[nodiscard]] const juce::String& name() const noexcept { return name_; }
    [[nodiscard]] float scrollOffset() const noexcept { return offset_; }
    void tick() {
        const float overflow = std::max(0.f, rendered_.length() * kAdvance - (getWidth() - 16.f));
        if (overflow <= 0.f || hovering_ || hasKeyboardFocus(false)) return;
        if (dwell_ > 0) { --dwell_; return; }
        if (offset_ < overflow) {
            offset_ = std::min(overflow, offset_ + 35.f / 30.f);
            if (offset_ == overflow) dwell_ = 45;
        } else {
            offset_ = 0.f;
            dwell_ = 45;
        }
        repaint();
    }
    void mouseEnter(const juce::MouseEvent&) override { hovering_ = true; }
    void mouseExit(const juce::MouseEvent&) override { hovering_ = false; }
    void paint(juce::Graphics& g) override {
        g.saveState();
        g.reduceClipRegion(getLocalBounds().reduced(4, 2));
        const float y0 = (getHeight() - 7.f * kPitch) * .5f;
        for (int i = 0; i < rendered_.length(); ++i) {
            const float x0 = 8.f + i * kAdvance - offset_;
            if (x0 > getWidth() || x0 + kAdvance < 0.f) continue;
            const auto rows = glyph(static_cast<char>(rendered_[i]));
            for (int y = 0; y < 7; ++y) for (int x = 0; x < 5; ++x) {
                if (!(rows[static_cast<std::size_t>(y)] & (1u << (4 - x)))) continue;
                const float px = x0 + x * kPitch, py = y0 + y * kPitch;
                g.setColour(juce::Colour{0xffe5382e}.withAlpha(.16f));
                g.fillEllipse(px - 1.6f, py - 1.6f, 5.4f, 5.4f);
                g.setColour(juce::Colour{0xffff7568});
                g.fillRoundedRectangle(px, py, 2.2f, 2.2f, .5f);
            }
        }
        g.restoreState();
    }
private:
    using Rows = std::array<std::uint8_t, 7>;
    static constexpr float kPitch = 3.25f, kAdvance = 6.f * kPitch;
    static Rows glyph(char c) noexcept {
        switch (c) {
        case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {15,16,16,16,16,16,15}; case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {15,16,16,19,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {31,4,4,4,4,4,31}; case 'J': return {7,2,2,2,18,18,12};
        case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,30,1,1,17,14};
        case '6': return {7,8,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,2,28};
        case ' ': return {}; case '-': return {0,0,0,31,0,0,0};
        case '.': return {0,0,0,0,0,12,12}; case '/': return {1,1,2,4,8,16,16};
        case '*': return {0,21,14,31,14,21,0}; case '&': return {12,18,20,8,21,18,13};
        case '+': return {0,4,4,31,4,4,0}; case '_': return {0,0,0,0,0,0,31};
        case '\'': return {4,4,8,0,0,0,0}; case ':': return {0,12,12,0,12,12,0};
        default: return {14,17,1,2,4,0,4};
        }
    }
    juce::String name_, rendered_;
    float offset_{};
    int dwell_{45};
    bool hovering_{};
};
