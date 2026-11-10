#include "OutputMeter.h"
#include <algorithm>
#include <cmath>

namespace {
float db(float value) { return 20.f*std::log10(std::max(value,.000001f)); }
juce::String level(float value) { return value<.000001f ? juce::String("-inf") : juce::String(db(value),1); }
}
void OutputMeter::update(p6plugin::OutputReadings readings)
{
    readings_=readings;
    juce::String detail;
    for(std::size_t channel=0;channel<2;++channel) {
        detail += channel==0 ? "L" : "\nR";
        detail += " peak " + level(readings_.peak[channel]) + " dBFS, RMS "
               + level(readings_.rms[channel]) + " dBFS";
    }
    detail += (readings_.clips & 12u) ? "\nPre-master exceeded 0 dBFS" : "\nPre-master below 0 dBFS";
    setTooltip(detail);setDescription(detail);repaint();
}
void OutputMeter::paint(juce::Graphics& g)
{
    g.setFont(9.f);g.setColour(juce::Colour{0xffa39f9a});
    g.drawText("OUTPUT",18,0,getWidth()-18,15,juce::Justification::centredLeft);
    constexpr int segments=24;
    const float railWidth=static_cast<float>(getWidth()-22);
    const float step=railWidth/static_cast<float>(segments);
    for(std::size_t channel=0;channel<2;++channel) {
        const float y=24.f+static_cast<float>(channel)*20.f;
        g.setFont(10.f);g.setColour(juce::Colour{0xffb7b4b0});
        g.drawText(channel==0?"L":"R",0,static_cast<int>(y)-3,14,14,juce::Justification::centredLeft);
        const auto position=[](float value){return std::clamp((db(value)+60.f)/60.f,0.f,1.f);};
        const float rms=position(readings_.rms[channel]);
        const int peak=static_cast<int>(std::ceil(position(readings_.peak[channel])*segments))-1;
        for(int index=0;index<segments;++index) {
            const bool lit=static_cast<float>(index)<rms*segments || (index==peak&&peak>=0);
            const auto colour=index>=22?juce::Colour{0xffdd4d3f}
                             :index>=18?juce::Colour{0xffbc985f}:juce::Colour{0xffada89c};
            g.setColour(lit?colour:juce::Colour{0xff343536});
            g.fillRect(18.f+static_cast<float>(index)*step,y,std::max(1.f,step-1.2f),7.f);
        }
    }
    if(readings_.clips!=0) {
        g.setColour(juce::Colour{0xffef6c59});g.setFont(9.f);
        g.drawText((readings_.clips&3u)?"CLIP":"PRE > 0",18,59,getWidth()-18,12,juce::Justification::centredRight);
    }
}
