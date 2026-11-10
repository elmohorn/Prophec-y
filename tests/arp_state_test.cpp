#include "PluginStateCodec.h"
#include <cassert>
#include <cmath>
int main(){
 p6plugin::WrapperState original;
 original.arpCharacterEnabled=true;original.arpCharacterFreeze=true;
 original.arpCharacterMask=0x5a;original.arpCharacterRhythm=3;original.arpCharacterMotion=.375f;
 original.arpVariationSeed=0xaabbccdd;
 auto bytes=p6plugin::encodeState(original);p6plugin::WrapperState actual;
 assert(p6plugin::decodeState(bytes,actual));
 assert(actual.arpCharacterEnabled&&actual.arpCharacterFreeze&&actual.arpCharacterMask==0x5a);
 assert(actual.arpCharacterRhythm==3&&std::abs(actual.arpCharacterMotion-.375f)<.001f);
 assert(actual.arpVariationSeed==0xaabbccddu);
 bytes.resize(p6plugin::kV6StateBytes);bytes[4]=6;bytes[5]=0;
 assert(p6plugin::decodeState(bytes,actual));
 assert(!actual.arpCharacterEnabled&&!actual.arpCharacterFreeze&&actual.arpCharacterMask==255);
 assert(actual.arpCharacterRhythm==8&&actual.arpCharacterMotion==0);
 bytes[4]=7;assert(!p6plugin::decodeState(bytes,actual));
}
