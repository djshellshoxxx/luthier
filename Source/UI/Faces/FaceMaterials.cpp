#include "FaceMaterials.h"

#include "../Theme.h"
#include "../../Accessibility/Accessibility.h"

namespace luthier::faces
{

namespace
{
    /** Runs `paint` clipped to `shape`, with the graphics state restored after. */
    template <typename Fn>
    void clipped (juce::Graphics& g, const juce::Path& shape, Fn&& paint)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (shape);
        paint (shape.getBounds());
    }

    /** A light falling from the top left across a surface: the one key light of visual-polish.md 1. */
    void keyLight (juce::Graphics& g, juce::Rectangle<float> b, float lightAlpha, float shadeAlpha)
    {
        juce::ColourGradient light (juce::Colours::white.withAlpha (lightAlpha), b.getX(), b.getY(),
                                    juce::Colours::black.withAlpha (shadeAlpha), b.getRight(), b.getBottom(), false);
        light.addColour (0.45, juce::Colours::white.withAlpha (0.0f));
        g.setGradientFill (light);
        g.fillRect (b);
    }
}

//==============================================================================
float hashNoise (juce::uint32 seed, int i) noexcept
{
    auto x = seed ^ (juce::uint32) (i * 0x9e3779b9u);
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return (float) (x & 0xffffffu) / (float) 0x1000000;
}

Materials Materials::current()
{
    const auto& p = Palette::current();
    Materials m;
    m.textured = Palette::textured;

    if (! m.textured)
    {
        // High contrast: flat surfaces in the palette's own roles, text in its text colour.
        for (auto* c : { &m.tolexBlack, &m.tweed, &m.fawn, &m.rust, &m.slate, &m.walnut, &m.oxbloodTolex })
            *c = p.panel;

        for (auto* c : { &m.chrome, &m.nickel, &m.brass, &m.copper, &m.aluminium, &m.darkMetal, &m.ivoryPanel, &m.blackPanel })
            *c = p.panelRaised;

        for (auto* c : { &m.grilleSilver, &m.grilleOxblood, &m.grilleBrown, &m.grilleBlack, &m.grilleGold })
            *c = p.panelSunken;

        m.piping = p.edgeBright;
        m.jewelRed = p.clip;
        m.ledGreen = p.success;
        m.ledAmber = p.warning;
        m.ledCool = p.secondary;
        m.tubeGlow = p.warning;
        m.inkLight = p.textPrimary;
        m.inkDark = p.textPrimary;
        return m;
    }

    // The black of the knobs is the black of the Tolex; brass is the plate brass.
    const auto black = Palette::knobBody;
    const auto brass = Palette::plate;

    m.tolexBlack   = black.interpolatedWith (p.panel, 0.18f);
    m.tweed        = brass.interpolatedWith (p.textPrimary, 0.42f);
    m.fawn         = p.textMuted.interpolatedWith (p.textPrimary, 0.35f);
    m.rust         = p.warning.interpolatedWith (p.accentDim, 0.35f);
    m.slate        = p.secondaryDim.interpolatedWith (black, 0.35f);
    m.walnut       = p.accentDim.interpolatedWith (black, 0.45f);
    m.oxbloodTolex = p.clip.interpolatedWith (black, 0.55f);

    m.chrome     = p.textPrimary.withSaturation (0.04f).withBrightness (0.86f);
    m.nickel     = m.chrome.withBrightness (0.70f);
    m.brass      = brass;
    m.copper     = brass.interpolatedWith (p.warning, 0.35f).darker (0.15f);
    m.aluminium  = p.textMuted.withSaturation (0.05f).withBrightness (0.80f);
    m.darkMetal  = black.interpolatedWith (m.nickel, 0.28f);
    m.ivoryPanel = p.textPrimary.withSaturation (0.14f).withBrightness (0.92f);
    m.blackPanel = black.interpolatedWith (p.panelSunken, 0.25f);

    m.grilleSilver  = m.chrome.withBrightness (0.62f);
    m.grilleOxblood = p.clip.interpolatedWith (black, 0.62f);
    m.grilleBrown   = p.accentDim.interpolatedWith (black, 0.42f);
    m.grilleBlack   = black.brighter (0.1f);
    m.grilleGold    = brass.interpolatedWith (black, 0.3f);

    m.piping   = p.textPrimary.withSaturation (0.1f).withBrightness (0.92f);
    m.jewelRed = p.clip;
    m.ledGreen = p.success;
    m.ledAmber = p.warning;
    m.ledCool  = p.secondary;
    m.tubeGlow = p.warning.interpolatedWith (p.accentBright, 0.4f);
    m.inkLight = m.piping;
    m.inkDark  = Palette::plateText;
    return m;
}

juce::Colour inkFor (juce::Colour surface)
{
    const auto& p = Palette::current();

    juce::Colour best = p.textPrimary;
    double bestRatio = 0.0;

    for (auto c : { p.textPrimary, Palette::plateText, Materials::current().inkLight, Materials::current().inkDark })
    {
        const double r = PaletteColours::contrastRatio (c, surface);

        if (r > bestRatio)
        {
            bestRatio = r;
            best = c;
        }
    }

    return best;
}

//==============================================================================
void fillTolex (juce::Graphics& g, const juce::Path& shape, juce::Colour base, juce::uint32 seed)
{
    g.setColour (base);
    g.fillPath (shape);

    if (! Palette::textured)
        return;

    clipped (g, shape, [&] (juce::Rectangle<float> b)
    {
        keyLight (g, b, 0.10f, 0.22f);

        // The pebbled grain: fine light and dark flecks.
        const int n = (int) juce::jlimit (60.0f, 3000.0f, b.getWidth() * b.getHeight() / 16.0f);

        for (int i = 0; i < n; ++i)
        {
            const float x = b.getX() + hashNoise (seed, i * 3) * b.getWidth();
            const float y = b.getY() + hashNoise (seed, i * 3 + 1) * b.getHeight();
            const bool light = hashNoise (seed, i * 3 + 2) > 0.5f;
            g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (light ? 0.045f : 0.09f));
            g.fillEllipse (x, y, 1.4f, 1.1f);
        }
    });
}

void fillTweed (juce::Graphics& g, const juce::Path& shape, juce::Colour base)
{
    g.setColour (base);
    g.fillPath (shape);

    if (! Palette::textured)
        return;

    clipped (g, shape, [&] (juce::Rectangle<float> b)
    {
        // The diagonal twill of lacquered tweed: alternating dark and light threads.
        const float step = 2.6f;
        int k = 0;

        for (float d = -b.getHeight(); d < b.getWidth(); d += step, ++k)
        {
            g.setColour ((k % 3 == 0 ? juce::Colours::black.withAlpha (0.16f) : juce::Colours::white.withAlpha (0.05f)));
            g.drawLine (b.getX() + d, b.getBottom(), b.getX() + d + b.getHeight(), b.getY(), 0.9f);
        }

        // Aged lacquer: darker and warmer toward the edges.
        juce::ColourGradient lacquer (juce::Colours::black.withAlpha (0.0f), b.getCentreX(), b.getCentreY(),
                                      base.darker (0.9f).withAlpha (0.35f), b.getX(), b.getY(), true);
        g.setGradientFill (lacquer);
        g.fillRect (b);
        keyLight (g, b, 0.08f, 0.12f);
    });
}

void fillHammertone (juce::Graphics& g, const juce::Path& shape, juce::Colour base, juce::uint32 seed)
{
    g.setColour (base);
    g.fillPath (shape);

    if (! Palette::textured)
        return;

    clipped (g, shape, [&] (juce::Rectangle<float> b)
    {
        const int n = (int) juce::jlimit (40.0f, 900.0f, b.getWidth() * b.getHeight() / 40.0f);

        for (int i = 0; i < n; ++i)
        {
            const float x = b.getX() + hashNoise (seed, i * 4) * b.getWidth();
            const float y = b.getY() + hashNoise (seed, i * 4 + 1) * b.getHeight();
            const float r = 1.5f + hashNoise (seed, i * 4 + 2) * 3.0f;
            const bool light = hashNoise (seed, i * 4 + 3) > 0.55f;
            g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (light ? 0.07f : 0.12f));
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 1.7f);
        }

        keyLight (g, b, 0.22f, 0.2f);
    });
}

void fillGloss (juce::Graphics& g, const juce::Path& shape, juce::Colour base)
{
    g.setColour (base);
    g.fillPath (shape);

    if (! Palette::textured)
        return;

    clipped (g, shape, [&] (juce::Rectangle<float> b)
    {
        keyLight (g, b, 0.24f, 0.22f);

        // A soft specular band across the upper left.
        juce::Path band;
        band.addEllipse (b.getX() - b.getWidth() * 0.2f, b.getY() - b.getHeight() * 0.35f,
                         b.getWidth() * 0.9f, b.getHeight() * 0.55f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillPath (band);
    });
}

void fillBrushedMetal (juce::Graphics& g, const juce::Path& shape, juce::Colour base)
{
    g.setColour (base);
    g.fillPath (shape);

    if (! Palette::textured)
        return;

    clipped (g, shape, [&] (juce::Rectangle<float> b)
    {
        juce::ColourGradient sheen (base.brighter (0.35f), b.getX(), b.getY(), base.darker (0.25f), b.getX(), b.getBottom(), false);
        sheen.addColour (0.42, base.darker (0.12f));
        sheen.addColour (0.55, base.brighter (0.18f));
        g.setGradientFill (sheen);
        g.fillRect (b);

        for (float y = b.getY(); y < b.getBottom(); y += 1.3f)
        {
            const float a = hashNoise (0x51u, (int) (y * 7.0f));
            g.setColour ((a > 0.5f ? juce::Colours::white : juce::Colours::black).withAlpha (0.03f + 0.05f * a));
            g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
        }
    });
}

void fillDiamondPlate (juce::Graphics& g, const juce::Path& shape, juce::Colour base)
{
    fillBrushedMetal (g, shape, base);

    if (! Palette::textured)
        return;

    clipped (g, shape, [&] (juce::Rectangle<float> b)
    {
        // Raised lozenges, alternating in direction, as on checker plate.
        const float step = juce::jlimit (5.0f, 10.0f, b.getHeight() / 5.0f);
        int row = 0;

        for (float y = b.getY() + step * 0.5f; y < b.getBottom() + step; y += step, ++row)
        {
            int col = 0;

            for (float x = b.getX() + (row % 2 ? step * 0.5f : 0.0f); x < b.getRight() + step; x += step, ++col)
            {
                const float angle = ((row + col) % 2 == 0) ? 0.785f : -0.785f;
                juce::Path bump;
                bump.addRoundedRectangle (-step * 0.36f, -step * 0.09f, step * 0.72f, step * 0.18f, step * 0.09f);
                bump.applyTransform (juce::AffineTransform::rotation (angle).translated (x, y));

                g.setColour (juce::Colours::black.withAlpha (0.28f));
                g.fillPath (bump, juce::AffineTransform::translation (0.6f, 0.8f));
                g.setColour (base.brighter (0.4f));
                g.fillPath (bump);
            }
        }
    });
}

void fillWood (juce::Graphics& g, const juce::Path& shape, juce::Colour base, juce::uint32 seed)
{
    g.setColour (base);
    g.fillPath (shape);

    if (! Palette::textured)
        return;

    clipped (g, shape, [&] (juce::Rectangle<float> b)
    {
        // Long wavy grain lines along the board.
        const int lines = juce::jmax (6, (int) (b.getHeight() / 2.4f));

        for (int i = 0; i < lines; ++i)
        {
            const float y0 = b.getY() + (i + hashNoise (seed, i)) * b.getHeight() / (float) lines;
            const float amp = 0.6f + 1.8f * hashNoise (seed, i + 100);
            const float freq = 0.01f + 0.02f * hashNoise (seed, i + 200);

            juce::Path grain;
            grain.startNewSubPath (b.getX(), y0);

            for (float x = b.getX(); x <= b.getRight(); x += 6.0f)
                grain.lineTo (x, y0 + amp * std::sin ((x - b.getX()) * freq + (float) i));

            g.setColour ((i % 3 == 0 ? juce::Colours::white.withAlpha (0.05f) : juce::Colours::black.withAlpha (0.14f)));
            g.strokePath (grain, juce::PathStrokeType (0.8f));
        }

        keyLight (g, b, 0.14f, 0.2f);
    });
}

void fillGrille (juce::Graphics& g, const juce::Path& shape, Grille style, juce::Colour base, juce::uint32 seed)
{
    g.setColour (base);
    g.fillPath (shape);

    if (! Palette::textured)
        return;

    const auto m = Materials::current();

    clipped (g, shape, [&] (juce::Rectangle<float> b)
    {
        // The weave every cloth shares: fine crossing threads.
        for (float x = b.getX(); x < b.getRight(); x += 2.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
        }

        for (float y = b.getY(); y < b.getBottom(); y += 2.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
        }

        switch (style)
        {
            case Grille::silverSparkle:
            {
                const int n = (int) juce::jlimit (30.0f, 1200.0f, b.getWidth() * b.getHeight() / 30.0f);

                for (int i = 0; i < n; ++i)
                {
                    const float x = b.getX() + hashNoise (seed, i * 2) * b.getWidth();
                    const float y = b.getY() + hashNoise (seed, i * 2 + 1) * b.getHeight();
                    g.setColour (juce::Colours::white.withAlpha (0.35f));
                    g.fillRect (x, y, 1.0f, 1.0f);
                }
                break;
            }

            case Grille::oxbloodStripe:
            {
                // Pale threads running across the cloth every few millimetres.
                const float step = juce::jlimit (4.0f, 8.0f, b.getHeight() / 10.0f);

                for (float y = b.getY() + step * 0.5f; y < b.getBottom(); y += step)
                {
                    g.setColour (m.grilleGold.withAlpha (0.55f));
                    g.fillRect (b.getX(), y, b.getWidth(), 0.9f);
                }
                break;
            }

            case Grille::diamond:
            {
                // The diamond lattice woven into the cloth.
                const float step = juce::jlimit (7.0f, 14.0f, b.getHeight() / 5.0f);

                for (float d = -b.getHeight(); d < b.getWidth() + b.getHeight(); d += step)
                {
                    g.setColour (base.brighter (0.45f).withAlpha (0.55f));
                    g.drawLine (b.getX() + d, b.getY(), b.getX() + d + b.getHeight(), b.getBottom(), 1.2f);
                    g.drawLine (b.getX() + d, b.getBottom(), b.getX() + d + b.getHeight(), b.getY(), 1.2f);
                }
                break;
            }

            case Grille::basket:
            {
                // Basket weave: small blocks of threads running alternately across and along.
                const float cell = 4.0f;
                int row = 0;

                for (float y = b.getY(); y < b.getBottom(); y += cell, ++row)
                {
                    int col = 0;

                    for (float x = b.getX(); x < b.getRight(); x += cell, ++col)
                    {
                        const bool across = (row + col) % 2 == 0;
                        g.setColour ((across ? juce::Colours::white : juce::Colours::black).withAlpha (across ? 0.08f : 0.16f));

                        if (across)
                            g.fillRect (x, y + 1.0f, cell, cell - 2.0f);
                        else
                            g.fillRect (x + 1.0f, y, cell - 2.0f, cell);
                    }
                }

                // The salt-and-pepper flecks of the pale threads.
                const int n = (int) juce::jlimit (20.0f, 600.0f, b.getWidth() * b.getHeight() / 60.0f);

                for (int i = 0; i < n; ++i)
                {
                    g.setColour (m.piping.withAlpha (0.25f));
                    g.fillRect (b.getX() + hashNoise (seed, i * 2) * b.getWidth(),
                                b.getY() + hashNoise (seed, i * 2 + 1) * b.getHeight(), 1.2f, 1.2f);
                }
                break;
            }

            case Grille::blackMesh:
            {
                const float step = 3.0f;

                for (float y = b.getY() + 1.5f; y < b.getBottom(); y += step)
                    for (float x = b.getX() + ((int) (y / step) % 2 ? 1.5f : 0.0f); x < b.getRight(); x += step)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.07f));
                        g.fillEllipse (x, y, 1.2f, 1.2f);
                    }
                break;
            }
        }

        // The cloth sits back from the baffle: shade along its top and left.
        juce::ColourGradient inset (juce::Colours::black.withAlpha (0.45f), b.getX(), b.getY(),
                                    juce::Colours::black.withAlpha (0.0f), b.getX(), b.getY() + juce::jmin (12.0f, b.getHeight() * 0.25f), false);
        g.setGradientFill (inset);
        g.fillRect (b);
    });
}

//==============================================================================
void drawJewel (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, bool lit)
{
    const auto m = Materials::current();
    const float jewel = r * 0.72f;

    if (! m.textured)
    {
        g.setColour (lit ? colour : Palette::current().panelSunken);
        g.fillEllipse (c.x - jewel, c.y - jewel, jewel * 2.0f, jewel * 2.0f);
        g.setColour (Palette::current().edgeBright);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
        return;
    }

    if (lit)
    {
        juce::ColourGradient glow (colour.withAlpha (0.45f), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 1.5f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - r * 1.5f, c.y - r * 1.5f, r * 3.0f, r * 3.0f);
    }

    // The chrome bezel.
    juce::ColourGradient bezel (m.chrome.brighter (0.3f), c.x - r, c.y - r, m.chrome.darker (0.6f), c.x + r, c.y + r, false);
    g.setGradientFill (bezel);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.8f);

    // The jewel and its facets.
    const auto dome = lit ? colour : colour.darker (1.2f).withMultipliedSaturation (0.6f);
    juce::ColourGradient body (dome.brighter (lit ? 0.9f : 0.3f), c.x - jewel * 0.3f, c.y - jewel * 0.3f,
                               dome.darker (0.4f), c.x + jewel, c.y + jewel, true);
    g.setGradientFill (body);
    g.fillEllipse (c.x - jewel, c.y - jewel, jewel * 2.0f, jewel * 2.0f);

    g.setColour (juce::Colours::white.withAlpha (lit ? 0.3f : 0.12f));

    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f;
        g.drawLine (c.x, c.y, c.x + jewel * std::cos (a), c.y + jewel * std::sin (a), 0.6f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.fillEllipse (c.x - jewel * 0.5f, c.y - jewel * 0.55f, jewel * 0.45f, jewel * 0.3f);
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, bool lit)
{
    const auto m = Materials::current();

    if (! m.textured)
    {
        g.setColour (lit ? colour : Palette::current().panelSunken);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (Palette::current().edgeBright);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
        return;
    }

    if (lit)
    {
        juce::ColourGradient glow (colour.withAlpha (0.5f), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 2.2f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - r * 2.2f, c.y - r * 2.2f, r * 4.4f, r * 4.4f);
    }

    g.setColour (m.nickel.darker (0.4f));
    g.fillEllipse (c.x - r * 1.25f, c.y - r * 1.25f, r * 2.5f, r * 2.5f);

    const auto dome = lit ? colour.brighter (0.2f) : colour.darker (1.4f);
    juce::ColourGradient body (dome.brighter (0.8f), c.x - r * 0.3f, c.y - r * 0.4f, dome, c.x + r, c.y + r, true);
    g.setGradientFill (body);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
}

void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
{
    const auto m = Materials::current();

    if (! m.textured)
        return;

    juce::ColourGradient head (m.chrome, c.x - r, c.y - r, m.nickel.darker (0.5f), c.x + r, c.y + r, false);
    g.setGradientFill (head);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (c.x - r * 0.7f, c.y + r * 0.7f, c.x + r * 0.7f, c.y - r * 0.7f, juce::jmax (0.6f, r * 0.3f));
}

void drawJack (juce::Graphics& g, juce::Point<float> c, float r)
{
    const auto m = Materials::current();

    if (! m.textured)
    {
        g.setColour (Palette::current().edgeBright);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
        g.fillEllipse (c.x - r * 0.4f, c.y - r * 0.4f, r * 0.8f, r * 0.8f);
        return;
    }

    // The hex-ish nut, the washer and the socket.
    juce::Path nut;
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f + 0.52f;
        const juce::Point<float> v (c.x + r * std::cos (a), c.y + r * std::sin (a));

        if (i == 0)
            nut.startNewSubPath (v);
        else
            nut.lineTo (v);
    }
    nut.closeSubPath();

    juce::ColourGradient metal (m.chrome.brighter (0.2f), c.x - r, c.y - r, m.nickel.darker (0.4f), c.x + r, c.y + r, false);
    g.setGradientFill (metal);
    g.fillPath (nut);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.strokePath (nut, juce::PathStrokeType (0.7f));

    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.45f, r * 0.9f, r * 0.9f);
}

void drawCornerCap (juce::Graphics& g, juce::Rectangle<float> cabinet, int corner, float size)
{
    const auto m = Materials::current();

    if (! m.textured)
        return;

    // Built at the top-left corner, then turned into place.
    juce::Path cap;
    cap.startNewSubPath (0.0f, size);
    cap.lineTo (0.0f, size * 0.3f);
    cap.quadraticTo (0.0f, 0.0f, size * 0.3f, 0.0f);
    cap.lineTo (size, 0.0f);
    cap.quadraticTo (size * 0.55f, size * 0.25f, size * 0.45f, size * 0.45f);
    cap.quadraticTo (size * 0.25f, size * 0.55f, 0.0f, size);
    cap.closeSubPath();

    const float angle = juce::MathConstants<float>::halfPi * (float) corner;
    const juce::Point<float> at[] = { cabinet.getTopLeft(), cabinet.getTopRight(), cabinet.getBottomRight(), cabinet.getBottomLeft() };
    const auto place = juce::AffineTransform::rotation (angle).translated (at[corner & 3]);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (cap, place.translated (0.8f, 1.2f));

    juce::ColourGradient metal (m.chrome.brighter (0.2f), at[corner & 3].x, at[corner & 3].y,
                                m.nickel.darker (0.3f), cabinet.getCentreX(), cabinet.getCentreY(), false);
    g.setGradientFill (metal);
    g.fillPath (cap, place);

    const auto rivet = juce::Point<float> (size * 0.22f, size * 0.22f).transformedBy (place);
    g.setColour (m.nickel.darker (0.6f));
    g.fillEllipse (rivet.x - size * 0.07f, rivet.y - size * 0.07f, size * 0.14f, size * 0.14f);
}

void drawRocker (juce::Graphics& g, juce::Rectangle<float> area, bool on, juce::Colour lamp)
{
    const auto m = Materials::current();
    const auto& p = Palette::current();

    auto frame = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight() * 0.62f), area.getHeight());

    g.setColour (m.textured ? m.blackPanel.darker (0.3f) : p.panelSunken);
    g.fillRoundedRectangle (frame, 1.5f);
    g.setColour (m.textured ? m.nickel.darker (0.5f) : p.edgeBright);
    g.drawRoundedRectangle (frame, 1.5f, 0.8f);

    // The rocker, pressed at its top when on, with a lens that glows.
    auto rocker = frame.reduced (frame.getWidth() * 0.16f, frame.getHeight() * 0.08f);
    auto upper = rocker.removeFromTop (rocker.getHeight() * 0.5f);

    const auto face = on ? lamp : lamp.darker (1.3f);
    g.setColour (m.textured ? face : (on ? p.accent : p.panelSunken));
    g.fillRect (on ? upper : rocker);

    if (m.textured)
    {
        g.setColour (juce::Colours::white.withAlpha (on ? 0.25f : 0.08f));
        g.fillRect ((on ? upper : rocker).removeFromTop (juce::jmax (1.0f, frame.getHeight() * 0.08f)));
    }
}

void drawFootswitch (juce::Graphics& g, juce::Rectangle<float> area, bool down)
{
    const auto m = Materials::current();
    const auto& p = Palette::current();
    const auto c = area.getCentre();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;

    // The hex nut.
    juce::Path nut;
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f;
        const juce::Point<float> v (c.x + r * std::cos (a), c.y + r * std::sin (a));

        if (i == 0)
            nut.startNewSubPath (v);
        else
            nut.lineTo (v);
    }
    nut.closeSubPath();

    if (! m.textured)
    {
        g.setColour (p.panelRaised);
        g.fillPath (nut);
        g.setColour (p.edgeBright);
        g.strokePath (nut, juce::PathStrokeType (1.0f));
        const float b = r * 0.62f;
        g.drawEllipse (c.x - b, c.y - b, b * 2.0f, b * 2.0f, 1.2f);
        return;
    }

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (nut, juce::AffineTransform::translation (1.0f, 1.5f));

    juce::ColourGradient metal (m.chrome.brighter (0.25f), c.x - r, c.y - r, m.nickel.darker (0.45f), c.x + r, c.y + r, false);
    g.setGradientFill (metal);
    g.fillPath (nut);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (nut, juce::PathStrokeType (0.8f));

    // The domed button: lower and darker while pressed.
    const float b = r * (down ? 0.56f : 0.62f);
    juce::ColourGradient dome (m.chrome.brighter (down ? 0.0f : 0.4f), c.x - b * 0.5f, c.y - b * 0.6f,
                               m.nickel.darker (down ? 0.6f : 0.3f), c.x + b, c.y + b, true);
    g.setGradientFill (dome);
    g.fillEllipse (c.x - b, c.y - b, b * 2.0f, b * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - b, c.y - b, b * 2.0f, b * 2.0f, 0.8f);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.fillEllipse (c.x - b * 0.55f, c.y - b * 0.6f, b * 0.6f, b * 0.35f);
}

void drawTubeVent (juce::Graphics& g, juce::Rectangle<float> area, int numTubes, float drive, bool standby, bool stale)
{
    const auto m = Materials::current();
    const auto& p = Palette::current();

    // visual-polish.md 4: the valves glow with the drive level; stale readings go grey.
    const float glow = standby ? 0.08f : juce::jlimit (0.0f, 1.0f, 0.25f + 0.75f * drive);
    const auto glowColour = stale ? p.textDisabled : m.tubeGlow;

    g.setColour (m.textured ? juce::Colours::black.withAlpha (0.8f) : p.panelSunken);
    g.fillRoundedRectangle (area, 2.0f);

    const float slotW = area.getWidth() / (float) juce::jmax (1, numTubes);

    for (int i = 0; i < numTubes; ++i)
    {
        auto slot = juce::Rectangle<float> (area.getX() + slotW * (float) i, area.getY(), slotW, area.getHeight()).reduced (slotW * 0.22f, area.getHeight() * 0.12f);
        const auto c = slot.getCentre();

        if (m.textured)
        {
            juce::ColourGradient halo (glowColour.withAlpha (0.65f * glow), c.x, slot.getBottom() - slot.getHeight() * 0.25f,
                                       glowColour.withAlpha (0.0f), c.x, slot.getY(), true);
            g.setGradientFill (halo);
            g.fillRect (slot.expanded (slotW * 0.2f, 0.0f));
        }

        // The bottle and its glowing heater.
        g.setColour (m.textured ? m.chrome.withAlpha (0.28f) : p.edgeBright);
        g.drawRoundedRectangle (slot, slot.getWidth() * 0.45f, 0.8f);
        g.setColour (glowColour.withAlpha (m.textured ? glow : (standby ? 0.3f : 1.0f)));
        g.fillEllipse (slot.withSizeKeepingCentre (slot.getWidth() * 0.35f, slot.getHeight() * 0.3f)
                           .withY (slot.getBottom() - slot.getHeight() * 0.42f));
    }

    // Slats across the vent.
    g.setColour (m.textured ? m.nickel.darker (0.3f) : p.edge);
    for (float y = area.getY() + area.getHeight() * 0.25f; y < area.getBottom(); y += area.getHeight() * 0.25f)
        g.fillRect (area.getX(), y, area.getWidth(), 1.2f);

    g.setColour (m.textured ? m.nickel.darker (0.5f) : p.edgeBright);
    g.drawRoundedRectangle (area, 2.0f, 1.0f);
}

//==============================================================================
void drawPrint (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Colour colour,
                float height, bool display, juce::Justification justification, int maxLines)
{
    if (text.isEmpty() || area.isEmpty())
        return;

    g.setColour (colour);
    g.setFont (display ? Fonts::display (height) : Fonts::ui (height, true));
    g.drawFittedText (text, area.toNearestInt(), justification, juce::jmax (1, maxLines), 0.6f);
}

//==============================================================================
float printWidth (const juce::String& text, float height, bool display)
{
    return juce::GlyphArrangement::getStringWidth (display ? Fonts::display (height) : Fonts::ui (height, true), text);
}

float fitPrintHeight (const juce::String& text, juce::Rectangle<float> area, float maxHeight, bool display)
{
    if (text.isEmpty() || area.isEmpty())
        return 0.0f;

    // Half a pixel spare, so rounding in the glyph layout never clips the last letter.
    const float room = area.getWidth() - 0.5f;
    float height = juce::jmin (maxHeight, area.getHeight());

    if (height < minPrintHeight)
        return 0.0f;

    // Width grows with height, so one scale lands close; hinting makes it not
    // quite linear, and the loop takes up the difference.
    if (const float w = printWidth (text, height, display); w > room && w > 0.0f)
        height *= room / w;

    while (height >= minPrintHeight && printWidth (text, height, display) > room)
        height -= 0.25f;

    return height >= minPrintHeight ? height : 0.0f;
}

FittedPrint fitPrint (const juce::String& text, const juce::String& shortForm, juce::Rectangle<float> area,
                      float maxHeight, bool display)
{
    for (const auto& candidate : { text, shortForm })
        if (const float h = fitPrintHeight (candidate, area, maxHeight, display); h > 0.0f)
            return { candidate, h };

    return {};
}

void drawFittedPrint (juce::Graphics& g, const FittedPrint& print, juce::Rectangle<float> area, juce::Colour colour,
                      bool display, juce::Justification justification)
{
    if (! print.isVisible() || area.isEmpty())
        return;

    g.setColour (colour);
    g.setFont (display ? Fonts::display (print.height) : Fonts::ui (print.height, true));
    g.drawText (print.text, area, justification, false);
}

//==============================================================================
juce::uint64 paletteDigest()
{
    const auto& p = Palette::current();
    juce::uint64 digest = 1469598103934665603ull;

    auto mix = [&digest] (juce::uint32 v) { digest = (digest ^ v) * 1099511628211ull; };

    for (auto c : { p.backgroundDeep, p.background, p.panel, p.panelRaised, p.panelSunken, p.edge, p.edgeBright,
                    p.accent, p.accentBright, p.accentDim, p.secondary, p.secondaryDim,
                    p.textPrimary, p.textMuted, p.textDisabled, p.success, p.warning, p.clip,
                    Palette::accent, Palette::knobBody, Palette::knobPointer, Palette::plate, Palette::plateText })
        mix (c.getARGB());

    mix (Palette::textured ? 1u : 0u);
    return digest;
}

} // namespace luthier::faces
