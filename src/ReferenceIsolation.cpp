#include "p6/ReferenceIsolation.h"

#include "p6/ProgramLayout.h"

#include <stdexcept>

namespace p6 {
namespace {

void setVerifiedRaw(P6PatchState& patch, std::uint16_t nrpn, std::uint16_t raw)
{
    const auto* info = findSerializedByNrpn(nrpn);
    if (!info)
        throw std::logic_error("reference isolation uses an unknown NRPN");
    if (raw < info->minimum || raw > info->maximum || raw > 255)
        throw std::logic_error("reference isolation raw value is outside verified serialized range");
    patch.rawPayload[info->sysexOffset] = static_cast<std::uint8_t>(raw);
}

} // namespace

P6PatchState makeReferenceIsolationPatch(const P6PatchState& source,
                                         const ReferenceIsolationOptions& options)
{
    auto out = source;

    // Performance/global paths which can contaminate an otherwise isolated
    // oscillator/filter measurement. Program volume and the VCA envelope are
    // intentionally retained because they are still in the physical dry path
    // on the hardware and must be matched by the paired capture patch.
    if (options.disableSync)
        setVerifiedRaw(out, 1, 0);
    if (options.disableGlide)
        setVerifiedRaw(out, 29, 0);

    setVerifiedRaw(out, 33, 0);  // Slop
    setVerifiedRaw(out, 48, 0);  // LP velocity
    setVerifiedRaw(out, 52, 0);  // HP velocity
    setVerifiedRaw(out, 63, 0);  // Pan Spread
    setVerifiedRaw(out, 64, 0);  // Distortion
    setVerifiedRaw(out, 71, 0);  // Amp velocity

    // Global LFO: zero Initial Amount and every verified destination. The LFO
    // itself may keep free-running internally; it contributes exactly zero to
    // the reference render.
    setVerifiedRaw(out, 89, 0);
    setVerifiedRaw(out, 91, 0);
    for (const auto nrpn : {93u, 94u, 95u, 96u, 97u, 98u})
        setVerifiedRaw(out, static_cast<std::uint16_t>(nrpn), 0);

    // Channel aftertouch is neutral at centre and all known destinations off.
    setVerifiedRaw(out, 109, 127);
    for (const auto nrpn : {110u, 111u, 112u, 113u, 114u, 115u})
        setVerifiedRaw(out, static_cast<std::uint16_t>(nrpn), 0);

    // Serial digital FX are completely disabled. Type bytes are also set Off
    // so a later accidental enable cannot expose stale algorithm selection in
    // a generated reference patch.
    setVerifiedRaw(out, 119, 0);
    setVerifiedRaw(out, 127, 0);
    setVerifiedRaw(out, 135, 0);

    if (options.disablePolyMod) {
        setVerifiedRaw(out, 143, 127);
        setVerifiedRaw(out, 144, 127);
        for (const auto nrpn : {145u, 146u, 147u, 148u, 149u})
            setVerifiedRaw(out, static_cast<std::uint16_t>(nrpn), 0);
    }

    // A reference note is one voice unless the test explicitly exercises
    // stacking; arp/sequencer timing is likewise outside a direct renderer.
    setVerifiedRaw(out, 156, 0);
    setVerifiedRaw(out, 160, 0);

    // rawPayload is canonical. Rebuild the convenience NRPN view so all mapper
    // layers observe the edited bytes while every unknown byte stays intact.
    populateRawParametersFromPayload(out);
    return out;
}

} // namespace p6
