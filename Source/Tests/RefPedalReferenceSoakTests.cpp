#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_ce2.h"
#include "../ReferencePedals/Models/boss_ce2w.h"
#include "../ReferencePedals/Models/boss_dd3t.h"
#include "../ReferencePedals/Models/mxr_phase90_script.h"
#include <array>
namespace {
template<class P> void soak(luthier::tests::TestContext& ctx)
{
    for(double sr:{44100.,48000.,96000.,192000.})
    {
        const double seconds=luthier::tests::perfRunRequested()?1800:60;
        P pedal;pedal.prepare(sr,2048,1);pedal.setEnabled(true);
        std::array<double,2048> left{},right{};uint32_t random=73;int position=0,blocks=0;bool finite=true,bounded=true;
        auto next=[&](){random=1664525u*random+1013904223u;return double(random)/4294967296.;};
        constexpr int sizes[]{1,4,16,64,512,2048};
        while(position<int(sr*seconds))
        {
            const int n=std::min(sizes[blocks%6],int(sr*seconds)-position);
            if(blocks%31==0)
            {
                const int parameter=int(next()*pedal.desc().numParams);
                pedal.setParam(parameter,float(next()));pedal.setExpression(0,float(next()));
                pedal.setEnabled(next()>.1);pedal.onFoot({blocks%2==0,float(next()),float(next()),0});
            }
            for(int i=0;i<n;++i){left[i]=(next()-.5)*2;right[i]=(next()-.5)*2;}
            pedal.process(left.data(),right.data(),n);
            for(int i=0;i<n;++i){finite=finite&&std::isfinite(left[i])&&std::isfinite(right[i]);bounded=bounded&&std::abs(left[i])<=16&&std::abs(right[i])<=16;}
            position+=n;++blocks;
        }
        CHECK(finite);CHECK(bounded);
    }
}
}
LUTHIER_TEST(RefPedalReferenceSoak, randomizedSixtySecondsAtEveryRate)
{
    using namespace luthier::reference::models;
    soak<BossCe2>(ctx);soak<BossCe2w>(ctx);soak<BossDd3t>(ctx);soak<MxrPhase90Script>(ctx);
}
