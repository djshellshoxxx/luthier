#include "RiffTabView.h"

#include "Theme.h"

#include <cmath>

namespace luthier
{

namespace
{
    using T = ScoreTechnique::Type;

    juce::String bendWords (double semitones)
    {
        if (std::abs (semitones - 0.5) < 0.01) return "quarter step";
        if (std::abs (semitones - 1.0) < 0.01) return "half step";
        if (std::abs (semitones - 2.0) < 0.01) return "whole step";
        return juce::String (semitones, 1) + " semitones";
    }
}

RiffTabView::RiffTabView()
{
    setWantsKeyboardFocus (true);
    setTitle ("Riff tab");
}

void RiffTabView::setCompiled (std::shared_ptr<const CompiledRiff> newCompiled)
{
    compiled = std::move (newCompiled);
    announcedBar = 0;
    updateDescription();
    repaint();
}

void RiffTabView::setPlayhead (double beat, juce::uint32 stamp)
{
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (stamp != lastStamp)
    {
        lastStamp = stamp;
        lastStampChangeMs = now;
    }

    // gui-engine-dataflow: a value not refreshed for 250 ms is stale and hides.
    const bool show = beat >= 0.0 && now - lastStampChangeMs < 250.0;

    if (show != playheadShowing || (show && std::abs (beat - playhead) > 1.0e-4))
    {
        playhead = beat;
        playheadShowing = show;
        repaint();
    }
}

juce::String RiffTabView::markFor (const ScoreNote& n)
{
    if (n.hasTechnique (T::deadNote))
        return "x";

    juce::String s;

    if (n.hasTechnique (T::hammerOn)) s << "h";
    if (n.hasTechnique (T::pullOff))  s << "p";

    if (n.hasTechnique (T::naturalHarmonic))
        s << "<" << n.fret << ">";
    else
        s << n.fret;

    for (const auto& t : n.techniques)
    {
        switch (t.type)
        {
            case T::bend:               s << "b" << juce::String (t.value, t.value == std::floor (t.value) ? 0 : 1); break;
            case T::bendRelease:        s << "b" << juce::String (t.value, t.value == std::floor (t.value) ? 0 : 1) << "r"; break;
            case T::preBend:            s << "pb"; break;
            case T::slideLegato:        s << (t.value >= n.fret ? "/" : "\\") << juce::roundToInt (t.value); break;
            case T::slideShift:         s << "s" << (t.value >= n.fret ? "/" : "\\") << juce::roundToInt (t.value); break;
            case T::slideIn:            s = "/" + s; break;
            case T::slideDown:          s = "\\" + s; break;
            case T::slideOut:           s << "\\"; break;
            case T::slideUp:            s << "/"; break;
            case T::vibrato:            s << "~"; break;
            case T::pinchHarmonic:      s << "*"; break;
            case T::artificialHarmonic: s << "ah"; break;
            case T::tapHarmonic:        s << "th"; break;
            case T::tap:                s << "T"; break;
            case T::trill:              s << "tr" << juce::roundToInt (t.value); break;
            case T::whammy:             s << "w"; break;
            case T::ghostNote:          s = "(" + s + ")"; break;
            case T::accent:             s << ">"; break;
            case T::staccato:           s << "."; break;
            default:                    break;
        }
    }

    return s;
}

juce::String RiffTabView::describeBars (int firstBar, int count) const
{
    if (compiled == nullptr || compiled->placement.notes.empty())
        return "No riff selected.";

    const double bar = juce::jmax (0.25, compiled->beatsPerBar);
    juce::StringArray bars;

    for (int b = firstBar; b < firstBar + count; ++b)
    {
        juce::StringArray notes;

        for (const auto& n : compiled->placement.notes)
        {
            if (n.startBeat < b * bar - 1.0e-9 || n.startBeat >= (b + 1) * bar - 1.0e-9)
                continue;

            juce::String text;
            text << "string " << (n.stringIndex + 1) << " fret " << n.fret;

            for (const auto& t : n.techniques)
            {
                switch (t.type)
                {
                    case T::bend:        text << " bend " << bendWords (t.value); break;
                    case T::bendRelease: text << " bend " << bendWords (t.value) << " and release"; break;
                    case T::preBend:     text << " prebend " << bendWords (t.value); break;
                    default:             text << " " << juce::String (getTechniqueName (t.type)).toLowerCase(); break;
                }
            }

            notes.add (text);
        }

        if (! notes.isEmpty())
            bars.add ("Bar " + juce::String (b + 1) + ": " + notes.joinIntoString (", "));
    }

    return bars.isEmpty() ? juce::String ("No notes in these bars.") : bars.joinIntoString (". ");
}

void RiffTabView::updateDescription()
{
    setDescription (describeBars (announcedBar));
}

bool RiffTabView::keyPressed (const juce::KeyPress& key)
{
    if (compiled == nullptr)
        return false;

    const int bars = juce::jmax (1, (int) std::ceil (compiled->lengthBeats / juce::jmax (0.25, compiled->beatsPerBar) - 1.0e-9));

    if (key == juce::KeyPress (juce::KeyPress::downKey, juce::ModifierKeys::commandModifier, 0))
    {
        announcedBar = juce::jmin (juce::jmax (0, bars - 1), announcedBar + 2);
    }
    else if (key == juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::commandModifier, 0))
    {
        announcedBar = juce::jmax (0, announcedBar - 2);
    }
    else
    {
        return false;
    }

    updateDescription();
    juce::AccessibilityHandler::postAnnouncement (getDescription(),
                                                  juce::AccessibilityHandler::AnnouncementPriority::medium);
    return true;
}

std::unique_ptr<juce::AccessibilityHandler> RiffTabView::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::staticText);
}

void RiffTabView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (area, Metrics::controlCorner);

    if (compiled == nullptr)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (12.0f));
        g.drawText ("Select a riff to see its tab", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const int strings = juce::jmax (1, compiled->numStrings);
    auto inner = area.reduced (18.0f, 8.0f);
    const float lineGap = inner.getHeight() / (float) juce::jmax (1, strings - 1);
    const double length = juce::jmax (0.25, compiled->loopBeats);
    const float beatWidth = inner.getWidth() / (float) length;

    // Strings.
    g.setColour (Palette::edgeBright);

    for (int s = 0; s < strings; ++s)
    {
        const float y = inner.getY() + (float) s * lineGap;
        g.drawHorizontalLine ((int) std::round (y), inner.getX(), inner.getRight());
    }

    // Bar lines.
    const double bar = juce::jmax (0.25, compiled->beatsPerBar);

    for (double b = 0.0; b <= length + 1.0e-9; b += bar)
    {
        const float x = inner.getX() + (float) b * beatWidth;
        g.drawVerticalLine ((int) std::round (x), inner.getY(), inner.getBottom());
    }

    // Notes.
    const float fontHeight = juce::jlimit (9.0f, 13.0f, lineGap * 0.9f);
    g.setFont (Fonts::ui (fontHeight, true));

    for (const auto& n : compiled->placement.notes)
    {
        const float x = inner.getX() + (float) n.startBeat * beatWidth;
        const float y = inner.getY() + (float) n.stringIndex * lineGap;
        const auto text = markFor (n);
        const float w = juce::jmax (10.0f, juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 4.0f);

        juce::Rectangle<float> box (x - 2.0f, y - fontHeight * 0.6f, w, fontHeight * 1.2f);
        g.setColour (Palette::panelSunken);
        g.fillRect (box);
        g.setColour (Palette::textPrimary);
        g.drawText (text, box, juce::Justification::centredLeft, false);
    }

    // The playhead.
    if (playheadShowing && playhead >= 0.0)
    {
        const float x = inner.getX() + (float) std::fmod (playhead, length) * beatWidth;
        g.setColour (Palette::accent);
        g.fillRect (x - 1.0f, area.getY() + 2.0f, 2.0f, area.getHeight() - 4.0f);
    }
}

} // namespace luthier
