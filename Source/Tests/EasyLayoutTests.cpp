/*  Easy mode's layout and its tone strip: gui-integration.md 3. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/EasyPanel.h"
#include "../UI/AmpFacePanel.h"
#include "../Accessibility/Accessibility.h"

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

    // TODO 2h: at the default 1200 x 720 the amp card's six knobs are big enough
    // to use - two rows of three on the face, not one cramped row.
    {
        auto& face = easy->getAmpFace();
        CHECK_MSG (face.getFaceLayout().knobRows == 2, "the amp card's face has " + juce::String (face.getFaceLayout().knobRows) + " knob row(s) at 1200 x 720");

        for (int k = 0; k < faces::numAmpKnobs; ++k)
        {
            auto& slider = face.getKnob ((faces::AmpKnob) k).getSlider();
            const int size = juce::jmin (slider.getWidth(), slider.getHeight());
            CHECK_MSG (size >= 34, "amp card knob " + juce::String (k) + " is " + juce::String (size) + " px at 1200 x 720");
        }

        // The face fills the card below its title row; the model choice sits in that row.
        CHECK (face.getHeight() >= 180);
    }

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

//==============================================================================
/*  TODO 2h: the rig strip's cards take the heights their controls need and the
    amp card takes the rest; a short strip shrinks every card alike and never
    loses one. */
LUTHIER_TEST (EasyLayout, theRigStripCardsShareTheHeightTheyHave)
{
    for (int total : { 380, 500, 606, 800, 1200 })
    {
        const auto h = EasyPanel::cardHeights (total);
        CHECK_MSG (h.circuit + 2 * h.rack + h.amp + h.cab + h.room == total,
                   "the cards do not add up to " + juce::String (total));
        CHECK (h.circuit > 0 && h.rack > 0 && h.amp > 0 && h.cab > 0 && h.room > 0);
        CHECK_MSG (h.amp >= h.circuit && h.amp >= h.cab && h.amp >= h.room && h.amp >= h.rack,
                   "the amp card is not the tallest at " + juce::String (total));
    }

    // At the default window the amp card has two rows of three knobs' worth.
    CHECK (EasyPanel::cardHeights (606).amp >= 220);

    // More height goes mostly to the amp and the guitar.
    const auto a = EasyPanel::cardHeights (606), b = EasyPanel::cardHeights (906);
    CHECK (b.amp - a.amp > b.rack - a.rack);
    CHECK (b.circuit - a.circuit > b.rack - a.rack);
}

//==============================================================================
/*  visual-polish.md 4: the ROOM card's light warms with the wet level and
    reaches further with the size; a dry room, or High contrast, leaves the
    card alone. */
LUTHIER_TEST (EasyLayout, theRoomLightFollowsSizeAndWetLevel)
{
    const PaletteColours saved = Palette::current();
    const bool textured = Palette::textured;
    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::defaultDark), true);

    const juce::Rectangle<float> card (0.0f, 0.0f, 268.0f, 80.0f);

    auto render = [&card] (float size, float wet)
    {
        juce::Image image (juce::Image::ARGB, (int) card.getWidth(), (int) card.getHeight(), true, juce::SoftwareImageType());
        juce::Graphics g (image);
        g.fillAll (Palette::panel);
        EasyPanel::paintRoomLight (g, card, size, wet);
        return image;
    };

    auto warmth = [] (const juce::Image& image, int x0, int x1)
    {
        // How far the pixels have moved from the panel colour, summed over a band.
        double sum = 0.0;
        const auto base = Palette::panel;

        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = x0; x < x1; ++x)
            {
                const auto c = image.getPixelAt (x, y);
                sum += std::abs ((int) c.getRed() - base.getRed()) + std::abs ((int) c.getGreen() - base.getGreen());
            }

        return sum;
    };

    const auto dry = render (0.5f, 0.0f), wet = render (0.5f, 1.0f), damp = render (0.5f, 0.3f);
    CHECK_MSG (warmth (dry, 0, 268) == 0.0, "a dry room lit up");
    CHECK_MSG (warmth (wet, 0, 268) > warmth (damp, 0, 268), "more wet did not warm the card more");
    CHECK (warmth (damp, 0, 268) > 0.0);

    // A bigger room reaches the card's ends; a small one stays in the middle.
    const auto small = render (0.0f, 1.0f), large = render (1.0f, 1.0f);
    CHECK_MSG (warmth (large, 0, 40) > warmth (small, 0, 40), "a large room does not reach further than a small one");

    // High contrast: no gradient at all (visual-polish.md 0.2).
    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::highContrast), false);
    CHECK_MSG (warmth (render (1.0f, 1.0f), 0, 268) == 0.0, "High contrast still has a room light");

    Palette::apply (saved, textured);
}
