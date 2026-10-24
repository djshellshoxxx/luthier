// OWNER: WP5
#include "TestFramework.h"
#include "../ReferencePedals/Midi/MidiRouter.h"
#include "../ReferencePedals/Midi/MidiMap.h"
#include <memory>
using namespace luthier::reference;
using namespace luthier::reference::midi;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
namespace
{
void activityFlood(juce::MidiBuffer& input)
{
    for (int i=0;i<512;++i)
        input.addEvent(juce::MidiMessage::controllerEvent(1,20,i%128),0);
}
std::unique_ptr<MapSnap> feedbackMap(TempoSource source)
{
    auto map=std::make_unique<MapSnap>(); map->tempoSource=source;
    Binding binding; binding.srcNum=20; binding.target=0; map->add(binding); return map;
}
}
LUTHIER_TEST(RefPedalMidiFeedback, hiddenUiTempoConvergesDuringContinuedActivityFlood)
{
    MidiRouter router; router.prepare(48000,64); router.publishMap(feedbackMap(TempoSource::Host));
    HostTransport host; host.valid=true; host.playing=true; host.bpm=120;
    juce::MidiBuffer input; activityFlood(input); RouteOut out;
    router.process(input,host,0,64,out);
    router.process(input,host,64,64,out);
    CHECK(router.droppedCount()>0);
    host.bpm=180;
    router.process(input,host,128,64,out);
    router.process(input,host,3000,64,out);
    UiEv events[512]; CHECK(router.drainUi(events,512)>0); // hidden root reopens
    const auto before=luthier::tests::allocationsOnThisThread();
    router.process(input,host,3064,64,out); // activity continues while visible
    const auto after=luthier::tests::allocationsOnThisThread(); CHECK(before==after);
    int n=router.drainUi(events,512); bool tempo=false,overload=false;
    for(int i=0;i<n;++i)
    {
        tempo |= events[i].kind==UiEvKind::TempoChanged && std::abs(events[i].value-180.f)<.01f;
        overload |= events[i].kind==UiEvKind::Overload && events[i].a>0;
    }
    CHECK(tempo); CHECK(overload);
    input.clear(); router.process(input,host,3128,64,out);
    n=router.drainUi(events,512); bool duplicate=false;
    for(int i=0;i<n;++i) duplicate |= events[i].kind==UiEvKind::TempoChanged;
    CHECK(!duplicate);
}
LUTHIER_TEST(RefPedalMidiFeedback, hiddenUiClockLossRetriesAfterRingDrain)
{
    MidiRouter router; router.prepare(48000,64); router.publishMap(feedbackMap(TempoSource::MidiClock));
    juce::MidiBuffer input; activityFlood(input);
    for(int tick=0;tick<=48;++tick) input.addEvent(juce::MidiMessage::midiClock(),tick*1000);
    RouteOut out; router.process(input,{},0,49000,out); CHECK(out.time.clockLocked);
    input.clear(); activityFlood(input); router.process(input,{},80000,64,out); CHECK(!out.time.clockLocked);
    UiEv events[512]; CHECK(router.drainUi(events,512)>0);
    input.clear(); router.process(input,{},80064,64,out);
    const int n=router.drainUi(events,512); bool lost=false;
    for(int i=0;i<n;++i) lost |= events[i].kind==UiEvKind::ClockLost && events[i].value==1.f;
    CHECK(lost);
    router.process(input,{},80128,64,out); CHECK(router.drainUi(events,512)==0);
}
