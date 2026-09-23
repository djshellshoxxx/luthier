#include "AmpFace.h"
#include "FaceMaterials.h"

namespace luthier::faces
{

namespace
{
    enum class Cover { black, tweed, fawn, rust, slate, walnut };
    enum class Plate { black, chrome, gold, brushed, copper, ivory, brass };
    enum class Strip { cover, diamond, wood };
    enum class Pilot { jewel, led };
    enum class LogoStyle { plate, lettering };

    /** A family's look (visual-polish.md 2): what the cabinet, faceplate and hardware are made of. */
    struct Look
    {
        bool combo = false;
        Cover cover = Cover::black;
        Plate plate = Plate::black;
        bool grille = false;
        Grille grilleStyle = Grille::blackMesh;
        Strip strip = Strip::cover;
        KnobCap cap = KnobCap::bell;
        Pilot pilot = Pilot::jewel;
        LogoStyle logo = LogoStyle::plate;
        bool piping = false;
        bool rockers = false;
        bool chromeDrip = false;     ///< a bright strip along the top of a black faceplate
        bool plateStripe = false;    ///< a dark band across a metal faceplate
        int tubes = 0;
        int channelLeds = 0;
        juce::uint32 seed = 1;
    };

    Look lookFor (AmpModel model)
    {
        Look l;
        l.seed = 0x1000u + (juce::uint32) model * 7919u;

        switch (model)
        {
            case AmpModel::FenderTwin:
            case AmpModel::FenderDeluxe:
                // Black-panel: black Tolex, silver-sparkle cloth, black panel under a bright drip edge.
                l.combo = true; l.cover = Cover::black; l.plate = Plate::black; l.chromeDrip = true;
                l.grille = true; l.grilleStyle = Grille::silverSparkle;
                l.cap = KnobCap::skirtedNumbers; l.pilot = Pilot::jewel;
                break;

            case AmpModel::FenderTweed:
            case AmpModel::FenderChamp:
                // Tweed: lacquered twill, oxblood cloth with pale stripes, a chrome control plate.
                l.combo = true; l.cover = Cover::tweed; l.plate = Plate::chrome;
                l.grille = true; l.grilleStyle = Grille::oxbloodStripe;
                l.cap = KnobCap::pointer; l.pilot = Pilot::jewel;
                break;

            case AmpModel::MarshallPlexi:
            case AmpModel::MarshallJCM800:
                // British stack head: black levant, piped gold panel, gold-cap knobs, rockers.
                l.cover = Cover::black; l.plate = Plate::gold; l.piping = true; l.rockers = true;
                l.cap = KnobCap::goldCap; l.pilot = Pilot::jewel; l.logo = LogoStyle::lettering;
                l.tubes = 4;
                l.plateStripe = model == AmpModel::MarshallJCM800;
                break;

            case AmpModel::VoxAC30:
                // British chime combo: fawn covering, diamond cloth, copper top panel, chicken-heads.
                l.combo = true; l.cover = Cover::fawn; l.plate = Plate::copper; l.piping = true;
                l.grille = true; l.grilleStyle = Grille::diamond;
                l.cap = KnobCap::chickenHead; l.pilot = Pilot::jewel;
                break;

            case AmpModel::MesaRectifier:
                // Modern high gain: black, a checker-plate front strip, black panel, LEDs.
                l.cover = Cover::black; l.plate = Plate::black; l.strip = Strip::diamond;
                l.cap = KnobCap::chromeDome; l.pilot = Pilot::led; l.channelLeds = 2;
                l.tubes = 4;
                break;

            case AmpModel::BognerEcstasy:
                // Boutique: slate covering, polished plate, speed knobs.
                l.cover = Cover::slate; l.plate = Plate::brushed; l.piping = true;
                l.cap = KnobCap::speed; l.pilot = Pilot::led; l.channelLeds = 3;
                l.tubes = 4;
                break;

            case AmpModel::DiezelVH4:
                // Four-channel modern metal: black, brushed aluminium plate, a row of channel LEDs.
                l.cover = Cover::black; l.plate = Plate::brushed;
                l.cap = KnobCap::chromeDome; l.pilot = Pilot::led; l.channelLeds = 4;
                l.tubes = 4;
                break;

            case AmpModel::OrangeOR120:
                // British crunch: warm rust covering, ivory panel, black chicken-heads, white piping.
                l.cover = Cover::rust; l.plate = Plate::ivory; l.piping = true;
                l.cap = KnobCap::chickenHeadBlack; l.pilot = Pilot::jewel; l.logo = LogoStyle::lettering;
                l.tubes = 4;
                break;

            case AmpModel::AmpegSVT:
                // Bass head: black, brushed plate with a dark band, chrome-skirted knobs, six valves.
                l.cover = Cover::black; l.plate = Plate::brushed; l.plateStripe = true; l.rockers = true;
                l.cap = KnobCap::chromeSkirt; l.pilot = Pilot::jewel;
                l.tubes = 6;
                break;

            case AmpModel::AcousticDI:
                // A preamp box: walnut cheeks, an ivory panel, soft modern knobs.
                l.cover = Cover::walnut; l.plate = Plate::ivory; l.strip = Strip::wood;
                l.cap = KnobCap::softTouch; l.pilot = Pilot::led;
                break;

            case AmpModel::Custom:
            case AmpModel::NumModels:
            default:
                // The workbench amp: walnut and brass, the standard bell knobs.
                l.cover = Cover::walnut; l.plate = Plate::brass; l.strip = Strip::wood; l.piping = true;
                l.cap = KnobCap::bell; l.pilot = Pilot::jewel;
                l.tubes = 3;
                break;
        }

        return l;
    }

    juce::Colour coverColour (const Materials& m, Cover c)
    {
        switch (c)
        {
            case Cover::tweed:  return m.tweed;
            case Cover::fawn:   return m.fawn;
            case Cover::rust:   return m.rust;
            case Cover::slate:  return m.slate;
            case Cover::walnut: return m.walnut;
            case Cover::black:
            default:            return m.tolexBlack;
        }
    }

    juce::Colour plateColour (const Materials& m, Plate p)
    {
        switch (p)
        {
            case Plate::chrome:  return m.chrome;
            case Plate::gold:    return m.brass;
            case Plate::brushed: return m.aluminium;
            case Plate::copper:  return m.copper;
            case Plate::ivory:   return m.ivoryPanel;
            case Plate::brass:   return m.brass;
            case Plate::black:
            default:             return m.blackPanel;
        }
    }

    juce::Colour grilleColour (const Materials& m, Grille g)
    {
        switch (g)
        {
            case Grille::silverSparkle: return m.grilleSilver;
            case Grille::oxbloodStripe: return m.grilleOxblood;
            case Grille::diamond:       return m.grilleBrown;
            case Grille::basket:        return m.grilleBlack;
            case Grille::blackMesh:
            default:                    return m.grilleBlack;
        }
    }

    const char* const knobLabels[numAmpKnobs] = { "GAIN", "BASS", "MIDDLE", "TREBLE", "PRESENCE", "MASTER" };
    const char* const switchLabels[numAmpSwitches] = { "BRIGHT", "MID", "STANDBY" };

    juce::Path rounded (juce::Rectangle<float> r, float corner)
    {
        juce::Path p;
        p.addRoundedRectangle (r, corner);
        return p;
    }

    void fillCover (juce::Graphics& g, const juce::Path& shape, const Look& look, const Materials& m)
    {
        const auto colour = coverColour (m, look.cover);

        switch (look.cover)
        {
            case Cover::tweed:  fillTweed (g, shape, colour); break;
            case Cover::walnut: fillWood (g, shape, colour, look.seed); break;
            case Cover::black:
            case Cover::fawn:
            case Cover::rust:
            case Cover::slate:
            default:            fillTolex (g, shape, colour, look.seed); break;
        }
    }

    void fillPlate (juce::Graphics& g, const juce::Path& shape, const Look& look, const Materials& m)
    {
        const auto colour = plateColour (m, look.plate);

        switch (look.plate)
        {
            case Plate::chrome:
            case Plate::gold:
            case Plate::brushed:
            case Plate::copper:
            case Plate::brass:  fillBrushedMetal (g, shape, colour); break;
            case Plate::ivory:
            case Plate::black:
            default:            fillGloss (g, shape, colour); break;
        }
    }

    juce::String displayName (AmpModel model)
    {
        return juce::String (AmpEngine::getModelName (model)).toUpperCase();
    }
}

//==============================================================================
KnobCap knobCapFor (AmpModel model) noexcept
{
    return lookFor (model).cap;
}

juce::StringArray ampFaceTexts (AmpModel model)
{
    juce::StringArray texts;
    texts.add (displayName (model));

    for (auto* s : knobLabels)
        texts.add (s);

    for (auto* s : switchLabels)
        texts.add (s);

    texts.add ("INPUT");
    return texts;
}

AmpFaceLayout layoutAmpFace (juce::Rectangle<float> bounds, AmpModel model)
{
    const auto look = lookFor (model);
    AmpFaceLayout l;
    l.combo = look.combo;

    l.cabinet = bounds.reduced (1.0f);
    const float border = juce::jlimit (3.0f, 14.0f, juce::jmin (l.cabinet.getWidth(), l.cabinet.getHeight()) * 0.07f);
    auto inner = l.cabinet.reduced (border);

    bool logoOnPlate = false;

    if (look.combo)
    {
        if (inner.getHeight() > inner.getWidth() * 0.4f)
        {
            l.faceplate = inner.removeFromTop (juce::jmax (34.0f, inner.getHeight() * 0.44f));
            inner.removeFromTop (border * 0.6f);
            l.grille = inner;

            l.logo = { l.grille.getX() + l.grille.getWidth() * 0.05f, l.grille.getY() + l.grille.getHeight() * 0.1f,
                       l.grille.getWidth() * 0.26f, juce::jmax (8.0f, l.grille.getHeight() * 0.2f) };
        }
        else
        {
            l.faceplate = inner;
            logoOnPlate = true;
        }
    }
    else
    {
        if (inner.getHeight() > inner.getWidth() * 0.3f)
        {
            auto strip = inner.removeFromTop (inner.getHeight() * 0.32f);
            inner.removeFromTop (border * 0.4f);
            l.faceplate = inner;

            l.logo = strip.withSizeKeepingCentre (strip.getWidth() * 0.5f, strip.getHeight() * 0.62f)
                          .withX (strip.getX() + strip.getWidth() * 0.06f);

            if (look.tubes > 0)
                l.vent = strip.withSizeKeepingCentre (strip.getWidth() * 0.28f, strip.getHeight() * 0.66f)
                              .withX (strip.getRight() - strip.getWidth() * 0.34f);
        }
        else
        {
            l.faceplate = inner;
            logoOnPlate = true;
        }
    }

    // The faceplate, left to right: inputs (and the name if it lives here), six knobs, switches and pilot.
    auto area = l.faceplate.reduced (l.faceplate.getHeight() * 0.06f, l.faceplate.getHeight() * 0.06f);
    auto left = area.removeFromLeft (area.getWidth() * (logoOnPlate ? 0.2f : 0.09f));
    auto right = area.removeFromRight (area.getWidth() * 0.2f);

    if (logoOnPlate)
    {
        l.logo = left.removeFromTop (left.getHeight() * 0.56f).reduced (1.0f);
        left.removeFromRight (left.getWidth() * 0.4f);
    }

    const float jack = juce::jmin (left.getWidth() * 0.3f, left.getHeight() * 0.16f);
    const auto jackCentre = left.getCentre();
    l.inputs[0] = juce::Rectangle<float> (jack * 2.0f, jack * 2.0f).withCentre (jackCentre.translated (0.0f, -jack * 1.2f));
    l.inputs[1] = juce::Rectangle<float> (jack * 2.0f, jack * 2.0f).withCentre (jackCentre.translated (0.0f, jack * 1.2f));

    const float slot = area.getWidth() / (float) numAmpKnobs;
    const float knobZone = area.getHeight() * 0.74f;
    const float knob = juce::jmin (slot * 0.94f, knobZone);

    for (int i = 0; i < numAmpKnobs; ++i)
    {
        auto column = juce::Rectangle<float> (area.getX() + slot * (float) i, area.getY(), slot, area.getHeight());
        l.knobs[(size_t) i] = juce::Rectangle<float> (knob, knob).withCentre ({ column.getCentreX(), area.getY() + knobZone * 0.5f });
        l.labels[(size_t) i] = column.withTrimmedTop (knobZone).reduced (1.0f, 0.0f);
    }

    // Two small switches stacked, then standby and the pilot.
    auto switchesArea = right.removeFromLeft (right.getWidth() * 0.5f);
    const float sw = juce::jmin (switchesArea.getWidth() * 0.8f, switchesArea.getHeight() * 0.34f);
    l.switches[brightSwitch]   = switchesArea.removeFromTop (switchesArea.getHeight() * 0.5f).withSizeKeepingCentre (sw, sw);
    l.switches[midBoostSwitch] = switchesArea.withSizeKeepingCentre (sw, sw);

    const float st = juce::jmin (right.getWidth() * 0.8f, right.getHeight() * 0.34f);
    l.switches[standbySwitch] = right.removeFromBottom (right.getHeight() * 0.5f).withSizeKeepingCentre (st, st);
    const float pilot = juce::jmin (right.getWidth(), right.getHeight()) * 0.62f;
    l.pilot = right.withSizeKeepingCentre (pilot, pilot);

    return l;
}

//==============================================================================
void paintAmpFace (juce::Graphics& g, juce::Rectangle<float> bounds, AmpModel model, const AmpFaceState& state)
{
    const auto look = lookFor (model);
    const auto m = Materials::current();
    const auto& p = Palette::current();
    const auto l = layoutAmpFace (bounds, model);

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (bounds.toNearestIntEdges());

    const float corner = juce::jlimit (2.0f, 10.0f, juce::jmin (l.cabinet.getWidth(), l.cabinet.getHeight()) * 0.05f);

    // ---- the cabinet ---------------------------------------------------------
    const auto cabinet = rounded (l.cabinet, corner);
    fillCover (g, cabinet, look, m);

    // Strip treatments on heads: checker plate or a wood front.
    if (! look.combo && ! l.logo.isEmpty() && l.faceplate.getY() > l.logo.getBottom())
    {
        const auto strip = juce::Rectangle<float> (l.faceplate.getX(), l.cabinet.getY() + (l.faceplate.getX() - l.cabinet.getX()),
                                                   l.faceplate.getWidth(),
                                                   l.faceplate.getY() - l.cabinet.getY() - (l.faceplate.getX() - l.cabinet.getX()) * 1.4f);

        if (look.strip == Strip::diamond && strip.getHeight() > 4.0f)
            fillDiamondPlate (g, rounded (strip, 2.0f), m.aluminium);
    }

    if (m.textured)
    {
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.strokePath (cabinet, juce::PathStrokeType (1.2f));

        const float cap = juce::jlimit (5.0f, 16.0f, corner * 1.6f);
        for (int i = 0; i < 4; ++i)
            drawCornerCap (g, l.cabinet, i, cap);
    }
    else
    {
        g.setColour (p.edgeBright);
        g.strokePath (cabinet, juce::PathStrokeType (1.0f));
    }

    // ---- the grille ------------------------------------------------------------
    if (! l.grille.isEmpty())
    {
        const auto cloth = rounded (l.grille, 2.0f);
        fillGrille (g, cloth, look.grilleStyle, grilleColour (m, look.grilleStyle), look.seed + 11u);

        if (look.piping)
        {
            g.setColour (m.brass.withAlpha (m.textured ? 0.9f : 1.0f));
            g.strokePath (cloth, juce::PathStrokeType (1.4f));
        }
    }

    // ---- the faceplate -----------------------------------------------------------
    const auto plate = rounded (l.faceplate, 2.0f);
    fillPlate (g, plate, look, m);
    const auto plateBase = plateColour (m, look.plate);

    if (look.plateStripe)
    {
        auto band = l.faceplate.withHeight (juce::jmax (2.0f, l.faceplate.getHeight() * 0.12f));
        g.setColour (m.textured ? m.slate.darker (0.4f) : p.edge);
        g.fillRect (band);
    }

    if (look.chromeDrip)
    {
        auto drip = l.faceplate.withHeight (juce::jmax (1.5f, l.faceplate.getHeight() * 0.07f));
        fillBrushedMetal (g, rounded (drip, 1.0f), m.chrome);
    }

    g.setColour (m.textured ? juce::Colours::black.withAlpha (0.6f) : p.edgeBright);
    g.strokePath (plate, juce::PathStrokeType (1.0f));

    if (look.piping)
    {
        g.setColour (m.piping);
        g.strokePath (rounded (l.faceplate.expanded (juce::jmin (3.0f, corner * 0.5f)), 3.0f), juce::PathStrokeType (1.3f));
    }

    const auto ink = inkFor (plateBase);

    // ---- the name ---------------------------------------------------------------------
    if (! l.logo.isEmpty())
    {
        const auto name = displayName (model);
        const bool onCover = l.logo.getY() < l.faceplate.getY() || ! l.grille.isEmpty();

        if (look.logo == LogoStyle::lettering && onCover && l.grille.isEmpty())
        {
            // Bold cream lettering straight on the covering.
            drawPrint (g, name, l.logo, inkFor (coverColour (m, look.cover)), l.logo.getHeight() * 0.72f, true,
                       juce::Justification::centredLeft);
        }
        else
        {
            // A small metal name plate with engraved lettering.
            const auto badge = rounded (l.logo, juce::jmin (3.0f, l.logo.getHeight() * 0.2f));
            const auto badgeColour = look.plate == Plate::black || look.plate == Plate::ivory ? m.chrome : plateBase.brighter (0.15f);
            fillBrushedMetal (g, badge, badgeColour);
            g.setColour (m.textured ? juce::Colours::black.withAlpha (0.55f) : p.edgeBright);
            g.strokePath (badge, juce::PathStrokeType (0.8f));
            // A name plate squeezed onto a small faceplate takes two lines.
            const bool twoLines = l.logo.getWidth() < l.logo.getHeight() * 4.0f;
            drawPrint (g, name, l.logo.reduced (juce::jmin (4.0f, l.logo.getHeight() * 0.15f), 1.0f), inkFor (badgeColour),
                       l.logo.getHeight() * (twoLines ? 0.34f : 0.62f), true, juce::Justification::centred, twoLines ? 2 : 1);
        }
    }

    // ---- the valves -----------------------------------------------------------------
    if (! l.vent.isEmpty())
        drawTubeVent (g, l.vent, look.tubes, state.drive, state.standby, state.driveStale);

    // ---- inputs -----------------------------------------------------------------------
    for (auto& in : l.inputs)
        if (in.getWidth() >= 4.0f)
            drawJack (g, in.getCentre(), in.getWidth() * 0.5f);

    const float labelHeight = juce::jlimit (0.0f, 11.0f, l.labels[0].getHeight() * 0.62f);

    if (labelHeight >= 6.0f && l.inputs[0].getWidth() >= 6.0f)
        drawPrint (g, "INPUT", l.inputs[1].translated (0.0f, l.inputs[1].getHeight() * 0.8f).expanded (6.0f, 0.0f),
                   ink, labelHeight * 0.8f, false);

    // ---- knobs and their labels -----------------------------------------------------
    for (int i = 0; i < numAmpKnobs; ++i)
    {
        if (state.drawKnobs)
            paintKnob (g, l.knobs[(size_t) i], look.cap, state.knobs[(size_t) i], state.enabled);

        if (labelHeight >= 6.0f)
            drawPrint (g, knobLabels[i], l.labels[(size_t) i], ink, labelHeight, false);
    }

    // ---- switches and pilot -------------------------------------------------------------
    const bool switchValues[numAmpSwitches] = { state.bright, state.midBoost, ! state.standby };

    if (state.drawSwitches)
    {
        for (int i = 0; i < numAmpSwitches; ++i)
        {
            const auto area = l.switches[(size_t) i];

            if (area.getWidth() < 4.0f)
                continue;

            if (look.rockers && i == standbySwitch)
                drawRocker (g, area, switchValues[i], m.jewelRed);
            else
                LuthierLookAndFeel::drawMiniToggle (g, area, switchValues[i], state.enabled);
        }
    }

    if (labelHeight >= 6.0f)
        for (int i = 0; i < numAmpSwitches; ++i)
            drawPrint (g, switchLabels[i], l.switches[(size_t) i].withY (l.switches[(size_t) i].getBottom() + 1.0f)
                                                              .expanded (10.0f, 0.0f).withHeight (labelHeight + 1.0f),
                       ink, labelHeight * 0.72f, false);

    // visual-polish.md 2: the pilot light follows Standby.
    const bool lit = ! state.standby && state.enabled;
    const auto pilotColour = look.pilot == Pilot::led ? (model == AmpModel::AcousticDI ? m.ledGreen : m.ledCool) : m.jewelRed;

    if (look.pilot == Pilot::jewel)
        drawJewel (g, l.pilot.getCentre(), l.pilot.getWidth() * 0.5f, pilotColour, lit);
    else
        drawLed (g, l.pilot.getCentre(), l.pilot.getWidth() * 0.3f, pilotColour, lit);

    // Channel LEDs on the modern heads, one lit.
    if (look.channelLeds > 0 && l.pilot.getWidth() >= 6.0f)
    {
        const float r = juce::jmax (1.5f, l.pilot.getWidth() * 0.12f);
        const auto row = l.pilot.translated (0.0f, -l.pilot.getHeight() * 0.95f);

        for (int i = 0; i < look.channelLeds; ++i)
        {
            const float x = row.getX() + row.getWidth() * ((float) i + 0.5f) / (float) look.channelLeds;
            drawLed (g, { x, row.getCentreY() }, r, m.ledAmber, lit && i == juce::jmin (look.channelLeds - 1, 1));
        }
    }

    // A few panel screws, where there is room.
    if (l.faceplate.getHeight() > 30.0f)
    {
        const float r = juce::jlimit (1.5f, 2.6f, l.faceplate.getHeight() * 0.03f);
        const float inset = r * 2.2f;
        drawScrew (g, { l.faceplate.getX() + inset, l.faceplate.getY() + inset + (look.chromeDrip ? l.faceplate.getHeight() * 0.07f : 0.0f) }, r);
        drawScrew (g, { l.faceplate.getRight() - inset, l.faceplate.getY() + inset + (look.chromeDrip ? l.faceplate.getHeight() * 0.07f : 0.0f) }, r);
        drawScrew (g, { l.faceplate.getX() + inset, l.faceplate.getBottom() - inset }, r);
        drawScrew (g, { l.faceplate.getRight() - inset, l.faceplate.getBottom() - inset }, r);
    }
}

} // namespace luthier::faces
