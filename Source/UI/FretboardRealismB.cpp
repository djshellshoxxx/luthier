/*  REALISM-B's layer of the fretboard illustration (FretboardComponent.h).

    harmonic-realism.md 7: during a touch, a hollow ring at the contact,
    fading over the touch time; a missed (off-node) touch draws dashed.
    string-interaction.md 9 (gui-techniques-updates.md 4's mute zone): the
    palm's coverage, band opacity = its weight on that string.
    fingerstyle-attack.md 7: each string's tool glyph at the picking end.
*/

#include "FretboardComponent.h"
#include "RightHandGroup.h"
#include "../PluginProcessor.h"

namespace luthier
{

bool FretboardComponent::refreshRealismB() noexcept
{
    auto& engine = processor.getEngine();
    bool changed = false;

    for (int s = 0; s < (int) realismB.size(); ++s)
    {
        RealismBView v;

        if (s < numStrings)
        {
            const auto& d = engine.getContactDisplay (s);
            v.life = d.life.load (std::memory_order_relaxed);
            v.touchFret = v.life > 0.0f ? d.fret.load (std::memory_order_relaxed) - (float) capoFret : -1.0f;
            v.missed = d.missed.load (std::memory_order_relaxed);
            v.palm = engine.getPalmWeight (s);
            v.tool = (int) engine.getRightHand().toolForString (s, engine.getGuitarSpec().twelveString,
                                                                GuitarLibrary::courseForString (s));
        }

        auto& old = realismB[(size_t) s];

        if (std::abs (old.life - v.life) > 0.02f || std::abs (old.palm - v.palm) > 0.02f
            || old.tool != v.tool || old.missed != v.missed || std::abs (old.touchFret - v.touchFret) > 0.01f)
            changed = true;

        old = v;
    }

    return changed;
}

void FretboardComponent::paintRealismB (juce::Graphics& g)
{
    const float laneHalf = (float) boardArea.getHeight() / (float) juce::jmax (1, numStrings) * 0.5f;

    for (int s = 0; s < numStrings; ++s)
    {
        const auto& v = realismB[(size_t) s];
        const float y = stringY (s);

        // The palm's band, across the board's picking end.
        if (v.palm > 0.01f)
        {
            auto band = juce::Rectangle<float> ((float) boardArea.getRight() - 46.0f, y - laneHalf, 46.0f, laneHalf * 2.0f);
            g.setColour (Palette::warning.withAlpha (0.35f * juce::jlimit (0.0f, 1.0f, v.palm)));
            g.fillRect (band);
        }

        // The tool glyph, at the picking end.
        if (v.tool != 0)
        {
            g.setColour (Palette::textMuted);
            g.setFont (Fonts::ui (9.0f));
            g.drawText (StringToolCell::glyphFor (v.tool),
                        juce::Rectangle<float> ((float) boardArea.getRight() - 18.0f, y - 7.0f, 16.0f, 14.0f),
                        juce::Justification::centred, false);
        }

        // The touch.
        if (v.touchFret >= 0.0f && v.life > 0.0f && v.touchFret <= (float) numFrets + 0.5f)
        {
            const float x = fretX (juce::jmax (0.0, (double) v.touchFret));
            const float r = 6.0f;
            juce::Path ring;
            ring.addEllipse (x - r, y - r, 2.0f * r, 2.0f * r);

            g.setColour (Palette::accentBright.withAlpha (juce::jlimit (0.0f, 1.0f, 0.25f + 0.75f * v.life)));

            if (v.missed)
            {
                juce::Path dashed;
                const float dashes[] = { 2.0f, 2.0f };
                juce::PathStrokeType (1.4f).createDashedStroke (dashed, ring, dashes, 2);
                g.fillPath (dashed);
            }
            else
            {
                g.strokePath (ring, juce::PathStrokeType (1.6f));
            }
        }
    }
}

} // namespace luthier
