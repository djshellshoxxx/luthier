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
    startTimerHz (kRefreshHz);   // SPEC-SWEEP GD-2
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
    /*  This used to set a number the fretboard drew and nothing else ever read.
        docs/USER_MANUAL.md has promised a right-click capo since it was written,
        and this was it: a capo that moved a line on a picture while every note
        carried on sounding exactly as before.

        It drives the parameter now, which is the one capo the build has
        (ambiguity-resolutions 4.5, gui-integration 19). Twelve is the parameter's
        range - the neck is usually longer, and a capo past the twelfth fret is a
        different instrument rather than a capo. */
    const int wanted = juce::jlimit (0, juce::jmin (12, numFrets), fret);

    if (auto* p = processor.getState().getParameter (ParamIDs::capoFret))
        p->setValueNotifyingHost ((float) wanted / 12.0f);

    capoFret = wanted;
    repaint();
}

void FretboardComponent::setStringMuted (int stringIndex, bool isMuted)
{
    if (juce::isPositiveAndBelow (stringIndex, 12))
    {
        muted[(size_t) stringIndex] = isMuted;

        // SPEC-SWEEP (UW-5): through the processor's command queue; the string
        // belongs to the audio thread.
        processor.setStringMuted (stringIndex, isMuted);

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

    /*  The capo is read back rather than only written, so the drawn one follows
        a preset load, host automation, the Advanced column's control and the
        Rhythm panel's up/down buttons. Before it was a parameter this could not
        move except by right-clicking this component, which is why it never
        disagreed with anything - there was nothing to disagree with. */
    const int engineCapo = engine.getTuningEngine().getCapoFret();

    bool changed = (strings != numStrings) || (spec.maxFrets != numFrets)
                     || (engineCapo != capoFret);

    capoFret = engineCapo;

    numStrings = juce::jlimit (1, 12, strings);
    numFrets = juce::jlimit (12, 27, spec.maxFrets);

    updateFretCells();   // SPEC-SWEEP: A11Y-8 (cheap unless something changed)

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

    // ---- the slide bar ----------------------------------------------------------
    {
        const auto& slide = engine.getSlideEngine();
        const double target = slide.getOverlayFret();

        // 80 ms ease at the rate this runs at (SPEC-SWEEP GD-2: 60 Hz now).
        const double ease = 1.0 - std::exp (-(1.0 / (double) kRefreshHz) / 0.080);

        // SPEC-SWEEP (GD-14): when the bar last moved, for the stale dim.
        if (std::abs (target - barLastTarget) > 1.0e-4)
        {
            barLastTarget = target;
            barLastMoveMs = juce::Time::getMillisecondCounterHiRes();
            changed = true;
        }

        if (target >= 0.0)
        {
            barFret = barFret < 0.0 ? target : barFret + (target - barFret) * ease;
            barOpacity += (1.0f - barOpacity) * (float) ease;
        }
        else
        {
            barOpacity -= barOpacity * (float) ease;

            if (barOpacity < 0.01f)
            {
                barOpacity = 0.0f;
                barFret = -1.0;
            }
        }

        barSlantDegrees = (float) slide.getSettings().slantDegrees;
        barColour = juce::Colour (getSlideMaterial (slide.getBar().material).colour);

        if (barOpacity > 0.0f)
            changed = true;
    }

    // ---- notation-export 3: the current bar as tablature dots (MODEL-GAPS) --------
    {
        const auto before = tabDots.size();
        const auto newestBefore = tabDots.empty() ? -1.0 : tabDots.back().fret;
        refreshTabDots();

        if (tabDots.size() != before || (! tabDots.empty() && tabDots.back().fret != newestBefore))
            changed = true;
    }
    if (refreshRealismB())   // REALISM-B: contact rings, palm bands, tool glyphs
        changed = true;

    if (changed)
        repaint();
}

void FretboardComponent::refreshTabDots()
{
    tabDots.clear();

    if (! processor.isShowingTabDotsOnFretboard())
        return;

    const auto& capture = processor.getPerformanceCapture();
    const auto& notes = capture.getNotes();

    if (notes.empty())
        return;

    // The bar the newest note is in: on the host's grid when it was played in
    // time, otherwise the last bar's worth of seconds at the host tempo.
    const auto& newest = notes.back();
    const double bpm = juce::jmax (20.0, processor.getHostTempo());
    const auto& meters = capture.getMeters();
    const int numerator = meters.empty() ? 4 : meters.back().numerator;
    const int denominator = meters.empty() ? 4 : meters.back().denominator;
    const double barBeats = (double) numerator * 4.0 / (double) juce::jmax (1, denominator);

    std::vector<size_t> inBar;

    if (newest.musical)
    {
        const double barStart = std::floor (newest.startPpq / barBeats + 1.0e-9) * barBeats;

        for (size_t i = notes.size(); i-- > 0;)
        {
            if (! notes[i].musical || notes[i].startPpq < barStart)
                break;

            inBar.push_back (i);
        }
    }
    else
    {
        const double barSeconds = barBeats * 60.0 / bpm;
        const auto from = newest.startSample - (juce::int64) (barSeconds * juce::jmax (1.0, processor.getSampleRate()));

        for (size_t i = notes.size(); i-- > 0;)
        {
            if (notes[i].startSample <= from)
                break;

            inBar.push_back (i);
        }
    }

    const float n = (float) juce::jmax ((size_t) 1, inBar.size());

    // Oldest first, so the newest is drawn on top.
    for (size_t k = inBar.size(); k-- > 0;)
    {
        const auto& note = notes[inBar[k]];
        tabDots.push_back ({ note.stringIndex, note.fret, (float) k / n });
    }
}

//==============================================================================
void FretboardComponent::resized()
{
    boardArea = getLocalBounds().reduced (Metrics::grid, compact ? 2 : Metrics::gridHalf);

    if (! compact)
        boardArea.removeFromBottom (14);   // fret number row

    layoutFretCells();   // SPEC-SWEEP: A11Y-8
}

//==============================================================================
// SPEC-SWEEP: A11Y-8 - the painted board, readable cell by cell.
class FretboardComponent::FretCell : public juce::Component
{
public:
    FretCell()
    {
        setInterceptsMouseClicks (false, false);
        setWantsKeyboardFocus (false);   // 150 tab stops would bury the rest
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::cell);
    }
};

juce::Component* FretboardComponent::getFretCell (int stringIndex, int fret) const
{
    const int index = stringIndex * (numFrets + 1) + fret;
    return juce::isPositiveAndBelow (stringIndex, numStrings) && juce::isPositiveAndBelow (fret, numFrets + 1)
             ? fretCells[index] : nullptr;
}

void FretboardComponent::updateFretCells()
{
    const auto& tuning = processor.getEngine().getTuningEngine();

    auto noteAt = [&tuning] (int s, int f)
    {
        const double hz = tuning.computeFrequency (s, (double) f, 0.0);
        return (int) std::round (hzToMidi (hz, tuning.getConcertA()));
    };

    juce::String signature;
    signature << numStrings << '/' << numFrets << '/' << capoFret;

    for (int s = 0; s < numStrings; ++s)
        signature << ',' << noteAt (s, 0);

    if (signature == fretCellsSignature)
        return;

    fretCellsSignature = signature;

    const int wanted = numStrings * (numFrets + 1);

    while (fretCells.size() > wanted)
        fretCells.removeLast();

    while (fretCells.size() < wanted)
        addAndMakeVisible (fretCells.add (new FretCell()));

    for (int s = 0; s < numStrings; ++s)
        for (int f = 0; f <= numFrets; ++f)
        {
            juce::String title ("String " + juce::String (s + 1) + ", "
                                + (f == 0 ? juce::String ("open") : "fret " + juce::String (f)) + ", "
                                + juce::MidiMessage::getMidiNoteName (noteAt (s, f), true, true, 4));

            if (f > 0 && f <= capoFret)
                title << " (behind the capo)";

            fretCells[s * (numFrets + 1) + f]->setTitle (title);
        }

    layoutFretCells();
}

void FretboardComponent::layoutFretCells()
{
    if (fretCells.size() != numStrings * (numFrets + 1))
        return;

    const float spacing = (float) boardArea.getHeight() / (float) juce::jmax (1, numStrings);

    for (int s = 0; s < numStrings; ++s)
        for (int f = 0; f <= numFrets; ++f)
        {
            const float left = f == 0 ? (float) boardArea.getX() : fretX (f - 1);
            const float right = fretX (f);
            fretCells[s * (numFrets + 1) + f]->setBounds (
                juce::Rectangle<float> (left, stringY (s) - spacing * 0.5f,
                                        juce::jmax (1.0f, right - left), spacing).toNearestInt());
        }
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

    // ---- tablature dots (notation-export 3, MODEL-GAPS) ------------------------------
    for (const auto& dot : tabDots)
    {
        if (! juce::isPositiveAndBelow (dot.stringIndex, numStrings))
            continue;

        // A fretted note sits between its fret wire and the one before; open at the nut.
        const float x = dot.fret <= 0.0 ? fretX (0.0) - 6.0f
                                        : 0.5f * (fretX (dot.fret) + fretX (juce::jmax (0.0, dot.fret - 1.0)));
        const float y = stringY (dot.stringIndex);
        const float r = compact ? 5.0f : 7.0f;
        const auto colour = Palette::secondary.withAlpha (1.0f - 0.6f * dot.age);

        g.setColour (colour);
        g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
        g.setColour (Palette::backgroundDeep);
        g.setFont (Fonts::ui (compact ? 8.0f : 9.0f, true));
        g.drawText (juce::String (juce::roundToInt (dot.fret)), juce::Rectangle<float> (x - r, y - r, 2.0f * r, 2.0f * r),
                    juce::Justification::centred, false);
    }

    // ---- slide bar (gui-integration 21) -------------------------------------------
    // A 6 px rounded bar in the material's colour at 80%, over the strings at
    // the bar's position and rotated by its slant.
    if (barOpacity > 0.0f && barFret >= 0.0)
    {
        const float x = fretX (barFret);
        const auto centre = juce::Point<float> (x, (float) boardArea.getCentreY());
        const float height = (float) boardArea.getHeight() + 6.0f;

        juce::Path bar;
        bar.addRoundedRectangle (-3.0f, -height * 0.5f, 6.0f, height, 3.0f);
        bar.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (barSlantDegrees))
                              .translated (centre));

        g.setColour (barColour.withAlpha (slideBarAlpha (barOpacity,
                                                          juce::Time::getMillisecondCounterHiRes() - barLastMoveMs)));
        g.fillPath (bar);
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

    paintRealismB (g);   // REALISM-B

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
