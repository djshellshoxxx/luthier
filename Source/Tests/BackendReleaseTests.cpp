#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../Support/BoundedMidi.h"
#include <memory>
#include <atomic>
#include <thread>
using namespace luthier;
namespace
{
struct BackendReleaseRig
{
    std::unique_ptr<LuthierAudioProcessor> processor=std::make_unique<LuthierAudioProcessor>();
    juce::AudioBuffer<float> audio{2,64};
    juce::MidiBuffer midi;
    BackendReleaseRig()
    {
        set(ParamIDs::macroHumanize,0); set(ParamIDs::playingMode,2); // exact per-string channel convention
        processor->getParameterBridge().applyAllNow();
        processor->prepareToPlay(48000,64); processor->reserveMidiOutputBuffer(midi);
    }
    ~BackendReleaseRig() { processor->releaseResources(); }
    void set(const char* id,float raw)
    {
        if(auto* p=processor->getState().getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(raw));
    }
    void block() { audio.clear(); processor->processBlock(audio,midi); midi.clear(); }
    void settle(int count=16) { while(count-->0) block(); }
    int note() const { return processor->getEngine().getStringMidiNote(0); }
};
}
LUTHIER_TEST(BackendRelease, immediateReleaseRetriesAfterEventQueueSaturation)
{
    BackendReleaseRig rig;
    rig.processor->playKeyboardNote(72,.8f); rig.settle(); CHECK(rig.note()==72);
    rig.processor->getEngine().getMidiInterpreter().setMinimumNoteDurationMs(0);
    const int before=rig.processor->getEngine().getMidiOverflowDropCount();
    rig.processor->releaseKeyboardNote(72);
    for(int i=0;i<PlayEventQueue::kCapacity+32;++i)
    {
        rig.midi.addEvent(juce::MidiMessage::noteOn(2,61,.7f),0);
        rig.midi.addEvent(juce::MidiMessage::noteOff(2,61),0);
    }
    CHECK(BoundedMidi::bytesUsed(rig.midi)<8192); // byte capacity is not the bottleneck
    rig.block(); CHECK(rig.processor->getEngine().getMidiOverflowDropCount()==before); // documented byte copies did not overflow
    rig.settle(); CHECK(rig.note()<0);
}
LUTHIER_TEST(BackendRelease, minimumDurationReleaseRetriesAfterEventQueueSaturation)
{
    BackendReleaseRig rig;
    rig.processor->prepareToPlay(48000,2048); rig.audio.setSize(2,2048); rig.block();
    rig.processor->getEngine().getMidiInterpreter().setMinimumNoteDurationMs(.1);
    rig.processor->playKeyboardNote(72,.8f); rig.processor->releaseKeyboardNote(72);
    for(int i=0;i<PlayEventQueue::kCapacity+32;++i)
    {
        rig.midi.addEvent(juce::MidiMessage::noteOn(2,61,.7f),i*6);
        rig.midi.addEvent(juce::MidiMessage::noteOff(2,61),i*6+5);
    }
    rig.block(); CHECK_NEAR(rig.processor->getEngine().getMidiInterpreter().getMinimumNoteDurationMs(),.1,1.e-9);
    rig.settle(); CHECK(rig.note()<0);
}
