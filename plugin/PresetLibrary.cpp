#include "PresetLibrary.h"
#include "p6/SysexCodec.h"
#include <algorithm>

namespace p6plugin {
namespace {
struct LibraryAccess {
    juce::InterProcessLock lock;
    bool acquired;
    explicit LibraryAccess(const juce::File& root):lock("Prophec-y-presets-"+juce::String::toHexString(root.getFullPathName().hashCode64())),acquired(lock.enter(50)){}
    ~LibraryAccess(){if(acquired)lock.exit();}
};
juce::String sourceName(PresetSource source){return source==PresetSource::Imported?"Imported":source==PresetSource::Factory?"Factory":"User";}
bool safeId(const juce::String& id){return id.isNotEmpty()&&id.length()<80&&id.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-");}
}
PresetLibrary::PresetLibrary(juce::File root,WrapperState factory):root_(std::move(root)),factory_(std::move(factory)){PresetEntry init;init.id="factory-init";init.name="Init P6 Core";init.bankKey="factory";init.bankLabel="Factory / Init";init.category="Init";init.source=PresetSource::Factory;init.state=factory_;entries_.push_back(init);scan();}
juce::File PresetLibrary::defaultRoot(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Prophec-y/Presets/v1");}
const PresetEntry* PresetLibrary::find(const juce::String& id) const{for(const auto& p:entries_)if(p.id==id)return &p;return nullptr;}
juce::Result PresetLibrary::scan()
{
    LibraryAccess access(root_);if(!access.acquired)return juce::Result::fail("Library busy in another instance; refresh shortly");
    return scanUnlocked();
}
juce::Result PresetLibrary::scanUnlocked()
{
    if(root_.exists()&&!root_.isDirectory())return juce::Result::fail("Preset library path is not a directory");
    entries_.clear();PresetEntry init;init.id="factory-init";init.name="Init P6 Core";init.category="Init";init.bankKey="factory";init.bankLabel="Factory / Init";init.source=PresetSource::Factory;init.state=factory_;entries_.push_back(init);
    int skipped=0;
    for(const auto& file:root_.findChildFiles(juce::File::findFiles,false,"*.p6preset")) {
        if(file.getSize()>128*1024){++skipped;continue;}
        const auto json=juce::JSON::parse(file.loadFileAsString());auto* object=json.getDynamicObject();
        if(!object||static_cast<int>(object->getProperty("version"))!=1){++skipped;continue;}
        const auto id=object->getProperty("id").toString();
        if(!safeId(id)||id!=file.getFileNameWithoutExtension()){++skipped;continue;}
        if(id=="factory-init"){entries_[0].favorite=static_cast<bool>(object->getProperty("favorite"));continue;}
        PresetEntry entry;entry.id=id;entry.name=object->getProperty("name").toString();entry.category=object->getProperty("category").toString();
        const auto source=object->getProperty("source").toString();if(source!="Imported"&&source!="User"){++skipped;continue;}
        entry.source=source=="Imported"?PresetSource::Imported:PresetSource::User;
        entry.bankKey=object->getProperty("bankKey").toString();entry.bankLabel=object->getProperty("bankLabel").toString();
        entry.importOrder=static_cast<int>(object->getProperty("importOrder"));entry.bank=static_cast<int>(object->getProperty("bank"));entry.program=static_cast<int>(object->getProperty("program"));entry.favorite=static_cast<bool>(object->getProperty("favorite"));
        juce::MemoryBlock data,original;
        if(!data.fromBase64Encoding(object->getProperty("state").toString())||!decodeState({static_cast<const std::uint8_t*>(data.getData()),data.getSize()},entry.state)||entry.name.isEmpty()||entry.bank < -1||entry.bank>9||entry.program < -1||entry.program>99){++skipped;continue;}
        if(!original.fromBase64Encoding(object->getProperty("original").toString())){++skipped;continue;}
        if(entry.source==PresetSource::Imported) {
            const auto* start=static_cast<const std::uint8_t*>(original.getData());std::vector<ImportedProgram> verified;std::string error;
            if(!decodePresetBank({start,original.getSize()},verified,error)||verified.size()!=1||verified[0].state.patch.rawPayload!=entry.state.patch.rawPayload||verified[0].state.patch.bank!=entry.bank||verified[0].state.patch.program!=entry.program){++skipped;continue;}
            entry.original.assign(start,start+original.getSize());
        } else if(original.getSize()){++skipped;continue;}
        entries_.push_back(std::move(entry));
    }
    std::stable_sort(entries_.begin()+1,entries_.end(),[](const auto& a,const auto& b){if(a.bankLabel!=b.bankLabel)return a.bankLabel.compareNatural(b.bankLabel)<0;if(a.program!=b.program)return a.program<b.program;return a.name.compareNatural(b.name)<0;});
    return skipped?juce::Result::fail(juce::String(skipped)+" unreadable preset file(s) skipped"):juce::Result::ok();
}
juce::Result PresetLibrary::write(const PresetEntry& p) const
{
    if(!safeId(p.id))return juce::Result::fail("Invalid preset identifier");
    if(auto result=root_.createDirectory();result.failed())return result;
    auto* object=new juce::DynamicObject;juce::var json(object);
    object->setProperty("version",1);object->setProperty("id",p.id);object->setProperty("name",p.name);object->setProperty("category",p.category);object->setProperty("source",sourceName(p.source));object->setProperty("bankKey",p.bankKey);object->setProperty("bankLabel",p.bankLabel);object->setProperty("bank",p.bank);object->setProperty("program",p.program);object->setProperty("favorite",p.favorite);object->setProperty("importOrder",p.importOrder);
    const auto state=musicalPresetBytes(p.state);
    object->setProperty("state",juce::MemoryBlock(state.data(),state.size()).toBase64Encoding());object->setProperty("original",juce::MemoryBlock(p.original.data(),p.original.size()).toBase64Encoding());
    juce::TemporaryFile temp(root_.getChildFile(p.id+".p6preset"));
    if(!temp.getFile().replaceWithText(juce::JSON::toString(json))||!temp.overwriteTargetFileWithTemporary())return juce::Result::fail("Could not atomically save preset file");
    return juce::Result::ok();
}
juce::Result PresetLibrary::importSysex(std::span<const std::uint8_t> bytes,const juce::String& filename,int& imported)
{
    imported=0;LibraryAccess access(root_);if(!access.acquired)return juce::Result::fail("Library busy; retry import");
    std::vector<ImportedProgram> programs;std::string error;
    if(!decodePresetBank(bytes,programs,error))return juce::Result::fail(error);
    std::vector<PresetEntry> staged;const auto batch=juce::Uuid().toString();
    int ordinal=0;for(auto& program:programs) {
        PresetEntry p;p.importOrder=ordinal++;p.id=juce::Uuid().toString();p.source=PresetSource::Imported;p.state=std::move(program.state);p.original=std::move(program.original);
        p.name=juce::String(p.state.patch.name);if(p.name.isEmpty())p.name="Unnamed Prophet program";
        p.bank=p.state.patch.bank;p.program=p.state.patch.program;p.bankKey=batch+"-"+juce::String(p.bank+1);p.bankLabel="Imported / "+filename+" / "+(p.bank>=0?"Bank "+juce::String(p.bank):"Edit buffers");staged.push_back(std::move(p));
    }
    std::vector<juce::File> written;
    for(const auto& p:staged) {
        const auto result=write(p);if(result.failed()){for(const auto& f:written)f.deleteFile();return result;}
        written.push_back(root_.getChildFile(p.id+".p6preset"));
    }
    imported=static_cast<int>(staged.size());scanUnlocked();return juce::Result::ok();
}
juce::Result PresetLibrary::saveUser(juce::String existingId,WrapperState state,juce::String name,juce::String category,int bank,int program,juce::String& savedId)
{
    std::vector<std::uint8_t> expected;
    if(const auto* old=find(existingId))expected=musicalPresetBytes(old->state);
    LibraryAccess access(root_);if(!access.acquired)return juce::Result::fail("Library busy; retry save");scanUnlocked();
    if(const auto* fresh=find(existingId);fresh&&!expected.empty()&&expected!=musicalPresetBytes(fresh->state))return juce::Result::fail("Preset changed in another instance; refresh or use Save As");
    if(name.trim().isEmpty()||name.length()>80||category.length()>80||bank<0||bank>9||program<0||program>99)return juce::Result::fail("Use a name (1-80 characters), bank 0-9 and program 0-99");
    const auto* existing=find(existingId);
    if(existingId.isNotEmpty()&&(!existing||existing->source!=PresetSource::User))return juce::Result::fail("Use Save As for Factory or Imported presets");
    for(const auto& p:entries_)if(p.source==PresetSource::User&&p.bank==bank&&p.program==program&&p.id!=existingId)return juce::Result::fail("That User bank/program slot is occupied; choose another slot");
    PresetEntry p;if(existing)p=*existing;
    p.id=existing?existingId:juce::Uuid().toString();p.name=name.trim();p.category=category.trim();p.bank=bank;p.program=program;p.bankKey="user-"+juce::String(bank);p.bankLabel="User / Bank "+juce::String(bank);p.source=PresetSource::User;p.state=std::move(state);p.original.clear();
    // User bank addresses are metadata; unknown payload and hardware name stay intact.
    const auto result=write(p);if(result.failed())return result;savedId=p.id;scanUnlocked();return juce::Result::ok();
}
juce::Result PresetLibrary::setFavorite(juce::String id,bool favorite)
{
    LibraryAccess access(root_);if(!access.acquired)return juce::Result::fail("Library busy; retry favorite");scanUnlocked();
    const auto* found=find(id);if(!found)return juce::Result::fail("Preset no longer exists; refresh the library");
    auto p=*found;p.favorite=favorite;const auto result=write(p);if(result.wasOk())scanUnlocked();return result;
}
std::vector<std::uint8_t> PresetLibrary::exportProgram(const PresetEntry& p)
{
    if(!p.original.empty())return p.original;
    return p.bank>=0&&p.program>=0?p6::SysexCodec::encodeProgramDump(p.state.patch,static_cast<std::uint8_t>(p.bank),static_cast<std::uint8_t>(p.program)):p6::SysexCodec::encodeEditBuffer(p.state.patch);
}
std::vector<std::uint8_t> PresetLibrary::exportBank(const juce::String& key) const
{
    std::vector<std::uint8_t> bytes;
    std::vector<const PresetEntry*> bank;
    for(const auto& p:entries_)if(p.bankKey==key)bank.push_back(&p);
    std::stable_sort(bank.begin(),bank.end(),[](const auto* a,const auto* b){if(a->source!=b->source)return static_cast<int>(a->source)<static_cast<int>(b->source);return a->source==PresetSource::Imported?a->importOrder<b->importOrder:a->program<b->program;});
    for(const auto* p:bank){const auto frame=exportProgram(*p);bytes.insert(bytes.end(),frame.begin(),frame.end());}
    return bytes;
}
} // namespace p6plugin
