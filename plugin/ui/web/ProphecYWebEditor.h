#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include <memory>
#include <set>
class P6AudioProcessor;
namespace p6plugin { class PresetLibrary; }

// Experimental JUCE 9 WebView editor. Native DSP and host state stay in C++.
class ProphecYWebEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ProphecYWebEditor(P6AudioProcessor&);
    ~ProphecYWebEditor() override;
    void resized() override;
private:
    void timerCallback() override;
    void handleParameter(const juce::var&);
    void handleGesture(const juce::var&);
    void handleResize(const juce::var&);
    void handlePresetNavigate(const juce::var&);
    void handlePresetLoad(const juce::var&);
    void handlePresetFavorite(const juce::var&);
    void emitPresetCatalog();
    juce::WebBrowserComponent::Options makeOptions();
    P6AudioProcessor& processor_;
    juce::WebBrowserComponent browser_;
    std::unique_ptr<p6plugin::PresetLibrary> presetLibrary_;
    juce::String lastPresetId_, lastPresetName_;
    bool ready_{false};
    bool forceFullState_{true};
    std::map<juce::String,float> lastValues_;
    std::set<juce::String> gestures_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProphecYWebEditor)
};
