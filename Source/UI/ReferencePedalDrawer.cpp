#include "ReferencePedalDrawer.h"
#include "../PluginProcessor.h"
#include "Theme.h"

namespace luthier::reference
{
Card::Card (LuthierAudioProcessor& owner, int location)
    : processor (owner), position (location)
{
    addAndMakeVisible (title);
    addAndMakeVisible (modelChoice);
    addAndMakeVisible (placementChoice);
    addAndMakeVisible (footswitch);
    addAndMakeVisible (earlier);
    addAndMakeVisible (later);

    modelChoice.addItem ("Empty", 1);
    modelChoice.addItem ("TS808-inspired prototype", 2);
    modelChoice.addItem ("DS-1-inspired prototype", 3);
    modelChoice.addItem ("Phase 90-inspired prototype", 4);
    modelChoice.setTooltip ("Research-stage model. Not calibrated to physical hardware.");
    placementChoice.addItem ("Before existing FX", 1);
    placementChoice.addItem ("After existing FX", 2);
    placementChoice.setTooltip ("PRE: before legacy pre-amp rack. LOOP: after legacy post-amp rack, before cabinet.");
    modelChoice.onChange = [this]
    {
        if (refreshing) return;
        board().setModel (instanceId(), static_cast<Model> (modelChoice.getSelectedId() - 1));
    };
    placementChoice.onChange = [this]
    {
        if (!refreshing) board().setPlacement (instanceId(), placementChoice.getSelectedId() == 2
                                                    ? Placement::afterExistingPost : Placement::beforeExistingPre);
    };
    footswitch.onClick = [this]
    {
        auto& b = board();
        const int id = instanceId();
        b.setSlotEnabled (id, !b.isSlotEnabled (id));
        refresh();
    };
    earlier.onClick = [this]
    {
        board().move (position, position - 1);
        if (auto* d = dynamic_cast<Drawer*> (getParentComponent()->getParentComponent()))
            d->refresh();
    };
    later.onClick = [this]
    {
        board().move (position, position + 1);
        if (auto* d = dynamic_cast<Drawer*> (getParentComponent()->getParentComponent()))
            d->refresh();
    };
    for (int i = 0; i < 3; ++i)
    {
        auto& slider = knobs[(size_t)i];
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 17);
        slider.setRange (0.0, 1.0, 0.001);
        slider.setTooltip ("Physical control position, 0 to 100%. Uncalibrated prototype.");
        slider.onValueChange = [this, i]
        {
            if (!refreshing) board().setKnob (instanceId(), i, knobs[(size_t)i].getValue());
        };
        addAndMakeVisible (slider);
        addAndMakeVisible (knobLabels[(size_t)i]);
        knobLabels[(size_t)i].setJustificationType (juce::Justification::centred);
    }
    refresh();
}

Board& Card::board() const noexcept { return processor.getEngine().getReferencePedals(); }
int Card::instanceId() const noexcept { return board().getIdAtPosition (position); }

void Card::refresh()
{
    const juce::ScopedValueSetter<bool> suppress (refreshing, true);
    const auto& b = board();
    const int id = instanceId();
    const auto model = b.getModel (id);
    title.setText ("#" + juce::String (position + 1), juce::dontSendNotification);
    modelChoice.setSelectedId ((int)model + 1, juce::dontSendNotification);
    placementChoice.setSelectedId (b.getPlacement (id) == Placement::afterExistingPost ? 2 : 1,
                                   juce::dontSendNotification);
    footswitch.setButtonText (b.isSlotEnabled (id) ? "ON / bypass" : "OFF / engage");
    earlier.setEnabled (position > 0);
    later.setEnabled (position + 1 < Board::slotCount);
    const char* labels[][3] = {
        {"", "", ""},
        {"OVERDRIVE", "TONE", "LEVEL"},
        {"DIST", "TONE", "LEVEL"},
        {"SPEED", "", ""}
    };
    for (int i = 0; i < 3; ++i)
    {
        knobs[(size_t)i].setValue (b.getKnob (id, i), juce::dontSendNotification);
        const bool active = labels[(int)model][i][0] != 0;
        knobs[(size_t)i].setVisible (active);
        knobLabels[(size_t)i].setVisible (active);
        knobLabels[(size_t)i].setText (labels[(int)model][i], juce::dontSendNotification);
        knobs[(size_t)i].setEnabled (model != Model::empty);
    }
}

void Card::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panelRaised);
    g.setColour (Palette::edgeBright);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.0f);
}
void Card::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (24);
    title.setBounds (top.removeFromLeft (28));
    earlier.setBounds (top.removeFromLeft (26));
    later.setBounds (top.removeFromLeft (26));
    modelChoice.setBounds (r.removeFromTop (25));
    placementChoice.setBounds (r.removeFromTop (24));
    const auto row = r.removeFromTop (100);
    for (int i = 0; i < 3; ++i)
    {
        auto cell = row.withTrimmedLeft (i * (row.getWidth() / 3))
                             .withWidth (row.getWidth() / 3);
        knobLabels[(size_t)i].setBounds (cell.removeFromTop (19));
        knobs[(size_t)i].setBounds (cell.withTrimmedTop (18));
    }
    footswitch.setBounds (r.removeFromTop (28));
}

Drawer::Drawer (LuthierAudioProcessor& owner) : processor (owner)
{
    addAndMakeVisible (boardOn);
    addAndMakeVisible (close);
    addAndMakeVisible (description);
    addAndMakeVisible (viewport);
    viewport.setScrollBarsShown (false, true, true, true);
    contents.setSize (Board::slotCount * 233, 194);
    viewport.setViewedComponent (&contents, false);
    for (int i = 0; i < Board::slotCount; ++i)
    {
        auto* card = cards.add (new Card (processor, i));
        contents.addAndMakeVisible (*card);
    }
    boardOn.setToggleState (processor.getEngine().getReferencePedals().isBoardEnabled(), juce::dontSendNotification);
    boardOn.onClick = [this] { processor.getEngine().getReferencePedals().setBoardEnabled (boardOn.getToggleState()); };
    close.onClick = [this] { setOpen (false); };
    description.setText ("Experimental models: audio is NOT measured against reference pedals", juce::dontSendNotification);
    description.setJustificationType (juce::Justification::centredLeft);
    setOpen (false);
}
void Drawer::setOpen (bool value) noexcept { open = value; setVisible (open); if (open) refresh(); }
void Drawer::refresh()
{
    boardOn.setToggleState (processor.getEngine().getReferencePedals().isBoardEnabled(), juce::dontSendNotification);
    for (auto* c : cards) c->refresh();
}
void Drawer::paint (juce::Graphics& g)
{
    g.fillAll (Palette::backgroundDeep);
    g.setColour (Palette::edgeBright);
    g.drawRect (getLocalBounds(), 2);
}
void Drawer::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto top = area.removeFromTop (25);
    boardOn.setBounds (top.removeFromLeft (210));
    close.setBounds (top.removeFromRight (75));
    description.setBounds (top);
    viewport.setBounds (area);
    constexpr int cardWidth = 225, cardGap = 8;
    contents.setSize (Board::slotCount * (cardWidth + cardGap), 194);
    for (int i = 0; i < cards.size(); ++i)
        cards[i]->setBounds (i * (cardWidth + cardGap), 0, cardWidth, 194);
}
} // namespace luthier::reference
