#include "PianoRollStrip.h"
#include "Theme.h"
#include "VisualAids.h"
#include "../PluginProcessor.h"
#include "../Model/Playing/RubricVoicer.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    // The two-row layout: A W S E D F T G Y H U J K O L P ; (C .. E an octave up).
    constexpr const char* kComputerKeys = "awsedftgyhujkolp;";

    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

//==============================================================================
PianoRollStrip::PianoRollStrip (LuthierAudioProcessor& p, bool advancedMode)
    : processor (p), advanced (advancedMode)
{
    setWantsKeyboardFocus (true);
    setTitle ("Piano roll");
    setDescription ("A piano keyboard that lights the notes the guitar sounds and plays the guitar from its keys. "
                    "With it focused, A to L play white keys, W to P black keys, Z and X change octave.");
    setTooltip ("Click or drag across the keys to play the guitar. Focus the strip to play from the computer keyboard.");

    auto setUp = [this] (juce::TextButton& b, const juce::String& tip)
    {
        b.setTooltip (tip);
        b.setTitle (tip);
        b.setWantsKeyboardFocus (true);
        addAndMakeVisible (b);
    };

    setUp (modeButton, "Keys only, or keys with the last four seconds of notes above them");
    modeButton.setTitle ("Piano roll shows");
    modeButton.onClick = [this]
    {
        VisualAids::setPianoRollShowsRoll (! VisualAids::pianoRollShowsRoll());
        refreshButtons();

        if (onLayoutChanged)
            onLayoutChanged();
    };

    setUp (latchButton, "Latch: clicks collect keys into a chord; Play sends it");
    latchButton.setTitle ("Latch");
    latchButton.setClickingTogglesState (true);
    latchButton.onClick = [this] { setLatch (latchButton.getToggleState()); };

    setUp (fingeringButton, "Show where the keys lie on this guitar, as hollow dots on the fretboard");
    fingeringButton.setTitle ("Show fingering");
    fingeringButton.setClickingTogglesState (true);
    fingeringButton.onClick = [this] { setShowFingering (fingeringButton.getToggleState()); };

    setUp (playButton, "Play the latched keys as one chord (Enter)");
    playButton.setTitle ("Play latched chord");
    playButton.onClick = [this] { playLatched(); };

    setUp (clearButton, "Empty the latched keys (Escape)");
    clearButton.setTitle ("Clear latched keys");
    clearButton.onClick = [this] { clearLatched(); };

    if (advanced)
    {
        setUp (collapseButton, "Collapse or expand the piano roll");
        collapseButton.setTitle ("Collapse piano roll");
        collapseButton.onClick = [this] { setCollapsed (! isCollapsed()); };
    }

    range = PianoRollModel::rangeFor (processor.getEngine().getTuningEngine(), processor.getEngine().getNumStrings());
    wasWanted = isWanted();
    lastShowsRoll = VisualAids::pianoRollShowsRoll();
    refreshButtons();
    motion.startTimerHz (*this, 30);   // cpu-quality-modes 6
}

PianoRollStrip::~PianoRollStrip()
{
    motion.stopTimer();

    for (int note : held)
        processor.releaseKeyboardNote (note);

    if (fretboard != nullptr)
        fretboard->setGhostDots ({});

    if (onGhostDots)
        onGhostDots ({});
}

//==============================================================================
bool PianoRollStrip::isWanted() const        { return VisualAids::showPianoRoll (advanced); }
bool PianoRollStrip::isShowingRoll() const   { return VisualAids::pianoRollShowsRoll() && ! isCollapsed(); }
bool PianoRollStrip::isLatchOn() const       { return processor.getUiState().pianoLatch; }
const juce::Array<int>& PianoRollStrip::getLatched() const { return processor.getUiState().pianoLatchedNotes; }
bool PianoRollStrip::isCollapsed() const     { return advanced && ! processor.getUiState().pianoRollExpanded; }

int PianoRollStrip::getPreferredHeight() const
{
    if (! advanced)
        return kEasyHeight;

    if (isCollapsed())
        return kCollapsedHeight;

    return juce::jlimit (40, 140, processor.getUiState().pianoRollHeight);
}

void PianoRollStrip::setCollapsed (bool collapsed)
{
    if (! advanced)
        return;

    processor.getUiState().pianoRollExpanded = ! collapsed;
    refreshButtons();

    if (onLayoutChanged)
        onLayoutChanged();
}

void PianoRollStrip::setLatch (bool on)
{
    auto& ui = processor.getUiState();

    if (ui.pianoLatch == on)
        return;

    ui.pianoLatch = on;

    // Leaving latch: whatever it was holding lets go.
    if (! on)
        clearLatched();

    refreshButtons();
    updateFingering();
}

void PianoRollStrip::setShowFingering (bool on)
{
    processor.getUiState().pianoShowFingering = on;
    refreshButtons();
    updateFingering();
}

void PianoRollStrip::refreshButtons()
{
    const auto& ui = processor.getUiState();
    modeButton.setButtonText (VisualAids::pianoRollShowsRoll() ? "ROLL" : "KEYS");
    latchButton.setToggleState (ui.pianoLatch, juce::dontSendNotification);
    fingeringButton.setToggleState (ui.pianoShowFingering, juce::dontSendNotification);
    playButton.setVisible (ui.pianoLatch);
    clearButton.setVisible (ui.pianoLatch);
    playButton.setEnabled (! ui.pianoLatchedNotes.isEmpty());
    collapseButton.setButtonText (isCollapsed() ? juce::String::fromUTF8 ("\xe2\x96\xb8") : juce::String::fromUTF8 ("\xe2\x96\xbe"));
    collapseButton.setTitle (isCollapsed() ? "Expand piano roll" : "Collapse piano roll");

    for (auto* b : { &modeButton, &latchButton, &fingeringButton })
        b->setVisible (! isCollapsed());

    if (isCollapsed())
    {
        playButton.setVisible (false);
        clearButton.setVisible (false);
    }

    resized();
    repaint();
}

//==============================================================================
void PianoRollStrip::pressKey (int note, float velocity)
{
    if (! juce::isPositiveAndBelow (note, 128))
        return;

    if (isLatchOn())
    {
        auto& latched = processor.getUiState().pianoLatchedNotes;

        if (latched.contains (note))
            latched.removeFirstMatchingValue (note);
        else
            latched.add (note);

        refreshButtons();
        updateFingering();
        repaint();
        return;
    }

    if (held.contains (note))
        return;

    held.add (note);
    processor.playKeyboardNote (note, velocity);
    updateFingering();
}

void PianoRollStrip::releaseKey (int note)
{
    if (isLatchOn() || ! held.contains (note))
        return;

    held.removeFirstMatchingValue (note);
    processor.releaseKeyboardNote (note);
    updateFingering();
}

void PianoRollStrip::playLatched()
{
    const auto& latched = getLatched();

    if (! sentChord.isEmpty())
        processor.releaseKeyboardChord (sentChord);

    if (latched.isEmpty())
        return;

    // Every note at the same sample: the interpreter strums it as one chord at
    // the global crossing speed, or the rhythm engine takes it as the held chord.
    sentChord = latched;
    processor.playKeyboardChord (sentChord, 0.8f);
}

void PianoRollStrip::clearLatched()
{
    if (! sentChord.isEmpty())
        processor.releaseKeyboardChord (sentChord);

    sentChord.clear();
    processor.getUiState().pianoLatchedNotes.clear();
    refreshButtons();
    updateFingering();
    repaint();
}

void PianoRollStrip::updateFingering()
{
    unreachable.clear();
    const auto& notes = isLatchOn() ? getLatched() : held;

    for (int note : notes)
        if (note < range.lowestPlayable)
            unreachable.add (note);

    auto publish = [this] (const std::vector<FretboardComponent::GhostDot>& dots)
    {
        ghostDots = dots;

        if (fretboard != nullptr)
            fretboard->setGhostDots (dots);

        if (onGhostDots)
            onGhostDots (dots);
    };

    if (! processor.getUiState().pianoShowFingering || notes.isEmpty())
    {
        publish ({});
        return;
    }

    // The same voicer the engine uses, a copy of its own on this guitar's tuning
    // (the engine's are the audio thread's and carry its previous voicing).
    auto& engine = processor.getEngine();
    RubricVoicer voicer;
    voicer.prepare (&engine.getTuningEngine(), engine.getNumStrings());
    voicer.setMaxFret (engine.getGuitarSpec().maxFrets);

    juce::Array<int> reachable;

    for (int note : notes)
        if (note >= range.lowestPlayable)
            reachable.add (note);

    std::vector<FretboardComponent::GhostDot> dots;

    if (! reachable.isEmpty())
    {
        const auto voicing = voicer.voice (reachable.getRawDataPointer(), nullptr, reachable.size());

        for (int i = 0; i < juce::jmin (voicing.numNotes, (int) SoundingNotes::kMaxStrings); ++i)
            if (voicing.notes[(size_t) i].valid)
                dots.push_back ({ voicing.notes[(size_t) i].stringIndex, voicing.notes[(size_t) i].fretPosition });
    }

    publish (dots);
}

//==============================================================================
void PianoRollStrip::timerCallback()
{
    tick (juce::Time::getMillisecondCounterHiRes());
}

void PianoRollStrip::tick (double nowMs)
{
    // The options, followed here (per mode: Options -> Visual aids).
    if (const bool wanted = isWanted(); wanted != wasWanted)
    {
        wasWanted = wanted;

        if (onLayoutChanged)
            onLayoutChanged();
    }

    if (const bool rolls = VisualAids::pianoRollShowsRoll(); rolls != lastShowsRoll)
    {
        lastShowsRoll = rolls;
        refreshButtons();

        if (onLayoutChanged)
            onLayoutChanged();
    }

    // The range follows tuning and capo (PR-06).
    const auto now = PianoRollModel::rangeFor (processor.getEngine().getTuningEngine(), processor.getEngine().getNumStrings());

    if (now.drawLow != range.drawLow || now.drawHigh != range.drawHigh
        || now.lowestPlayable != range.lowestPlayable || now.highestPlayable != range.highestPlayable)
    {
        range = now;
        updateFingering();
        repaint();
    }

    SoundingNotes::Frame frame;

    if (! processor.getSoundingNotes().read (frame))
        return;

    if (frame.sequence != lastSequence)
    {
        lastSequence = frame.sequence;
        lastPublishMs = nowMs;
    }

    const bool stale = nowMs - lastPublishMs > PianoRollModel::kStaleMs;
    model.update (nowMs, frame, stale);
    lastTickMs = nowMs;

    if (isShowing())
        repaint (keysArea.getUnion (rollArea));
}

//==============================================================================
bool PianoRollStrip::isBlack (int note) noexcept
{
    const int pc = note % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

juce::Rectangle<float> PianoRollStrip::getKeyBounds (int note) const
{
    if (note < range.drawLow || note > range.drawHigh || keysArea.isEmpty())
        return {};

    int whites = 0;

    for (int n = range.drawLow; n <= range.drawHigh; ++n)
        whites += isBlack (n) ? 0 : 1;

    const float w = (float) keysArea.getWidth() / (float) juce::jmax (1, whites);
    int index = 0;

    for (int n = range.drawLow; n < note; ++n)
        index += isBlack (n) ? 0 : 1;

    const auto area = keysArea.toFloat();

    if (! isBlack (note))
        return { area.getX() + w * (float) index, area.getY(), w, area.getHeight() };

    return { area.getX() + w * (float) index - w * 0.3f, area.getY(), w * 0.6f, area.getHeight() * 0.6f };
}

int PianoRollStrip::noteAt (juce::Point<float> position, float* velocity) const
{
    if (! keysArea.toFloat().contains (position))
        return -1;

    auto velocityFor = [&] (juce::Rectangle<float> key)
    {
        // Section 3: from the top of the key (40) to its bottom (110), as JUCE's keyboard does.
        const float t = juce::jlimit (0.0f, 1.0f, (position.y - key.getY()) / juce::jmax (1.0f, key.getHeight()));
        return (40.0f + 70.0f * t) / 127.0f;
    };

    // Black keys sit on top.
    for (int n = range.drawLow; n <= range.drawHigh; ++n)
        if (isBlack (n))
            if (const auto b = getKeyBounds (n); b.contains (position))
            {
                if (velocity != nullptr) *velocity = velocityFor (b);
                return n;
            }

    for (int n = range.drawLow; n <= range.drawHigh; ++n)
        if (! isBlack (n))
            if (const auto b = getKeyBounds (n); b.contains (position))
            {
                if (velocity != nullptr) *velocity = velocityFor (b);
                return n;
            }

    return -1;
}

//==============================================================================
void PianoRollStrip::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (handleArea.contains (e.getPosition()))
    {
        draggingHandle = true;
        dragStartHeight = getPreferredHeight();
        return;
    }

    float velocity = 0.6f;
    mouseNote = noteAt (e.position, &velocity);

    if (mouseNote >= 0)
        pressKey (mouseNote, velocity);
}

void PianoRollStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingHandle)
    {
        processor.getUiState().pianoRollHeight = juce::jlimit (40, 140, dragStartHeight + e.getDistanceFromDragStartY());

        if (onLayoutChanged)
            onLayoutChanged();

        return;
    }

    if (isLatchOn())
        return;

    // A glissando: note-off for the old key, note-on for the new one.
    float velocity = 0.6f;
    const int note = noteAt (e.position, &velocity);

    if (note != mouseNote)
    {
        if (mouseNote >= 0)
            releaseKey (mouseNote);

        mouseNote = note;

        if (note >= 0)
            pressKey (note, velocity);
    }
}

void PianoRollStrip::mouseUp (const juce::MouseEvent&)
{
    draggingHandle = false;

    if (mouseNote >= 0)
        releaseKey (mouseNote);

    mouseNote = -1;
}

void PianoRollStrip::mouseMove (const juce::MouseEvent& e)
{
    const int note = noteAt (e.position);
    setMouseCursor (handleArea.contains (e.getPosition()) ? juce::MouseCursor::UpDownResizeCursor
                                                          : juce::MouseCursor::NormalCursor);

    // A key no fingering reaches says why (section 3).
    setTooltip (note >= 0 && note < range.lowestPlayable ? "Below this guitar's range"
                                                         : "Click or drag across the keys to play the guitar. "
                                                           "Focus the strip to play from the computer keyboard.");
}

//==============================================================================
void PianoRollStrip::computerKeyNote (int semitone, bool down)
{
    const int note = juce::jlimit (0, 127, computerOctave * 12 + 12 + semitone);

    if (down)
        pressKey (note, 0.8f);
    else
        releaseKey (note);
}

bool PianoRollStrip::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey && isLatchOn())
    {
        playLatched();
        return true;
    }

    if (key == juce::KeyPress::escapeKey && isLatchOn())
    {
        clearLatched();
        return true;
    }

    const auto c = (char) juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (key.getModifiers().isAnyModifierKeyDown() && ! key.getModifiers().isShiftDown())
        return false;

    if (c == 'z' || c == 'x')
    {
        computerOctave = juce::jlimit (0, 8, computerOctave + (c == 'x' ? 1 : -1));
        return true;
    }

    // The note keys are played by keyStateChanged (press and release); claiming
    // them here keeps them from the global shortcuts while the strip has focus.
    return juce::String (kComputerKeys).containsChar (c);
}

bool PianoRollStrip::keyStateChanged (bool)
{
    bool used = false;
    const juce::String keys (kComputerKeys);

    for (int i = 0; i < keys.length() && i < (int) computerDown.size(); ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (keys[i]) || juce::KeyPress::isKeyCurrentlyDown (juce::CharacterFunctions::toUpperCase (keys[i]));

        if (down != computerDown[(size_t) i])
        {
            computerDown[(size_t) i] = down;
            computerKeyNote (i, down);
            used = true;
        }
    }

    return used;
}

void PianoRollStrip::focusLost (FocusChangeType)
{
    for (size_t i = 0; i < computerDown.size(); ++i)
        if (computerDown[i])
        {
            computerDown[i] = false;
            computerKeyNote ((int) i, false);
        }
}

std::unique_ptr<juce::AccessibilityHandler> PianoRollStrip::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group);
}

//==============================================================================
void PianoRollStrip::resized()
{
    auto b = getLocalBounds();

    handleArea = {};

    if (advanced && ! isCollapsed())
        handleArea = b.removeFromBottom (5);

    header = b.removeFromLeft (kHeaderWidth);
    auto h = header.reduced (2, 2);

    if (advanced)
        collapseButton.setBounds (h.removeFromTop (juce::jmin (18, h.getHeight())).removeFromLeft (22));

    if (isCollapsed())
    {
        keysArea = rollArea = {};
        return;
    }

    // Two small rows of controls, then the latch pair when latched.
    const int rowH = juce::jmax (12, juce::jmin (18, h.getHeight() / (isLatchOn() ? 3 : 2)));
    auto row1 = h.removeFromTop (rowH);
    modeButton.setBounds (row1.removeFromLeft (row1.getWidth() / 2).reduced (1, 0));
    latchButton.setBounds (row1.reduced (1, 0));
    h.removeFromTop (1);
    fingeringButton.setBounds (h.removeFromTop (rowH).reduced (1, 0));

    if (isLatchOn())
    {
        h.removeFromTop (1);
        auto row3 = h.removeFromTop (rowH);
        playButton.setBounds (row3.removeFromLeft (row3.getWidth() / 2).reduced (1, 0));
        clearButton.setBounds (row3.reduced (1, 0));
    }

    // Keys at the bottom (18 - 28 px), the roll above them.
    const int keysH = advanced ? juce::jlimit (18, 28, b.getHeight() / 3) : 24;
    keysArea = b.removeFromBottom (juce::jmin (keysH, b.getHeight()));
    rollArea = VisualAids::pianoRollShowsRoll() ? b : juce::Rectangle<int>();

    if (rollArea.isEmpty())
        keysArea = keysArea.getUnion (b);
}

void PianoRollStrip::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6
    const double now = lastTickMs > 0.0 ? lastTickMs : juce::Time::getMillisecondCounterHiRes();

    g.setColour (Palette::panelSunken);
    g.fillRect (getLocalBounds());

    if (isCollapsed())
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (10.0f, true));
        g.drawText ("PIANO ROLL", getLocalBounds().withTrimmedLeft (28), juce::Justification::centredLeft, false);
        return;
    }

    // ---- the roll: four seconds, right to left ------------------------------------
    if (! rollArea.isEmpty())
    {
        const auto area = rollArea.toFloat();
        const float rowH = area.getHeight() / (float) juce::jmax (1, range.numKeys());
        const float pxPerMs = area.getWidth() / (float) (PianoRollModel::kRollSeconds * 1000.0);

        g.saveState();
        g.reduceClipRegion (rollArea);

        for (const auto& bar : model.getBars())
        {
            if (bar.note < range.drawLow || bar.note > range.drawHigh)
                continue;

            const double end = bar.endMs < 0.0 ? now : bar.endMs;
            const float x1 = area.getRight() - (float) (now - bar.startMs) * pxPerMs;
            const float x2 = area.getRight() - (float) (now - end) * pxPerMs;
            const float y = area.getBottom() - (float) (bar.note - range.drawLow + 1) * rowH;
            const auto r = juce::Rectangle<float> (x1, y, juce::jmax (2.0f, x2 - x1), juce::jmax (1.5f, rowH));

            g.setColour (StringColours::forString (bar.string, model.numStrings).withAlpha (PianoRollModel::kBarAlpha));
            g.fillRect (r);

            // Bent past 20 cents: a small tick at the start.
            if (bar.bent)
            {
                g.setColour (Palette::textPrimary.withAlpha (0.8f));
                g.drawVerticalLine ((int) x1, r.getY() - 2.0f, r.getBottom() + 2.0f);
            }
        }

        g.restoreState();
        g.setColour (Palette::edge);
        g.drawHorizontalLine (rollArea.getBottom() - 1, (float) rollArea.getX(), (float) rollArea.getRight());
    }

    // ---- the keys -------------------------------------------------------------------
    const auto& latched = getLatched();

    auto drawKey = [&] (int n)
    {
        const auto k = getKeyBounds (n);
        const bool black = isBlack (n);
        auto base = black ? juce::Colour (0xff1c1c1c) : juce::Colour (0xfff2eee6);

        if (! range.isPlayable (n))
            base = base.interpolatedWith (Palette::panelSunken, 0.55f);   // outside the guitar: dimmed, still playable

        g.setColour (base);
        g.fillRect (k.reduced (black ? 0.0f : 0.5f, 0.0f));

        if (const float a = model.keyAlpha (n, now); a > 0.0f)
        {
            g.setColour (model.getKey (n).colour.withAlpha (a));
            g.fillRect (k.reduced (1.0f, 0.0f));
        }

        if (latched.contains (n) || held.contains (n))
        {
            g.setColour (Palette::accent);
            g.fillEllipse (k.getCentreX() - 2.5f, k.getBottom() - 8.0f, 5.0f, 5.0f);
        }

        if (unreachable.contains (n))   // "below this guitar's range"
        {
            g.setColour (Palette::clip);
            const auto c = k.getCentre().withY (k.getBottom() - 12.0f);
            g.drawLine (c.x - 3.0f, c.y - 3.0f, c.x + 3.0f, c.y + 3.0f, 1.2f);
            g.drawLine (c.x - 3.0f, c.y + 3.0f, c.x + 3.0f, c.y - 3.0f, 1.2f);
        }

        if (! black)
        {
            g.setColour (Palette::edge);
            g.drawVerticalLine ((int) k.getRight(), k.getY(), k.getBottom());

            // Each C named; middle C marked.
            if (n % 12 == 0 && k.getWidth() >= 7.0f)
            {
                g.setColour (n == 60 ? Palette::accent : juce::Colour (0xff555047));
                g.setFont (Fonts::ui (juce::jmin (9.0f, k.getWidth() * 0.9f), n == 60));
                g.drawText (juce::String (noteNames[0]) + juce::String (n / 12 - 1),
                            k.withTrimmedTop (k.getHeight() - 11.0f).expanded (4.0f, 0.0f).toNearestInt(),
                            juce::Justification::centred, false);
            }
        }
    };

    for (int n = range.drawLow; n <= range.drawHigh; ++n)
        if (! isBlack (n))
            drawKey (n);

    for (int n = range.drawLow; n <= range.drawHigh; ++n)
        if (isBlack (n))
            drawKey (n);

    if (! handleArea.isEmpty())
    {
        g.setColour (Palette::edge);
        g.fillRect (handleArea.withSizeKeepingCentre (28, 2));
    }

    if (hasKeyboardFocus (true))
    {
        g.setColour (Palette::accent.withAlpha (0.6f));
        g.drawRect (getLocalBounds(), 1);
    }
}

} // namespace luthier
