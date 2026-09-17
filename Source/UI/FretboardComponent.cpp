#include "FretboardComponent.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    /** Semitone sets for each overlay, relative to the root. */
    struct ScaleDef { const char* name; int notes[12]; int count; };

    const ScaleDef kScales[(size_t) ScaleOverlay::NumScales] =
    {
        { "None",             { 0 }, 0 },
        { "Major",            { 0, 2, 4, 5, 7, 9, 11 }, 7 },
        { "Natural Minor",    { 0, 2, 3, 5, 7, 8, 10 }, 7 },
        { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 }, 7 },
        { "Major Pentatonic", { 0, 2, 4, 7, 9 }, 5 },
        { "Minor Pentatonic", { 0, 3, 5, 7, 10 }, 5 },
        { "Blues",            { 0, 3, 5, 6, 7, 10 }, 6 },
        { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 }, 7 },
        { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 }, 7 },
        { "Lydian",           { 0, 2, 4, 6, 7, 9, 11 }, 7 },
        { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 }, 7 },
        { "Whole Tone",       { 0, 2, 4, 6, 8, 10 }, 6 },
        { "Chromatic",        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, 12 }
    };

    /** Frets that carry a position marker on virtually every guitar. */
    bool hasInlay (int fret) noexcept
    {
        switch (fret)
        {
            case 3: case 5: case 7: case 9: case 15: case 17: case 19: case 21:
                return true;
            case 12: case 24:
                return true;   // double dot
            default:
                return false;
        }
    }

    bool hasDoubleInlay (int fret) noexcept { return fret == 12 || fret == 24; }
}

const char* getScaleName (ScaleOverlay s) noexcept
{
    return kScales[(size_t) juce::jlimit (0, (int) ScaleOverlay::NumScales - 1, (int) s)].name;
}

//==============================================================================
FretboardComponent::FretboardComponent (LuthierAudioProcessor& p)
    : processor (p)
{
    muted.fill (false);
    liveFret.fill (0.0);
    liveLevel.fill (0.0);
    liveNote.fill (-1);

    setTooltip ("Click a fret to hear that note. Right-click for string options.");
    startTimerHz (30);
}

FretboardComponent::~FretboardComponent()
{
    stopTimer();
}

//==============================================================================
void FretboardComponent::setScaleOverlay (ScaleOverlay s, int root)
{
    scale = s;
    scaleRoot = ((root % 12) + 12) % 12;
    repaint();
}

void FretboardComponent::setCapoFret (int fret)
{
    capoFret = juce::jlimit (0, numFrets, fret);
    repaint();
}

void FretboardComponent::setStringMuted (int stringIndex, bool isMuted)
{
    if (juce::isPositiveAndBelow (stringIndex, 12))
    {
        muted[(size_t) stringIndex] = isMuted;

        auto& engine = processor.getEngine();
        engine.getString (stringIndex).setDamping (
            isMuted ? StringEngine::Damping::Choked : StringEngine::Damping::Open, 1.0);

        repaint();
    }
}

bool FretboardComponent::isStringMuted (int stringIndex) const
{
    return juce::isPositiveAndBelow (stringIndex, 12) && muted[(size_t) stringIndex];
}

void FretboardComponent::setSelectedString (int stringIndex)
{
    selectedString = juce::jlimit (0, juce::jmax (0, numStrings - 1), stringIndex);
    repaint();
}

void FretboardComponent::setCompact (bool shouldBeCompact)
{
    compact = shouldBeCompact;
    resized();
    repaint();
}

//==============================================================================
void FretboardComponent::timerCallback()
{
    auto& engine = processor.getEngine();

    const int strings = engine.getNumStrings();
    const auto& spec = engine.getGuitarSpec();

    bool changed = (strings != numStrings) || (spec.maxFrets != numFrets);

    numStrings = juce::jlimit (1, 12, strings);
    numFrets = juce::jlimit (12, 27, spec.maxFrets);

    for (int s = 0; s < numStrings; ++s)
    {
        const double level = engine.getStringLevel (s);
        const double fret = engine.getStringFret (s);
        const int note = engine.getStringMidiNote (s);

        if (std::abs (level - liveLevel[(size_t) s]) > 0.0008
            || std::abs (fret - liveFret[(size_t) s]) > 0.01
            || note != liveNote[(size_t) s])
            changed = true;

        liveLevel[(size_t) s] = level;
        liveFret[(size_t) s] = fret;
        liveNote[(size_t) s] = note;
    }

    if (changed)
        repaint();
}

//==============================================================================
void FretboardComponent::resized()
{
    boardArea = getLocalBounds().reduced (Metrics::grid, compact ? 2 : Metrics::gridHalf);

    if (! compact)
        boardArea.removeFromBottom (14);   // fret number row
}

float FretboardComponent::fretX (double fret) const
{
    // The real rule: distance from the nut = scale * (1 - 1 / 2^(n/12)).
    // Drawing frets evenly spaced is the single most obvious way to get a
    // fretboard wrong, and a guitarist spots it instantly.
    const double total = 1.0 - 1.0 / std::pow (2.0, (double) numFrets / 12.0);
    const double d = 1.0 - 1.0 / std::pow (2.0, juce::jmax (0.0, fret) / 12.0);

    const float nutX = (float) boardArea.getX() + 10.0f;
    const float endX = (float) boardArea.getRight();

    return nutX + (float) (d / juce::jmax (1.0e-6, total)) * (endX - nutX);
}

double FretboardComponent::fretAtX (float x) const
{
    const float nutX = (float) boardArea.getX() + 10.0f;
    const float endX = (float) boardArea.getRight();

    if (x <= nutX)
        return 0.0;

    const double total = 1.0 - 1.0 / std::pow (2.0, (double) numFrets / 12.0);
    const double t = juce::jlimit (0.0, 1.0, (double) ((x - nutX) / juce::jmax (1.0f, endX - nutX)));
    const double d = t * total;

    return -12.0 * std::log2 (juce::jmax (1.0e-6, 1.0 - d));
}

float FretboardComponent::stringY (int stringIndex) const
{
    const float h = (float) boardArea.getHeight();
    const float spacing = h / (float) juce::jmax (1, numStrings);

    return (float) boardArea.getY() + spacing * ((float) stringIndex + 0.5f);
}

int FretboardComponent::stringAtY (float y) const
{
    const float h = (float) boardArea.getHeight();
    const float spacing = h / (float) juce::jmax (1, numStrings);

    return juce::jlimit (0, numStrings - 1, (int) ((y - (float) boardArea.getY()) / juce::jmax (1.0f, spacing)));
}

//==============================================================================
int FretboardComponent::pitchClassAt (int stringIndex, int fret) const
{
    const auto& tuning = processor.getEngine().getTuningEngine();
    const double hz = tuning.computeFrequency (stringIndex, (double) fret, 0.0);
    const int note = (int) std::round (hzToMidi (hz, tuning.getConcertA()));

    return ((note % 12) + 12) % 12;
}

bool FretboardComponent::isNoteInScale (int stringIndex, int fret) const
{
    if (scale == ScaleOverlay::None)
        return false;

    const auto& def = kScales[(size_t) scale];
    const int pc = pitchClassAt (stringIndex, fret);
    const int relative = ((pc - scaleRoot) % 12 + 12) % 12;

    for (int i = 0; i < def.count; ++i)
        if (def.notes[i] == relative)
            return true;

    return false;
}

//==============================================================================
void FretboardComponent::paint (juce::Graphics& g)
{
    if (boardArea.isEmpty())
        return;

    const float nutX = fretX (0.0);

    // ---- board --------------------------------------------------------------
    auto board = boardArea.toFloat().withLeft (nutX);

    juce::ColourGradient wood (Palette::panelSunken.brighter (0.06f), board.getX(), board.getY(),
                               Palette::panelSunken, board.getX(), board.getBottom(), false);
    g.setGradientFill (wood);
    g.fillRoundedRectangle (board, 2.0f);

    // ---- inlays (behind the strings) -----------------------------------------
    for (int fret = 1; fret <= numFrets; ++fret)
    {
        if (! hasInlay (fret))
            continue;

        const float x = (fretX (fret - 1) + fretX (fret)) * 0.5f;
        const float radius = 3.2f;

        g.setColour (Palette::edgeBright.withAlpha (0.55f));

        if (hasDoubleInlay (fret))
        {
            const float y1 = (float) boardArea.getY() + boardArea.getHeight() * 0.28f;
            const float y2 = (float) boardArea.getY() + boardArea.getHeight() * 0.72f;
            g.fillEllipse (x - radius, y1 - radius, radius * 2.0f, radius * 2.0f);
            g.fillEllipse (x - radius, y2 - radius, radius * 2.0f, radius * 2.0f);
        }
        else
        {
            const float y = (float) boardArea.getCentreY();
            g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        }
    }

    // ---- scale overlay --------------------------------------------------------
    if (scale != ScaleOverlay::None)
    {
        for (int s = 0; s < numStrings; ++s)
        {
            for (int fret = 0; fret <= numFrets; ++fret)
            {
                if (! isNoteInScale (s, fret))
                    continue;

                const float x = (fret == 0) ? nutX - 5.0f : (fretX (fret - 1) + fretX (fret)) * 0.5f;
                const float y = stringY (s);

                const bool isRoot = (pitchClassAt (s, fret) == scaleRoot);

                g.setColour (isRoot ? Palette::secondary.withAlpha (0.55f)
                                    : Palette::secondary.withAlpha (0.22f));
                g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
            }
        }
    }

    // ---- frets ---------------------------------------------------------------
    for (int fret = 1; fret <= numFrets; ++fret)
    {
        const float x = fretX (fret);

        g.setColour (Palette::edgeBright.withAlpha (0.8f));
        g.drawVerticalLine (juce::roundToInt (x), (float) boardArea.getY(), (float) boardArea.getBottom());
    }

    // ---- nut ------------------------------------------------------------------
    g.setColour (Palette::textMuted);
    g.fillRect (nutX - 3.0f, (float) boardArea.getY(), 3.0f, (float) boardArea.getHeight());

    // ---- capo ------------------------------------------------------------------
    if (capoFret > 0)
    {
        const float x = (fretX (capoFret - 1) + fretX (capoFret)) * 0.5f;

        g.setColour (Palette::accent.withAlpha (0.35f));
        g.fillRect (x - 3.0f, (float) boardArea.getY() - 2.0f, 6.0f, (float) boardArea.getHeight() + 4.0f);

        g.setColour (Palette::accent);
        g.drawRect (x - 3.0f, (float) boardArea.getY() - 2.0f, 6.0f, (float) boardArea.getHeight() + 4.0f, 1.0f);
    }

    // ---- strings ---------------------------------------------------------------
    for (int s = 0; s < numStrings; ++s)
    {
        const float y = stringY (s);

        // Thicker line for the lower strings, as they really are.
        const float thickness = 0.9f + 1.5f * ((float) s / (float) juce::jmax (1, numStrings - 1));

        const bool isMuted = isStringMuted (s);
        const bool isSelected = (s == selectedString);

        auto colour = isMuted ? Palette::textDisabled
                    : isSelected ? Palette::textPrimary
                                 : Palette::textMuted.brighter (0.25f);

        // A ringing string is drawn brighter and, at high level, slightly blurred
        // to suggest movement.
        const double level = liveLevel[(size_t) s];

        if (level > 0.0015)
        {
            const float excitement = (float) juce::jlimit (0.0, 1.0, level * 14.0);

            g.setColour (Palette::accent.withAlpha (excitement * 0.30f));
            g.drawLine ((float) boardArea.getX(), y, (float) boardArea.getRight(), y,
                        thickness + excitement * 3.5f);

            colour = colour.interpolatedWith (Palette::accentBright, excitement * 0.8f);
        }

        g.setColour (colour);
        g.drawLine ((float) boardArea.getX(), y, (float) boardArea.getRight(), y, thickness);
    }

    // ---- sounding notes ----------------------------------------------------------
    for (int s = 0; s < numStrings; ++s)
    {
        if (liveNote[(size_t) s] < 0 && liveLevel[(size_t) s] < 0.002)
            continue;

        const double fret = liveFret[(size_t) s];
        const float x = (fret < 0.05) ? nutX - 6.0f
                                      : (fretX (juce::jmax (0.0, fret - 1.0)) + fretX (fret)) * 0.5f;
        const float y = stringY (s);

        const float alpha = (float) juce::jlimit (0.15, 1.0, liveLevel[(size_t) s] * 18.0);
        const float radius = 7.0f;

        g.setColour (Palette::accent.withAlpha (alpha * 0.35f));
        g.fillEllipse (x - radius * 1.7f, y - radius * 1.7f, radius * 3.4f, radius * 3.4f);

        g.setColour (Palette::accentBright.withAlpha (alpha));
        g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);

        // The note name, if there is room.
        if (radius >= 6.0f && liveNote[(size_t) s] >= 0)
        {
            g.setColour (Palette::backgroundDeep);
            g.setFont (Fonts::mono (8.5f));
            g.drawText (TuningEngine::noteName (liveNote[(size_t) s]),
                        juce::Rectangle<float> (x - 14.0f, y - 6.0f, 28.0f, 12.0f),
                        juce::Justification::centred, false);
        }
    }

    // ---- hover ---------------------------------------------------------------------
    if (hoverString >= 0 && hoverFret >= 0)
    {
        const float x = (hoverFret == 0) ? nutX - 6.0f
                                         : (fretX (hoverFret - 1) + fretX (hoverFret)) * 0.5f;
        const float y = stringY (hoverString);

        g.setColour (Palette::textPrimary.withAlpha (0.45f));
        g.drawEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f, 1.2f);
    }

    // ---- fret numbers ----------------------------------------------------------------
    if (! compact)
    {
        auto numberRow = getLocalBounds().removeFromBottom (14);

        g.setFont (Fonts::mono (9.0f));

        for (int fret = 0; fret <= numFrets; ++fret)
        {
            if (fret != 0 && ! hasInlay (fret))
                continue;

            const float x = (fret == 0) ? nutX - 6.0f : (fretX (fret - 1) + fretX (fret)) * 0.5f;

            g.setColour (hasDoubleInlay (fret) ? Palette::textMuted : Palette::textDisabled);
            g.drawText (juce::String (fret),
                        juce::Rectangle<float> (x - 12.0f, (float) numberRow.getY(), 24.0f, 12.0f),
                        juce::Justification::centred, false);
        }
    }
}

//==============================================================================
void FretboardComponent::mouseMove (const juce::MouseEvent& e)
{
    const int s = stringAtY ((float) e.y);
    const int f = juce::jlimit (0, numFrets, (int) std::ceil (fretAtX ((float) e.x)));

    if (s != hoverString || f != hoverFret)
    {
        hoverString = s;
        hoverFret = f;
        repaint();
    }
}

void FretboardComponent::mouseExit (const juce::MouseEvent&)
{
    hoverString = -1;
    hoverFret = -1;
    repaint();
}

void FretboardComponent::mouseDown (const juce::MouseEvent& e)
{
    const int s = stringAtY ((float) e.y);
    const int f = juce::jlimit (0, numFrets, (int) std::ceil (fretAtX ((float) e.x)));

    if (! juce::isPositiveAndBelow (s, numStrings))
        return;

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());

        menu.addSectionHeader ("String " + juce::String (s + 1)
                               + "  |  fret " + juce::String (f));
        menu.addItem (1, "Mute string", true, isStringMuted (s));
        menu.addItem (2, "Select string", true, s == selectedString);
        menu.addSeparator();
        menu.addItem (3, "Set capo here", f > 0, capoFret == f);
        menu.addItem (4, "Remove capo", capoFret > 0);
        menu.addSeparator();

        juce::PopupMenu scaleMenu;

        for (int i = 0; i < (int) ScaleOverlay::NumScales; ++i)
            scaleMenu.addItem (100 + i, getScaleName ((ScaleOverlay) i), true, (int) scale == i);

        menu.addSubMenu ("Scale overlay", scaleMenu);

        juce::PopupMenu rootMenu;

        for (int i = 0; i < 12; ++i)
            rootMenu.addItem (200 + i, TuningEngine::noteName (60 + i).dropLastCharacters (1),
                              true, scaleRoot == i);

        menu.addSubMenu ("Scale root", rootMenu);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this, s, f] (int result)
        {
            if (result == 1)      setStringMuted (s, ! isStringMuted (s));
            else if (result == 2) { setSelectedString (s); if (onStringSelected) onStringSelected (s); }
            else if (result == 3) setCapoFret (f);
            else if (result == 4) setCapoFret (0);
            else if (result >= 100 && result < 100 + (int) ScaleOverlay::NumScales)
                setScaleOverlay ((ScaleOverlay) (result - 100), scaleRoot);
            else if (result >= 200 && result < 212)
                setScaleOverlay (scale, result - 200);
        });

        return;
    }

    if (isStringMuted (s))
        return;

    setSelectedString (s);

    if (onStringSelected)
        onStringSelected (s);

    // Velocity from the vertical position within the string's lane: higher up the
    // lane is a harder pick, which makes the fretboard playable rather than a
    // fixed-velocity trigger pad.
    const float laneHeight = (float) boardArea.getHeight() / (float) juce::jmax (1, numStrings);
    const float withinLane = juce::jlimit (0.0f, 1.0f,
                                           ((float) e.y - (stringY (s) - laneHeight * 0.5f)) / laneHeight);

    const double velocity = juce::jlimit (0.25, 1.0, 0.35 + (1.0 - withinLane) * 0.65);

    const int effectiveFret = juce::jmax (f, capoFret);

    processor.triggerPreviewNote (s, (double) effectiveFret, velocity);
    playingString = s;
}

void FretboardComponent::mouseUp (const juce::MouseEvent&)
{
    if (playingString >= 0)
    {
        processor.releasePreviewNote (playingString);
        playingString = -1;
    }
}

} // namespace luthier
