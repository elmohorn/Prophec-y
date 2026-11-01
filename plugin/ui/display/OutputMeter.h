#pragma once
#include "OutputStage.h"
#include <juce_gui_basics/juce_gui_basics.h>
class OutputMeter final : public juce::Component {
public:
    OutputMeter(){setTitle("Output L/R meter");setInterceptsMouseClicks(false,false);}
    void update(p6plugin::OutputReadings r){readings_=r;repaint();}
    void paint(juce::Graphics&) override;
private:
    p6plugin::OutputReadings readings_{};
};
