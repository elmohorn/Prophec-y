#include "ProphecyParameterControl.h"
#include <utility>

void ProphecyParameterControl::configure(juce::RangedAudioParameter& parameter,int nrpn,ContextProvider context)
{
    parameter_=&parameter;nrpn_=nrpn;context_=std::move(context);
    setWantsKeyboardFocus(true);setMouseClickGrabsKeyboardFocus(true);
    setDescription(p6plugin::ui::parameterHelp(nrpn_));
    setTooltip(getTitle()+" — "+juce::String(p6plugin::ui::parameterHelp(nrpn_)));
    textFromValueFunction=[this](double raw){return juce::String(p6plugin::ui::formatParameterValue(nrpn_,raw,context_()));};
    valueFromTextFunction=[this](const juce::String& text){
        const auto context=context_();
        const auto unit=p6plugin::ui::parameterValue(nrpn_,getValue(),context).unit;
        const auto raw=p6plugin::ui::parseParameterValue(nrpn_,text.toStdString(),context,
            unit==p6plugin::ui::ValueUnit::Raw||unit==p6plugin::ui::ValueUnit::Morph);
        return raw.value_or(getValue());
    };
    setDoubleClickReturnValue(true,parameter.convertFrom0to1(parameter.getDefaultValue()));
}
void ProphecyParameterControl::applyRaw(double raw)
{
    if(!parameter_)return;
    // SliderAttachment publishes the value; explicitly bracket non-drag edits
    // so menus, reset and keyboard operations are valid host gestures too.
    parameter_->beginChangeGesture();setValue(raw,juce::sendNotificationSync);parameter_->endChangeGesture();
}
bool ProphecyParameterControl::applyExactText(const juce::String& text,bool rawInput)
{
    if(!parameter_)return false;
    const auto raw=p6plugin::ui::parseParameterValue(nrpn_,text.toStdString(),context_(),rawInput);
    if(!raw)return false;
    applyRaw(*raw);return true;
}
void ProphecyParameterControl::resetToInit()
{
    if(parameter_)applyRaw(parameter_->convertFrom0to1(parameter_->getDefaultValue()));
}
void ProphecyParameterControl::mouseDown(const juce::MouseEvent& event)
{
    if(onSelected)onSelected();
    if(parameter_&&event.mods.isPopupMenu()){showContextMenu();return;}
    if(parameter_)setMouseDragSensitivity(event.mods.isShiftDown()?1500:250);
    juce::Slider::mouseDown(event);
}
bool ProphecyParameterControl::keyPressed(const juce::KeyPress& key)
{
    if(!parameter_)return juce::Slider::keyPressed(key);
    const int code=key.getKeyCode();
    if(code==juce::KeyPress::F10Key&&key.getModifiers().isShiftDown()){showContextMenu();return true;}
    if(code==juce::KeyPress::returnKey){showExactEditor(false);return true;}
    if(code==juce::KeyPress::homeKey){resetToInit();return true;}
    if(code==juce::KeyPress::leftKey||code==juce::KeyPress::downKey){applyRaw(getValue()-1.);return true;}
    if(code==juce::KeyPress::rightKey||code==juce::KeyPress::upKey){applyRaw(getValue()+1.);return true;}
    return juce::Slider::keyPressed(key);
}
void ProphecyParameterControl::paint(juce::Graphics& g)
{
    juce::Slider::paint(g);
    if(parameter_&&hasKeyboardFocus(false)) {
        g.setColour(juce::Colour{0xffd9853c});g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),4.f,1.3f);
    }
}
void ProphecyParameterControl::showContextMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader(getTitle());
    menu.addItem(1,"Edit exact value…");menu.addItem(2,"Edit raw Prophet value…");
    menu.addItem(3,"Reset to Init");menu.addSeparator();
    menu.addItem("NRPN "+juce::String(nrpn_)+" / RAW "+juce::String(getValue(),0),false,false,{});
    menu.addItem(4,"Help");
    const juce::Component::SafePointer<ProphecyParameterControl> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this),[safe](int item){
        if(!safe)return;
        if(item==1||item==2)safe->showExactEditor(item==2);
        else if(item==3)safe->resetToInit();
        else if(item==4)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,safe->getTitle(),p6plugin::ui::parameterHelp(safe->nrpn_));
    });
}
void ProphecyParameterControl::showExactEditor(bool rawInput)
{
    const auto unit=p6plugin::ui::parameterValue(nrpn_,getValue(),context_()).unit;
    rawInput=rawInput||unit==p6plugin::ui::ValueUnit::Raw||unit==p6plugin::ui::ValueUnit::Morph;
    const auto hint=rawInput?"Enter an integer raw Prophet value (NRPN "+juce::String(nrpn_)+").":
        "Use the displayed unit (%, ms/s, Hz, st or cent). Values snap to the nearest hardware step under the active profile.";
    auto* window=new juce::AlertWindow("Edit "+getTitle(),hint,juce::MessageBoxIconType::NoIcon);
    window->addTextEditor("value",rawInput?juce::String(getValue(),0):getTextFromValue(getValue()),rawInput?"Raw value":"Value");
    window->addButton("Apply",1,juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<ProphecyParameterControl> safe(this);
    window->enterModalState(true,juce::ModalCallbackFunction::create([safe,window,rawInput](int result){
        if(result==1&&safe&&!safe->applyExactText(window->getTextEditorContents("value"),rawInput))
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Invalid value","Enter a finite value inside the parameter range. No value was changed.");
    }),true);
}

void ProphecyParameterControl::focusGained(juce::Component::FocusChangeType)
{
    if(onSelected)onSelected();
    repaint();
}
