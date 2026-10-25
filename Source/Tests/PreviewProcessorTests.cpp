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
