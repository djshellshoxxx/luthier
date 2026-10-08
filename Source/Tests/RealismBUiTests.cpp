/*  REALISM-B's UI (gui-integration.md 4.4 and 3.3): the CHARACTER tab's
    PICK -> HARMONICS row, RIGHT HAND and STRING INTERACTION groups, the Easy
    Playing strip's Tool selector, and the fretboard's contact ring, palm band
    and tool glyphs. Every control is attached to the parameter its spec names.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/CharacterPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/FretboardComponent.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    template <typename T>
    T* findIn (juce::Component& root)
    {
        if (auto* hit = dynamic_cast<T*> (&root))
            return hit;

        for (auto* child : root.getChildren())
            if (auto* hit = findIn<T> (*child))
                return hit;

        return nullptr;
    }

    void collectLearnIds (juce::Component& root, juce::StringArray& ids)
    {
        if (auto* target = dynamic_cast<LearnTarget*> (&root))
            ids.addIfNotAlreadyThere (target->getLearnParameterId());

        for (auto* child : root.getChildren())
            collectLearnIds (*child, ids);
    }

    float plainOf (LuthierAudioProcessor& processor, const juce::String& id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));
        return p != nullptr ? p->convertFrom0to1 (p->getValue()) : -1.0f;
    }
}

LUTHIER_TEST (RealismBUi, theCharacterTabCarriesTheThreeGroups)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 900);
    CHECK (panel.setWorkspaceTabNamed ("CHARACTER"));

    auto* character = findIn<CharacterPanel> (panel);
    auto* harmonics = findIn<HarmonicsGroup> (panel);
    auto* hand = findIn<RightHandGroup> (panel);
    auto* interaction = findIn<StringInteractionGroup> (panel);

    CHECK (character != nullptr && harmonics != nullptr && hand != nullptr && interaction != nullptr);

    if (character == nullptr || harmonics == nullptr || hand == nullptr || interaction == nullptr)
        return;

    CHECK_MSG (character->getHeight() >= character->preferredHeight(), "the CHARACTER tab does not fit its groups");

    for (auto* g : { (juce::Component*) harmonics, (juce::Component*) hand, (juce::Component*) interaction })
        CHECK (g->isVisible() && g->getHeight() > 0 && g->getWidth() > 0);

    juce::StringArray ids;
    collectLearnIds (*character, ids);

    for (const char* id : { ParamIDs::harmonicTouchPressure, ParamIDs::harmonicFingerWidth, ParamIDs::harmonicTouchTime,
                            ParamIDs::harmonicBriefTouch, ParamIDs::pinchThumbOffsetMm, ParamIDs::artificialHarmonicOffset,
                            ParamIDs::tappedHarmonicOffset, ParamIDs::harmonicNoteMapping,
                            ParamIDs::couplingAirAmount, ParamIDs::palmMuteSpread, ParamIDs::adjacentMuteAmount,
                            ParamIDs::releaseStaggerMs, ParamIDs::releaseStaggerBias, ParamIDs::pickupApertureScale,
                            ParamIDs::mutedThumpLevel,
                            ParamIDs::fingerFleshReleaseMs, ParamIDs::fingerNailReleaseMs, ParamIDs::thumbPositionOffset,
                            ParamIDs::restStrokeDamping, ParamIDs::rhStroke, ParamIDs::thumbPalmMute, ParamIDs::hybridSnap,
                            ParamIDs::nailVsFlesh, ParamIDs::useFingers })
        CHECK_MSG (ids.contains (id), juce::String ("nothing on the CHARACTER tab edits ") + id);

    // The string row: a click cycles string 3's tool, and it is a parameter.
    const float before = plainOf (processor, ParamIDs::rhStringTool (3));
    hand->getCell (2).mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), {},
                                                   juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                   &hand->getCell (2), &hand->getCell (2), juce::Time::getCurrentTime(),
                                                   {}, juce::Time::getCurrentTime(), 1, false));
    CHECK (juce::roundToInt (plainOf (processor, ParamIDs::rhStringTool (3))) == (juce::roundToInt (before) + 1) % 7);

    // The style box writes the table.
    hand->getStyleBox().setSelectedItemIndex ((int) RhStyle::fingerstyle, juce::sendNotificationSync);
    CHECK (juce::roundToInt (plainOf (processor, ParamIDs::rhStringTool (1))) == (int) RhTool::finger);
    CHECK (juce::roundToInt (plainOf (processor, ParamIDs::rhStringTool (6))) == (int) RhTool::thumb);
    CHECK (RightHandGroup::describeStyle (processor) == "Fingerstyle");

    processor.getState().getParameter (ParamIDs::rhStringTool (6))->setValueNotifyingHost (0.0f);
    CHECK (RightHandGroup::describeStyle (processor) == "Fingerstyle (modified)");

    // The offsets name their partials.
    CHECK (HarmonicsGroup::describeOffset (3).contains ("partial 5"));
}

LUTHIER_TEST (RealismBUi, theEasyPlayingStripHasTheToolSelector)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto* easy = findIn<EasyPanel> (*editor);
    auto* selector = easy != nullptr ? findIn<RightHandToolSelector> (*easy) : nullptr;
    CHECK (selector != nullptr);

    if (selector == nullptr)
        return;

    CHECK_MSG (selector->isVisible() && selector->getWidth() >= 140, "the Tool selector is " + juce::String (selector->getWidth()) + " wide");
    CHECK (selector->getSegment (0).getButtonText() == "Mixed");

    selector->getSegment ((int) RhStyle::travis).onClick();

    CHECK (juce::roundToInt (plainOf (processor, ParamIDs::rhStyle)) == (int) RhStyle::travis);
    CHECK (juce::roundToInt (plainOf (processor, ParamIDs::rhStringTool (5))) == (int) RhTool::thumbpick);
    CHECK_NEAR (plainOf (processor, ParamIDs::thumbPalmMute), 0.35, 1.0e-3);
}

LUTHIER_TEST (RealismBUi, theFretboardDrawsTheTouch)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    FretboardComponent board (processor);
    board.setSize (900, 160);

    // A 12th-fret natural harmonic on the low E.
    auto& engine = processor.getEngine();
    NoteOnEvent e;
    e.stringIndex = 5;
    e.fretPosition = 0.0;
    e.touchFret = 12.0;
    e.technique = Technique::NaturalHarmonic;
    e.pitchHz = engine.getTuningEngine().computeFrequency (5, 0.0, 0.0);
    engine.triggerNoteNow (e);

    board.refreshRealismB();
    CHECK_NEAR (board.getRealismBView (5).touchFret, 12.0, 1.0e-3);
    CHECK (board.getRealismBView (5).life > 0.0f && ! board.getRealismBView (5).missed);

    // Off a node: dashed.
    e.touchFret = 6.0;
    engine.triggerNoteNow (e);
    board.refreshRealismB();
    CHECK (board.getRealismBView (5).missed);

    // It paints without complaint.
    juce::Image image (juce::Image::ARGB, 900, 160, true);
    juce::Graphics g (image);
    board.paintEntireComponent (g, false);
}
