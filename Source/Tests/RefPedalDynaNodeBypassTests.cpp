// OWNER: WP12
#include "TestFramework.h"
#include "../ReferencePedals/Models/mxr_dynacomp.h"
#include "../ReferencePedals/BoardEngine.h"
#include "../ReferencePedals/Routing/RoutingNodes.h"
#include <array>
#include <cstring>
#include <cstdio>

namespace luthier::tests { long allocationsOnThisThread() noexcept; }
using namespace luthier::reference;

LUTHIER_TEST(RefPedalDynaNodeBypass, originalFiveMsEqualPowerLawAndLatencyMatchedDry)
{
    // Original model §3:5ms equal-power audio; board mix_enable stays linear.
    // Low-frequency inputs isolate the bypass step from a high-frequency
    // stimulus's own adjacent-sample slope. §9.12's .01FS bound is unchanged.
    constexpr double halfPi=1.57079632679489661923;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
        for(int quality:{0,1,2})for(int hostBlock:{1,64,2048})for(bool enabling:{false,true})
    {
        PedalRuntime runtime;runtime.pedal=std::make_unique<models::MxrDynaComp>();
        auto& model=static_cast<models::MxrDynaComp&>(*runtime.pedal);
        models::MxrDynaComp active;
        for(auto* pedal:{&model,&active})
        {
            pedal->setParam(5,0); // deterministic noise-off reference
            pedal->setParam(6,quality*.5f); // natural2x/4x/off, policyHigh
            pedal->prepare(rate,2048,2);
        }
        const int latency=model.getLatencySamples();
        CHECK(latency==(quality==0?15:quality==1?23:0));
        for(auto& line:runtime.dry)line.prepare(latency);
        runtime.enabled=!enabling;runtime.enableMix=enabling?0:1;
        runtime.bypassSamples=rate*.005;
        Topology topology;topology.slotLatency[0]=latency;
        PedalNodeState state;state.runtime=&runtime;
        ExecCtx execution;execution.topo=&topology;execution.state[0]=&state;execution.sampleRate=rate;
        Node node;node.lane=node.slot=0;
        std::array<std::array<double,kSubBlock>,2> input{},wet{},board{};
        std::array<std::array<double,4096>,2> history{};
        const int event=257,fade=int(std::ceil(rate*.005)),end=event+fade+64;
        double error=0,controlError=0,maxStep=0,previous[2]{};
        bool finite=true,exactDry=true;
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
                for(int c=0;c<2;++c)history[c][sample]=wet[c][i]=board[c][i]=input[c][i];
            }
            const bool render=runtime.enabled || runtime.enableMix>0;
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
                    const double dry=sample<latency?0:history[c][sample-latency];
                    const double expected=amount<=0?dry:amount>=1?wet[c][i]
                        :dry*std::cos(halfPi*amount)+wet[c][i]*std::sin(halfPi*amount);
                    error=std::max(error,std::abs(board[c][i]-expected));
                    finite &= std::isfinite(board[c][i]);
                    if(amount==0 && (since<0 || since>fade+1))exactDry &= board[c][i]==dry;
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
        const auto label=juce::String(rate)+"/q"+juce::String(quality)+"/block"+juce::String(hostBlock)+(enabling?"/on":"/off");
        CHECK_MSG(error<1.e-12,label+" equal-power error "+juce::String(error,15));
        CHECK_MSG(controlError<1.e-12,label+" control position error "+juce::String(controlError,15));
        CHECK(finite);CHECK(exactDry);
        CHECK_MSG(maxStep<.01,label+" transition step "+juce::String(maxStep,15));
        // Evidence fingerprint only: no platform-specific golden is imposed.
        // Compare identical declared active renders before/after descriptor fix.
        std::printf("DYNA_NODE rate%.0f quality%d block%d enable%d latency%d error%.17g control%.17g step%.17g dry%d active%016llx\n",
            rate,quality,hostBlock,int(enabling),latency,error,controlError,maxStep,int(exactDry),
            static_cast<unsigned long long>(activeHash));
    }
}
