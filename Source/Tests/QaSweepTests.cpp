/*  SPEC-SWEEP: qa-polish.md checks (QA-48 bypass null, QA-27 tooltips). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/Widgets.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  QA-48: Luthier is an instrument, so "bit-identical to no plugin" means
    silence: a bypassed block with a note playing into it outputs exact zeros
    on the main output, whatever the buffer held. */
LUTHIER_TEST (HostState, bypassOutputsSilence)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const int channels = juce::jmax (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (juce::jmax (2, channels), 256);

    // A chord sounding first, so a leaking tail would show.
    {
        juce::MidiBuffer midi;
        for (int note : { 40, 47, 52 })
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    for (int block = 0; block < 8; ++block)
    {
        // Whatever the host left in the buffer, including on the sidechain.
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 0.25f, buffer.getNumSamples());

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        processor.processBlockBypassed (buffer, midi);

        const int mainOut = juce::jmin (2, processor.getMainBusNumOutputChannels());
        const int mainIn = processor.getMainBusNumInputChannels();

        for (int ch = 0; ch < mainOut; ++ch)
        {
            // A main input (if a layout ever has one) passes through; the
            // instrument itself adds nothing.
            const float expected = ch < mainIn ? 0.25f : 0.0f;

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (buffer.getSample (ch, i) != expected)
                {
                    CHECK_MSG (false, "bypassed channel " + juce::String (ch) + " sample " + juce::String (i)
                                        + " = " + juce::String (buffer.getSample (ch, i)));
                    return;
                }
        }
    }
}

//==============================================================================
/*  QA-27: every visible attached control in the default editor has a tooltip. */
LUTHIER_TEST (GuiReach, everyVisibleAttachedControlHasATooltip)
{
    LuthierAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
        return;

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    std::function<void (juce::Component&, std::vector<juce::Component*>&)> walk
        = [&walk] (juce::Component& root, std::vector<juce::Component*>& out)
    {
        for (auto* c : root.getChildren())
        {
            if (dynamic_cast<LearnTarget*> (c) != nullptr)
                out.push_back (c);

            walk (*c, out);
        }
    };

    std::vector<juce::Component*> targets;
    walk (*editor, targets);

    auto tooltipOf = [] (juce::Component* c) -> juce::String
    {
        for (auto* p = c; p != nullptr; p = p->getParentComponent())
            if (auto* t = dynamic_cast<juce::TooltipClient*> (p))
                if (auto text = t->getTooltip(); text.isNotEmpty())
                    return text;

        return {};
    };

    auto innerOf = [] (juce::Component* c) -> juce::Component*
    {
        if (auto* k = dynamic_cast<LuthierKnob*> (c))   return &k->getSlider();
        if (auto* s = dynamic_cast<LuthierSlider*> (c)) return &s->getSlider();
        if (auto* ch = dynamic_cast<LuthierChoice*> (c)) return &ch->getComboBox();
        if (auto* t = dynamic_cast<LuthierToggle*> (c)) return &t->getButton();
        return c;
    };

    int visible = 0;
    juce::StringArray missing;

    for (auto* c : targets)
    {
        if ((! c->isShowing() && ! c->isVisible())
            || dynamic_cast<LearnTarget*> (c)->getLearnParameterId().isEmpty())   // not attached yet
            continue;

        ++visible;

        if (tooltipOf (innerOf (c)).isEmpty() && tooltipOf (c).isEmpty())
            missing.add (dynamic_cast<LearnTarget*> (c)->getLearnParameterId());
    }

    CHECK (visible > 0);
    CHECK_MSG (missing.isEmpty(), juce::String (missing.size()) + " controls have no tooltip: "
                                    + missing.joinIntoString (", ").substring (0, 400));
}

//==============================================================================
/*  IN-38 (installer.md 8): old files keep loading. A loop saved before MIDI
    events carried class tags loads its events with defaults, and a plain SMF
    with no Luthier chunk imports as the Generic profile. */
#include "../Practice/Looper.h"
#include "../Export/MidiProfiles.h"
#include "../Export/MidiPerformance.h"

LUTHIER_TEST (PracticeLooper, anOldLoopWithoutTagsLoads)
{
    juce::TemporaryFile holder;
    const auto folder = holder.getFile();
    folder.createDirectory();

    // A note-on and a note-off, the way the first looper wrote them: bytes and
    // time only.
    const juce::String json = R"({ "loopLength": 4800,
        "layers": [ { "midi": [ { "t": 0.0, "b": "903C64" }, { "t": 0.05, "b": "803C00" } ] } ] })";
    CHECK (folder.getChildFile ("loop.json").replaceWithText (json));

    Looper looper;
    looper.prepare (48000.0, 10.0);
    CHECK (looper.load (folder));
    CHECK (looper.getLoopLengthSamples() == 4800);
    CHECK (looper.getLayer (0).getMidi().getNumEvents() == 2);

    folder.deleteRecursively();
}

LUTHIER_TEST (MidiImport, aPlainMidiFileLoadsAsGeneric)
{
    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0.0);
    track.addEvent (juce::MidiMessage::noteOff (1, 64), 480.0);
    track.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (480);
    file.addTrack (track);

    juce::MemoryOutputStream out;
    CHECK (file.writeTo (out));

    MidiPerformance performance;
    const auto result = MidiProfiles::importFromMemory (out.getData(), out.getDataSize(), performance, 48000.0);

    CHECK_MSG (result.ok, result.error);
    CHECK (result.detectedProfile == MidiProfile::generic);
}
