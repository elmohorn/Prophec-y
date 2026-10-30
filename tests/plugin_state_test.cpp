#include "PluginStateCodec.h"
#include "p6/NrpnCodec.h"
#include "p6/ParameterRegistry.h"
#include "p6/ProgramLayout.h"
#include "p6/SysexCodec.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <unordered_set>

#define P6_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "CHECK failed: " #expr << " at " << __FILE__ << ':' << __LINE__ << '\n'; \
            return 1; \
        } \
    } while (false)

int main()
{
    // Catch accidental std::array over-sizing: a default-initialised trailing
    // SerializedParameterInfo previously created a duplicate NRPN 0 parameter
    // with min == max, which JUCE normalised to NaN and pluginval correctly
    // rejected. Every serialized entry must be real, uniquely addressable, and
    // backed by registry metadata.
    const auto layout = p6::serializedParameterLayout();
    P6_CHECK(layout.size() == 87);
    std::unordered_set<std::uint16_t> seenNrpns;
    std::unordered_set<std::size_t> seenOffsets;
    for (const auto& entry : layout)
    {
        P6_CHECK(!entry.name.empty());
        P6_CHECK(entry.maximum > entry.minimum);
        P6_CHECK(seenNrpns.insert(entry.nrpn).second);
        P6_CHECK(seenOffsets.insert(entry.sysexOffset).second);
        P6_CHECK(p6::findProgramParameter(entry.nrpn) != nullptr);
    }

    p6plugin::WrapperState a;
    a.patch.bank = -1;
    a.patch.program = 499;
    for (std::size_t i = 0; i < a.patch.rawPayload.size(); ++i)
        a.patch.rawPayload[i] = static_cast<std::uint8_t>((i * 73u + 19u) & 0xffu);

    // Make the visible name bytes valid but retain arbitrary data everywhere else.
    constexpr char name[] = "State Roundtrip";
    for (std::size_t i = 0; i < p6::SysexCodec::kPatchNameLength; ++i)
        a.patch.rawPayload[p6::SysexCodec::kPatchNameOffset + i] = i < sizeof(name) - 1
            ? static_cast<std::uint8_t>(name[i]) : static_cast<std::uint8_t>(' ');
    p6::populateRawParametersFromPayload(a.patch);

    a.sequenceControls = { 1, 37, 2, 3 };
    a.extras = { 3, 2, 1, 1, 0, 1, 1, 1 };

    const auto bytes = p6plugin::encodeState(a);
    P6_CHECK(bytes.size() == p6plugin::kEncodedStateBytes);

    p6plugin::WrapperState b;
    P6_CHECK(p6plugin::decodeState(bytes, b));
    P6_CHECK(b.patch.bank == a.patch.bank);
    P6_CHECK(b.patch.program == a.patch.program);
    P6_CHECK(b.patch.rawPayload == a.patch.rawPayload);
    P6_CHECK(b.patch.name == "State Roundtrip");
    P6_CHECK(b.sequenceControls == a.sequenceControls);
    P6_CHECK(b.extras == a.extras);

    auto broken = bytes;
    broken[0] = 'X';
    P6_CHECK(!p6plugin::decodeState(broken, b));
    P6_CHECK(!p6plugin::decodeState(std::span<const std::uint8_t>(bytes.data(), bytes.size() - 1), b));

    std::cout << "p6 wrapper state/layout v0.16 tests passed\n";
    return 0;
}
