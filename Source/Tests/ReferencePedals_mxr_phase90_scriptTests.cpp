#include "TestFramework.h"
#include "../ReferencePedals/Models/mxr_phase90_script.h"
#include "../ReferencePedals/BoardEngine.h"
#include "../ReferencePedals/Routing/RoutingNodes.h"
#include <array>
#include <complex>
using luthier::reference::models::MxrPhase90Script;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
// Actual model/node composition regression, adapted from the retained
// docs/coverage/probes/phase90-bypass-contract.cpp acceptance audit.
// This tests the written fade law, NOT a new interpretation of the separately
// unresolved 24-bit click criterion. Its original measurements remain retained.
LUTHIER_TEST(RefPedalPhase90, oneEqualPowerEnvelopeInDirectAndBoardPaths)
{
    using namespace luthier::reference;
    constexpr double halfPi = 1.57079632679489661923;
    for (double rate : {44100., 48000., 88200., 96000., 192000.})
        for (int hostBlock : {1, 4, 16, 64, 512, 2048})
            for (bool enabling : {false, true})
    {
        MxrPhase90Script direct, active;
        PedalRuntime runtime;
        runtime.pedal = std::make_unique<MxrPhase90Script>();
        auto& model = static_cast<MxrPhase90Script&>(*runtime.pedal);
        for (auto* pedal : {&direct, &active, &model})
        {
            pedal->setEnabled(pedal == &active || !enabling);
            // Remove LFO-history differences from the independent active oracle;
            // retain the real four nonlinear stages and both filter histories.
            pedal->setParam(3, 0); pedal->setParam(5, 0);
            pedal->prepare(rate, 2048, 1);
        }
        runtime.enabled = !enabling;
        runtime.enableMix = enabling ? 0 : 1;
        runtime.bypassSamples = rate * .005;
        for (auto& line : runtime.dry) line.prepare(0);
        Topology topology;
        PedalNodeState state; state.runtime = &runtime;
        ExecCtx execution; execution.topo = &topology; execution.state[0] = &state;
        execution.sampleRate = rate;
        Node node; node.lane = 0; node.slot = 0;
        std::array<std::array<double, kSubBlock>, 2> input{}, d{}, w{}, b{};
        const int event = int(rate / 4), fadeSamples = int(std::ceil(rate * .005));
        const int end = event + fadeSamples + 64;
        double directError = 0, boardError = 0, controlError = 0;
        bool finite = true, exactDry = true;
        int frame = 0;
        while (frame < end)
        {
            if (frame == event)
            {
                direct.setEnabled(enabling); model.setEnabled(enabling);
                runtime.enabled = enabling;
            }
            int n = std::min({kSubBlock, hostBlock - frame % hostBlock, end - frame});
            if (frame < event) n = std::min(n, event - frame);
            for (int i = 0; i < n; ++i)
            {
                input[0][i] = .1 * std::sin(2 * halfPi * 2 * 1000 * (frame+i) / rate + .37);
                input[1][i] = .15 * std::cos(2 * halfPi * 2 * 731 * (frame+i) / rate + .13);
                for (int c = 0; c < 2; ++c) d[c][i] = w[c][i] = b[c][i] = input[c][i];
            }
            direct.process(d[0].data(), d[1].data(), n);
            active.process(w[0].data(), w[1].data(), n);
            execution.n = n; execution.bus[0][0] = b[0].data(); execution.bus[0][1] = b[1].data();
            routing::nodePedal(node, execution);
            for (int i = 0; i < n; ++i)
            {
                const int since = frame + i - event;
                const double t = since < 0 ? 0 : std::min(1., (since+1) / (rate * .005));
                const double amount = enabling ? t : 1-t;
                for (int c = 0; c < 2; ++c)
                {
                    const double expected = amount == 0 ? input[c][i] : amount == 1 ? w[c][i]
                        : input[c][i] * std::cos(amount * halfPi) + w[c][i] * std::sin(amount * halfPi);
                    directError = std::max(directError, std::abs(d[c][i] - expected));
                    boardError = std::max(boardError, std::abs(b[c][i] - expected));
                    finite &= std::isfinite(d[c][i]) && std::isfinite(b[c][i]);
                    if (!enabling && since >= fadeSamples + 1)
                        exactDry &= d[c][i] == input[c][i] && b[c][i] == input[c][i];
                }
            }
            const int since = frame + n - 1 - event;
            const double t = since < 0 ? 0 : std::min(1., (since+1) / (rate * .005));
            controlError = std::max(controlError, std::abs(runtime.enableMix - (enabling ? t : 1-t)));
            frame += n;
        }
        const auto label = juce::String(rate) + "/" + juce::String(hostBlock) + (enabling ? "/on" : "/off");
        CHECK_MSG(directError < 1.e-12, label + " direct error " + juce::String(directError, 15));
        CHECK_MSG(boardError < 1.e-12, label + " node error " + juce::String(boardError, 15));
        CHECK_MSG(controlError < 1.e-12, label + " linear control " + juce::String(controlError, 15));
        CHECK(finite); CHECK(exactDry);
    }
}
LUTHIER_TEST(RefPedalPhase90, nodeReversalsKeepOneEnvelopeAndSlotTrims)
{
    using namespace luthier::reference;
    constexpr double halfPi = 1.57079632679489661923;
    for (double rate : {44100.1, 48000., 192000.}) for (int hostBlock : {1, 64, 2048})
        for (double fadeSeconds : {.0025, .005, .01})
    {
        PedalRuntime runtime;
        runtime.pedal = std::make_unique<MxrPhase90Script>();
        auto& model = static_cast<MxrPhase90Script&>(*runtime.pedal);
        MxrPhase90Script active;
        for (auto* p : {&model, &active})
        {
            p->setEnabled(p == &active); p->setParam(3, 0); p->setParam(5, 0);
            p->prepare(rate, 2048, 1);
        }
        runtime.enabled = false; runtime.enableMix = 0; runtime.bypassSamples = rate * fadeSeconds;
        runtime.inputGain = .5f; runtime.outputGain = 2.f;
        for (auto& line : runtime.dry) line.prepare(0);
        Topology topology;
        PedalNodeState state; state.runtime = &runtime;
        ExecCtx execution; execution.topo = &topology; execution.state[0] = &state;
        execution.sampleRate = rate;
        Node node; node.lane = node.slot = 0;
        std::array<std::array<double, kSubBlock>, 2> input{}, wet{}, board{};
        const int events[]{17, 51, 83, 312};
        const int end = 312 + int(std::ceil(rate * fadeSeconds)) + 64;
        int eventIndex = 0, frame = 0;
        double position = 0, error = 0, controlError = 0;
        bool exactDry = true, finite = true;
        const auto allocations = luthier::tests::allocationsOnThisThread();
        while (frame < end)
        {
            if (eventIndex < 4 && frame == events[eventIndex])
            {
                runtime.enabled = (eventIndex++ % 2) == 0;
                model.setEnabled(runtime.enabled);
            }
            int n = std::min({kSubBlock, hostBlock - frame % hostBlock, end-frame});
            if (eventIndex < 4) n = std::min(n, events[eventIndex]-frame);
            for (int i = 0; i < n; ++i) for (int c = 0; c < 2; ++c)
            {
                input[c][i] = .1 * std::sin((frame+i) * .13 + c * .7);
                board[c][i] = input[c][i]; wet[c][i] = input[c][i] * runtime.inputGain;
            }
            active.process(wet[0].data(), wet[1].data(), n);
            execution.n = n; execution.bus[0][0] = board[0].data(); execution.bus[0][1] = board[1].data();
            routing::nodePedal(node, execution);
            for (int i = 0; i < n; ++i)
            {
                position = std::clamp(position + (runtime.enabled ? 1 : -1) / (rate * fadeSeconds), 0., 1.);
                for (int c = 0; c < 2; ++c)
                {
                    const double dry = input[c][i], processed = wet[c][i] * runtime.outputGain;
                    const double expected = position <= 0 ? dry : position >= 1 ? processed
                        : dry * std::cos(position * halfPi) + processed * std::sin(position * halfPi);
                    error = std::max(error, std::abs(board[c][i] - expected));
                    finite &= std::isfinite(board[c][i]);
                    if (frame+i > 312 + int(std::ceil(rate * fadeSeconds))) exactDry &= board[c][i] == dry;
                }
            }
            controlError = std::max(controlError, std::abs(runtime.enableMix-position));
            frame += n;
        }
        CHECK(luthier::tests::allocationsOnThisThread() == allocations);
        CHECK_NEAR(error, 0, 1.e-12); CHECK_NEAR(controlError, 0, 1.e-12);
        CHECK(finite); CHECK(exactDry);
    }
}
LUTHIER_TEST(RefPedalPhase90, otherModelsRetainLinearAndResidualTailPaths)
{
    using namespace luthier::reference;
    class ExistingPedal final : public IPedal
    {
    public:
        PedalDesc description{"test.bypass", "test_bypass_", 1, "Bypass fixture", "Test", nullptr,
            ChannelMode::Stereo, BypassMode::True, 0, nullptr, 0, false, false, false, 0};
        int ordinary = 0, board = 0, residual = 0;
        const PedalDesc& desc() const noexcept override { return description; }
        void prepare(double, int, int) override {}
        void reset() noexcept override {}
        void setParam(int, float) noexcept override {}
        float getParam(int) const noexcept override { return 0; }
        void process(double* l, double* r, int n) noexcept override
        { ++ordinary; for (int i=0; i<n; ++i) {l[i] *= -.5; r[i] *= -.5;} }
        void processForBoard(double*, double*, int) noexcept override { ++board; }
        bool processWithWetResidual(double* l, double* r, double* wl, double* wr, int n) noexcept override
        { ++residual; process(l,r,n); std::fill_n(wl,n,.01); std::fill_n(wr,n,-.02); return true; }
        void processBypass(double* l, double* r, int n) noexcept override
        { for (int i=0; i<n; ++i) { l[i] += .03; r[i] -= .04; } }
    };
    for (int kind = 0; kind < 4; ++kind)
    {
        const auto mode = kind == 0 ? BypassMode::True : kind == 1 ? BypassMode::DSP : BypassMode::Buffered;
        PedalRuntime runtime; runtime.pedal = std::make_unique<ExistingPedal>();
        auto& pedal = static_cast<ExistingPedal&>(*runtime.pedal);
        CHECK(pedal.desc().bypassFade == BypassFade::Linear);
        pedal.description.bypass = mode;
        const bool tails = kind == 1 || kind == 2;
        pedal.description.hasTails = tails;
        // Even an opted-in descriptor cannot change an existing tail contract.
        if (tails) pedal.description.bypassFade = BypassFade::EqualPower;
        for (auto& line : runtime.dry) line.prepare(0);
        runtime.enabled = false; runtime.enableMix = .5; runtime.bypassSamples = 240;
        runtime.inputGain = .5f; runtime.outputGain = 2.f;
        Topology topology; PedalNodeState state; state.runtime = &runtime;
        ExecCtx execution; execution.topo = &topology; execution.state[0] = &state; execution.n = 3;
        double l[]{.1,.1,.1}, r[]{-.2,-.2,-.2}; execution.bus[0][0]=l; execution.bus[0][1]=r;
        Node node; node.lane = node.slot = 0;
        routing::nodePedal(node, execution);
        CHECK(pedal.ordinary == 1); CHECK(pedal.board == 0); CHECK(pedal.residual == (tails ? 1 : 0));
        for (int i=0; i<3; ++i)
        {
            const double amount = .5 - (i+1)/240.;
            const double dryL = .1 + (mode == BypassMode::Buffered ? .03 : 0) + (tails ? .02 : 0);
            const double dryR = -.2 - (mode == BypassMode::Buffered ? .04 : 0) - (tails ? .04 : 0);
            CHECK_NEAR(l[i], dryL + amount*(-.05-dryL), 1.e-15);
            CHECK_NEAR(r[i], dryR + amount*(.1-dryR), 1.e-15);
        }
    }
}
LUTHIER_TEST(RefPedalPhase90, bothRenderEntriesKeepFreeRunningClockFrozenWhenDisabled)
{
    // Sync/transport phase writes are a separate retained acceptance gap.
    // This protects the existing free-running freeze in both render entries.
    for (bool boardOwned : {false, true})
    {
        MxrPhase90Script model; model.setEnabled(true); model.prepare(48000, 64, 1);
        std::array<double,64> l{},r{};
        const auto render = [&] {
            if (boardOwned) model.processForBoard(l.data(),r.data(),64);
            else model.process(l.data(),r.data(),64);
        };
        render(); CHECK(model.modulationPhase() > 0);
        const double phase = model.modulationPhase();
        model.setEnabled(false);
        for (int i=0; i<8; ++i) render();
        CHECK(model.modulationPhase() == phase);
        model.setEnabled(true); render(); CHECK(model.modulationPhase() > phase);
    }
}
LUTHIER_TEST(RefPedalPhase90, revisedTwoAnchorStaticNotchesAndBypass)
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        MxrPhase90Script p;p.setEnabled(true);p.setParam(3,0);p.setParam(5,0);p.prepare(sr,1,1);
        constexpr double pi=3.14159265358979323846;const double center=330*std::sqrt(10.);
        double real[2]{},imaginary[2]{};
        double frequency[]{center*std::tan(pi/8),center*std::tan(3*pi/8)};
        for(int i=0;i<int(sr*1.5);++i)
        {
            double l=i==0?.001:0,r=l;p.process(&l,&r,1);
            for(int k=0;k<2;++k){real[k]+=l*std::cos(2*pi*frequency[k]*i/sr);imaginary[k]-=l*std::sin(2*pi*frequency[k]*i/sr);}
        }
        for(int k=0;k<2;++k)CHECK_MSG(std::hypot(real[k],imaginary[k])<1e-7,juce::String(std::hypot(real[k],imaginary[k]),12));
        p.setEnabled(false);for(int i=0;i<int(sr*.01);++i){double l=.1,r=-.1;p.process(&l,&r,1);}
        double l=.125,r=-.25;p.process(&l,&r,1);CHECK_NEAR(l,.125,1e-15);CHECK_NEAR(r,-.25,1e-15);
    }
}
LUTHIER_TEST(RefPedalPhase90, originalLiteralBilinearOracleRetained)
{
    // Historical literal formula remains an independent executable oracle.
    // E58 deliberately changes this transfer to restore the specified analog
    // notch anchors; it does not assert equality to the old digital response.
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double fc:{330.,330*std::sqrt(10.),3300.})
    {
        const double g=std::tan(pi*fc/sr),a=(g-1)/(g+1);
        for(double tangent:{std::tan(pi/8),std::tan(3*pi/8)})
        {
            const double oldNotch=sr/pi*std::atan(g*tangent);
            const auto z=std::exp(std::complex<double>(0,-2*pi*oldNotch/sr));
            const auto stage=(a+z)/(1.+a*z);
            CHECK(std::abs(.5*(1.+stage*stage*stage*stage))<1e-12);
        }
        if(fc==3300 && sr<=48000)
        {
            const double oldHigh=sr/pi*std::atan(g*std::tan(3*pi/8));
            CHECK(std::abs(oldHigh/(fc*std::tan(3*pi/8))-1)>.03);
        }
    }
}
LUTHIER_TEST(RefPedalPhase90, revisedSweepEndpointsAndZeroExplicitDelay)
{
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double position:{0.,.5,1.})
    {
        MxrPhase90Script p;p.setEnabled(true);p.setParam(5,0);p.setParam(6,1);p.prepare(sr,1,1);
        luthier::reference::TimeInfo t;t.valid=t.playing=true;t.bpm=120;t.ppq=position*.65;
        for(int i=0;i<int(sr);++i){double l=0,r=0;p.setTime(t);p.process(&l,&r,1);}
        const double fc=330*std::pow(10.,position),frequencies[]{fc*std::tan(pi/8),fc*std::tan(3*pi/8)};
        std::complex<double> bins[2]{},phase[]{1.,1.},step[]{std::exp(std::complex<double>(0,-2*pi*frequencies[0]/sr)),std::exp(std::complex<double>(0,-2*pi*frequencies[1]/sr))};
        for(int i=0;i<int(sr*2);++i)
        {
            double l=i==0?1e-6:0,r=l;p.setTime(t);p.process(&l,&r,1);
            if(i==0)CHECK(std::abs(l)>1e-8); // Direct IIR response; no hidden buffering.
            for(int k=0;k<2;++k){bins[k]+=l*phase[k];phase[k]*=step[k];}
        }
        CHECK(p.getLatencySamples()==0);
        for(auto bin:bins)CHECK(std::abs(bin)/1e-6<1e-7);
    }
}
LUTHIER_TEST(RefPedalPhase90, allRateBlockExtremesAndStereoPhase)
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        MxrPhase90Script a,b;
        for(auto* p:{&a,&b}){p->setEnabled(true);p->setParam(2,1);p->setParam(5,1);p->setParam(8,1);p->prepare(sr,2048,58);}
        int frame=0;std::array<double,2048> l{},r{},x{},y{};
        for(int n:{1,4,16,64,512,2048})
        {
            for(int i=0;i<n;++i,++frame){l[i]=x[i]=.1*std::sin(frame*.1);r[i]=y[i]=.1*std::cos(frame*.13);}
            a.process(l.data(),r.data(),n);for(int i=0;i<n;++i)b.process(x.data()+i,y.data()+i,1);
            for(int i=0;i<n;++i){CHECK(l[i]==x[i]);CHECK(r[i]==y[i]);CHECK(std::isfinite(l[i]));CHECK(std::isfinite(r[i]));}
        }
        a.reset();double peak=0;
        for(int i=0;i<int(sr*.1);++i){double l0=0,r0=0;a.process(&l0,&r0,1);peak=std::max(peak,std::max(std::abs(l0),std::abs(r0)));}
        CHECK(peak==0);
    }
}
LUTHIER_TEST(RefPedalPhase90, blockPartitionAndSeededReset)
{
    MxrPhase90Script a,b;a.setEnabled(true);b.setEnabled(true);a.prepare(48000,2048,1);b.prepare(48000,2048,1);
    std::array<double,2048> l{},r{},x{},y{};
    for(int i=0;i<2048;++i)l[i]=x[i]=.1*std::sin(i*.1),r[i]=y[i]=-l[i];
    a.process(l.data(),r.data(),2048);for(int i=0;i<2048;++i)b.process(x.data()+i,y.data()+i,1);
    CHECK(l==x);CHECK(r==y);
    a.setParam(11,1);b.setParam(11,1);a.reset();b.reset();l.fill(0);r.fill(0);x.fill(0);y.fill(0);
    a.process(l.data(),r.data(),2048);b.process(x.data(),y.data(),2048);CHECK(l==x);CHECK(r==y);
}
LUTHIER_TEST(RefPedalPhase90, lfoRateAndSixtySecondFeedbackStability)
{
    for(float setting:{0.f,1.f})
    {
        MxrPhase90Script p;p.setEnabled(true);p.setParam(0,setting);p.prepare(48000,1,1);double phase=0;int wraps=0;
        for(int i=0;i<48000*4;++i){double l=0,r=0;p.process(&l,&r,1);if(p.modulationPhase()<phase)++wraps;phase=p.modulationPhase();}
        CHECK_NEAR((wraps+phase)/4,.3*std::pow(25,setting),1e-9);
    }
    MxrPhase90Script p;p.setEnabled(true);p.setParam(2,1);p.prepare(48000,2048,1);std::array<double,2048> l{},r{};l[0]=r[0]=1;bool stable=true;
    for(int block=0;block<1407;++block){p.process(l.data(),r.data(),2048);for(int i=0;i<2048;++i)stable=stable && std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i])<4 && std::abs(r[i])<4;l.fill(0);r.fill(0);}
    CHECK(stable);
}
LUTHIER_TEST(RefPedalPhase90, independentPointSixFeedbackStabilityBound)
{
    // Acceptance-only numerical probe. The hardware-facing Block variant stays
    // at its specified0.35; no parameter or model law is changed for this test.
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        double states[4]{},feedback=0,phase=0,triangle=.5,a=0,step=0,peak=0,tail=0;
        const double coefficient=1-std::exp(-1/(sr*.02));bool finite=true;
        for(int i=0;i<int(sr*60);++i)
        {
            const double tri=phase<.65?phase/.65:(1-phase)/.35;
            triangle+=(tri-triangle)*coefficient;
            if((i&7)==0){const double g=std::tan(pi*330*std::pow(10.,triangle)/sr);step=((g-1)/(g+1)-a)/8;}
            a+=step;double x=(i==0?1.:0.)+.6*std::tanh(feedback);
            for(auto& state:states)
            {
                const double y=a*x+state;state=x-a*y;if(std::abs(state)<1e-30)state=0;
                x=y/std::sqrt(1+.04*y*y);
            }
            feedback=x;finite=finite&&std::isfinite(x);peak=std::max(peak,std::abs(x));
            if(i>=int(sr*59))tail+=x*x;
            phase+=16/sr;if(phase>=1)phase-=1;
        }
        CHECK(finite);CHECK(peak<=16);CHECK(tail/sr<1e-12);
    }
}
