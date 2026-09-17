#include "Widgets.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
double ControlClipboard::value = 0.0;
bool ControlClipboard::filled = false;

void ControlClipboard::store (double v) noexcept { value = v; filled = true; }
bool ControlClipboard::hasValue() noexcept       { return filled; }
double ControlClipboard::retrieve() noexcept     { return value; }

//==============================================================================
void showParameterContextMenu (juce::Component& owner,
                               LuthierAudioProcessor& processor,
                               const juce::String& parameterId,
                               std::function<void()> onChanged)
{
    auto* param = processor.getState().getParameter (parameterId);

    if (param == nullptr)
        return;

    auto& midiLearn = processor.getMidiLearn();
    const int mappedCc = midiLearn.getCcForParameter (parameterId);
    const bool locked = processor.isParameterLocked (parameterId);

    juce::PopupMenu menu;
    menu.setLookAndFeel (&owner.getLookAndFeel());

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

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&owner),
                        [&processor, parameterId, param, onChanged, &owner] (int result)
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
                param->setValueNotifyingHost (r.nextFloat());
                break;
            }

            default:
                break;
        }

        if (onChanged && result != 1)
            onChanged();
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

    if (tooltip.isNotEmpty())
    {
        slider.setTooltip (tooltip);
        setTooltip (tooltip);
    }
    else if (auto* param = p.getState().getParameter (id))
    {
        slider.setTooltip (param->getName (64));
    }

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
    }

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.getState(), id, slider);
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

void DataStreamDisplay::timerCallback()
{
    if (processor == nullptr)
        return;

    auto& diagnostics = processor->getDiagnostics();
    const int total = diagnostics.getTotalRecords();

    // The stream stops when nothing is happening, and starts again on new data.
    if (total == lastRecordCount)
    {
        if (scrolling)
        {
            scrolling = false;
            repaint();
        }
        return;
    }

    const int newRecords = juce::jmin (total - lastRecordCount, numLines);
    lastRecordCount = total;
    scrolling = true;

    std::vector<Diagnostics::Record> records ((size_t) juce::jmax (1, newRecords));
    const int count = diagnostics.getRecords (records.data(), newRecords);

    for (int i = 0; i < count; ++i)
        lines.add (Diagnostics::formatRecord (records[(size_t) i],
                                              processor->getEngine().getSampleRate()));

    while (lines.size() > numLines)
        lines.remove (0);

    repaint();
}

void DataStreamDisplay::paint (juce::Graphics& g)
{
    if (lines.isEmpty())
        return;

    const float lineHeight = (float) getHeight() / (float) numLines;

    g.setFont (Fonts::mono (juce::jlimit (7.0f, 11.0f, lineHeight * 0.78f)));

    for (int i = 0; i < lines.size(); ++i)
    {
        // The top two and bottom two lines fade away, as specified.
        const int fromTop = i;
        const int fromBottom = lines.size() - 1 - i;

        float alpha = 1.0f;

        if (fromTop == 0 || fromBottom == 0)       alpha = 0.12f;
        else if (fromTop == 1 || fromBottom == 1)  alpha = 0.38f;
        else                                        alpha = 0.62f;

        if (! scrolling)
            alpha *= 0.55f;

        g.setColour (Palette::dataStream.withAlpha (alpha * 0.85f));

        const juce::Rectangle<int> row (0, juce::roundToInt ((float) i * lineHeight),
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

} // namespace luthier
