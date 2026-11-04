#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_dd3t.h"
#include "../ReferencePedals/BoardEngine.h"
#include "../ReferencePedals/BoardLimiter.h"
#include "../ReferencePedals/Routing/RoutingNodes.h"
#include <array>
using namespace luthier::reference;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
namespace
{
// Real node/model fixture, following the Phase90 composition fixture. The
// independent model renders once per sample; the node uses real host partitions.
struct Dd3tNodeFixture
{
    PedalRuntime runtime;
    models::BossDd3t reference;
    Topology topology;
    PedalNodeState state;
    ExecCtx execution;
    Node node;
    explicit Dd3tNodeFixture(double rate)
    {
        runtime.pedal=std::make_unique<models::BossDd3t>();
        runtime.enabled=true;runtime.enableMix=1;runtime.bypassSamples=rate*.005;
        runtime.sampleRate=rate;
        for(auto& line:runtime.dry)line.prepare(0);
        state.runtime=&runtime;execution.topo=&topology;execution.state[0]=&state;
        execution.sampleRate=rate;node.slot=node.lane=0;
    }
    void prepare(double rate,bool loop,bool trails)
    {
        for(auto* pedal:{runtime.pedal.get(),static_cast<IPedal*>(&reference)})
        {
            pedal->setEnabled(true);pedal->setParam(0,1);pedal->setParam(1,0);
            pedal->setParam(2,loop?2.f/3:0);pedal->setParam(3,loop?1:0);
            pedal->setParam(8,trails?1:0);pedal->setParam(10,1);
            pedal->prepare(rate,2048,1);
        }
    }
    void enable(bool enabled)
    {runtime.enabled=enabled;runtime.pedal->setEnabled(enabled);reference.setEnabled(enabled);}
    void process(double* l,double* r,int n)
    {execution.n=n;execution.bus[0][0]=l;execution.bus[0][1]=r;routing::nodePedal(node,execution);}
};
}
LUTHIER_TEST(RefPedalDd3tNode, actualResidualComposesWithSlotTrimsAndGuardedDry)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.}) for(int hostBlock:{1,64,2048})
        for(bool trails:{false,true}) for(bool hot:{false,true})
    {
        Dd3tNodeFixture f(rate);f.prepare(rate,false,trails);
        f.runtime.inputGain=hot?4.f:.5f;f.runtime.outputGain=hot?.25f:2.f;
        std::array<double,kSubBlock> l{},r{};
        const int event=int(rate*.12),end=event+int(rate*.12);
        int frame=0;double amount=1,error=0,lateResidual=0;bool finite=true;
        const auto allocations=luthier::tests::allocationsOnThisThread();
        while(frame<end)
        {
            if(frame==event)f.enable(false);
            int n=std::min({kSubBlock,hostBlock-frame%hostBlock,end-frame});
            if(frame<event)n=std::min(n,event-frame);
            std::array<std::array<double,kSubBlock>,2> expected{};
            for(int i=0;i<n;++i)
            {
                // Opposite hot channels expose erroneous dry subtraction and
                // polarity reversal, without relying on a clipped output oracle.
                const double dryL=(hot?32.:.1)*std::sin((frame+i)*.13+.3);
                const double dryR=(hot?-32.:.2)*std::cos((frame+i)*.17+.7);
                l[i]=dryL;r[i]=dryR;
                double wetL=dryL*f.runtime.inputGain,wetR=dryR*f.runtime.inputGain,wl=0,wr=0;
                f.reference.processWithWetResidual(&wetL,&wetR,&wl,&wr,1);
                amount=std::clamp(amount+(f.runtime.enabled?1:-1)/f.runtime.bypassSamples,0.,1.);
                const double bypassL=dryL+wl*f.runtime.outputGain,bypassR=dryR+wr*f.runtime.outputGain;
                expected[0][i]=BoardLimiter::process(bypassL+amount*(wetL*f.runtime.outputGain-bypassL),false,1);
                expected[1][i]=BoardLimiter::process(bypassR+amount*(wetR*f.runtime.outputGain-bypassR),false,1);
                if(frame+i>event+int(rate*.08))lateResidual+=wl*wl+wr*wr;
            }
            f.process(l.data(),r.data(),n);
            for(int i=0;i<n;++i)
            {
                error=std::max({error,std::abs(l[i]-expected[0][i]),std::abs(r[i]-expected[1][i])});
                finite &= std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i])<=16 && std::abs(r[i])<=16;
            }
            frame+=n;
        }
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        CHECK_MSG(error<1.e-12,juce::String(rate)+"/"+juce::String(hostBlock)+" node composition error "+juce::String(error,15));
        CHECK(finite);CHECK(f.runtime.tailResidualAvailable);
        if(!trails)CHECK(lateResidual==0);
    }
}
LUTHIER_TEST(RefPedalDd3tNode, heldLoopIsNotDeclaredSilentAndDroppedDuringSpillover)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.}) for(int hostBlock:{1,64,2048})
    {
        Dd3tNodeFixture f(rate);f.prepare(rate,true,true);
        std::array<double,kSubBlock> l{},r{};
        const int capture=int(rate),bypass=capture+int(rate*.03),release=bypass+int(rate*2),end=release+int(rate*.2);
        int frame=0;double error=0,lastLoopEnergy=0,finalEnergy=0;bool finite=true,heldReportedActive=false;
        const auto allocations=luthier::tests::allocationsOnThisThread();
        while(frame<end)
        {
            if(frame==capture){f.runtime.pedal->setParam(5,1);f.reference.setParam(5,1);}
            if(frame==bypass)f.enable(false);
            if(frame==release){f.runtime.pedal->setParam(5,0);f.reference.setParam(5,0);}
            int n=std::min({kSubBlock,hostBlock-frame%hostBlock,end-frame});
            for(int event:{capture,bypass,release})if(frame<event)n=std::min(n,event-frame);
            std::array<double,kSubBlock> expectedL{},expectedR{};
            for(int i=0;i<n;++i)
            {
                l[i]=frame+i<capture?.1*std::sin((frame+i)*.07):0;
                r[i]=frame+i<capture?.12*std::cos((frame+i)*.11):0;
                expectedL[i]=l[i];expectedR[i]=r[i];
                f.reference.process(&expectedL[i],&expectedR[i],1);
                if(frame+i>release-int(rate*.2) && frame+i<release)
                    lastLoopEnergy+=expectedL[i]*expectedL[i]+expectedR[i]*expectedR[i];
            }
            f.process(l.data(),r.data(),n);
            // Once the node's 5ms control has settled, its zero-input spillover
            // must equal the direct model's actual held loop, not the empty line.
            for(int i=0;i<n;++i)
            {
                if(frame+i>bypass+int(rate*.05))error=std::max({error,std::abs(l[i]-expectedL[i]),std::abs(r[i]-expectedR[i])});
                if(frame+i>end-int(rate*.05))finalEnergy+=l[i]*l[i]+r[i]*r[i];
                finite &= std::isfinite(l[i]) && std::isfinite(r[i]);
            }
            if(frame+n==release)heldReportedActive=f.runtime.pedal->getTailEnergyDb()>-90;
            frame+=n;
        }
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        CHECK_MSG(heldReportedActive,"audible held loop reported silent");
        CHECK_MSG(error<1.e-12,juce::String(rate)+"/"+juce::String(hostBlock)+" held-loop dropout "+juce::String(error,15));
        CHECK(lastLoopEnergy>1.e-6);CHECK(finalEnergy<1.e-12);
        CHECK(f.runtime.pedal->getTailEnergyDb()<-90);CHECK(finite);
    }
}
LUTHIER_TEST(RefPedalDd3tNode, sparseHeldLoopSurvivesQuietGapsAndTrailsOffStillCuts)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        Dd3tNodeFixture f(rate);f.prepare(rate,true,true);
        f.runtime.pedal->setParam(2,1);f.reference.setParam(2,1);
        // Prepare again so the captured loop is the maximum800ms immediately.
        f.runtime.pedal->prepare(rate,2048,1);f.reference.prepare(rate,2048,1);
        const int capture=int(rate*2),bypass=capture+int(rate*.03),cut=bypass+int(rate*2.4),end=cut+int(rate*.2);
        std::array<double,kSubBlock> l{},r{};
        int frame=0;double error=0,lateLoopEnergy=0,cutEnergy=0;bool finite=true,heldReportedActive=false;
        const auto allocations=luthier::tests::allocationsOnThisThread();
        while(frame<end)
        {
            if(frame==capture){f.runtime.pedal->setParam(5,1);f.reference.setParam(5,1);}
            if(frame==bypass)f.enable(false);
            if(frame==cut){f.runtime.pedal->setParam(8,0);f.reference.setParam(8,0);}
            int n=std::min(kSubBlock,end-frame);
            for(int event:{capture,bypass,cut})if(frame<event)n=std::min(n,event-frame);
            std::array<double,kSubBlock> x{},y{};
            for(int i=0;i<n;++i)
            {
                const bool pulse=frame+i>=int(rate*.55) && frame+i<int(rate*.553);
                l[i]=x[i]=pulse?.1*std::sin((frame+i)*.07):0;
                r[i]=y[i]=pulse?.13*std::cos((frame+i)*.11):0;
                f.reference.process(&x[i],&y[i],1);
                if(frame+i>cut-int(rate*.8) && frame+i<cut)lateLoopEnergy+=x[i]*x[i]+y[i]*y[i];
            }
            f.process(l.data(),r.data(),n);
            for(int i=0;i<n;++i)
            {
                if(frame+i>bypass+int(rate*.05))error=std::max({error,std::abs(l[i]-x[i]),std::abs(r[i]-y[i])});
                if(frame+i>end-int(rate*.05))cutEnergy+=l[i]*l[i]+r[i]*r[i];
                finite &= std::isfinite(l[i]) && std::isfinite(r[i]);
            }
            if(frame+n==cut)heldReportedActive=f.runtime.pedal->getTailEnergyDb()>-90;
            frame+=n;
        }
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        CHECK_MSG(error<1.e-12,juce::String(rate)+" sparse held-loop dropout "+juce::String(error,15));
        CHECK(lateLoopEnergy>1.e-6);CHECK(cutEnergy==0);CHECK(finite);CHECK(heldReportedActive);
        CHECK(f.runtime.pedal->getTailEnergyDb()<-90);
        CHECK(static_cast<models::BossDd3t&>(*f.runtime.pedal).shortLoopActive());
    }
}
