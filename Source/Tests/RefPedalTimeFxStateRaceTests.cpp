#include "TestFramework.h"
#include "../ReferencePedals/Generic/Delay.h"
#include "../ReferencePedals/Models/boss_dd3t.h"
#include <thread>
#include <atomic>
#include <array>
namespace {
template<class P> void concurrentSave(luthier::tests::TestContext& ctx,int trigger,bool dd3)
{
    P pedal;pedal.prepare(48000,1024,1);std::atomic<bool> done{false};
    std::thread audio([&]
    {
        std::array<double,1024> l{},r{};
        for(int tap=0;tap<64;++tap)
        {
            pedal.setParam(trigger,1);pedal.setParam(trigger,0);
            for(int at=0;at<7200*(1+tap%5);at+=1024)pedal.process(l.data(),r.data(),std::min(1024,7200*(1+tap%5)-at));
        }
        done.store(true,std::memory_order_release);
    });
    bool valid=true;int captures=0;juce::var state;
    do
    {
        pedal.saveExtra(state);const double interval=double(state.getProperty(dd3?"tapIntervalMs":"tapMs",0.));
        valid=valid && std::isfinite(interval) && interval>=60 && interval<=2000;
        if(dd3)
        {
            const double bpm=double(state.getProperty("tempoBpm",0.));
            const bool tapped=bool(state.getProperty("hasTapped",false));
            valid=valid && bpm==60000/interval && (tapped || interval==500);
        }
        ++captures;
    }while(!done.load(std::memory_order_acquire));
    audio.join();CHECK(valid);CHECK(captures>0);
}
}
LUTHIER_TEST(RefPedalTimeFxStateRace, messageSaveReadsCoherentAudioTapSnapshots)
{
    concurrentSave<luthier::reference::generic::Delay>(ctx,13,false);
    concurrentSave<luthier::reference::models::BossDd3t>(ctx,6,true);
}
LUTHIER_TEST(RefPedalTimeFxStateRace, publishedTapRoundTripPreservesDoublePrecision)
{
    using namespace luthier::reference;
    const double interval=std::nextafter(333.3333333333333,400.);
    juce::var input=new juce::DynamicObject();input.getDynamicObject()->setProperty("tapMs",interval);
    generic::Delay delay;delay.loadExtra(input);juce::var saved;delay.saveExtra(saved);
    CHECK(double(saved.getProperty("tapMs",0.))==interval);
    input.getDynamicObject()->setProperty("tapIntervalMs",interval);
    for(bool tapped:{false,true})
    {
        input.getDynamicObject()->setProperty("hasTapped",tapped);models::BossDd3t dd;dd.loadExtra(input);dd.saveExtra(saved);
        CHECK(double(saved.getProperty("tapIntervalMs",0.))==interval);
        CHECK(bool(saved.getProperty("hasTapped",false))==tapped);
        CHECK(double(saved.getProperty("tempoBpm",0.))==60000/interval);
    }
}
