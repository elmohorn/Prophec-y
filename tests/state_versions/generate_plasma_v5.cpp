// Build with the unchanged codec v5 headers and encoder, not the v6 codec.
#include "PluginStateCodec.h"
#include <fstream>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    p6plugin::WrapperState state;
    state.character.type = 2;
    state.character.custom[0][5] = 0.8f;
    state.midiInputChannel = 7;
    const auto bytes = p6plugin::encodeState(state);
    std::ofstream out(argv[1], std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out ? 0 : 1;
}
