#pragma once

/*  global-search.md 6.1: the header's magnifier. A TextButton so it takes
    the look and feel's button background and focus ring; the glyph is drawn,
    because a magnifying-glass character is not in every font the theme may
    fall back to. */

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Theme.h"

namespace luthier::search
{

class MagnifierButton : public juce::TextButton
{
public:
    MagnifierButton() : juce::TextButton (juce::String())
    {
        setTitle ("Search");
        setDescription ("Search everything: controls, places, commands, presets and help");
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        juce::TextButton::paintButton (g, highlighted, down);

        auto r = getLocalBounds().toFloat().reduced (juce::jmax (4.0f, getHeight() * 0.22f));
        const float d = juce::jmin (r.getWidth(), r.getHeight()) * 0.68f;
        const juce::Rectangle<float> lens (r.getX(), r.getY(), d, d);

        g.setColour (highlighted ? Palette::textPrimary : Palette::textMuted);
        g.drawEllipse (lens, 1.6f);
        g.drawLine (lens.getRight() - d * 0.12f, lens.getBottom() - d * 0.12f,
                    r.getX() + juce::jmin (r.getWidth(), r.getHeight()), r.getY() + juce::jmin (r.getWidth(), r.getHeight()), 2.0f);
    }
};

} // namespace luthier::search
