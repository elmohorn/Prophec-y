#include "p6/ProgramLayout.h"
#include "p6/ReferenceIsolation.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>

#ifdef NDEBUG
#error "p6_v019_reference_tests requires assertions to be enabled"
#endif

namespace {

std::uint16_t rawByNrpn(const p6::P6PatchState& patch, std::uint16_t nrpn)
{
    for (const auto& parameter : patch.rawParameters)
        if (parameter.id == nrpn)
            return parameter.raw;
    assert(false && "expected NRPN missing from isolated patch");
    return 0;
}

void expectRaw(const p6::P6PatchState& patch,
               std::uint16_t nrpn,
               std::uint16_t expected)
{
    const auto* info = p6::findSerializedByNrpn(nrpn);
    assert(info != nullptr);
    assert(patch.rawPayload[info->sysexOffset] == expected);
    assert(rawByNrpn(patch, nrpn) == expected);
}

void testDryIsolationPreservesUnknownBytes()
{
    p6::P6PatchState source;
    for (std::size_t i = 0; i < source.rawPayload.size(); ++i)
        source.rawPayload[i] = static_cast<std::uint8_t>((i * 53u + 17u) & 0xffu);
    p6::populateRawParametersFromPayload(source);
    const auto before = source.rawPayload;

    const std::map<std::uint16_t, std::uint16_t> expected{
        {1, 0}, {29, 0}, {33, 0}, {48, 0}, {52, 0}, {63, 0}, {64, 0}, {71, 0},
        {89, 0}, {91, 0}, {93, 0}, {94, 0}, {95, 0}, {96, 0}, {97, 0}, {98, 0},
        {109, 127}, {110, 0}, {111, 0}, {112, 0}, {113, 0}, {114, 0}, {115, 0},
        {119, 0}, {127, 0}, {135, 0},
        {143, 127}, {144, 127}, {145, 0}, {146, 0}, {147, 0}, {148, 0}, {149, 0},
        {156, 0}, {157, 0}, {160, 0}
    };

    const auto isolated = p6::makeReferenceIsolationPatch(source);

    std::set<std::size_t> allowedChangedOffsets;
    for (const auto& [nrpn, raw] : expected) {
        const auto* info = p6::findSerializedByNrpn(nrpn);
        assert(info != nullptr);
        allowedChangedOffsets.insert(info->sysexOffset);
        expectRaw(isolated, nrpn, raw);
    }

    for (std::size_t i = 0; i < before.size(); ++i) {
        if (!allowedChangedOffsets.contains(i))
            assert(isolated.rawPayload[i] == before[i]);
    }

    assert(source.rawPayload == before);
}

void testOptionalSyncGlideAndPolyModPreservation()
{
    p6::P6PatchState source;
    source.rawPayload.fill(0);

    const std::map<std::uint16_t, std::uint16_t> seed{
        {1, 1}, {29, 1}, {143, 22}, {144, 233},
        {145, 1}, {146, 1}, {147, 1}, {148, 1}, {149, 1}
    };
    for (const auto& [nrpn, raw] : seed) {
        const auto* info = p6::findSerializedByNrpn(nrpn);
        assert(info != nullptr);
        source.rawPayload[info->sysexOffset] = static_cast<std::uint8_t>(raw);
    }
    p6::populateRawParametersFromPayload(source);

    p6::ReferenceIsolationOptions options;
    options.disableSync = false;
    options.disableGlide = false;
    options.disablePolyMod = false;
    const auto isolated = p6::makeReferenceIsolationPatch(source, options);

    for (const auto& [nrpn, raw] : seed)
        expectRaw(isolated, nrpn, raw);

    expectRaw(isolated, 33, 0);
    expectRaw(isolated, 64, 0);
    expectRaw(isolated, 135, 0);
    expectRaw(isolated, 156, 0);
    expectRaw(isolated, 157, 0);
}

} // namespace

int main()
{
    testDryIsolationPreservesUnknownBytes();
    testOptionalSyncGlideAndPolyModPreservation();
    std::cout << "v0.19 reference-isolation tests passed\n";
    return 0;
}
