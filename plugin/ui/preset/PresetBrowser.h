#pragma once
#include "PluginProcessor.h"
#include "PresetLibrary.h"

class PresetBrowser final : public juce::Component,private juce::ListBoxModel {
public:
    explicit PresetBrowser(P6AudioProcessor&,juce::File root=p6plugin::PresetLibrary::defaultRoot());
    ~PresetBrowser() override {closing_=true;chooser_.reset();setLookAndFeel(nullptr);}
    void paint(juce::Graphics&) override;
    void resized() override;
    bool refreshCurrent();
    void refreshLibrary();
    std::function<void()> onClose;
private:
    int getNumRows() override{return static_cast<int>(rows_.size());}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void selectedRowsChanged(int) override;
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override {loadSelected();}
    void returnKeyPressed(int) override {loadSelected();}
    void rebuildRows();
    const p6plugin::PresetEntry* selected() const;
    void loadSelected();
    void guardReplace(std::function<void()>);
    void save(bool asNew);
    void saveAsDialog();
    void importFile();
    void exportFile(bool bank);
    void report(juce::Result,const juce::String& success);
    P6AudioProcessor& processor_;
    p6plugin::PresetLibrary library_;
    juce::LookAndFeel_V4 look_;
    juce::TextEditor search_;
    juce::ComboBox source_,bank_,category_,sort_;
    juce::ToggleButton favorites_{"Favorites only"};
    juce::Label title_,status_,current_;
    juce::TextButton load_{"LOAD"},save_{"SAVE"},saveAs_{"SAVE AS"},favorite_{"FAVORITE"},import_{"IMPORT .SYX"},exportProgram_{"EXPORT PROGRAM"},exportBank_{"EXPORT BANK"},refresh_{"REFRESH"},close_{"CLOSE"};
    juce::ListBox list_{"Preset list",this};
    std::unique_ptr<juce::FileChooser> chooser_;
    std::vector<juce::String> rows_,banks_;
    bool closing_{};
};
