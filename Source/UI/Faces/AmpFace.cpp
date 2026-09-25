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

    // Full and short forms: the short ones are what a small face prints rather than truncate.
    const char* const knobLabels[numAmpKnobs]      = { "GAIN", "BASS", "MIDDLE", "TREBLE", "PRESENCE", "MASTER" };
    const char* const knobShortLabels[numAmpKnobs] = { "GAIN", "BASS", "MID", "TREB", "PRES", "MSTR" };
    const char* const switchLabels[numAmpSwitches]      = { "BRIGHT", "MID", "STANDBY" };
    const char* const switchShortLabels[numAmpSwitches] = { "BRT", "MID", "STBY" };

    constexpr float knobLabelMax = 11.0f, switchLabelMax = 8.5f, inputLabelMax = 8.0f;

    /** Fits a row of labels at one height: the full forms while they print at a
        comfortable size, the short ones when those print larger, else none. */
    template <size_t N>
    std::array<FittedPrint, N> fitRow (const std::array<juce::Rectangle<float>, N>& areas,
                                       const char* const (&full)[N], const char* const (&shortForms)[N], float maxHeight)
    {
        // The height every label of a row fits at, or 0 when one of them fits at none.
        auto sharedHeight = [&areas, maxHeight] (const char* const* forms)
        {
            float shared = maxHeight;
            bool any = false;

            for (size_t i = 0; i < N; ++i)
            {
                if (areas[i].isEmpty())
                    continue;

                any = true;
                shared = juce::jmin (shared, fitPrintHeight (forms[i], areas[i], maxHeight, false));
            }

            return any ? shared : 0.0f;
        };

        const float fullHeight = sharedHeight (full);
        const float shortHeight = sharedHeight (shortForms);

        const char* const* forms = nullptr;
        float height = 0.0f;

        if (fullHeight > 0.0f && (fullHeight >= maxHeight * 0.8f || fullHeight >= shortHeight))
        {
            forms = full;
            height = fullHeight;
        }
        else if (shortHeight > 0.0f)
        {
            forms = shortForms;
            height = shortHeight;
        }
        else
        {
            return {};
        }

        std::array<FittedPrint, N> row;

        for (size_t i = 0; i < N; ++i)
            if (! areas[i].isEmpty())
                row[i] = { forms[i], height };

        return row;
    }

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

bool standbyIsRocker (AmpModel model) noexcept
{
    return lookFor (model).rockers;
}

juce::String ampKnobLabel (AmpKnob knob, bool shortForm)
{
    if (! juce::isPositiveAndBelow ((int) knob, (int) numAmpKnobs))
        return {};

    return shortForm ? knobShortLabels[knob] : knobLabels[knob];
}

juce::String ampSwitchLabel (AmpSwitch sw, bool shortForm)
{
    if (! juce::isPositiveAndBelow ((int) sw, (int) numAmpSwitches))
        return {};

    return shortForm ? switchShortLabels[sw] : switchLabels[sw];
}

juce::StringArray ampFaceTexts (AmpModel model)
{
    juce::StringArray texts;
    texts.add (displayName (model));
    texts.add ("VU");

    for (auto* s : knobLabels)
        texts.add (s);

    for (auto* s : knobShortLabels)
        texts.addIfNotAlreadyThere (s);

    for (auto* s : switchLabels)
        texts.add (s);

    for (auto* s : switchShortLabels)
        texts.addIfNotAlreadyThere (s);

    texts.add ("INPUT");
    texts.add ("IN");
    return texts;
}

AmpFaceLabels fitAmpFaceLabels (const AmpFaceLayout& l)
{
    AmpFaceLabels fitted;
    fitted.knobs = fitRow (l.labels, knobLabels, knobShortLabels, knobLabelMax);

    if (l.hasSwitches)
        fitted.switches = fitRow (l.switchLabels, switchLabels, switchShortLabels, switchLabelMax);

    if (! l.inputLabel.isEmpty())
        fitted.input = fitPrint ("INPUT", "IN", l.inputLabel, inputLabelMax, false);

    return fitted;
}

AmpFaceLayout layoutAmpFace (juce::Rectangle<float> bounds, AmpModel model, bool withSwitches)
{
    const auto look = lookFor (model);
    AmpFaceLayout l;
    l.combo = look.combo;
    l.hasSwitches = withSwitches;

    l.cabinet = bounds.reduced (1.0f);
    const float border = juce::jlimit (3.0f, 14.0f, juce::jmin (l.cabinet.getWidth(), l.cabinet.getHeight()) * 0.07f);
    auto inner = l.cabinet.reduced (border);

    // A narrow column (the Advanced AMP section) takes the knobs in two rows of
    // three; anything wider keeps them in one row, as on the amp. A short card
    // has little room above its faceplate.
    l.knobRows = inner.getHeight() > inner.getWidth() * 0.62f ? 2 : 1;
    const bool tall = l.knobRows == 2;
    const bool shortFace = ! tall && inner.getHeight() < 110.0f;

    bool logoOnPlate = false;
    juce::Rectangle<float> strip;   // a head's covering above its faceplate

    if (look.combo)
    {
        if (tall || inner.getHeight() > inner.getWidth() * 0.4f)
        {
            l.faceplate = inner.removeFromTop (juce::jmax (34.0f, inner.getHeight() * (tall ? 0.66f : 0.44f)));
            inner.removeFromTop (border * 0.6f);
            l.grille = inner;

            l.logo = { l.grille.getX() + l.grille.getWidth() * 0.05f, l.grille.getY() + l.grille.getHeight() * 0.1f,
                       l.grille.getWidth() * (tall ? 0.5f : 0.26f), juce::jmax (8.0f, l.grille.getHeight() * (tall ? 0.28f : 0.2f)) };

            // visual-polish.md 4: the VU meter at the grille's top right, opposite the name.
            if (l.grille.getHeight() >= 28.0f && l.grille.getWidth() >= 120.0f)
            {
                const float meterW = juce::jmin (l.grille.getWidth() * 0.26f, 64.0f);
                const float meterH = juce::jmin (l.grille.getHeight() * 0.55f, meterW * 0.62f);
                l.meter = { l.grille.getRight() - l.grille.getWidth() * 0.05f - meterW,
                            l.grille.getY() + l.grille.getHeight() * 0.1f, meterW, meterH };
            }
        }
        else
        {
            l.faceplate = inner;
            logoOnPlate = true;
        }
    }
    else
    {
        if (tall || inner.getHeight() > inner.getWidth() * 0.3f)
        {
            strip = inner.removeFromTop (inner.getHeight() * (tall ? 0.2f : shortFace ? 0.26f : 0.32f));
            inner.removeFromTop (border * 0.4f);
            l.faceplate = inner;

            l.logo = strip.withSizeKeepingCentre (strip.getWidth() * 0.5f, strip.getHeight() * 0.62f)
                          .withX (strip.getX() + strip.getWidth() * 0.06f);

            if (look.tubes > 0)
                l.vent = strip.withSizeKeepingCentre (strip.getWidth() * 0.28f, strip.getHeight() * 0.66f)
                              .withX (strip.getRight() - strip.getWidth() * 0.34f);

            // visual-polish.md 4: the VU meter on the covering between the name and
            // the vent, where the strip has room; the name gives up some width for it.
            if (strip.getWidth() >= 150.0f && strip.getHeight() >= 18.0f)
            {
                l.logo = l.logo.withWidth (strip.getWidth() * 0.36f);
                const float meterW = strip.getWidth() * (look.tubes > 0 ? 0.2f : 0.24f);
                const float meterH = juce::jmin (strip.getHeight() * 0.84f, meterW * 0.62f);
                l.meter = juce::Rectangle<float> (meterW, meterH)
                              .withCentre ({ strip.getX() + strip.getWidth() * (look.tubes > 0 ? 0.53f : 0.62f), strip.getCentreY() });
            }
        }
        else
        {
            l.faceplate = inner;
            logoOnPlate = true;
        }
    }

    const float pad = l.faceplate.getHeight() * 0.06f;
    auto area = l.faceplate.reduced (pad);

    // ---- the name on a faceplate with nowhere else for it: a band across the top, with the inputs
    juce::Rectangle<float> band;

    if (logoOnPlate)
    {
        band = area.removeFromTop (juce::jmax (10.0f, area.getHeight() * 0.24f));
        area.removeFromTop (pad * 0.5f);
        l.logo = band.withWidth (band.getWidth() * 0.42f);

        const float jack = band.getHeight() * 0.34f;
        const auto first = juce::Rectangle<float> (jack * 2.0f, jack * 2.0f)
                               .withCentre ({ l.logo.getRight() + jack * 2.4f, band.getCentreY() });

        l.inputs[0] = first;
        l.inputs[1] = first.translated (jack * 2.6f, 0.0f);
    }

    // ---- the inputs down the left of a wide faceplate, their label under them
    if (! tall && ! logoOnPlate)
    {
        auto left = area.removeFromLeft (area.getWidth() * 0.09f);
        const float jack = juce::jmin (left.getWidth() * 0.3f, left.getHeight() * 0.16f);
        const auto jackCentre = left.getCentre().translated (0.0f, -jack * 0.5f);

        l.inputs[0] = juce::Rectangle<float> (jack * 2.0f, jack * 2.0f).withCentre (jackCentre.translated (0.0f, -jack * 1.2f));
        l.inputs[1] = juce::Rectangle<float> (jack * 2.0f, jack * 2.0f).withCentre (jackCentre.translated (0.0f, jack * 1.2f));
        l.inputLabel = left.withTop (juce::jmin (left.getBottom(), l.inputs[1].getBottom() + 1.0f));
    }

    // ---- the switch column on the right (or just the pilot's, when the face has no switches)
    juce::Rectangle<float> right;

    if (withSwitches)
        right = area.removeFromRight (area.getWidth() * (tall ? 0.28f : 0.2f));
    else if (band.isEmpty() && strip.isEmpty())
        right = area.removeFromRight (area.getWidth() * 0.08f);

    // ---- the knobs: one row, or two rows of three, each knob over its label
    const int cols = numAmpKnobs / l.knobRows;
    const float rowH = area.getHeight() / (float) l.knobRows;
    const float slot = area.getWidth() / (float) cols;
    const float labelH = juce::jmin (20.0f, rowH * 0.26f);
    const float knobZone = rowH - labelH;
    const float knob = juce::jmax (0.0f, juce::jmin (slot * 0.94f, knobZone));

    for (int i = 0; i < numAmpKnobs; ++i)
    {
        const auto cell = juce::Rectangle<float> (area.getX() + slot * (float) (i % cols), area.getY() + rowH * (float) (i / cols),
                                                  slot, rowH);
        l.knobs[(size_t) i] = juce::Rectangle<float> (knob, knob).withCentre ({ cell.getCentreX(), cell.getY() + knobZone * 0.5f });
        l.labels[(size_t) i] = cell.withTrimmedTop (knobZone).reduced (1.0f, 0.0f);
    }

    // ---- switches and the pilot
    auto placeSwitch = [&l] (AmpSwitch s, juce::Rectangle<float> cell)
    {
        const float labelHeight = juce::jmin (11.0f, cell.getHeight() * 0.32f);
        const auto zone = cell.withTrimmedBottom (labelHeight);
        const float size = juce::jmax (0.0f, juce::jmin (30.0f, cell.getWidth() * 0.8f, zone.getHeight() - 1.0f));

        l.switches[(size_t) s] = zone.withSizeKeepingCentre (size, size);
        l.switchLabels[(size_t) s] = cell.withTop (zone.getBottom()).reduced (0.5f, 0.0f);
    };

    auto placePilot = [&l, &look] (juce::Rectangle<float> cell)
    {
        // The modern heads' channel lamps sit in a row above the pilot.
        if (look.channelLeds > 0 && cell.getHeight() > 16.0f)
            l.channelLeds = cell.removeFromTop (cell.getHeight() * 0.3f);

        const float size = juce::jmin (cell.getWidth(), cell.getHeight()) * 0.62f;
        l.pilot = cell.withSizeKeepingCentre (size, size);
    };

    if (withSwitches)
    {
        if (tall)
        {
            // Down the right-hand column: the pilot, then bright, mid boost and standby.
            placePilot (right.removeFromTop (right.getHeight() * 0.28f));
            const float cellH = right.getHeight() / 3.0f;
            placeSwitch (brightSwitch, right.removeFromTop (cellH));
            placeSwitch (midBoostSwitch, right.removeFromTop (cellH));
            placeSwitch (standbySwitch, right);
        }
        else
        {
            // Bright over mid boost, and beside them the pilot over standby.
            auto pair = right.removeFromLeft (right.getWidth() * 0.5f);
            placeSwitch (brightSwitch, pair.removeFromTop (pair.getHeight() * 0.5f));
            placeSwitch (midBoostSwitch, pair);
            placeSwitch (standbySwitch, right.removeFromBottom (right.getHeight() * 0.5f));
            placePilot (right);
        }
    }
    else if (! band.isEmpty())
    {
        // At the far end of the name band, the channel lamps just before it.
        const float size = band.getHeight() * 0.8f;
        l.pilot = juce::Rectangle<float> (size, size).withCentre ({ band.getRight() - size * 0.6f, band.getCentreY() });

        if (look.channelLeds > 0)
        {
            const float w = (float) look.channelLeds * band.getHeight() * 0.5f;
            l.channelLeds = band.withX (l.pilot.getX() - w - size * 0.3f).withWidth (w);
        }
    }
    else if (! strip.isEmpty())
    {
        // On the covering, past the vent.
        const float size = juce::jmin (strip.getWidth() * 0.045f, strip.getHeight() * 0.5f);
        l.pilot = juce::Rectangle<float> (size, size).withCentre ({ strip.getRight() - strip.getWidth() * 0.03f, strip.getCentreY() });
    }
    else
    {
        placePilot (right);
    }

    return l;
}

//==============================================================================
void paintAmpFace (juce::Graphics& g, juce::Rectangle<float> bounds, AmpModel model, const AmpFaceState& state)
{
    const auto look = lookFor (model);
    const auto m = Materials::current();
    const auto& p = Palette::current();
    const auto l = layoutAmpFace (bounds, model, state.hasSwitches);

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
    if (state.drawValves && ! l.vent.isEmpty())
        drawTubeVent (g, l.vent, look.tubes, state.drive, state.standby, state.driveStale);

    // ---- the VU meter (visual-polish.md 4) --------------------------------------------
    if (! l.meter.isEmpty())
    {
        drawVuMeterFace (g, l.meter);

        if (state.drawMeter)
            drawVuMeterNeedle (g, l.meter, state.vu, state.vuStale, state.enabled);
    }

    // ---- inputs -----------------------------------------------------------------------
    for (auto& in : l.inputs)
        if (in.getWidth() >= 4.0f)
            drawJack (g, in.getCentre(), in.getWidth() * 0.5f);

    // Every label fitted to its own rectangle: full, short or not at all, never over a neighbour.
    const auto labels = fitAmpFaceLabels (l);
    drawFittedPrint (g, labels.input, l.inputLabel, ink, false, juce::Justification::centredTop);

    // ---- knobs and their labels -----------------------------------------------------
    for (int i = 0; i < numAmpKnobs; ++i)
    {
        if (state.drawKnobs)
            paintKnob (g, l.knobs[(size_t) i], look.cap, state.knobs[(size_t) i], state.enabled);

        drawFittedPrint (g, labels.knobs[(size_t) i], l.labels[(size_t) i], ink, false);
    }

    // ---- switches and pilot -------------------------------------------------------------
    const bool switchValues[numAmpSwitches] = { state.bright, state.midBoost, ! state.standby };

    if (l.hasSwitches)
    {
        for (int i = 0; i < numAmpSwitches; ++i)
        {
            const auto area = l.switches[(size_t) i];

            if (state.drawSwitches && area.getWidth() >= 4.0f)
            {
                if (look.rockers && i == standbySwitch)
                    drawRocker (g, area, switchValues[i], m.jewelRed);
                else
                    LuthierLookAndFeel::drawMiniToggle (g, area, switchValues[i], state.enabled);
            }

            drawFittedPrint (g, labels.switches[(size_t) i], l.switchLabels[(size_t) i], ink, false);
        }
    }

    // visual-polish.md 2: the pilot light follows Standby.
    const bool lit = ! state.standby && state.enabled;
    const auto pilotColour = look.pilot == Pilot::led ? (model == AmpModel::AcousticDI ? m.ledGreen : m.ledCool) : m.jewelRed;

    if (look.pilot == Pilot::jewel)
        drawJewel (g, l.pilot.getCentre(), l.pilot.getWidth() * 0.5f, pilotColour, lit);
    else
        drawLed (g, l.pilot.getCentre(), l.pilot.getWidth() * 0.3f, pilotColour, lit);

    // Channel LEDs on the modern heads, one lit.
    if (look.channelLeds > 0 && ! l.channelLeds.isEmpty())
    {
        const auto row = l.channelLeds;
        const float spacing = row.getWidth() / (float) look.channelLeds;
        const float r = juce::jmax (1.5f, juce::jmin (row.getHeight() * 0.3f, spacing * 0.3f));

        for (int i = 0; i < look.channelLeds; ++i)
        {
            const float x = row.getX() + spacing * ((float) i + 0.5f);
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

void paintAmpFaceMeter (juce::Graphics& g, juce::Rectangle<float> bounds, AmpModel model, const AmpFaceState& state)
{
    const auto l = layoutAmpFace (bounds, model, state.hasSwitches);

    if (! l.meter.isEmpty())
        drawVuMeterNeedle (g, l.meter, state.vu, state.vuStale, state.enabled);
}

void paintAmpFaceValves (juce::Graphics& g, juce::Rectangle<float> bounds, AmpModel model, const AmpFaceState& state)
{
    const auto look = lookFor (model);
    const auto l = layoutAmpFace (bounds, model, state.hasSwitches);

    if (look.tubes > 0 && ! l.vent.isEmpty())
        drawTubeVent (g, l.vent, look.tubes, state.drive, state.standby, state.driveStale);
}

} // namespace luthier::faces
