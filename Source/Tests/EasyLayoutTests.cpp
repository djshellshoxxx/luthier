/*  Easy mode's layout and its tone strip: gui-integration.md 3. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/EasyPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    template <typename T>
    T* findOne (juce::Component& root)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                return match;
            if (auto* deeper = findOne<T> (*child))
                return deeper;
        }
        return nullptr;
    }

    /** A plucked chord through the engine; returns the stereo output. */
    juce::AudioBuffer<float> pluck (LuthierEngine& engine)
    {
        engine.reset();
        juce::AudioBuffer<float> out (2, 512 * 24), block (2, 512);

        for (int b = 0; b < 24; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                for (int note : { 40, 47, 52 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            block.clear();
            engine.processBlock (block, midi);
            out.copyFrom (0, b * 512, block, 0, 0, 512);
            out.copyFrom (1, b * 512, block, 1, 0, 512);
        }

        return out;
    }

    float rms (const juce::AudioBuffer<float>& b)
    {
        return 0.5f * (b.getRMSLevel (0, 0, b.getNumSamples()) + b.getRMSLevel (1, 0, b.getNumSamples()));
    }
}

//==============================================================================
LUTHIER_TEST (EasyLayout, theToneStripIsHeard)
{
    // 3.4: input gain, wet/dry and width each change the sound as their names say.
    LuthierEngine engine;
    engine.prepare (48000.0, 512);

    const float plain = rms (pluck (engine));

    engine.setInputGainDb (-18.0);
    const float quieter = rms (pluck (engine));
    engine.setInputGainDb (0.0);
    CHECK_MSG (quieter < plain * 0.8f, "input -18 dB: " + juce::String (quieter) + " against " + juce::String (plain));

    engine.setStereoWidth (0.0);
    const auto mono = pluck (engine);
    float sideMax = 0.0f;
    for (int i = 4096; i < mono.getNumSamples(); ++i)
        sideMax = juce::jmax (sideMax, std::abs (mono.getSample (0, i) - mono.getSample (1, i)));
    engine.setStereoWidth (1.0);
    CHECK_MSG (sideMax < 1.0e-4f, "width 0 left a side signal of " + juce::String (sideMax));

    engine.setOutputMix (0.0);
    const auto dry = pluck (engine);
    engine.setOutputMix (1.0);
    const auto wet = pluck (engine);

    float diff = 0.0f;
    for (int i = 4096; i < dry.getNumSamples(); ++i)
        diff = juce::jmax (diff, std::abs (dry.getSample (0, i) - wet.getSample (0, i)));
    CHECK_MSG (diff > 1.0e-3f, "wet/dry at 0 sounds the same as at 1");
}

LUTHIER_TEST (EasyLayout, theCharacterMacroIsTheCharacterAmount)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::macroCharacter));
    CHECK (p != nullptr);
    p->setValueNotifyingHost (p->convertTo0to1 (0.8f));
    processor.getParameterBridge().applyAllNow();

    CHECK_NEAR (processor.getEngine().getCharacterEngine().getAmount(), 0.8, 1.0e-3);
}

LUTHIER_TEST (EasyLayout, theWindowMatchesSection3)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto* easy = findOne<EasyPanel> (*editor);
    CHECK (easy != nullptr);

    if (easy == nullptr)
        return;

    // 3.2: the rig strip down the right, 280 points wide.
    CHECK_MSG (easy->getRigArea().getWidth() == EasyPanel::kRigWidth, "rig strip is " + juce::String (easy->getRigArea().getWidth()) + " wide");
    CHECK (easy->getRigArea().getX() > easy->getPlayingArea().getRight());

    // 3.1 - 3.5: guitar on top, then playing, tone, rhythm, top to bottom.
    CHECK (easy->getGuitar().getBottom() <= easy->getPlayingArea().getY());
    CHECK (easy->getPlayingArea().getBottom() <= easy->getToneArea().getY());
    CHECK (easy->getToneArea().getBottom() <= easy->getRhythmArea().getY());
    CHECK (easy->getGuitar().getHeight() > easy->getPlayingArea().getHeight());

    // Every labelled control 3.2 - 3.5 names is on screen.
    juce::StringArray labels;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (child->isVisible())
                if (auto* knob = dynamic_cast<LuthierKnob*> (child))
                    labels.add (knob->getParameterId());
            walk (*child);
        }
    };
    walk (*easy);

    for (auto id : { ParamIDs::guitarVolume, ParamIDs::guitarTone, ParamIDs::ampGain, ParamIDs::ampMaster,
                     ParamIDs::micBlend, ParamIDs::roomBlend, ParamIDs::macroHumanize, ParamIDs::macroCharacter,
                     ParamIDs::inputGain, ParamIDs::masterGain, ParamIDs::outputMix, ParamIDs::stereoWidth })
        CHECK_MSG (labels.contains (id), juce::String ("Easy mode has no ") + id + " control");

    // For a person to look at.
    juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (image);
        editor->paintEntireComponent (g, true);
    }

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
    dir.createDirectory();
    const auto file = dir.getChildFile ("_easy.png");
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
}

LUTHIER_TEST (EasyLayout, ampKnobsHaveRoomAtCompactWindowSize)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setVisible (true);
    editor->setSize (1200, 720);

    auto* easy = findOne<EasyPanel> (*editor);
    CHECK (easy != nullptr);

    if (easy == nullptr)
        return;

    auto* ampFace = findOne<AmpFacePanel> (*easy);
    CHECK (ampFace != nullptr);

    if (ampFace == nullptr)
        return;

    const auto face = ampFace->getFaceLayout();
    CHECK (face.knobRows == 2);

    for (const auto& knob : face.knobs)
        CHECK_MSG (knob.getWidth() >= 40.0f && knob.getHeight() >= 40.0f,
                   "amp knob face area is " + juce::String (knob.getWidth()) + " x " + juce::String (knob.getHeight()));
}
