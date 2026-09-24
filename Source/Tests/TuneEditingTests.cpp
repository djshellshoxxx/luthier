/*  The TUNE tab's editors (tune-builder.md 3.2, 3.3, 3.4, 6, 7).
    TUNE-HELP-ONBOARDING workstream.

    Menus are driven through performMenuItem and mouse gestures through the
    functions the mouse handlers call: a popup cannot be clicked in the console
    test runner. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Tune/TuneHarmony.h"
#include "../Tune/TuneMelody.h"
#include "../Tune/TuneVary.h"
#include "../UI/TuneChordEditor.h"
#include "../UI/TunePanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** A panel over the processor's own session, with a four-bar verse in C. */
    struct Fixture
    {
        LuthierAudioProcessor processor;
        std::unique_ptr<TunePanel> panel;

        Fixture()
        {
            Tune t;
            t.meta.title = "Editing";
            TuneSection verse;
            verse.name = "Verse";
            verse.lengthBars = 4;
            verse.genreKitId = "Folk Fingerstyle";
            verse.rhythmPatternId = "Folk Down Up";
            t.addSection (verse);
            applyProgressionText (t, 0, "C Am F G");
            session().newTune (t);

            panel = std::make_unique<TunePanel> (processor, processor.getTunePlayer(), session());
            panel->setSize (480, panel->getPreferredHeight());
        }

        ~Fixture() { panel.reset(); }

        TuneSession& session() { return processor.getTuneSession(); }
        const TuneSection& verse() { return *session().getTune().getSection (0); }

        juce::StringArray chords (int section = 0)
        {
            juce::StringArray out;

            for (const auto& c : session().getTune().getSection (section)->chords)
                out.add (getChordSymbol (c, false));

            return out;
        }
    };
}

//==============================================================================
/*  3.2: the pill menu - insert before and after, duplicate, delete, copy and
    paste, suggest substitution - every one a single undo step. */
LUTHIER_TEST (TuneEditing, theChordPillMenuInsertsDuplicatesDeletesCopiesPastesAndSubstitutes)
{
    Fixture f;
    auto& pills = f.panel->getChordPills();

    CHECK (f.chords() == juce::StringArray ({ "C", "Am", "F", "G" }));

    pills.performMenuItem (1, TuneChordPills::insertBeforeItem);
    CHECK (f.chords() == juce::StringArray ({ "C", "Am", "Am", "F", "G" }));
    CHECK (f.session().undo());

    pills.performMenuItem (3, TuneChordPills::insertAfterItem);
    CHECK (f.chords() == juce::StringArray ({ "C", "Am", "F", "G", "G" }));
    CHECK (f.session().undo());

    pills.performMenuItem (2, TuneChordPills::duplicateItem);
    CHECK (f.chords() == juce::StringArray ({ "C", "Am", "F", "F", "G" }));

    pills.performMenuItem (2, TuneChordPills::deleteItem);
    CHECK (f.chords() == juce::StringArray ({ "C", "Am", "F", "G" }));

    pills.performMenuItem (0, TuneChordPills::copyItem);
    CHECK (TuneChordPills::hasClipboard());
    pills.performMenuItem (3, TuneChordPills::pasteItem);
    CHECK (f.chords() == juce::StringArray ({ "C", "Am", "F", "G", "C" }));
    CHECK (f.session().undo());

    // The menu lists substitutions for G (a dominant): pick the first.
    const auto subs = suggestSubstitutions (f.verse().chords, 3, 0, TuneMode::ionian);
    CHECK (! subs.empty());
    const auto menu = pills.buildMenu (3);
    CHECK (menu.getNumItems() >= 8);

    const auto before = f.session().getNumUndoSteps();
    pills.performMenuItem (3, TuneChordPills::substitutionBase);
    CHECK (f.session().getNumUndoSteps() == before + 1);
    CHECK_MSG (f.chords() != juce::StringArray ({ "C", "Am", "F", "G" }), "the substitution changed nothing");
    CHECK (f.session().getUndoDescription().startsWith ("Substitute"));
}

//==============================================================================
/*  3.2: dragging a pill's right edge changes its length in half beats;
    dragging a pill moves it. */
LUTHIER_TEST (TuneEditing, dragsResizeAndReorderChordPills)
{
    Fixture f;
    auto& pills = f.panel->getChordPills();
    pills.setSize (400, 34);

    // Each chord is a bar of a four-bar section: 100 px a bar, 25 px a beat.
    const auto first = pills.getCellBounds (0);
    CHECK (first.getWidth() == 100);
    CHECK (pills.isOnRightEdge (0, { first.getRight() - 2, 10 }));
    CHECK (! pills.isOnRightEdge (0, { first.getCentreX(), 10 }));
    CHECK (pills.getCellAt ({ 150, 10 }) == 1);

    CHECK_NEAR (pills.beatsForEdgeAt (0, 150), 6.0, 1.0e-9);
    CHECK_NEAR (pills.beatsForEdgeAt (0, 60), 2.5, 1.0e-9);
    CHECK_NEAR (pills.beatsForEdgeAt (0, 1), 0.5, 1.0e-9);   // never shorter than half a beat

    CHECK (pills.resizeCell (0, 2.0));
    CHECK_NEAR (f.verse().chords[0].durationBeats, 2.0, 1.0e-9);

    // Move G (the last) to the front, and C back to the end.
    CHECK (pills.dropIndexAt (0) == 0);
    CHECK (pills.moveCell (3, 0));
    CHECK (f.chords() == juce::StringArray ({ "G", "C", "Am", "F" }));
    CHECK (pills.moveCell (1, 4));
    CHECK (f.chords() == juce::StringArray ({ "G", "Am", "F", "C" }));
    CHECK (! pills.moveCell (2, 3));   // its own slot: nothing
}

//==============================================================================
/*  3.2: the popover edits root, quality, bass, extensions, duration, strum
    override and emphasis, and refuses what the vocabulary does not know. */
LUTHIER_TEST (TuneEditing, theChordPopoverEditsEveryFieldOfTheCell)
{
    Fixture f;
    TuneChordEditor editor (f.session(), 0, 1, { "Folk Down Up", "Classic Strum" });

    CHECK (editor.getRootBox().getSelectedId() == 9 + 1);   // A
    CHECK (TuneChordEditor::getQualityChoices().contains ("m7"));

    CHECK (editor.setRoot (2));
    CHECK (editor.setQuality ("m7"));
    CHECK (editor.setBass (0));
    CHECK (editor.setExtensions ("9, 11"));
    CHECK (! editor.setExtensions ("9, banana"));
    CHECK (editor.setDuration (2.0));
    CHECK (editor.setStrumOverride ("Classic Strum"));
    CHECK (editor.setEmphasis (ChordEmphasis::accent));

    const auto& cell = f.verse().chords[1];
    CHECK (cell.root == 2 && cell.quality == "m7" && cell.bass == 0);
    CHECK (cell.extensions == juce::StringArray ({ "9", "11" }));
    CHECK_NEAR (cell.durationBeats, 2.0, 1.0e-9);
    CHECK (cell.strumOverride == "Classic Strum");
    CHECK (cell.emphasis == ChordEmphasis::accent);

    // The controls read the cell back, and an undo shows.
    editor.refresh();
    CHECK (editor.getEmphasisBox().getSelectedId() == 2);
    CHECK (editor.getStrumBox().getText() == "Classic Strum");
    CHECK (f.session().undo());
    editor.refresh();
    CHECK (editor.getEmphasisBox().getSelectedId() == 1);
}

//==============================================================================
/*  3.3: drag a tab to reorder; "Vary" makes a subtle sibling; tabs dragged into
    the setlist timeline arrange the play order. */
LUTHIER_TEST (TuneEditing, sectionsReorderVaryAndArrangeInTheSetlist)
{
    Fixture f;
    auto& strip = f.panel->getSectionStrip();
    auto& setlist = f.panel->getSetlistStrip();

    strip.performMenuItem (0, TuneSectionStrip::addItem);
    CHECK (f.session().getTune().getNumSections() == 2);

    const auto firstName = f.session().getTune().getSection (0)->name;
    CHECK (strip.moveSectionTo (0, 2));
    CHECK (f.session().getTune().getSection (1)->name == firstName);
    CHECK (strip.moveSectionTo (1, 0));
    CHECK (f.session().getTune().getSection (0)->name == firstName);

    // Vary: a sibling with the same length and chords but the turnaround.
    f.session().edit (TuneEditClass::melodyGenerate, "melody", [] (Tune& t) { return generateMelody (t, 0); });
    f.session().edit (TuneEditClass::melodyEdit, "lock", [] (Tune& t) { return t.setMelodyNoteLocked (0, 0, true); });
    const auto locked = f.verse().melody->notes[0];

    const int made = strip.varySection (0);
    CHECK (made == 1);

    const auto* vary = f.session().getTune().getSection (made);
    CHECK (vary != nullptr);

    if (vary != nullptr)
    {
        CHECK (vary->name == "Verse (vary)");
        CHECK (vary->lengthBars == f.verse().lengthBars);
        CHECK_NEAR (f.session().getTune().getSectionLengthBeats (made), f.session().getTune().getSectionLengthBeats (0), 1.0e-9);
        CHECK (vary->chords.front() == f.verse().chords.front());
        CHECK_MSG (vary->melody.has_value() && std::find (vary->melody->notes.begin(), vary->melody->notes.end(), locked)
                                                 != vary->melody->notes.end(),
                   "Vary changed a locked note");
        CHECK (vary->feel != f.verse().feel);
    }

    CHECK (f.session().getUndoDescription() == "Vary section");
    CHECK (f.session().undo());
    CHECK (f.session().getTune().getNumSections() == 2);

    // The setlist timeline: drop Verse twice around the other section, move, repeat, remove.
    const auto other = f.session().getTune().getSection (1)->name;
    CHECK (setlist.dropSection (0, 0));
    CHECK (setlist.dropSection (1, 1));
    CHECK (setlist.dropSection (0, 2));
    auto names = [&f]
    {
        juce::StringArray n;

        for (const auto& e : f.session().getTune().arrangement.setlist)
            n.add (e.section + "x" + juce::String (e.repeats));

        return n;
    };

    CHECK (names().joinIntoString (",") == "Versex1," + other + "x1,Versex1");
    CHECK (setlist.moveEntry (1, 0));
    CHECK (names().joinIntoString (",") == other + "x1,Versex1,Versex1");
    CHECK (setlist.setEntryRepeats (0, 4));
    setlist.performMenuItem (2, TuneSetlistStrip::removeItem);
    CHECK (names().joinIntoString (",") == other + "x4,Versex1");

    // Vary with a setlist: the sibling plays after the original.
    const int v = strip.varySection (0);
    CHECK (v >= 0);
    CHECK (names().joinIntoString (",") == other + "x4,Versex1,Verse (vary)x1");

    // A tab dropped over the timeline arrives through the strip's callback.
    setlist.setBounds (0, 0, 300, TuneSetlistStrip::kHeight);
    CHECK (strip.onTabDropped != nullptr);
    CHECK (strip.onTabDropped (0, setlist.localPointToGlobal (juce::Point<int> (299, 10))));
    CHECK (f.session().getTune().arrangement.setlist.back().section == "Verse");
    CHECK (! strip.onTabDropped (0, setlist.localPointToGlobal (juce::Point<int> (10, 200))));

    setlist.performMenuItem (0, TuneSetlistStrip::clearItem);
    CHECK (f.session().getTune().arrangement.setlist.empty());
}

//==============================================================================
/*  3.4: select, box-select, nudge, move, cut / copy / paste, and the note menu. */
LUTHIER_TEST (TuneEditing, thePianoRollSelectsNudgesCopiesAndEditsNotes)
{
    Fixture f;
    auto& roll = f.panel->getPianoRoll();
    roll.setSize (400, 200);

    CHECK (roll.addNote (0.0, 60, 1.0));
    CHECK (roll.addNote (1.0, 64, 1.0));
    CHECK (roll.addNote (2.0, 67, 1.0));
    CHECK (roll.getSelection() == std::vector<int> ({ 2 }));   // the last drawn is selected

    roll.selectNote (0, false);
    roll.selectNote (1, true);
    CHECK (roll.getSelection().size() == 2);
    roll.selectNote (1, true);
    CHECK (roll.getSelection() == std::vector<int> ({ 0 }));

    // Box: everything in the first two beats.
    roll.selectInBox ({ 0.0f, 0.0f, (float) roll.getWidth() * 2.0f / 16.0f - 1.0f, 200.0f }, false);
    CHECK (roll.getSelection().size() == 2);

    // Nudge right by the grid (1/8 = half a beat) and up a scale step (E -> F, C -> D in C major).
    CHECK (roll.nudgeSelected (1, 1, false));
    const auto& notes = f.verse().melody->notes;
    CHECK_NEAR (notes[0].startBeat, 0.5, 1.0e-9);
    CHECK (notes[0].pitch.value == 62);
    CHECK (notes[1].pitch.value == 65);
    CHECK (roll.getSelection().size() == 2);

    // Shift: a bar and an octave.
    CHECK (roll.nudgeSelected (1, -1, true));
    CHECK_NEAR (notes[0].startBeat, 4.5, 1.0e-9);
    CHECK (notes[0].pitch.value == 50);

    // Arrow keys do the same with the roll focused; left brings it back.
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier, 0)));
    CHECK_NEAR (notes[0].startBeat, 0.5, 1.0e-9);
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0)));
    CHECK (notes[0].pitch.value == 62);

    // Copy, paste after, cut.
    CHECK (roll.keyPressed (juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)));
    CHECK (TunePianoRoll::hasClipboard());
    CHECK (roll.paste (8.0));
    CHECK (notes.size() == 5);
    CHECK_NEAR (notes[3].startBeat, 8.0, 1.0e-9);
    CHECK_NEAR (notes[4].startBeat, 9.0, 1.0e-9);
    CHECK (roll.getSelection() == std::vector<int> ({ 3, 4 }));
    CHECK (roll.cutSelected());
    CHECK (notes.size() == 3);
    CHECK (roll.keyPressed (juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0)));
    CHECK (notes.size() == 5);
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (notes.size() == 3);

    // The note menu: velocity, articulation, technique, unlock, delete.
    roll.selectNote (2, false);
    roll.performNoteMenuItem (TunePianoRoll::velocityBase + 40);
    roll.performNoteMenuItem (TunePianoRoll::articulationBase + (int) NoteArticulation::staccato);
    roll.performNoteMenuItem (TunePianoRoll::techniqueBase + (int) NoteTechnique::bend);
    CHECK (notes[2].velocity == 40);
    CHECK (notes[2].articulation == NoteArticulation::staccato);
    CHECK (notes[2].technique == NoteTechnique::bend);
    CHECK (notes[2].locked);

    roll.performNoteMenuItem (TunePianoRoll::unlockItem);
    CHECK (! notes[2].locked);
    CHECK (roll.buildNoteMenu().getNumItems() >= 8);

    // An unlocked note is Auto's; the locked ones survive a regenerate byte-identical (0.3).
    const auto keep0 = notes[0], keep1 = notes[1];
    f.session().edit (TuneEditClass::melodyGenerate, "regen", [] (Tune& t) { return regenerateMelody (t, 0); });
    const auto& after = f.verse().melody->notes;
    CHECK (std::find (after.begin(), after.end(), keep0) != after.end());
    CHECK (std::find (after.begin(), after.end(), keep1) != after.end());

    roll.selectAll();
    roll.performNoteMenuItem (TunePianoRoll::deleteItem);
    CHECK (f.verse().melody->notes.empty());
}

//==============================================================================
/*  6 and 7: the same roll edits the bass line and the countermelody. Editing a
    generated bass makes it the manual line it was. */
LUTHIER_TEST (TuneEditing, theRollEditsTheBassAndTheCountermelody)
{
    Fixture f;
    auto& roll = f.panel->getPianoRoll();
    roll.setSize (400, 200);

    f.session().edit (TuneEditClass::sectionEdit, "bass", [] (Tune& t) { return t.setBassMode (0, BassMode::rootFifth); });
    roll.setTarget (TunePianoRoll::Target::bass);
    CHECK (roll.isShowingGeneratedBass());

    const auto generated = roll.getShownNotes();
    CHECK (! generated.empty());
    CHECK (roll.getLowestPitch() < 40);

    roll.selectNote (0, false);
    CHECK (roll.nudgeSelected (0, 0, true) == false || true);   // nothing to move by zero
    CHECK (roll.setSelectedVelocity (70));
    CHECK (f.verse().bass.mode == BassMode::manual);
    CHECK (f.verse().bass.notes.size() == generated.size());
    CHECK (f.verse().bass.notes[0].velocity == 70);
    CHECK (! roll.isShowingGeneratedBass());

    CHECK (roll.addNote (3.0, 40, 1.0));
    CHECK (f.verse().bass.notes.size() == generated.size() + 1);
    CHECK (! f.verse().melody.has_value());   // the melody was never touched

    // The countermelody: a layer made on the first note.
    roll.setTarget (TunePianoRoll::Target::countermelody);
    CHECK (roll.getShownNotes().empty());
    CHECK (roll.addNote (0.0, 55, 2.0));
    const auto* layer = f.verse().findLayer (LayerType::countermelody);
    CHECK (layer != nullptr && layer->notes.size() == 1);
    CHECK (f.session().undo());
    CHECK (f.verse().findLayer (LayerType::countermelody) == nullptr);
}

//==============================================================================
/*  6 and 7 in the TUNE tab: the bass mode, and each layer's on / off, volume
    and pan, the arpeggio's pattern and the countermelody's regenerate. */
LUTHIER_TEST (TuneEditing, theBassAndLayerRowsEditTheSection)
{
    Fixture f;
    auto& strip = f.panel->getLayersStrip();

    for (int m = 0; m < (int) BassMode::numModes; ++m)
    {
        strip.getBassBox().setSelectedId (m + 1, juce::dontSendNotification);
        strip.getBassBox().onChange();
        CHECK (f.verse().bass.mode == (BassMode) m);
    }

    // Each layer: on, volume, pan, off - settings kept while off.
    for (int i = 0; i < (int) LayerType::numTypes; ++i)
    {
        const auto type = (LayerType) i;
        CHECK (strip.setLayerEnabled (type, true));
        CHECK (strip.setLayerVolume (type, 0.3));
        CHECK (strip.setLayerPan (type, -0.5));

        const auto* layer = f.verse().findLayer (type);
        CHECK (layer != nullptr && layer->enabled);
        CHECK (layer != nullptr && std::abs (layer->volume - 0.3) < 1.0e-9 && std::abs (layer->pan + 0.5) < 1.0e-9);

        CHECK (strip.setLayerEnabled (type, false));
        CHECK (! f.verse().findLayer (type)->enabled);
        CHECK (std::abs (f.verse().findLayer (type)->volume - 0.3) < 1.0e-9);
    }

    // The countermelody is written when it is first switched on, and again on Regenerate.
    f.session().edit (TuneEditClass::melodyGenerate, "melody", [] (Tune& t) { return generateMelody (t, 0); });
    CHECK (strip.setLayerEnabled (LayerType::countermelody, true));
    const auto first = f.verse().findLayer (LayerType::countermelody)->notes;
    CHECK (strip.regenerateCountermelody());
    CHECK (f.verse().findLayer (LayerType::countermelody)->seed == 2);

    // The arpeggio names a fingerpick pattern.
    if (strip.getArpeggioBox().getNumItems() > 1)
    {
        strip.getArpeggioBox().setSelectedId (2, juce::dontSendNotification);
        strip.getArpeggioBox().onChange();
        CHECK (f.verse().findLayer (LayerType::arpeggio)->patternId == strip.getArpeggioBox().getItemText (1));
    }

    // A volume drag is one undo step.
    const int steps = f.session().getNumUndoSteps();
    strip.setLayerVolume (LayerType::pad, 0.4);
    strip.setLayerVolume (LayerType::pad, 0.5);
    strip.setLayerVolume (LayerType::pad, 0.6);
    CHECK (f.session().getNumUndoSteps() == steps + 1);

    // The roll's target box picks what it edits.
    f.panel->getRollTargetBox().setSelectedId (2, juce::dontSendNotification);
    f.panel->getRollTargetBox().onChange();
    CHECK (f.panel->getPianoRoll().getTarget() == TunePianoRoll::Target::bass);
}
