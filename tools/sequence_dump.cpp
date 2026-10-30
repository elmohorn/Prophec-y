#include "p6/P6Sequence.h"
#include "p6/SysexStream.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
std::vector<std::uint8_t> readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: p6_sequence_dump <file.syx> [message-index]\n";
        return 2;
    }

    const auto bytes = readFile(argv[1]);
    if (bytes.empty()) {
        std::cerr << "unable to read input\n";
        return 1;
    }
    const auto stream = p6::decodeP6SysexStream(bytes);
    if (stream.entries.empty()) {
        std::cerr << "no Prophet-6 program/edit-buffer messages found\n";
        return 1;
    }

    std::size_t index = 0;
    if (argc == 3) {
        try { index = static_cast<std::size_t>(std::stoul(argv[2])); }
        catch (...) { std::cerr << "invalid message-index\n"; return 2; }
    }
    if (index >= stream.entries.size()) {
        std::cerr << "message-index out of range; file contains " << stream.entries.size() << " P6 messages\n";
        return 2;
    }

    const auto& entry = stream.entries[index];
    const auto sequence = p6::decodeSequence(entry.patch);
    std::cout << "message " << index;
    if (entry.patch.bank >= 0)
        std::cout << "  bank " << entry.patch.bank << " program " << std::setw(2) << std::setfill('0') << entry.patch.program << std::setfill(' ');
    std::cout << "  name: " << entry.patch.name << "\n";
    std::cout << "sequence bytes: " << p6::P6Sequence::kPayloadOffset << ".."
              << (p6::P6Sequence::kPayloadOffset + p6::P6Sequence::kPayloadSize - 1) << "\n";
    std::cout << "populated sequence cells: " << sequence.populatedCellCount()
              << ", nonzero low-7 velocity cells: " << sequence.velocityEventCount()
              << ", velocity bit7 cells: " << sequence.highBitCount() << "\n\n";

    for (std::size_t step = 0; step < p6::P6Sequence::kSteps; ++step) {
        bool any = false;
        for (std::size_t track = 0; track < p6::P6Sequence::kTracks; ++track)
            any |= sequence.tracks[track][step].isPopulated();
        if (!any)
            continue;

        std::cout << "step " << std::setw(2) << (step + 1) << ':';
        for (std::size_t track = 0; track < p6::P6Sequence::kTracks; ++track) {
            const auto& c = sequence.tracks[track][step];
            std::cout << "  T" << (track + 1) << "[n=" << std::setw(3) << unsigned(c.noteRaw)
                      << " v=" << std::setw(3) << unsigned(c.velocity)
                      << " rawV=" << std::setw(3) << unsigned(c.velocityRaw)
                      << " b7=" << (c.highBitFlag ? '1' : '0') << ']';
        }
        std::cout << '\n';
    }

    std::cout << "\nNote: bit 7 of the stored velocity byte is preserved as an opaque flag. "
                 "Its musical meaning is not assumed by this diagnostic tool.\n";
    return 0;
}
