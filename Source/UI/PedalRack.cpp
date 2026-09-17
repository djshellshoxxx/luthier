#include "PedalRack.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
//  PedalSlotComponent
//==============================================================================
PedalSlotComponent::PedalSlotComponent (LuthierAudioProcessor& p, bool post, int slot)
    : processor (p), postChain (post), slotIndex (slot)
{
    addAndMakeVisible (typeSelector);
    typeSelector.setLabelVisible (false);
    typeSelector.attachTo (processor, ParamIDs::slotType (postChain, slotIndex),
                           "The pedal in this slot. Empty slots cost nothing.");

    addAndMakeVisible (bypassToggle);
    bypassToggle.attachTo (processor, ParamIDs::slotBypass (postChain, slotIndex),
                           "Bypass this pedal. The switch crossfades over 10 ms, so it never clicks.");

    addAndMakeVisible (mixKnob);
    mixKnob.attachTo (processor, ParamIDs::slotMix (postChain, slotIndex),
                      "Blend between this pedal's output and its input");

    rebuildControls();
    startTimerHz (4);
}

PedalSlotComponent::~PedalSlotComponent()
{
    stopTimer();
}

void PedalSlotComponent::timerCallback()
{
    auto& chain = postChain ? processor.getEngine().getPostEffects()
                            : processor.getEngine().getPreEffects();

    if (chain.getSlotType (slotIndex) != cachedType)
        rebuildControls();
}

void PedalSlotComponent::rebuildControls()
{
    auto& chain = postChain ? processor.getEngine().getPostEffects()
                            : processor.getEngine().getPreEffects();

    cachedType = chain.getSlotType (slotIndex);

    paramKnobs.clear();

    if (auto* pedal = chain.getPedal (slotIndex))
    {
        const int numParams = juce::jmin (pedal->getNumParameters(), Pedal::kMaxParams);

        for (int i = 0; i < numParams; ++i)
        {
            const auto& descriptor = pedal->getParameterDescriptor (i);

            auto* knob = new LuthierKnob (descriptor.name, LuthierKnob::Size::Small);
            paramKnobs.add (knob);
            addAndMakeVisible (knob);

            juce::String tooltip = juce::String (pedal->getName()) + " - " + descriptor.name;

            if (juce::String (descriptor.unit).isNotEmpty())
                tooltip += " (" + juce::String (descriptor.unit) + ")";

            knob->attachTo (processor, ParamIDs::slotParam (postChain, slotIndex, i), tooltip);
        }
    }

    // The bypass toggle is meaningless on an empty slot.
    const bool hasPedal = (cachedType != PedalType::None);
    bypassToggle.setVisible (hasPedal);
    mixKnob.setVisible (hasPedal);

    if (auto* parent = getParentComponent())
        parent->resized();

    resized();
    repaint();
}

int PedalSlotComponent::getPreferredHeight() const
{
    if (cachedType == PedalType::None)
        return 34;

    const int knobRows = (paramKnobs.size() + 3) / 4;   // four knobs per row

    return 34 + juce::jmax (1, knobRows) * (LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small) + 4)
           + Metrics::gridHalf;
}

//==============================================================================
void PedalSlotComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    const bool empty = (cachedType == PedalType::None);

    g.setColour (empty ? Palette::panelSunken.withAlpha (0.55f) : Palette::panel);
    g.fillRoundedRectangle (bounds, Metrics::panelCorner);

    g.setColour (dragging ? Palette::accent : Palette::edge);
    g.drawRoundedRectangle (bounds, Metrics::panelCorner, dragging ? 1.6f : 1.0f);

    // Slot number down the left, so a drag target is unambiguous.
    g.setColour (Palette::textDisabled);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (juce::String (slotIndex + 1), getLocalBounds().removeFromLeft (16),
                juce::Justification::centred, false);
}

void PedalSlotComponent::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    bounds.removeFromLeft (14);   // slot number gutter

    auto header = bounds.removeFromTop (26);

    bypassToggle.setBounds (header.removeFromRight (48).reduced (1));
    mixKnob.setBounds (header.removeFromRight (LuthierKnob::preferredWidthFor (LuthierKnob::Size::Small))
                             .withHeight (header.getHeight()));

    header.removeFromRight (Metrics::gridHalf);
    typeSelector.setBounds (header.reduced (1));

    if (paramKnobs.isEmpty())
        return;

    bounds.removeFromTop (Metrics::gridHalf);

    const int knobWidth = LuthierKnob::preferredWidthFor (LuthierKnob::Size::Small);
    const int knobHeight = LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small);
    const int perRow = juce::jmax (1, bounds.getWidth() / juce::jmax (1, knobWidth));

    int index = 0;

    while (index < paramKnobs.size() && bounds.getHeight() >= knobHeight)
    {
        auto row = bounds.removeFromTop (knobHeight);

        for (int i = 0; i < perRow && index < paramKnobs.size(); ++i, ++index)
            paramKnobs[index]->setBounds (row.removeFromLeft (knobWidth));

        bounds.removeFromTop (4);
    }
}

//==============================================================================
void PedalSlotComponent::mouseDown (const juce::MouseEvent& e)
{
    dragStart = e.getPosition();

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());

        menu.addSectionHeader ("Slot " + juce::String (slotIndex + 1));
        menu.addItem (1, "Clear slot", cachedType != PedalType::None);
        menu.addItem (2, "Reset this pedal's controls", cachedType != PedalType::None);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this] (int result)
        {
            auto& state = processor.getState();

            if (result == 1)
            {
                if (auto* p = state.getParameter (ParamIDs::slotType (postChain, slotIndex)))
                    p->setValueNotifyingHost (0.0f);
            }
            else if (result == 2)
            {
                auto& chain = postChain ? processor.getEngine().getPostEffects()
                                        : processor.getEngine().getPreEffects();

                if (auto* pedal = chain.getPedal (slotIndex))
                {
                    for (int i = 0; i < juce::jmin (pedal->getNumParameters(), Pedal::kMaxParams); ++i)
                    {
                        const auto& d = pedal->getParameterDescriptor (i);

                        if (auto* p = state.getParameter (ParamIDs::slotParam (postChain, slotIndex, i)))
                            p->setValueNotifyingHost ((float) d.toNormalised (d.defaultValue));
                    }
                }
            }
        });
    }
}

void PedalSlotComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 8)
        dragging = true;

    if (dragging)
        repaint();
}

void PedalSlotComponent::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    dragging = false;
    repaint();

    // Work out which sibling slot the pointer was released over.
    if (auto* parent = getParentComponent())
    {
        const auto pointInParent = e.getEventRelativeTo (parent).getPosition();

        for (auto* child : parent->getChildren())
        {
            if (child == this)
                continue;

            if (auto* other = dynamic_cast<PedalSlotComponent*> (child))
            {
                if (other->getBounds().contains (pointInParent) && onReorderRequested)
                {
                    onReorderRequested (slotIndex, other->getSlotIndex());
                    return;
                }
            }
        }
    }
}

//==============================================================================
//  PedalRack
//==============================================================================
PedalRack::PedalRack (LuthierAudioProcessor& p, bool post)
    : processor (p), postChain (post)
{
    for (int i = 0; i < EffectsChain::kNumSlots; ++i)
    {
        auto* slot = new PedalSlotComponent (processor, postChain, i);
        slot->onReorderRequested = [this] (int from, int to) { reorder (from, to); };

        slots.add (slot);
        addAndMakeVisible (slot);
    }
}

PedalRack::~PedalRack() = default;

int PedalRack::getPreferredHeight() const
{
    int total = 0;

    for (auto* slot : slots)
        total += slot->getPreferredHeight() + Metrics::gridHalf;

    return total;
}

void PedalRack::refresh()
{
    for (auto* slot : slots)
        slot->rebuildControls();

    resized();
}

void PedalRack::reorder (int fromSlot, int toSlot)
{
    if (fromSlot == toSlot)
        return;

    auto& state = processor.getState();

    // Reordering has to move the parameters, not just the engine's pedals, or the
    // next state save would put everything back where it started.
    auto readSlot = [&state, this] (int slot)
    {
        std::vector<float> values;

        values.push_back (state.getParameter (ParamIDs::slotType (postChain, slot))->getValue());
        values.push_back (state.getParameter (ParamIDs::slotBypass (postChain, slot))->getValue());
        values.push_back (state.getParameter (ParamIDs::slotMix (postChain, slot))->getValue());

        for (int p = 0; p < Pedal::kMaxParams; ++p)
            values.push_back (state.getParameter (ParamIDs::slotParam (postChain, slot, p))->getValue());

        return values;
    };

    auto writeSlot = [&state, this] (int slot, const std::vector<float>& values)
    {
        size_t i = 0;

        state.getParameter (ParamIDs::slotType (postChain, slot))->setValueNotifyingHost (values[i++]);
        state.getParameter (ParamIDs::slotBypass (postChain, slot))->setValueNotifyingHost (values[i++]);
        state.getParameter (ParamIDs::slotMix (postChain, slot))->setValueNotifyingHost (values[i++]);

        for (int p = 0; p < Pedal::kMaxParams; ++p)
            state.getParameter (ParamIDs::slotParam (postChain, slot, p))->setValueNotifyingHost (values[i++]);
    };

    const auto moving = readSlot (fromSlot);

    if (fromSlot < toSlot)
    {
        for (int i = fromSlot; i < toSlot; ++i)
            writeSlot (i, readSlot (i + 1));
    }
    else
    {
        for (int i = fromSlot; i > toSlot; --i)
            writeSlot (i, readSlot (i - 1));
    }

    writeSlot (toSlot, moving);

    refresh();
}

void PedalRack::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void PedalRack::resized()
{
    auto bounds = getLocalBounds();

    for (auto* slot : slots)
    {
        slot->setBounds (bounds.removeFromTop (slot->getPreferredHeight()));
        bounds.removeFromTop (Metrics::gridHalf);
    }
}

} // namespace luthier
