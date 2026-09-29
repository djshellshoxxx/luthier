/*  accessibility.md 1 and 2 (A11Y-8): the fretboard without a mouse.

    The board is painted, not built of child components, so a screen reader would
    see one silent rectangle and a keyboard user could not play it. This gives it
    a focus cursor (arrows, Enter or Space to play, M to mute), a spoken
    description of the cell under the cursor, and a value the reader can query. */

#include "FretboardComponent.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    /** Exposes the cursor cell as the board's value, so a reader can ask for it. */
    struct CursorValue : juce::AccessibilityTextValueInterface
    {
        explicit CursorValue (FretboardComponent& f) : board (f) {}

        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override
        {
            return board.describeCell (board.getCursorString(), board.getCursorFret());
        }
        void setValueAsString (const juce::String&) override {}

        FretboardComponent& board;
    };
}

std::unique_ptr<juce::AccessibilityHandler> FretboardComponent::createAccessibilityHandler()
{
    setTitle ("Fretboard");
    setDescription ("Arrow keys move between strings and frets. Enter or Space plays the note. "
                    "M mutes the string.");

    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::group, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces { std::make_unique<CursorValue> (*this) });
}

juce::String FretboardComponent::describeCell (int stringIndex, int fret) const
{
    stringIndex = juce::jlimit (0, juce::jmax (0, numStrings - 1), stringIndex);
    fret = juce::jlimit (0, numFrets, fret);

    static const char* const names[12] = { "C", "C sharp", "D", "D sharp", "E", "F",
                                           "F sharp", "G", "G sharp", "A", "A sharp", "B" };

    // The row nearest the top is the lowest string in the drawing; the engine
    // counts strings from the same end, so the user's "string 1" is row 0.
    auto text = "String " + juce::String (stringIndex + 1) + ", fret " + juce::String (fret)
                + ", current note: " + names[pitchClassAt (stringIndex, fret)];

    if (isStringMuted (stringIndex))
        text << ", muted";

    if (fret == capoFret && capoFret > 0)
        text << ", capo";

    return text;
}

void FretboardComponent::setCursor (int stringIndex, int fret)
{
    const int s = juce::jlimit (0, juce::jmax (0, numStrings - 1), stringIndex);
    const int f = juce::jlimit (0, numFrets, fret);

    if (s == cursorString && f == cursorFret && cursorShown)
        return;

    cursorString = s;
    cursorFret = f;
    cursorShown = true;

    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);

    AccessibleSetup::announce (describeCell (s, f), AccessibleSetup::Announcement::valueChange);
    repaint();
}

bool FretboardComponent::playCursor()
{
    const int s = cursorString;

    if (isStringMuted (s))
    {
        AccessibleSetup::announce ("String " + juce::String (s + 1) + " is muted",
                                   AccessibleSetup::Announcement::standard);
        return false;
    }

    setSelectedString (s);

    if (onStringSelected)
        onStringSelected (s);

    processor.triggerPreviewNote (s, (double) juce::jmax (cursorFret, capoFret), 0.7);
    playingString = s;
    return true;
}

bool FretboardComponent::keyPressed (const juce::KeyPress& key)
{
    const bool jump = key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown();
    const int step = jump ? 5 : 1;

    if (key == juce::KeyPress::upKey)         { setCursor (cursorString - 1, cursorFret); return true; }
    if (key == juce::KeyPress::downKey)       { setCursor (cursorString + 1, cursorFret); return true; }
    if (key.getKeyCode() == juce::KeyPress::leftKey)  { setCursor (cursorString, cursorFret - step); return true; }
    if (key.getKeyCode() == juce::KeyPress::rightKey) { setCursor (cursorString, cursorFret + step); return true; }
    if (key == juce::KeyPress::homeKey)       { setCursor (cursorString, 0); return true; }
    if (key == juce::KeyPress::endKey)        { setCursor (cursorString, numFrets); return true; }

    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        return playCursor(), true;

    if (key.getTextCharacter() == 'm' || key.getTextCharacter() == 'M')
    {
        setStringMuted (cursorString, ! isStringMuted (cursorString));
        AccessibleSetup::announce (describeCell (cursorString, cursorFret), AccessibleSetup::Announcement::standard);
        return true;
    }

    return false;
}

void FretboardComponent::focusGained (FocusChangeType)
{
    // Focus lands on a definite cell, and says which.
    cursorShown = false;
    setCursor (cursorString, cursorFret);
}

void FretboardComponent::paintOverChildren (juce::Graphics& g)
{
    // A visible focus ring on the cursor cell, only while the board has focus.
    if (! cursorShown || ! hasKeyboardFocus (false) || boardArea.isEmpty())
        return;

    const auto cell = noteDotArea (cursorString, (double) cursorFret).toFloat().reduced (2.0f);
    g.setColour (Palette::textPrimary);
    g.drawEllipse (cell, 2.0f);
    g.setColour (Palette::panel);
    g.drawEllipse (cell.expanded (2.0f), 1.0f);
}

void FretboardComponent::focusLost (FocusChangeType)
{
    cursorShown = false;
    repaint();
}

} // namespace luthier
