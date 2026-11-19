#pragma once
#include <array>
#include <cstdint>
#include <span>
#include "p6/SysexCodec.h"

namespace p6 {
// Fixed-size, allocation-free decoding of one complete Prophet-6 program or
// edit-buffer frame. Destination is unchanged if validation fails.
struct RealtimeSysexPayload {
    std::array<std::uint8_t, SysexCodec::kUnpackedPayloadSize> bytes{};
    int bank{-1}, program{-1};
};
inline bool decodeRealtimeSysex(std::span<const std::uint8_t> input,
                                RealtimeSysexPayload& destination) noexcept {
    if(input.size()<5 || input[0]!=0xf0 || input[1]!=SysexCodec::kSequentialManufacturer ||
       input[2]!=SysexCodec::kProphet6Id || input.back()!=0xf7) return false;
    RealtimeSysexPayload result;
    std::size_t offset=0;
    if(input[3]==SysexCodec::kProgramDataCommand) {
        if(input.size()!=SysexCodec::kProgramDumpSize || input[4]>9 || input[5]>99) return false;
        result.bank=input[4];result.program=input[5];offset=6;
    } else if(input[3]==SysexCodec::kEditBufferDataCommand) {
        if(input.size()!=SysexCodec::kEditBufferSize)return false;
        offset=4;
    } else return false;
    std::size_t end=offset+SysexCodec::kPackedPayloadSize,out=0;
    while(offset<end) {
        const auto msb=input[offset++];if(msb&0x80)return false;
        for(unsigned i=0;i<7 && offset<end;++i) {
            const auto byte=input[offset++];if(byte&0x80)return false;
            if(out>=result.bytes.size())return false;
            result.bytes[out++]=static_cast<std::uint8_t>(byte | (((msb>>i)&1u)<<7u));
        }
    }
    if(out!=result.bytes.size())return false;
    destination=result;return true;
}
}
