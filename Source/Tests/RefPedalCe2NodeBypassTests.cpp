// OWNER: WP14
#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_ce2.h"
#include "../ReferencePedals/Common/OnePole.h"
#include "../ReferencePedals/BoardEngine.h"
#include "../ReferencePedals/Routing/RoutingNodes.h"
#include <array>
#include <cstring>
#include <cstdio>

namespace luthier::tests { long allocationsOnThisThread() noexcept; }
using namespace luthier::reference;

LUTHIER_TEST(RefPedalCe2NodeBypass, originalFiveMsEqualPowerUsesSeparatelyBufferedDry)
{
    // Original CE2 §3: buffered5ms equal-power audio, not raw dry. E60 leaves
    // the active DSP engagement-free; the board supplies the single envelope.
    constexpr double halfPi=1.57079632679489661923;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
        for(bool bbd:{false,true})for(int hostBlock:{1,64,2048})for(bool enabling:{false,true})
    {
        PedalRuntime runtime;runtime.pedal=std::make_unique<models::BossCe2>();
        auto& model=static_cast<models::BossCe2&>(*runtime.pedal);
        models::BossCe2 active;
        for(auto* pedal:{&model,&active})
        {
            pedal->setParam(4,bbd?1.f:0.f);pedal->setParam(5,0);
            pedal->prepare(rate,2048,1);
        }
        CHECK(model.getLatencySamples()==0);
        for(auto& line:runtime.dry)line.prepare(0);
        common::OnePole dryReference[2];for(auto& filter:dryReference)filter.setLowpass(rate,15);
        runtime.enabled=!enabling;runtime.enableMix=enabling?0:1;
        runtime.bypassSamples=rate*.005;
        Topology topology;topology.slotLatency[0]=0;
        PedalNodeState state;state.runtime=&runtime;
        ExecCtx execution;execution.topo=&topology;execution.state[0]=&state;execution.sampleRate=rate;
        Node node;node.lane=node.slot=0;
        std::array<std::array<double,kSubBlock>,2> input{},dry{},wet{},board{};
        // Fill the entire prepared12ms wet-delay range before toggling;
        // a delayed input-start edge is not a bypass-event transient.
        const int event=int(std::ceil(rate*.020))+17,fade=int(std::ceil(rate*.005)),end=event+fade+64;
        double error=0,controlError=0,maxStep=0,previous[2]{};
        bool finite=true,exactDry=true,colouredDry=false;
        uint64_t activeHash=1469598103934665603ull;
        const auto allocations=luthier::tests::allocationsOnThisThread();
        for(int frame=0;frame<end;)
        {
            if(frame==event)runtime.enabled=enabling;
            int n=std::min({kSubBlock,hostBlock-frame%hostBlock,end-frame});
            if(frame<event)n=std::min(n,event-frame);
            for(int i=0;i<n;++i)
            {
                const int sample=frame+i;
                input[0][i]=.1*std::sin(4*halfPi*20*sample/rate+.37);
                input[1][i]=.15*std::cos(4*halfPi*31*sample/rate+.13);
                for(int c=0;c<2;++c)
                {
                    wet[c][i]=board[c][i]=input[c][i];
                    // Separate reference state advances even when active DSP sleeps.
                    dry[c][i]=dryReference[c].highpass(input[c][i]);
                    colouredDry |= dry[c][i]!=input[c][i];
                }
            }
            const bool render=runtime.enabled || runtime.enableMix>0 || model.getTailEnergyDb()>-90.f;
            if(render)active.process(wet[0].data(),wet[1].data(),n);
            execution.n=n;execution.bus[0][0]=board[0].data();execution.bus[0][1]=board[1].data();
            routing::nodePedal(node,execution);
            for(int i=0;i<n;++i)
            {
                const int sample=frame+i,since=sample-event;
                const double t=since<0?0:std::min(1.,(since+1)/(rate*.005));
                const double amount=enabling?t:1-t;
                for(int c=0;c<2;++c)
                {
                    const double expected=amount<=0?dry[c][i]:amount>=1?wet[c][i]
                        :dry[c][i]*std::cos(halfPi*amount)+wet[c][i]*std::sin(halfPi*amount);
                    error=std::max(error,std::abs(board[c][i]-expected));
                    finite &= std::isfinite(board[c][i]);
                    if(amount==0 && (since<0 || since>fade+1))exactDry &= board[c][i]==dry[c][i];
                    if(sample>=event-1 && sample<=event+fade+1)
                        maxStep=std::max(maxStep,std::abs(board[c][i]-previous[c]));
                    previous[c]=board[c][i];
                    if(render)
                    {
                        uint64_t bits;std::memcpy(&bits,&wet[c][i],sizeof(bits));
                        activeHash^=bits;activeHash*=1099511628211ull;
                    }
                }
            }
            const int since=frame+n-1-event;
            const double t=since<0?0:std::min(1.,(since+1)/(rate*.005));
            controlError=std::max(controlError,std::abs(runtime.enableMix-(enabling?t:1-t)));
            frame+=n;
        }
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        const auto label=juce::String(rate)+"/bbd"+juce::String(int(bbd))+"/block"+juce::String(hostBlock)+(enabling?"/on":"/off");
        CHECK_MSG(error<1.e-12,label+" equal-power error "+juce::String(error,15));
        CHECK_MSG(controlError<1.e-12,label+" control error "+juce::String(controlError,15));
        CHECK(finite);CHECK(exactDry);CHECK(colouredDry);
        // Diagnostic only: this scoped law test does not reinterpret or close
        // original §9's raw-60dBFSjump/raw-input-null criteria.
        std::printf("CE2_NODE rate%.0f bbd%d block%d enable%d error%.17g control%.17g step%.17g dry%d active%016llx\n",
            rate,int(bbd),hostBlock,int(enabling),error,controlError,maxStep,int(exactDry),
            static_cast<unsigned long long>(activeHash));
    }
}
