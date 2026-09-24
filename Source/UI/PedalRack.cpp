#include "PedalRack.h"
#include "Faces/FaceMaterials.h"
#include "../PluginProcessor.h"

#include <cmath>

namespace luthier
{

namespace
{
    constexpr int slotGutter = 14;      // the slot number, down the left
    constexpr int headerHeight = 26;    // the type selector, above the face

    /** LuthierKnob keeps a value row above its knob and a label row below it (Widgets.cpp). */
    constexpr float knobRow = 14.0f;

    EffectsChain& chainFor (LuthierAudioProcessor& processor, bool postChain)
    {
        return postChain ? processor.getEngine().getPostEffects() : processor.getEngine().getPreEffects();
    }

    /** A knob on a face shows no label of its own (the face prints it), and only the
        knob itself takes clicks, so the rows it hangs over the face never cover a neighbour. */
    void prepareForFace (LuthierKnob& knob)
    {
        knob.setLabelText ({});
        knob.setInterceptsMouseClicks (false, true);
    }
}

//==============================================================================
//  PedalSlotComponent
//==============================================================================
PedalSlotComponent::PedalSlotComponent (LuthierAudioProcessor& p, bool post, int slot)
    : processor (p), postChain (post), slotIndex (slot)
{
    setLookAndFeel (&faceLookAndFeel);

    addAndMakeVisible (typeSelector);
    typeSelector.setLabelVisible (false);
    typeSelector.attachTo (processor, ParamIDs::slotType (postChain, slotIndex),
                           "The pedal in this slot. Empty slots cost nothing.");

    // visual-polish.md 2: bypass is the pedal's footswitch; its LED is on the face.
    addAndMakeVisible (bypassToggle);
    bypassToggle.attachTo (processor, ParamIDs::slotBypass (postChain, slotIndex),
                           "Bypass this pedal. The switch crossfades over 10 ms, so it never clicks.");
    faces::setFaceSwitch (bypassToggle.getButton(), faces::FaceSwitch::footswitch);

    // The mix knob is the last knob on the face, labelled MIX.
    addAndMakeVisible (mixKnob);
    mixKnob.attachTo (processor, ParamIDs::slotMix (postChain, slotIndex),
                      "Blend between this pedal's output and its input");
    prepareForFace (mixKnob);

    shownBypass = bypassFromParameter();

    rebuildControls();
    startTimerHz (10);
}

PedalSlotComponent::~PedalSlotComponent()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

bool PedalSlotComponent::bypassFromParameter() const
{
    if (auto* value = processor.getState().getRawParameterValue (ParamIDs::slotBypass (postChain, slotIndex)))
        return value->load() > 0.5f;

    return false;
}

void PedalSlotComponent::refresh()
{
    if (chainFor (processor, postChain).getSlotType (slotIndex) != cachedType)
        rebuildControls();

    // visual-polish.md 2: the LED follows bypass.
    if (const bool bypassed = bypassFromParameter(); bypassed != shownBypass)
    {
        shownBypass = bypassed;
        repaint (getFaceBounds().getSmallestIntegerContainer());
    }
}

void PedalSlotComponent::rebuildControls()
{
    auto& chain = chainFor (processor, postChain);

    cachedType = chain.getSlotType (slotIndex);

    paramKnobs.clear();
    faceLabels.clear();

    if (auto* pedal = chain.getPedal (slotIndex))
    {
        const int numParams = juce::jmin (pedal->getNumParameters(), Pedal::kMaxParams);
        faceLabels = faces::PedalFaceState::from (*pedal).labels;

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
            prepareForFace (*knob);
        }
    }

    // Every pedal declares fewer than kMaxParams, which leaves the mix a place on the face.
    jassert (paramKnobs.size() < Pedal::kMaxParams);

    if (paramKnobs.size() < Pedal::kMaxParams)
        faceLabels.add ("BLEND");   // not "MIX": several pedals have a Mix of their own

    // visual-polish.md 3: the type's knob caps. The graphic EQ's face is a bank of
    // sliders, so its controls are faders.
    faceLookAndFeel.setCap (faces::knobCapFor (cachedType));

    const bool faders = faces::layoutPedalFace ({ 0.0f, 0.0f, 300.0f, 120.0f }, cachedType, 1,
                                                faces::PedalOrientation::onItsSide).sliders;

    auto setStyle = [faders] (LuthierKnob& knob)
    {
        knob.getSlider().setSliderStyle (faders ? juce::Slider::LinearVertical
                                                : juce::Slider::RotaryHorizontalVerticalDrag);
    };

    for (auto* knob : paramKnobs)
        setStyle (*knob);

    setStyle (mixKnob);

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
    return getPreferredHeightFor (getWidth() > 0 ? getWidth() : nominalWidth);
}

int PedalSlotComponent::getPreferredHeightFor (int width) const
{
    if (cachedType == PedalType::None)
        return 34;

    // The header, then a face tall enough that every knob on it keeps a usable size.
    const float faceWidth = (float) (width - Metrics::gridHalf * 2 - slotGutter);
    const float face = faces::preferredHeightOnItsSide (faceWidth, cachedType,
                                                        juce::jmin (faceLabels.size(), Pedal::kMaxParams));

    return Metrics::gridHalf * 3 + headerHeight + (int) std::ceil (face);
}

//==============================================================================
juce::Rectangle<float> PedalSlotComponent::getFaceBounds() const
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);
    bounds.removeFromLeft (slotGutter);
    bounds.removeFromTop (headerHeight + Metrics::gridHalf);
    return bounds.toFloat();
}

faces::PedalFaceLayout PedalSlotComponent::getFaceLayout() const
{
    return faces::layoutPedalFace (getFaceBounds(), cachedType, juce::jmin (faceLabels.size(), Pedal::kMaxParams),
                                   faces::PedalOrientation::onItsSide);
}

faces::PedalFaceState PedalSlotComponent::faceState() const
{
    faces::PedalFaceState state;
    state.bypassed = shownBypass;
    state.numKnobs = juce::jmin (faceLabels.size(), Pedal::kMaxParams);
    state.labels = faceLabels;
    state.drawKnobs = false;         // the live knobs sit on the face
    state.drawFootswitch = false;    // and the bypass toggle is the footswitch
    state.enabled = isEnabled();
    state.orientation = faces::PedalOrientation::onItsSide;
    return state;
}

PedalSlotComponent::CacheKey PedalSlotComponent::keyFor (float scale) const
{
    const auto face = getFaceBounds();

    CacheKey key;
    key.type = cachedType;
    key.bypassed = shownBypass;
    key.enabled = isEnabled();
    key.knobs = faceLabels.size();
    key.width = juce::roundToInt (face.getWidth());
    key.height = juce::roundToInt (face.getHeight());
    key.scale = scale;
    key.palette = faces::paletteDigest();
    return key;
}

void PedalSlotComponent::renderFace (float scale)
{
    const auto face = getFaceBounds();
    const int w = juce::jmax (1, juce::roundToInt (face.getWidth() * scale));
    const int h = juce::jmax (1, juce::roundToInt (face.getHeight() * scale));

    // A software image: it draws the same in the plugin, in the tests and in the PNG renders.
    faceImage = juce::Image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());

    {
        juce::Graphics g (faceImage);
        g.addTransform (juce::AffineTransform::scale (scale));
        faces::paintPedalFace (g, face.withZeroOrigin(), cachedType, faceState());
    }

    cachedKey = keyFor (scale);
    ++faceRenders;
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

    const auto face = getFaceBounds();

    if (empty || face.isEmpty())
        return;

    // visual-polish.md 0.3: the pedal is drawn once, and again only when what it depicts changes.
    const float scale = juce::jlimit (1.0f, 4.0f, g.getInternalContext().getPhysicalPixelScaleFactor());

    if (faceImage.isNull() || keyFor (scale) != cachedKey)
        renderFace (scale);

    g.drawImage (faceImage, face);
}

void PedalSlotComponent::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    bounds.removeFromLeft (slotGutter);

    auto header = bounds.removeFromTop (headerHeight);

    // Only if a pedal ever declares kMaxParams parameters is there no room for the mix on the face.
    const bool mixOnFace = paramKnobs.size() < Pedal::kMaxParams;

    if (! mixOnFace)
        mixKnob.setBounds (header.removeFromRight (LuthierKnob::preferredWidthFor (LuthierKnob::Size::Small)));

    typeSelector.setBounds (header.reduced (1));

    if (cachedType == PedalType::None)
        return;

    const auto l = getFaceLayout();

    auto place = [&l] (LuthierKnob& knob, int index)
    {
        const auto r = l.knobs[(size_t) index];

        if (l.sliders)
        {
            // A fader: the travel fills the slot, with the knob's value row at its top.
            knob.setBounds (r.withHeight (r.getHeight() + knobRow).toNearestInt());
            return;
        }

        // The slider lands exactly on the face's knob; the knob's rows hang above and below it.
        const auto column = l.labels[(size_t) index].isEmpty() ? r : l.labels[(size_t) index];
        knob.setBounds (juce::Rectangle<float> (column.getX(), r.getY() - knobRow,
                                                column.getWidth(), r.getHeight() + knobRow * 2.0f).toNearestInt());
    };

    for (int i = 0; i < paramKnobs.size() && i < l.numKnobs; ++i)
        place (*paramKnobs[i], i);

    if (mixOnFace && paramKnobs.size() < l.numKnobs)
        place (mixKnob, paramKnobs.size());

    bypassToggle.setBounds (l.footswitch.toNearestInt());
}

void PedalSlotComponent::lookAndFeelChanged()
{
    // A palette change (accessibility.md 6) reaches the caps and the footswitch.
    faceLookAndFeel.refreshColours();
    repaint();
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
    const int width = getWidth() > 0 ? getWidth() : PedalSlotComponent::nominalWidth;
    int total = 0;

    for (auto* slot : slots)
        total += slot->getPreferredHeightFor (width) + Metrics::gridHalf;

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
        slot->setBounds (bounds.removeFromTop (slot->getPreferredHeightFor (getWidth())));
        bounds.removeFromTop (Metrics::gridHalf);
    }
}

} // namespace luthier
