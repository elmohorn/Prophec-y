#include <cmath>
#include <iostream>
namespace juce {bool approximatelyEqual(float a,float b){return std::abs(a-b)<1e-6f;}}
struct WTOscillator {struct Params {float formant{},bend{},asym{},fold{},position{};};};
struct WavetableComponent {WTOscillator::Params params;bool needsUpdate{};int repaints{};void repaint(){++repaints;}void setParams(WTOscillator::Params);};
void WavetableComponent::setParams (WTOscillator::Params params_)
{
    if (! juce::approximatelyEqual (params.formant, params_.formant) || 
        ! juce::approximatelyEqual (params.bend, params_.bend)       ||
        ! juce::approximatelyEqual (params.asym, params_.asym)       ||
        ! juce::approximatelyEqual (params.fold, params_.fold) )
    {
        params = params_;
        needsUpdate = true;
        repaint();
    }
    else if (! juce::approximatelyEqual (params.position, params_.position))
    {
        params = params_;
        repaint();
    }
}

int main(){WavetableComponent w;WTOscillator::Params p;p.position=.5f;w.setParams(p);if(w.repaints!=1||w.needsUpdate)return 1;p.fold=.4f;w.setParams(p);if(w.repaints!=2||!w.needsUpdate)return 2;w.needsUpdate=false;w.setParams(p);if(w.repaints!=2||w.needsUpdate)return 3;std::cout<<"upstream FigBug/Gin setParams: position repaints, fold invalidates, unchanged ignored\n";}

