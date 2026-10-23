#include "TestFramework.h"
#include "../ReferencePedals/Generic/Chorus.h"
#include "../ReferencePedals/Generic/Phaser.h"
#include "../ReferencePedals/Generic/Tremolo.h"
#include "../ReferencePedals/Generic/Wah.h"
#include "../ReferencePedals/Generic/Delay.h"
#include "../ReferencePedals/Generic/Reverb.h"
#include "../ReferencePedals/Common/LooperCore.h"
#include <array>
#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
namespace {
template<class P> long allocations()
{
    P p;p.prepare(48000,2048,1);std::array<double,2048> l{},r{};
    const auto before=luthier::tests::allocationsOnThisThread();
    for(int b=0;b<32;++b)
    {
        for(int k=0;k<p.desc().numParams;++k)p.setParam(k,float((b+k)%3)*.5f);
        l[0]=.1;r[0]=-.1;p.process(l.data(),r.data(),2048);
        p.onFoot({true,.5f,0,0});p.setExpression(0,.5f);p.setEnabled(b%2);
    }
    return luthier::tests::allocationsOnThisThread()-before;
}
}
LUTHIER_TEST(RefPedalTimeFxRt, noAudioCallbackAllocations)
{
    using namespace luthier::reference;
    CHECK(allocations<generic::Chorus>()==0);CHECK(allocations<generic::Phaser>()==0);
    CHECK(allocations<generic::Tremolo>()==0);CHECK(allocations<generic::Wah>()==0);
    CHECK(allocations<generic::Delay>()==0);CHECK(allocations<generic::Reverb>()==0);
    common::LooperCore core;core.prepare(48000,1,2,2000000);
    std::array<double,2048> l{},r{};
    const auto before=luthier::tests::allocationsOnThisThread();
    core.enqueue({common::LooperCore::Action::Record,0});core.enqueue({common::LooperCore::Action::Play,4096});
    for(int i=0;i<8;++i)core.process(l.data(),r.data(),2048);
    core.apply(common::LooperCore::Action::Overdub);core.process(l.data(),r.data(),2048);
    core.apply(common::LooperCore::Action::Play);core.apply(common::LooperCore::Action::Undo);core.apply(common::LooperCore::Action::Redo);
    core.apply(common::LooperCore::Action::Clear);core.reset();
    const auto delta=luthier::tests::allocationsOnThisThread()-before;CHECK(delta==0);
}
#endif
