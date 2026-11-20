#include "p6/RealtimeControlQueue.h"
#include <cstdlib>
#define CHECK(expr) do { if (!(expr)) std::abort(); } while (false)
#include <thread>

int main() {
    p6::RealtimeControlQueue<2> queue;
    p6::RealtimeControlEvent in, out;
    in.sysex.bytes[0] = 77;
    CHECK(queue.push(in));
    in.type = p6::RealtimeControlEvent::Type::nrpn;
    in.parameter = 45; in.value = 99;
    CHECK(queue.push(in));
    CHECK(!queue.push(in));
    CHECK(queue.pop(out) && out.type == p6::RealtimeControlEvent::Type::sysex && out.sysex.bytes[0] == 77);
    CHECK(queue.pop(out) && out.type == p6::RealtimeControlEvent::Type::nrpn && out.parameter == 45 && out.value == 99);
    CHECK(!queue.pop(out));

    p6::RealtimeControlQueue<32> concurrent;
    constexpr unsigned count = 30000;
    std::thread producer([&] {
        p6::RealtimeControlEvent event;
        event.type = p6::RealtimeControlEvent::Type::nrpn;
        for (unsigned i = 0; i < count; ++i) {
            event.parameter = static_cast<std::uint16_t>(i);
            event.value = static_cast<std::uint16_t>(i ^ 0x5a5a);
            while (!concurrent.push(event)) std::this_thread::yield();
        }
    });
    for (unsigned i = 0; i < count; ++i) {
        while (!concurrent.pop(out)) std::this_thread::yield();
        CHECK(out.parameter == i && out.value == static_cast<std::uint16_t>(i ^ 0x5a5a));
    }
    producer.join();
}
