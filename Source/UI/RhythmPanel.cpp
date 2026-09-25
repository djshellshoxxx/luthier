#include "RhythmPanel.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    /** The glyph a strum type is drawn with. Direction is shown by an arrow, and
        a muted stroke by the dot beside it, so that the grid stays readable when
        the cells are only twenty pixels wide. */
    const char* strumGlyph (StrumType type) noexcept
    {
        switch (type)
        {
            case StrumType::down:      return "v";
            case StrumType::up:        return "^";
            case StrumType::downMute:  return "v.";
            case StrumType::upMute:    return "^.";
            case StrumType::rake:      return "R";
            case StrumType::rasgueado: return "*";
            case StrumType::chuck:     return "x";
            case StrumType::rest:
            case StrumType::numTypes:
            default:                   return "";
        }
    }

    juce::Colour strumColour (StrumType type) noexcept
    {
        if (type == StrumType::rest)
            return Palette::panelSunken;

        if (isMutedStrum (type) || type == StrumType::chuck)
            return Palette::secondaryDim;

        if (type == StrumType::rasgueado)
            return Palette::accentBright;

        return Palette::accent;
    }

    void drawGridCell (juce::Graphics& g, juce::Rectangle<int> bounds,
                       const juce::String& text, juce::Colour fill,
                       bool playing, bool onBeat)
    {
        const auto area = bounds.toFloat().reduced (1.0f);

        g.setColour (fill);
        g.fillRoundedRectangle (area, 2.0f);

        // The downbeats are outlined more brightly, so the eye can count in four
        // without reading the ruler.
        g.setColour (playing ? Palette::textPrimary
                             : (onBeat ? Palette::edgeBright : Palette::edge));
        g.drawRoundedRectangle (area, 2.0f, playing ? 2.0f : 1.0f);

        if (text.isNotEmpty())
        {
            g.setColour (fill.getPerceivedBrightness() > 0.5f ? Palette::backgroundDeep
                                                              : Palette::textPrimary);
            g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
            g.drawText (text, bounds, juce::Justification::centred, false);
        }
    }

    void styleSectionLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        label.setColour (juce::Label::textColourId, Palette::accent);
    }

    void styleValueSlider (juce::Slider& slider, double minimum, double maximum,
                           double interval, const juce::String& suffix)
    {
        slider.setRange (minimum, maximum, interval);
        slider.setTextValueSuffix (suffix);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }
}

//==============================================================================
StrumGrid::StrumGrid (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Strum pattern. Click a step to cycle its direction, "
                "right-click for dynamic, string mask and delete.");

    pattern = processor.getEngine().getRhythmEngine().getPattern();
}

StrumGrid::~StrumGrid() = default;

void StrumGrid::refresh()
{
    auto current = processor.getEngine().getRhythmEngine().getPattern();

    // Only take a pattern the engine changed under us; otherwise an edit in
    // progress would be overwritten by the panel's own timer.
    if (current.getName() != pattern.getName()
          || current.getLength() != pattern.getLength()
          || current.getKind() != pattern.getKind())
    {
        pattern = current;
        repaint();
    }
}

void StrumGrid::setPlayingStep (int step)
{
    if (step == playingStep)
        return;

    playingStep = step;
    repaint();
}

juce::Rectangle<int> StrumGrid::cellBounds (int step) const
{
    const int perRow = juce::jmax (1, (pattern.getLength() > 16) ? 16 : pattern.getLength());
    const int row = step / perRow;
    const int column = step % perRow;

    const int cellWidth = juce::jmax (1, getWidth() / perRow);

    return { column * cellWidth, row * rowHeight, cellWidth, rowHeight };
}

int StrumGrid::stepAt (juce::Point<int> position) const
{
    for (int step = 0; step < pattern.getLength(); ++step)
        if (cellBounds (step).contains (position))
            return step;

    return -1;
}

void StrumGrid::paint (juce::Graphics& g)
{
    if (pattern.getKind() != RhythmPattern::Kind::strum)
    {
        g.setColour (Palette::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("The loaded pattern is a fingerpick pattern.",
                    getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    for (int step = 0; step < pattern.getLength(); ++step)
    {
        const auto cell = pattern.getStrumStep (step);
        const auto bounds = cellBounds (step);

        auto fill = strumColour (cell.type);

        // A quieter stroke is drawn dimmer, so the dynamics of a pattern are
        // visible without opening every cell's menu.
        if (! cell.isRest())
            fill = fill.withMultipliedBrightness ((float) (0.55 + 0.45 * cell.dynamic));

        drawGridCell (g, bounds, strumGlyph (cell.type), fill,
                      step == playingStep, (step % 4) == 0);
    }
}

void StrumGrid::resized() {}

void StrumGrid::mouseDown (const juce::MouseEvent& event)
{
    if (pattern.getKind() != RhythmPattern::Kind::strum)
        return;

    const int step = stepAt (event.getPosition());

    if (step < 0)
        return;

    if (event.mods.isPopupMenu())
    {
        showStepMenu (step);
        return;
    }

    // Left-click cycles through the stroke types, rest included, so a step can be
    // cleared without going to the menu.
    auto cell = pattern.getStrumStep (step);
    cell.type = (StrumType) (((int) cell.type + 1) % (int) StrumType::numTypes);
    pattern.setStrumStep (step, cell);

    commit();
    repaint();
}

void StrumGrid::showStepMenu (int step)
{
    auto cell = pattern.getStrumStep (step);

    juce::PopupMenu menu;

    juce::PopupMenu dynamicMenu;
    const int dynamics[] = { 100, 85, 70, 55, 40 };

    for (int i = 0; i < 5; ++i)
        dynamicMenu.addItem (100 + i, juce::String (dynamics[i]) + "%", true,
                             std::abs (cell.dynamic * 100.0 - dynamics[i]) < 1.0);

    menu.addSubMenu ("Dynamic", dynamicMenu);

    juce::PopupMenu maskMenu;
    struct { const char* name; uint16_t mask; } masks[] = {
        { "All strings",   0x0FFF },
        { "Top three",     0x0007 },
        { "Middle three",  0x001C },
        { "Low strings",   0x0FF8 },
        { "Bass pair",     0x0FF0 }
    };

    for (int i = 0; i < 5; ++i)
        maskMenu.addItem (200 + i, masks[i].name, true, cell.stringMask == masks[i].mask);

    menu.addSubMenu ("String mask", maskMenu);

    menu.addSeparator();
    menu.addItem (300, "Clear this step", ! cell.isRest());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this, step, dynamics, masks] (int result)
    {
        if (result == 0)
            return;

        auto edited = pattern.getStrumStep (step);

        if (result >= 100 && result < 105)
            edited.dynamic = dynamics[result - 100] / 100.0;
        else if (result >= 200 && result < 205)
            edited.stringMask = masks[result - 200].mask;
        else if (result == 300)
            edited = StrumStep {};

        pattern.setStrumStep (step, edited);
        commit();
        repaint();
    });
}

void StrumGrid::commit()
{
    processor.pushUndoAction ("Edit strum pattern", "rhythm-pattern", "strum");   // gui-integration 18
    processor.getEngine().getRhythmEngine().setPattern (pattern);

    if (onPatternEdited != nullptr)
        onPatternEdited();
}

//==============================================================================
FingerpickGrid::FingerpickGrid (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Fingerpick pattern. One row per finger: p thumb, i index, "
                "m middle, a ring, e little. Click a cell to assign that step.");

    pattern = processor.getEngine().getRhythmEngine().getPattern();
}

FingerpickGrid::~FingerpickGrid() = default;

void FingerpickGrid::refresh()
{
    auto current = processor.getEngine().getRhythmEngine().getPattern();

    if (current.getName() != pattern.getName()
          || current.getLength() != pattern.getLength()
          || current.getKind() != pattern.getKind())
    {
        pattern = current;
        repaint();
    }
}

void FingerpickGrid::setPlayingStep (int step)
{
    if (step == playingStep)
        return;

    playingStep = step;
    repaint();
}

juce::Rectangle<int> FingerpickGrid::cellBounds (int step, int finger) const
{
    const int steps = juce::jmax (1, juce::jmin (16, pattern.getLength()));
    const int cellWidth = juce::jmax (1, (getWidth() - headerWidth) / steps);

    return { headerWidth + step * cellWidth, finger * rowHeight, cellWidth, rowHeight };
}

void FingerpickGrid::paint (juce::Graphics& g)
{
    if (pattern.getKind() != RhythmPattern::Kind::fingerpick)
    {
        g.setColour (Palette::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("The loaded pattern is a strum pattern.",
                    getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    const int steps = juce::jmin (16, pattern.getLength());

    for (int finger = 0; finger < (int) Finger::numFingers; ++finger)
    {
        // The row label names the finger and the string it has been assigned, so
        // the pattern reads the way a fingerstyle player describes one.
        const auto stringIndex = pattern.getStringForFinger ((Finger) finger);

        g.setColour (Palette::textMuted);
        g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());
        g.drawText (juce::String (getFingerName ((Finger) finger)).substring (0, 1),
                    juce::Rectangle<int> (0, finger * rowHeight, headerWidth, rowHeight),
                    juce::Justification::centred, false);

        for (int step = 0; step < steps; ++step)
        {
            const auto cell = pattern.getFingerpickStep (step);
            const bool active = cell.active && (int) cell.finger == finger;

            auto fill = active ? Palette::accent.withMultipliedBrightness (
                                     (float) (0.55 + 0.45 * cell.dynamic))
                               : Palette::panelSunken;

            drawGridCell (g, cellBounds (step, finger),
                          active ? juce::String (stringIndex + 1) : juce::String(),
                          fill, active && step == playingStep, (step % 4) == 0);
        }
    }

    g.setColour (Palette::textDisabled);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("cell shows the string that finger plays",
                getLocalBounds().removeFromBottom (16), juce::Justification::centred, false);
}

void FingerpickGrid::mouseDown (const juce::MouseEvent& event)
{
    if (pattern.getKind() != RhythmPattern::Kind::fingerpick)
        return;

    const int steps = juce::jmin (16, pattern.getLength());

    for (int finger = 0; finger < (int) Finger::numFingers; ++finger)
    {
        for (int step = 0; step < steps; ++step)
        {
            if (! cellBounds (step, finger).contains (event.getPosition()))
                continue;

            auto cell = pattern.getFingerpickStep (step);

            // Clicking the cell that is already lit clears the step; clicking any
            // other cell moves the step to that finger. A step belongs to one
            // finger at a time, which is what the data model holds.
            if (cell.active && (int) cell.finger == finger)
            {
                cell = FingerpickStep {};
            }
            else
            {
                cell.active = true;
                cell.finger = (Finger) finger;
                cell.dynamic = (finger == (int) Finger::thumb) ? 1.0 : 0.78;
            }

            pattern.setFingerpickStep (step, cell);
            commit();
            repaint();
            return;
        }
    }
}

void FingerpickGrid::commit()
{
    processor.pushUndoAction ("Edit fingerpick pattern", "rhythm-pattern", "fingerpick");   // gui-integration 18
    processor.getEngine().getRhythmEngine().setPattern (pattern);

    if (onPatternEdited != nullptr)
        onPatternEdited();
}

//==============================================================================
RhythmIndicators::RhythmIndicators (LuthierAudioProcessor& p)
    : processor (p)
{
    voicedFrets.fill (-1);
}

RhythmIndicators::~RhythmIndicators() = default;

void RhythmIndicators::refresh()
{
    auto& engine = processor.getEngine().getRhythmEngine();

    const auto chord = engine.getCurrentChord();
    const auto newText = chord.isKnown() ? chord.toString() : juce::String ("--");

    const auto& voicing = engine.getCurrentVoicing();

    std::array<int, kMaxStrings> frets {};
    frets.fill (-1);

    for (int i = 0; i < voicing.numNotes; ++i)
    {
        const auto& note = voicing.notes[(size_t) i];

        if (note.valid && juce::isPositiveAndBelow (note.stringIndex, kMaxStrings))
            frets[(size_t) note.stringIndex] = (int) note.fretPosition;
    }

    const auto stroke = engine.getNextStrumType();
    const int strings = engine.getNumStrings();

    if (newText == chordText && frets == voicedFrets
          && stroke == nextStroke && strings == numStringsShown)
        return;

    chordText = newText;
    voicedFrets = frets;
    nextStroke = stroke;
    numStringsShown = strings;
    voicingIsValid = voicing.numNotes > 0;

    repaint();
}

void RhythmIndicators::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds.toFloat(), 3.0f);

    // ---- chord symbol ----------------------------------------------------------
    auto chordArea = bounds.removeFromLeft (84).reduced (6);

    g.setColour (Palette::textMuted);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("CHORD", chordArea.removeFromTop (12), juce::Justification::centredLeft, false);

    g.setColour (voicingIsValid ? Palette::accent : Palette::textDisabled);
    g.setFont (juce::Font (juce::FontOptions (20.0f)).boldened());
    g.drawText (chordText, chordArea, juce::Justification::centredLeft, false);

    // ---- next stroke -----------------------------------------------------------
    auto strokeArea = bounds.removeFromRight (56).reduced (6);

    g.setColour (Palette::textMuted);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("NEXT", strokeArea.removeFromTop (12), juce::Justification::centred, false);

    g.setColour (nextStroke == StrumType::rest ? Palette::textDisabled
                                               : strumColour (nextStroke));
    g.setFont (juce::Font (juce::FontOptions (22.0f)).boldened());
    g.drawText (nextStroke == StrumType::rest ? juce::String ("-")
                                              : juce::String (strumGlyph (nextStroke)),
                strokeArea, juce::Justification::centred, false);

    // ---- voicing as fretboard dots ---------------------------------------------
    auto fretArea = bounds.reduced (6, 8);

    if (fretArea.getWidth() < 40)
        return;

    const int strings = juce::jlimit (1, kMaxStrings, numStringsShown);
    const float rowSpacing = (float) fretArea.getHeight() / (float) juce::jmax (1, strings);

    // The window follows the voicing rather than always starting at the nut, so a
    // barre chord high on the neck is still visible in a strip this small.
    int lowest = 99, highest = 0;

    for (int s = 0; s < strings; ++s)
    {
        if (voicedFrets[(size_t) s] < 0)
            continue;

        lowest = juce::jmin (lowest, voicedFrets[(size_t) s]);
        highest = juce::jmax (highest, voicedFrets[(size_t) s]);
    }

    const int firstFret = (lowest > 90) ? 0 : juce::jmax (0, juce::jmin (lowest, highest - 4));
    const int shownFrets = 6;
    const float columnWidth = (float) fretArea.getWidth() / (float) shownFrets;

    for (int f = 0; f <= shownFrets; ++f)
    {
        const float x = (float) fretArea.getX() + columnWidth * (float) f;

        g.setColour ((firstFret + f == 0) ? Palette::edgeBright : Palette::edge);
        g.drawLine (x, (float) fretArea.getY(), x, (float) fretArea.getBottom(), 1.0f);
    }

    for (int s = 0; s < strings; ++s)
    {
        const float y = (float) fretArea.getY() + rowSpacing * ((float) s + 0.5f);

        g.setColour (Palette::edge);
        g.drawLine ((float) fretArea.getX(), y, (float) fretArea.getRight(), y, 1.0f);

        const int fret = voicedFrets[(size_t) s];

        if (fret < 0)
        {
            // A muted string is marked with an x at the nut, the way a chord
            // diagram does it - shape, not colour, so it reads without hue.
            g.setColour (Palette::textDisabled);
            g.setFont (juce::Font (juce::FontOptions (9.0f)).boldened());
            g.drawText ("x", juce::Rectangle<float> ((float) fretArea.getX() - 9.0f, y - 6.0f,
                                                     9.0f, 12.0f).toNearestInt(),
                        juce::Justification::centred, false);
            continue;
        }

        const int column = fret - firstFret;

        if (! juce::isPositiveAndBelow (column, shownFrets + 1))
            continue;

        const float x = (float) fretArea.getX() + columnWidth * ((float) column + 0.5f);

        g.setColour (Palette::accent);
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);

        g.setColour (Palette::backgroundDeep);
        g.setFont (juce::Font (juce::FontOptions (8.0f)).boldened());
        g.drawText (juce::String (fret),
                    juce::Rectangle<float> (x - 6.0f, y - 6.0f, 12.0f, 12.0f).toNearestInt(),
                    juce::Justification::centred, false);
    }

    g.setColour (Palette::textDisabled);
    g.setFont (juce::Font (juce::FontOptions (8.0f)));
    g.drawText ("fret " + juce::String (firstFret),
                fretArea.removeFromBottom (10), juce::Justification::left, false);
}

//==============================================================================
int RhythmPanel::PatternListModel::getNumRows()
{
    return owner.visiblePatterns.size();
}

void RhythmPanel::PatternListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                      int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, owner.visiblePatterns.size()))
        return;

    const auto& pattern = owner.processor.getPatternLibrary()
                            .getPattern (owner.visiblePatterns[row]);

    if (selected)
    {
        g.setColour (Palette::accentDim);
        g.fillRect (0, 0, width, height);
    }

    g.setColour (selected ? Palette::textPrimary : Palette::textMuted);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (pattern.getName(), 6, 0, width - 60, height, juce::Justification::centredLeft, true);

    // The kind is spelled out rather than colour-coded, so it survives a
    // colour-blind palette.
    g.setColour (Palette::textDisabled);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText (pattern.getKind() == RhythmPattern::Kind::strum ? "strum" : "pick",
                width - 54, 0, 48, height, juce::Justification::centredRight, false);
}

void RhythmPanel::PatternListModel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    owner.loadSelectedPattern();
}

//==============================================================================
RhythmPanel::RhythmPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    buildGenreControls();
    buildVoicingControls();

    strumGrid = std::make_unique<StrumGrid> (processor);
    fingerpickGrid = std::make_unique<FingerpickGrid> (processor);

    strumGrid->onPatternEdited = [this] { refreshFromEngine(); };
    fingerpickGrid->onPatternEdited = [this] { refreshFromEngine(); };

    addAndMakeVisible (*strumGrid);
    addAndMakeVisible (*fingerpickGrid);

    buildFeelControls();

    // gui-integration 4.4: the STRUM group (strum-dynamics 6.3) follows the feel controls.
    strumGroup = std::make_unique<StrumGroup> (processor);
    addAndMakeVisible (*strumGroup);

    buildBrowser();

    indicators = std::make_unique<RhythmIndicators> (processor);
    addAndMakeVisible (*indicators);

    styleSectionLabel (genreHeading,   "GENRE KIT");
    styleSectionLabel (voicingHeading, "VOICING");
    styleSectionLabel (strumHeading,   "STRUM PATTERN");
    styleSectionLabel (pickHeading,    "FINGERPICK PATTERN");
    styleSectionLabel (feelHeading,    "FEEL");
    styleSectionLabel (browserHeading, "PATTERN BROWSER");

    for (auto* label : { &genreHeading, &voicingHeading, &strumHeading,
                         &pickHeading, &feelHeading, &browserHeading })
        addAndMakeVisible (*label);

    refreshFromEngine();
    refreshBrowserList();

    startTimerHz (20);
}

RhythmPanel::~RhythmPanel()
{
    stopTimer();
}

RhythmEngine& RhythmPanel::rhythm()
{
    return processor.getEngine().getRhythmEngine();
}

//==============================================================================
void RhythmPanel::buildGenreControls()
{
    enableToggle = std::make_unique<LuthierToggle> ("RHYTHM ENGINE");

    // The engine's enable is not a parameter, so the toggle is driven directly
    // rather than through an attachment.
    enableToggle->getButton().setClickingTogglesState (true);
    enableToggle->getButton().onClick = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoState ("Turn rhythm engine on/off");   // action-and-undo.md (rhythm settings)
        rhythm().setEnabled (enableToggle->getButton().getToggleState());
    };

    enableToggle->setTooltip ("Turns the rhythm engine on. Off, the plugin plays "
                              "your MIDI exactly as it arrives.");
    addAndMakeVisible (*enableToggle);

    freeRunButton.setClickingTogglesState (true);
    freeRunButton.setTooltip ("Free-run: keep playing the pattern when the host "
                              "transport is stopped.");
    freeRunButton.onClick = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoState ("Toggle rhythm free-run");   // action-and-undo.md (rhythm settings)
        rhythm().setFreeRun (freeRunButton.getToggleState());
    };
    addAndMakeVisible (freeRunButton);

    modeHintLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    modeHintLabel.setColour (juce::Label::textColourId, Palette::warning);
    addAndMakeVisible (modeHintLabel);

    // ---- genre kit -------------------------------------------------------------
    processor.getGenreKits().refresh();

    int itemId = 1;

    for (const auto& name : processor.getGenreKits().getNames())
        genreBox.addItem (name, itemId++);

    genreBox.setTextWhenNothingSelected ("Choose a style");
    genreBox.onChange = [this] { if (! updatingControls) applySelectedKit(); };
    addAndMakeVisible (genreBox);

    diceButton.setTooltip ("Randomise within this style.");
    diceButton.onClick = [this] { randomiseWithinStyle(); };
    addAndMakeVisible (diceButton);

    rigHintLabel.setFont (juce::Font (juce::FontOptions (9.0f)));
    rigHintLabel.setColour (juce::Label::textColourId, Palette::textDisabled);
    addAndMakeVisible (rigHintLabel);
}

void RhythmPanel::buildVoicingControls()
{
    for (int i = 0; i < (int) VoicingStyle::numStyles; ++i)
        styleBox.addItem (getVoicingStyleName ((VoicingStyle) i), i + 1);

    styleBox.onChange = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Change voicing style", "rhythm-setting", "voicingStyle");   // action-and-undo.md (rhythm settings)
        rhythm().setVoicingStyle ((VoicingStyle) (styleBox.getSelectedId() - 1));
    };
    addAndMakeVisible (styleBox);

    styleValueSlider (densitySlider, 0.0, 100.0, 1.0, " %");
    densitySlider.onValueChange = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Change voicing density", "rhythm-setting", "density");   // action-and-undo.md (rhythm settings)
        rhythm().setVoicingDensity (densitySlider.getValue());
    };
    addAndMakeVisible (densitySlider);

    styleValueSlider (handPositionSlider, 0.0, 22.0, 1.0, " fr");
    handPositionSlider.onValueChange = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Change hand position", "rhythm-setting", "handPosition");   // action-and-undo.md (rhythm settings)
        rhythm().setHandPositionHint ((int) handPositionSlider.getValue());
    };
    addAndMakeVisible (handPositionSlider);

    capoLabel.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    capoLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    capoLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (capoLabel);

    capoDown.onClick = [this] { processor.pushUndoAction ("Change rhythm capo", "rhythm-setting", "capo"); rhythm().setCapoFret (rhythm().getCapoFret() - 1); refreshFromEngine(); };
    capoUp.onClick   = [this] { processor.pushUndoAction ("Change rhythm capo", "rhythm-setting", "capo"); rhythm().setCapoFret (rhythm().getCapoFret() + 1); refreshFromEngine(); };

    capoDown.setTooltip ("Move the capo down a fret.");
    capoUp.setTooltip ("Move the capo up a fret.");

    addAndMakeVisible (capoDown);
    addAndMakeVisible (capoUp);
}

void RhythmPanel::buildFeelControls()
{
    styleValueSlider (swingSlider, 50.0, 75.0, 1.0, " %");
    swingSlider.setTooltip ("Swing: how far the offbeats are pushed late.");
    swingSlider.onValueChange = [this]
    {
        if (updatingControls)
            return;

        // Swing lives in the pattern rather than in the engine, because it is a
        // property of the figure being played.
        processor.pushUndoAction ("Change swing", "rhythm-pattern", "swing");   // gui-integration 18
        auto pattern = rhythm().getPattern();
        pattern.setSwing (swingSlider.getValue() / 100.0);
        rhythm().setPattern (pattern);
    };
    addAndMakeVisible (swingSlider);

    styleValueSlider (timingSlider, 0.0, 40.0, 0.5, " ms");
    timingSlider.setTooltip ("Timing jitter, as a gaussian sigma.");
    timingSlider.onValueChange = [this] { pushHumaniseToEngine(); };
    addAndMakeVisible (timingSlider);

    styleValueSlider (velocitySlider, 0.0, 50.0, 1.0, " %");
    velocitySlider.setTooltip ("Velocity jitter.");
    velocitySlider.onValueChange = [this] { pushHumaniseToEngine(); };
    addAndMakeVisible (velocitySlider);

    styleValueSlider (missSlider, 0.0, 25.0, 0.5, " %");
    missSlider.setTooltip ("Chance that a scheduled stroke simply does not happen.");
    missSlider.onValueChange = [this] { pushHumaniseToEngine(); };
    addAndMakeVisible (missSlider);

    styleValueSlider (ghostSlider, 0.0, 40.0, 0.5, " %");
    ghostSlider.setTooltip ("Chance of an extra muted stroke before a hit.");
    ghostSlider.onValueChange = [this] { pushHumaniseToEngine(); };
    addAndMakeVisible (ghostSlider);
}

void RhythmPanel::buildBrowser()
{
    tagFilterBox.addItem ("All patterns", 1);
    tagFilterBox.setSelectedId (1, juce::dontSendNotification);

    // The filter offers exactly the tags the library actually carries, so it can
    // never list a tag that matches nothing.
    juce::StringArray tags;

    const auto& library = processor.getPatternLibrary();

    for (int i = 0; i < library.getNumPatterns(); ++i)
        for (const auto& tag : library.getPattern (i).getTags())
            tags.addIfNotAlreadyThere (tag);

    tags.sort (true);

    int itemId = 2;

    for (const auto& tag : tags)
        tagFilterBox.addItem (tag, itemId++);

    tagFilterBox.onChange = [this] { refreshBrowserList(); };
    addAndMakeVisible (tagFilterBox);

    patternList.setModel (&listModel);
    patternList.setRowHeight (18);
    patternList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    addAndMakeVisible (patternList);

    loadButton.onClick   = [this] { loadSelectedPattern(); };
    saveButton.onClick   = [this] { saveCurrentPattern(); };
    exportButton.onClick = [this] { exportCurrentPattern(); };

    loadButton.setTooltip ("Load the selected pattern into the engine.");
    saveButton.setTooltip ("Save the current pattern to your pattern folder.");
    exportButton.setTooltip ("Write the current pattern to a .luthierpattern file.");

    addAndMakeVisible (loadButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (exportButton);
}

//==============================================================================
void RhythmPanel::applySelectedKit()
{
    const int index = genreBox.getSelectedId() - 1;

    if (! juce::isPositiveAndBelow (index, processor.getGenreKits().getNumKits()))
        return;

    processor.pushUndoState ("Apply genre kit " + genreBox.getText());   // action-and-undo.md (rhythm settings)
    const auto preferredPreset = processor.applyGenreKit (index);

    // rhythm-engine 7: the rig is a suggestion. It is named, never loaded.
    rigHintLabel.setText (preferredPreset.isEmpty()
                            ? juce::String()
                            : "Written for the \"" + preferredPreset + "\" preset",
                          juce::dontSendNotification);

    refreshFromEngine();
}

void RhythmPanel::randomiseWithinStyle()
{
    const int index = genreBox.getSelectedId() - 1;

    if (! juce::isPositiveAndBelow (index, processor.getGenreKits().getNumKits()))
        return;

    auto random = juce::Random (juce::Time::currentTimeMillis());

    const int patternIndex = GenreKitLibrary::chooseRandomPattern (
        processor.getGenreKits().getKit (index), processor.getPatternLibrary(), random);

    if (patternIndex < 0)
        return;

    processor.pushUndoState ("Randomise rhythm pattern");   // gui-integration 18
    rhythm().setPattern (processor.getPatternLibrary().getPattern (patternIndex));
    refreshFromEngine();
}

void RhythmPanel::refreshBrowserList()
{
    visiblePatterns.clearQuick();

    const auto& library = processor.getPatternLibrary();
    const int filterId = tagFilterBox.getSelectedId();

    if (filterId <= 1)
    {
        for (int i = 0; i < library.getNumPatterns(); ++i)
            visiblePatterns.add (i);
    }
    else
    {
        visiblePatterns = library.findByTag (tagFilterBox.getText());
    }

    patternList.updateContent();
    patternList.repaint();
}

void RhythmPanel::loadSelectedPattern()
{
    const int row = patternList.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, visiblePatterns.size()))
        return;

    processor.pushUndoState ("Load rhythm pattern");   // gui-integration 18
    rhythm().setPattern (processor.getPatternLibrary().getPattern (visiblePatterns[row]));
    refreshFromEngine();
}

void RhythmPanel::saveCurrentPattern()
{
    processor.getPatternLibrary().save (rhythm().getPattern());
    processor.getPatternLibrary().refresh();

    refreshBrowserList();
}

void RhythmPanel::exportCurrentPattern()
{
    const auto pattern = rhythm().getPattern();

    chooser = std::make_unique<juce::FileChooser> (
        "Export pattern",
        PatternLibrary::getUserDirectory()
            .getChildFile (juce::File::createLegalFileName (pattern.getName())
                             + ".luthierpattern"),
        "*.luthierpattern");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::warnAboutOverwriting,
                          [pattern] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (file != juce::File())
            pattern.saveTo (file);
    });
}

void RhythmPanel::pushHumaniseToEngine()
{
    if (updatingControls)
        return;

    processor.pushUndoAction ("Change humanise", "rhythm-setting", "humanise");   // action-and-undo.md (rhythm settings)

    auto humanise = rhythm().getHumanise();

    humanise.timingMs        = timingSlider.getValue();
    humanise.velocityPercent = velocitySlider.getValue();
    humanise.missPercent     = missSlider.getValue();
    humanise.ghostPercent    = ghostSlider.getValue();

    rhythm().setHumanise (humanise);
}

void RhythmPanel::refreshFromEngine()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& engine = rhythm();

    enableToggle->getButton().setToggleState (engine.isEnabled(), juce::dontSendNotification);
    freeRunButton.setToggleState (engine.isFreeRunning(), juce::dontSendNotification);

    styleBox.setSelectedId ((int) engine.getVoicingStyle() + 1, juce::dontSendNotification);
    densitySlider.setValue (engine.getVoicingDensity(), juce::dontSendNotification);
    handPositionSlider.setValue (engine.getHandPositionHint(), juce::dontSendNotification);

    const int capo = engine.getCapoFret();
    capoLabel.setText (capo == 0 ? "Capo: off" : "Capo: fret " + juce::String (capo),
                       juce::dontSendNotification);

    const auto pattern = engine.getPattern();
    swingSlider.setValue (pattern.getSwing() * 100.0, juce::dontSendNotification);

    const auto humanise = engine.getHumanise();
    timingSlider.setValue (humanise.timingMs, juce::dontSendNotification);
    velocitySlider.setValue (humanise.velocityPercent, juce::dontSendNotification);
    missSlider.setValue (humanise.missPercent, juce::dontSendNotification);
    ghostSlider.setValue (humanise.ghostPercent, juce::dontSendNotification);

    strumGrid->refresh();
    fingerpickGrid->refresh();
    strumGroup->refresh();
}

//==============================================================================
void RhythmPanel::timerCallback()
{
    const int step = rhythm().getCurrentStep();

    strumGrid->setPlayingStep (step);
    fingerpickGrid->setPlayingStep (step);

    indicators->refresh();

    // rhythm-engine 8: Poly is the required playing mode, and the panel says so
    // rather than silently doing nothing.
    const auto* modeParam = processor.getState().getRawParameterValue (ParamIDs::playingMode);
    const bool isPoly = (modeParam != nullptr) && ((int) modeParam->load() == 1);

    const auto hint = (rhythm().isEnabled() && ! isPoly)
                        ? juce::String ("Switch to Poly mode to hear the rhythm engine.")
                        : juce::String();

    if (hint != modeHintLabel.getText())
        modeHintLabel.setText (hint, juce::dontSendNotification);
}

//==============================================================================
int RhythmPanel::preferredHeight() const
{
    return 14 + Metrics::buttonHeight            // enable row
         + 16 + 26                               // genre heading + kit row
         + 12                                    // rig hint
         + 16 + 26 + 22 + 22 + 26                // voicing heading + controls
         + 16 + StrumGrid::preferredHeight       // strum grid
         + 16 + FingerpickGrid::preferredHeight  // fingerpick grid
         + 16 + 22 * 5                           // feel heading + five sliders
         + StrumGroup::preferredHeight + 4       // STRUM group
         + 16 + 26 + 96 + 26                     // browser heading, filter, list, buttons
         + RhythmIndicators::preferredHeight
         + 24;
}

void RhythmPanel::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
}

void RhythmPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    auto row = [&bounds] (int height, int gap = 2)
    {
        auto r = bounds.removeFromTop (height);
        bounds.removeFromTop (gap);
        return r;
    };

    // ---- enable ------------------------------------------------------------------
    {
        auto r = row (Metrics::buttonHeight);
        enableToggle->setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        freeRunButton.setBounds (r);
    }

    modeHintLabel.setBounds (row (12));

    // ---- genre -------------------------------------------------------------------
    genreHeading.setBounds (row (16));
    {
        auto r = row (26);
        diceButton.setBounds (r.removeFromRight (26));
        r.removeFromRight (4);
        genreBox.setBounds (r);
    }
    rigHintLabel.setBounds (row (12));

    // ---- voicing -----------------------------------------------------------------
    voicingHeading.setBounds (row (16));
    styleBox.setBounds (row (26));
    densitySlider.setBounds (row (22));
    handPositionSlider.setBounds (row (22));
    {
        auto r = row (26);
        capoDown.setBounds (r.removeFromLeft (30));
        capoUp.setBounds (r.removeFromRight (30));
        capoLabel.setBounds (r);
    }

    // ---- pattern editors -----------------------------------------------------------
    strumHeading.setBounds (row (16));
    strumGrid->setBounds (row (StrumGrid::preferredHeight));

    pickHeading.setBounds (row (16));
    fingerpickGrid->setBounds (row (FingerpickGrid::preferredHeight));

    // ---- feel --------------------------------------------------------------------
    feelHeading.setBounds (row (16));
    swingSlider.setBounds (row (22));
    timingSlider.setBounds (row (22));
    velocitySlider.setBounds (row (22));
    missSlider.setBounds (row (22));
    ghostSlider.setBounds (row (22));

    // ---- strum (strum-dynamics 6.3) ----------------------------------------------
    strumGroup->setBounds (row (StrumGroup::preferredHeight, 4));

    // ---- browser -----------------------------------------------------------------
    browserHeading.setBounds (row (16));
    tagFilterBox.setBounds (row (26));
    patternList.setBounds (row (96));
    {
        auto r = row (26);
        const int third = r.getWidth() / 3;
        loadButton.setBounds (r.removeFromLeft (third).reduced (1, 0));
        saveButton.setBounds (r.removeFromLeft (third).reduced (1, 0));
        exportButton.setBounds (r.reduced (1, 0));
    }

    // ---- live indicators -----------------------------------------------------------
    indicators->setBounds (row (RhythmIndicators::preferredHeight));
}

} // namespace luthier
