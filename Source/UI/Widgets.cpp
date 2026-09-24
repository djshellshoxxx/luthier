#include "Widgets.h"
#include "RangesUi.h"
#include "UiPreferences.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

//==============================================================================
// kModulateMenuBase is in the header now. Ids for the Modulate submenu start well
// past the fixed items so a source slot is encoded directly in the id, and a test
// driving a result needs to know where that range begins.

//==============================================================================
double ControlClipboard::value = 0.0;
bool ControlClipboard::filled = false;

void ControlClipboard::store (double v) noexcept { value = v; filled = true; }
bool ControlClipboard::hasValue() noexcept       { return filled; }
double ControlClipboard::retrieve() noexcept     { return value; }

//==============================================================================
juce::PopupMenu buildParameterContextMenu (LuthierAudioProcessor& processor,
                                           const juce::String& parameterId)
{
    auto* param = processor.getState().getParameter (parameterId);

    if (param == nullptr)
        return {};

    auto& midiLearn = processor.getMidiLearn();
    const int mappedCc = midiLearn.getCcForParameter (parameterId);
    const bool locked = processor.isParameterLocked (parameterId);

    // The look and feel is the caller's: showParameterContextMenu sets it from the
    // control the menu belongs to, and a test that only inspects the items does
    // not need one at all.
    juce::PopupMenu menu;

    menu.addSectionHeader (param->getName (40));
    menu.addItem (1, "Enter value...");
    menu.addItem (2, "Reset to default");
    menu.addSeparator();
    menu.addItem (3, "Copy value");
    menu.addItem (4, "Paste value", ControlClipboard::hasValue());
    menu.addSeparator();

    if (mappedCc >= 0)
    {
        menu.addItem (5, "MIDI Learn (mapped to CC " + juce::String (mappedCc) + ")");
        menu.addItem (6, "Clear MIDI mapping");
    }
    else
    {
        menu.addItem (5, midiLearn.isLearning() && midiLearn.getLearningParameterId() == parameterId
                            ? "MIDI Learn - move a controller..."
                            : "MIDI Learn");
    }

    menu.addSeparator();
    menu.addItem (7, "Lock (exclude from randomise)", true, locked);
    menu.addItem (8, "Randomise this control", ! locked);

    // ---- modulation (modulation-matrix 5) -------------------------------------
    // Right-clicking any control offers every source, which is the quickest way
    // to build a route: the destination is the control under the cursor, so the
    // user only has to choose what should drive it.
    auto& matrix = processor.getModMatrix();
    const int existingRoutes = matrix.getRouteCountForDestination (parameterId);
    const bool roomForMore = existingRoutes < ModMatrix::kMaxRoutesPerDestination;

    juce::PopupMenu modulate;

    auto addSourceGroup = [&modulate, roomForMore] (const juce::String& groupName,
                                                    int firstSlot, int count)
    {
        juce::PopupMenu group;

        for (int i = 0; i < count; ++i)
            group.addItem (kModulateMenuBase + firstSlot + i,
                           modSourceDisplayName (firstSlot + i), roomForMore);

        modulate.addSubMenu (groupName, group);
    };

    addSourceGroup ("LFO", ModSourceSlots::lfoBase, ModSourceSlots::numLfos);
    addSourceGroup ("Envelope", ModSourceSlots::envBase, ModSourceSlots::numEnvelopes);
    addSourceGroup ("Sequencer", ModSourceSlots::seqBase, ModSourceSlots::numSequencers);
    addSourceGroup ("Follower", ModSourceSlots::followerBase, ModSourceSlots::numFollowers);
    addSourceGroup ("Macro", ModSourceSlots::macroBase, ModSourceSlots::numMacros);

    {
        juce::PopupMenu performance;

        for (int slot : { ModSourceSlots::notePitch, ModSourceSlots::noteVelocity,
                          ModSourceSlots::noteTrigger, ModSourceSlots::notesHeld,
                          ModSourceSlots::aftertouch, ModSourceSlots::polyAftertouch,
                          ModSourceSlots::pitchBend, ModSourceSlots::modWheel,
                          ModSourceSlots::channelPressure, ModSourceSlots::randomPerNote,
                          ModSourceSlots::randomPerBar, ModSourceSlots::randomSmooth })
            performance.addItem (kModulateMenuBase + slot, modSourceDisplayName (slot), roomForMore);

        modulate.addSubMenu ("Performance", performance);
    }

    menu.addSeparator();
    menu.addSubMenu (roomForMore ? "Modulate"
                                 : "Modulate (8 sources already routed)", modulate);

    if (existingRoutes > 0)
        menu.addItem (9, "Remove modulation (" + juce::String (existingRoutes) + ")");

    // ---- advanced ranges (gui-integration 16 items 9-10) ------------------------
    // Only on a physical control, and only the item that would change something:
    // unlocking a control whose family is already unlocked is redundant, and
    // restricting is offered only for a control unlocked on its own
    // (advanced-ranges.md 4).
    if (const auto* physical = RangeRegistry::find (parameterId))
    {
        const auto& ranges = processor.getRanges();

        if (ranges.isUnlockedIndividually (parameterId))
        {
            menu.addSeparator();
            menu.addItem (kRestrictRangeMenuId, "Restrict to stock range for this control");
        }
        else if (! ranges.isFamilyAdvanced (physical->family))
        {
            menu.addSeparator();
            menu.addItem (kUnlockRangeMenuId, "Unlock advanced range for this control");
        }
    }

    // ---- gui-integration 16 items 11-13 ------------------------------------------
    menu.addSeparator();
    menu.addItem (kAutomationIdMenuId, "Automation ID: " + parameterId, false);

    if (const auto action = shortcutActionForParameter (parameterId); action.isNotEmpty())
        if (const auto* binding = AccessibilitySettings::get().findShortcut (action); binding != nullptr && binding->key.isValid())
            menu.addItem (kShowShortcutMenuId, "Show in Options -> Shortcuts (" + binding->key.getTextDescription() + ")");

    return menu;
}

std::function<void (const juce::String&)> showShortcutInOptions;
std::function<void (const juce::String&)> openHelpForPanel;

PanelHelpButton::PanelHelpButton (const juce::String& panelName)
    : juce::Button ("?"), panel (panelName)
{
    setTooltip ("Help for " + panelName);
    setTitle ("Help for " + panelName);
    setWantsKeyboardFocus (true);
}

void PanelHelpButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    b = b.withSizeKeepingCentre (d, d);

    g.setColour ((highlighted || down) ? Palette::accent : Palette::textMuted);
    g.drawEllipse (b.reduced (0.5f), 1.0f);
    g.setFont (Fonts::ui (d * 0.72f, true));
    g.drawText ("?", b, juce::Justification::centred, false);
}

void PanelHelpButton::clicked()
{
    if (openHelpForPanel)
        openHelpForPanel (panel);
}

void labelForScreenReaders (juce::Component& control, LuthierAudioProcessor& processor,
                            const juce::String& parameterId, const juce::String& tooltip)
{
    // ui-wiring.md 21: every attached control has a name a screen reader reads
    // (the parameter's) and its help (the tooltip); the value comes from the
    // attachment's own text function, so it carries the unit.
    if (auto* param = processor.getState().getParameter (parameterId))
    {
        control.setTitle (param->getName (64));
        control.setHelpText (tooltip.isNotEmpty() ? tooltip : param->getName (64));
    }
}

juce::String shortcutActionForParameter (const juce::String& parameterId)
{
    // The parameters a shortcut in accessibility.md 2's table toggles.
    if (parameterId == ParamIDs::slideGuitar)
        return "toggleSlideMode";

    return {};
}

//==============================================================================
void applyParameterMenuResult (int result,
                               juce::Component& owner,
                               LuthierAudioProcessor& processor,
                               const juce::String& parameterId,
                               std::function<void()> onChanged)
{
    auto* param = processor.getState().getParameter (parameterId);

    if (param == nullptr)
        return;

    {
        auto& learn = processor.getMidiLearn();

        switch (result)
        {
            case 1:
            {
                // Value entry uses an inline editor rather than a modal dialog,
                // because modal loops are disabled in the plugin build.
                auto* editor = new juce::TextEditor();
                editor->setSize (120, 24);
                editor->setText (param->getCurrentValueAsText(), false);
                editor->selectAll();
                editor->setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);

                auto& box = juce::CallOutBox::launchAsynchronously (
                    std::unique_ptr<juce::Component> (editor), owner.getScreenBounds(), nullptr);

                editor->onReturnKey = [editor, param, onChanged, &box]
                {
                    const auto text = editor->getText();

                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                    {
                        const float plain = ranged->getValueForText (text);
                        param->setValueNotifyingHost (plain);
                    }

                    if (onChanged)
                        onChanged();

                    box.dismiss();
                };

                editor->onEscapeKey = [&box] { box.dismiss(); };
                editor->grabKeyboardFocus();
                break;
            }

            case 2:
                param->setValueNotifyingHost (param->getDefaultValue());
                break;

            case 3:
                ControlClipboard::store ((double) param->getValue());
                break;

            case 4:
                if (ControlClipboard::hasValue())
                    param->setValueNotifyingHost ((float) ControlClipboard::retrieve());
                break;

            case 5:
                if (learn.isLearning() && learn.getLearningParameterId() == parameterId)
                    learn.cancelLearning();
                else
                    learn.startLearning (parameterId);
                break;

            case 6:
                learn.removeMappingForParameter (parameterId);
                break;

            case 7:
                processor.setParameterLocked (parameterId, ! processor.isParameterLocked (parameterId));
                break;

            case 8:
            {
                juce::Random r;
                float v = r.nextFloat();

                // advanced-ranges.md 5: stock unless the user said otherwise.
                if (RangesUi::randomiseRespectsStock())
                    if (const auto* physical = RangeRegistry::find (parameterId))
                        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                            v = juce::jmap (v, ranged->convertTo0to1 (physical->stockMin),
                                               ranged->convertTo0to1 (physical->stockMax));

                param->setValueNotifyingHost (v);
                break;
            }

            case kShowShortcutMenuId:
                if (showShortcutInOptions)
                    showShortcutInOptions (shortcutActionForParameter (parameterId));
                break;

            case kUnlockRangeMenuId:
            case kRestrictRangeMenuId:
            {
                const bool unlock = (result == kUnlockRangeMenuId);
                auto next = processor.getRanges();
                next.setUnlockedIndividually (parameterId, unlock);

                const int clamped = RangesUi::apply (processor, next,
                                                     (unlock ? "Unlock " : "Lock ")
                                                       + param->getName (40) + " range",
                                                     &owner);

                // A restrict that moved the value says so where it happened,
                // rather than letting the knob jump without a word.
                if (clamped > 0 && owner.isShowing())
                    if (auto* top = owner.getTopLevelComponent())
                    {
                        auto* bubble = new juce::BubbleMessageComponent();
                        top->addChildComponent (bubble);

                        juce::AttributedString text;
                        text.append ("Moved back inside the stock range: now "
                                       + param->getCurrentValueAsText() + ".",
                                     Fonts::ui (12.0f), Palette::textPrimary);

                        // Deletes itself when it fades (the last argument).
                        bubble->showAt (&owner, text, 3500, true, true);
                    }

                break;
            }

            case 9:
            {
                // Walk backwards so removing one does not shift the next.
                auto& modMatrix = processor.getModMatrix();

                for (int i = modMatrix.getNumRoutes(); --i >= 0;)
                    if (modMatrix.getRoute (i).destinationId == parameterId)
                        modMatrix.removeRoute (i);

                break;
            }

            default:
            {
                if (result >= kModulateMenuBase
                      && result < kModulateMenuBase + ModSourceSlots::count)
                {
                    ModRoute route;
                    route.sourceId = modSourceIdForSlot (result - kModulateMenuBase);
                    route.destinationId = parameterId;

                    // A new route starts at a third of full depth: enough to be
                    // obviously doing something, not so much that it swamps the
                    // control the user just right-clicked.
                    route.depth = 0.33f;
                    route.enabled = true;

                    processor.getModMatrix().addRoute (route);
                }

                break;
            }
        }

        if (onChanged && result != 1)
            onChanged();
    }
}

//==============================================================================
bool showLockedRangeNoticeIfAtEdge (juce::Component& owner,
                                    LuthierAudioProcessor& processor,
                                    const juce::String& parameterId,
                                    const juce::Slider& slider)
{
    if (RangeRegistry::find (parameterId) == nullptr
          || processor.getRanges().isParameterAdvanced (parameterId))
        return false;

    const double span = slider.getMaximum() - slider.getMinimum();
    const double value = slider.getValue();

    const bool atEdge = value >= slider.getMaximum() - span * 1.0e-4
                     || value <= slider.getMinimum() + span * 1.0e-4;

    auto* top = owner.getTopLevelComponent();

    if (! atEdge || top == nullptr || ! owner.isShowing())
        return false;

    // An inline notice at the control, not a banner (advanced-ranges.md 6.3):
    // it answers something the user just did, where they did it.
    auto* bubble = new juce::BubbleMessageComponent();
    top->addChildComponent (bubble);

    juce::AttributedString text;
    text.append (RangesUi::kLockedNoticeText, Fonts::ui (12.0f), Palette::textPrimary);
    text.setWordWrap (juce::AttributedString::byWord);

    // Deletes itself when it fades (the last argument).
    bubble->showAt (&owner, text, 4500, true, true);
    return true;
}

//==============================================================================
void showParameterContextMenu (juce::Component& owner,
                               LuthierAudioProcessor& processor,
                               const juce::String& parameterId,
                               std::function<void()> onChanged)
{
    auto menu = buildParameterContextMenu (processor, parameterId);

    /*  An empty menu means there is no such parameter. Showing it would put an
        empty box under the cursor, which is worse than the click doing nothing. */
    if (menu.getNumItems() == 0)
        return;

    menu.setLookAndFeel (&owner.getLookAndFeel());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&owner),
                        [&owner, &processor, parameterId, onChanged] (int result)
    {
        applyParameterMenuResult (result, owner, processor, parameterId, onChanged);
    });
}

//==============================================================================
//  LuthierKnob
//==============================================================================
int LuthierKnob::preferredWidthFor (Size s) noexcept
{
    switch (s)
    {
        case Size::Small: return Metrics::knobSmall + 16;
        case Size::Large: return Metrics::knobLarge + 16;
        case Size::Macro: return Metrics::knobMacro + 16;
        case Size::Normal:
        default:          return Metrics::knobDefault + 16;
    }
}

int LuthierKnob::preferredHeightFor (Size s) noexcept
{
    // Knob, plus the value row above and the label row below.
    const int knob = (s == Size::Small) ? Metrics::knobSmall
                   : (s == Size::Large) ? Metrics::knobLarge
                   : (s == Size::Macro) ? Metrics::knobMacro
                                        : Metrics::knobDefault;

    return knob + 14 + 14;
}

LuthierKnob::LuthierKnob (const juce::String& text, Size s)
    : labelText (text), size (s)
{
    addAndMakeVisible (slider);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (Metrics::arcStart, Metrics::arcEnd, true);

    // Vertical drag for fine control is the spec's feel; the velocity mode gives
    // a long throw without needing the whole screen height.
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (180);

    setInterceptsMouseClicks (true, true);
}

LuthierKnob::~LuthierKnob()
{
    attachment.reset();
}

void LuthierKnob::attachTo (LuthierAudioProcessor& p, const juce::String& id, const juce::String& tooltip)
{
    processor = &p;
    paramId = id;

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.getState(), id, slider);

    RangesUi::tagSlider (slider, id);

    if (tooltip.isNotEmpty())
    {
        slider.setTooltip (tooltip);
        setTooltip (tooltip);
    }
    else if (auto* param = p.getState().getParameter (id))
    {
        slider.setTooltip (param->getName (64));
    }

    labelForScreenReaders (slider, p, id, tooltip);
    updateMidiLearnIndicator();
}

void LuthierKnob::setLabelText (const juce::String& text)
{
    labelText = text;
    repaint();
}

void LuthierKnob::setAccentColour (juce::Colour colour)
{
    accent = colour;
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    repaint();
}

void LuthierKnob::setShowDiceAndLock (bool shouldShow)
{
    showDiceAndLock = shouldShow;
    resized();
    repaint();
}

void LuthierKnob::resyncRange()
{
    if (processor == nullptr || paramId.isEmpty())
        return;

    // The old attachment goes first: two on one slider would fight over it for
    // as long as both existed.
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor->getState(), paramId, slider);

    RangesUi::tagSlider (slider, paramId);
    repaint();
}

void LuthierKnob::updateMidiLearnIndicator()
{
    mappedCc = (processor != nullptr) ? processor->getMidiLearn().getCcForParameter (paramId) : -1;
    repaint();
}

void LuthierKnob::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (14);            // value readout row

    auto bottom = bounds.removeFromBottom (14);   // label row

    if (showDiceAndLock)
    {
        diceBounds = bottom.removeFromLeft (14).withSizeKeepingCentre (10, 10);
        lockBounds = bottom.removeFromRight (14).withSizeKeepingCentre (10, 10);
    }
    else
    {
        diceBounds = {};
        lockBounds = {};
    }

    slider.setBounds (bounds);
}

void LuthierKnob::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto valueRow = bounds.removeFromTop (14);
    auto labelRow = bounds.removeFromBottom (14);

    const bool showValue = hovering || slider.isMouseButtonDown();

    // theme.md: the value sits above and is only visible on hover or during a drag;
    // otherwise the label is shown there instead.
    if (showValue)
    {
        g.setColour (accent);
        g.setFont (Fonts::mono (12.0f));

        juce::String text;

        if (processor != nullptr && paramId.isNotEmpty())
            if (auto* param = processor->getState().getParameter (paramId))
                text = param->getCurrentValueAsText();

        if (text.isEmpty())
            text = juce::String (slider.getValue(), 2);

        // advanced-ranges.md 6.1: a value outside stock carries a `*`.
        if (processor != nullptr)
            text = RangesUi::markReadout (*processor, paramId, text);

        g.drawText (text, valueRow, juce::Justification::centred, true);
    }

    // ---- label -----------------------------------------------------------------
    auto labelArea = labelRow;

    if (showDiceAndLock)
    {
        labelArea = labelArea.withTrimmedLeft (14).withTrimmedRight (14);

        const bool locked = (processor != nullptr) && processor->isParameterLocked (paramId);

        // Dice.
        g.setColour (hovering ? Palette::textMuted : Palette::textDisabled);
        g.drawRoundedRectangle (diceBounds.toFloat(), 1.5f, 1.0f);

        for (int i = 0; i < 3; ++i)
        {
            const float px = (float) diceBounds.getX() + 2.0f + (float) (i % 2) * 5.0f;
            const float py = (float) diceBounds.getY() + 2.0f + (float) (i / 2) * 5.0f;
            g.fillRect (px, py, 1.5f, 1.5f);
        }

        // Padlock.
        g.setColour (locked ? accent : (hovering ? Palette::textMuted : Palette::textDisabled));

        auto body = lockBounds.toFloat().withTrimmedTop (4.0f);
        g.drawRoundedRectangle (body, 1.0f, 1.0f);

        juce::Path shackle;
        shackle.addCentredArc ((float) lockBounds.getCentreX(), (float) lockBounds.getY() + 4.0f,
                               2.5f, 3.0f, 0.0f,
                               -juce::MathConstants<float>::halfPi,
                               juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (1.0f));
    }

    g.setColour (hovering ? Palette::textPrimary : Palette::textMuted);
    g.setFont (Fonts::label());
    Fonts::drawTrackedText (g, labelText.toUpperCase(), labelArea, juce::Justification::centred);

    // ---- modulation arc (modulation-matrix 5 and 7) -----------------------------
    // Modulation does not move the control - that is what distinguishes it from
    // automation - so it is drawn as an arc from where the knob is to where the
    // modulation has pushed the value.
    if (processor != nullptr && paramId.isNotEmpty())
    {
        auto& matrix = processor->getModMatrix();
        const int index = processor->getParameterBridge().parameterIndex (paramId);

        if (matrix.isDestinationModulated (index))
        {
            const auto range = processor->getState().getParameterRange (paramId);
            const float span = juce::jmax (1.0e-9f, range.end - range.start);
            const float base = (float) slider.getValue();

            const float baseNorm = juce::jlimit (0.0f, 1.0f, (base - range.start) / span);
            const float modNorm = juce::jlimit (0.0f, 1.0f,
                                                (base + matrix.getOffsetFor (index) - range.start) / span);

            // Same sweep the slider's rotary uses, so the arc lines up with the
            // pointer rather than floating near it.
            const float startAngle = juce::MathConstants<float>::pi * 1.2f;
            const float endAngle = juce::MathConstants<float>::pi * 2.8f;

            const auto knobArea = bounds.toFloat().reduced (3.0f);
            const float radius = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f - 1.0f;

            if (radius > 2.0f && std::abs (modNorm - baseNorm) > 1.0e-4f)
            {
                const float a0 = startAngle + baseNorm * (endAngle - startAngle);
                const float a1 = startAngle + modNorm * (endAngle - startAngle);

                juce::Path arc;
                arc.addCentredArc (knobArea.getCentreX(), knobArea.getCentreY(),
                                   radius, radius, 0.0f,
                                   juce::jmin (a0, a1), juce::jmax (a0, a1), true);

                g.setColour (Palette::secondary.withAlpha (0.85f));
                g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
            }
        }
    }

    // A mapped CC is shown as a small patina dot in the corner, so a player can
    // see at a glance which controls their pedalboard is driving.
    if (mappedCc >= 0)
    {
        g.setColour (Palette::secondary);
        g.fillEllipse ((float) getWidth() - 7.0f, 3.0f, 4.0f, 4.0f);
    }

    // While this control is the one being learned, it pulses.
    if (processor != nullptr && processor->getMidiLearn().isLearning()
        && processor->getMidiLearn().getLearningParameterId() == paramId)
    {
        g.setColour (Palette::secondary.withAlpha (0.65f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), Metrics::panelCorner, 1.5f);
    }
}

void LuthierKnob::mouseEnter (const juce::MouseEvent&)
{
    hovering = true;
    updateMidiLearnIndicator();
    repaint();
}

void LuthierKnob::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

void LuthierKnob::mouseDown (const juce::MouseEvent& e)
{
    if (processor == nullptr || paramId.isEmpty())
        return;

    if (showDiceAndLock && e.mods.isLeftButtonDown())
    {
        if (diceBounds.contains (e.getPosition()))
        {
            if (auto* param = processor->getState().getParameter (paramId))
            {
                juce::Random r;
                param->setValueNotifyingHost (r.nextFloat());
            }
            return;
        }

        if (lockBounds.contains (e.getPosition()))
        {
            processor->setParameterLocked (paramId, ! processor->isParameterLocked (paramId));
            repaint();
            return;
        }
    }

    if (e.mods.isPopupMenu())
        showParameterContextMenu (*this, *processor, paramId, [this] { updateMidiLearnIndicator(); });
}

//==============================================================================
void LuthierKnob::KnobSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (owner.processor != nullptr && owner.paramId.isNotEmpty())
            showParameterContextMenu (owner, *owner.processor, owner.paramId,
                                      [this] { owner.updateMidiLearnIndicator(); });
        return;
    }

    noticeShownThisDrag = false;
    juce::Slider::mouseDown (e);
    owner.repaint();
}

void LuthierKnob::KnobSlider::mouseDrag (const juce::MouseEvent& e)
{
    // Shift is coarse, Ctrl/Cmd is ultra-fine, plain drag is normal.
    if (e.mods.isShiftDown())
        setMouseDragSensitivity (70);
    else if (e.mods.isCommandDown())
        setMouseDragSensitivity (1200);
    else
        setMouseDragSensitivity (180);

    juce::Slider::mouseDrag (e);

    // advanced-ranges.md 6.3: the knob stops at the stock edge and says why.
    if (! noticeShownThisDrag && owner.processor != nullptr && e.getDistanceFromDragStartY() != 0)
        noticeShownThisDrag = showLockedRangeNoticeIfAtEdge (owner, *owner.processor,
                                                             owner.paramId, *this);

    owner.repaint();
}

void LuthierKnob::KnobSlider::mouseEnter (const juce::MouseEvent& e)
{
    juce::Slider::mouseEnter (e);
    owner.hovering = true;
    owner.repaint();
}

void LuthierKnob::KnobSlider::mouseExit (const juce::MouseEvent& e)
{
    juce::Slider::mouseExit (e);
    owner.hovering = false;
    owner.repaint();
}

//==============================================================================
//  LuthierChoice
//==============================================================================
LuthierChoice::LuthierChoice (const juce::String& text)
    : labelText (text)
{
    addAndMakeVisible (box);
    box.setJustificationType (juce::Justification::centredLeft);
}

LuthierChoice::~LuthierChoice()
{
    attachment.reset();
}

void LuthierChoice::attachTo (LuthierAudioProcessor& p, const juce::String& id, const juce::String& tooltip)
{
    processor = &p;
    paramId = id;

    if (auto* param = p.getState().getParameter (id))
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
            box.addItemList (choice->choices, 1);

        box.setTooltip (tooltip.isNotEmpty() ? tooltip : param->getName (64));
        setTooltip (box.getTooltip());
        labelForScreenReaders (box, p, id, tooltip);
    }

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.getState(), id, box);
}

void LuthierChoice::setLabelText (const juce::String& text)
{
    labelText = text;
    repaint();
}

void LuthierChoice::setLabelVisible (bool visible)
{
    labelVisible = visible;
    resized();
    repaint();
}

void LuthierChoice::resized()
{
    auto bounds = getLocalBounds();

    if (labelVisible && labelText.isNotEmpty())
        bounds.removeFromTop (labelHeight);

    box.setBounds (bounds);
}

void LuthierChoice::paint (juce::Graphics& g)
{
    if (! labelVisible || labelText.isEmpty())
        return;

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::label());
    Fonts::drawTrackedText (g, labelText.toUpperCase(),
                            getLocalBounds().removeFromTop (labelHeight),
                            juce::Justification::centredLeft);
}

void LuthierChoice::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && processor != nullptr && paramId.isNotEmpty())
        showParameterContextMenu (*this, *processor, paramId);
}

//==============================================================================
//  LuthierToggle
//==============================================================================
LuthierToggle::LuthierToggle (const juce::String& text)
{
    addAndMakeVisible (button);
    button.setButtonText (text);
    button.setClickingTogglesState (true);
}

LuthierToggle::~LuthierToggle()
{
    attachment.reset();
}

void LuthierToggle::attachTo (LuthierAudioProcessor& p, const juce::String& id, const juce::String& tooltip)
{
    processor = &p;
    paramId = id;

    if (auto* param = p.getState().getParameter (id))
    {
        button.setTooltip (tooltip.isNotEmpty() ? tooltip : param->getName (64));
        setTooltip (button.getTooltip());
        labelForScreenReaders (button, p, id, tooltip);
    }

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.getState(), id, button);
}

void LuthierToggle::resized()
{
    button.setBounds (getLocalBounds());
}

void LuthierToggle::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && processor != nullptr && paramId.isNotEmpty())
        showParameterContextMenu (*this, *processor, paramId);
}

//==============================================================================
//  LuthierSlider
//==============================================================================
LuthierSlider::LuthierSlider (const juce::String& text, bool isVertical)
    : labelText (text), vertical (isVertical)
{
    addAndMakeVisible (slider);

    slider.setSliderStyle (vertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 18);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::textPrimary);
}

LuthierSlider::~LuthierSlider()
{
    attachment.reset();
}

void LuthierSlider::attachTo (LuthierAudioProcessor& p, const juce::String& id, const juce::String& tooltip)
{
    processor = &p;
    paramId = id;

    if (auto* param = p.getState().getParameter (id))
    {
        slider.setTooltip (tooltip.isNotEmpty() ? tooltip : param->getName (64));
        setTooltip (slider.getTooltip());
        labelForScreenReaders (slider, p, id, tooltip);
    }

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.getState(), id, slider);

    RangesUi::tagSlider (slider, id);
}

void LuthierSlider::resyncRange()
{
    if (processor == nullptr || paramId.isEmpty())
        return;

    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor->getState(), paramId, slider);

    RangesUi::tagSlider (slider, paramId);
    repaint();
}

void LuthierSlider::resized()
{
    auto bounds = getLocalBounds();

    if (! vertical && labelText.isNotEmpty())
        bounds.removeFromLeft (92);

    slider.setBounds (bounds);
}

void LuthierSlider::paint (juce::Graphics& g)
{
    if (vertical || labelText.isEmpty())
        return;

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::label());
    Fonts::drawTrackedText (g, labelText.toUpperCase(),
                            getLocalBounds().removeFromLeft (92).withTrimmedRight (Metrics::grid),
                            juce::Justification::centredLeft);
}

void LuthierSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && processor != nullptr && paramId.isNotEmpty())
        showParameterContextMenu (*this, *processor, paramId);
}

//==============================================================================
//  LevelMeter
//==============================================================================
LevelMeter::LevelMeter()
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

LevelMeter::~LevelMeter()
{
    stopTimer();
}

void LevelMeter::setSource (LuthierAudioProcessor* p)
{
    processor = p;
}

void LevelMeter::timerCallback()
{
    if (processor == nullptr)
        return;

    const auto& master = processor->getEngine().getMasterBus();

    auto toNormalised = [] (double linear)
    {
        const double db = gainToDb (linear);
        return (float) juce::jlimit (0.0, 1.0, (db + 60.0) / 60.0);
    };

    const float newL = toNormalised (master.getPeakLeft());
    const float newR = toNormalised (master.getPeakRight());

    // Fast attack, slow release, so transients are visible but the meter is calm.
    levelL = juce::jmax (newL, levelL * 0.80f);
    levelR = juce::jmax (newR, levelR * 0.80f);

    // Peak hold: 1.5 s, then falls at 20 dB/s.
    const float fallPerTick = (20.0f / 60.0f) / 30.0f;

    if (levelL >= peakHoldL) { peakHoldL = levelL; holdCountL = 45; }
    else if (--holdCountL <= 0) { peakHoldL = juce::jmax (0.0f, peakHoldL - fallPerTick); holdCountL = 0; }

    if (levelR >= peakHoldR) { peakHoldR = levelR; holdCountR = 45; }
    else if (--holdCountR <= 0) { peakHoldR = juce::jmax (0.0f, peakHoldR - fallPerTick); holdCountR = 0; }

    displayPeakDb = (float) master.getPeakDb();

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    auto readout = horizontal ? bounds.removeFromRight (46) : bounds.removeFromTop (12);

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds.toFloat(), 1.5f);

    auto drawBar = [&g, this] (juce::Rectangle<int> area, float level, float hold)
    {
        if (level > 0.001f)
        {
            // Smooth gradient rather than visible LED segments.
            juce::Rectangle<int> filled = horizontal
                ? area.withWidth (juce::roundToInt (area.getWidth() * level))
                : area.withTop (area.getBottom() - juce::roundToInt (area.getHeight() * level));

            juce::ColourGradient gradient (
                LuthierLookAndFeel::meterColourFor (0.0f),
                horizontal ? (float) area.getX() : (float) area.getCentreX(),
                horizontal ? (float) area.getCentreY() : (float) area.getBottom(),
                LuthierLookAndFeel::meterColourFor (1.0f),
                horizontal ? (float) area.getRight() : (float) area.getCentreX(),
                horizontal ? (float) area.getCentreY() : (float) area.getY(),
                false);

            gradient.addColour (0.55, LuthierLookAndFeel::meterColourFor (0.55f));
            gradient.addColour (0.82, LuthierLookAndFeel::meterColourFor (0.82f));

            g.setGradientFill (gradient);
            g.fillRect (filled);
        }

        if (hold > 0.005f)
        {
            g.setColour (LuthierLookAndFeel::meterColourFor (hold));

            if (horizontal)
                g.fillRect (area.getX() + juce::roundToInt (area.getWidth() * hold), area.getY(), 1, area.getHeight());
            else
                g.fillRect (area.getX(), area.getBottom() - juce::roundToInt (area.getHeight() * hold), area.getWidth(), 1);
        }
    };

    if (horizontal)
    {
        auto top = bounds.removeFromTop (bounds.getHeight() / 2).reduced (1);
        auto bottom = bounds.reduced (1);
        drawBar (top, levelL, peakHoldL);
        drawBar (bottom, levelR, peakHoldR);
    }
    else
    {
        auto left = bounds.removeFromLeft (bounds.getWidth() / 2).reduced (1);
        auto right = bounds.reduced (1);
        drawBar (left, levelL, peakHoldL);
        drawBar (right, levelR, peakHoldR);
    }

    g.setColour (displayPeakDb > -0.3f ? Palette::clip : Palette::textMuted);
    g.setFont (Fonts::mono (9.0f));
    g.drawText (displayPeakDb <= -99.0f ? juce::String ("-inf")
                                        : juce::String (displayPeakDb, 1),
                readout, juce::Justification::centredRight, false);
}

//==============================================================================
//  OutputLed
//==============================================================================
OutputLed::OutputLed()
{
    setInterceptsMouseClicks (false, false);
    setTooltip ("Output level. Dark when silent, white as it approaches 0 dBFS, "
                "red while the signal is over.");
    startTimerHz (30);
}

OutputLed::~OutputLed()
{
    stopTimer();
}

void OutputLed::setSource (LuthierAudioProcessor* p)
{
    processor = p;
}

void OutputLed::timerCallback()
{
    if (processor == nullptr)
        return;

    const double db = processor->getEngine().getMasterBus().getPeakDb();

    // -inf is the dark grey; brightness rises toward white as the level nears 0 dB.
    const float target = (float) juce::jlimit (0.0, 1.0, (db + 48.0) / 48.0);

    brightness = juce::jmax (target, brightness * 0.86f);
    overThreshold = (db > 0.0);

    repaint();
}

void OutputLed::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();

    const auto dark = juce::Colour (0xff4a4640);

    auto colour = overThreshold
                    ? Palette::clip
                    : dark.interpolatedWith (juce::Colours::white, brightness);

    // Glow, so a hot signal reads from across the room.
    if (brightness > 0.05f || overThreshold)
    {
        g.setColour (colour.withAlpha (0.25f * (overThreshold ? 1.0f : brightness)));
        g.fillEllipse (centre.x - radius * 1.9f, centre.y - radius * 1.9f, radius * 3.8f, radius * 3.8f);
    }

    g.setColour (colour);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour (Palette::edge);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
}

//==============================================================================
//  StringMaskSelector
//==============================================================================
StringMaskSelector::StringMaskSelector (LuthierAudioProcessor& p, const juce::String& parameterId)
    : processor (p), parameter (p.getState().getParameter (parameterId))
{
    setTooltip ("Which strings the E-Bow drives: HELD is any string with a note down; "
                "or pick strings, and they keep going after the note is released");
    AccessibleSetup::configureDescriptive (*this, "E-Bow strings", "Which strings the E-Bow drives");
    timerCallback();
    startTimerHz (10);
}

StringMaskSelector::~StringMaskSelector()
{
    stopTimer();
}

juce::Rectangle<int> StringMaskSelector::cellBounds (int cell) const
{
    auto bounds = getLocalBounds().reduced (1);
    const int heldWidth = 40;

    if (cell < 0)
        return bounds.removeFromLeft (heldWidth);

    bounds.removeFromLeft (heldWidth + 3);
    const int w = juce::jmax (1, bounds.getWidth() / juce::jmax (1, numStrings));
    return { bounds.getX() + cell * w, bounds.getY(), w, bounds.getHeight() };
}

void StringMaskSelector::toggle (int cell)
{
    if (parameter == nullptr)
        return;

    const int next = cell < 0 ? 0 : (mask ^ (1 << cell));

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) next));
    parameter->endChangeGesture();

    timerCallback();
}

void StringMaskSelector::mouseDown (const juce::MouseEvent& e)
{
    if (cellBounds (-1).contains (e.getPosition()))
    {
        toggle (-1);
        return;
    }

    for (int s = 0; s < numStrings; ++s)
        if (cellBounds (s).contains (e.getPosition()))
            toggle (s);
}

void StringMaskSelector::timerCallback()
{
    const int nowMask = parameter != nullptr ? juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue())) : 0;
    const int nowStrings = processor.getEngine().getNumStrings();

    if (nowMask != mask || nowStrings != numStrings)
    {
        mask = nowMask;
        numStrings = nowStrings;
        repaint();
    }
}

void StringMaskSelector::paint (juce::Graphics& g)
{
    auto drawCell = [&g] (juce::Rectangle<int> r, const juce::String& text, bool on)
    {
        const auto area = r.toFloat().reduced (1.0f);
        g.setColour (on ? Palette::accent : Palette::panelSunken);
        g.fillRoundedRectangle (area, Metrics::controlCorner);
        g.setColour (on ? Palette::accentBright : Palette::edge);
        g.drawRoundedRectangle (area, Metrics::controlCorner, 1.0f);
        g.setColour (on ? Palette::backgroundDeep : Palette::textMuted);
        g.setFont (Fonts::ui (10.0f, true));
        g.drawFittedText (text, r, juce::Justification::centred, 1);
    };

    drawCell (cellBounds (-1), "HELD", mask == 0);

    // Strings as the guitar shows them: 1 is the high E.
    for (int s = 0; s < numStrings; ++s)
        drawCell (cellBounds (s), juce::String (s + 1), (mask & (1 << s)) != 0);
}

//==============================================================================
//  FeedbackLed
//==============================================================================
FeedbackLed::FeedbackLed (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Feedback: glows as the amp feeds the strings, and lights fully "
                "when the loop is sustaining a note on its own");
    AccessibleSetup::configureDescriptive (*this, "Feedback indicator",
                                           "Lights when the feedback loop is sustaining a note");
    startTimerHz (20);
}

FeedbackLed::~FeedbackLed()
{
    stopTimer();
}

void FeedbackLed::refresh()
{
    const auto& loop = processor.getEngine().getFeedbackLoop();
    const float nowActivity = loop.isActive() ? (float) loop.getActivity() : 0.0f;
    const bool nowResonant = loop.isActive() && loop.isResonant();

    if (std::abs (nowActivity - activity) > 0.01f || nowResonant != resonant)
    {
        activity = nowActivity;
        resonant = nowResonant;
        repaint();
    }
}

void FeedbackLed::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();

    const auto dark = juce::Colour (0xff4a4640);
    const auto colour = resonant ? Palette::warning
                                 : dark.interpolatedWith (Palette::accent, juce::jlimit (0.0f, 1.0f, activity * 4.0f));

    if (resonant || activity > 0.02f)
    {
        g.setColour (colour.withAlpha (resonant ? 0.35f : 0.25f * juce::jmin (1.0f, activity * 4.0f)));
        g.fillEllipse (centre.x - radius * 1.9f, centre.y - radius * 1.9f, radius * 3.8f, radius * 3.8f);
    }

    g.setColour (colour);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour (Palette::edge);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
}

//==============================================================================
//  DataStreamDisplay
//==============================================================================
DataStreamDisplay::DataStreamDisplay()
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (20);
}

DataStreamDisplay::~DataStreamDisplay()
{
    stopTimer();
}

void DataStreamDisplay::setSource (LuthierAudioProcessor* p)
{
    processor = p;

    // The stream is a real readout, so it needs the diagnostics ring running.
    if (processor != nullptr)
        processor->getDiagnostics().setEnabled (true);
}

bool DataStreamDisplay::isEnabledByUser()
{
    return UiPreferences::get().getBool ("appearance.dataStream", true);
}

void DataStreamDisplay::setEnabledByUser (bool enabled)
{
    UiPreferences::get().setBool ("appearance.dataStream", enabled);
}

void DataStreamDisplay::timerCallback()
{
    update (juce::Time::getMillisecondCounterHiRes());
}

void DataStreamDisplay::update (double nowMs)
{
    // Options -> Appearance's switch, followed here so no one else has to.
    if (const bool wanted = isEnabledByUser(); wanted != isVisible() && getParentComponent() != nullptr)
        setVisible (wanted);

    // ui-wiring 11: a moving readout is motion; under reduced motion it holds still.
    if (processor == nullptr || ! isVisible() || AccessibilitySettings::get().isReducedMotion())
        return;

    auto& diagnostics = processor->getDiagnostics();
    const int total = diagnostics.getTotalRecords();

    // The stream stops 500 ms after the last record, and starts again on new data.
    if (total == lastRecordCount)
    {
        if (scrolling && nowMs - lastArrivalMs >= kStopAfterMs)
        {
            scrolling = false;
            repaint();
        }
        return;
    }

    const int newRecords = juce::jmin (total - lastRecordCount, kMaxLines);
    lastRecordCount = total;
    lastArrivalMs = nowMs;
    scrolling = true;

    std::vector<Diagnostics::Record> records ((size_t) juce::jmax (1, newRecords));
    const int count = diagnostics.getRecords (records.data(), newRecords);

    for (int i = 0; i < count; ++i)
        lines.add (Diagnostics::formatRecord (records[(size_t) i],
                                              processor->getEngine().getSampleRate()));

    while (lines.size() > kMaxLines)
        lines.remove (0);

    repaint();
}

void DataStreamDisplay::paint (juce::Graphics& g)
{
    if (lines.isEmpty())
        return;

    const float lineHeight = (float) getHeight() / (float) numLines;

    g.setFont (Fonts::mono (juce::jlimit (7.0f, 11.0f, lineHeight * 0.78f)));

    // The newest numLines of the 200 kept.
    const int first = juce::jmax (0, lines.size() - numLines);

    for (int i = first; i < lines.size(); ++i)
    {
        // The top two and bottom two lines fade away, as specified - when
        // there are lines enough to fade (the footer shows one).
        const int fromTop = i - first;
        const int fromBottom = lines.size() - 1 - i;

        float alpha = 1.0f;

        if (numLines <= 2)                          alpha = 0.62f;
        else if (fromTop == 0 || fromBottom == 0)   alpha = 0.12f;
        else if (fromTop == 1 || fromBottom == 1)   alpha = 0.38f;
        else                                        alpha = 0.62f;

        if (! scrolling)
            alpha *= 0.55f;

        g.setColour (Palette::dataStream.withAlpha (alpha * 0.85f));

        const juce::Rectangle<int> row (0, juce::roundToInt ((float) fromTop * lineHeight),
                                        getWidth(), juce::roundToInt (lineHeight));

        g.drawText (lines[i], row.reduced (4, 0), juce::Justification::centredLeft, false);
    }
}

//==============================================================================
//  SectionPanel
//==============================================================================
SectionPanel::SectionPanel (const juce::String& t, bool isRaised)
    : title (t), raised (isRaised)
{
}

juce::Rectangle<int> SectionPanel::getContentBounds() const
{
    return getLocalBounds()
             .withTrimmedTop (title.isNotEmpty() ? headerHeight : Metrics::grid)
             .reduced (Metrics::grid, Metrics::grid)
             .withTrimmedTop (0);
}

void SectionPanel::paint (juce::Graphics& g)
{
    LuthierLookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (0.5f), raised);

    if (title.isNotEmpty())
    {
        auto header = getLocalBounds().removeFromTop (headerHeight).reduced (Metrics::grid, 0);
        LuthierLookAndFeel::drawSectionHeader (g, header, title, accent);
    }
}


//==============================================================================
//  InlineNotice
//==============================================================================
InlineNotice::InlineNotice()
{
    setVisible (false);
    setInterceptsMouseClicks (true, false);

    // It is a status message, not a control: a screen reader should read it when
    // it appears and never have to be tabbed onto.
    setWantsKeyboardFocus (false);
}

InlineNotice::~InlineNotice()
{
    stopTimer();
}

void InlineNotice::show (const juce::String& newMessage, Level newLevel, int millisecondsToLive)
{
    message = newMessage;
    level = newLevel;

    setVisible (true);
    repaint();

    /*  accessibility 1: announced rather than only drawn. A notice explaining why
        the window changed shape is exactly the case where a user who cannot see
        the window needs it most. */
    juce::AccessibilityHandler::postAnnouncement (
        message, juce::AccessibilityHandler::AnnouncementPriority::high);

    if (millisecondsToLive > 0)
        startTimer (millisecondsToLive);
    else
        stopTimer();
}

void InlineNotice::dismiss()
{
    stopTimer();

    if (! isVisible())
        return;

    setVisible (false);

    if (onVisibilityChanged != nullptr)
        onVisibilityChanged();
}

void InlineNotice::timerCallback()
{
    dismiss();
}

void InlineNotice::mouseDown (const juce::MouseEvent&)
{
    dismiss();
}

void InlineNotice::paint (juce::Graphics& g)
{
    const auto tint = level == Level::warning ? Palette::warning : Palette::secondary;

    auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (tint.withAlpha (0.12f));
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (tint.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // A marker bar on the left, so the strip reads as a notice at a glance and
    // not as another panel that happens to have text in it.
    g.setColour (tint);
    g.fillRect (bounds.withWidth (3.0f).reduced (0.0f, 4.0f));

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (11.0f));
    g.drawText (message, getLocalBounds().reduced (Metrics::grid + 2, 0),
                juce::Justification::centredLeft, true);

    g.setColour (Palette::textDisabled);
    g.setFont (Fonts::ui (9.0f));
    g.drawText ("click to dismiss", getLocalBounds().reduced (Metrics::grid, 0),
                juce::Justification::centredRight, false);
}

} // namespace luthier
