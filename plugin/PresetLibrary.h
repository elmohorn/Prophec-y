#pragma once
#include "PresetBankCodec.h"
#include <juce_core/juce_core.h>

namespace p6plugin {
enum class PresetSource { Factory, Imported, User };
struct PresetEntry {
    juce::String id,name,category,bankKey,bankLabel;
    PresetSource source{PresetSource::User};
    int bank{-1},program{-1},importOrder{};
    bool favorite{};
    WrapperState state;
    std::vector<std::uint8_t> original;
};
class PresetLibrary {
public:
    PresetLibrary(juce::File root,WrapperState factory);
    static juce::File defaultRoot();
    juce::Result scan();
    const std::vector<PresetEntry>& entries() const noexcept{return entries_;}
    const PresetEntry* find(const juce::String& id) const;
    juce::Result importSysex(std::span<const std::uint8_t>,const juce::String& filename,int& imported);
    juce::Result saveUser(juce::String existingId,WrapperState,juce::String name,juce::String category,int bank,int program,juce::String& savedId);
    juce::Result setFavorite(juce::String id,bool);
    std::vector<std::uint8_t> exportBank(const juce::String& bankKey) const;
    static std::vector<std::uint8_t> exportProgram(const PresetEntry&);
private:
    juce::Result scanUnlocked();
    juce::Result write(const PresetEntry&) const;
    juce::File root_;
    WrapperState factory_;
    std::vector<PresetEntry> entries_;
};
} // namespace p6plugin
