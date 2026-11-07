#include "p6/NrpnCodec.h"
#include "p6/P6Sequence.h"
#include "p6/ParameterRegistry.h"
#include "p6/ProgramLayout.h"
#include "p6/SysexCodec.h"

#include <algorithm>

namespace p6 {

void NrpnDecoder::reset()
{
    paramMsb_ = paramLsb_ = valueMsb_ = 0;
    haveParamMsb_ = haveParamLsb_ = haveValueMsb_ = false;
    rpnResetMsbSeen_ = false;
}

std::optional<std::uint16_t> NrpnDecoder::selectedParameter() const noexcept
{
    if (!haveParamMsb_ || !haveParamLsb_)
        return std::nullopt;
    return static_cast<std::uint16_t>((paramMsb_ << 7) | paramLsb_);
}

std::optional<NrpnUpdate> NrpnDecoder::processCC(std::uint8_t controller, std::uint8_t value)
{
    value &= 0x7F;
    switch (controller) {
    case 99: // NRPN parameter MSB
        paramMsb_ = value;
        haveParamMsb_ = true;
        haveValueMsb_ = false;
        return std::nullopt;
    case 98: // NRPN parameter LSB
        paramLsb_ = value;
        haveParamLsb_ = true;
        haveValueMsb_ = false;
        return std::nullopt;
    case 6: // Data Entry MSB
        if (!haveParamMsb_ || !haveParamLsb_)
            return std::nullopt;
        valueMsb_ = value;
        haveValueMsb_ = true;
        return std::nullopt;
    case 38: // Data Entry LSB
        if (!haveParamMsb_ || !haveParamLsb_ || !haveValueMsb_)
            return std::nullopt;
        return NrpnUpdate{
            static_cast<std::uint16_t>((paramMsb_ << 7) | paramLsb_),
            static_cast<std::uint16_t>((valueMsb_ << 7) | value)
        };
    case 101: // RPN MSB. Prophet-6 resets the NRPN selection on 127/127.
        rpnResetMsbSeen_ = value == 127;
        if (rpnResetMsbSeen_)
            haveValueMsb_ = false;
        return std::nullopt;
    case 100: // RPN LSB: reset only after the matching RPN MSB=127.
        if (value == 127 && rpnResetMsbSeen_)
            reset();
        else
            rpnResetMsbSeen_ = false;
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

namespace {

void upsertRawParameter(P6PatchState& patch,
                        std::uint16_t parameter,
                        std::uint16_t value,
                        std::uint16_t minimum,
                        std::uint16_t maximum)
{
    const auto bounded = std::clamp(value, minimum, maximum);
    const auto normalized = maximum > minimum
        ? static_cast<float>(bounded - minimum) / static_cast<float>(maximum - minimum)
        : 0.0f;

    for (auto& p : patch.rawParameters) {
        if (p.id == parameter) {
            p.raw = value;
            p.normalized = normalized;
            return;
        }
    }

    RawParameter raw;
    raw.id = parameter;
    raw.raw = value;
    raw.normalized = normalized;
    raw.implemented = false;
    patch.rawParameters.push_back(raw);
}

void refreshName(P6PatchState& patch)
{
    patch.name.clear();
    patch.name.reserve(SysexCodec::kPatchNameLength);
    for (std::size_t i = 0; i < SysexCodec::kPatchNameLength; ++i) {
        const auto c = patch.rawPayload[SysexCodec::kPatchNameOffset + i];
        patch.name.push_back(c >= 32 && c <= 126 ? static_cast<char>(c) : ' ');
    }
    while (!patch.name.empty() && patch.name.back() == ' ')
        patch.name.pop_back();
}

} // namespace

void applyRawNrpn(P6PatchState& patch, NrpnUpdate update)
{
    // The 768 sequence note/velocity NRPNs are documented but intentionally
    // not expanded into the small scalar parameter registry.
    SequenceNrpnAddress seq{};
    if (sequenceAddressForNrpn(update.parameter, seq)) {
        const auto minimum = static_cast<std::uint16_t>(seq.velocity ? 0 : 12);
        const auto maximum = static_cast<std::uint16_t>(seq.velocity ? 127 : 108);
        const auto clamped = std::clamp(update.value, minimum, maximum);
        const auto base = P6Sequence::kPayloadOffset + seq.track * P6Sequence::kBytesPerTrack;
        const auto offset = base + (seq.velocity ? P6Sequence::kSteps : 0) + seq.step;

        if (seq.velocity) {
            // NRPN carries only the documented 7-bit velocity. Keep the
            // program-dump-only bit-7 metadata untouched until its exact
            // firmware meaning is proven.
            const auto flag = static_cast<std::uint8_t>(patch.rawPayload[offset] & 0x80u);
            patch.rawPayload[offset] = static_cast<std::uint8_t>(flag | (clamped & 0x7fu));
        } else {
            patch.rawPayload[offset] = static_cast<std::uint8_t>(clamped);
        }
        upsertRawParameter(patch, update.parameter, clamped, minimum, maximum);
        return;
    }

    // Program name characters use NRPN 236..255 and payload bytes 107..126.
    if (update.parameter >= 236 && update.parameter <= 255) {
        const auto clamped = std::clamp<std::uint16_t>(update.value, 32, 125);
        const auto offset = SysexCodec::kPatchNameOffset + (update.parameter - 236);
        patch.rawPayload[offset] = static_cast<std::uint8_t>(clamped);
        upsertRawParameter(patch, update.parameter, clamped, 32, 125);
        refreshName(patch);
        return;
    }

    const auto* info = findProgramParameter(update.parameter);
    const auto maxValue = info ? info->maximum : static_cast<std::uint16_t>(16383);
    const auto minValue = info ? info->minimum : static_cast<std::uint16_t>(0);
    const auto clamped = std::clamp(update.value, minValue, maxValue);

    // If this scalar has a verified SysEx byte mapping, keep the canonical raw
    // payload and the NRPN convenience view in lock-step. Sequence controls
    // 168..171 remain NRPN-only until their program-dump offsets are verified.
    if (const auto* serialized = findSerializedByNrpn(update.parameter))
        patch.rawPayload[serialized->sysexOffset] = static_cast<std::uint8_t>(clamped & 0xffu);

    upsertRawParameter(patch, update.parameter, clamped, minValue, maxValue);
}

} // namespace p6
