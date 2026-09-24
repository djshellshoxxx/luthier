#include "BassGridGroup.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
BassStepGridView::BassStepGridView (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Bass step grid: click a step to cycle rest, thumb, pop, ghost, finger and dead; "
                "right-click for the chord tone and the level");
    refresh();
}

void BassStepGridView::refresh()
{
    grid = processor.getEngine().getRhythmEngine().getBassGrid();
    repaint();
}

void BassStepGridView::setPlayingStep (int step)
{
    if (step != playingStep)
    {
        playingStep = step;
        repaint();
    }
}

juce::Rectangle<int> BassStepGridView::cellBounds (int step) const
{
    const int n = juce::jmax (1, grid.getLength());
    const float w = (float) getWidth() / (float) n;
    return juce::Rectangle<float> (step * w, 0.0f, w, (float) getHeight()).reduced (1.0f).toNearestInt();
}

int BassStepGridView::stepAt (juce::Point<int> position) const
{
    for (int i = 0; i < grid.getLength(); ++i)
        if (cellBounds (i).contains (position))
            return i;

    return -1;
}

void BassStepGridView::paint (juce::Graphics& g)
{
    const juce::Colour colours[] =
    {
        Palette::panelSunken,                  // rest
        Palette::accent,                       // thumb
        Palette::warning,                      // pop
        Palette::textMuted.withAlpha (0.6f),   // ghost
        Palette::accent.withAlpha (0.55f),     // finger
        Palette::edge                          // dead
    };

    static const char* letters[] = { "", "T", "P", "g", "F", "x" };

    for (int i = 0; i < grid.getLength(); ++i)
    {
        const auto& step = grid.getStep (i);
        const auto r = cellBounds (i);
        const int type = juce::jlimit (0, 5, (int) step.type);

        g.setColour (colours[type].withMultipliedAlpha (step.isRest() ? 1.0f : (float) (0.45 + 0.55 * step.level)));
        g.fillRoundedRectangle (r.toFloat(), 2.0f);

        g.setColour (i == playingStep ? Palette::textPrimary : ((i % 4) == 0 ? Palette::textMuted : Palette::edge));
        g.drawRoundedRectangle (r.toFloat(), 2.0f, i == playingStep ? 2.0f : 1.0f);

        if (! step.isRest())
        {
            g.setColour (Palette::textPrimary);
            g.setFont (Fonts::ui (11.0f));
            g.drawText (letters[type], r.withTrimmedBottom (r.getHeight() / 2), juce::Justification::centred);

            if (step.note != BassStepNote::root)
            {
                g.setFont (Fonts::ui (9.0f));
                g.drawText (getBassStepNoteName (step.note), r.withTrimmedTop (r.getHeight() / 2), juce::Justification::centred);
            }
        }
    }
}

void BassStepGridView::cycleStep (int index)
{
    if (! juce::isPositiveAndBelow (index, grid.getLength()))
        return;

    auto step = grid.getStep (index);
    step.type = BassStepGrid::nextType (step.type);
    grid.setStep (index, step);
    commit();
    repaint();
}

void BassStepGridView::mouseDown (const juce::MouseEvent& event)
{
    const int step = stepAt (event.getPosition());

    if (step < 0)
        return;

    if (event.mods.isPopupMenu())
        showStepMenu (step);
    else
        cycleStep (step);
}

void BassStepGridView::showStepMenu (int step)
{
    const auto cell = grid.getStep (step);
    juce::PopupMenu menu, typeMenu, noteMenu, levelMenu;

    for (int t = 0; t < (int) BassStepType::numTypes; ++t)
        typeMenu.addItem (100 + t, getBassStepTypeName ((BassStepType) t), true, (int) cell.type == t);

    for (int n = 0; n < (int) BassStepNote::numNotes; ++n)
        noteMenu.addItem (200 + n, getBassStepNoteName ((BassStepNote) n), true, (int) cell.note == n);

    static const int levels[] = { 100, 85, 70, 55, 40 };

    for (int i = 0; i < 5; ++i)
        levelMenu.addItem (300 + i, juce::String (levels[i]) + "%", true, std::abs (cell.level * 100.0 - levels[i]) < 1.0);

    menu.addSubMenu ("Technique", typeMenu);
    menu.addSubMenu ("Chord tone", noteMenu);
    menu.addSubMenu ("Level", levelMenu);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this, step] (int result)
    {
        if (result == 0)
            return;

        auto edited = grid.getStep (step);

        if (result >= 100 && result < 100 + (int) BassStepType::numTypes)
            edited.type = (BassStepType) (result - 100);
        else if (result >= 200 && result < 200 + (int) BassStepNote::numNotes)
            edited.note = (BassStepNote) (result - 200);
        else if (result >= 300 && result < 305)
            edited.level = levels[result - 300] / 100.0;

        grid.setStep (step, edited);
        commit();
        repaint();
    });
}

void BassStepGridView::commit()
{
    processor.getEngine().getRhythmEngine().setBassGrid (grid);
}

//==============================================================================
BassGridGroup::BassGridGroup (LuthierAudioProcessor& p)
    : processor (p), gridView (p)
{
    heading.setText ("BASS GRID", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    patternLabel.setText ("Bass voicing", juce::dontSendNotification);
    patternLabel.setFont (Fonts::ui (11.0f));
    patternLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible (patternLabel);

    // ambiguity-resolutions 4.3 / 4.7: what the Bass style voices.
    patternBox.addItem ("Root", 1);
    patternBox.addItem ("Root + fifth", 2);
    patternBox.addItem ("Walking", 3);
    patternBox.setTooltip ("What the Bass voicing style plays under a chord: the root, root and fifth, "
                           "or a walking approach into the next chord");
    patternBox.onChange = [this]
    {
        rhythm().setBassPattern ((RubricBassPattern) juce::jlimit (0, 2, patternBox.getSelectedId() - 1));
    };
    addAndMakeVisible (patternBox);

    factoryBox.setTextWhenNothingSelected ("Load a bass grid...");
    for (int f = 0; f < (int) BassStepGrid::Factory::numFactory; ++f)
        factoryBox.addItem (BassStepGrid::getFactoryName ((BassStepGrid::Factory) f), f + 1);
    factoryBox.setTooltip ("The bass kits' grids");
    factoryBox.onChange = [this]
    {
        const int id = factoryBox.getSelectedId();

        if (id <= 0)
            return;

        rhythm().setBassGrid (BassStepGrid::factory ((BassStepGrid::Factory) (id - 1)));
        refresh();
    };
    addAndMakeVisible (factoryBox);

    for (int n : { 8, 16, 32 })
        lengthBox.addItem (juce::String (n) + " steps", n);
    lengthBox.setTooltip ("How many steps the grid has");
    lengthBox.onChange = [this]
    {
        auto grid = rhythm().getBassGrid();
        grid.setLength (juce::jmax (1, lengthBox.getSelectedId()));
        rhythm().setBassGrid (grid);
        gridView.refresh();
    };
    addAndMakeVisible (lengthBox);

    clearButton.setTooltip ("Empty the grid: the strum pattern plays again");
    clearButton.onClick = [this]
    {
        auto grid = rhythm().getBassGrid();
        grid.clear();
        rhythm().setBassGrid (grid);
        factoryBox.setSelectedId (0, juce::dontSendNotification);
        refresh();
    };
    addAndMakeVisible (clearButton);

    hint.setText ("A grid with steps in it plays instead of the strum pattern.", juce::dontSendNotification);
    hint.setFont (Fonts::ui (10.0f));
    hint.setColour (juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible (hint);

    addAndMakeVisible (gridView);

    shown = processor.getEngine().getGuitarSpec().category == GuitarCategory::Bass;
    setVisible (shown);
    refresh();
    startTimerHz (10);
}

BassGridGroup::~BassGridGroup()
{
    stopTimer();
}

RhythmEngine& BassGridGroup::rhythm()
{
    return processor.getEngine().getRhythmEngine();
}

void BassGridGroup::refresh()
{
    patternBox.setSelectedId ((int) rhythm().getBassPattern() + 1, juce::dontSendNotification);
    lengthBox.setSelectedId (rhythm().getBassGrid().getLength(), juce::dontSendNotification);
    gridView.refresh();
    timerCallback();
}

int BassGridGroup::preferredHeight() const
{
    return shown ? 18 + 26 + 26 + BassStepGridView::preferredHeight + 14 + 10 : 0;
}

void BassGridGroup::timerCallback()
{
    const bool bass = processor.getEngine().getGuitarSpec().category == GuitarCategory::Bass;

    if (bass != shown)
    {
        shown = bass;
        setVisible (bass);

        if (onShownChanged)
            onShownChanged();
    }

    if (shown)
        gridView.setPlayingStep (rhythm().isBassGridActive() ? rhythm().getCurrentStep() : -1);
}

void BassGridGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (2);
        return r;
    };

    heading.setBounds (take (16));
    {
        auto r = take (24);
        patternLabel.setBounds (r.removeFromLeft (90));
        patternBox.setBounds (r);
    }
    {
        auto r = take (24);
        clearButton.setBounds (r.removeFromRight (64));
        r.removeFromRight (4);
        lengthBox.setBounds (r.removeFromRight (90));
        r.removeFromRight (4);
        factoryBox.setBounds (r);
    }
    gridView.setBounds (take (BassStepGridView::preferredHeight));
    hint.setBounds (take (12));
}

} // namespace luthier
