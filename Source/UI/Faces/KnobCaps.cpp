#include "KnobCaps.h"
#include "FaceMaterials.h"

namespace luthier::faces
{

namespace
{
    using juce::Point;

    /** The indicator: the standard cream line, edged in dark ink so it reads on light caps too. */
    void pointerLine (juce::Graphics& g, Point<float> c, float angle, float from, float to, float width,
                      bool enabled, bool edged)
    {
        const auto a = c.getPointOnCircumference (from, angle);
        const auto b = c.getPointOnCircumference (to, angle);

        if (edged && Palette::textured)
        {
            g.setColour (Palette::plateText.withAlpha (0.85f));
            g.drawLine ({ a, b }, width + 1.6f);
        }

        g.setColour (enabled ? Palette::knobPointer : Palette::current().textDisabled);
        g.drawLine ({ a, b }, width);
    }

    void dropShadow (juce::Graphics& g, Point<float> c, float r)
    {
        if (! Palette::textured)
            return;

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - r * 1.02f, c.y - r * 0.94f, r * 2.04f, r * 2.1f);
    }

    /** A disc lit from the top left. */
    void litDisc (juce::Graphics& g, Point<float> c, float r, juce::Colour base, float lift = 0.5f)
    {
        if (Palette::textured)
        {
            juce::ColourGradient grad (base.brighter (lift), c.x - r * 0.6f, c.y - r * 0.7f,
                                       base.darker (0.25f), c.x + r * 0.5f, c.y + r * 0.6f, true);
            g.setGradientFill (grad);
        }
        else
        {
            g.setColour (base);
        }

        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }

    /** Radial ridges round a rim, turning with the knob. */
    void knurl (juce::Graphics& g, Point<float> c, float inner, float outer, float angle, int ridges, juce::Colour colour)
    {
        if (! Palette::textured)
            return;

        g.setColour (colour);

        for (int i = 0; i < ridges; ++i)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
            g.drawLine ({ c.getPointOnCircumference (inner, a), c.getPointOnCircumference (outer, a) }, 0.7f);
        }
    }

    void rimStroke (juce::Graphics& g, Point<float> c, float r)
    {
        g.setColour (Palette::textured ? juce::Colours::black.withAlpha (0.75f) : Palette::current().edgeBright);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
    }

    /** A chicken-head: a tapered bar with a round tail, built pointing up and turned to the angle. */
    juce::Path chickenHeadShape (Point<float> c, float r, float angle)
    {
        juce::Path p;
        p.startNewSubPath (0.0f, -r * 0.98f);
        p.lineTo (r * 0.24f, -r * 0.05f);
        p.lineTo (r * 0.21f, r * 0.42f);
        p.quadraticTo (r * 0.2f, r * 0.7f, 0.0f, r * 0.7f);
        p.quadraticTo (-r * 0.2f, r * 0.7f, -r * 0.21f, r * 0.42f);
        p.lineTo (-r * 0.24f, -r * 0.05f);
        p.closeSubPath();
        p.applyTransform (juce::AffineTransform::rotation (angle).translated (c));
        return p;
    }
}

//==============================================================================
float knobRadiusIn (juce::Rectangle<float> area) noexcept
{
    return juce::jmax (6.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - (Metrics::arcThickness + Metrics::arcGap));
}

float knobAngleFor (float normalised) noexcept
{
    return Metrics::arcStart + juce::jlimit (0.0f, 1.0f, normalised) * (Metrics::arcEnd - Metrics::arcStart);
}

void paintValueArc (juce::Graphics& g, juce::Rectangle<float> area, float normalised, bool enabled)
{
    const auto c = area.getCentre();
    const float r = knobRadiusIn (area) + Metrics::arcGap + Metrics::arcThickness * 0.5f;
    const juce::PathStrokeType stroke (Metrics::arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, Metrics::arcStart, Metrics::arcEnd, true);
    g.setColour (Palette::current().edge.withAlpha (0.6f));
    g.strokePath (track, stroke);

    if (normalised > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, r, r, 0.0f, Metrics::arcStart, knobAngleFor (normalised), true);
        g.setColour (enabled ? Palette::current().accent : Palette::current().textDisabled);
        g.strokePath (value, stroke);
    }
}

void paintKnobCap (juce::Graphics& g, Point<float> c, float r, float angle, KnobCap cap, bool enabled)
{
    const auto m = Materials::current();
    const auto& p = Palette::current();
    const auto black = Palette::knobBody;
    const bool flat = ! Palette::textured;
    const float pointerWidth = juce::jmax (1.6f, r * 0.1f);

    dropShadow (g, c, r);

    switch (cap)
    {
        case KnobCap::bell:
        {
            litDisc (g, c, r, black.brighter (0.08f), 0.2f);
            knurl (g, c, r * 0.8f, r, angle, 36, juce::Colours::black.withAlpha (0.6f));
            litDisc (g, c, r * 0.74f, black, 0.55f);
            rimStroke (g, c, r);
            pointerLine (g, c, angle, r * 0.22f, r * 0.96f, pointerWidth, enabled, false);
            break;
        }

        case KnobCap::skirtedNumbers:
        {
            litDisc (g, c, r, black, 0.25f);

            // The printed scale on the skirt; numbers where there is room, ticks otherwise.
            for (int i = 0; i < 10; ++i)
            {
                const float a = Metrics::arcStart + (Metrics::arcEnd - Metrics::arcStart) * (float) i / 9.0f;

                if (r >= 14.0f)
                {
                    const auto at = c.getPointOnCircumference (r * 0.8f, a);
                    const float h = r * 0.22f;
                    drawPrint (g, juce::String (i + 1), { at.x - h, at.y - h * 0.6f, h * 2.0f, h * 1.2f }, m.inkLight.withAlpha (0.85f), h, false);
                }
                else
                {
                    g.setColour (m.inkLight.withAlpha (0.7f));
                    g.drawLine ({ c.getPointOnCircumference (r * 0.74f, a), c.getPointOnCircumference (r * 0.92f, a) }, 0.8f);
                }
            }

            const float top = r * 0.55f;
            litDisc (g, c, top, m.chrome, 0.35f);

            if (! flat)
            {
                g.setColour (juce::Colours::black.withAlpha (0.18f));
                g.drawEllipse (c.x - top * 0.66f, c.y - top * 0.66f, top * 1.32f, top * 1.32f, 0.7f);
            }

            rimStroke (g, c, r);
            pointerLine (g, c, angle, r * 0.12f, r * 0.62f, pointerWidth, enabled, true);
            break;
        }

        case KnobCap::pointer:
        {
            const auto brown = flat ? black : p.accentDim.interpolatedWith (black, 0.55f);

            juce::Path fin;
            fin.startNewSubPath (c.getPointOnCircumference (r * 1.0f, angle));
            fin.lineTo (c.getPointOnCircumference (r * 0.66f, angle - 0.55f));
            fin.lineTo (c.getPointOnCircumference (r * 0.66f, angle + 0.55f));
            fin.closeSubPath();

            g.setColour (brown.darker (0.1f));
            g.fillPath (fin);
            litDisc (g, c, r * 0.72f, brown, 0.45f);

            if (! flat)
            {
                g.setColour (juce::Colours::black.withAlpha (0.25f));
                g.drawEllipse (c.x - r * 0.4f, c.y - r * 0.4f, r * 0.8f, r * 0.8f, 0.8f);
            }

            g.setColour (flat ? p.edgeBright : juce::Colours::black.withAlpha (0.7f));
            g.strokePath (fin, juce::PathStrokeType (0.8f));
            pointerLine (g, c, angle, r * 0.3f, r * 0.94f, pointerWidth, enabled, false);
            break;
        }

        case KnobCap::goldCap:
        {
            litDisc (g, c, r, black, 0.25f);
            knurl (g, c, r * 0.86f, r, angle, 40, juce::Colours::white.withAlpha (0.08f));

            const float top = r * 0.6f;
            litDisc (g, c, top, m.brass, 0.5f);
            knurl (g, c, top * 0.84f, top, angle, 28, juce::Colours::black.withAlpha (0.35f));

            rimStroke (g, c, r);
            pointerLine (g, c, angle, r * 0.64f, r * 0.98f, pointerWidth, enabled, false);

            g.setColour (flat ? p.edgeBright : Palette::plateText.withAlpha (0.8f));
            g.drawLine ({ c.getPointOnCircumference (top * 0.15f, angle), c.getPointOnCircumference (top * 0.84f, angle) }, 1.0f);
            break;
        }

        case KnobCap::chickenHead:
        case KnobCap::chickenHeadBlack:
        {
            const bool ivory = cap == KnobCap::chickenHead;
            const auto body = flat ? black : (ivory ? m.ivoryPanel.darker (0.08f) : black);

            litDisc (g, c, r * 0.55f, body.darker (0.15f), 0.2f);

            const auto head = chickenHeadShape (c, r, angle);

            if (! flat)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillPath (head, juce::AffineTransform::translation (0.0f, r * 0.08f));

                juce::ColourGradient grad (body.brighter (ivory ? 0.25f : 0.45f), c.x - r * 0.6f, c.y - r * 0.7f,
                                           body.darker (0.3f), c.x + r * 0.6f, c.y + r * 0.7f, false);
                g.setGradientFill (grad);
            }
            else
            {
                g.setColour (body);
            }

            g.fillPath (head);
            g.setColour (flat ? p.edgeBright : juce::Colours::black.withAlpha (0.6f));
            g.strokePath (head, juce::PathStrokeType (0.9f));
            pointerLine (g, c, angle, r * 0.1f, r * 0.9f, juce::jmax (1.4f, r * 0.08f), enabled, ivory);
            break;
        }

        case KnobCap::speed:
        {
            litDisc (g, c, r, m.chrome, 0.3f);
            knurl (g, c, r * 0.8f, r, angle, 48, juce::Colours::black.withAlpha (0.35f));
            litDisc (g, c, r * 0.76f, m.chrome, 0.6f);

            if (! flat)
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.55f, r * 0.55f, r * 0.3f);
            }

            rimStroke (g, c, r);
            pointerLine (g, c, angle, r * 0.15f, r * 0.9f, pointerWidth, enabled, true);
            break;
        }

        case KnobCap::chromeDome:
        {
            litDisc (g, c, r, black, 0.25f);
            knurl (g, c, r * 0.84f, r, angle, 36, juce::Colours::white.withAlpha (0.07f));
            litDisc (g, c, r * 0.6f, m.chrome, 0.7f);
            rimStroke (g, c, r);
            pointerLine (g, c, angle, r * 0.62f, r * 0.98f, pointerWidth, enabled, false);
            break;
        }

        case KnobCap::chromeSkirt:
        {
            litDisc (g, c, r, m.chrome, 0.35f);

            for (int i = 0; i < 11; ++i)
            {
                const float a = Metrics::arcStart + (Metrics::arcEnd - Metrics::arcStart) * (float) i / 10.0f;
                g.setColour (flat ? p.edgeBright : juce::Colours::black.withAlpha (0.45f));
                g.drawLine ({ c.getPointOnCircumference (r * 0.78f, a), c.getPointOnCircumference (r * 0.94f, a) }, 0.7f);
            }

            litDisc (g, c, r * 0.62f, black, 0.45f);
            rimStroke (g, c, r);
            pointerLine (g, c, angle, r * 0.12f, r * 0.6f, pointerWidth, enabled, false);
            break;
        }

        case KnobCap::softTouch:
        {
            litDisc (g, c, r, black.brighter (0.22f), 0.2f);

            if (! flat)
            {
                g.setColour (juce::Colours::black.withAlpha (0.3f));
                g.drawEllipse (c.x - r * 0.84f, c.y - r * 0.84f, r * 1.68f, r * 1.68f, 0.8f);
            }

            rimStroke (g, c, r);
            pointerLine (g, c, angle, r * 0.35f, r * 0.9f, pointerWidth, enabled, false);
            break;
        }

        case KnobCap::ribbed:
        case KnobCap::creamRibbed:
        {
            const bool cream = cap == KnobCap::creamRibbed;
            const auto body = flat ? black : (cream ? m.ivoryPanel.darker (0.05f) : black);

            litDisc (g, c, r, body, 0.25f);
            knurl (g, c, r * 0.78f, r, angle, 24, (cream ? juce::Colours::black.withAlpha (0.25f) : juce::Colours::white.withAlpha (0.12f)));
            litDisc (g, c, r * 0.76f, body.brighter (0.04f), cream ? 0.2f : 0.35f);
            rimStroke (g, c, r);
            pointerLine (g, c, angle, r * 0.1f, r * 0.98f, pointerWidth, enabled, cream);
            break;
        }

        case KnobCap::witchHat:
        {
            litDisc (g, c, r, black, 0.2f);

            // The cone: stepped rings rising to an apex a little up and left of centre.
            if (! flat)
            {
                for (int i = 0; i < 4; ++i)
                {
                    const float rr = r * (0.8f - 0.17f * (float) i);
                    const auto cc = c.translated (-r * 0.03f * (float) i, -r * 0.04f * (float) i);
                    litDisc (g, cc, rr, black.brighter (0.06f * (float) i), 0.35f + 0.1f * (float) i);
                    g.setColour (juce::Colours::black.withAlpha (0.4f));
                    g.drawEllipse (cc.x - rr, cc.y - rr, rr * 2.0f, rr * 2.0f, 0.6f);
                }
            }

            rimStroke (g, c, r);
            pointerLine (g, c, angle, r * 0.62f, r * 0.98f, pointerWidth, enabled, false);
            break;
        }
    }
}

void paintKnob (juce::Graphics& g, juce::Rectangle<float> area, KnobCap cap, float normalised, bool enabled)
{
    paintValueArc (g, area, normalised, enabled);
    paintKnobCap (g, area.getCentre(), knobRadiusIn (area), knobAngleFor (normalised), cap, enabled);
}

//==============================================================================
void FaceKnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    // The arc, its advanced-range marking and the hover state come from the standard knob.
    LuthierLookAndFeel::drawRotarySlider (g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, slider);

    if (knobCap == KnobCap::bell)
        return;

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float radius = juce::jmax (6.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f
                                             - (Metrics::arcThickness + Metrics::arcGap));
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    paintKnobCap (g, centre, radius, angle, knobCap, slider.isEnabled());

    // The centre dot the standard knob uses to show "moved from default".
    const bool atDefault = std::abs (slider.getValue() - slider.getDoubleClickReturnValue()) < 1.0e-6;
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    g.setColour (atDefault || ! slider.isEnabled() ? Palette::current().textDisabled : accent);
    g.fillEllipse (centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
}

} // namespace luthier::faces
