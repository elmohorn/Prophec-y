#include "ProphecYWebEditor.h"
#include "../../PluginProcessor.h"
#include "BinaryData.h"
#include "../CharacterFx.h"

#if !JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
 #error "Prophec-Y v3 requires a WebView resource provider; enable JUCE_USE_WIN_WEBVIEW2 on Windows"
#endif
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr std::array<int,30> kKnobNrpn{0,3,4,2,5,8,9,7,45,46,77,49,120,128,143,144,
                                       88,89,78,79,80,81,67,68,69,70,30,33,63,62};
constexpr std::array<int,4> kFaderNrpn{2,7,27,32};
constexpr std::array<int,14> kSwitchNrpn{135,145,146,147,148,149,91,93,94,95,97,156,29,48};
juce::String nrpnId(int id){return "nrpn_"+juce::String(id).paddedLeft('0',4);}
juce::String property(const juce::var& object,const char* name){
    if(auto* o=object.getDynamicObject())return o->getProperty(name).toString();
    return {};
}
} // namespace

juce::WebBrowserComponent::Options ProphecYWebEditor::makeOptions()
{
    auto options=juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withEventListener("prophecyParameter",[this](juce::var v){handleParameter(v);})
        .withEventListener("prophecyGesture",[this](juce::var v){handleGesture(v);})
        .withEventListener("prophecyResize",[this](juce::var v){handleResize(v);})
        .withEventListener("prophecyReady",[this](juce::var){ready_=true;forceFullState_=true;});
#if JUCE_WINDOWS
    options=options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2);
#endif
#if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
    options=options.withResourceProvider([](const juce::String& path)
        ->std::optional<juce::WebBrowserComponent::Resource> {
        if(path!="/"&&path!="/index.html")return std::nullopt;
        const auto* first=reinterpret_cast<const std::byte*>(BinaryData::ProphecY_v3_0_html);
        return juce::WebBrowserComponent::Resource{
            std::vector<std::byte>(first,first+BinaryData::ProphecY_v3_0_htmlSize),"text/html"};
    });
#endif
    return options;
}

ProphecYWebEditor::ProphecYWebEditor(P6AudioProcessor& p)
    :juce::AudioProcessorEditor(p),processor_(p),browser_(makeOptions())
{
    addAndMakeVisible(browser_);
    setSize(1670,865);
    browser_.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz(20);
}

ProphecYWebEditor::~ProphecYWebEditor()
{
    stopTimer();
    for(const auto& id:gestures_)
        if(auto* p=processor_.parameters().getParameter(id))p->endChangeGesture();
}

void ProphecYWebEditor::resized(){browser_.setBounds(getLocalBounds());}

void ProphecYWebEditor::handleParameter(const juce::var& payload)
{
    const auto id=property(payload,"id");
    auto* object=payload.getDynamicObject();
    if(!object||id.isEmpty())return;
    const auto value=object->getProperty("value");
    if(!value.isDouble()&&!value.isInt()&&!value.isInt64())return;
    const float normalized=static_cast<float>(static_cast<double>(value));
    if(!std::isfinite(normalized))return;
    if(auto* p=processor_.parameters().getParameter(id))
        p->setValueNotifyingHost(std::clamp(normalized,0.f,1.f));
}

void ProphecYWebEditor::handleGesture(const juce::var& payload)
{
    const auto id=property(payload,"id"),phase=property(payload,"phase");
    if(auto* p=processor_.parameters().getParameter(id)){
        if(phase=="begin"&&gestures_.insert(id).second)p->beginChangeGesture();
        else if(phase=="end"&&gestures_.erase(id)>0)p->endChangeGesture();
    }
}

void ProphecYWebEditor::handleResize(const juce::var& payload)
{
    if(auto* object=payload.getDynamicObject()){
        const bool shown=static_cast<bool>(object->getProperty("character"));
        const int desiredWidth=shown?1960:1670;
        if(getWidth()!=desiredWidth)setSize(desiredWidth,865);
    }
}

void ProphecYWebEditor::timerCallback()
{
    if(!ready_ || !browser_.isShowing())return;
    auto* object=new juce::DynamicObject;
    bool changed=false;
    auto append=[&](const juce::String& id){
        if(auto* p=processor_.parameters().getParameter(id)){
            const float value=p->getValue();
            auto it=lastValues_.find(id);
            if(forceFullState_||it==lastValues_.end()||std::abs(it->second-value)>0.0001f){
                object->setProperty(juce::Identifier(id),value);
                lastValues_[id]=value;
                changed=true;
            }
        }
    };
    for(int nrpn:kKnobNrpn)append(nrpnId(nrpn));
    for(int nrpn:kFaderNrpn)append(nrpnId(nrpn));
    for(int nrpn:kSwitchNrpn)append(nrpnId(nrpn));
    append("wrapper_master_output_db");
    append("wrapper_arp_hold");
    append("wrapper_arp_beat_sync");
    append(nrpnId(160));
    append(nrpnId(168));
    append("character_bypass");
    append("character_type");
    append("character_custom_type");
    for (const auto* id : p6plugin::kTubeIDs) append(id);
    for (const auto* id : p6plugin::kCustomIDs[1]) append(id);
    forceFullState_=false;
    if(changed)browser_.emitEventIfBrowserIsVisible("prophecyHostState",juce::var(object));
    else delete object;
}
