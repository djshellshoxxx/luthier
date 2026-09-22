#include "GuitarBodyComponent.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    /** The proportions that distinguish one body shape from another. */
    struct ShapeProportions
    {
        float upperBout;      ///< Width of the upper bout, 0-1 of the area.
        float waist;          ///< Width at the waist.
        float lowerBout;      ///< Width of the lower bout.
        float upperCutaway;   ///< How deeply the treble-side horn cuts in.
        float lowerCutaway;   ///< The bass-side horn; 0 for a single cutaway.
        float offsetSkew;     ///< Vertical offset between the two bouts.
        float squareness;     ///< 0 = round curves, 1 = angular.
    };

    ShapeProportions proportionsFor (BodyShape shape) noexcept
    {
        switch (shape)
        {
            case BodyShape::SolidThin:
            case BodyShape::SolidStandard:
            case BodyShape::SolidHeavy:   return { 0.82f, 0.62f, 0.94f, 0.40f, 0.28f, 0.00f, 0.10f };
            case BodyShape::Offset:       return { 0.86f, 0.60f, 0.92f, 0.32f, 0.20f, 0.14f, 0.05f };
            case BodyShape::SemiHollow:   return { 0.90f, 0.66f, 0.96f, 0.34f, 0.30f, 0.00f, 0.00f };
            case BodyShape::Hollow:       return { 0.92f, 0.68f, 0.98f, 0.30f, 0.26f, 0.00f, 0.00f };
            case BodyShape::Chambered:    return { 0.84f, 0.62f, 0.94f, 0.38f, 0.10f, 0.02f, 0.14f };

            case BodyShape::Parlor:       return { 0.70f, 0.54f, 0.84f, 0.00f, 0.00f, 0.00f, 0.00f };
            case BodyShape::Concert:      return { 0.74f, 0.56f, 0.88f, 0.00f, 0.00f, 0.00f, 0.00f };
            case BodyShape::Auditorium:   return { 0.76f, 0.56f, 0.90f, 0.00f, 0.00f, 0.00f, 0.00f };
            case BodyShape::Dreadnought:  return { 0.84f, 0.70f, 0.94f, 0.00f, 0.00f, 0.00f, 0.00f };
            case BodyShape::TwelveStringDread: return { 0.85f, 0.71f, 0.95f, 0.00f, 0.00f, 0.00f, 0.00f };
            case BodyShape::Jumbo:        return { 0.88f, 0.74f, 1.00f, 0.00f, 0.00f, 0.00f, 0.00f };
            case BodyShape::Classical:    return { 0.74f, 0.54f, 0.88f, 0.00f, 0.00f, 0.00f, 0.00f };
            case BodyShape::Flamenco:     return { 0.72f, 0.52f, 0.86f, 0.00f, 0.00f, 0.00f, 0.00f };
            case BodyShape::Resonator:    return { 0.80f, 0.64f, 0.90f, 0.00f, 0.00f, 0.00f, 0.06f };

            case BodyShape::BassSolid:    return { 0.80f, 0.60f, 0.92f, 0.44f, 0.30f, 0.06f, 0.08f };
            case BodyShape::BassHollow:   return { 0.88f, 0.66f, 0.96f, 0.30f, 0.24f, 0.04f, 0.02f };

            case BodyShape::NumShapes:
            default:                      return { 0.82f, 0.62f, 0.94f, 0.40f, 0.28f, 0.00f, 0.10f };
        }
    }
}

//==============================================================================
GuitarBodyComponent::GuitarBodyComponent (LuthierAudioProcessor& p)
    : processor (p)
{
    stringLevels.fill (0.0);
    startTimerHz (24);
}

GuitarBodyComponent::~GuitarBodyComponent()
{
    stopTimer();
}

//==============================================================================
juce::Colour GuitarBodyComponent::woodColourFor (int wood)
{
    switch ((Wood) wood)
    {
        case Wood::SitkaSpruce: return juce::Colour (0xffd8c093);
        case Wood::Cedar:       return juce::Colour (0xffc19a6b);
        case Wood::Mahogany:    return juce::Colour (0xff7b3f26);
        case Wood::Maple:       return juce::Colour (0xffd9b378);
        case Wood::Alder:       return juce::Colour (0xffc19a70);
        case Wood::Ash:         return juce::Colour (0xffd4b98a);
        case Wood::Rosewood:    return juce::Colour (0xff4a2c20);
        case Wood::Koa:         return juce::Colour (0xffa0673a);
        case Wood::Basswood:    return juce::Colour (0xffcbb694);
        case Wood::Korina:      return juce::Colour (0xffc3a271);
        case Wood::Poplar:      return juce::Colour (0xffb8ab84);
        case Wood::Walnut:      return juce::Colour (0xff6b4430);
        case Wood::Ebony:       return juce::Colour (0xff231d19);
        case Wood::Sapele:      return juce::Colour (0xff7d4a2e);
        case Wood::Agathis:     return juce::Colour (0xffbda57f);
        case Wood::Nato:        return juce::Colour (0xff8a5636);
        case Wood::NumWoods:
        default:                return juce::Colour (0xff8a5636);
    }
}

//==============================================================================
juce::Path GuitarBodyComponent::buildBodyOutline (juce::Rectangle<float> area,
                                                  float upperBout, float waist, float lowerBout,
                                                  float upperCutaway, float lowerCutaway,
                                                  float offsetSkew, float squareness)
{
    juce::Path path;

    const float cx = area.getCentreX();
    const float top = area.getY();
    const float bottom = area.getBottom();
    const float height = area.getHeight();
    const float halfWidth = area.getWidth() * 0.5f;

    // Vertical anchors down the body: neck joint, upper bout, waist, lower bout, tail.
    const float yNeck   = top;
    const float yUpper  = top + height * (0.22f + offsetSkew * 0.06f);
    const float yWaist  = top + height * 0.50f;
    const float yLower  = top + height * (0.74f - offsetSkew * 0.06f);
    const float yTail   = bottom;

    const float wUpper = halfWidth * upperBout;
    const float wWaist = halfWidth * waist;
    const float wLower = halfWidth * lowerBout;

    // Cutaways pull the bout in toward the neck on the relevant side.
    const float trebleUpper = wUpper * (1.0f - upperCutaway * 0.55f);
    const float bassUpper = wUpper * (1.0f - lowerCutaway * 0.55f);

    // Squareness pulls the control points toward the corners, which is what turns
    // a Strat outline into an Explorer without a separate path.
    const float round = 1.0f - juce::jlimit (0.0f, 1.0f, squareness);

    // ---- treble side, going down ------------------------------------------------
    path.startNewSubPath (cx + halfWidth * 0.14f, yNeck);

    path.cubicTo (cx + trebleUpper * (0.55f + 0.45f * round), yNeck + height * 0.02f,
                  cx + trebleUpper, yUpper - height * 0.08f,
                  cx + trebleUpper, yUpper);

    path.cubicTo (cx + trebleUpper, yUpper + height * 0.10f * round,
                  cx + wWaist, yWaist - height * 0.10f * round,
                  cx + wWaist, yWaist);

    path.cubicTo (cx + wWaist, yWaist + height * 0.10f * round,
                  cx + wLower, yLower - height * 0.10f * round,
                  cx + wLower, yLower);

    path.cubicTo (cx + wLower, yLower + height * 0.14f * round,
                  cx + wLower * 0.55f * round, yTail,
                  cx, yTail);

    // ---- bass side, coming back up ------------------------------------------------
    path.cubicTo (cx - wLower * 0.55f * round, yTail,
                  cx - wLower, yLower + height * 0.14f * round,
                  cx - wLower, yLower);

    path.cubicTo (cx - wLower, yLower - height * 0.10f * round,
                  cx - wWaist, yWaist + height * 0.10f * round,
                  cx - wWaist, yWaist);

    path.cubicTo (cx - wWaist, yWaist - height * 0.10f * round,
                  cx - bassUpper, yUpper + height * 0.08f,
                  cx - bassUpper, yUpper);

    path.cubicTo (cx - bassUpper, yUpper - height * 0.08f,
                  cx - bassUpper * (0.55f + 0.45f * round), yNeck + height * 0.02f,
                  cx - halfWidth * 0.14f, yNeck);

    path.closeSubPath();

    return path;
}

//==============================================================================
void GuitarBodyComponent::rebuildGeometry()
{
    const auto& spec = processor.getEngine().getGuitarSpec();
    auto& pickups = processor.getEngine().getPickupEngine();

    geometry = Geometry {};
    geometry.acoustic = (spec.category == GuitarCategory::Acoustic);

    auto area = getLocalBounds().toFloat().reduced (Metrics::grid);

    if (area.isEmpty())
        return;

    // The neck runs up the left and the body sits on the right, which matches how
    // a right-handed player sees the instrument on their knee.
    auto neckArea = area.removeFromLeft (area.getWidth() * 0.44f);
    auto bodyArea = area;

    // Keep the body a sane aspect ratio whatever the panel is.
    const float bodyHeight = juce::jmin (bodyArea.getHeight(), bodyArea.getWidth() * 1.28f);
    bodyArea = bodyArea.withSizeKeepingCentre (bodyHeight / 1.28f, bodyHeight);

    const auto prop = proportionsFor (spec.bodyShape);

    geometry.body = buildBodyOutline (bodyArea, prop.upperBout, prop.waist, prop.lowerBout,
                                      prop.upperCutaway, prop.lowerCutaway,
                                      prop.offsetSkew, prop.squareness);

    geometry.bodyColour = woodColourFor ((int) spec.backWood);
    geometry.topColour = woodColourFor ((int) spec.topWood);

    // ---- neck and headstock ------------------------------------------------------
    const float neckWidth = neckArea.getHeight() * 0.20f;
    const float neckY = bodyArea.getCentreY() - neckWidth * 0.5f;

    geometry.neck.addRoundedRectangle (neckArea.getX() + neckArea.getWidth() * 0.18f, neckY,
                                       bodyArea.getCentreX() - neckArea.getX() - neckArea.getWidth() * 0.18f,
                                       neckWidth, 2.0f);

    const float headW = neckArea.getWidth() * 0.20f;
    const float headH = neckWidth * 1.9f;

    geometry.headstock.addRoundedRectangle (neckArea.getX(),
                                            bodyArea.getCentreY() - headH * 0.5f,
                                            headW, headH, 3.0f);

    // ---- soundhole or pickups -------------------------------------------------------
    if (geometry.acoustic && spec.bodyShape != BodyShape::Resonator)
    {
        geometry.hasSoundHole = true;

        const float holeRadius = bodyArea.getWidth() * 0.155f;
        const float holeY = bodyArea.getY() + bodyArea.getHeight() * 0.36f;

        geometry.soundHole.addEllipse (bodyArea.getCentreX() - holeRadius, holeY - holeRadius,
                                       holeRadius * 2.0f, holeRadius * 2.0f);

        geometry.bridgeBounds = { bodyArea.getCentreX() - bodyArea.getWidth() * 0.19f,
                                  bodyArea.getY() + bodyArea.getHeight() * 0.68f,
                                  bodyArea.getWidth() * 0.38f,
                                  bodyArea.getHeight() * 0.055f };
    }
    else
    {
        geometry.numPickups = juce::jlimit (0, 3, spec.numPickups);

        // Pickup position runs 0 at the bridge to 0.5 at the midpoint of the
        // string, so the illustration places them from exactly the same number the
        // comb filter uses.
        const float bridgeY = bodyArea.getY() + bodyArea.getHeight() * 0.76f;
        const float neckJointY = bodyArea.getY() + bodyArea.getHeight() * 0.12f;
        const float span = bridgeY - neckJointY;

        for (int i = 0; i < geometry.numPickups; ++i)
        {
            const auto& pspec = pickups.getPickupSpec (i);
            const float t = (float) juce::jlimit (0.02, 0.48, pspec.position) / 0.48f;

            const bool humbucker = (pspec.type == PickupType::Humbucker && ! pspec.coilTapped);
            const float pickupH = bodyArea.getHeight() * (humbucker ? 0.062f : 0.040f);
            const float pickupW = bodyArea.getWidth() * 0.34f;

            const float y = bridgeY - span * t * 0.86f;

            geometry.pickupBounds[i] = { bodyArea.getCentreX() - pickupW * 0.5f,
                                         y - pickupH * 0.5f, pickupW, pickupH };

            cachedPickupPositions[i] = pspec.position;
        }

        geometry.bridgeBounds = { bodyArea.getCentreX() - bodyArea.getWidth() * 0.18f,
                                  bridgeY, bodyArea.getWidth() * 0.36f,
                                  bodyArea.getHeight() * 0.05f };
    }

    // ---- controls --------------------------------------------------------------------
    const float knobSize = juce::jlimit (16.0f, 30.0f, bodyArea.getWidth() * 0.13f);

    geometry.volumeKnob = { bodyArea.getCentreX() + bodyArea.getWidth() * 0.16f,
                            bodyArea.getY() + bodyArea.getHeight() * 0.62f,
                            knobSize, knobSize };

    geometry.toneKnob = { bodyArea.getCentreX() + bodyArea.getWidth() * 0.16f,
                          bodyArea.getY() + bodyArea.getHeight() * 0.62f + knobSize * 1.35f,
                          knobSize, knobSize };

    geometry.selectorSwitch = { bodyArea.getCentreX() - bodyArea.getWidth() * 0.34f,
                                bodyArea.getY() + bodyArea.getHeight() * 0.40f,
                                knobSize * 0.75f, knobSize * 1.5f };

    if (! geometry.acoustic)
        geometry.pickguard = geometry.body;

    cachedGuitarType = (int) processor.getEngine().getGuitarType();
    cachedNumPickups = geometry.numPickups;
}

//==============================================================================
void GuitarBodyComponent::resized()
{
    rebuildGeometry();
}

void GuitarBodyComponent::timerCallback()
{
    auto& engine = processor.getEngine();

    bool needsRebuild = ((int) engine.getGuitarType() != cachedGuitarType);

    for (int i = 0; i < juce::jmin (3, engine.getGuitarSpec().numPickups); ++i)
    {
        const double p = engine.getPickupEngine().getPickupSpec (i).position;

        if (std::abs (p - cachedPickupPositions[i]) > 0.002)
            needsRebuild = true;
    }

    if (needsRebuild)
        rebuildGeometry();

    bool levelChanged = false;

    for (int s = 0; s < engine.getNumStrings(); ++s)
    {
        const double level = engine.getStringLevel (s);

        if (std::abs (level - stringLevels[(size_t) s]) > 0.002)
            levelChanged = true;

        stringLevels[(size_t) s] = level;
    }

    if (needsRebuild || levelChanged)
        repaint();
}

//==============================================================================
void GuitarBodyComponent::paint (juce::Graphics& g)
{
    if (geometry.body.isEmpty())
        return;

    auto& engine = processor.getEngine();
    const auto& spec = engine.getGuitarSpec();
    auto& pickups = engine.getPickupEngine();

    // ---- neck ----------------------------------------------------------------
    g.setColour (woodColourFor ((int) spec.neckWood).darker (0.35f));
    g.fillPath (geometry.neck);

    g.setColour (Palette::backgroundDeep.withAlpha (0.6f));
    g.strokePath (geometry.neck, juce::PathStrokeType (1.0f));

    g.setColour (woodColourFor ((int) spec.neckWood).darker (0.55f));
    g.fillPath (geometry.headstock);

    // ---- body ------------------------------------------------------------------
    juce::DropShadow (Palette::shadow, 12, { 0, 3 }).drawForPath (g, geometry.body);

    const auto bounds = geometry.body.getBounds();

    juce::ColourGradient bodyGradient (geometry.topColour.brighter (0.10f),
                                       bounds.getCentreX(), bounds.getY(),
                                       geometry.bodyColour.darker (0.32f),
                                       bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (bodyGradient);
    g.fillPath (geometry.body);

    // A soft highlight along the upper edge reads as a gloss finish without any
    // texture map.
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.saveState();
    g.reduceClipRegion (geometry.body);
    g.fillEllipse (bounds.getX() + bounds.getWidth() * 0.12f,
                   bounds.getY() - bounds.getHeight() * 0.20f,
                   bounds.getWidth() * 0.76f, bounds.getHeight() * 0.55f);
    g.restoreState();

    g.setColour (Palette::backgroundDeep.withAlpha (0.75f));
    g.strokePath (geometry.body, juce::PathStrokeType (1.5f));

    // ---- soundhole -------------------------------------------------------------
    if (geometry.hasSoundHole)
    {
        const auto holeBounds = geometry.soundHole.getBounds();

        // Rosette rings.
        for (int i = 0; i < 3; ++i)
        {
            const float inset = -4.0f - (float) i * 3.5f;
            g.setColour ((i % 2 == 0 ? Palette::accent : Palette::secondary).withAlpha (0.55f));
            g.drawEllipse (holeBounds.expanded (-inset), 1.4f);
        }

        g.setColour (juce::Colour (0xff0a0806));
        g.fillPath (geometry.soundHole);

        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (holeBounds.reduced (0.5f), 1.0f);
    }

    // ---- pickups ---------------------------------------------------------------
    const auto selector = pickups.getSelector();

    auto isSlotActive = [selector, &spec] (int slot)
    {
        const int neck = juce::jmax (0, spec.numPickups - 1);
        const int middle = (spec.numPickups >= 3) ? 1 : -1;

        switch (selector)
        {
            case PickupSelector::Bridge:       return slot == 0;
            case PickupSelector::BridgeMiddle: return slot == 0 || slot == middle;
            case PickupSelector::Middle:       return slot == middle;
            case PickupSelector::MiddleNeck:   return slot == middle || slot == neck;
            case PickupSelector::Neck:         return slot == neck;
            case PickupSelector::BridgeNeck:   return slot == 0 || slot == neck;
            case PickupSelector::All:          return true;
            case PickupSelector::NumSelections:
            default:                           return slot == 0;
        }
    };

    for (int i = 0; i < geometry.numPickups; ++i)
    {
        const auto& r = geometry.pickupBounds[i];
        const auto& pspec = pickups.getPickupSpec (i);
        const bool active = isSlotActive (i);
        const bool hovered = (i == hoveredPickup);

        g.setColour (active ? juce::Colour (0xff1c1a18) : juce::Colour (0xff141210));
        g.fillRoundedRectangle (r, 2.0f);

        g.setColour (active ? Palette::accent : Palette::edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, hovered ? 1.8f : 1.0f);

        // Pole pieces, one per string; a humbucker gets two rows.
        const int strings = engine.getNumStrings();
        const bool dual = (pspec.type == PickupType::Humbucker && ! pspec.coilTapped);
        const int rows = dual ? 2 : 1;

        for (int row = 0; row < rows; ++row)
        {
            const float y = dual ? juce::jmap ((float) row, 0.0f, 1.0f,
                                               r.getY() + r.getHeight() * 0.30f,
                                               r.getY() + r.getHeight() * 0.70f)
                                 : r.getCentreY();

            for (int s = 0; s < strings; ++s)
            {
                const float x = juce::jmap ((float) s, 0.0f, (float) juce::jmax (1, strings - 1),
                                            r.getX() + r.getWidth() * 0.12f,
                                            r.getRight() - r.getWidth() * 0.12f);

                // The pole glows with what that string is doing: the picture shows
                // the instrument actually working.
                const float level = (float) juce::jlimit (0.0, 1.0, stringLevels[(size_t) s] * 16.0);

                g.setColour (active ? Palette::textMuted.interpolatedWith (Palette::accentBright, level)
                                    : Palette::textDisabled);
                g.fillEllipse (x - 1.6f, y - 1.6f, 3.2f, 3.2f);
            }
        }
    }

    // ---- bridge -----------------------------------------------------------------
    g.setColour (juce::Colour (0xff9a927f));
    g.fillRoundedRectangle (geometry.bridgeBounds, 1.5f);

    g.setColour (Palette::backgroundDeep.withAlpha (0.6f));
    g.drawRoundedRectangle (geometry.bridgeBounds.reduced (0.5f), 1.5f, 1.0f);

    // ---- strings across the body --------------------------------------------------
    {
        const int strings = engine.getNumStrings();
        const float x0 = geometry.headstock.getBounds().getCentreX();
        const float x1 = geometry.bridgeBounds.getCentreX();

        for (int s = 0; s < strings; ++s)
        {
            const float spread = geometry.bridgeBounds.getWidth() * 0.42f;
            const float y = juce::jmap ((float) s, 0.0f, (float) juce::jmax (1, strings - 1),
                                        geometry.bridgeBounds.getCentreY() - spread,
                                        geometry.bridgeBounds.getCentreY() + spread);

            const float neckY = juce::jmap ((float) s, 0.0f, (float) juce::jmax (1, strings - 1),
                                            geometry.neck.getBounds().getY() + 3.0f,
                                            geometry.neck.getBounds().getBottom() - 3.0f);

            const float level = (float) juce::jlimit (0.0, 1.0, stringLevels[(size_t) s] * 14.0);

            g.setColour (Palette::textMuted.withAlpha (0.35f + level * 0.55f));
            g.drawLine (x0, neckY, x1, y, 0.8f + level * 1.2f);
        }
    }

    // ---- controls ---------------------------------------------------------------------
    auto drawBodyKnob = [&g] (juce::Rectangle<float> r, double value, const juce::String& text, bool hot)
    {
        juce::DropShadow (Palette::shadow, 4, { 0, 1 }).drawForRectangle (g, r.toNearestInt());

        juce::ColourGradient gradient (juce::Colour (0xff3a2f24), r.getCentreX(), r.getY(),
                                        juce::Colour (0xff1a1410), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (gradient);
        g.fillEllipse (r);

        g.setColour (hot ? Palette::accent : Palette::edge);
        g.drawEllipse (r.reduced (0.5f), 1.0f);

        // Pointer, over a 270-degree sweep like every other knob in the plugin.
        const float angle = Metrics::arcStart
                            + (float) juce::jlimit (0.0, 1.0, value) * (Metrics::arcEnd - Metrics::arcStart);
        const auto centre = r.getCentre();
        const auto tip = centre.getPointOnCircumference (r.getWidth() * 0.34f, angle);

        g.setColour (Palette::accentBright);
        g.drawLine ({ centre, tip }, 1.6f);

        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (8.0f));
        g.drawText (text, r.translated (0.0f, r.getHeight() * 0.62f).withHeight (10.0f),
                    juce::Justification::centred, false);
    };

    const auto& state = processor.getState();

    const double volume = state.getParameter (ParamIDs::guitarVolume) != nullptr
                            ? state.getParameter (ParamIDs::guitarVolume)->getValue() : 1.0;
    const double tone = state.getParameter (ParamIDs::guitarTone) != nullptr
                          ? state.getParameter (ParamIDs::guitarTone)->getValue() : 1.0;

    drawBodyKnob (geometry.volumeKnob, volume, "VOL", draggingKnob == 0);
    drawBodyKnob (geometry.toneKnob, tone, "TONE", draggingKnob == 1);

    // ---- selector switch -----------------------------------------------------------------
    if (geometry.numPickups > 1)
    {
        auto r = geometry.selectorSwitch;

        g.setColour (juce::Colour (0xff15110e));
        g.fillRoundedRectangle (r, 3.0f);

        g.setColour (Palette::edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

        const int positions = (geometry.numPickups >= 3) ? 5 : 3;
        const int current = juce::jlimit (0, positions - 1, (int) selector);

        const float lever = juce::jmap ((float) current, 0.0f, (float) (positions - 1),
                                        r.getBottom() - 6.0f, r.getY() + 6.0f);

        g.setColour (Palette::accent);
        g.fillRoundedRectangle (r.getCentreX() - 2.0f, lever - 5.0f, 4.0f, 10.0f, 2.0f);

        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (7.5f));
        g.drawText ("SEL", r.translated (0.0f, r.getHeight() * 0.5f + 4.0f).withHeight (10.0f),
                    juce::Justification::centred, false);
    }

    // ---- name plate -------------------------------------------------------------------
    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (11.0f, true));
    g.drawText (spec.name, getLocalBounds().removeFromBottom (16).reduced (Metrics::grid, 0),
                juce::Justification::centredLeft, true);
}

//==============================================================================
void GuitarBodyComponent::mouseMove (const juce::MouseEvent& e)
{
    int hovered = -1;

    for (int i = 0; i < geometry.numPickups; ++i)
        if (geometry.pickupBounds[i].contains (e.position))
            hovered = i;

    if (hovered != hoveredPickup)
    {
        hoveredPickup = hovered;

        if (hovered >= 0)
        {
            const auto& pspec = processor.getEngine().getPickupEngine().getPickupSpec (hovered);
            setTooltip (juce::String (Parameters::pickupTypeNames()[(int) pspec.type])
                        + " at " + juce::String (pspec.position * 100.0, 1) + "% of the string");
        }
        else
        {
            setTooltip (describeHoverTarget (e.position));
        }

        repaint();
    }
    else if (hovered < 0)
    {
        /*  The headstock and the bridge are both outside every pickup, so moving
            between them never changes hoveredPickup and the branch above never
            fires. Without this the tooltip would keep saying "click a pickup"
            while the cursor sat on the headstock - which is how a click target
            nobody knows about stays unknown. Section 20 is discoverability. */
        const auto wanted = describeHoverTarget (e.position);

        if (wanted != getTooltip())
            setTooltip (wanted);
    }
}

juce::String GuitarBodyComponent::describeHoverTarget (juce::Point<float> position) const
{
    if (geometry.headstock.contains (position))
        return "Click the headstock for tuning, temperament and per-string detune.";

    if (geometry.bridgeBounds.contains (position))
    {
        const auto bridge = Parameters::bridgeTypeNames()[getBridgeTypeIndex()];

        return WhammyPopover::isWhammyFitted (const_cast<LuthierAudioProcessor&> (processor))
                 ? bridge + " - click the bridge for whammy range and spring tension."
                 : bridge + " - no arm fitted, so there is nothing to set here.";
    }

    return "Click a pickup to select it, drag the knobs, click the switch to change position.";
}

int GuitarBodyComponent::getBridgeTypeIndex() const
{
    if (auto* param = processor.getState().getParameter (ParamIDs::bridgeType))
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
            return choice->getIndex();

    return 0;
}

void GuitarBodyComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredPickup = -1;
    repaint();
}

void GuitarBodyComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& state = processor.getState();

    // ---- knobs ---------------------------------------------------------------
    if (geometry.volumeKnob.contains (e.position))
    {
        draggingKnob = 0;
        dragStartY = e.y;

        if (auto* p = state.getParameter (ParamIDs::guitarVolume))
            dragStartValue = p->getValue();

        return;
    }

    if (geometry.toneKnob.contains (e.position))
    {
        draggingKnob = 1;
        dragStartY = e.y;

        if (auto* p = state.getParameter (ParamIDs::guitarTone))
            dragStartValue = p->getValue();

        return;
    }

    // ---- selector switch -------------------------------------------------------
    if (geometry.numPickups > 1 && geometry.selectorSwitch.contains (e.position))
    {
        if (auto* p = state.getParameter (ParamIDs::pickupSelector))
        {
            const int positions = (geometry.numPickups >= 3) ? 5 : 3;
            const int current = (int) std::round (p->getValue() * (float) (positions - 1));
            const int next = (current + 1) % positions;

            p->setValueNotifyingHost ((float) next / (float) juce::jmax (1, positions - 1));
        }

        repaint();
        return;
    }

    // ---- pickups -----------------------------------------------------------------
    for (int i = 0; i < geometry.numPickups; ++i)
    {
        if (! geometry.pickupBounds[i].contains (e.position))
            continue;

        if (e.mods.isPopupMenu())
        {
            showParameterContextMenu (*this, processor, ParamIDs::pickupPosition (i));
            return;
        }

        // Clicking a pickup selects that pickup alone, which is what the switch on
        // the real instrument would do.
        if (auto* p = state.getParameter (ParamIDs::pickupSelector))
        {
            const int neck = juce::jmax (0, geometry.numPickups - 1);
            const int target = (i == 0) ? (int) PickupSelector::Bridge
                             : (i == neck) ? (int) PickupSelector::Neck
                                           : (int) PickupSelector::Middle;

            p->setValueNotifyingHost ((float) target / (float) ((int) PickupSelector::NumSelections - 1));
        }

        if (onPickupSelected)
            onPickupSelected (i);

        repaint();
        return;
    }

    /*  ---- headstock and bridge, section 3.1 -------------------------------------

        These two come last on purpose. The knobs, the selector and the pickups are
        small targets sitting on top of the body, and the headstock and bridge
        paths are large; testing the large regions first would swallow clicks
        meant for the controls drawn over them.

        Both open a popover rather than acting directly, because both are a group
        of settings rather than one value - which is what section 3.1 asks for.
    */
    if (geometry.headstock.contains (e.position))
    {
        showTuningPopover();
        return;
    }

    if (geometry.bridgeBounds.contains (e.position))
    {
        showWhammyPopover();
        return;
    }
}

void GuitarBodyComponent::showTuningPopover()
{
    auto popover = std::make_unique<TuningPopover> (processor);

    auto area = getScreenBounds();

    // Anchored on the headstock rather than the whole component, so the callout
    // points at the thing that was clicked.
    if (! geometry.headstock.getBounds().isEmpty())
        area = geometry.headstock.getBounds().toNearestInt() + getScreenPosition();

    juce::CallOutBox::launchAsynchronously (std::move (popover), area, nullptr);
}

void GuitarBodyComponent::showWhammyPopover()
{
    /*  Section 3.1: "only if a whammy is fitted". On a hardtail the click does
        nothing rather than opening a popover whose every control is inert - and
        the tooltip under the cursor has already said which bridge is fitted, so
        the silence is not unexplained. */
    if (! WhammyPopover::isWhammyFitted (processor))
        return;

    auto popover = std::make_unique<WhammyPopover> (processor);

    const auto area = geometry.bridgeBounds.isEmpty()
                        ? getScreenBounds()
                        : geometry.bridgeBounds.toNearestInt() + getScreenPosition();

    juce::CallOutBox::launchAsynchronously (std::move (popover), area, nullptr);
}

void GuitarBodyComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0)
        return;

    const auto id = (draggingKnob == 0) ? juce::String (ParamIDs::guitarVolume)
                                        : juce::String (ParamIDs::guitarTone);

    if (auto* p = processor.getState().getParameter (id))
    {
        // Vertical drag, 150 px for the full range, matching the main knobs.
        const double delta = (double) (dragStartY - e.y) / 150.0;
        p->setValueNotifyingHost ((float) juce::jlimit (0.0, 1.0, dragStartValue + delta));
    }

    repaint();
}

void GuitarBodyComponent::mouseUp (const juce::MouseEvent&)
{
    draggingKnob = -1;
    repaint();
}


//==============================================================================
//  TuningPopover
//==============================================================================
namespace
{
    constexpr int kPopoverPadding = 12;
    constexpr int kRowHeight = 22;
    constexpr int kNoteColumnWidth = 44;
}

TuningPopover::TuningPopover (LuthierAudioProcessor& p)
    : processor (p)
{
    numStrings = juce::jlimit (1, 12, processor.getEngine().getNumStrings());

    preset = std::make_unique<LuthierChoice> ("Tuning");
    preset->attachTo (processor, ParamIDs::tuningPreset,
                      "The open tuning every string starts from.");
    addAndMakeVisible (*preset);

    temperament = std::make_unique<LuthierChoice> ("Temperament");
    temperament->attachTo (processor, ParamIDs::temperament,
                           "How the twelve semitones are spaced. Equal is the modern default.");
    addAndMakeVisible (*temperament);

    capo = std::make_unique<LuthierChoice> ("Capo");
    capo->attachTo (processor, ParamIDs::capoFret,
                    "Where the capo sits. The open strings become the capo'd notes, and "
                    "the neck gets that much shorter.");
    addAndMakeVisible (*capo);

    concertA = std::make_unique<LuthierKnob> ("Concert A", LuthierKnob::Size::Small);
    concertA->attachTo (processor, ParamIDs::concertA,
                        "Reference pitch. 440 Hz is standard; 415 is baroque.");
    addAndMakeVisible (*concertA);

    /*  One detune slider per string, writing TuningEngine directly.

        These are not parameters, so there is no attachment and no MIDI Learn on
        them - see the note on the class. The value is read back from the engine
        on construction rather than cached here, so a preset loaded behind this
        popover and a popover opened after it agree. */
    auto& tuning = processor.getEngine().getTuningEngine();

    for (int i = 0; i < numStrings; ++i)
    {
        auto* slider = detuneSliders.add (new juce::Slider (juce::Slider::LinearHorizontal,
                                                            juce::Slider::TextBoxRight));

        slider->setRange (-100.0, 100.0, 0.1);
        slider->setTextValueSuffix (" ct");
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        slider->setDoubleClickReturnValue (true, 0.0);
        slider->setValue (tuning.getStringTuning (i).detuneCents, juce::dontSendNotification);

        slider->setTooltip ("Deliberate detune for this string, in cents. "
                            "Double-click to return it to zero.");

        AccessibleSetup::configureSlider (*slider, "String " + juce::String (i + 1) + " detune",
                                          " cents");

        /*  An undo entry per gesture rather than per value change: a drag is one
            action to the user, and action-and-undo.md asks for the undo stack to
            match what they think they did. */
        slider->onDragStart = [this] { processor.pushUndoState ("Detune string"); };

        slider->onValueChange = [this, i, slider]
        {
            processor.getEngine().getTuningEngine().setDetuneCents (i, slider->getValue());
            refreshNoteNames();
            repaint();
        };

        addAndMakeVisible (slider);
    }

    refreshNoteNames();

    setSize (preferredSize (numStrings).getWidth(), preferredSize (numStrings).getHeight());
}

TuningPopover::~TuningPopover() = default;

juce::Rectangle<int> TuningPopover::preferredSize (int numStrings)
{
    const int rows = juce::jlimit (1, 12, numStrings);

    // Three choice rows, a knob, the string rows, and the footer.
    const int height = kPopoverPadding * 2
                         + LuthierChoice::labelHeight + kRowHeight          // tuning
                         + LuthierChoice::labelHeight + kRowHeight          // temperament
                         + LuthierChoice::labelHeight + kRowHeight          // capo
                         + LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small)
                         + 8
                         + rows * kRowHeight
                         + 8 + 28;

    return { 0, 0, 300, height };
}

void TuningPopover::refreshNoteNames()
{
    const auto& tuning = processor.getEngine().getTuningEngine();

    noteNames.clearQuick();

    for (int i = 0; i < numStrings; ++i)
        noteNames.add (TuningEngine::describeFrequency (tuning.getEffectiveOpenFrequency (i),
                                                       tuning.getConcertA()));
}

void TuningPopover::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);

    auto bounds = getLocalBounds().reduced (kPopoverPadding);

    // The note name beside each slider, so a detune is read as a pitch rather
    // than as a number of cents.
    auto rows = bounds.removeFromBottom (8 + 28 + numStrings * kRowHeight)
                      .withTrimmedBottom (8 + 28);

    g.setFont (Fonts::mono (10.0f));

    for (int i = 0; i < numStrings; ++i)
    {
        auto row = rows.removeFromTop (kRowHeight);

        g.setColour (Palette::textMuted);
        g.drawText (noteNames[i], row.removeFromLeft (kNoteColumnWidth),
                    juce::Justification::centredLeft, false);
    }

    /*  Section 3.1's third item used to be a line here saying capo was not built,
        which was true of the pitch and wrong about the plugin: there was a capo
        in RhythmEngine moving chord voicings and another on the fretboard drawing
        itself, and neither changed a note. There is one capo now and it is the
        control above. Partial capos are still absent - they need the capo part
        from the Workshop - so that is what the footer says instead. */
    auto footer = getLocalBounds().reduced (kPopoverPadding).removeFromBottom (28);

    g.setColour (Palette::edge);
    g.fillRect (footer.removeFromTop (1));

    g.setColour (Palette::textDisabled);
    g.setFont (Fonts::ui (10.0f));
    g.drawText ("Partial capos need the Workshop, which is not built.",
                footer, juce::Justification::centredLeft, true);
}

void TuningPopover::resized()
{
    auto bounds = getLocalBounds().reduced (kPopoverPadding);

    preset->setBounds (bounds.removeFromTop (LuthierChoice::labelHeight + kRowHeight));
    temperament->setBounds (bounds.removeFromTop (LuthierChoice::labelHeight + kRowHeight));
    capo->setBounds (bounds.removeFromTop (LuthierChoice::labelHeight + kRowHeight));

    concertA->setBounds (bounds.removeFromTop (
        LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small)));

    bounds.removeFromTop (8);
    bounds.removeFromBottom (8 + 28);

    for (auto* slider : detuneSliders)
    {
        auto row = bounds.removeFromTop (kRowHeight);
        row.removeFromLeft (kNoteColumnWidth);          // the note name paint() draws
        slider->setBounds (row);
    }
}

//==============================================================================
//  WhammyPopover
//==============================================================================
WhammyPopover::WhammyPopover (LuthierAudioProcessor& p)
    : processor (p)
{
    bridgeType = std::make_unique<LuthierChoice> ("Bridge");
    bridgeType->attachTo (processor, ParamIDs::bridgeType,
                          "The bridge fitted to this instrument. A hardtail has no arm.");
    addAndMakeVisible (*bridgeType);

    auto addKnob = [this] (std::unique_ptr<LuthierKnob>& knob, const char* label,
                           const char* paramId, const char* tooltip)
    {
        knob = std::make_unique<LuthierKnob> (label, LuthierKnob::Size::Small);
        knob->attachTo (processor, paramId, tooltip);
        addAndMakeVisible (*knob);
    };

    addKnob (position,  "Arm",     ParamIDs::whammyPos,
             "Where the arm is right now. Centre is at rest.");
    addKnob (downRange, "Down",    ParamIDs::whammyDown,
             "How far down the arm bends, in semitones.");
    addKnob (upRange,   "Up",      ParamIDs::whammyUp,
             "How far up the arm pulls, in semitones. A hardtail-mounted vintage "
             "tremolo pulls up very little.");
    addKnob (springs,   "Springs", ParamIDs::whammySprings,
             "Spring tension. Slacker springs make the bridge return more slowly "
             "and pull the other strings further out of tune.");

    setSize (preferredSize().getWidth(), preferredSize().getHeight());
}

WhammyPopover::~WhammyPopover() = default;

bool WhammyPopover::isWhammyFitted (LuthierAudioProcessor& processor)
{
    /*  Index 0 of bridgeTypeNames is "Fixed / Hardtail" and every other entry is
        a bridge with an arm. Read through the parameter rather than the engine so
        this agrees with what the Bridge control is showing at the instant the
        user clicks. */
    if (auto* param = processor.getState().getParameter (ParamIDs::bridgeType))
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
            return choice->getIndex() > 0;

    return false;
}

juce::Rectangle<int> WhammyPopover::preferredSize()
{
    const int knobHeight = LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small);

    return { 0, 0, 4 * LuthierKnob::preferredWidthFor (LuthierKnob::Size::Small) + kPopoverPadding * 2,
             kPopoverPadding * 2 + LuthierChoice::labelHeight + kRowHeight + 8 + knobHeight };
}

void WhammyPopover::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);
}

void WhammyPopover::resized()
{
    auto bounds = getLocalBounds().reduced (kPopoverPadding);

    bridgeType->setBounds (bounds.removeFromTop (LuthierChoice::labelHeight + kRowHeight));
    bounds.removeFromTop (8);

    const int width = bounds.getWidth() / 4;

    position->setBounds  (bounds.removeFromLeft (width));
    downRange->setBounds (bounds.removeFromLeft (width));
    upRange->setBounds   (bounds.removeFromLeft (width));
    springs->setBounds   (bounds);
}

} // namespace luthier
