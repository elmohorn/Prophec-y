#include "ContextDisplay.h"
#include <algorithm>
#include <cmath>
#include <utility>

ContextDisplay::ContextDisplay()
{
    setTitle("Context display");setDescription("Selected parameter and labelled model preview. Not a live oscilloscope.");
    setInterceptsMouseClicks(false,false);
}
void ContextDisplay::setContent(Content next)
{
    if(next==content_)return;
    if(next.spec!=content_.spec)curve_=p6plugin::ui::makeContextPreview(next.spec);
    content_=std::move(next);repaint();
}
void ContextDisplay::paint(juce::Graphics& g)
{
    auto r=getLocalBounds().toFloat().reduced(2);
    const juce::Colour ink{0xffcce0e4},dim{0xff78959e},line{0xff36515b},accent{0xffe6aa69};
    g.setColour(juce::Colour{0xff8b948b});g.fillRoundedRectangle(r.expanded(2),6);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff142127},0,0,juce::Colour{0xff070d10},r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r,4);
    g.setColour(ink);g.setFont(15.f);g.drawText(content_.preset,16,8,getWidth()-110,23,juce::Justification::centredLeft);
    g.setColour(dim);g.setFont(8.f);g.drawText("POLY / 6",getWidth()-85,11,66,16,juce::Justification::centredRight);
    g.setColour(line);g.drawLine(16,35.f,static_cast<float>(getWidth()-16),35.f,1.f);
    g.setColour(ink);g.setFont(10.f);g.drawText(content_.title,16,38,getWidth()-130,17,juce::Justification::centredLeft);
    g.setColour(accent);g.setFont(12.f);g.drawText(content_.value,getWidth()-125,38,109,17,juce::Justification::centredRight);
    const juce::Rectangle<float> plot(17,58,static_cast<float>(getWidth()-34),static_cast<float>(std::max(20,getHeight()-80)));
    g.setColour(line.withAlpha(.5f));
    for(int i=0;i<=4;++i){const float x=plot.getX()+plot.getWidth()*static_cast<float>(i)/4.f;g.drawLine(x,plot.getY(),x,plot.getBottom(),.6f);}
    g.drawLine(plot.getX(),plot.getBottom(),plot.getRight(),plot.getBottom(),.7f);
    if(content_.pitchBend) {
        const float x=plot.getCentreX()+static_cast<float>(content_.position)*plot.getWidth()*.48f;
        g.setColour(dim);g.drawLine(plot.getX(),plot.getCentreY(),plot.getRight(),plot.getCentreY(),1.f);
        g.setColour(accent);g.fillEllipse(x-3,plot.getCentreY()-3,6,6);
    } else if(curve_.valid&&content_.spec.kind!=p6plugin::ui::PreviewKind::Position) {
        const bool envelope=content_.spec.kind==p6plugin::ui::PreviewKind::Envelope;
        auto point=[&](const p6plugin::ui::PreviewPoint& p){return juce::Point<float>{plot.getX()+static_cast<float>(p.x/curve_.duration)*plot.getWidth(),envelope?plot.getBottom()-static_cast<float>(p.y)*plot.getHeight():plot.getCentreY()-static_cast<float>(p.y/curve_.amplitude)*plot.getHeight()*.48f};};
        if(!envelope){g.setColour(line);g.drawLine(plot.getX(),plot.getCentreY(),plot.getRight(),plot.getCentreY(),.7f);}
        juce::Path trace;bool first=true;
        for(const auto& p:curve_.points){const auto xy=point(p);if(first){trace.startNewSubPath(xy);first=false;}else trace.lineTo(xy);}
        if(envelope) {
            auto fill=trace;fill.lineTo(plot.getRight(),plot.getBottom());fill.lineTo(plot.getX(),plot.getBottom());fill.closeSubPath();
            g.setGradientFill(juce::ColourGradient(ink.withAlpha(.13f),0,plot.getY(),ink.withAlpha(0.f),0,plot.getBottom(),false));g.fillPath(fill);
        }
        g.setColour(envelope?ink:juce::Colour{0xffb6e3db});g.strokePath(trace,juce::PathStrokeType(1.8f));
        if(envelope&&curve_.noteOff<curve_.duration){const float x=plot.getX()+static_cast<float>(curve_.noteOff/curve_.duration)*plot.getWidth();g.setColour(accent.withAlpha(.7f));g.drawLine(x,plot.getY(),x,plot.getBottom(),.8f);}
    } else {
        // A position bar cannot be mistaken for a filter transfer function.
        g.setColour(line);g.fillRoundedRectangle(plot.getX(),plot.getCentreY()-2,plot.getWidth(),4,2);
        g.setColour(ink);g.fillRoundedRectangle(plot.getX(),plot.getCentreY()-2,plot.getWidth()*static_cast<float>(std::clamp(content_.position,0.,1.)),4,2);
    }
    g.setColour(dim);g.setFont(8.f);
    g.drawText(curve_.truncated?"TRUNCATED / "+content_.caption:content_.caption,16,getHeight()-18,getWidth()-32,15,juce::Justification::centredLeft);
}
