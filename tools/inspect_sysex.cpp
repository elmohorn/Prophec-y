#include "p6/ProgramLayout.h"
#include "p6/SysexStream.h"

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: p6_inspect <bank-or-program.syx>\n";
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "cannot open input SysEx\n";
        return 3;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    const auto stream = p6::decodeP6SysexStream(bytes);
    std::cout << "Prophet-6 messages: " << stream.entries.size() << "\n";
    std::cout << "Skipped bytes: " << stream.skippedBytes << "\n";
    std::cout << "Errors: " << stream.errors.size() << "\n";
    for (const auto& e : stream.errors)
        std::cout << "  ! " << e << "\n";

    std::set<std::pair<int,int>> locations;
    std::size_t duplicateLocations = 0;
    std::size_t outOfMetadata = 0;
    for (const auto& entry : stream.entries) {
        const auto& p = entry.patch;
        if (entry.kind == p6::SysexKind::ProgramDump) {
            if (!locations.emplace(p.bank, p.program).second)
                ++duplicateLocations;
        }
        for (const auto& raw : p.rawParameters) {
            if (const auto* info = p6::findSerializedByNrpn(raw.id))
                if (raw.raw < info->minimum || raw.raw > info->maximum)
                    ++outOfMetadata;
        }
    }
    std::cout << "Duplicate bank/program locations: " << duplicateLocations << "\n";
    std::cout << "Known parameter bytes outside current metadata: " << outOfMetadata << "\n";

    const std::size_t preview = std::min<std::size_t>(stream.entries.size(), 20);
    for (std::size_t i = 0; i < preview; ++i) {
        const auto& e = stream.entries[i];
        std::cout << "  [" << i << "] @" << e.byteOffset << " ";
        if (e.kind == p6::SysexKind::ProgramDump)
            std::cout << e.patch.bank << "-" << e.patch.program;
        else
            std::cout << "EDIT";
        std::cout << "  " << e.patch.name << "\n";
    }
    if (stream.entries.size() > preview)
        std::cout << "  ... " << (stream.entries.size() - preview) << " more\n";

    return stream.errors.empty() && !stream.entries.empty() ? 0 : 1;
}
