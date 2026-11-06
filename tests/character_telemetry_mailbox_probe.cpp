#include "RealtimeSnapshotMailbox.h"

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>

struct TestFrame
{
    std::uint32_t tag{};
    std::array<std::uint32_t,72> measures{};
};

static TestFrame makeFrame(std::uint32_t tag)
{
    TestFrame t{tag,{}};
    for(std::size_t i=0;i<t.measures.size();++i)
        t.measures[i]=(tag*2654435761u)^static_cast<std::uint32_t>(i*7919u);
    return t;
}

static void validate(const TestFrame& f)
{
    for(std::size_t i=0;i<f.measures.size();++i)
        assert(f.measures[i]==((f.tag*2654435761u)^
               static_cast<std::uint32_t>(i*7919u)));
}

int main()
{
    p6plugin::RealtimeSnapshotMailbox<TestFrame> mailbox;
    TestFrame frame{};
    frame.tag=31337; // unsuccessful reads cannot modify caller-owned output
    std::uint32_t generation=99;
    assert(!mailbox.tryRead(frame,generation));
    assert(generation==99 && frame.tag==31337);
    assert(mailbox.publish(makeFrame(1)));
    assert(mailbox.tryRead(frame,generation));
    validate(frame);
    assert(frame.tag==1 && generation==1);

    std::atomic<bool> finished{false};
    std::atomic<unsigned> failedWrites{0};
    std::atomic<std::uint32_t> lastPublishedTag{1};
    std::thread producer([&] {
        for(std::uint32_t tag=2;tag<50002;++tag)
        {
            if(mailbox.publish(makeFrame(tag)))
                lastPublishedTag.store(tag,std::memory_order_release);
            else
                failedWrites.fetch_add(1,std::memory_order_relaxed);
            if((tag&255)==0)
                std::this_thread::yield();
        }
        finished.store(true,std::memory_order_release);
    });

    unsigned validated=0;
    std::uint32_t lastRevision=1;
    for(unsigned attempts=0;attempts<400000;++attempts)
    {
        TestFrame sampled{};
        std::uint32_t observedRevision{};
        if(mailbox.tryRead(sampled,observedRevision))
        {
            validate(sampled);
            // The writer can skip publishing when both alternate slots are
            // temporarily busy, but a successful publish never tears.
            assert(observedRevision>=lastRevision);
            lastRevision=observedRevision;
            ++validated;
        }
        if((attempts&127)==0)
            std::this_thread::yield();
        if(finished.load(std::memory_order_acquire)&&attempts>20000)
            break;
    }
    producer.join();
    assert(validated>0);
    assert(mailbox.tryRead(frame,generation));
    validate(frame);
    assert(generation>=lastRevision);
    assert(mailbox.revision()==generation);
    // The producer finished: the final published frame is authoritative,
    // and the full 72-element payload must still match its generation.
    assert(frame.tag==lastPublishedTag.load(std::memory_order_acquire));
    assert(generation>=2 && generation<=50001);
    assert(failedWrites.load(std::memory_order_relaxed)<50000);
    std::cout<<"Coherent Character mailbox: concurrent publication and "
             <<"72 correlated values per frame; no torn data\n";
}
