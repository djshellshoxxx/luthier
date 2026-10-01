#include "JamLaneView.h"
#include "JamUiText.h"
#include "Theme.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    constexpr int kLanes = 8;          // 7 drums and the bass
    constexpr int kChordRow = 18;

    /** 12: a glyph per lane - circle, square, cross, diamond, star, triangle, dot, bar. */
    void drawGlyph (juce::Graphics& g, int lane, juce::Rectangle<float> r)
    {
        const auto c = r.getCentre();
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        juce::Path p;

        switch (lane)
        {
            case 0:  g.fillEllipse (r.withSizeKeepingCentre (2 * s, 2 * s)); return;                        // kick
            case 1:  g.fillRect (r.withSizeKeepingCentre (1.6f * s, 1.6f * s)); return;                     // snare
            case 2:  g.drawLine (c.x - s, c.y - s, c.x + s, c.y + s, 1.5f);                                 // hats
                     g.drawLine (c.x - s, c.y + s, c.x + s, c.y - s, 1.5f); return;
            case 3:  p.addQuadrilateral (c.x, c.y - s, c.x + s, c.y, c.x, c.y + s, c.x - s, c.y); break;   // ride
            case 4:  p.addStar (c, 5, s * 0.45f, s); break;                                                 // crash
            case 5:  p.addTriangle (c.x - s, c.y + s, c.x + s, c.y + s, c.x, c.y - s); break;               // toms
            case 6:  g.fillEllipse (r.withSizeKeepingCentre (s, s)); return;                                // perc
            default: g.fillRect (r.withSizeKeepingCentre (2 * s, s)); return;                               // bass
        }

        g.fillPath (p);
    }

    juce::Colour laneColour (int lane)
    {
        return lane == 7 ? Palette::secondary : lane == 0 || lane == 1 ? Palette::accentBright : Palette::accent;
    }
}

JamLaneView::JamLaneView()
{
    setTitle ("Jam lanes");
    setDescription ("No bar yet");
    setAccessible (true);
    setWantsKeyboardFocus (true);   // so a screen reader can land on it and read the bar
}

std::unique_ptr<juce::AccessibilityHandler> JamLaneView::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::staticText);
}

void JamLaneView::setStatus (const JamStatus& s, bool isFresh)
{
    const bool changed = ! haveStatus || s.sequence != status.sequence || isFresh != fresh;
    status = s;
    fresh = isFresh;
    haveStatus = true;

    // 8.3: no playhead when stale; 12: per beat under reduced motion.
    const bool running = s.state == JamState::playing || s.state == JamState::ending;
    const int step = juce::jlimit (0, JamStatus::kLaneSteps - 1, s.step);
    playheadStep = ! fresh || ! running ? -1
                 : AccessibilitySettings::get().isReducedMotion() ? step - step % juce::jmax (1, s.stepsPerBeat)
                                                                  : step;

    const auto text = JamUiText::laneDescription (s);

    if (text != description)
    {
        description = text;
        setDescription (description);
    }

    if (changed)
        repaint();
}

void JamLaneView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour (Palette::panelSunken);
    g.fillRect (bounds);

    // The chord track: 2 back, this bar, 2 ahead.
    {
        auto row = bounds.removeFromTop (kChordRow);
        row.removeFromLeft (kNameWidth);
        const int w = juce::jmax (1, row.getWidth() / JamStatus::kChordTrack);
        g.setFont (Fonts::mono (11.0f));

        for (int i = 0; i < JamStatus::kChordTrack; ++i)
        {
            auto cell = row.removeFromLeft (w).reduced (1);
            const auto& chord = status.chordTrack[(size_t) i];
            const auto src = (JamStatus::NextSource) status.chordTrackSource[(size_t) i];

            g.setColour (i == 2 ? Palette::panelRaised.brighter (0.15f) : Palette::panelRaised);
            g.fillRect (cell);

            juce::String text = haveStatus && chord.isKnown() ? chord.toString() : juce::String ("-");

            if (i > 2 && src == JamStatus::NextSource::predicted)
                text << " (pred)";
            else if (i > 2 && src == JamStatus::NextSource::tune)
                text << " (tune)";

            g.setColour (i == 2 ? Palette::textPrimary : src == JamStatus::NextSource::predicted ? Palette::textDisabled : Palette::textMuted);
            g.drawFittedText (text, cell.reduced (3, 0), juce::Justification::centredLeft, 1);
        }
    }

    // The lanes.
    const int steps = juce::jlimit (1, JamStatus::kLaneSteps, status.stepsInBar);
    const float laneH = (float) bounds.getHeight() / (float) kLanes;
    auto names = bounds.removeFromLeft (kNameWidth);
    const float stepW = (float) bounds.getWidth() / (float) steps;

    g.setFont (Fonts::ui (9.0f));

    for (int lane = 0; lane < kLanes; ++lane)
    {
        const float y = (float) bounds.getY() + laneH * (float) lane;

        g.setColour (Palette::textMuted);
        g.drawText (JamUiText::laneName (lane), names.getX() + 3, (int) y, names.getWidth() - 3, (int) laneH,
                    juce::Justification::centredLeft);

        g.setColour (Palette::edge.withAlpha (0.4f));
        g.drawHorizontalLine ((int) y, (float) bounds.getX(), (float) bounds.getRight());

        const int statusLane = lane == 7 ? 7 : lane;

        for (int s = 0; s < steps; ++s)
        {
            const int v = haveStatus ? status.lanes[(size_t) statusLane][(size_t) s] : 0;

            if (v <= 0)
                continue;

            g.setColour (laneColour (lane).withAlpha (0.25f + 0.75f * (float) v / 127.0f));
            drawGlyph (g, lane, { (float) bounds.getX() + stepW * (float) s + 1.0f, y + 1.0f, stepW - 2.0f, laneH - 2.0f });
        }
    }

    // Beat lines.
    g.setColour (Palette::edge.withAlpha (0.6f));

    for (int s = 0; s < steps; s += juce::jmax (1, status.stepsPerBeat))
        g.drawVerticalLine (bounds.getX() + (int) (stepW * (float) s), (float) bounds.getY(), (float) bounds.getBottom());

    if (playheadStep >= 0)
    {
        g.setColour (Palette::accentBright.withAlpha (0.8f));
        g.fillRect ((float) bounds.getX() + stepW * (float) playheadStep, (float) bounds.getY(), 2.0f, (float) bounds.getHeight());
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accentBright.withAlpha (0.6f));
        g.drawRect (getLocalBounds(), 1);
    }
}

} // namespace luthier
