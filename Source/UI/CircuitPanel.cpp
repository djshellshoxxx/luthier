#include "CircuitPanel.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
//  CircuitResponseView
//==============================================================================
CircuitResponseView::CircuitResponseView (LuthierAudioProcessor& p)
    : processor (p)
{
    setTitle ("Circuit response");
    setInterceptsMouseClicks (false, false);
    refresh();
    startTimerHz (kRefreshHz);   // SPEC-SWEEP GD-2
}

CircuitResponseView::~CircuitResponseView()
{
    stopTimer();
}

bool CircuitResponseView::refresh()
{
    const auto live = processor.getEngine().getLiveCircuitComponents();

    if (hasShown && live == shown)
        return false;

    shown = live;
    hasShown = true;

    for (int i = 0; i < kPoints; ++i)
    {
        const double hz = kMinHz * std::pow (kMaxHz / kMinHz, (double) i / (kPoints - 1));
        curveDb[(size_t) i] = GuitarCircuit::magnitudeDb (shown, hz);
    }

    peakHz = GuitarCircuit::findResonantPeakHz (shown);
    peakDb = GuitarCircuit::magnitudeDb (shown, peakHz);

    setDescription ("Resonant peak at " + juce::String (peakHz / 1000.0, 1) + " kilohertz, "
                    + juce::String (peakDb, 1) + " decibels");
    repaint();
    return true;
}

void CircuitResponseView::timerCallback()
{
    if (isShowing())
        refresh();
}

void CircuitResponseView::resized()
{
    repaint();
}

void CircuitResponseView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds, Metrics::panelCorner);

    auto plot = bounds.reduced (6.0f, 4.0f);
    plot.removeFromBottom (12.0f);   // frequency labels

    auto xFor = [&plot] (double hz)
    {
        return plot.getX() + plot.getWidth() * (float) (std::log (hz / kMinHz) / std::log (kMaxHz / kMinHz));
    };

    auto yFor = [&plot] (double db)
    {
        const double clamped = juce::jlimit (kBottomDb, kTopDb, db);
        return plot.getY() + plot.getHeight() * (float) ((kTopDb - clamped) / (kTopDb - kBottomDb));
    };

    // ---- grid ----------------------------------------------------------------
    g.setFont (Fonts::mono (9.0f));

    for (double hz : { 100.0, 1000.0, 10000.0 })
    {
        const float x = xFor (hz);
        g.setColour (Palette::edge.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

        g.setColour (Palette::textDisabled);
        g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 16.0f, plot.getBottom() + 1.0f, 32.0f, 11.0f),
                    juce::Justification::centred);
    }

    g.setColour (Palette::edge);
    g.drawHorizontalLine ((int) yFor (0.0), plot.getX(), plot.getRight());

    // ---- curve ---------------------------------------------------------------
    juce::Path curve;

    for (int i = 0; i < kPoints; ++i)
    {
        const double hz = kMinHz * std::pow (kMaxHz / kMinHz, (double) i / (kPoints - 1));
        const juce::Point<float> point (xFor (hz), yFor (curveDb[(size_t) i]));

        if (i == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    g.setColour (Palette::accent);
    g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // ---- the peak --------------------------------------------------------------
    const juce::Point<float> peak (xFor (peakHz), yFor (peakDb));

    g.setColour (Palette::secondary);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (peak));

    g.setFont (Fonts::mono (10.0f));
    const auto label = juce::String (peakHz / 1000.0, 1) + " kHz";
    const bool labelLeft = peak.x > plot.getCentreX();

    g.drawText (label,
                juce::Rectangle<float> (labelLeft ? peak.x - 58.0f : peak.x + 6.0f,
                                        juce::jmax (plot.getY(), peak.y - 16.0f), 52.0f, 12.0f),
                labelLeft ? juce::Justification::centredRight : juce::Justification::centredLeft);
}

std::unique_ptr<juce::AccessibilityHandler> CircuitResponseView::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::image);
}

//==============================================================================
//  StandardValueChoice
//==============================================================================
StandardValueChoice::StandardValueChoice (const juce::String& text,
                                          juce::Array<double> v,
                                          juce::StringArray n)
    : labelText (text), values (std::move (v)), names (std::move (n))
{
    jassert (values.size() == names.size());

    for (int i = 0; i < names.size(); ++i)
        box.addItem (names[i], i + 1);

    box.addItem ("Custom", getCustomItemId());
    box.setJustificationType (juce::Justification::centredLeft);

    box.onChange = [this]
    {
        const int index = box.getSelectedId() - 1;

        // Choosing Custom changes nothing: it is what the box says when the
        // knob beside it holds a value that is not on the list.
        if (attachment != nullptr && juce::isPositiveAndBelow (index, values.size()))
            attachment->setValueAsCompleteGesture ((float) values[index]);
    };

    addAndMakeVisible (box);
}

StandardValueChoice::~StandardValueChoice()
{
    attachment.reset();
}

void StandardValueChoice::attachTo (LuthierAudioProcessor& p, const juce::String& id,
                                    const juce::String& tooltip)
{
    processor = &p;
    paramId = id;

    if (auto* param = p.getState().getParameter (id))
    {
        box.setTooltip (tooltip.isNotEmpty() ? tooltip : param->getName (64));
        setTooltip (box.getTooltip());
        box.setTitle (param->getName (64));

        attachment = std::make_unique<juce::ParameterAttachment> (
            *param, [this] (float plain) { showValue (plain); }, nullptr);

        attachment->sendInitialUpdate();
    }
}

void StandardValueChoice::showValue (float plain)
{
    int id = getCustomItemId();

    for (int i = 0; i < values.size(); ++i)
        if (std::abs (plain - values[i]) <= std::abs (values[i]) * 0.005)
            id = i + 1;

    box.setSelectedId (id, juce::dontSendNotification);
}

void StandardValueChoice::resized()
{
    auto bounds = getLocalBounds();

    if (labelText.isNotEmpty())
        bounds.removeFromTop (LuthierChoice::labelHeight);

    box.setBounds (bounds);
}

void StandardValueChoice::paint (juce::Graphics& g)
{
    if (labelText.isEmpty())
        return;

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::label());
    Fonts::drawTrackedText (g, labelText.toUpperCase(),
                            getLocalBounds().removeFromTop (LuthierChoice::labelHeight),
                            juce::Justification::centredLeft);
}

void StandardValueChoice::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && processor != nullptr && paramId.isNotEmpty())
        showParameterContextMenu (*this, *processor, paramId);
}

} // namespace luthier
