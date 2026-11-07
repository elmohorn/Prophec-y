#include "PresetBrowser.h"
#include "p6/SysexCodec.h"

PresetBrowser::PresetBrowser(P6AudioProcessor& processor,juce::File root):processor_(processor),library_(std::move(root),P6AudioProcessor::factoryInitPreset())
{
    setLookAndFeel(&look_);
    look_.setColour(juce::TextButton::buttonColourId,juce::Colour{0xff46584a});look_.setColour(juce::TextButton::textColourOffId,juce::Colour{0xffe7eadf});
    setTitle("Preset browser");setOpaque(true);setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    title_.setText("PRESETS / LOCAL LIBRARY",juce::dontSendNotification);title_.setFont(juce::Font(17.f).boldened());
    search_.setTextToShowWhenEmpty("Search name, category, bank or program",juce::Colour{0xff8d9b94});search_.setTitle("Search presets");
    source_.addItemList(juce::StringArray{"All sources","Factory","Imported","User"},1);source_.setSelectedId(1);source_.setTitle("Preset source");
    bank_.setTitle("Preset bank");category_.setTitle("Preset category");
    for(auto* component:std::array<juce::Component*,18>{&title_,&current_,&status_,&search_,&source_,&bank_,&category_,&favorites_,&list_,&load_,&save_,&saveAs_,&favorite_,&import_,&exportProgram_,&exportBank_,&refresh_,&close_})addAndMakeVisible(component);
    for(auto* label:{&title_,&current_,&status_})label->setColour(juce::Label::textColourId,juce::Colour{0xffe3e7de});
    for(auto* box:{&source_,&bank_,&category_}){box->setColour(juce::ComboBox::backgroundColourId,juce::Colour{0xff34413c});box->setColour(juce::ComboBox::textColourId,juce::Colour{0xffe3e7de});}
    search_.setColour(juce::TextEditor::backgroundColourId,juce::Colour{0xff202b27});search_.setColour(juce::TextEditor::textColourId,juce::Colour{0xffe3e7de});
    favorites_.setColour(juce::ToggleButton::textColourId,juce::Colour{0xffe3e7de});
    list_.setColour(juce::ListBox::backgroundColourId,juce::Colour{0xff202b27});list_.setRowHeight(36);list_.setOutlineThickness(1);
    search_.onTextChange=[this]{rebuildRows();};source_.onChange=[this]{rebuildRows();};bank_.onChange=[this]{rebuildRows();};category_.onChange=[this]{rebuildRows();};favorites_.onClick=[this]{rebuildRows();};
    load_.onClick=[this]{loadSelected();};save_.onClick=[this]{save(false);};saveAs_.onClick=[this]{save(true);};
    favorite_.onClick=[this]{if(const auto* p=selected()){const auto id=p->id;const auto value=!p->favorite;report(library_.setFavorite(id,value),"Favorite updated");rebuildRows();}};
    import_.onClick=[this]{importFile();};exportProgram_.onClick=[this]{exportFile(false);};exportBank_.onClick=[this]{exportFile(true);};refresh_.onClick=[this]{refreshLibrary();};close_.onClick=[this]{if(onClose)onClose();};
    load_.setTooltip("Load the selected preset. Unsaved edits require confirmation.");
    save_.setTooltip("Overwrite the active User preset; Factory/Imported use Save As.");
    exportProgram_.setTooltip("Export the CURRENT Prophet patch, including current edits; Character and globals are not SysEx.");
    exportBank_.setTooltip("Export SAVED entries in the selected bank as concatenated SysEx dumps. Current unsaved edits are not included.");
    refreshLibrary();
}
void PresetBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour{0xff29352f});g.setColour(juce::Colour{0xff8b9589});g.drawRect(getLocalBounds(),2);
    g.setFont(10.f);g.setColour(juce::Colour{0xffacb7ac});const int columns=getWidth()-36;
    g.drawText("NAME",26,112,columns*36/100-12,18,juce::Justification::centredLeft);
    g.drawText("SOURCE / BANK",18+columns*36/100,112,columns*36/100,18,juce::Justification::centredLeft);
    g.drawText("PROGRAM",18+columns*72/100,112,columns*9/100,18,juce::Justification::centredLeft);
    g.drawText("CATEGORY",18+columns*82/100,112,columns*18/100-8,18,juce::Justification::centredLeft);
}
void PresetBrowser::resized()
{
    const int width=getWidth();title_.setBounds(18,10,330,26);close_.setBounds(width-105,12,85,24);
    current_.setBounds(18,39,width-36,22);search_.setBounds(18,72,width-640,28);source_.setBounds(width-610,72,130,28);bank_.setBounds(width-470,72,195,28);category_.setBounds(width-265,72,120,28);favorites_.setBounds(width-140,72,125,28);
    list_.setBounds(18,132,width-36,getHeight()-215);
    const int y=getHeight()-70;int x=18;
    for(auto* button:{&load_,&save_,&saveAs_,&favorite_,&import_,&exportProgram_,&exportBank_,&refresh_}){const int w=button==&exportProgram_?145:button==&exportBank_?125:105;button->setBounds(x,y,w,26);x+=w+8;}
    status_.setBounds(18,getHeight()-37,width-36,25);
}
void PresetBrowser::report(juce::Result result,const juce::String& success){status_.setText(result.wasOk()?success:result.getErrorMessage(),juce::dontSendNotification);}
bool PresetBrowser::refreshCurrent()
{
    const bool dirty=processor_.presetDirty();current_.setText("CURRENT: "+processor_.activePresetName()+(dirty?" *   / UNSAVED":"   / saved baseline"),juce::dontSendNotification);return dirty;
}
void PresetBrowser::refreshLibrary()
{
    const auto result=library_.scan();const auto oldBank=bank_.getText(),oldCategory=category_.getText();banks_.clear();bank_.clear(juce::dontSendNotification);category_.clear(juce::dontSendNotification);
    bank_.addItem("All banks",1);category_.addItem("All categories",1);juce::StringArray categories;
    for(const auto& p:library_.entries()) {
        if(std::find(banks_.begin(),banks_.end(),p.bankKey)==banks_.end()){banks_.push_back(p.bankKey);bank_.addItem(p.bankLabel,static_cast<int>(banks_.size())+1);}
        if(p.category.isNotEmpty())categories.addIfNotAlreadyThere(p.category);
    }
    categories.sortNatural();category_.addItemList(categories,2);bank_.setSelectedId(1,juce::dontSendNotification);category_.setSelectedId(1,juce::dontSendNotification);
    for(int i=0;i<bank_.getNumItems();++i)if(bank_.getItemText(i)==oldBank)bank_.setSelectedId(bank_.getItemId(i),juce::dontSendNotification);
    for(int i=0;i<category_.getNumItems();++i)if(category_.getItemText(i)==oldCategory)category_.setSelectedId(category_.getItemId(i),juce::dontSendNotification);
    rebuildRows();refreshCurrent();report(result,"Factory includes our Init only. Import your .syx programs/banks, or save a User preset.");
}
void PresetBrowser::rebuildRows()
{
    const auto* old=selected();const auto oldId=old?old->id:juce::String{};rows_.clear();const auto query=search_.getText().trim();
    for(const auto& p:library_.entries()) {
        const int source=p.source==p6plugin::PresetSource::Factory?2:p.source==p6plugin::PresetSource::Imported?3:4;
        if(source_.getSelectedId()>1&&source_.getSelectedId()!=source)continue;
        if(favorites_.getToggleState()&&!p.favorite)continue;
        const int bankIndex=bank_.getSelectedId()-2;if(bankIndex>=0&&bankIndex<static_cast<int>(banks_.size())&&p.bankKey!=banks_[static_cast<std::size_t>(bankIndex)])continue;
        if(category_.getSelectedId()>1&&p.category!=category_.getText())continue;
        const auto haystack=p.name+" "+p.category+" "+p.bankLabel+" "+(p.program>=0?juce::String(p.bank*100+p.program).paddedLeft('0',3):juce::String{});
        if(query.isNotEmpty()&&!haystack.containsIgnoreCase(query))continue;rows_.push_back(p.id);
    }
    title_.setText("PRESETS / LOCAL LIBRARY  ("+juce::String(rows_.size())+")",juce::dontSendNotification);
    list_.deselectAllRows();list_.updateContent();for(std::size_t i=0;i<rows_.size();++i)if(rows_[i]==oldId)list_.selectRow(static_cast<int>(i));
    if(list_.getSelectedRow()<0)for(std::size_t i=0;i<rows_.size();++i)if(rows_[i]==processor_.activePresetId())list_.selectRow(static_cast<int>(i));
    selectedRowsChanged(list_.getSelectedRow());list_.repaint();
}
const p6plugin::PresetEntry* PresetBrowser::selected() const {const int row=list_.getSelectedRow();return row>=0&&row<static_cast<int>(rows_.size())?library_.find(rows_[static_cast<std::size_t>(row)]):nullptr;}
void PresetBrowser::selectedRowsChanged(int){const auto* p=selected();load_.setEnabled(p!=nullptr);favorite_.setEnabled(p!=nullptr);favorite_.setButtonText(p&&p->favorite?"UNFAVORITE":"FAVORITE");exportBank_.setEnabled(p!=nullptr);}
void PresetBrowser::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selectedRow)
{
    if(row<0||row>=static_cast<int>(rows_.size()))return;const auto* p=library_.find(rows_[static_cast<std::size_t>(row)]);if(!p)return;
    g.fillAll(juce::Colour{selectedRow?0xff4a5b4c:row%2?0xff26342d:0xff202b27});g.setFont(13.f);g.setColour(juce::Colour{0xffe5e7dd});
    g.drawText((p->favorite?juce::String::fromUTF8("\xe2\x98\x85 "):juce::String("  "))+p->name,8,0,width*36/100-12,height,juce::Justification::centredLeft);
    g.setColour(juce::Colour{0xffbac4b7});g.drawText(p->bankLabel,width*36/100,0,width*36/100-8,height,juce::Justification::centredLeft);
    g.drawText(p->program>=0?juce::String(p->bank*100+p->program).paddedLeft('0',3):"--",width*72/100,0,width*9/100,height,juce::Justification::centredLeft);
    g.drawText(p->category,width*82/100,0,width*18/100-8,height,juce::Justification::centredLeft);
}
void PresetBrowser::guardReplace(std::function<void()> action)
{
    if(!processor_.presetDirty()){action();return;}
    const juce::Component::SafePointer<PresetBrowser> safe(this);
    juce::AlertWindow::showAsync(juce::MessageBoxOptions{}.withIconType(juce::MessageBoxIconType::WarningIcon).withTitle("Unsaved preset edits").withMessage("The current patch has unsaved edits. Cancel to save them first, or discard them and load the selected preset.").withButton("Discard and load").withButton("Cancel"),[safe,action=std::move(action)](int result){if(safe&&result==1)action();});
}
void PresetBrowser::loadSelected()
{
    const auto* found=selected();if(!found)return;const auto entry=*found;const juce::Component::SafePointer<PresetBrowser> safe(this);
    guardReplace([safe,entry]{if(!safe)return;safe->processor_.loadMusicalPreset(entry.state,entry.id,entry.name);safe->refreshCurrent();safe->status_.setText("Loaded "+entry.name+" / globals retained",juce::dontSendNotification);});
}
void PresetBrowser::save(bool asNew)
{
    const auto* active=library_.find(processor_.activePresetId());
    if(asNew||!active||active->source!=p6plugin::PresetSource::User){saveAsDialog();return;}
    const auto id=active->id,name=active->name;juce::String saved;
    const auto snapshot=processor_.presetSnapshot();const auto result=library_.saveUser(id,snapshot,name,active->category,active->bank,active->program,saved);
    if(result.wasOk())processor_.adoptPreset(saved,name,&snapshot);report(result,"Saved "+name);rebuildRows();refreshCurrent();
}
void PresetBrowser::saveAsDialog()
{
    auto* dialog=new juce::AlertWindow("Save User preset","Name/category are library metadata; the original Prophet payload/name stays intact.",juce::MessageBoxIconType::NoIcon);
    int program=0;for(;program<100;++program){bool occupied=false;for(const auto& p:library_.entries())if(p.source==p6plugin::PresetSource::User&&p.bank==0&&p.program==program)occupied=true;if(!occupied)break;}
    dialog->addTextEditor("name",processor_.activePresetName(),"Name");dialog->addTextEditor("category",{},"Category");dialog->addTextEditor("bank","0","User bank (0-9)");dialog->addTextEditor("program",juce::String(std::min(program,99)),"Program (0-99)");
    dialog->addButton("Save",1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<PresetBrowser> safe(this);const juce::Component::SafePointer<juce::AlertWindow> window(dialog);
    dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,window](int result){if(!safe||!window||result!=1)return;
        const auto name=window->getTextEditorContents("name"),category=window->getTextEditorContents("category"),bank=window->getTextEditorContents("bank").trim(),programText=window->getTextEditorContents("program").trim();
        if(bank.isEmpty()||programText.isEmpty()||!bank.containsOnly("0123456789")||!programText.containsOnly("0123456789")){safe->report(juce::Result::fail("Bank/program must be whole numbers"),{});return;}
        const auto snapshot=safe->processor_.presetSnapshot();juce::String saved;const auto savedResult=safe->library_.saveUser({},snapshot,name,category,bank.getIntValue(),programText.getIntValue(),saved);
        if(savedResult.wasOk())safe->processor_.adoptPreset(saved,name.trim(),&snapshot);safe->refreshLibrary();safe->report(savedResult,"Saved "+name);
    }),true);
}
void PresetBrowser::importFile()
{
    chooser_=std::make_unique<juce::FileChooser>("Import Prophet-6 program or bank",juce::File{},"*.syx");const juce::Component::SafePointer<PresetBrowser> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const juce::FileChooser& chooser){if(!safe||safe->closing_)return;const auto file=chooser.getResult();if(!file.existsAsFile())return;
        if(file.getSize()>16*1024*1024){safe->report(juce::Result::fail("SysEx file exceeds 16 MiB"),{});return;}
        juce::MemoryBlock bytes;if(!file.loadFileAsData(bytes)){safe->report(juce::Result::fail("Could not read SysEx file"),{});return;}
        int count=0;const auto result=safe->library_.importSysex({static_cast<const std::uint8_t*>(bytes.getData()),bytes.getSize()},file.getFileNameWithoutExtension(),count);safe->refreshLibrary();safe->report(result,"Imported "+juce::String(count)+" program(s); current sound retained");
    });
}
void PresetBrowser::exportFile(bool bank)
{
    std::vector<std::uint8_t> bytes;juce::String title;
    if(bank){const auto* p=selected();if(!p)return;bytes=library_.exportBank(p->bankKey);title="Export "+p->bankLabel+" (saved entries; current edits excluded)";}
    else {
        const auto state=processor_.presetSnapshot();const auto* active=library_.find(processor_.activePresetId());
        if(active&&active->state.patch.rawPayload==state.patch.rawPayload)bytes=p6plugin::PresetLibrary::exportProgram(*active);
        else {p6plugin::PresetEntry current;current.state=state;current.bank=active?active->bank:state.patch.bank;current.program=active?active->program:state.patch.program;bytes=p6plugin::PresetLibrary::exportProgram(current);}title="Export current Prophet program (.syx)";
    }
    if(bytes.empty()){report(juce::Result::fail("No programs to export"),{});return;}
    chooser_=std::make_unique<juce::FileChooser>(title,juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(bank?"prophec-y-bank.syx":"prophec-y-program.syx"),"*.syx");const juce::Component::SafePointer<PresetBrowser> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe,bytes=std::move(bytes)](const juce::FileChooser& chooser){if(!safe||safe->closing_)return;auto file=chooser.getResult();if(file==juce::File{})return;juce::TemporaryFile temp(file);
        const bool ok=temp.getFile().replaceWithData(bytes.data(),bytes.size())&&temp.overwriteTargetFileWithTemporary();safe->report(ok?juce::Result::ok():juce::Result::fail("Could not export SysEx file"),"Exported "+file.getFileName());
    });
}
