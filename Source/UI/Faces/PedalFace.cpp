#include "PedalFace.h"
#include "FaceMaterials.h"

namespace luthier::faces
{

namespace
{
    enum class Body { box, treadle, cabinet, tank, empty };
    enum class Finish { gloss, hammertone, brushed };

    struct Look
    {
        Body body = Body::box;
        Finish finish = Finish::gloss;
        juce::Colour colour;
        KnobCap cap = KnobCap::ribbed;
        juce::Colour led;
        bool band = false;           ///< a contrasting band behind the name
        juce::Colour bandColour;
        juce::uint32 seed = 1;
    };

    Look lookFor (PedalType type)
    {
        const auto m = Materials::current();
        const auto& p = Palette::current();

        Look l;
        l.seed = 0x2000u + (juce::uint32) type * 104729u;
        l.led = m.jewelRed;

        switch (type)
        {
            case PedalType::Compressor:
                l.colour = m.slate.brighter (0.45f); l.cap = KnobCap::ribbed; l.led = m.ledGreen; break;

            case PedalType::NoiseGate:
                l.colour = m.tolexBlack.brighter (0.15f); l.cap = KnobCap::ribbed;
                l.band = true; l.bandColour = m.piping; break;

            case PedalType::Wah:
                l.body = Body::treadle; l.colour = m.darkMetal; l.cap = KnobCap::ribbed; break;

            case PedalType::VolumePedal:
                l.body = Body::treadle; l.colour = m.tolexBlack.brighter (0.1f); l.cap = KnobCap::ribbed; l.led = m.ledGreen; break;

            case PedalType::EnvelopeFilter:
                l.colour = p.clip.interpolatedWith (p.secondaryDim, 0.45f); l.cap = KnobCap::creamRibbed; break;

            case PedalType::Octaver:
                l.colour = m.aluminium; l.finish = Finish::brushed; l.cap = KnobCap::ribbed; break;

            case PedalType::PitchShifter:
                l.colour = m.slate; l.cap = KnobCap::chromeSkirt; l.led = m.ledGreen; break;

            case PedalType::Overdrive:
                l.colour = p.secondary.interpolatedWith (p.success, 0.4f).darker (0.2f); l.cap = KnobCap::creamRibbed;
                l.band = true; l.bandColour = p.secondary.interpolatedWith (p.success, 0.4f).brighter (0.35f); break;

            case PedalType::Distortion:
                l.colour = p.warning.interpolatedWith (p.accent, 0.3f); l.cap = KnobCap::ribbed; break;

            case PedalType::Fuzz:
                l.colour = p.clip.darker (0.4f); l.finish = Finish::hammertone; l.cap = KnobCap::witchHat; break;

            case PedalType::Boost:
                l.colour = m.ivoryPanel; l.cap = KnobCap::ribbed; break;

            case PedalType::Chorus:
                l.colour = p.secondary.brighter (0.4f).withMultipliedSaturation (0.7f); l.cap = KnobCap::ribbed; break;

            case PedalType::Phaser:
                l.colour = p.warning.interpolatedWith (p.clip, 0.35f); l.cap = KnobCap::ribbed; break;

            case PedalType::Flanger:
                l.colour = m.aluminium.interpolatedWith (p.secondaryDim, 0.35f); l.finish = Finish::brushed; l.cap = KnobCap::chromeDome; break;

            case PedalType::Tremolo:
                l.colour = m.tolexBlack.interpolatedWith (p.secondaryDim, 0.4f); l.cap = KnobCap::witchHat; l.led = m.ledAmber;
                l.band = true; l.bandColour = m.brass; break;

            case PedalType::RotarySpeaker:
                l.body = Body::cabinet; l.colour = m.walnut.brighter (0.2f); l.cap = KnobCap::chickenHead; l.led = m.ledAmber; break;

            case PedalType::Delay:
                l.colour = m.aluminium.darker (0.1f); l.finish = Finish::brushed; l.cap = KnobCap::chromeSkirt; l.led = m.ledGreen; break;

            case PedalType::Reverb:
                l.colour = m.slate.brighter (0.15f); l.cap = KnobCap::ribbed; l.led = m.ledCool; break;

            case PedalType::SpringReverb:
                l.body = Body::tank; l.colour = m.oxbloodTolex.interpolatedWith (m.walnut, 0.5f); l.cap = KnobCap::pointer; break;

            case PedalType::GraphicEQ:
                l.colour = m.blackPanel.brighter (0.15f); l.cap = KnobCap::ribbed; break;

            case PedalType::ParametricEQ:
                l.colour = m.darkMetal; l.finish = Finish::brushed; l.cap = KnobCap::chromeSkirt; l.led = m.ledGreen; break;

            // ambiguity-resolutions 3: a studio ADT unit - brushed, with the
            // tape-deck brass band and an amber lamp.
            case PedalType::Doubler:
                l.colour = m.slate.interpolatedWith (p.accentDim, 0.3f); l.finish = Finish::brushed;
                l.cap = KnobCap::chromeDome; l.led = m.ledAmber;
                l.band = true; l.bandColour = m.brass; break;

            case PedalType::None:
            case PedalType::NumTypes:
            default:
                l.body = Body::empty; l.colour = p.panelSunken; break;
        }

        if (! m.textured)
        {
            l.colour = p.panelRaised;
            l.bandColour = p.panelSunken;
        }

        return l;
    }

    juce::Path rounded (juce::Rectangle<float> r, float corner)
    {
        juce::Path p;
        p.addRoundedRectangle (r, corner);
        return p;
    }

    void fillFinish (juce::Graphics& g, const juce::Path& shape, const Look& look)
    {
        switch (look.finish)
        {
            case Finish::hammertone: fillHammertone (g, shape, look.colour, look.seed); break;
            case Finish::brushed:    fillBrushedMetal (g, shape, look.colour); break;
            case Finish::gloss:
            default:                 fillGloss (g, shape, look.colour); break;
        }
    }

    /** A grid of cells for n knobs: rows of up to `perRow`, the last row centred. */
    void gridOfKnobs (PedalFaceLayout& l, juce::Rectangle<float> area, int n, int perRow, bool withLabels, bool square = true)
    {
        if (n <= 0 || area.isEmpty())
            return;

        const int cols = juce::jmin (n, perRow);
        const int rows = (n + cols - 1) / cols;
        const float cellW = area.getWidth() / (float) cols;
        const float cellH = area.getHeight() / (float) rows;
        const float knobZone = withLabels ? cellH * 0.74f : cellH;
        const float knob = juce::jmin (cellW * 0.92f, knobZone);

        for (int i = 0; i < n; ++i)
        {
            const int row = i / cols;
            const int inRow = juce::jmin (cols, n - row * cols);
            const int col = i % cols;
            const float rowOffset = (area.getWidth() - cellW * (float) inRow) * 0.5f;

            const auto cell = juce::Rectangle<float> (area.getX() + rowOffset + cellW * (float) col,
                                                      area.getY() + cellH * (float) row, cellW, cellH);
            l.knobs[(size_t) i] = square ? juce::Rectangle<float> (knob, knob).withCentre ({ cell.getCentreX(), cell.getY() + knobZone * 0.5f })
                                         : cell.withHeight (knobZone).reduced (cellW * 0.08f, 2.0f);
            l.labels[(size_t) i] = withLabels ? cell.withTrimmedTop (knobZone) : juce::Rectangle<float>();
        }
    }

    juce::String shortLabel (const juce::String& name)
    {
        return name.upToFirstOccurrenceOf (" (", false, false).toUpperCase().trim();
    }

    /** The narrowest cell a knob keeps a usable size in (a 36 px knob and its arc). */
    constexpr float minKnobCell = 42.0f;

    /** A knob or slider this size or more is usable; preferredHeightOnItsSide() grows the face until they all are. */
    constexpr float usableKnob = 34.0f, usableSliderSlot = 92.0f;

    /** How many knobs a row on its side takes: the pedal's own arrangement
        (up to five across, else two rows), fewer when the row is too narrow. */
    int knobsPerRow (float width, int n)
    {
        const int natural = n <= 5 ? juce::jmax (1, n) : (n + 1) / 2;
        const int fit = juce::jmax (1, (int) (width / minKnobCell));

        if (natural <= fit)
            return natural;

        const int rows = (n + fit - 1) / fit;
        return (n + rows - 1) / rows;
    }

    /** The share of a pedal on its side that the rotary's louvres or the tank's cloth take at the left:
        less on a narrow rack row, so the knobs keep their room. */
    float sideShare (juce::Rectangle<float> enclosure)
    {
        return enclosure.getWidth() >= 300.0f ? 0.27f : 0.16f;
    }

    bool isPortrait (juce::Rectangle<float> bounds, PedalOrientation orientation)
    {
        switch (orientation)
        {
            case PedalOrientation::upright:   return true;
            case PedalOrientation::onItsSide: return false;
            case PedalOrientation::automatic:
            default:                          return bounds.getWidth() <= bounds.getHeight();
        }
    }
}

//==============================================================================
PedalFaceState PedalFaceState::from (const Pedal& pedal)
{
    PedalFaceState s;
    s.bypassed = pedal.isBypassed();
    s.numKnobs = juce::jmin (pedal.getNumParameters(), Pedal::kMaxParams);

    for (int i = 0; i < s.numKnobs; ++i)
    {
        s.values[(size_t) i] = (float) pedal.getParameterNormalised (i);
        s.labels.add (shortLabel (pedal.getParameterDescriptor (i).name));
    }

    return s;
}

PedalFaceState PedalFaceState::defaultsFor (PedalType type)
{
    if (auto pedal = Pedal::create (type))
    {
        pedal->resetParametersToDefault();
        return from (*pedal);
    }

    return {};
}

KnobCap knobCapFor (PedalType type) noexcept
{
    switch (type)
    {
        case PedalType::EnvelopeFilter:
        case PedalType::Overdrive:      return KnobCap::creamRibbed;
        case PedalType::PitchShifter:
        case PedalType::Delay:
        case PedalType::ParametricEQ:   return KnobCap::chromeSkirt;
        case PedalType::Fuzz:
        case PedalType::Tremolo:        return KnobCap::witchHat;
        case PedalType::Flanger:
        case PedalType::Doubler:        return KnobCap::chromeDome;
        case PedalType::RotarySpeaker:  return KnobCap::chickenHead;
        case PedalType::SpringReverb:   return KnobCap::pointer;
        case PedalType::None:
        case PedalType::Compressor:
        case PedalType::NoiseGate:
        case PedalType::Wah:
        case PedalType::Octaver:
        case PedalType::Distortion:
        case PedalType::Boost:
        case PedalType::VolumePedal:
        case PedalType::Chorus:
        case PedalType::Phaser:
        case PedalType::Reverb:
        case PedalType::GraphicEQ:
        case PedalType::NumTypes:
        default:                        return KnobCap::ribbed;
    }
}

juce::String shortPedalLabel (const juce::String& label)
{
    // Frequencies lose their unit: "100 HZ" -> "100", "1.6 KHZ" -> "1.6K".
    if (label.endsWithIgnoreCase (" kHz"))
        return label.dropLastCharacters (4).trim() + "K";

    if (label.endsWithIgnoreCase (" Hz"))
        return label.dropLastCharacters (3).trim();

    return {};
}

juce::StringArray pedalFaceTexts (PedalType type)
{
    juce::StringArray texts;
    texts.add (juce::String (Pedal::getTypeName (type)).toUpperCase());

    // The footswitch's state, printed by the LED.
    texts.add ("ON");
    texts.add ("BYPASS");
    texts.add ("BYP");

    for (const auto& label : PedalFaceState::defaultsFor (type).labels)
    {
        texts.add (label);

        if (const auto s = shortPedalLabel (label); s.isNotEmpty())
            texts.add (s);
    }

    return texts;
}

//==============================================================================
PedalFaceLayout layoutPedalFace (juce::Rectangle<float> bounds, PedalType type, int numKnobs, PedalOrientation orientation)
{
    const auto look = lookFor (type);
    PedalFaceLayout l;
    l.numKnobs = juce::jlimit (0, Pedal::kMaxParams, numKnobs);
    l.portrait = isPortrait (bounds, orientation);
    l.sliders = type == PedalType::GraphicEQ;

    auto space = bounds.reduced (1.0f);

    if (l.portrait)
    {
        // Room for the jacks on the sides; the box keeps a stompbox's proportions.
        space = space.reduced (juce::jmin (6.0f, space.getWidth() * 0.05f), 0.0f);
        const float w = juce::jmin (space.getWidth(), space.getHeight() * 0.62f);
        const float h = juce::jmin (space.getHeight(), w / 0.58f);
        l.enclosure = space.withSizeKeepingCentre (w, h);
    }
    else
    {
        l.enclosure = space;
    }

    if (look.body == Body::empty)
        return l;

    const float pad = juce::jmin (l.enclosure.getWidth(), l.enclosure.getHeight()) * 0.08f;
    auto inner = l.enclosure.reduced (pad);

    if (look.body == Body::treadle)
    {
        if (l.portrait)
        {
            auto base = inner;
            auto controls = base.removeFromBottom (l.numKnobs > 0 ? base.getHeight() * 0.18f : 0.0f);
            l.treadle = base.reduced (base.getWidth() * 0.04f, 0.0f);
            l.led = juce::Rectangle<float> (pad * 0.8f, pad * 0.8f).withCentre ({ l.enclosure.getRight() - pad * 0.55f, l.enclosure.getY() + pad * 0.55f });
            gridOfKnobs (l, controls, l.numKnobs, 5, false);

            // The toe switch, under the front of the treadle.
            const float toe = juce::jmin (l.treadle.getWidth(), l.treadle.getHeight()) * 0.26f;
            l.footswitch = juce::Rectangle<float> (toe, toe).withCentre ({ l.treadle.getCentreX(), l.treadle.getY() + l.treadle.getHeight() * 0.3f });
        }
        else
        {
            // On its side the controls get two usable knobs across, with their labels.
            auto base = inner;
            const float controlsWidth = juce::jmax (base.getWidth() * 0.22f, juce::jmin (base.getWidth() * 0.5f, minKnobCell * 2.0f));
            auto controls = base.removeFromRight (l.numKnobs > 0 ? controlsWidth : 0.0f);
            l.treadle = base.reduced (0.0f, base.getHeight() * 0.04f);
            l.led = juce::Rectangle<float> (pad * 0.8f, pad * 0.8f).withCentre ({ l.enclosure.getX() + pad * 0.55f, l.enclosure.getY() + pad * 0.55f });
            gridOfKnobs (l, controls, l.numKnobs, 2, true);

            const float toe = juce::jmin (l.treadle.getWidth(), l.treadle.getHeight()) * 0.34f;
            l.footswitch = juce::Rectangle<float> (toe, toe).withCentre ({ l.treadle.getRight() - toe * 0.9f, l.treadle.getCentreY() + toe * 0.25f });
        }

        l.name = l.treadle.withSizeKeepingCentre (l.treadle.getWidth() * 0.8f, juce::jmin (l.treadle.getHeight() * 0.12f, 18.0f))
                          .withY (l.treadle.getY() + l.treadle.getHeight() * 0.06f);
        return l;
    }

    if (l.portrait)
    {
        if (look.body == Body::cabinet)
            inner.removeFromTop (inner.getHeight() * 0.28f);        // louvres

        if (look.body == Body::tank)
        {
            auto strip = inner.removeFromTop (inner.getHeight() * 0.46f);
            l.name = strip.removeFromTop (strip.getHeight() * 0.22f);
            gridOfKnobs (l, strip, l.numKnobs, 3, true);
            l.footswitch = juce::Rectangle<float> (inner.getWidth() * 0.34f, inner.getWidth() * 0.34f)
                               .withCentre ({ inner.getCentreX(), inner.getBottom() - inner.getWidth() * 0.22f });
            l.led = juce::Rectangle<float> (inner.getWidth() * 0.16f, inner.getWidth() * 0.16f)
                        .withCentre ({ inner.getCentreX(), inner.getY() + inner.getHeight() * 0.3f });
            return l;
        }

        auto knobArea = inner.removeFromTop (inner.getHeight() * (look.body == Body::cabinet ? 0.46f : 0.5f));
        l.name = inner.removeFromTop (inner.getHeight() * 0.3f);
        const float fs = juce::jmin (inner.getWidth() * 0.44f, inner.getHeight() * 0.62f);
        l.footswitch = juce::Rectangle<float> (fs, fs).withCentre ({ inner.getCentreX(), inner.getBottom() - fs * 0.62f });
        const float led = juce::jmax (4.0f, fs * 0.22f);
        l.led = juce::Rectangle<float> (led, led).withCentre ({ inner.getCentreX(), inner.getY() + (l.footswitch.getY() - inner.getY()) * 0.45f });

        if (l.sliders)
            gridOfKnobs (l, knobArea, l.numKnobs, 10, false, false);
        else
            gridOfKnobs (l, knobArea, l.numKnobs, l.numKnobs == 4 ? 2 : 3, true);
    }
    else
    {
        auto controls = inner.removeFromRight (juce::jmin (inner.getWidth() * 0.3f, inner.getHeight() * 1.1f));
        inner.removeFromRight (pad * 0.5f);

        if (look.body == Body::cabinet || look.body == Body::tank)   // louvres or grille on the left
            inner.removeFromLeft (juce::jmax (0.0f, l.enclosure.getX() + l.enclosure.getWidth() * sideShare (l.enclosure) - inner.getX()));

        l.name = controls.removeFromTop (controls.getHeight() * 0.3f);
        const float fs = juce::jmin (controls.getWidth() * 0.62f, controls.getHeight() * 0.6f, 56.0f);
        l.footswitch = juce::Rectangle<float> (fs, fs).withCentre ({ controls.getCentreX(), controls.getBottom() - fs * 0.6f });
        const float led = juce::jmax (4.0f, fs * 0.22f);
        l.led = juce::Rectangle<float> (led, led).withCentre ({ controls.getCentreX(), controls.getY() + (l.footswitch.getY() - controls.getY()) * 0.5f });

        // As many rows as keep the knobs usable; the graphic EQ's sliders stay in one bank, labelled underneath.
        const int perRow = l.sliders ? 10 : knobsPerRow (inner.getWidth(), l.numKnobs);
        gridOfKnobs (l, inner, l.numKnobs, perRow, true, ! l.sliders);
    }

    return l;
}

float preferredHeightOnItsSide (float width, PedalType type, int numKnobs)
{
    constexpr float shortest = 96.0f, tallest = 480.0f;

    if (type == PedalType::None || numKnobs <= 0)
        return shortest;

    // The shortest face on which every knob (or slider slot) is usable.
    for (float h = shortest; h < tallest; h += 4.0f)
    {
        const auto l = layoutPedalFace ({ 0.0f, 0.0f, width, h }, type, numKnobs, PedalOrientation::onItsSide);
        bool usable = true;

        for (int i = 0; i < l.numKnobs && usable; ++i)
            usable = l.sliders ? l.knobs[(size_t) i].getHeight() >= usableSliderSlot
                               : l.knobs[(size_t) i].getWidth() >= usableKnob;

        if (usable)
            return h;
    }

    return tallest;
}

//==============================================================================
void paintPedalFace (juce::Graphics& g, juce::Rectangle<float> bounds, PedalType type, const PedalFaceState& state)
{
    const auto look = lookFor (type);
    const auto m = Materials::current();
    const auto& p = Palette::current();
    const auto l = layoutPedalFace (bounds, type, state.numKnobs, state.orientation);
    const float side = sideShare (l.enclosure) - 0.07f;   // the louvres' or cloth's width on its side

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (bounds.toNearestIntEdges());

    const float corner = juce::jlimit (2.0f, 9.0f, juce::jmin (l.enclosure.getWidth(), l.enclosure.getHeight()) * 0.07f);

    // ---- an empty slot ---------------------------------------------------------------
    if (look.body == Body::empty)
    {
        const auto slot = rounded (l.enclosure, corner);
        g.setColour (p.panelSunken.withAlpha (0.7f));
        g.fillPath (slot);

        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, slot, dashes, 2);
        g.setColour (p.edge);
        g.fillPath (dashed);

        drawPrint (g, juce::String (Pedal::getTypeName (type)).toUpperCase(), l.enclosure,
                   p.textDisabled, juce::jlimit (8.0f, 14.0f, l.enclosure.getHeight() * 0.14f), true);
        return;
    }

    // ---- jacks on the sides of an upright box --------------------------------------------
    if (l.portrait && look.body != Body::treadle)
    {
        const float jw = juce::jmin (5.0f, (l.enclosure.getX() - bounds.getX()) - 0.5f);

        if (jw >= 2.0f)
            for (float x : { l.enclosure.getX() - jw, l.enclosure.getRight() })
            {
                const auto tab = juce::Rectangle<float> (x, l.enclosure.getY() + l.enclosure.getHeight() * 0.16f, jw, l.enclosure.getHeight() * 0.07f);
                g.setColour (m.textured ? m.nickel : p.edgeBright);
                g.fillRect (tab);
                g.setColour (m.textured ? juce::Colours::black.withAlpha (0.5f) : p.edge);
                g.drawRect (tab, 0.7f);
            }
    }

    // ---- the enclosure ---------------------------------------------------------------
    const auto box = rounded (l.enclosure, corner);

    if (m.textured)
    {
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillPath (rounded (l.enclosure.translated (0.0f, 1.5f), corner));
    }

    if (look.body == Body::cabinet)
        fillWood (g, box, look.colour, look.seed);
    else if (look.body == Body::tank)
        fillTolex (g, box, look.colour, look.seed);
    else if (look.body == Body::treadle)
        fillBrushedMetal (g, box, look.colour);
    else
        fillFinish (g, box, look);

    g.setColour (m.textured ? juce::Colours::black.withAlpha (0.7f) : p.edgeBright);
    g.strokePath (box, juce::PathStrokeType (1.0f));

    auto surface = look.colour;

    // ---- body features ----------------------------------------------------------------------
    if (look.body == Body::treadle)
    {
        const auto pad = rounded (l.treadle, corner * 0.6f);
        fillGloss (g, pad, m.textured ? m.tolexBlack.brighter (0.05f) : p.panelSunken);

        // The rubber grip: ribs across the treadle.
        if (m.textured)
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (pad);

            const bool across = l.portrait;
            const float step = juce::jmax (3.0f, (across ? l.treadle.getHeight() : l.treadle.getWidth()) / 28.0f);

            for (float t = step; t < (across ? l.treadle.getHeight() : l.treadle.getWidth()); t += step)
            {
                g.setColour (juce::Colours::white.withAlpha (0.07f));

                if (across)
                    g.fillRect (l.treadle.getX(), l.treadle.getY() + t, l.treadle.getWidth(), step * 0.35f);
                else
                    g.fillRect (l.treadle.getX() + t, l.treadle.getY(), step * 0.35f, l.treadle.getHeight());
            }
        }

        g.setColour (m.textured ? juce::Colours::black.withAlpha (0.7f) : p.edgeBright);
        g.strokePath (pad, juce::PathStrokeType (1.0f));
        surface = m.tolexBlack;
    }
    else if (look.body == Body::cabinet)
    {
        // Louvres across the top (upright) or down the left (on its side).
        auto louvres = l.portrait ? l.enclosure.reduced (l.enclosure.getWidth() * 0.1f).withHeight (l.enclosure.getHeight() * 0.26f)
                                  : l.enclosure.reduced (l.enclosure.getHeight() * 0.12f).withWidth (l.enclosure.getWidth() * side);
        g.setColour (m.textured ? juce::Colours::black.withAlpha (0.75f) : p.panelSunken);
        g.fillRoundedRectangle (louvres, 2.0f);

        const int slats = 6;
        for (int i = 1; i < slats; ++i)
        {
            g.setColour (m.textured ? look.colour.brighter (0.2f) : p.edge);

            if (l.portrait)
                g.fillRect (louvres.getX(), louvres.getY() + louvres.getHeight() * (float) i / (float) slats - 1.0f, louvres.getWidth(), 2.0f);
            else
                g.fillRect (louvres.getX() + louvres.getWidth() * (float) i / (float) slats - 1.0f, louvres.getY(), 2.0f, louvres.getHeight());
        }
    }
    else if (look.body == Body::tank)
    {
        // The cloth-covered lower half (upright) or left end (on its side), and a chrome control strip.
        auto cloth = l.portrait ? l.enclosure.reduced (l.enclosure.getWidth() * 0.08f).withTrimmedTop (l.enclosure.getHeight() * 0.53f)
                                : l.enclosure.reduced (l.enclosure.getHeight() * 0.1f).withWidth (l.enclosure.getWidth() * side);
        fillGrille (g, rounded (cloth, 2.0f), Grille::oxbloodStripe, m.grilleOxblood, look.seed);

        auto strip = l.portrait ? l.enclosure.reduced (l.enclosure.getWidth() * 0.06f).withHeight (l.enclosure.getHeight() * 0.47f)
                                : l.enclosure.reduced (l.enclosure.getHeight() * 0.08f).withTrimmedLeft (l.enclosure.getWidth() * (side + 0.04f));
        fillBrushedMetal (g, rounded (strip, 2.0f), m.chrome);
        surface = m.chrome;
    }

    if (look.band && ! l.name.isEmpty())
    {
        g.setColour (look.bandColour);
        g.fillRect (l.name.withX (l.enclosure.getX()).withWidth (l.enclosure.getWidth()).reduced (0.0f, l.name.getHeight() * 0.12f));
    }

    // ---- the name ----------------------------------------------------------------------------
    const auto nameSurface = look.band ? look.bandColour : surface;
    const float nameHeight = juce::jlimit (7.0f, 20.0f, juce::jmin (l.name.getHeight() * 0.62f, l.name.getWidth() * 0.2f));
    drawPrint (g, juce::String (Pedal::getTypeName (type)).toUpperCase(), l.name, inkFor (nameSurface), nameHeight, true);

    // ---- knobs or sliders ------------------------------------------------------------------
    const auto labelInk = inkFor (surface);

    for (int i = 0; i < l.numKnobs; ++i)
    {
        const auto area = l.knobs[(size_t) i];
        const float v = state.values[(size_t) i];

        // The label: whole, or its short form, or hidden - never truncated over its neighbour.
        const auto labelArea = l.labels[(size_t) i].reduced (1.0f, 0.0f);

        if (i < state.labels.size() && ! labelArea.isEmpty())
            drawFittedPrint (g, fitPrint (state.labels[i], shortPedalLabel (state.labels[i]), labelArea,
                                          juce::jmin (10.0f, labelArea.getHeight() * 0.8f), false),
                             labelArea, labelInk, false);

        if (l.sliders)
        {
            if (! state.drawKnobs)
                continue;

            // A slider slot with a cap at the value (0 at the bottom).
            const auto slotRect = area.withSizeKeepingCentre (juce::jmax (2.0f, area.getWidth() * 0.16f), area.getHeight() * 0.86f);
            g.setColour (m.textured ? juce::Colours::black.withAlpha (0.75f) : p.panelSunken);
            g.fillRoundedRectangle (slotRect, 1.0f);

            const float capH = juce::jmax (3.0f, area.getHeight() * 0.1f);
            const float y = slotRect.getBottom() - capH * 0.5f - v * (slotRect.getHeight() - capH);
            const auto capRect = juce::Rectangle<float> (area.getWidth() * 0.6f, capH).withCentre ({ area.getCentreX(), y });

            if (m.textured)
                fillBrushedMetal (g, rounded (capRect, 1.0f), m.chrome);
            else
            {
                g.setColour (p.panelRaised);
                g.fillRect (capRect);
            }

            g.setColour (state.enabled ? Palette::knobPointer : p.textDisabled);
            g.fillRect (capRect.withSizeKeepingCentre (capRect.getWidth(), 1.2f));
            continue;
        }

        if (state.drawKnobs)
            paintKnob (g, area, look.cap, v, state.enabled);
    }

    // ---- LED and footswitch --------------------------------------------------------------------
    // visual-polish.md 2: the LED follows bypass.
    if (! l.led.isEmpty())
    {
        drawLed (g, l.led.getCentre(), l.led.getWidth() * 0.5f, look.led, ! state.bypassed && state.enabled);

        /*  The state in words, by the LED. A lit LED alone did not say which
            way the footswitch was: players "turned every effect on" by
            stepping on pedals that were already on, and bypassed the lot. */
        const float d = l.led.getWidth();
        const juce::String text = state.bypassed ? "BYPASS" : "ON";
        const juce::String shortText = state.bypassed ? "BYP" : "ON";

        auto stateArea = look.body == Body::treadle
                           ? juce::Rectangle<float> (l.led.getRight() + d * 0.4f, l.led.getY() - d * 0.2f, d * 6.0f, d * 1.4f)
                           : juce::Rectangle<float> (d * 7.0f, d * 1.3f).withCentre ({ l.led.getCentreX(), l.led.getBottom() + d * 0.95f });

        stateArea = stateArea.getIntersection (l.enclosure.reduced (1.0f));

        if (! l.footswitch.isEmpty())
            stateArea = stateArea.withBottom (juce::jmin (stateArea.getBottom(), l.footswitch.getY() - 1.0f));

        if (! stateArea.isEmpty())
            drawFittedPrint (g, fitPrint (text, shortText, stateArea, juce::jmin (9.0f, stateArea.getHeight()), false),
                             stateArea, labelInk, false);
    }

    if (state.drawFootswitch && ! l.footswitch.isEmpty())
        drawFootswitch (g, l.footswitch, false);
}

} // namespace luthier::faces
