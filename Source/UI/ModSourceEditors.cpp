#include "ModSourceEditors.h"
#include "Theme.h"

namespace luthier
{

//==============================================================================
LfoBreakpointEditor::LfoBreakpointEditor()
{
    setTooltip ("Custom LFO shape: drag each of the eight points up or down");
}

int LfoBreakpointEditor::pointAt (float x) const noexcept
{
    const float step = (float) getWidth() / (float) (ModLfo::kNumBreakpoints - 1);

    if (step <= 0.0f)
        return 0;

    return juce::jlimit (0, ModLfo::kNumBreakpoints - 1, juce::roundToInt (x / step));
}

double LfoBreakpointEditor::valueAt (float y) const noexcept
{
    const float h = juce::jmax (1.0f, (float) getHeight());
    return juce::jlimit (-1.0, 1.0, 1.0 - 2.0 * (double) (y / h));
}

void LfoBreakpointEditor::edit (juce::Point<float> position)
{
    if (setPoint)
        setPoint (pointAt (position.x), valueAt (position.y));

    repaint();
}

void LfoBreakpointEditor::mouseDown (const juce::MouseEvent& e) { edit (e.position); }
void LfoBreakpointEditor::mouseDrag (const juce::MouseEvent& e) { edit (e.position); }

void LfoBreakpointEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::panelSunken);
    g.fillRect (bounds);
    g.setColour (Palette::edge);
    g.drawRect (bounds, 1.0f);
    g.drawHorizontalLine (juce::roundToInt (bounds.getCentreY()), bounds.getX(), bounds.getRight());

    if (! getPoint)
        return;

    juce::Path line;
    const float step = bounds.getWidth() / (float) (ModLfo::kNumBreakpoints - 1);

    for (int i = 0; i < ModLfo::kNumBreakpoints; ++i)
    {
        const float x = bounds.getX() + step * (float) i;
        const float y = bounds.getY() + bounds.getHeight() * (float) (0.5 - 0.5 * getPoint (i));

        if (i == 0) line.startNewSubPath (x, y);
        else        line.lineTo (x, y);

        g.setColour (Palette::accent);
        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
    }

    g.setColour (Palette::secondary);
    g.strokePath (line, juce::PathStrokeType (1.4f));
}

//==============================================================================
StepGridEditor::StepGridEditor()
{
    setTooltip ("Steps: drag a bar to its value. Cmd/Ctrl-click or right-click toggles "
                "a step's gate, Alt-click its slide. The bottom strip is each step's "
                "probability.");
}

int StepGridEditor::length() const
{
    return juce::jlimit (1, ModStepSequencer::kMaxSteps, getLength ? getLength() : 16);
}

int StepGridEditor::stepAt (float x) const noexcept
{
    const int n = length();
    const float w = (float) getWidth() / (float) n;

    return w <= 0.0f ? 0 : juce::jlimit (0, n - 1, (int) (x / w));
}

bool StepGridEditor::isInProbabilityRow (float y) const noexcept
{
    return y >= (float) (getHeight() - probabilityRowHeight);
}

void StepGridEditor::editValue (juce::Point<float> position)
{
    if (! getStep || ! setStep)
        return;

    const int index = stepAt (position.x);
    auto step = getStep (index);

    if (isInProbabilityRow (position.y))
    {
        const float w = (float) getWidth() / (float) length();
        step.probability = juce::jlimit (0.0, 1.0, (double) ((position.x - w * (float) index) / juce::jmax (1.0f, w)));
    }
    else
    {
        const float h = juce::jmax (1.0f, (float) (getHeight() - probabilityRowHeight));
        step.value = juce::jlimit (-1.0, 1.0, 1.0 - 2.0 * (double) (position.y / h));
    }

    setStep (index, step);
    repaint();
}

void StepGridEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! getStep || ! setStep)
        return;

    const bool toggleGate = e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isPopupMenu();

    if ((toggleGate || e.mods.isAltDown()) && ! isInProbabilityRow (e.position.y))
    {
        const int index = stepAt (e.position.x);
        auto step = getStep (index);

        if (e.mods.isAltDown()) step.slide = ! step.slide;
        else                    step.gate = ! step.gate;

        setStep (index, step);
        repaint();
        return;
    }

    editValue (e.position);
}

void StepGridEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown() || e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isPopupMenu())
        return;

    editValue (e.position);
}

void StepGridEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::panelSunken);
    g.fillRect (bounds);
    g.setColour (Palette::edge);
    g.drawRect (bounds, 1.0f);

    if (! getStep)
        return;

    const int n = length();
    const float w = bounds.getWidth() / (float) n;
    auto bars = bounds.withTrimmedBottom ((float) probabilityRowHeight);
    const float centre = bars.getCentreY();
    const int playing = getPlayingStep ? getPlayingStep() : -1;

    g.setColour (Palette::edge.withAlpha (0.6f));
    g.drawHorizontalLine (juce::roundToInt (centre), bars.getX(), bars.getRight());

    for (int i = 0; i < n; ++i)
    {
        const auto step = getStep (i);
        const float x = bars.getX() + w * (float) i;
        const float top = centre - (float) step.value * bars.getHeight() * 0.5f;

        auto colour = step.gate ? Palette::accent : Palette::textMuted.withAlpha (0.35f);

        if (i == playing)
            colour = colour.brighter (0.4f);

        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x + 1.0f, juce::jmin (top, centre), juce::jmax (1.0f, w - 2.0f),
                                            std::abs (centre - top) + 1.0f));

        if (step.slide)
        {
            g.setColour (Palette::secondary);
            g.drawLine (x + w - 4.0f, top, x + w + 2.0f, top, 2.0f);
        }

        // Probability strip.
        g.setColour (Palette::secondary.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (x + 1.0f, bounds.getBottom() - (float) probabilityRowHeight + 2.0f,
                                            juce::jmax (1.0f, (w - 2.0f) * (float) step.probability),
                                            (float) probabilityRowHeight - 4.0f));
    }
}

} // namespace luthier
