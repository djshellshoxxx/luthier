#pragma once

/*  Banjo reveal badge (easter-egg: Dueling Banjos).

    A small, tasteful floating badge that appears when the player performs the
    Dueling Banjos opening motif, announcing that the instrument has become a
    banjo. It is a plain overlay child: hidden by default (addChildComponent),
    positioned absolutely so it never disturbs the main layout, and shown only
    while the banjo voice is active. Clicking it dismisses the reveal and the
    editor returns the engine to its normal voice.

    Drawn in the house brass-on-walnut style (UI/Theme.h); no textures, so it
    stays legible under the High-contrast palette too.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace luthier
{

class BanjoReveal : public juce::Component
{
public:
    BanjoReveal()
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);
        setAccessible (true);
        setTitle ("Dueling Banjos");
        setDescription ("The banjo voice is active. Click to return to the normal voice.");
    }

    /** Called when the player clicks the badge to dismiss the reveal. */
    std::function<void()> onDismiss;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = 10.0f;

        // Drop shadow so the badge reads as floating above the panels.
        g.setColour (Palette::shadow);
        g.fillRoundedRectangle (b.translated (0.0f, 2.0f), radius);

        g.setColour (Palette::panel);
        g.fillRoundedRectangle (b, radius);
        g.setColour (hovered ? Palette::accentBright : Palette::accent);
        g.drawRoundedRectangle (b, radius, hovered ? 2.0f : 1.4f);

        auto content = b.reduced (12.0f, 8.0f);
        auto iconArea = content.removeFromLeft (content.getHeight());
        content.removeFromLeft (10.0f);

        drawBanjo (g, iconArea);

        auto textArea = content;
        const float titleH = juce::jmin (20.0f, textArea.getHeight() * 0.55f);
        auto titleRow = textArea.removeFromTop (titleH);

        g.setColour (Palette::accentBright);
        g.setFont (juce::Font (juce::FontOptions (titleH * 0.92f)).boldened());
        g.drawText ("Dueling Banjos!", titleRow, juce::Justification::centredLeft, true);

        g.setColour (Palette::textMuted);
        g.setFont (juce::Font (juce::FontOptions (juce::jmin (12.0f, textArea.getHeight() * 0.9f))));
        g.drawText ("Banjo voice on - click to dismiss", textArea,
                    juce::Justification::centredLeft, true);
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && onDismiss)
            onDismiss();
    }

private:
    /** A simple vector banjo: round pot with a head, a short neck and strings. */
    static void drawBanjo (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float d = juce::jmin (area.getWidth(), area.getHeight());
        auto square = area.withSizeKeepingCentre (d, d);

        // Neck, angled up-left out of the pot.
        const float neckW = d * 0.16f;
        juce::Point<float> potCentre (square.getCentreX() + d * 0.12f,
                                      square.getCentreY() + d * 0.12f);
        juce::Point<float> neckEnd (square.getX() + d * 0.02f, square.getY() + d * 0.02f);

        g.setColour (Palette::edgeBright);
        juce::Path neck;
        neck.addLineSegment ({ potCentre, neckEnd }, neckW);
        g.fillPath (neck);

        // Pot (the drum body) and its head.
        const float potD = d * 0.62f;
        auto pot = juce::Rectangle<float> (potD, potD).withCentre (potCentre);
        g.setColour (Palette::edge);
        g.fillEllipse (pot);
        g.setColour (Palette::accent);
        g.drawEllipse (pot, juce::jmax (1.0f, d * 0.04f));

        g.setColour (Palette::textPrimary.withAlpha (0.92f));
        g.fillEllipse (pot.reduced (potD * 0.16f));   // the pale head

        // Bridge dot.
        g.setColour (Palette::edge);
        g.fillEllipse (juce::Rectangle<float> (d * 0.09f, d * 0.09f).withCentre (potCentre));

        // A few strings running from the head-stock down over the head.
        g.setColour (Palette::accentDim);
        const float sw = juce::jmax (0.6f, d * 0.02f);
        for (int i = -1; i <= 1; ++i)
        {
            const float off = (float) i * d * 0.06f;
            juce::Point<float> a (neckEnd.x + off, neckEnd.y + off);
            juce::Point<float> c (potCentre.x + off, potCentre.y - off);
            g.drawLine ({ a, c }, sw);
        }
    }

    bool hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BanjoReveal)
};

} // namespace luthier
