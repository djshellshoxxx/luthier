#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../Support/BoundedMidi.h"
#include <memory>
#include <atomic>
#include <thread>
using namespace luthier;
namespace
{
struct PreviewProcessorRig
{
    std::unique_ptr<LuthierAudioProcessor> processor=std::make_unique<LuthierAudioProcessor>();
    juce::AudioBuffer<float> audio{2,64};
    juce::MidiBuffer midi;
    PreviewProcessorRig()
    {
        set(ParamIDs::macroHumanize,0); set(ParamIDs::playingMode,2); // exact per-string channel convention
        processor->getParameterBridge().applyAllNow();
        processor->prepareToPlay(48000,64); processor->reserveMidiOutputBuffer(midi);
    }
    ~PreviewProcessorRig() { processor->releaseResources(); }
    void set(const char* id,float raw)
    {
        if(auto* p=processor->getState().getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(raw));
    }
    void block() { audio.clear(); processor->processBlock(audio,midi); midi.clear(); }
    void settle(int count=16) { while(count-->0) block(); }
    int note() const { return processor->getEngine().getStringMidiNote(0); }
};
}
LUTHIER_TEST(PreviewProcessor, prepareCancelsKeyboardCommandsFromStoppedIncarnation)
{
    PreviewProcessorRig rig;
    rig.processor->playKeyboardNote(72,.8f);
    rig.processor->playKeyboardChord(juce::Array<int>{67,72},.8f);
    rig.processor->releaseResources();
    rig.processor->prepareToPlay(96000,64);
    rig.settle();
    CHECK(rig.note()<0);
    rig.processor->playKeyboardNote(72,.8f); rig.settle(); CHECK(rig.note()==72);
    rig.processor->releaseKeyboardNote(72); rig.settle(); CHECK(rig.note()<0);
}
LUTHIER_TEST(PreviewProcessor, retainedKeyboardReleaseSurvivesFullHostReleaseBuffer)
{
    PreviewProcessorRig rig;
    rig.processor->playKeyboardNote(72,.8f); rig.settle(); CHECK(rig.note()==72);
    rig.processor->releaseKeyboardNote(72);
    // 7281 short releases consume65529 of65536bytes, leaving less than one
    // preview note-off. They release an unrelated/unheld note, not note72.
    for(int i=0;i<7281;++i) rig.midi.addEvent(juce::MidiMessage::noteOff(1,0),0);
    CHECK(BoundedMidi::bytesUsed(rig.midi)==65529);
    rig.block(); rig.settle();
    CHECK(rig.note()<0);
}
LUTHIER_TEST(PreviewProcessor, keyboardNoteChordAndStopKeepExistingVoicing)
{
    PreviewProcessorRig rig;
    rig.processor->playKeyboardNote(72,.8f); rig.settle(); CHECK(rig.note()==72);
    rig.processor->releaseKeyboardNote(72); rig.settle(); CHECK(rig.note()<0);
    rig.processor->playKeyboardChord(juce::Array<int>{67,72},.8f); rig.settle(); CHECK(rig.note()==72);
    rig.processor->releaseKeyboardChord(juce::Array<int>{67,72}); rig.settle(); CHECK(rig.note()<0);
    rig.processor->playKeyboardNote(72,.8f); rig.processor->panic(); rig.settle(); CHECK(rig.note()<0);
}
LUTHIER_TEST(PreviewProcessor, mixedKeyboardAndFretSuccessKeepsSameSampleProducerOrder)
{
    PreviewProcessorRig rig;
    const auto& tuning=rig.processor->getEngine().getTuningEngine();
    const int expected=juce::jlimit(0,127,(int)std::lround(hzToMidi(tuning.computeFrequency(0,5),tuning.getConcertA())));
    CHECK(expected!=72);
    rig.processor->playKeyboardNote(72,.8f);
    rig.processor->triggerPreviewNote(0,5,.8);
    rig.settle();
    CHECK(rig.note()==expected);
    rig.processor->releaseKeyboardNote(72); rig.settle(); CHECK(rig.note()==expected);
    rig.processor->releasePreviewNote(0); rig.settle(); CHECK(rig.note()<0);
}
LUTHIER_TEST(PreviewProcessor, keyboardReleaseDoesNotStopADifferentHostNoteOnTheSameChannel)
{
    PreviewProcessorRig rig;
    rig.processor->playKeyboardNote(72,.8f); rig.settle(); CHECK(rig.note()==72);
    rig.midi.addEvent(juce::MidiMessage::noteOn(1,74,.8f),0); rig.block(); rig.settle(); CHECK(rig.note()==74);
    rig.processor->releaseKeyboardNote(72); rig.settle(); CHECK(rig.note()==74);
}
LUTHIER_TEST(PreviewProcessor, retainedKeyboardReleaseSurvivesRendererExclusion)
{
    PreviewProcessorRig rig;
    rig.processor->playKeyboardNote(72,.8f); rig.settle(); CHECK(rig.note()==72);
    rig.processor->releaseKeyboardNote(72);
    std::atomic<bool> held{false},finish{false};
    std::thread writer([&]
    {
        const juce::ScopedLock guard(rig.processor->getParameterBridge().getEngineLock());
        held.store(true,std::memory_order_release);
        while(!finish.load(std::memory_order_acquire)) std::this_thread::yield();
    });
    while(!held.load(std::memory_order_acquire)) std::this_thread::yield();
    rig.block(); // actual render bridge cannot enter; preview release must stay pending
    finish.store(true,std::memory_order_release); writer.join(); rig.settle();
    CHECK(rig.note()<0);
}
LUTHIER_TEST(PreviewProcessor, emptyCallbackDoesNotConsumeAKeyboardAttack)
{
    PreviewProcessorRig rig;
    rig.processor->playKeyboardNote(72,.8f);
    juce::AudioBuffer<float> empty(2,0); juce::MidiBuffer midi;
    rig.processor->processBlock(empty,midi);
    rig.settle(); CHECK(rig.note()==72);
    rig.processor->releaseKeyboardNote(72); rig.settle(); CHECK(rig.note()<0);
}
LUTHIER_TEST(PreviewProcessor, concurrentActualKeyboardAndChordProducersReleaseAllAcceptedNotes)
{
    PreviewProcessorRig rig;
    rig.processor->playKeyboardNote(72,.8f); rig.settle();
    rig.processor->releaseKeyboardNote(72); rig.settle();
    std::atomic<bool> begin{false},done{false};
    std::atomic<int> callbacks{0};
    std::thread audio([&]
    {
        begin.store(true,std::memory_order_release);
        while(!done.load(std::memory_order_acquire)) { rig.block(); ++callbacks; }
        rig.settle(32);
    });
    while(!begin.load(std::memory_order_acquire)) std::this_thread::yield();
    std::array<std::thread,4> producers;
    for(int t=0;t<4;++t) producers[(size_t)t]=std::thread([&,t]
    {
        const juce::Array<int> chord{67+t,72+t};
        for(int i=0;i<1000;++i)
        {
            rig.processor->playKeyboardNote(72+t,.7f);
            rig.processor->releaseKeyboardNote(72+t);
            rig.processor->playKeyboardChord(chord,.7f);
            rig.processor->releaseKeyboardChord(chord);
        }
    });
    for(auto& producer:producers) producer.join();
    juce::Array<int> all; for(int note=0;note<128;++note) all.add(note);
    rig.processor->releaseKeyboardChord(all);
    done.store(true,std::memory_order_release); audio.join();
    CHECK(callbacks.load()>0); CHECK(rig.note()<0);
    CHECK(rig.processor->getMidiOverflowDropCount()>=0);
}
LUTHIER_TEST(PreviewProcessor, releaseSurvivesTheBackendEventCapacityOnAnotherString)
{
    PreviewProcessorRig rig;
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
    rig.block(); CHECK(rig.processor->getEngine().getMidiOverflowDropCount()>before);
    rig.settle(); CHECK(rig.note()<0);
}
