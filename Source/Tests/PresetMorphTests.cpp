/*  Preset-to-preset morph (ambiguity-resolutions.md 5, tests 5.3; 8). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"
#include "../UI/Overlays.h"
#include "../Presets/PresetLibrary.h"   // FEAT-BROWSER

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    juce::var factoryState (LuthierAudioProcessor& processor, const juce::String& name)
    {
        for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
            if (juce::String (FactoryPresets::getPreset (i).name) == name)
            {
                processor.getPresetManager().fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (i), processor));
                break;
            }

        if (auto* p = processor.getState().getParameter (ParamIDs::macroHumanize))
            p->setValueNotifyingHost (0.0f);

        return processor.getPresetManager().toVar (name);
    }

    /** A held chord for `seconds`; `perBlock` runs before each block. */
    std::vector<float> render (LuthierAudioProcessor& processor, double seconds,
                               std::function<void (int block)> perBlock = {})
    {
        processor.prepareToPlay (kSr, kBlock);
        processor.getEngine().reset();

        std::vector<float> out;
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);

        for (int b = 0; b < (int) (seconds * kSr / kBlock); ++b)
        {
            if (perBlock)
                perBlock (b);

            buffer.clear();
            juce::MidiBuffer midi;

            if (b == 0)
                for (int note : { 45, 52, 57, 61 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 96), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    }

    double nullDb (const std::vector<float>& a, const std::vector<float>& b)
    {
        double sum = 0.0;

        for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
            sum += ((double) a[i] - b[i]) * ((double) a[i] - b[i]);

        return juce::Decibels::gainToDecibels (std::sqrt (sum / (double) juce::jmax ((size_t) 1, a.size())), -400.0);
    }

    float steepestStep (const std::vector<float>& v)
    {
        float steepest = 0.0f;

        for (size_t i = 1; i < v.size(); ++i)
            steepest = juce::jmax (steepest, std::abs (v[i] - v[i - 1]));

        return steepest;
    }

    void setPosition (LuthierAudioProcessor& processor, float position)
    {
        if (auto* p = processor.getState().getParameter (ParamIDs::presetMorphPosition))
            p->setValueNotifyingHost (position);

        processor.updatePresetMorph();
    }

    /** A morph between two factory presets that differ in amp model. */
    struct Morph
    {
        Morph()
        {
            a = factoryState (processor, "Clean Double-Cut Funk");
            b = factoryState (processor, "Single-Cut Crunch");

            processor.getPresetMorph().setEnabled (true);
            processor.getPresetMorph().setSlot (PresetMorph::slotA, a, "A");
            processor.getPresetMorph().setSlot (PresetMorph::slotB, b, "B");
        }

        LuthierAudioProcessor processor;
        juce::var a, b;
    };
}

//==============================================================================
LUTHIER_TEST (PresetMorph, theEndsAreThePresetsThemselves)
{
    for (const float end : { 0.0f, 1.0f })
    {
        Morph morph;
        setPosition (morph.processor, end);
        const auto morphed = render (morph.processor, 1.5);

        LuthierAudioProcessor alone;
        alone.getPresetManager().fromVar (end < 0.5f ? morph.a : morph.b);
        alone.getParameterBridge().applyAllNow();
        const auto reference = render (alone, 1.5);

        const double n = nullDb (morphed, reference);
        CHECK_MSG (n < -100.0, "morph " + juce::String (end, 1) + " is not preset " + (end < 0.5f ? "A" : "B")
                                 + " alone: null " + juce::String (n, 1) + " dBFS");
    }
}

LUTHIER_TEST (PresetMorph, theMidpointSwitchesDiscretesAndHalvesTheRest)
{
    Morph morph;
    setPosition (morph.processor, 0.5f);

    auto* aParams = morph.a.getProperty ("parameters", {}).getDynamicObject();
    auto* bParams = morph.b.getProperty ("parameters", {}).getDynamicObject();
    CHECK (aParams != nullptr && bParams != nullptr);

    if (aParams == nullptr || bParams == nullptr)
        return;

    int continuousChecked = 0, discreteChecked = 0;

    for (auto* p : morph.processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);

        if (withId == nullptr || withId->paramID == ParamIDs::presetMorphPosition)
            continue;

        const juce::Identifier id (withId->paramID);

        if (! aParams->hasProperty (id) || ! bParams->hasProperty (id))
            continue;

        const double a = aParams->getProperty (id), b = bParams->getProperty (id);

        if (std::abs (a - b) < 1.0e-6)
            continue;

        if (SnapshotBank::isDiscrete (*p))
        {
            ++discreteChecked;
            CHECK_MSG (std::abs (withId->getValue() - b) < 1.0e-5, withId->paramID + " has not switched to B at 0.5");
        }
        else
        {
            ++continuousChecked;
            CHECK_MSG (std::abs (withId->getValue() - 0.5 * (a + b)) < 1.0e-5,
                       withId->paramID + " is " + juce::String (withId->getValue()) + ", not the midpoint "
                         + juce::String (0.5 * (a + b)));
        }
    }

    CHECK_MSG (discreteChecked > 0 && continuousChecked > 0, "the two presets did not differ enough to test");
}

LUTHIER_TEST (PresetMorph, aFourSecondSweepDoesNotClick)
{
    // On the heap: a processor is large, and three on one stack frame overflow it.
    auto owned = std::make_unique<Morph>();
    auto& morph = *owned;

    // Automation 0 -> 1 over 4 s, applied at the processor timer's rate.
    const int blocks = (int) (4.0 * kSr / kBlock);
    const auto swept = render (morph.processor, 4.5, [&morph, blocks] (int b)
    {
        if (b % 6 == 0)
            setPosition (morph.processor, juce::jlimit (0.0f, 1.0f, (float) b / (float) blocks));
    });

    auto aAlone = std::make_unique<LuthierAudioProcessor>();
    auto bAlone = std::make_unique<LuthierAudioProcessor>();
    aAlone->getPresetManager().fromVar (morph.a);
    bAlone->getPresetManager().fromVar (morph.b);
    const auto ra = render (*aAlone, 4.5), rb = render (*bAlone, 4.5);

    const float worst = juce::jmax (steepestStep (ra), steepestStep (rb));
    CHECK_MSG (steepestStep (swept) <= 2.0f * worst + 1.0e-4f,
               "the sweep has a step of " + juce::String (steepestStep (swept), 4)
                 + " where either preset alone peaks at " + juce::String (worst, 4));
}

LUTHIER_TEST (PresetMorph, aSnapshotRecallCancelsTheMorph)
{
    Morph morph;
    setPosition (morph.processor, 0.3f);

    auto& bank = morph.processor.getSnapshots();
    bank.capture (0, "Here");
    CHECK (morph.processor.getPresetMorph().isEnabled());

    morph.processor.recallSnapshot (0);
    CHECK_MSG (! morph.processor.getPresetMorph().isEnabled(), "a snapshot recall left the preset morph running");

    // And the slider no longer moves the sound.
    const auto before = morph.processor.getPresetManager().toVar ("x");
    setPosition (morph.processor, 0.9f);
    const auto after = morph.processor.getPresetManager().toVar ("x");
    CHECK (juce::JSON::toString (before.getProperty ("parameters", {}))
             == juce::JSON::toString (after.getProperty ("parameters", {})));
}

LUTHIER_TEST (PresetMorph, thePositionIsNotPartOfAPreset)
{
    LuthierAudioProcessor processor;
    const auto state = processor.getPresetManager().toVar ("x");
    CHECK (! state.getProperty ("parameters", {}).getDynamicObject()->hasProperty (ParamIDs::presetMorphPosition));
}

namespace
{
    template <typename T>
    void collectAll (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collectAll<T> (*child, found);
        }
    }

    juce::Button* buttonStartingWith (juce::Component& root, const juce::String& text)
    {
        juce::Array<juce::Button*> buttons;
        collectAll<juce::Button> (root, buttons);

        for (auto* button : buttons)
            if (button->getButtonText().startsWith (text))
                return button;

        return nullptr;
    }

    /** The user's click, synchronously (triggerClick posts a message). */
    void press (juce::Button& button)
    {
        if (button.getClickingTogglesState())
            button.setToggleState (! button.getToggleState(), juce::dontSendNotification);

        if (button.onClick)
            button.onClick();
    }
}

/*  5.2: the browser's Morph toggle shows the slots and the slider; a load goes
    into the selected slot; the slider is the automatable parameter. */
LUTHIER_TEST (PresetMorph, theBrowserMorphRowFillsTheSelectedSlot)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    PresetBrowserPanel panel (*processor);
    panel.setSize (780, 560);
    panel.overlayShown();

    processor->getPresetLibrary().refreshSynchronously();   // FEAT-BROWSER: the rows are index entries
    panel.refilter();

    // FEAT-BROWSER: the sidebar's chips ("Acoustic", "Bend"...) share the slot
    // buttons' initials; the slots are the radio pair "A" / "B".
    const auto slotButton = [&panel] (const juce::String& letter) -> juce::Button*
    {
        juce::Array<juce::Button*> buttons;
        collectAll<juce::Button> (panel, buttons);

        for (auto* button : buttons)
            if (button->getRadioGroupId() != 0 && (button->getButtonText() == letter || button->getButtonText().startsWith (letter + ":")))
                return button;

        return nullptr;
    };

    auto* toggle = buttonStartingWith (panel, "Morph");
    auto* slotA = slotButton ("A");
    auto* slotB = slotButton ("B");
    auto* load = buttonStartingWith (panel, "Load");

    juce::Array<juce::Slider*> sliders;
    collectAll<juce::Slider> (panel, sliders);
    sliders.removeAllInstancesOf (&panel.getVolumeSlider());   // FEAT-BROWSER: the preview volume is not the morph

    CHECK (toggle != nullptr && slotA != nullptr && slotB != nullptr && load != nullptr);
    CHECK (sliders.size() == 1);

    if (toggle == nullptr || slotA == nullptr || slotB == nullptr || load == nullptr || sliders.size() != 1)
        return;

    auto* slider = sliders.getFirst();
    CHECK_MSG (! slotA->isVisible() && ! slider->isVisible(), "the morph row shows before Morph is on");

    press (*toggle);
    CHECK (processor->getPresetMorph().isEnabled());
    CHECK_MSG (slotA->isVisible() && slotB->isVisible() && slider->isVisible(), "Morph on did not show the slots");

    press (*slotB);
    CHECK (processor->getPresetMorph().getCurrentSlot() == PresetMorph::slotB);

    auto* list = [&panel]() -> juce::ListBox*
    {
        juce::Array<juce::ListBox*> lists;
        collectAll<juce::ListBox> (panel, lists);
        return lists.isEmpty() ? nullptr : lists.getFirst();
    }();

    CHECK (list != nullptr && list->getListBoxModel() != nullptr
           && list->getListBoxModel()->getNumRows() > 1);

    if (list == nullptr || list->getListBoxModel() == nullptr || list->getListBoxModel()->getNumRows() < 2)
        return;

    list->selectRow (1);
    press (*load);

    // B holds the preset just loaded; the sound stays where the slider is (A).
    const auto& morph = processor->getPresetMorph();
    const auto aName = morph.getSlotName (PresetMorph::slotA), bName = morph.getSlotName (PresetMorph::slotB);
    CHECK_MSG (bName.isNotEmpty() && bName != aName, "slot B holds '" + bName + "' and A '" + aName + "'");
    CHECK_MSG (slotB->getButtonText() == "B: " + bName, slotB->getButtonText());
    CHECK_MSG (processor->getPresetManager().getCurrentPresetName() == aName,
               "at 0 the sound is '" + processor->getPresetManager().getCurrentPresetName() + "', not A '" + aName + "'");

    // The slider is preset_morph_position.
    slider->setValue (1.0, juce::sendNotificationSync);
    CHECK (processor->getState().getRawParameterValue (ParamIDs::presetMorphPosition)->load() > 0.99f);

    press (*toggle);
    CHECK (! processor->getPresetMorph().isEnabled());
    CHECK (! slotA->isVisible() && ! slider->isVisible());
}
