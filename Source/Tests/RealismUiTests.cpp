/*  The REALISM-C UI (noise-floor.md 5, sustain-and-decay.md 8,
    tuning-stability.md 6), group RealismUi.

    The groups are built the way the window builds them - inside the CHARACTER
    panel, Column 1's DecayRow, the headstock popover - and driven through
    their controls, with the engine rendering between steps where a readout
    needs the audio thread.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/CharacterPanel.h"
#include "../UI/RealismGroups.h"
#include "../UI/RoutingPanel.h"
#include "../UI/GuitarBodyComponent.h"
#include "../Presets/RealismStyleActions.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr int kBlock = 128;

    template <typename T>
    void collect (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* t = dynamic_cast<T*> (child))
                found.add (t);

            collect<T> (*child, found);
        }
    }

    juce::StringArray attachedIds (juce::Component& root)
    {
        juce::StringArray ids;
        juce::Array<LearnTarget*> targets;
        collect<LearnTarget> (root, targets);

        for (auto* t : targets)
            ids.addIfNotAlreadyThere (t->getLearnParameterId());

        return ids;
    }

    void render (LuthierAudioProcessor& processor, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    }

    void set (LuthierAudioProcessor& processor, const char* id, float plain)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    float get (LuthierAudioProcessor& processor, const char* id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));
        return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
    }
}

//==============================================================================
LUTHIER_TEST (RealismUi, everyRealismCParameterHasAControlOnTheCharacterTab)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kBlock);

    CharacterPanel panel (processor);
    panel.setSize (420, panel.preferredHeight());

    const auto ids = attachedIds (panel);

    // The two styles are combo boxes that write their row (a discrete
    // switch, action-and-undo.md 3.2), not attachments; they are checked below.
    for (auto* id : { ParamIDs::ampBuzz, ParamIDs::noiseMainsHz, ParamIDs::noiseFluorescent,
                      ParamIDs::noisePassiveHiss, ParamIDs::noiseCableMovement, ParamIDs::noiseRadio,
                      ParamIDs::noiseGroundLoop, ParamIDs::noiseAmpHiss, ParamIDs::noiseMicrophonics,
                      ParamIDs::noiseFloorToAux8,
                      ParamIDs::sustainAttackTransient, ParamIDs::sustainAttackTime, ParamIDs::sustainFastShare,
                      ParamIDs::sustainFastRatio, ParamIDs::sustainTensionMod, ParamIDs::sustainReleaseTime,
                      ParamIDs::sustainReleaseSag, ParamIDs::sustainReleaseRing,
                      ParamIDs::stabilityAmount, ParamIDs::stabilitySettling, ParamIDs::stabilityNutBinding,
                      ParamIDs::stabilityBacklash, ParamIDs::stabilitySaddleCreep, ParamIDs::stabilityBendMemory,
                      ParamIDs::stabilityCapoBias, ParamIDs::stabilityAutoRetune })
        CHECK_MSG (ids.contains (id), juce::String (id) + " has no control on the CHARACTER tab");

    // The position pad writes the angle and the distance.
    auto& pad = panel.getNoiseFloorGroup()->getPad();
    pad.setDistanceFromY (16.0f);
    CHECK_MSG (get (processor, ParamIDs::noisePlayerDistance) < 0.35f, "the pad's top is not next to the amp");
    pad.setDistanceFromY ((float) PositionPad::padHeight);
    CHECK_MSG (get (processor, ParamIDs::noisePlayerDistance) > 4.5f, "the pad's bottom is not across the room");

    const auto image = panel.createComponentSnapshot (panel.getLocalBounds());
    CHECK (image.isValid());
}

//==============================================================================
LUTHIER_TEST (RealismUi, theStyleBoxesApplyAndReadModified)
{
    LuthierAudioProcessor processor;
    CharacterPanel panel (processor);

    auto& box = panel.getNoiseFloorGroup()->getStyleBox();
    box.setSelectedId (3, juce::sendNotificationSync);   // Home desk
    CHECK_NEAR (get (processor, ParamIDs::noiseFluorescent), 0.15, 1.0e-4);
    CHECK (describeNoiseFloorStyle (processor) == "Home desk");

    set (processor, ParamIDs::noiseAmpHiss, 0.9f);
    CHECK (describeNoiseFloorStyle (processor) == "Home desk (modified)");

    // A style is one undo entry.
    processor.undo();
    CHECK_NEAR (get (processor, ParamIDs::noiseFluorescent), 0.0, 1.0e-4);

    applySustainStyle (processor, 2);
    CHECK (describeSustainStyle (processor) == "Acoustic, natural");
}

//==============================================================================
LUTHIER_TEST (RealismUi, theNoiseMeterReadsAndGoesStale)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kBlock);
    NoiseFloorGroup group (processor);

    group.getMeter().pollNow();
    CHECK_MSG (group.getMeter().isStale(), "an idle noise floor should read as off");

    set (processor, ParamIDs::noiseGroundLoop, 1.0f);
    render (processor, 20);
    group.getMeter().pollNow();
    CHECK (! group.getMeter().isStale());
    CHECK_MSG (std::abs (group.getMeter().getShownDb() + 45.0) < 3.0,
               "the meter shows " + juce::String (group.getMeter().getShownDb(), 1) + " dB for a -45 dB ground loop");
}

//==============================================================================
LUTHIER_TEST (RealismUi, theAux8SwitchIsMirroredOnRouting)
{
    LuthierAudioProcessor processor;
    RoutingPanel routing (processor);
    NoiseFloorGroup group (processor);

    juce::Array<LuthierToggle*> a, b;
    collect<LuthierToggle> (routing, a);
    collect<LuthierToggle> (group, b);

    LuthierToggle* onRouting = nullptr;
    for (auto* t : a) if (t->getLearnParameterId() == ParamIDs::noiseFloorToAux8) onRouting = t;

    CHECK_MSG (onRouting != nullptr, "ROUTING has no Aux 8 noise-floor switch");

    if (onRouting != nullptr)
    {
        onRouting->getButton().setToggleState (true, juce::sendNotificationSync);
        CHECK (get (processor, ParamIDs::noiseFloorToAux8) > 0.5f);
    }
}

//==============================================================================
LUTHIER_TEST (RealismUi, sustainShapeReadoutAndDecaySketch)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kBlock);
    SustainShapeGroup group (processor);

    set (processor, ParamIDs::sustainTensionMod, 1.0f);
    render (processor, 2);

    NoteOnEvent e;
    const int low = processor.getEngine().getNumStrings() - 1;
    e.stringIndex = low;
    e.pitchHz = processor.getEngine().getTuningEngine().computeFrequency (low, 0.0);
    e.velocity = 1.0;
    processor.getEngine().triggerNoteNow (e);
    render (processor, 10);

    auto& readout = group.getReadout();
    readout.pollNow();
    CHECK (! readout.isStale());
    CHECK_MSG (readout.getShownCents (low) > 1.0, "the hard-picked low string shows no tension pitch");

    // The sketch's envelope is the engine's two-stage formula: the knee at 10 log (1 - a).
    const double knee = DecaySketch::envelopeDb (3.0, 4.5, 0.5, 0.2) - DecaySketch::envelopeDb (3.0, 4.5, 0.0, 0.2);
    CHECK_NEAR (knee, -3.01, 0.05);

    DecayRow row (processor);
    row.setSize (240, DecayRow::preferredHeight);
    CHECK (row.createComponentSnapshot (row.getLocalBounds()).isValid());
}

//==============================================================================
LUTHIER_TEST (RealismUi, theOffsetStripRetunesTheStringItIsClickedOn)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kBlock);

    set (processor, ParamIDs::stabilityAmount, 1.0f);
    set (processor, ParamIDs::capoFret, 3.0f);
    processor.getParameterBridge().applyAllNow();
    render (processor, 4);

    TuningStabilityGroup group (processor);
    group.setSize (360, group.preferredHeight());
    auto& strip = group.getStrip();
    strip.setSize (360, OffsetStrip::preferredHeightFor (6));
    strip.pollNow();

    CHECK_MSG (strip.getShownCents (5) > 2.0, "the capo's bias is not on the strip: " + juce::String (strip.getShownCents (5), 2));
    CHECK (OffsetStrip::describe (processor, 5).contains ("capo"));

    // Click string 5's bar: it retunes (tuned with the capo on), the others do not.
    const int y = 2 + 5 * (OffsetStrip::barHeight + 2) + 3;
    strip.mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 100.0f, (float) y },
                                       juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &strip, &strip,
                                       juce::Time::getCurrentTime(), { 100.0f, (float) y }, juce::Time::getCurrentTime(), 1, false));
    render (processor, 2);

    CHECK (std::abs (processor.getEngine().getTuningEngine().getStringTuning (5).stabilityCents) < 1.0e-6);
    CHECK (processor.getEngine().getTuningEngine().getStringTuning (4).stabilityCents > 1.0);
    CHECK (TuningStabilityGroup::describeCapoBias (processor).contains ("Capo bias"));
}

//==============================================================================
LUTHIER_TEST (RealismUi, theHeadstockPopoverShowsOffsetsAndRetunes)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kBlock);

    set (processor, ParamIDs::stabilityAmount, 1.0f);
    set (processor, ParamIDs::capoFret, 3.0f);
    processor.getParameterBridge().applyAllNow();
    render (processor, 4);

    TuningPopover popover (processor);
    juce::Array<StabilityBadge*> badges;
    collect<StabilityBadge> (popover, badges);
    CHECK (badges.size() == processor.getEngine().getNumStrings());


    if (badges.size() == processor.getEngine().getNumStrings())
    {
        auto* low = badges.getLast();
        CHECK_MSG (low->getText().startsWith ("+") && low->getText().endsWith (" c"),
                   "the low string reads \"" + low->getText() + "\"");

        juce::Array<juce::TextButton*> buttons;
        collect<juce::TextButton> (*low, buttons);

        if (! buttons.isEmpty())
            buttons.getFirst()->onClick();   // triggerClick is asynchronous

        render (processor, 2);
        CHECK (std::abs (processor.getEngine().getTuningEngine().getStringTuning (badges.size() - 1).stabilityCents) < 1.0e-6);
    }
}

//==============================================================================
LUTHIER_TEST (RealismUi, theWorkshopInspectorShowsTheDerivedFigures)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kBlock);

    Part tuners;
    tuners.type = PartType::tuners;
    tuners.fields = juce::JSON::parse ("{\"ratio\": 14, \"stability\": 0.6}");
    const auto lines = describeTuningFigures (processor, GuitarSlot::tuners, &tuners);
    CHECK (lines.size() == 1 && lines[0].startsWith ("Backlash"));

    Part nut;
    nut.type = PartType::nut;
    nut.fields = juce::JSON::parse ("{\"friction\": 0.35}");
    const auto nutLines = describeTuningFigures (processor, GuitarSlot::nut, &nut);
    CHECK (nutLines.size() == 1 && nutLines[0].contains ("+2.1 c"));
}

//==============================================================================
LUTHIER_TEST (RealismUi, theRetuneAllButtonClearsEveryOffset)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kBlock);

    set (processor, ParamIDs::stabilityAmount, 1.0f);
    set (processor, ParamIDs::capoFret, 3.0f);
    processor.getParameterBridge().applyAllNow();
    render (processor, 4);

    CharacterPanel panel (processor);
    CHECK (panel.getRetuneAllButton().getButtonText() == "Retune all");

    auto worst = [&processor]
    {
        double w = 0.0;
        for (int s = 0; s < processor.getEngine().getNumStrings(); ++s)
            w = juce::jmax (w, std::abs (processor.getEngine().getTuningEngine().getStringTuning (s).stabilityCents));
        return w;
    };

    const double before = worst();
    CHECK (before > 1.0);

    // The Strat's vintage trem floats, so each correction moves the others
    // (tuning-stability.md 2.4.1): one press leaves a residual, a few converge.
    for (int press = 0; press < 5; ++press)
    {
        panel.getRetuneAllButton().onClick();   // triggerClick is asynchronous
        render (processor, 2);

        if (press == 0)
            CHECK (worst() <= 0.4 * before);
    }

    CHECK_MSG (worst() < 0.05, "five presses left " + juce::String (worst(), 3) + " c");
}
