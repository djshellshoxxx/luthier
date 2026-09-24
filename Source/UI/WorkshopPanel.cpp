#include "WorkshopPanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

namespace
{
    using Tool = BenchIllustration::Tool;

    /** The pick's colour by material (pick-noise.md 2's list). */
    juce::Colour pickColour (const juce::String& material)
    {
        if (material == "nylon")     return juce::Colour (0xffd9d3c4);
        if (material == "delrin")    return juce::Colour (0xffd94b3b);
        if (material == "ultex")     return juce::Colour (0xffe3c26a);
        if (material == "brass")     return juce::Colour (0xffc9a227);
        if (material == "steel" || material == "metal") return juce::Colour (0xffb8bcc2);
        if (material == "wood")      return juce::Colour (0xffa26b3a);
        if (material == "felt")      return juce::Colour (0xff7a7a7a);
        return juce::Colour (0xff8b4a1f);   // celluloid tortoise
    }

    /** A fret position from a distance along the string, mm from the nut. */
    double fretFromNutDistance (double fromNutMm, double scaleMm)
    {
        const double remaining = 1.0 - fromNutMm / juce::jmax (1.0, scaleMm);
        if (remaining <= 0.001)
            return 24.0;
        return juce::jmax (0.0, -12.0 * std::log2 (remaining));
    }

    double nutDistanceFromFret (double fret, double scaleMm)
    {
        return scaleMm * (1.0 - std::pow (2.0, -juce::jmax (0.0, fret) / 12.0));
    }

    juce::String num (double v, int decimals)
    {
        return decimals == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, decimals);
    }
    int pickupIndexOfRegion (GuitarRegion r) noexcept
    {
        return r == GuitarRegion::pickupNeck ? 0 : r == GuitarRegion::pickupMiddle ? 1 : r == GuitarRegion::pickupBridge ? 2 : -1;
    }

    /** The slot a region on the illustration stands for (section 3). */
    GuitarSlot slotForRegion (GuitarRegion r) noexcept
    {
        switch (r)
        {
            case GuitarRegion::body:          return GuitarSlot::body;
            case GuitarRegion::pickguard:     return GuitarSlot::pickguard;
            case GuitarRegion::soundhole:     return GuitarSlot::body;
            case GuitarRegion::bridge:        return GuitarSlot::bridge;
            case GuitarRegion::tailpiece:     return GuitarSlot::tailpiece;
            case GuitarRegion::pickupNeck:    return GuitarSlot::pickupNeck;
            case GuitarRegion::pickupMiddle:  return GuitarSlot::pickupMiddle;
            case GuitarRegion::pickupBridge:  return GuitarSlot::pickupBridge;
            case GuitarRegion::strings:       return GuitarSlot::strings;
            case GuitarRegion::neck:          return GuitarSlot::neck;
            case GuitarRegion::fretboard:     return GuitarSlot::fretboard;
            case GuitarRegion::nut:           return GuitarSlot::nut;
            case GuitarRegion::headstock:     return GuitarSlot::tuners;
            case GuitarRegion::tuners:        return GuitarSlot::tuners;
            case GuitarRegion::controls:      return GuitarSlot::wiring;
            case GuitarRegion::selector:      return GuitarSlot::wiring;
            case GuitarRegion::jack:          return GuitarSlot::wiring;
            case GuitarRegion::none:
            case GuitarRegion::numRegions:    break;
        }

        return GuitarSlot::numSlots;
    }

    GuitarRenderer::Options renderOptions()
    {
        GuitarRenderer::Options o;
        o.materials = Palette::textured;
        return o;
    }
}

//==============================================================================
BenchIllustration::BenchIllustration (LuthierAudioProcessor& p)
    : processor (p), bench (p.getBench())
{
    setWantsKeyboardFocus (true);
    setTitle (tr ("workshop.illustration.title"));
    setDescription (tr ("workshop.illustration.description"));
    rebuild (true);
    startTimerHz (30);
}

BenchIllustration::~BenchIllustration()
{
    stopTimer();
}

const std::vector<GuitarRegion>& BenchIllustration::builderOrder()
{
    // Section 10: body, neck, fretboard, frets, nut, bridge, tailpiece, tuners,
    // pickups neck to bridge, wiring, strings, pickguard.
    static const std::vector<GuitarRegion> order {
        GuitarRegion::body, GuitarRegion::neck, GuitarRegion::fretboard, GuitarRegion::nut,
        GuitarRegion::bridge, GuitarRegion::tailpiece, GuitarRegion::tuners,
        GuitarRegion::pickupNeck, GuitarRegion::pickupMiddle, GuitarRegion::pickupBridge,
        GuitarRegion::controls, GuitarRegion::strings, GuitarRegion::pickguard
    };
    return order;
}

void BenchIllustration::rebuild (bool force)
{
    const auto* audition = bench.getAuditionGuitar();
    const auto& guitar = audition != nullptr ? *audition : bench.current();
    const auto options = renderOptions();
    const auto key = GuitarRenderer::keyFor (guitar, options);

    if (! force && key == shownKey && (audition != nullptr) == shownAudition)
        return;

    scene = GuitarRenderer::build (guitar, options);
    shownKey = key;
    shownAudition = audition != nullptr;
    resized();
    repaint();
}

void BenchIllustration::refresh()
{
    rebuild (false);
}

void BenchIllustration::resized()
{
    // The fitted view, then zoom about the centre and pan (guitar-illustration.md 1).
    auto area = getLocalBounds().toFloat().reduced (8.0f);
    area.removeFromBottom (22.0f);   // the ruler

    const auto fit = GuitarRenderer::fitTransform (scene, area);
    const auto c = area.getCentre();
    mmToPx = fit.followedBy (juce::AffineTransform::scale (zoom, zoom, c.x, c.y))
                .followedBy (juce::AffineTransform::translation (panPx.x, panPx.y));
}

juce::Point<float> BenchIllustration::toMm (juce::Point<float> px) const
{
    auto p = px;
    mmToPx.inverted().transformPoint (p.x, p.y);
    return p;
}

juce::Point<float> BenchIllustration::toPx (juce::Point<float> mm) const
{
    auto p = mm;
    mmToPx.transformPoint (p.x, p.y);
    return p;
}

void BenchIllustration::timerCallback()
{
    rebuild (false);

    // The live layer (guitar-illustration.md 2.2) is painted over the cached scene.
    if (updateLiveOverlay())
        repaint();
}

bool BenchIllustration::updateLiveOverlay()
{
    auto& engine = processor.getEngine();
    bool changed = false;

    for (int s = 0; s < juce::jmin (12, engine.getNumStrings()); ++s)
    {
        const auto level = (float) juce::jlimit (0.0, 1.0, engine.getStringLevel (s) * 4.0);
        const auto fret = (float) engine.getStringFret (s);

        if (std::abs (level - live.stringLevel[(size_t) s]) > 0.004f || std::abs (fret - live.stringFret[(size_t) s]) > 0.01f)
            changed = true;

        live.stringLevel[(size_t) s] = level;
        live.stringFret[(size_t) s] = fret;
    }

    const bool reduced = AccessibilitySettings::get().isReducedMotion();
    changed = changed || reduced != live.reducedMotion;
    live.reducedMotion = reduced;
    return changed;
}

GuitarRegion BenchIllustration::regionAt (juce::Point<float> px, int* stringIndex) const
{
    const auto mm = toMm (px);
    const auto* hit = GuitarRenderer::hitTest (scene, mm);

    if (hit == nullptr)
        return GuitarRegion::none;

    if (stringIndex != nullptr && hit->region == GuitarRegion::strings)
    {
        // The nearest string where the pointer is along the neck.
        float best = 1.0e9f;

        for (int s = 0; s < (int) scene.saddlePoints.size(); ++s)
        {
            const auto a = scene.saddlePoints[(size_t) s], b = scene.nutPoints[(size_t) s];
            const float t = juce::jlimit (0.0f, 1.0f, (mm.x - a.x) / juce::jmax (1.0f, b.x - a.x));
            const float y = a.y + (b.y - a.y) * t;

            if (std::abs (mm.y - y) < best)
            {
                best = std::abs (mm.y - y);
                *stringIndex = s;
            }
        }
    }

    return hit->region;
}

int BenchIllustration::pickupIndexFor (GuitarRegion r) const
{
    return pickupIndexOfRegion (r);
}

void BenchIllustration::select (GuitarRegion region, int stringIndex)
{
    selected = region;
    selectedTool = Tool::none;
    selectedString = region == GuitarRegion::strings || region == GuitarRegion::bridge || region == GuitarRegion::nut
                   ? stringIndex : -1;

    announceSelection();
    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

void BenchIllustration::selectTool (Tool tool)
{
    if (tool != Tool::none && ! hasTool (tool))
        return;

    selected = GuitarRegion::none;
    selectedString = -1;
    selectedTool = tool;

    announceSelection();
    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

void BenchIllustration::announceSelection()
{
    // Screen readers hear the part (guitar-illustration.md 16).
    const auto text = describeSelection();

    if (text.isNotEmpty())
        setDescription (text);
}

juce::String BenchIllustration::describeSelection() const
{
    if (selectedTool != Tool::none)
        return toolDescription (selectedTool);

    juce::String text;

    for (auto& h : scene.hits)
        if (h.region == selected)
            text = h.description;

    const auto& guitar = bench.current();

    if (selected == GuitarRegion::strings && juce::isPositiveAndBelow (selectedString, guitar.getStringCount()))
    {
        // One string: its gauge, winding and material, and whether it is overridden.
        text = tr ("workshop.a11y.string", { { "n", juce::String (selectedString + 1) },
                                              { "string", WorkshopBench::describeString (guitar, selectedString) },
                                              { "material", guitar.getStringMaterial (selectedString).replaceCharacter ('_', ' ') } });
        if (guitar.getStringOverride (selectedString) != nullptr)
            text << " " << tr ("workshop.a11y.stringOverridden");
    }
    else if (selected == GuitarRegion::nut && juce::isPositiveAndBelow (selectedString, guitar.getStringCount()))
    {
        const auto& depths = guitar.setup.nutSlotDepthsMm;
        text << " " << tr ("workshop.a11y.nutSlot", { { "n", juce::String (selectedString + 1) },
                                                     { "mm", num (juce::isPositiveAndBelow (selectedString, depths.size()) ? depths[selectedString] : 0.5, 2) } });
    }

    return text;
}

juce::String BenchIllustration::toolDescription (Tool tool) const
{
    if (tool == Tool::pick)
    {
        const auto part = bench.getAccessory (PartType::pick);
        return tr ("workshop.a11y.pick", { { "name", part != nullptr ? part->name : tr ("workshop.tool.currentPick") },
                                            { "mm", num (bench.getPickPositionMm(), 0) },
                                            { "deg", num (bench.getPickAngleDegrees(), 0) } });
    }

    if (tool == Tool::slide)
    {
        const auto part = bench.getAccessory (PartType::slide);
        return tr ("workshop.a11y.slide", { { "name", part != nullptr ? part->name : tr ("workshop.tool.defaultSlide") },
                                             { "fret", num (bench.getSlideFret(), 1) },
                                             { "deg", num (bench.getSlideSlantDegrees(), 0) } });
    }

    if (tool == Tool::capo)
    {
        const auto part = bench.getAccessory (PartType::capo);
        const auto name = part != nullptr ? part->name : tr ("workshop.tool.defaultCapo");
        const int fret = bench.getCapoFret();
        return fret > 0 ? tr ("workshop.a11y.capo", { { "name", name }, { "fret", juce::String (fret) } })
                        : tr ("workshop.a11y.capoOff", { { "name", name } });
    }

    return {};
}

//==============================================================================
bool BenchIllustration::hasTool (Tool tool) const
{
    if (tool == Tool::slide)
        return processor.getState().getRawParameterValue (ParamIDs::slideGuitar)->load() >= 0.5f;

    return tool == Tool::pick || tool == Tool::capo;
}

namespace
{
    /** Where string `s` is at a given X along the neck, mm. */
    juce::Point<float> pointOnString (const GuitarScene& scene, int s, float x)
    {
        if (! juce::isPositiveAndBelow (s, (int) scene.saddlePoints.size()))
            return { x, 0.0f };

        const auto a = scene.saddlePoints[(size_t) s], b = scene.nutPoints[(size_t) s];
        const float t = (x - a.x) / juce::jmax (1.0f, b.x - a.x);
        return { x, a.y + (b.y - a.y) * t };
    }

    /** A rounded bar between two points, `thickness` wide, `overhang` past each end, turned by `slantDegrees`. */
    juce::Path barBetween (juce::Point<float> a, juce::Point<float> b, float thickness, float overhang, float slantDegrees)
    {
        const auto centre = (a + b) * 0.5f;
        const float length = a.getDistanceFrom (b) + 2.0f * overhang;
        juce::Path bar;
        bar.addRoundedRectangle (-length * 0.5f, -thickness * 0.5f, length, thickness, thickness * 0.5f);
        const float along = std::atan2 (b.y - a.y, b.x - a.x);
        bar.applyTransform (juce::AffineTransform::rotation (along + juce::degreesToRadians (slantDegrees)).translated (centre));
        return bar;
    }
}

juce::Point<float> BenchIllustration::pickTipMm() const
{
    const int n = (int) scene.saddlePoints.size();
    const float x = (float) bench.getPickPositionMm();

    if (n == 0)
        return { x, 0.0f };

    return (pointOnString (scene, 0, x) + pointOnString (scene, n - 1, x)) * 0.5f;
}

juce::Path BenchIllustration::toolArea (Tool tool) const
{
    juce::Path area;
    const int n = (int) scene.saddlePoints.size();

    if (n == 0 || ! hasTool (tool))
        return area;

    if (tool == Tool::pick)
    {
        // A standard pick, 25 mm tip to back, 22 mm wide, its tip on the strings,
        // turned about the tip by the pick angle.
        const auto tip = pickTipMm();
        area.startNewSubPath (0.0f, 0.0f);
        area.quadraticTo (-13.0f, -14.0f, -11.0f, -22.0f);
        area.quadraticTo (0.0f, -28.0f, 11.0f, -22.0f);
        area.quadraticTo (13.0f, -14.0f, 0.0f, 0.0f);
        area.closeSubPath();
        area.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians ((float) bench.getPickAngleDegrees())).translated (tip));
        return area;
    }

    if (tool == Tool::slide)
    {
        const float fret = (float) bench.getSlideFret();
        const auto a = scene.stringAt (n - 1, fret), b = scene.stringAt (0, fret);
        const float diameter = (float) juce::jlimit (10.0, 30.0, bench.getSlideBar().diameterMm);
        return barBetween (a, b, diameter, 9.0f, (float) bench.getSlideSlantDegrees());
    }

    if (tool == Tool::capo)
    {
        const int fret = bench.getCapoFret();
        juce::Point<float> a, b;

        if (fret > 0)
        {
            // Just behind its fret, where a capo sits.
            a = scene.stringAt (n - 1, (float) fret - 0.35f);
            b = scene.stringAt (0, (float) fret - 0.35f);
        }
        else
        {
            // Parked on the headstock, past the nut.
            a = scene.nutPoints[(size_t) (n - 1)] + juce::Point<float> (28.0f, 0.0f);
            b = scene.nutPoints[0] + juce::Point<float> (28.0f, 0.0f);
        }

        area = barBetween (a, b, 9.0f, 7.0f, 0.0f);
        // The clamp's knob on the bass side.
        area.addEllipse (a.x - 5.0f, a.y - 14.0f, 10.0f, 10.0f);
        return area;
    }

    return area;
}

BenchIllustration::Tool BenchIllustration::toolAt (juce::Point<float> px) const
{
    const auto mm = toMm (px);

    // Layer order: the pick over the slide over the capo (guitar-illustration.md 5).
    for (auto tool : { Tool::pick, Tool::slide, Tool::capo })
        if (hasTool (tool) && toolArea (tool).contains (mm))
            return tool;

    return Tool::none;
}

int BenchIllustration::nutSlotAt (juce::Point<float> px) const
{
    // The nut itself, whatever sits over it in z-order: a slot is where the
    // string crosses the nut, and the strings' band reaches that far.
    const auto mm = toMm (px);
    bool onNut = false;

    for (auto& h : scene.hits)
        if (h.region == GuitarRegion::nut && h.area.contains (mm))
            onNut = true;

    if (! onNut)
        return -1;
    int best = -1;
    float distance = 1.0e9f;

    for (int s = 0; s < (int) scene.nutPoints.size(); ++s)
        if (std::abs (scene.nutPoints[(size_t) s].y - mm.y) < distance)
        {
            distance = std::abs (scene.nutPoints[(size_t) s].y - mm.y);
            best = s;
        }

    return best;
}

//==============================================================================
void BenchIllustration::paint (juce::Graphics& g)
{
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), Metrics::panelCorner);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (getLocalBounds().reduced (1));
        GuitarRenderer::paint (g, scene, mmToPx);

        // Overridden strings first (their colour), then the live layer, the
        // tools, and the outlines.
        paintStringOverrides (g);

        GuitarOverlay overlay = live;
        overlay.accent = Palette::accent;
        overlay.hovered = hovered;
        overlay.selected = selected;
        GuitarRenderer::paintOverlay (g, scene, mmToPx, overlay);

        paintTools (g);

        // A selected string, outlined along its length; a selected nut slot marked.
        if (selected == GuitarRegion::strings && juce::isPositiveAndBelow (selectedString, (int) scene.saddlePoints.size()))
        {
            g.setColour (Palette::accent);
            g.drawLine ({ toPx (scene.saddlePoints[(size_t) selectedString]), toPx (scene.nutPoints[(size_t) selectedString]) }, 2.0f);
        }
        else if (selected == GuitarRegion::nut && juce::isPositiveAndBelow (selectedString, (int) scene.nutPoints.size()))
        {
            const auto p = toPx (scene.nutPoints[(size_t) selectedString]);
            g.setColour (Palette::accent);
            g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.5f);
        }
    }

    // ---- the ruler: mm from the saddle, with the pickup rail (section 2) ------------------
    const float y = (float) getHeight() - 16.0f;
    const float x0 = toPx ({ 0.0f, 0.0f }).x;
    const float x250 = toPx ({ 250.0f, 0.0f }).x;

    g.setColour (Palette::edgeBright);
    g.drawLine (juce::jmin (x0, x250), y, juce::jmax (x0, x250), y, 1.0f);
    g.setFont (Fonts::mono (9.0f));

    for (int mm = 0; mm <= 250; mm += 10)
    {
        const float x = toPx ({ (float) mm, 0.0f }).x;
        const bool major = mm % 50 == 0;
        g.setColour (major ? Palette::textMuted : Palette::edgeBright);
        g.drawLine (x, y, x, y + (major ? 6.0f : 3.0f), 1.0f);

        if (major)
            g.drawText (juce::String (mm), juce::Rectangle<float> (x - 15.0f, y + 5.0f, 30.0f, 10.0f), juce::Justification::centred, false);
    }

    g.setColour (Palette::textMuted);
    g.drawText (tr ("workshop.ruler"), juce::Rectangle<float> (juce::jmax (x0, x250) + 6.0f, y - 5.0f, 90.0f, 10.0f),
                juce::Justification::centredLeft, false);

    // The pickup rail: where each pickup's centre sits.
    const auto& guitar = bench.current();

    for (int i = 0; i < 3; ++i)
    {
        if (guitar.get (WorkshopGuitar::pickupSlot (i)) == nullptr)
            continue;

        const float x = toPx ({ (float) guitar.placements[(size_t) i].positionMm, 0.0f }).x;
        juce::Path marker;
        marker.addTriangle (x - 4.0f, y - 7.0f, x + 4.0f, y - 7.0f, x, y - 1.0f);
        g.setColour (pickupIndexOfRegion (selected) == i ? Palette::accent : Palette::textMuted);
        g.fillPath (marker);

        if (pickupIndexOfRegion (selected) == i || drag == Drag::pickup)
            g.drawText (juce::String (guitar.placements[(size_t) i].positionMm, 1),
                        juce::Rectangle<float> (x - 20.0f, y - 20.0f, 40.0f, 10.0f), juce::Justification::centred, false);
    }

    if (bench.isAuditioning())
    {
        g.setColour (Palette::accent);
        g.setFont (Fonts::ui (11.0f, true));
        g.drawText (tr ("workshop.auditioningBanner"), getLocalBounds().reduced (10, 6), juce::Justification::topRight, false);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), Metrics::panelCorner, 1.5f);
    }
}

void BenchIllustration::paintStringOverrides (juce::Graphics& g) const
{
    // workshop-ui.md 3.3 / guitar-illustration.md 10: an overridden string is
    // drawn in its own material's colour and style, over the set's line.
    const auto& guitar = bench.current();
    const bool bass = guitar.family == "bass";
    const auto set = guitar.get (GuitarSlot::strings);
    const bool flat = set != nullptr && set->text ("winding", "round") == "flat";
    const float pxPerMm = GuitarRenderer::pxPerMm (mmToPx);

    for (const auto& o : guitar.stringOverrides)
    {
        const int s = o.stringIndex;

        if (! juce::isPositiveAndBelow (s, (int) scene.saddlePoints.size()))
            continue;

        const bool wound = guitar.isStringWound (s);
        const auto material = guitar.getStringMaterial (s);
        const auto colour = material == "nylon" ? juce::Colour (0xfff2e9d8) : GuitarRenderer::stringColour (material, wound, bass, flat);
        // Section 10's line widths: 2 px for bronze, 2.5 for a bass, 1.5 wound, 1 plain.
        const float floorPx = bass ? 2.5f : material.contains ("bronze") ? 2.0f : wound ? 1.5f : 1.0f;
        const float width = juce::jmax (floorPx, (float) guitar.getStringGaugeIn (s) * 25.4f * pxPerMm);

        const juce::Line<float> line (toPx (scene.saddlePoints[(size_t) s]), toPx (scene.nutPoints[(size_t) s]));
        g.setColour (colour);
        g.drawLine (line, width);

        if (wound && ! flat)
        {
            // The winding hint, as section 10 draws it.
            const float dashes[] = { 2.0f, 3.0f };
            g.setColour (colour.darker (0.35f));
            g.drawDashedLine (line, dashes, 2, juce::jmax (0.5f, width * 0.4f));
        }
    }
}

void BenchIllustration::paintTools (juce::Graphics& g) const
{
    // Layers 28 and 29 (guitar-illustration.md 5), drawn in millimetres.
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (mmToPx);
    const float pxPerMm = juce::jmax (0.01f, GuitarRenderer::pxPerMm (mmToPx));

    auto outline = [&] (Tool tool, const juce::Path& area)
    {
        if (selectedTool == tool)
        {
            g.setColour (Palette::accent);
            g.strokePath (area, juce::PathStrokeType (2.5f / pxPerMm));
        }
        else if (hoveredTool == tool)
        {
            g.setColour (Palette::accent.withAlpha (0.6f));
            g.strokePath (area, juce::PathStrokeType (1.5f / pxPerMm));
        }
    };

    if (hasTool (Tool::capo))
    {
        const auto area = toolArea (Tool::capo);
        const bool on = bench.getCapoFret() > 0;
        g.setColour (juce::Colour (0xff2b2b2b).withAlpha (on ? 0.92f : 0.55f));
        g.fillPath (area);
        g.setColour (juce::Colour (0xffb9bcc2).withAlpha (on ? 0.9f : 0.5f));
        g.strokePath (area, juce::PathStrokeType (0.6f));
        outline (Tool::capo, area);
    }

    if (hasTool (Tool::slide))
    {
        // slide-guitar.md 8: the bar in its material's colour at 80%.
        const auto area = toolArea (Tool::slide);
        g.setColour (juce::Colour (getSlideMaterial (bench.getSlideBar().material).colour).withAlpha (0.8f));
        g.fillPath (area);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.strokePath (area, juce::PathStrokeType (0.8f));
        outline (Tool::slide, area);
    }

    if (hasTool (Tool::pick))
    {
        const auto area = toolArea (Tool::pick);
        const auto part = bench.getAccessory (PartType::pick);
        g.setColour (pickColour (part != nullptr ? part->text ("material", "celluloid") : juce::String ("celluloid")).withAlpha (0.9f));
        g.fillPath (area);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.strokePath (area, juce::PathStrokeType (0.5f));
        outline (Tool::pick, area);
    }
}

//==============================================================================
void BenchIllustration::mouseMove (const juce::MouseEvent& e)
{
    // Section 3.1: hover outlines and names a part; it never selects. A tool
    // over the part takes the hover.
    const auto tool = toolAt (e.position);
    auto r = tool == Tool::none ? regionAt (e.position) : GuitarRegion::none;

    if (r == GuitarRegion::strings && nutSlotAt (e.position) >= 0)
        r = GuitarRegion::nut;

    if (r != hovered || tool != hoveredTool)
    {
        hovered = r;
        hoveredTool = tool;
        repaint();
    }

    juce::String tip;

    if (tool != Tool::none)
    {
        tip = toolDescription (tool);
        tip << "  " << tr (tool == Tool::pick ? "workshop.tip.pick" : tool == Tool::slide ? "workshop.tip.slide" : "workshop.tip.capo");
    }
    else
    {
        for (auto& h : scene.hits)
            if (h.region == r)
                tip = h.description;

        if (pickupIndexOfRegion (r) >= 0)
            tip << "  " << tr ("workshop.tip.pickup");
        else if (r == GuitarRegion::bridge)
            tip << "  " << tr ("workshop.tip.saddle");
        else if (r == GuitarRegion::nut)
            tip << "  " << tr ("workshop.tip.nutSlot");
        else if (r == GuitarRegion::strings)
            tip << "  " << tr ("workshop.tip.strings");
    }

    const bool sideways = pickupIndexOfRegion (r) >= 0 || tool == Tool::pick || tool == Tool::slide || tool == Tool::capo;
    setMouseCursor (sideways ? juce::MouseCursor::LeftRightResizeCursor
                  : r == GuitarRegion::nut ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    setHelpText (tip);
}

void BenchIllustration::mouseExit (const juce::MouseEvent&)
{
    hovered = GuitarRegion::none;
    hoveredTool = Tool::none;
    repaint();
}

void BenchIllustration::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (e.mods.isMiddleButtonDown() || (e.mods.isRightButtonDown() && zoom > 1.0f))
    {
        drag = Drag::pan;
        dragStartMm = e.position;
        dragStartValue = 0.0;
        return;
    }

    // The tools sit over every part (guitar-illustration.md 5's layers 28, 29).
    if (const auto tool = toolAt (e.position); tool != Tool::none)
    {
        selectTool (tool);
        const auto mm = toMm (e.position);
        dragStartMm = mm;
        bench.beginGesture();

        if (tool == Tool::pick)
        {
            // Near the back corners turns the pick; anywhere else moves it.
            const auto tip = pickTipMm();
            const bool corner = tip.getDistanceFrom (mm) > 18.0f;
            drag = corner ? Drag::pickAngle : Drag::pick;
            dragStartValue = bench.getPickPositionMm();
            dragStartValue2 = bench.getPickAngleDegrees();
        }
        else if (tool == Tool::slide)
        {
            // An end of the bar turns it; the middle moves it.
            const auto area = toolArea (Tool::slide).getBounds();
            const auto centre = area.getCentre();
            const float halfLength = juce::jmax (area.getWidth(), area.getHeight()) * 0.5f;
            drag = centre.getDistanceFrom (mm) > halfLength * 0.55f ? Drag::slideSlant : Drag::slide;
            dragStartValue = nutDistanceFromFret (bench.getSlideFret(), scene.scaleMm);
            dragStartValue2 = bench.getSlideSlantDegrees();
        }
        else
        {
            drag = Drag::capo;
            dragStartValue = (double) bench.getCapoFret();
        }

        return;
    }

    int stringIndex = -1;
    auto r = regionAt (e.position, &stringIndex);

    // Section 13.1: Shift-click reaches the part under the topmost one.
    if (e.mods.isShiftDown())
    {
        const auto mm = toMm (e.position);
        const GuitarScene::Hit* top = nullptr;
        const GuitarScene::Hit* below = nullptr;

        for (auto& h : scene.hits)
            if (h.area.contains (mm))
            {
                below = top;
                top = &h;
            }

        if (below != nullptr)
            r = below->region;
    }

    // A click on the bridge picks the nearest string's saddle.
    if (r == GuitarRegion::bridge)
    {
        const auto mm = toMm (e.position);
        float best = 1.0e9f;

        for (int s = 0; s < (int) scene.saddlePoints.size(); ++s)
            if (std::abs (scene.saddlePoints[(size_t) s].y - mm.y) < best)
            {
                best = std::abs (scene.saddlePoints[(size_t) s].y - mm.y);
                stringIndex = s;
            }
    }

    // A click on the nut picks the nearest string's slot (section 4: nut slots
    // drag down per string), even where a string's own band lies over the nut.
    if (const int slot = nutSlotAt (e.position); slot >= 0 && (r == GuitarRegion::nut || r == GuitarRegion::strings))
    {
        r = GuitarRegion::nut;
        stringIndex = slot;
    }

    select (r, stringIndex);

    const int pickup = pickupIndexOfRegion (r);

    if (pickup >= 0 && ! e.mods.isShiftDown())
    {
        drag = Drag::pickup;
        dragIndex = pickup;
        dragStartMm = toMm (e.position);
        dragStartValue = bench.current().placements[(size_t) pickup].positionMm;
        bench.beginGesture();
    }
    else if (r == GuitarRegion::bridge && stringIndex >= 0)
    {
        drag = Drag::saddle;
        dragIndex = stringIndex;
        dragStartMm = toMm (e.position);
        const auto& values = bench.current().setup.intonationMm;
        dragStartValue = juce::isPositiveAndBelow (stringIndex, values.size()) ? values[stringIndex] : 0.0;
        bench.beginGesture();
    }
    else if (r == GuitarRegion::nut && stringIndex >= 0)
    {
        drag = Drag::nutSlot;
        dragIndex = stringIndex;
        dragStartMm = toMm (e.position);
        const auto& values = bench.current().setup.nutSlotDepthsMm;
        dragStartValue = juce::isPositiveAndBelow (stringIndex, values.size()) ? values[stringIndex] : 0.5;
        bench.beginGesture();
    }
}

void BenchIllustration::mouseDrag (const juce::MouseEvent& e)
{
    const bool fine = e.mods.isShiftDown(), free = e.mods.isAltDown();

    if (drag == Drag::pan)
    {
        panPx += e.position - dragStartMm;
        dragStartMm = e.position;
        resized();
        repaint();
        return;
    }

    const auto mm = toMm (e.position);

    if (drag == Drag::pickup)
    {
        // Constrained to the string axis (section 4): only X counts.
        const double target = WorkshopBench::snap (dragStartValue + (double) (mm.x - dragStartMm.x), fine, free);
        juce::String why;
        const double landed = bench.movePickup (dragIndex, target, &why);

        if (why.isNotEmpty() && onLimit)
            onLimit ("The " + juce::String (dragIndex == 0 ? "neck" : dragIndex == 1 ? "middle" : "bridge")
                     + " pickup stops here: " + why + ".");

        if (onPickupDragged)
            onPickupDragged (dragIndex, landed);

        rebuild (false);
    }
    else if (drag == Drag::saddle)
    {
        // Pulling the saddle back (away from the nut) adds compensation.
        const double target = WorkshopBench::snap (dragStartValue - (double) (mm.x - dragStartMm.x), fine, free, 0.1);
        bench.setIntonation (dragIndex, target);
        rebuild (false);
    }
    else if (drag == Drag::nutSlot)
    {
        // Down on screen (toward the treble side) cuts the slot deeper: a
        // millimetre of travel is a tenth of a millimetre of depth, snapped to 0.05.
        const double target = WorkshopBench::snap (dragStartValue + 0.1 * (double) (mm.y - dragStartMm.y), fine, free, 0.05);
        bench.setNutSlotDepth (dragIndex, target);
        announceSelection();
        repaint();
    }
    else if (drag == Drag::pick)
    {
        const double target = WorkshopBench::snap (dragStartValue + (double) (mm.x - dragStartMm.x), fine, free);
        bench.setPickPlacement (target, dragStartValue2);
        announceSelection();
        repaint();
    }
    else if (drag == Drag::pickAngle)
    {
        // A millimetre of travel across the strings is a degree, snapped to 1 (Shift 0.1).
        const double target = WorkshopBench::snap (dragStartValue2 + (double) (mm.y - dragStartMm.y), fine, free);
        bench.setPickPlacement (dragStartValue, target);
        announceSelection();
        repaint();
    }
    else if (drag == Drag::slide)
    {
        // Along the string in millimetres (snapped 1 mm), shown as a fret position.
        const double fromNut = WorkshopBench::snap (dragStartValue - (double) (mm.x - dragStartMm.x), fine, free);
        bench.setSlidePlacement (fretFromNutDistance (fromNut, scene.scaleMm), dragStartValue2);
        announceSelection();
        repaint();
    }
    else if (drag == Drag::slideSlant)
    {
        const double target = WorkshopBench::snap (dragStartValue2 + (double) (mm.y - dragStartMm.y), fine, free);
        bench.setSlidePlacement (bench.getSlideFret(), target);
        announceSelection();
        repaint();
    }
    else if (drag == Drag::capo)
    {
        // Whole frets along the neck; past the nut it comes off.
        const int n = (int) scene.nutPoints.size();
        const float nutX = n > 0 ? scene.nutPoints[0].x : (float) scene.scaleMm;
        const double fromNut = nutX - mm.x;
        const int fret = fromNut < -6.0 ? 0 : juce::jlimit (1, WorkshopBench::kMaxCapoFret,
                                                             juce::roundToInt (fretFromNutDistance (juce::jmax (0.0, fromNut), scene.scaleMm) + 0.35));
        bench.setCapoFret (fret);
        announceSelection();
        repaint();
    }
}

void BenchIllustration::mouseUp (const juce::MouseEvent&)
{
    if (drag != Drag::none && drag != Drag::pan)
        bench.endGesture();

    if (drag == Drag::pickup && onPickupDragged)
        onPickupDragged (-1, 0.0);

    drag = Drag::none;
    dragIndex = -1;
    rebuild (false);
}

void BenchIllustration::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Ctrl-scroll zooms up to 4x about the pointer (guitar-illustration.md 1).
    if (e.mods.isCommandDown())
    {
        const float before = zoom;
        zoom = juce::jlimit (1.0f, 4.0f, zoom * (wheel.deltaY > 0 ? 1.15f : 1.0f / 1.15f));

        if (zoom <= 1.0f)
            panPx = {};
        else if (before != zoom)
        {
            // Keep the millimetre under the pointer where it was.
            const auto under = toMm (e.position);
            resized();
            panPx += e.position - toPx (under);
        }

        resized();
        repaint();
        return;
    }

    // Scroll over a pickup raises or lowers it: 0.1 mm a notch (section 4).
    const int pickup = pickupIndexOfRegion (regionAt (e.position));

    if (pickup < 0 || bench.current().get (WorkshopGuitar::pickupSlot (pickup)) == nullptr)
        return;

    const auto& p = bench.current().placements[(size_t) pickup];
    const double step = wheel.deltaY > 0 ? -0.1 : 0.1;   // up raises: less distance to the strings
    const bool trebleOnly = e.mods.isShiftDown(), bassOnly = e.mods.isAltDown();

    bench.setPickupHeights (pickup,
                            bassOnly ? p.heightTrebleMm : p.heightTrebleMm + step,
                            trebleOnly ? p.heightBassMm : p.heightBassMm + step);
    select (pickup == 0 ? GuitarRegion::pickupNeck : pickup == 1 ? GuitarRegion::pickupMiddle : GuitarRegion::pickupBridge);
    rebuild (false);
}

bool BenchIllustration::keyPressed (const juce::KeyPress& key)
{
    // Section 10: Tab walks the parts in the builder's order; arrows nudge.
    if (key.getKeyCode() == juce::KeyPress::tabKey)
    {
        // The parts, then the tools on the bench (pick, slide, capo).
        struct Stop { GuitarRegion region; Tool tool; };
        std::vector<Stop> present;

        for (auto r : builderOrder())
            for (auto& h : scene.hits)
                if (h.region == r)
                {
                    present.push_back ({ r, Tool::none });
                    break;
                }

        for (auto t : { Tool::pick, Tool::slide, Tool::capo })
            if (hasTool (t))
                present.push_back ({ GuitarRegion::none, t });

        if (present.empty())
            return false;

        int index = -1;
        for (size_t i = 0; i < present.size(); ++i)
            if ((selectedTool != Tool::none && present[i].tool == selectedTool)
                || (selectedTool == Tool::none && selected != GuitarRegion::none && present[i].region == selected))
                index = (int) i;

        index = key.getModifiers().isShiftDown() ? (index <= 0 ? (int) present.size() - 1 : index - 1)
                                                 : (index + 1) % (int) present.size();

        const auto& stop = present[(size_t) index];

        if (stop.tool != Tool::none)
            selectTool (stop.tool);
        else
            select (stop.region, selected == GuitarRegion::strings ? juce::jmax (0, selectedString) : 0);

        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::escapeKey && (selected != GuitarRegion::none || selectedTool != Tool::none))
    {
        select (GuitarRegion::none);
        return true;
    }

    const bool fine = key.getModifiers().isShiftDown();
    const int pickup = pickupIndexOfRegion (selected);
    const bool left = key.getKeyCode() == juce::KeyPress::leftKey, right = key.getKeyCode() == juce::KeyPress::rightKey;
    const bool up = key.getKeyCode() == juce::KeyPress::upKey, down = key.getKeyCode() == juce::KeyPress::downKey;

    if (selectedTool != Tool::none && (left || right || up || down))
    {
        // Left on screen is toward the headstock; up and down turn or lift.
        const double step = fine ? 0.1 : 1.0;

        if (selectedTool == Tool::pick)
        {
            if (left || right)
                bench.setPickPlacement (bench.getPickPositionMm() + (left ? step : -step), bench.getPickAngleDegrees());
            else
                bench.setPickPlacement (bench.getPickPositionMm(), bench.getPickAngleDegrees() + (up ? step : -step));
        }
        else if (selectedTool == Tool::slide)
        {
            if (left || right)
            {
                const double fromNut = nutDistanceFromFret (bench.getSlideFret(), scene.scaleMm) + (left ? -step : step);
                bench.setSlidePlacement (fretFromNutDistance (juce::jmax (0.0, fromNut), scene.scaleMm), bench.getSlideSlantDegrees());
            }
            else
                bench.setSlidePlacement (bench.getSlideFret(), bench.getSlideSlantDegrees() + (up ? step : -step));
        }
        else if (selectedTool == Tool::capo && (left || right))
        {
            bench.setCapoFret (bench.getCapoFret() + (left ? -1 : 1));
        }

        announceSelection();
        repaint();
        return true;
    }

    if (selected == GuitarRegion::nut && selectedString >= 0 && (up || down))
    {
        // Down cuts the slot deeper, in section 4's 0.05 mm steps (Shift: 0.005).
        const auto& values = bench.current().setup.nutSlotDepthsMm;
        const double from = juce::isPositiveAndBelow (selectedString, values.size()) ? values[selectedString] : 0.5;
        bench.setNutSlotDepth (selectedString, from + (down ? 0.05 : -0.05) * (fine ? 0.1 : 1.0));
        announceSelection();
        repaint();
        return true;
    }

    if (selected == GuitarRegion::nut && (left || right))
    {
        const int n = (int) scene.nutPoints.size();
        select (GuitarRegion::nut, juce::jlimit (0, n - 1, selectedString + (right ? -1 : 1)));
        return true;
    }

    if (pickup >= 0 && (left || right))
    {
        // Left on screen is toward the headstock: further from the saddle.
        const double step = fine ? 0.1 : 1.0;
        const double from = bench.current().placements[(size_t) pickup].positionMm;
        juce::String why;
        bench.movePickup (pickup, from + (left ? step : -step), &why);

        if (why.isNotEmpty() && onLimit)
            onLimit ("It stops here: " + why + ".");

        rebuild (false);
        return true;
    }

    if (pickup >= 0 && (up || down))
    {
        const auto& p = bench.current().placements[(size_t) pickup];
        const double step = up ? -0.1 : 0.1;
        bench.setPickupHeights (pickup, p.heightTrebleMm + step, p.heightBassMm + step);
        rebuild (false);
        return true;
    }

    if (selected == GuitarRegion::bridge && selectedString >= 0 && (left || right))
    {
        const auto& values = bench.current().setup.intonationMm;
        const double from = juce::isPositiveAndBelow (selectedString, values.size()) ? values[selectedString] : 0.0;
        bench.setIntonation (selectedString, from + (right ? 0.1 : -0.1) * (fine ? 0.1 : 1.0));
        rebuild (false);
        return true;
    }

    if (selected == GuitarRegion::strings && (up || down))
    {
        const int n = (int) scene.saddlePoints.size();
        select (GuitarRegion::strings, juce::jlimit (0, n - 1, selectedString + (down ? -1 : 1)));
        return true;
    }

    return false;
}

//==============================================================================
//  WorkshopPanel
//==============================================================================
namespace
{
    struct Category
    {
        const char* name;
        PartType type;
    };

    // Section 1's drawer. Preamp shows the wiring parts that are active.
    // "Guitar" is the family selector (guitar-illustration.md 12.1), not a part type.
    const Category kCategories[] = {
        { "Guitar", PartType::numTypes },
        { "Body", PartType::body },         { "Neck", PartType::neck },       { "Frets", PartType::frets },
        { "Nut", PartType::nut },           { "Bridge", PartType::bridge },   { "Tuners", PartType::tuners },
        { "Strings", PartType::strings },   { "Pickups", PartType::pickup },  { "Wiring", PartType::wiring },
        { "Preamp", PartType::wiring },     { "Pick", PartType::pick },       { "Slide", PartType::slide },
        { "Capo", PartType::capo }
    };

    const Category* findCategory (const juce::String& name)
    {
        for (auto& c : kCategories)
            if (name == c.name)
                return &c;
        return nullptr;
    }

    /** One line a card shows under the part's name. */
    juce::String summaryOf (const Part& p)
    {
        // juce::String (v, 0) would print every digit; whole numbers are rounded instead.
        auto num = [&p] (const char* f, int dp) { return dp == 0 ? juce::String (juce::roundToInt (p.number (f, 0.0)))
                                                                   : juce::String (p.number (f, 0.0), dp); };

        switch (p.type)
        {
            case PartType::pickup:   return p.text ("family").replaceCharacter ('_', ' ') + ", " + num ("dc_resistance_k", 1) + "k, " + p.text ("magnet");
            case PartType::bridge:   return p.text ("type").replaceCharacter ('_', ' ') + ", " + num ("mass_g", 0) + " g";
            case PartType::strings:  return p.text ("winding_material").replaceCharacter ('_', ' ') + ", " + p.text ("winding") + " wound";
            case PartType::body:     return p.text ("wood").replaceCharacter ('_', ' ') + ", " + p.text ("chambering");
            case PartType::neck:     return p.text ("wood").replaceCharacter ('_', ' ') + ", " + num ("scale_length_mm", 0) + " mm, " + p.text ("joint");
            case PartType::wiring:   return p.text ("switching").replaceCharacter ('_', ' ') + (p.flag ("active") ? ", active" : ", passive");
            case PartType::nut:      return p.text ("material") + ", " + num ("width_mm", 1) + " mm";
            case PartType::frets:    return p.text ("material").replaceCharacter ('_', ' ') + ", " + num ("height_mm", 2) + " mm";
            case PartType::tuners:   return juce::String (juce::roundToInt (p.number ("ratio", 15.0))) + ":1" + (p.flag ("locking") ? ", locking" : "");
            case PartType::pick:     return p.text ("material") + ", " + num ("thickness_mm", 2) + " mm";
            case PartType::slide:    return p.text ("material") + ", " + num ("mass_g", 0) + " g";
            case PartType::capo:     return p.text ("type") + ", pressure " + num ("pressure", 2);
            case PartType::numTypes: return "Rebuild as a " + p.text ("family") + " guitar (12.1)";
            case PartType::top:
            case PartType::fretboard:
            case PartType::tailpiece:
            case PartType::pickguard: break;
        }

        return p.text ("wood", p.text ("material", p.text ("type")));
    }

    /** A field value as the inspector shows it. */
    juce::String fieldText (const juce::var& v)
    {
        if (v.isArray())
        {
            juce::StringArray items;
            for (auto& x : *v.getArray())
                items.add (x.toString());
            return "[" + items.joinIntoString (", ") + "]";
        }

        return v.toString();
    }
}

juce::Array<RangeFamily> WorkshopPanel::rangeFamilies()
{
    // The setup strip's action, relief and nut depths are fret-buzz.md 7's
    // parameters, in the buzz family (PhysicalRange.cpp).
    return { RangeFamily::buzz };
}

juce::String WorkshopPanel::categoryIdOfButton (int index) const
{
    return juce::isPositiveAndBelow (index, (int) std::size (kCategories)) ? juce::String (kCategories[index].name) : juce::String();
}

WorkshopPanel::WorkshopPanel (LuthierAudioProcessor& p)
    : processor (p), bench (p.getBench()), illustration (p)
{
    setTitle (tr ("workshop.title"));
    setDescription (tr ("workshop.description"));

    addAndMakeVisible (title);
    title.setText (tr ("workshop.heading"), juce::dontSendNotification);
    title.setFont (Fonts::display (20.0f));
    title.setColour (juce::Label::textColourId, Palette::textPrimary);

    // onboarding 9: hidden until owed; it takes a row under the header.
    addChildComponent (firstEncounterHint);
    firstEncounterHint.onShownOrDismissed = [this] { resized(); };

    addAndMakeVisible (guitarName);
    guitarName.setFont (Fonts::ui (13.0f, true));
    guitarName.setColour (juce::Label::textColourId, Palette::textMuted);

    addAndMakeVisible (saveAsButton);
    saveAsButton.setButtonText (tr ("workshop.saveAsGuitar"));
    saveAsButton.setTooltip (tr ("workshop.saveAsGuitar.tip"));
    AccessibleSetup::configureButton (saveAsButton, tr ("workshop.saveAsGuitar"), tr ("workshop.saveAsGuitar.tip"));
    saveAsButton.onClick = [this]
    {
        if (onSaveAsGuitar)
            onSaveAsGuitar();
    };

    // Section 7: eight A/B slots. Click stores into an empty slot and recalls a
    // full one; Shift-click clears.
    for (int i = 0; i < WorkshopBench::kNumSlots; ++i)
    {
        auto* b = slotButtons.add (new juce::TextButton (WorkshopBench::slotName (i)));
        addAndMakeVisible (b);
        b->setClickingTogglesState (false);
        AccessibleSetup::configureButton (*b, tr ("workshop.slot.name", { { "slot", WorkshopBench::slotName (i) } }));
        b->onClick = [this, i]
        {
            if (juce::ModifierKeys::currentModifiers.isShiftDown())
                bench.clearSlot (i);
            else if (bench.hasSlot (i))
                bench.recallSlot (i);
            else
                bench.storeSlot (i);

            refreshAll();
        };
    }

    for (auto& c : kCategories)
    {
        // The id stays English (showCategory takes it); the text is the locale's.
        const auto key = "workshop.category." + juce::String (c.name).toLowerCase();
        auto* b = categoryButtons.add (new juce::TextButton (tr (key)));
        addAndMakeVisible (b);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (0x57);
        b->setTooltip (tr ("workshop.category.tip", { { "category", tr (key) } }));
        AccessibleSetup::configureButton (*b, tr (key), tr ("workshop.category.tip", { { "category", tr (key) } }));
        b->onClick = [this, name = juce::String (c.name)] { showCategory (name); };
    }

    addAndMakeVisible (illustration);
    illustration.onSelectionChanged = [this]
    {
        // Selecting a part shows its category in the drawer (section 5's Swap, done for you).
        const auto slot = slotForRegion (illustration.getSelected());

        if (slot != GuitarSlot::numSlots)
            for (auto& c : kCategories)
                if (c.type == getSlotPartType (slot) && juce::String (c.name) != "Preamp")
                {
                    showCategory (c.name);
                    break;
                }

        refreshInspector();
        repaint();
    };
    illustration.onPickupDragged = [this] (int index, double mm)
    {
        if (index >= 0)
            requestSpectrum (index, mm);
        refreshInspector();
    };
    illustration.onLimit = [this] (const juce::String& message)
    {
        limitMessage = message;
        limitShownAt = juce::Time::getMillisecondCounter();
        repaint();
    };

    addAndMakeVisible (swapButton);
    swapButton.setButtonText (tr ("workshop.swap"));
    swapButton.setTooltip (tr ("workshop.swap.tip"));
    AccessibleSetup::configureButton (swapButton, tr ("workshop.swap"), tr ("workshop.swap.tip"));
    swapButton.onClick = [this]
    {
        const auto slot = slotForRegion (illustration.getSelected());
        if (slot != GuitarSlot::numSlots)
            for (auto& c : kCategories)
                if (c.type == getSlotPartType (slot))
                {
                    showCategory (c.name);
                    break;
                }
    };

    addAndMakeVisible (revertButton);
    revertButton.setButtonText (tr ("workshop.revert"));
    revertButton.setTooltip (tr ("workshop.revert.tip"));
    AccessibleSetup::configureButton (revertButton, tr ("workshop.revert"), tr ("workshop.revert.tip"));
    revertButton.onClick = [this]
    {
        const auto slot = slotForRegion (illustration.getSelected());
        if (slot != GuitarSlot::numSlots)
            bench.revert (slot);
        refreshAll();
    };

    addAndMakeVisible (savePartButton);
    savePartButton.setButtonText (tr ("workshop.savePart"));
    savePartButton.setTooltip (tr ("workshop.savePart.tip"));
    AccessibleSetup::configureButton (savePartButton, tr ("workshop.savePart"), tr ("workshop.savePart.tip"));
    savePartButton.onClick = [this]
    {
        const auto slot = slotForRegion (illustration.getSelected());
        const auto part = slot != GuitarSlot::numSlots ? bench.current().get (slot) : nullptr;

        if (part == nullptr)
            return;

        auto* window = new juce::AlertWindow (tr ("workshop.savePart"), tr ("workshop.savePart.prompt"), juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", tr ("workshop.savePart.defaultName", { { "name", part->name } }));
        window->addButton (tr ("common.save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton (tr ("common.cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window, slot] (int result)
        {
            if (result == 1)
                processor.savePartAs (slot, window->getTextEditorContents ("name"));
            refreshAll();
        }), true);
    };

    addAndMakeVisible (autoZoomToggle);
    autoZoomToggle.setButtonText (tr ("workshop.autoZoom"));
    autoZoomToggle.setTooltip (tr ("workshop.autoZoom.tip"));
    AccessibleSetup::configureButton (autoZoomToggle, tr ("workshop.autoZoom"), tr ("workshop.autoZoom.tip"));
    autoZoomToggle.onClick = [this] { autoZoom = autoZoomToggle.getToggleState(); repaint(); };

    // Section 10: the spectrum delta reaches a screen reader as its summary
    // sentence, not as a curve - a descriptive element under the painted pane,
    // and an announcement whenever the sentence changes.
    addAndMakeVisible (spectrumPane);
    spectrumPane.setInterceptsMouseClicks (false, false);
    AccessibleSetup::configureDescriptive (spectrumPane, tr ("workshop.spectrum"), tr ("workshop.spectrum.empty"));

    // ---- setup strip: the parameters fret-buzz.md 1 already has ------------------------
    actionTreble = std::make_unique<LuthierKnob> (tr ("workshop.setup.actionTreble"), LuthierKnob::Size::Small);
    actionBass   = std::make_unique<LuthierKnob> (tr ("workshop.setup.actionBass"), LuthierKnob::Size::Small);
    relief       = std::make_unique<LuthierKnob> (tr ("workshop.setup.relief"), LuthierKnob::Size::Small);

    actionTreble->attachTo (processor, ParamIDs::setupActionTreble, tr ("workshop.setup.actionTreble.tip"));
    actionBass->attachTo (processor, ParamIDs::setupActionBass, tr ("workshop.setup.actionBass.tip"));
    relief->attachTo (processor, ParamIDs::setupRelief, tr ("workshop.setup.relief.tip"));

    for (auto* k : { actionTreble.get(), actionBass.get(), relief.get() })
        addAndMakeVisible (k);

    for (int n = 1; n <= ParamIDs::kNumNutDepths; ++n)
    {
        auto* k = nutDepths.add (new LuthierKnob (tr ("workshop.setup.nut", { { "n", juce::String (n) } }), LuthierKnob::Size::Small));
        k->attachTo (processor, ParamIDs::setupNutDepth (n), tr ("workshop.setup.nut.tip", { { "n", juce::String (n) } }));
        addAndMakeVisible (k);
    }

    showCategory (category);
    refreshAll();
    startTimerHz (20);
}

WorkshopPanel::~WorkshopPanel()
{
    stopTimer();
    bench.endAudition();
}

juce::StringArray WorkshopPanel::drawerCategories()
{
    juce::StringArray names;
    for (auto& c : kCategories)
        names.add (c.name);
    return names;
}

void WorkshopPanel::showCategory (const juce::String& name)
{
    if (findCategory (name) == nullptr)
        return;

    category = name;

    for (int i = 0; i < categoryButtons.size(); ++i)
        categoryButtons[i]->setToggleState (categoryIdOfButton (i) == name, juce::dontSendNotification);

    refreshDrawer();
    repaint();
}

GuitarSlot WorkshopPanel::targetSlot() const
{
    const auto* c = findCategory (category);

    if (c == nullptr || c->type == PartType::pick || c->type == PartType::slide || c->type == PartType::capo)
        return GuitarSlot::numSlots;

    if (c->type == PartType::pickup)
    {
        // The selected pickup, else the first fitted one (neck first), else the neck slot.
        const int selected = pickupIndexOfRegion (illustration.getSelected());
        if (selected >= 0)
            return WorkshopGuitar::pickupSlot (selected);

        for (int i = 0; i < 3; ++i)
            if (bench.current().get (WorkshopGuitar::pickupSlot (i)) != nullptr)
                return WorkshopGuitar::pickupSlot (i);

        return GuitarSlot::pickupNeck;
    }

    for (int i = 0; i < kNumGuitarSlots; ++i)
        if (getSlotPartType ((GuitarSlot) i) == c->type)
            return (GuitarSlot) i;

    return GuitarSlot::numSlots;
}

void WorkshopPanel::refreshDrawer()
{
    drawerParts.clear();
    const auto* c = findCategory (category);

    if (c == nullptr)
        return;

    if (c->type == PartType::numTypes)
    {
        // One card per family; the card is a stand-in, never fitted as a part.
        for (auto family : { "electric", "acoustic", "classical", "bass", "resonator" })
        {
            auto card = std::make_shared<Part>();
            card->name = juce::String (family).substring (0, 1).toUpperCase() + juce::String (family).substring (1);
            card->type = PartType::numTypes;
            card->fields = juce::var (new juce::DynamicObject());
            card->fields.getDynamicObject()->setProperty ("family", juce::String (family));
            card->isFactory = true;
            drawerParts.add (card);
        }

        return;
    }

    for (const auto& p : processor.getPartLibrary().getParts (c->type))
    {
        if (juce::String (c->name) == "Preamp" && ! p->flag ("active"))
            continue;

        drawerParts.add (p);
    }

    // Factory first, then the user's (section 9's user section), each by name.
    std::stable_sort (drawerParts.begin(), drawerParts.end(), [] (const PartPtr& a, const PartPtr& b)
    {
        if (a->isFactory != b->isFactory)
            return a->isFactory;
        return a->name.compareNatural (b->name) < 0;
    });
}

bool WorkshopPanel::switchFamily (const juce::String& family, bool confirmed)
{
    if (family == processor.getCurrentGuitar().family)
        return false;

    if (! confirmed && ! familyConfirmedThisSession)
    {
        // 12.1's one-time question, the first time in a session.
        auto options = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon,
                           tr ("workshop.family.title"), tr ("workshop.family.message"),
                           tr ("workshop.family.change"), tr ("common.cancel"), this);

        juce::AlertWindow::showAsync (options, [safe = juce::Component::SafePointer<WorkshopPanel> (this), family] (int result)
        {
            if (safe != nullptr && result == 1)
            {
                safe->familyConfirmedThisSession = true;
                safe->switchFamily (family, true);
            }
        });

        return false;
    }

    familyConfirmedThisSession = true;
    const bool ok = processor.switchGuitarFamily (family);

    // The banner (12.1) lists what changed.
    for (const auto& notice : processor.takeGuitarNotices())
    {
        limitMessage = notice;
        limitShownAt = juce::Time::getMillisecondCounter();
    }

    refreshAll();
    return ok;
}

bool WorkshopPanel::editInspectorField (const juce::String& field, const juce::String& text)
{
    const auto slot = slotForRegion (illustration.getSelected());
    const auto trimmed = text.trim();

    if (field.isEmpty())
        return false;

    // The tools' placements (section 4's last three rows), in real units.
    if (const auto tool = illustration.getSelectedTool(); tool != BenchIllustration::Tool::none)
    {
        const double v = trimmed.getDoubleValue();
        bool ok = false;

        if (field == "pick_position_mm")       { bench.setPickPlacement (v, bench.getPickAngleDegrees()); ok = true; }
        else if (field == "pick_angle_deg")    { bench.setPickPlacement (bench.getPickPositionMm(), v); ok = true; }
        else if (field == "slide_fret")        { bench.setSlidePlacement (v, bench.getSlideSlantDegrees()); ok = true; }
        else if (field == "slide_slant_deg")   { bench.setSlidePlacement (bench.getSlideFret(), v); ok = true; }
        else if (field == "capo_fret")         { bench.setCapoFret (juce::roundToInt (v)); ok = true; }

        refreshAll();
        return ok;
    }

    // Section 3.3: one string's override. Setting a field back to the set's
    // value drops that part of the override; all three back drops it entirely.
    if (field.startsWith ("string_") && slot == GuitarSlot::strings)
    {
        const int index = illustration.getSelectedString();
        const auto& guitar = bench.current();

        if (! juce::isPositiveAndBelow (index, guitar.getStringCount()))
            return false;

        StringOverride o;
        o.stringIndex = index;
        if (const auto* existing = guitar.getStringOverride (index))
            o = *existing;

        const auto set = guitar.get (GuitarSlot::strings);
        const auto setGauges = set != nullptr ? set->numbers ("gauges_in") : juce::Array<double>();
        const double setGauge = juce::isPositiveAndBelow (index, setGauges.size()) ? setGauges[index] : 0.0;
        const auto setMaterial = set != nullptr ? set->text ("winding_material", "nickel_plated_steel") : juce::String ("nickel_plated_steel");

        if (field == "string_gauge_in")
        {
            const double g = trimmed.getDoubleValue();
            o.gaugeIn = g > 0.0 && std::abs (g - setGauge) > 1.0e-9 ? juce::jlimit (0.005, 0.2, g) : 0.0;
        }
        else if (field == "string_material")
        {
            const auto m = trimmed.toLowerCase().replaceCharacter (' ', '_');
            o.material = m.isNotEmpty() && m != setMaterial ? m : juce::String();
        }
        else if (field == "string_wound")
        {
            const auto w = trimmed.toLowerCase();
            const bool wound = w == "wound" || w == "true" || w == "1" || w == "yes";
            const bool plain = w == "plain" || w == "false" || w == "0" || w == "no";
            // What the set would decide for this string, override aside.
            auto plainGuitar = guitar;
            plainGuitar.clearStringOverride (index);
            o.wound = (wound || plain) && wound != plainGuitar.isStringWound (index) ? (wound ? 1 : 0) : -1;
        }
        else
            return false;

        const bool ok = bench.setStringOverride (o);
        refreshAll();
        return ok;
    }

    if (slot == GuitarSlot::numSlots)
        return false;

    // Numbers stay numbers, true/false stay flags, lists stay lists; anything else is text.
    juce::var value;

    if (trimmed.startsWithChar ('['))
        value = juce::JSON::parse (trimmed);
    else if (trimmed.equalsIgnoreCase ("true") || trimmed.equalsIgnoreCase ("false"))
        value = trimmed.equalsIgnoreCase ("true");
    else if (trimmed.containsOnly ("0123456789.-+eE") && trimmed.isNotEmpty())
        value = trimmed.getDoubleValue();
    else
        value = trimmed;

    const bool ok = bench.editField (slot, field, value);
    refreshAll();
    return ok;
}

void WorkshopPanel::clickCard (int index)
{
    if (! juce::isPositiveAndBelow (index, drawerParts.size()))
        return;

    const auto part = drawerParts[index];
    bench.endAudition();
    auditioning = false;

    if (part->type == PartType::numTypes)
    {
        switchFamily (part->text ("family"), false);
        return;
    }

    if (part->type == PartType::pick || part->type == PartType::slide || part->type == PartType::capo)
    {
        // Section 9: a slide only fits in Slide Mode.
        if (part->type == PartType::slide
            && processor.getState().getRawParameterValue (ParamIDs::slideGuitar)->load() < 0.5f)
        {
            limitMessage = tr ("workshop.slideNeedsSlideMode");
            limitShownAt = juce::Time::getMillisecondCounter();
            repaint();
            return;
        }

        bench.fitAccessory (part);
        illustration.selectTool (part->type == PartType::pick ? BenchIllustration::Tool::pick
                               : part->type == PartType::slide ? BenchIllustration::Tool::slide : BenchIllustration::Tool::capo);
    }
    else if (const auto slot = targetSlot(); slot != GuitarSlot::numSlots)
    {
        bench.fit (slot, part);
    }

    refreshAll();
}

void WorkshopPanel::hoverCard (int index, bool altDown)
{
    // Section 3.2: Alt-hover auditions on a shadow guitar; moving off or
    // releasing Alt goes back. Accessories have nothing to audition on the guitar.
    const bool wantAudition = altDown && juce::isPositiveAndBelow (index, drawerParts.size())
                           && targetSlot() != GuitarSlot::numSlots;

    if (index != hoveredCard || wantAudition != auditioning)
    {
        hoveredCard = index;

        if (wantAudition)
        {
            bench.beginAudition (targetSlot(), drawerParts[index]);
            auditioning = true;
            requestSpectrum();
        }
        else if (auditioning)
        {
            bench.endAudition();
            auditioning = false;
        }

        illustration.refresh();
        refreshInspector();
        repaint();
    }
}

void WorkshopPanel::requestSpectrum (int pickupIndex, double positionMm)
{
    const auto& committed = processor.getCurrentGuitar();
    const auto* audition = bench.getAuditionGuitar();
    const auto& candidate = audition != nullptr ? *audition : bench.current();

    std::vector<float> notches;

    if (pickupIndex >= 0)
    {
        // Section 4: while dragging a pickup, the comb notches for the low string.
        const auto neck = candidate.get (GuitarSlot::neck);
        const double scale = neck != nullptr ? neck->number ("scale_length_mm", 648.0) : 648.0;
        const int lowest = processor.getEngine().getNumStrings() - 1;
        notches = SpectrumDelta::combNotches (positionMm, scale, processor.getEngine().getTuningEngine().getEffectiveOpenFrequency (lowest));
    }

    lastRequest = worker.request (committed, candidate, processor.getEngine().getGuitarType(), std::move (notches));
}

void WorkshopPanel::takeSpectrumResult (SpectrumDelta::Result&& result)
{
    spectrum = std::move (result);

    // Section 10: announced as a sentence, once per new sentence.
    if (spectrum.summary.isNotEmpty() && spectrum.summary != announcedSummary)
    {
        announcedSummary = spectrum.summary;
        AccessibleSetup::configureDescriptive (spectrumPane, tr ("workshop.spectrum"), spectrum.summary);
        juce::AccessibilityHandler::postAnnouncement (spectrum.summary, juce::AccessibilityHandler::AnnouncementPriority::medium);
    }

    repaint (spectrumArea);
}

bool WorkshopPanel::waitForSpectrum (int timeoutMs)
{
    const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

    while (juce::Time::getMillisecondCounter() < until)
    {
        SpectrumDelta::Result r;

        if (worker.takeResult (r))
        {
            takeSpectrumResult (std::move (r));
            if (spectrum.requestId == lastRequest)
                return true;
        }

        juce::Thread::sleep (5);
    }

    return false;
}

void WorkshopPanel::refreshAll()
{
    refreshHeader();
    refreshDrawer();
    refreshInspector();
    illustration.refresh();
    repaint();
}

void WorkshopPanel::refreshHeader()
{
    const auto& g = processor.getCurrentGuitar();
    guitarName.setText (bench.isModified() ? tr ("workshop.guitarModified", { { "name", g.name } }) : g.name, juce::dontSendNotification);

    for (int i = 0; i < slotButtons.size(); ++i)
    {
        auto* b = slotButtons[i];
        const bool filled = bench.hasSlot (i);
        b->setToggleState (filled, juce::dontSendNotification);
        b->setTooltip (tr (filled ? "workshop.slot.filled" : "workshop.slot.empty", { { "slot", WorkshopBench::slotName (i) } }));
    }
}

void WorkshopPanel::refreshInspector()
{
    inspectorLines.clear();
    inspectorFields.clear();

    const auto region = illustration.getSelected();
    const auto slot = slotForRegion (region);
    const auto* audition = bench.getAuditionGuitar();
    const auto& guitar = audition != nullptr ? *audition : bench.current();

    auto addField = [this] (const juce::String& line, const juce::String& field)
    {
        inspectorLines.add (line);
        while (inspectorFields.size() < inspectorLines.size() - 1)
            inspectorFields.add ({});
        inspectorFields.add (field);
    };

    // The tools: the pick, the slide bar, the capo (section 4's last rows).
    if (const auto tool = illustration.getSelectedTool(); tool != BenchIllustration::Tool::none)
    {
        const auto type = tool == BenchIllustration::Tool::pick ? PartType::pick
                        : tool == BenchIllustration::Tool::slide ? PartType::slide : PartType::capo;
        const auto part = bench.getAccessory (type);
        const auto what = tr (tool == BenchIllustration::Tool::pick ? "workshop.tool.pick"
                            : tool == BenchIllustration::Tool::slide ? "workshop.tool.slide" : "workshop.tool.capo");
        inspectorTitle = what + ": " + (part != nullptr ? part->name : tr ("workshop.tool.default"));

        if (tool == BenchIllustration::Tool::pick)
        {
            addField (tr ("workshop.field.pickPosition", { { "mm", num (bench.getPickPositionMm(), 0) } }), "pick_position_mm");
            addField (tr ("workshop.field.pickAngle", { { "deg", num (bench.getPickAngleDegrees(), 0) } }), "pick_angle_deg");
        }
        else if (tool == BenchIllustration::Tool::slide)
        {
            addField (tr ("workshop.field.slideFret", { { "fret", num (bench.getSlideFret(), 1) } }), "slide_fret");
            addField (tr ("workshop.field.slideSlant", { { "deg", num (bench.getSlideSlantDegrees(), 0) } }), "slide_slant_deg");
            inspectorLines.add (tr ("workshop.field.slidePositionNote"));
        }
        else
        {
            addField (tr ("workshop.field.capoFret", { { "fret", bench.getCapoFret() > 0 ? juce::String (bench.getCapoFret()) : tr ("common.off") } }), "capo_fret");
        }

        if (part != nullptr)
            if (auto* object = part->fields.getDynamicObject())
                for (auto& prop : object->getProperties())
                    inspectorLines.add (prop.name.toString().replaceCharacter ('_', ' ') + ": " + fieldText (prop.value));

        return;
    }

    if (slot == GuitarSlot::numSlots)
    {
        inspectorTitle = tr ("workshop.inspector.nothing");
        inspectorLines.add (tr ("workshop.inspector.nothing.hint"));
        return;
    }

    const auto part = guitar.get (slot);
    const auto label = WorkshopBench::describeSlot (slot);
    inspectorTitle = label.substring (0, 1).toUpperCase() + label.substring (1) + ": "
                   + (part != nullptr ? part->name : tr ("workshop.inspector.empty"));

    if (audition != nullptr)
        inspectorLines.add (tr ("workshop.inspector.auditioning"));

    if (part != nullptr)
    {
        inspectorLines.add (tr (part->isFactory ? "workshop.inspector.factory" : "workshop.inspector.user"));

        if (! part->suits (guitar.family))
            inspectorLines.add (tr ("workshop.inspector.unusual", { { "family", guitar.family } }));

        if (auto* object = part->fields.getDynamicObject())
            for (auto& prop : object->getProperties())
            {
                inspectorLines.add (prop.name.toString().replaceCharacter ('_', ' ') + ": " + fieldText (prop.value));

                while (inspectorFields.size() < inspectorLines.size() - 1)
                    inspectorFields.add ({});

                inspectorFields.add (prop.name.toString());
            }
    }

    if (const int i = pickupIndexOfRegion (region); i >= 0 && part != nullptr)
    {
        const auto& pl = guitar.placements[(size_t) i];
        const auto travel = bench.getPickupTravel (i);
        inspectorLines.add ("position: " + juce::String (pl.positionMm, 1) + " mm from the saddle ("
                            + juce::String (juce::roundToInt (travel.min)) + " - " + juce::String (juce::roundToInt (travel.max)) + ")");
        inspectorLines.add ("height: " + juce::String (pl.heightTrebleMm, 1) + " mm treble / "
                            + juce::String (pl.heightBassMm, 1) + " mm bass");
    }

    if (region == GuitarRegion::bridge || region == GuitarRegion::strings)
    {
        const int s = illustration.getSelectedString();

        if (s >= 0)
        {
            const auto& inton = guitar.setup.intonationMm;
            inspectorLines.add ("string " + juce::String (s + 1) + " saddle: "
                                + juce::String (juce::isPositiveAndBelow (s, inton.size()) ? inton[s] : 0.0, 1) + " mm");
        }

        // Section 3.3: the selected string's own fields, over the set's.
        if (region == GuitarRegion::strings && juce::isPositiveAndBelow (s, guitar.getStringCount()))
        {
            const auto n = juce::String (s + 1);
            const auto* o = guitar.getStringOverride (s);
            addField ("string " + n + " gauge in: " + juce::String (guitar.getStringGaugeIn (s), 3), "string_gauge_in");
            addField ("string " + n + " winding: " + tr (guitar.isStringWound (s) ? "workshop.string.wound" : "workshop.string.plain"), "string_wound");
            addField ("string " + n + " material: " + guitar.getStringMaterial (s).replaceCharacter ('_', ' '), "string_material");
            inspectorLines.add (o != nullptr ? tr ("workshop.string.overridden", { { "n", n } }) : tr ("workshop.string.fromSet", { { "n", n } }));
        }
    }

    if (region == GuitarRegion::nut)
    {
        juce::StringArray depths;
        for (auto d : guitar.setup.nutSlotDepthsMm)
            depths.add (juce::String (d, 2));
        inspectorLines.add ("slot depths: " + depths.joinIntoString (", ") + " mm");

        const int s = illustration.getSelectedString();
        if (juce::isPositiveAndBelow (s, guitar.setup.nutSlotDepthsMm.size()))
            inspectorLines.add ("string " + juce::String (s + 1) + " slot: " + juce::String (guitar.setup.nutSlotDepthsMm[s], 2) + " mm");
    }
}

//==============================================================================
void WorkshopPanel::showFirstEncounterHintIfDue()
{
    firstEncounterHint.showIfDue();
}

void WorkshopPanel::timerCallback()
{
    showFirstEncounterHintIfDue();

    const auto key = GuitarRenderer::keyFor (processor.getCurrentGuitar(), {});

    if (key != shownGuitarKey)
    {
        // A commit, an undo, a recall, a preset: show what the last change did.
        shownGuitarKey = key;
        refreshAll();
    }

    SpectrumDelta::Result r;

    if (worker.takeResult (r))
        takeSpectrumResult (std::move (r));

    // Alt let go without the mouse moving: the audition ends (section 3.2).
    if (auditioning && ! juce::ModifierKeys::currentModifiers.isAltDown())
        hoverCard (hoveredCard, false);

    if (limitMessage.isNotEmpty() && juce::Time::getMillisecondCounter() - limitShownAt > 3000)
    {
        limitMessage.clear();
        repaint();
    }
}

int WorkshopPanel::cardAt (juce::Point<int> p) const
{
    for (int i = 0; i < cardBounds.size(); ++i)
        if (cardBounds[i].contains (p))
            return i;
    return -1;
}

void WorkshopPanel::mouseMove (const juce::MouseEvent& e)
{
    const int card = cardAt (e.getPosition());
    hoverCard (card, e.mods.isAltDown());

    if (card >= 0)
    {
        const auto& p = *drawerParts[card];
        setHelpText (tr ("workshop.card.tip", { { "name", p.name }, { "summary", summaryOf (p) } }));
    }
}

void WorkshopPanel::mouseExit (const juce::MouseEvent&)
{
    hoverCard (-1, false);
}

void WorkshopPanel::mouseDown (const juce::MouseEvent& e)
{
    if (e.getNumberOfClicks() >= 2)
    {
        for (int i = 0; i < inspectorRows.size(); ++i)
        {
            if (! inspectorRows[i].contains (e.getPosition()) || inspectorFields[i].isEmpty())
                continue;

            editingField = inspectorFields[i];
            fieldEditor = std::make_unique<juce::TextEditor>();
            addAndMakeVisible (*fieldEditor);
            fieldEditor->setBounds (inspectorRows[i]);
            fieldEditor->setText (inspectorLines[i].fromFirstOccurrenceOf (": ", false, false), false);
            fieldEditor->selectAll();
            fieldEditor->grabKeyboardFocus();
            fieldEditor->onReturnKey = [this]
            {
                const auto text = fieldEditor->getText();
                const auto field = editingField;
                fieldEditor.reset();
                editInspectorField (field, text);
            };
            fieldEditor->onEscapeKey = [this] { fieldEditor.reset(); repaint(); };
            fieldEditor->onFocusLost = [this] { fieldEditor.reset(); repaint(); };
            return;
        }
    }

    if (const int card = cardAt (e.getPosition()); card >= 0)
        clickCard (card);
}

void WorkshopPanel::modifierKeysChanged (const juce::ModifierKeys& mods)
{
    hoverCard (hoveredCard, mods.isAltDown());
}

//==============================================================================
void WorkshopPanel::resized()
{
    auto area = getLocalBounds().reduced (Metrics::grid);

    headerArea = area.removeFromTop (32);
    {
        auto h = headerArea;
        title.setBounds (h.removeFromLeft (130));
        for (int i = slotButtons.size(); --i >= 0;)
        {
            slotButtons[i]->setBounds (h.removeFromRight (26).reduced (1, 3));
        }
        h.removeFromRight (Metrics::grid);
        saveAsButton.setBounds (h.removeFromRight (130).reduced (0, 3));
        guitarName.setBounds (h);
    }

    area.removeFromTop (Metrics::gridHalf);

    // onboarding 9: the first-session hint, under the header.
    if (firstEncounterHint.isVisible())
    {
        firstEncounterHint.setBounds (area.removeFromTop (FirstEncounterHint::kHeight));
        area.removeFromTop (Metrics::gridHalf);
    }

    // Section 1: the inspector down the right, the spectrum under it.
    const bool wide = area.getWidth() >= 900;
    auto right = area.removeFromRight (wide ? 240 : 190);
    area.removeFromRight (Metrics::grid);

    spectrumArea = right.removeFromBottom (juce::jmin (180, right.getHeight() / 3));
    spectrumPane.setBounds (spectrumArea);
    autoZoomToggle.setBounds (spectrumArea.getRight() - 96, spectrumArea.getY() + 2, 94, 20);
    right.removeFromBottom (Metrics::grid);

    inspectorArea = right;
    {
        auto buttons = inspectorArea.withTrimmedTop (inspectorArea.getHeight() - 28);
        const int w = buttons.getWidth() / 3;
        swapButton.setBounds (buttons.removeFromLeft (w).reduced (1, 2));
        revertButton.setBounds (buttons.removeFromLeft (w).reduced (1, 2));
        savePartButton.setBounds (buttons.reduced (1, 2));
    }

    setupArea = area.removeFromBottom (86);
    {
        auto s = setupArea.reduced (0, 2);
        const int knobW = juce::jmax (48, s.getWidth() / 9);
        for (auto* k : { actionTreble.get(), actionBass.get(), relief.get() })
            k->setBounds (s.removeFromLeft (knobW));
        for (auto* k : nutDepths)
            k->setBounds (s.removeFromLeft (knobW));
    }
    area.removeFromBottom (Metrics::gridHalf);

    drawerArea = area.removeFromBottom (juce::jmax (110, area.getHeight() / 3));
    {
        auto tabs = drawerArea.removeFromTop (24);
        const int w = tabs.getWidth() / juce::jmax (1, categoryButtons.size());
        for (auto* b : categoryButtons)
            b->setBounds (tabs.removeFromLeft (w).reduced (1, 0));
    }
    area.removeFromBottom (Metrics::gridHalf);

    illustrationArea = area;
    illustration.setBounds (illustrationArea);
}

void WorkshopPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    paintDrawer (g, drawerArea);
    paintInspector (g, inspectorArea);
    paintSpectrum (g, spectrumArea);

    LuthierLookAndFeel::drawSectionHeader (g, setupArea.withHeight (16).translated (0, -2), tr ("workshop.setup"));

    if (limitMessage.isNotEmpty())
    {
        auto r = illustrationArea.withHeight (22).reduced (8, 0).translated (0, 6);
        g.setColour (Palette::panelRaised.withAlpha (0.9f));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        g.setColour (Palette::warning);
        g.setFont (Fonts::ui (12.0f));
        g.drawText (limitMessage, r.reduced (8, 0), juce::Justification::centredLeft, true);
    }
}

void WorkshopPanel::paintDrawer (juce::Graphics& g, juce::Rectangle<int> area)
{
    cardBounds.clear();
    LuthierLookAndFeel::drawPanel (g, area.toFloat());

    auto inner = area.reduced (6);
    const auto slot = targetSlot();
    const auto fitted = slot != GuitarSlot::numSlots ? bench.current().get (slot) : nullptr;
    const auto* c = findCategory (category);
    const auto accessory = c != nullptr ? bench.getAccessory (c->type) : nullptr;

    const bool slideOff = category == "Slide"
                       && processor.getState().getRawParameterValue (ParamIDs::slideGuitar)->load() < 0.5f;

    if (slideOff)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (12.0f));
        g.drawText (tr ("workshop.slideNeedsSlideMode"), inner.removeFromTop (18), juce::Justification::centredLeft, false);
    }

    const int cardW = 168, cardH = 42;
    int x = inner.getX(), y = inner.getY();
    bool anyUser = false;

    for (int i = 0; i < drawerParts.size(); ++i)
    {
        const auto& part = *drawerParts[i];
        anyUser = anyUser || ! part.isFactory;

        if (x + cardW > inner.getRight())
        {
            x = inner.getX();
            y += cardH + 4;
        }

        const juce::Rectangle<int> card (x, y, cardW, cardH);
        x += cardW + 4;

        if (card.getBottom() > inner.getBottom())
            break;

        cardBounds.add (card);

        const bool isFitted = (fitted != nullptr && fitted->name == part.name) || (accessory != nullptr && accessory->name == part.name)
                           || (part.type == PartType::numTypes && part.text ("family") == bench.current().family);
        const bool hover = i == hoveredCard;

        g.setColour (isFitted ? Palette::accent.withAlpha (0.18f) : hover ? Palette::panelRaised.brighter (0.1f) : Palette::panelRaised);
        g.fillRoundedRectangle (card.toFloat(), 4.0f);
        g.setColour (isFitted ? Palette::accent : hover && auditioning ? Palette::secondary : Palette::edge);
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 4.0f, isFitted || (hover && auditioning) ? 1.5f : 1.0f);

        auto text = card.reduced (6, 3);
        g.setColour (slideOff ? Palette::textDisabled : Palette::textPrimary);
        g.setFont (Fonts::ui (12.0f, true));
        g.drawText (part.name, text.removeFromTop (18), juce::Justification::centredLeft, true);

        g.setFont (Fonts::ui (10.5f));
        g.setColour (Palette::textMuted);
        auto line = summaryOf (part);
        if (! part.suits (bench.current().family) && part.type != PartType::pick && part.type != PartType::slide
            && part.type != PartType::capo && part.type != PartType::numTypes)
            line = tr ("workshop.card.unusual", { { "summary", line } });
        if (! part.isFactory)
            line = tr ("workshop.card.yours", { { "summary", line } });
        g.drawText (line, text, juce::Justification::centredLeft, true);
    }

    // Section 9's empty user section.
    if (! anyUser && drawerParts.size() > 0 && y + cardH + 22 < inner.getBottom())
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (11.0f));
        g.drawText (tr ("workshop.drawer.noUserParts"),
                    juce::Rectangle<int> (inner.getX(), y + cardH + 6, inner.getWidth(), 16), juce::Justification::centredLeft, true);
    }

    if (drawerParts.isEmpty())
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (12.0f));
        g.drawText (tr ("workshop.drawer.none", { { "category", category.toLowerCase() } }), inner, juce::Justification::centred, true);
    }
}

void WorkshopPanel::paintInspector (juce::Graphics& g, juce::Rectangle<int> area)
{
    LuthierLookAndFeel::drawPanel (g, area.toFloat());
    auto inner = area.reduced (8).withTrimmedBottom (30);

    LuthierLookAndFeel::drawSectionHeader (g, inner.removeFromTop (22), tr ("workshop.inspector"));
    inner.removeFromTop (4);

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (13.0f, true));
    g.drawFittedText (inspectorTitle, inner.removeFromTop (34), juce::Justification::topLeft, 2);

    g.setFont (Fonts::ui (11.5f));
    inspectorRows.clearQuick();

    for (int i = 0; i < inspectorLines.size(); ++i)
    {
        if (inner.getHeight() < 14)
            break;

        const auto& line = inspectorLines[i];
        const auto row = inner.removeFromTop (16);
        inspectorRows.add (row);

        const bool note = line == tr ("workshop.inspector.auditioning") || line.startsWith ("Unusual");
        const bool editable = inspectorFields[i].isNotEmpty();
        g.setColour (note ? Palette::accent : editable ? Palette::textPrimary : Palette::textMuted);
        g.drawFittedText (line, row, juce::Justification::centredLeft, 1);
    }

    if (inspectorFields.joinIntoString ("").isNotEmpty() && inner.getHeight() > 16)
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (10.5f));
        g.drawFittedText (tr ("workshop.inspector.editHint"),
                          inner.removeFromTop (28), juce::Justification::topLeft, 2);
    }
}

void WorkshopPanel::paintSpectrum (juce::Graphics& g, juce::Rectangle<int> area)
{
    LuthierLookAndFeel::drawPanel (g, area.toFloat());
    auto inner = area.reduced (8);

    LuthierLookAndFeel::drawSectionHeader (g, inner.removeFromTop (20).withTrimmedRight (100), tr ("workshop.spectrum"));
    auto summaryArea = inner.removeFromBottom (28);
    auto plot = inner.reduced (0, 4).toFloat();

    // Section 6: +-12 dB by default so a small change looks small.
    const float range = autoZoom ? juce::jlimit (1.0f, 24.0f, spectrum.largestDb * 1.25f + 0.25f) : 12.0f;

    auto xOf = [&plot] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / 60.0f) / std::log (12000.0f / 60.0f); };
    auto yOf = [&plot, range] (float db) { return plot.getCentreY() - (juce::jlimit (-range, range, db) / range) * plot.getHeight() * 0.5f; };

    g.setColour (Palette::edge);
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) xOf (hz), plot.getY(), plot.getBottom());
    g.setColour (Palette::edgeBright);
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

    g.setFont (Fonts::mono (9.0f));
    g.setColour (Palette::textMuted);
    g.drawText ("+" + juce::String (juce::roundToInt (range)) + " dB", plot.withHeight (10.0f), juce::Justification::topLeft, false);
    g.drawText ("-" + juce::String (juce::roundToInt (range)) + " dB", plot.withTrimmedTop (plot.getHeight() - 10.0f), juce::Justification::bottomLeft, false);

    for (float f : spectrum.combNotchesHz)
    {
        g.setColour (Palette::secondary.withAlpha (0.5f));
        g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom());
    }

    if (! spectrum.deltaDb.empty())
    {
        juce::Path curve;
        for (size_t i = 0; i < spectrum.deltaDb.size(); ++i)
        {
            const juce::Point<float> p { xOf (spectrum.frequencies[i]), yOf (spectrum.deltaDb[i]) };
            if (i == 0) curve.startNewSubPath (p); else curve.lineTo (p);
        }

        g.setColour (Palette::accent);
        g.strokePath (curve, juce::PathStrokeType (1.6f));
    }

    g.setFont (Fonts::ui (11.0f));
    g.setColour (spectrum.noChange ? Palette::textMuted : Palette::textPrimary);
    g.drawFittedText (spectrum.summary.isNotEmpty() ? spectrum.summary : tr ("workshop.spectrum.empty"),
                      summaryArea, juce::Justification::centredLeft, 2);
}

} // namespace luthier
