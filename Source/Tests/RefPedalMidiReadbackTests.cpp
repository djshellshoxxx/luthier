// OWNER: WP6
#include "TestFramework.h"
#include "../ReferencePedals/Midi/ParameterReadbackMailbox.h"
#include <atomic>
#include <limits>
#include <mutex>
#include <thread>
using namespace luthier::reference::midi;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
namespace
{
ReadbackExpectation readbackStamp(uint64_t model=11,uint64_t restore=21,uint64_t edit=31,uint64_t host=41)
{
    ReadbackExpectation e; e.identity.instance[0]=1; e.identity.instance[15]=0x7f;
    e.identity.modelGeneration=model; e.identity.restoreGeneration=restore; e.modelIntent=edit; e.hostIntent=host; return e;
}
}
LUTHIER_TEST(RefPedalMidiReadback, latestValueCoalescesWithoutAllocating)
{
    ParameterReadbackMailbox mailbox; const auto stamp=readbackStamp(); ParameterReadback out;
    CHECK(mailbox.readMatching(0,stamp,out)==ReadbackStatus::Empty);
    const auto before=luthier::tests::allocationsOnThisThread(); bool accepted=true;
    for(int i=0;i<100000;++i) accepted &= mailbox.publish(0,stamp,(i%128)/127.f,ReadbackPersistence::Control);
    accepted &= mailbox.publish(0,stamp,.375f,ReadbackPersistence::Control);
    const auto after=luthier::tests::allocationsOnThisThread(); CHECK(before==after); CHECK(accepted);
    CHECK(mailbox.readMatching(0,stamp,out)==ReadbackStatus::Ready); CHECK(out.target==0); CHECK(out.normalised==.375f); CHECK(out.expected==stamp);
    CHECK(mailbox.readMatching(0,stamp,out)==ReadbackStatus::Empty);
}
LUTHIER_TEST(RefPedalMidiReadback, fullIdentityAndEveryIntentFieldRejectStaleWrites)
{
    const auto original=readbackStamp();
    for(int field=0;field<5;++field)
    {
        ParameterReadbackMailbox mailbox; auto current=original;
        switch(field)
        {
            case 0: current.identity.instance[15]^=1; break;
            case 1: current.identity.modelGeneration+=uint64_t(1)<<40; break;
            case 2: current.identity.restoreGeneration+=uint64_t(1)<<41; break;
            case 3: current.modelIntent+=uint64_t(1)<<42; break;
            case 4: current.hostIntent+=uint64_t(1)<<43; break;
        }
        CHECK(mailbox.publish(3,original,.9f,ReadbackPersistence::Control)); ParameterReadback out; out.normalised=.123f;
        CHECK(mailbox.readMatching(3,current,out)==ReadbackStatus::Stale); CHECK(out.normalised==.123f);
        CHECK(mailbox.readMatching(3,original,out)==ReadbackStatus::Empty); // stale records cannot reappear after restore
    }
}
LUTHIER_TEST(RefPedalMidiReadback, actionsAndInvalidValuesNeverBecomePersistentState)
{
    ParameterReadbackMailbox mailbox; const auto stamp=readbackStamp(); ParameterReadback out;
    CHECK(!mailbox.publish(1,stamp,1,ReadbackPersistence::Action)); CHECK(mailbox.readMatching(1,stamp,out)==ReadbackStatus::Empty);
    CHECK(!mailbox.publish((uint16_t)ParameterReadbackMailbox::capacity,stamp,.5f,ReadbackPersistence::Control));
    CHECK(!mailbox.publish(1,stamp,std::numeric_limits<float>::quiet_NaN(),ReadbackPersistence::Control));
    CHECK(!mailbox.publish(1,stamp,-.1f,ReadbackPersistence::Control)); CHECK(!mailbox.publish(1,stamp,1.1f,ReadbackPersistence::Control));
    auto invalid=stamp; invalid.identity.restoreGeneration=0; CHECK(!mailbox.publish(1,invalid,.5f,ReadbackPersistence::Control));
    CHECK(mailbox.publish(kTargetBoardBase+2,stamp,1,ReadbackPersistence::Control)); CHECK(mailbox.readMatching(kTargetBoardBase+2,stamp,out)==ReadbackStatus::Ready);
    CHECK(mailbox.readMatching((uint16_t)ParameterReadbackMailbox::capacity,stamp,out)==ReadbackStatus::Invalid);
}
LUTHIER_TEST(RefPedalMidiReadback, wrapSkipsUnwrittenZeroAndHostWrapInvalidatesOlderReadback)
{
    CHECK(detail::nextReadbackSequence(0)==2); CHECK(detail::nextReadbackSequence(UINT64_MAX-1)==2);
    ParameterReadbackMailbox mailbox; auto old=readbackStamp(11,21,31,UINT32_MAX),current=old; current.hostIntent=1;
    CHECK(mailbox.publish(0,old,.7f,ReadbackPersistence::Control)); ParameterReadback out;
    CHECK(mailbox.readMatching(0,current,out)==ReadbackStatus::Stale);
    CHECK(mailbox.publish(0,current,.4f,ReadbackPersistence::Control)); CHECK(mailbox.readMatching(0,current,out)==ReadbackStatus::Ready); CHECK(out.normalised==.4f);
    const auto captured=out; ++current.modelIntent; CHECK(!(captured.expected==current)); // readMatching is not a commit guard
}
LUTHIER_TEST(RefPedalMidiReadback, atomicPayloadSurvivesConcurrentRestoreAndReplacement)
{
    ParameterReadbackMailbox mailbox; std::atomic<int> epoch{1}; std::atomic<bool> restored{false},done{false},noAllocations{true};
    std::mutex modelMutationGuard; bool valid=true; float persisted=0;
    const auto a=readbackStamp(111,222,333,444),b=readbackStamp(1111,2222,3333,4444);
    std::thread restore([&]
    {
        for(int i=0;i<10000;++i) { const std::lock_guard<std::mutex> guard(modelMutationGuard); epoch.store(1+i%2,std::memory_order_release); persisted=.1f; }
        { const std::lock_guard<std::mutex> guard(modelMutationGuard); epoch.store(2,std::memory_order_release); }
        restored.store(true,std::memory_order_release);
    });
    std::thread audio([&]
    {
        const auto before=luthier::tests::allocationsOnThisThread();
        for(int i=0;i<100000;++i) { const bool second=epoch.load(std::memory_order_acquire)==2; mailbox.publish(0,second?b:a,second?.75f:.25f,ReadbackPersistence::Control); }
        while(!restored.load(std::memory_order_acquire)) {}
        mailbox.publish(0,b,.9f,ReadbackPersistence::Control);
        noAllocations.store(before==luthier::tests::allocationsOnThisThread(),std::memory_order_release); done.store(true,std::memory_order_release);
    });
    while(!done.load(std::memory_order_acquire))
    {
        const std::lock_guard<std::mutex> guard(modelMutationGuard); const bool second=epoch.load(std::memory_order_acquire)==2;
        ParameterReadback out; if(mailbox.readMatching(0,second?b:a,out)==ReadbackStatus::Ready)
        { valid &= out.expected==(second?b:a); valid &= out.normalised==(second?.75f:.25f)||out.normalised==.9f; persisted=out.normalised; }
    }
    audio.join(); restore.join(); ParameterReadback final;
    if(mailbox.readMatching(0,b,final)==ReadbackStatus::Ready) persisted=final.normalised;
    CHECK(valid); CHECK(noAllocations.load()); CHECK(persisted==.9f);
}
LUTHIER_TEST(RefPedalMidiReadback, unrelatedTargetsAndBoardControlsRetainIndependentLatestValues)
{
    ParameterReadbackMailbox mailbox; const auto stamp=readbackStamp(); auto board=readbackStamp(55,66); board.identity.instance.fill(0);
    CHECK(mailbox.publish(0,stamp,.25f,ReadbackPersistence::Control)); CHECK(mailbox.publish(1,stamp,.75f,ReadbackPersistence::Control));
    CHECK(mailbox.publish(0,stamp,.5f,ReadbackPersistence::Control)); CHECK(mailbox.publish(kTargetBoardBase,board,.8f,ReadbackPersistence::Control));
    CHECK(!mailbox.publish(1,stamp,1,ReadbackPersistence::Action)); ParameterReadback out;
    CHECK(mailbox.readMatching(0,stamp,out)==ReadbackStatus::Ready); CHECK(out.normalised==.5f);
    CHECK(mailbox.readMatching(1,stamp,out)==ReadbackStatus::Ready); CHECK(out.normalised==.75f);
    CHECK(mailbox.readMatching(kTargetBoardBase,board,out)==ReadbackStatus::Ready); CHECK(out.normalised==.8f);
}
