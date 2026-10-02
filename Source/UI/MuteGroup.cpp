#include "MuteGroup.h"
#include "../PluginProcessor.h"
#include "Techniques/TechniqueUi.h"

namespace luthier
{

namespace
{
    juce::Colour muteColour (MuteType type) noexcept
    {
        switch (type)
        {
            case MuteType::palmLight:   return Palette::secondaryDim;
            case MuteType::palmHeavy:   return Palette::secondary;
            case MuteType::palmExtreme: return Palette::accent;
            case MuteType::ghost:       return Palette::textDisabled;
            case MuteType::chuka:       return Palette::accentBright;
            case MuteType::fretMute:    return Palette::warning;
            case MuteType::open:
            case MuteType::numTypes:
            default:                    return Palette::panelSunken;
        }
    }

    void fillTypeBox (juce::ComboBox& box)
    {
        for (int t = 0; t < (int) MuteType::numTypes; ++t)
            box.addItem (getMuteTypeName ((MuteType) t), t + 1);
    }

    float plainOf (LuthierAudioProcessor& p, const char* id)
    {
        return TechniqueUndo::getPlain (p, id);
    }
}

//==============================================================================
MuteGridEditor::MuteGridEditor()
{
    setTooltip ("Click or drag to paint the selected mute type; right-click a cell to choose one.");
}

int MuteGridEditor::numCells() const
{
    return getNumCells != nullptr ? juce::jmax (0, getNumCells()) : 0;
}

juce::Rectangle<int> MuteGridEditor::cellBounds (int cell) const
{
    const int n = juce::jmax (1, numCells());
    const float w = (float) getWidth() / (float) n;

    return juce::Rectangle<float> ((float) cell * w, 0.0f, w, (float) getHeight()).toNearestInt();
}

int MuteGridEditor::cellAt (juce::Point<int> position) const
{
    const int n = numCells();

    if (n <= 0 || getWidth() <= 0)
        return -1;

    return juce::jlimit (0, n - 1, position.x * n / getWidth());
}

void MuteGridEditor::setPlayingCell (int cell)
{
    if (cell != playingCell)
    {
        playingCell = cell;
        repaint();
    }
}

void MuteGridEditor::paintCell (int cell)
{
    if (! juce::isPositiveAndBelow (cell, numCells()) || setCell == nullptr)
        return;

    setCell (cell, brush);
    repaint();
}

void MuteGridEditor::paint (juce::Graphics& g)
{
    const int n = numCells();

    for (int i = 0; i < n; ++i)
    {
        const auto type = getCell != nullptr ? getCell (i) : MuteType::open;
        const auto area = cellBounds (i).toFloat().reduced (1.0f);

        g.setColour (muteColour (type));
        g.fillRoundedRectangle (area, 2.0f);

        // Beats outlined brighter, as on the strum grid, so four can be counted.
        g.setColour (i == playingCell ? Palette::textPrimary
                                      : ((i % 4) == 0 ? Palette::edgeBright : Palette::edge));
        g.drawRoundedRectangle (area, 2.0f, i == playingCell ? 2.0f : 1.0f);

        if (const juce::String glyph (getMuteTypeGlyph (type)); glyph.isNotEmpty())
        {
            g.setColour (muteColour (type).getPerceivedBrightness() > 0.5f ? Palette::backgroundDeep
                                                                            : Palette::textPrimary);
            g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());
            g.drawText (glyph, cellBounds (i), juce::Justification::centred, false);
        }
    }
}

void MuteGridEditor::mouseDown (const juce::MouseEvent& e)
{
    const int cell = cellAt (e.getPosition());

    if (cell < 0)
        return;

    if (e.mods.isPopupMenu())
    {
        showCellMenu (cell);
        return;
    }

    lastPainted = cell;
    paintCell (cell);
}

void MuteGridEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    const int cell = cellAt (e.getPosition());

    if (cell >= 0 && cell != lastPainted)
    {
        lastPainted = cell;
        paintCell (cell);
    }
}

void MuteGridEditor::showCellMenu (int cell)
{
    juce::PopupMenu menu;

    for (int t = 0; t < (int) MuteType::numTypes; ++t)
        menu.addItem (t + 1, getMuteTypeName ((MuteType) t), true,
                      getCell != nullptr && getCell (cell) == (MuteType) t);

    juce::Component::SafePointer<MuteGridEditor> safe (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe, cell] (int result)
    {
        if (safe == nullptr || result <= 0 || safe->setCell == nullptr)
            return;

        safe->setCell (cell, (MuteType) (result - 1));
        safe->repaint();
    });
}

//==============================================================================
MuteGroup::MuteGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    heading.setText ("MUTE", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    armToggle.attachTo (processor, ParamIDs::muteArmed,
                        "Arms muting: the master mode, the live grid, soft strums as chuka, and humanise. "
                        "A pattern's own mute row plays whether or not this is on.");
    addAndMakeVisible (armToggle);

    masterMode.attachTo (processor, ParamIDs::muteMasterMode,
                         "Overrides every mute type in the pattern and the live grid with this one. Off leaves them be.");
    addAndMakeVisible (masterMode);

    // ---- the live grid (muting-rhythm 2) -------------------------------------------
    fillTypeBox (brushBox);
    brushBox.setSelectedId ((int) MuteType::palmHeavy + 1, juce::dontSendNotification);
    brushBox.setTooltip ("The mute type a click on the grid paints");
    brushBox.onChange = [this]
    {
        if (brushBox.getSelectedId() > 0)
            liveGrid.setBrush ((MuteType) (brushBox.getSelectedId() - 1));
    };
    addAndMakeVisible (brushBox);

    for (int i = 0; i < getNumMuteGridPresets(); ++i)
        presetBox.addItem (getMuteGridPreset (i).name, i + 1);

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setTooltip ("Mute grooves (muting-rhythm 6): writes all sixteen steps of the live grid");
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
            applyPreset (presetBox.getSelectedId() - 1);
    };
    addAndMakeVisible (presetBox);

    liveGrid.getNumCells = [] { return kLiveMuteSteps; };
    liveGrid.getCell = [this] (int i) { return processor.getEngine().getTechniqueLayer().mute.getLiveStep (i); };
    liveGrid.setCell = [this] (int i, MuteType t) { TechniqueUndo::paintLiveMuteStep (processor, i, t); };
    liveGrid.setBrush (MuteType::palmHeavy);
    liveGrid.setTooltip ("The live mute grid: a bar of sixteenths, locked to the host, that mutes what you "
                         "play when the rhythm engine is not. Click or drag to paint.");
    addAndMakeVisible (liveGrid);

    // ---- the hand (muting-rhythm 3) ---------------------------------------------------
    auto attach = [this] (LuthierSlider& slider, const char* id, const char* tip)
    {
        slider.attachTo (processor, id, tip);
        addAndMakeVisible (slider);
    };

    attach (palmPosition, ParamIDs::mutePalmPosition,
            "Where the palm rests, in mm from the bridge. Further in covers more string and mutes harder.");
    attach (palmPressure, ParamIDs::mutePalmPressure,
            "How hard the palm presses. Harder is a tighter, shorter chug.");
    attach (humanise, ParamIDs::muteHumanise,
            "The chance a step flips between open and palm-muted, so a groove is not a loop");
    attach (ghostVelocity, ParamIDs::muteGhostVelocity,
            "How loud a ghost note is next to an open one");

    frettingStyle.attachTo (processor, ParamIDs::muteFrettingStyle,
                            "Rock spread: the fretting hand's spare fingers deaden the strings a muted strum "
                            "does not strike. Classical fingertip: only the struck strings are muted.");
    addAndMakeVisible (frettingStyle);

    chukaSource.attachTo (processor, ParamIDs::muteChukaSource,
                          "What makes a chuka. Soft strums: any strum under 0.3 dynamics is played as a chuka.");
    addAndMakeVisible (chukaSource);

    refresh();
    startTimerHz (15);
}

MuteGroup::~MuteGroup()
{
    stopTimer();
}

void MuteGroup::applyPreset (int index)
{
    // One undo entry for the whole groove (mute-grid-paint).
    processor.pushUndoState ("Mute grid preset " + juce::String (getMuteGridPreset (index).name));
    TechniqueUndo::resetMergeWindow();
    processor.getEngine().getTechniqueLayer().mute.applyGridPreset (index);
    refresh();
}

void MuteGroup::refresh()
{
    liveGrid.repaint();
}

void MuteGroup::timerCallback()
{
    // The step the live grid is on, from the engine (gui-engine-dataflow: a
    // live indicator, drained at the panel's rate).
    liveGrid.setPlayingCell (processor.getEngine().getTechniqueLayer().mute.getPlayingStep());
    liveGrid.repaint();
}

void MuteGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (gap);
        return r;
    };

    heading.setBounds (take (headingHeight));
    armToggle.setBounds (take (Metrics::buttonHeight));
    masterMode.setBounds (take (choiceHeight));

    {
        auto r = take (rowHeight);
        brushBox.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        presetBox.setBounds (r);
    }

    liveGrid.setBounds (take (MuteGridEditor::preferredHeight));

    for (auto* s : { &palmPosition, &palmPressure, &humanise, &ghostVelocity })
        s->setBounds (take (rowHeight));

    frettingStyle.setBounds (take (choiceHeight));
    chukaSource.setBounds (take (choiceHeight));
}

//==============================================================================
EasyMuteButton::EasyMuteButton (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Mute: Off, Light, Heavy or Extreme palm mute on everything you play. "
                "The TECHNIQUES tab's MUTE controls have the rest.");
    onClick = [this] { write ((getState() + 1) % 4); };

    refresh();
    startTimerHz (5);
}

EasyMuteButton::~EasyMuteButton()
{
    stopTimer();
}

int EasyMuteButton::getState() const
{
    auto& p = processor;

    if (plainOf (p, ParamIDs::muteArmed) < 0.5f)
        return 0;

    // The master mode's choice list is Off, then MuteType in order.
    switch (juce::roundToInt (plainOf (p, ParamIDs::muteMasterMode)) - 1)
    {
        case (int) MuteType::palmLight:   return 1;
        case (int) MuteType::palmHeavy:   return 2;
        case (int) MuteType::palmExtreme: return 3;
        default:                          return 0;
    }
}

void EasyMuteButton::write (int state)
{
    static constexpr MuteType modes[] = { MuteType::open, MuteType::palmLight,
                                          MuteType::palmHeavy, MuteType::palmExtreme };

    state = juce::jlimit (0, 3, state);

    // One technique-arm entry for the pair of writes.
    const LuthierAudioProcessor::ScopedUndoAction undo (processor, state > 0 ? "Arm Mute technique" : "Disarm Mute technique");

    for (auto [id, plain] : { std::pair<const char*, float> { ParamIDs::muteArmed, state > 0 ? 1.0f : 0.0f },
                              { ParamIDs::muteMasterMode, state > 0 ? (float) ((int) modes[state] + 1) : 0.0f } })
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));

    refresh();
}

void EasyMuteButton::refresh()
{
    static const char* const names[] = { "MUTE: OFF", "MUTE: LIGHT", "MUTE: HEAVY", "MUTE: EXTREME" };

    const int state = getState();
    const juce::String text (names[state]);

    if (getButtonText() != text)
        setButtonText (text);

    setToggleState (state > 0, juce::dontSendNotification);
}

void EasyMuteButton::timerCallback()
{
    refresh();
}

} // namespace luthier
