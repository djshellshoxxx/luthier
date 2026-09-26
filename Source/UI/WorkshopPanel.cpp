#include "WorkshopPanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "RealismGroupsC.h"   // REALISM-C

namespace luthier
{

namespace
{
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
            case GuitarRegion::pick:          // the player's accessories are not slots
            case GuitarRegion::slideBar:
            case GuitarRegion::capo:
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
    setTitle ("Workshop guitar");
    setDescription ("The guitar on the bench. Tab walks the parts; arrow keys nudge the selected one.");
    rebuild (true);
    motion.startTimerHz (*this, 30);
}

BenchIllustration::~BenchIllustration()
{
    motion.stopTimer();
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

//==============================================================================
// The accessories on the bench (workshop-ui.md 2 and 4): the pick, the slide and
// the capo are the player's, kept as parameters, and drawn over the guitar.

double BenchIllustration::scaleMm() const
{
    const auto neck = bench.current().get (GuitarSlot::neck);
    return neck != nullptr ? neck->number ("scale_length_mm", 648.0) : 648.0;
}

double BenchIllustration::getParameterPlain (const char* id) const
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
        return p->convertFrom0to1 (p->getValue());
    return 0.0;
}

void BenchIllustration::setParameterPlain (const char* id, double plain)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) plain));
}

void BenchIllustration::beginParameterGesture (const char* id)
{
    // A drag is one undo entry (workshop-ui.md 8): the parameter's own gesture.
    if (auto* p = processor.getState().getParameter (id); p != nullptr && ! gestureIds.contains (id))
    {
        p->beginChangeGesture();
        gestureIds.add (id);
    }
}

void BenchIllustration::endParameterGestures()
{
    for (const auto& id : gestureIds)
        if (auto* p = processor.getState().getParameter (id))
            p->endChangeGesture();

    gestureIds.clear();
}

void BenchIllustration::setAuditionTint (float db, const juce::String& label)
{
    tintDb = db;
    tintLabel = label;
    tintActive = true;
    tintEndedMs = -1.0;
    repaint();
}

void BenchIllustration::endAuditionTint()
{
    if (! tintActive)
        return;

    tintActive = false;
    tintEndedMs = juce::Time::getMillisecondCounterHiRes();

    // Reduced motion: the label goes at once, with nothing fading.
    if (! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition))
    {
        tintEndedMs = -1.0;
        tintDb = 0.0f;
        tintLabel.clear();
    }

    repaint();
}

float BenchIllustration::getTintAlpha (double nowMs) const noexcept
{
    const float strength = juce::jlimit (0.0f, 0.3f, std::abs (tintDb) / 12.0f * 0.3f + (tintDb != 0.0f ? 0.05f : 0.0f));

    if (tintActive)
        return strength;

    if (tintEndedMs < 0.0)
        return 0.0f;

    const double t = (nowMs - tintEndedMs) / kTintFadeMs;
    return t >= 1.0 ? 0.0f : strength * (float) (1.0 - t);
}

void BenchIllustration::setPickShown (bool shown)
{
    if (pickShown != shown)
    {
        pickShown = shown;
        repaint();
    }
}

GuitarOverlay BenchIllustration::currentOverlay() const
{
    GuitarOverlay o;
    o.accent = Palette::accent;
    o.hovered = hovered;
    o.selected = selected;
    o.reducedMotion = ! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition);
    o.handles = true;

    auto& engine = processor.getEngine();
    o.capoFret = juce::roundToInt (getParameterPlain (ParamIDs::capoFret));
    o.capoMask = engine.getTuningEngine().getCapoStringMask();

    if (getParameterPlain (ParamIDs::slideGuitar) > 0.5)
    {
        const auto live = (float) engine.getSlideEngine().getOverlayFret();
        o.slideFret = live >= 0.0f ? live : slideRestFret;
        o.slideSlantDeg = (float) getParameterPlain (ParamIDs::slideSlant);
        o.slideColour = juce::Colour (getSlideMaterial (engine.getSlideEngine().getBar().material).colour);
    }

    if (pickShown || selected == GuitarRegion::pick)
    {
        o.pickPositionMm = (float) (getParameterPlain (ParamIDs::pluckPosition) * scaleMm());
        o.pickAngleDeg = (float) Parameters::pickAngleDegrees (getParameterPlain (ParamIDs::pickAngle));
        const auto pick = bench.getAccessory (PartType::pick);
        o.pickSizeMm = pick != nullptr && pick->text ("shape").containsIgnoreCase ("jazz") ? 25.0f : 30.0f;
    }

    return o;
}

GuitarRegion BenchIllustration::accessoryAt (juce::Point<float> px, bool* onHandle) const
{
    const auto mm = toMm (px);
    const auto o = currentOverlay();
    const float handleMm = 6.0f / juce::jmax (0.01f, std::sqrt (std::abs (mmToPx.getDeterminant())));

    if (onHandle != nullptr)
        *onHandle = false;

    // Topmost first: the pick, then the slide, then the capo.
    if (o.pickPositionMm >= 0.0f)
    {
        if (GuitarRenderer::pickHandle (scene, o.pickPositionMm, o.pickAngleDeg, o.pickSizeMm).getDistanceFrom (mm) <= handleMm)
        {
            if (onHandle != nullptr) *onHandle = true;
            return GuitarRegion::pick;
        }

        if (GuitarRenderer::pickPath (scene, o.pickPositionMm, o.pickAngleDeg, o.pickSizeMm).contains (mm))
            return GuitarRegion::pick;
    }

    if (o.slideFret >= 0.0f)
    {
        if (GuitarRenderer::slideHandle (scene, o.slideFret, o.slideSlantDeg).getDistanceFrom (mm) <= handleMm)
        {
            if (onHandle != nullptr) *onHandle = true;
            return GuitarRegion::slideBar;
        }

        if (GuitarRenderer::slidePath (scene, o.slideFret, o.slideSlantDeg).contains (mm))
            return GuitarRegion::slideBar;
    }

    if (o.capoFret > 0 && GuitarRenderer::capoPath (scene, o.capoFret, o.capoMask).contains (mm))
        return GuitarRegion::capo;

    return GuitarRegion::none;
}

juce::String BenchIllustration::describeAccessory (GuitarRegion region) const
{
    // guitar-illustration.md 16's sentences.
    const auto o = currentOverlay();

    if (region == GuitarRegion::pick)
    {
        const auto part = bench.getAccessory (PartType::pick);
        const auto name = part != nullptr ? part->name
                        : Parameters::pickMaterialNames()[juce::roundToInt (getParameterPlain (ParamIDs::pickMaterial))];
        return "Pick: " + name + " " + juce::String (Parameters::pickThicknessMm (getParameterPlain (ParamIDs::pickThickness)), 2)
             + " mm, " + (o.pickSizeMm < 28.0f ? "jazz" : "standard") + " shape, position "
             + juce::String (juce::roundToInt (getParameterPlain (ParamIDs::pluckPosition) * scaleMm()))
             + " mm from saddle, angle " + juce::String (juce::roundToInt (Parameters::pickAngleDegrees (getParameterPlain (ParamIDs::pickAngle))))
             + " degrees.";
    }

    if (region == GuitarRegion::slideBar)
    {
        const auto& bar = processor.getEngine().getSlideEngine().getBar();
        const double pressure = getParameterPlain (ParamIDs::slidePressure);
        return "Slide: " + juce::String (getSlideMaterial (bar.material).name).toLowerCase() + " "
             + juce::String (juce::roundToInt (bar.diameterMm)) + " mm; position fret " + juce::String (o.slideFret, 1)
             + ", slant " + juce::String (juce::roundToInt (o.slideSlantDeg)) + " degrees, pressure "
             + (pressure < 0.35 ? "light" : pressure > 0.75 ? "heavy" : "normal") + ".";
    }

    if (region == GuitarRegion::capo)
    {
        const auto part = processor.getCapoPart();
        return "Capo: " + (part != nullptr ? part->name : juce::String ("capo")) + ", fret " + juce::String (o.capoFret)
             + ". Drag along the neck to move it; drag it off the headstock end to take it off.";
    }

    return {};
}

void BenchIllustration::rebuild (bool force)
{
    const auto* audition = bench.getAuditionGuitar();
    const auto& guitar = audition != nullptr ? *audition : bench.current();
    const auto options = renderOptions();
    const auto key = GuitarRenderer::keyFor (guitar, options);

    if (! force && key == shownKey && (audition != nullptr) == shownAudition)
        return;

    auto next = GuitarRenderer::build (guitar, options);

    // A committed change - not a drag in progress, not an audition's hover -
    // crossfades from the old picture, or outlines what changed (12.1, 16).
    const bool committedChange = ! scene.hits.empty() && next.key != scene.key && ! bench.isInGesture()
                              && audition == nullptr && ! shownAudition && getWidth() > 0 && getHeight() > 0;

    if (committedChange)
    {
        const bool reduced = ! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition);

        if (reduced)
        {
            fade.begin ({}, 0.0, true);
            changedParts = changedRegions (scene, next);
        }
        else
        {
            juce::Image old (juce::Image::ARGB, getWidth(), getHeight(), true, juce::SoftwareImageType());
            {
                juce::Graphics g (old);
                g.reduceClipRegion (getLocalBounds().reduced (1));
                GuitarRenderer::paint (g, scene, mmToPx);
            }

            fade.begin (old, juce::Time::getMillisecondCounterHiRes(), false);
            changedParts = {};
        }
    }

    scene = std::move (next);
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

    if (fade.startMs >= 0.0)
    {
        repaint();
        fade.finishIfDone (juce::Time::getMillisecondCounterHiRes());
    }

    if (tintEndedMs >= 0.0)
    {
        repaint();

        if (getTintAlpha (juce::Time::getMillisecondCounterHiRes()) <= 0.0f)
            tintEndedMs = -1.0;
    }

    // The capo, slide and pick move from elsewhere too (a preset, the CHARACTER
    // tab, a played note): repaint when what the overlay would draw changes.
    const auto o = currentOverlay();
    const double signature = o.capoFret * 1000.0 + o.slideFret * 17.0 + o.slideSlantDeg * 3.0
                           + o.pickPositionMm * 7.0 + o.pickAngleDeg * 11.0 + (double) o.capoMask;

    if (signature != lastOverlaySignature)
    {
        lastOverlaySignature = signature;
        repaint();
    }
}

GuitarRegion BenchIllustration::regionAt (juce::Point<float> px, int* stringIndex) const
{
    // The accessories lie over the guitar, so they are hit first (section 13.1's z-order).
    if (const auto accessory = accessoryAt (px); accessory != GuitarRegion::none)
        return accessory;

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
    selectedString = region == GuitarRegion::strings || region == GuitarRegion::bridge || region == GuitarRegion::nut ? stringIndex : -1;

    // Screen readers hear the part (guitar-illustration.md 16).
    for (auto& h : scene.hits)
        if (h.region == region)
            setDescription (h.description);

    if (const auto text = describeAccessory (region); text.isNotEmpty())
        setDescription (text);

    // Section 16: a selected string is announced as itself, not as the set.
    if (region == GuitarRegion::strings && juce::isPositiveAndBelow (selectedString, (int) scene.stringDescriptions.size()))
        setDescription (scene.stringDescriptions[(size_t) selectedString]);

    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

//==============================================================================
void BenchIllustration::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), Metrics::panelCorner);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (getLocalBounds().reduced (1));
        GuitarRenderer::paint (g, scene, mmToPx);

        // 12.1: the old guitar fading out over the new.
        if (const float a = fade.alpha (juce::Time::getMillisecondCounterHiRes()); a > 0.0f)
        {
            g.setOpacity (a);
            g.drawImageAt (fade.previous, 0, 0);
            g.setOpacity (1.0f);
        }

        // 14: the frequency-band tint while a change is auditioned (a label under reduced motion).
        const double now = juce::Time::getMillisecondCounterHiRes();

        if (const float tint = getTintAlpha (now); tint > 0.0f && AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition))
            for (auto& h : scene.hits)
                if (h.region == GuitarRegion::body)
                {
                    g.saveState();
                    g.addTransform (mmToPx);
                    g.setColour ((tintDb >= 0.0f ? juce::Colour (0xffe8823a) : juce::Colour (0xff4f8fd6)).withAlpha (tint));
                    g.fillPath (h.area);
                    g.restoreState();
                }

        auto overlay = currentOverlay();
        overlay.changed = changedParts;
        overlay.changedColour = Palette::secondary;
        GuitarRenderer::paintOverlay (g, scene, mmToPx, overlay);

        // Section 4: the nut's slots, each drawn as deep as it is cut, while the nut is in hand.
        if (selected == GuitarRegion::nut)
        {
            const auto& depths = bench.current().setup.nutSlotDepthsMm;

            for (int s = 0; s < (int) scene.nutPoints.size(); ++s)
            {
                const double depth = juce::isPositiveAndBelow (s, depths.size()) ? depths[s]
                                   : getParameterPlain (ParamIDs::setupNutDepth (s % ParamIDs::kNumNutDepths + 1).toRawUTF8());
                const auto p = toPx (scene.nutPoints[(size_t) s]);
                const float len = 4.0f + (float) depth * 14.0f;
                g.setColour (s == selectedString ? Palette::accent : Palette::textMuted);
                g.fillRect (juce::Rectangle<float> (p.x - 1.0f, p.y - 1.5f, 2.0f + len, 3.0f));

                if (s == selectedString)
                {
                    g.setFont (Fonts::mono (10.0f));
                    g.drawText (juce::String (depth, 2) + " mm", juce::Rectangle<float> (p.x + len + 4.0f, p.y - 6.0f, 60.0f, 12.0f),
                                juce::Justification::centredLeft, false);
                }
            }
        }

        // A selected string, outlined along its length.
        if (selected == GuitarRegion::strings && juce::isPositiveAndBelow (selectedString, (int) scene.saddlePoints.size()))
        {
            g.setColour (Palette::accent);
            g.drawLine ({ toPx (scene.saddlePoints[(size_t) selectedString]), toPx (scene.nutPoints[(size_t) selectedString]) }, 2.0f);
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
    g.drawText ("mm from saddle", juce::Rectangle<float> (juce::jmax (x0, x250) + 6.0f, y - 5.0f, 90.0f, 10.0f),
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
        g.drawText ("AUDITIONING - release Alt to go back", getLocalBounds().reduced (10, 6), juce::Justification::topRight, false);

        // 14 under reduced motion: the change as a static label rather than a tint.
        if (! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition) && tintLabel.isNotEmpty())
            g.drawText (tintLabel, getLocalBounds().reduced (10, 22), juce::Justification::topRight, false);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), Metrics::panelCorner, 1.5f);
    }
}

//==============================================================================
void BenchIllustration::mouseMove (const juce::MouseEvent& e)
{
    // Section 3.1: hover outlines and names a part; it never selects.
    const auto r = regionAt (e.position);

    if (r != hovered)
    {
        hovered = r;
        repaint();
    }

    juce::String tip;

    for (auto& h : scene.hits)
        if (h.region == r)
            tip = h.description;

    if (pickupIndexOfRegion (r) >= 0)
        tip << "  Drag along the strings to move it; scroll to raise or lower it (Shift: treble side, Alt: bass side).";
    else if (r == GuitarRegion::bridge)
        tip << "  Drag a saddle along the string to set its intonation.";

    setMouseCursor (pickupIndexOfRegion (r) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    setHelpText (tip);
}

void BenchIllustration::mouseExit (const juce::MouseEvent&)
{
    hovered = GuitarRegion::none;
    repaint();
}

void BenchIllustration::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    changedParts = {};   // 16: the reduced-motion outline lasts until the next click

    if (e.mods.isMiddleButtonDown() || (e.mods.isRightButtonDown() && zoom > 1.0f))
    {
        drag = Drag::pan;
        dragStartMm = e.position;
        dragStartValue = 0.0;
        return;
    }

    // The accessories (section 4): the pick along the strings or turned at its
    // corner, the slide along the neck or turned at its end, the capo by fret.
    bool onHandle = false;

    if (const auto accessory = accessoryAt (e.position, &onHandle); accessory != GuitarRegion::none && ! e.mods.isShiftDown())
    {
        const auto o = currentOverlay();
        select (accessory);
        dragStartMm = toMm (e.position);

        if (accessory == GuitarRegion::pick)
        {
            drag = onHandle ? Drag::pickRotate : Drag::pick;
            dragStartValue = o.pickPositionMm;
            dragStartValue2 = o.pickAngleDeg;
            beginParameterGesture (onHandle ? ParamIDs::pickAngle : ParamIDs::pluckPosition);
        }
        else if (accessory == GuitarRegion::slideBar)
        {
            drag = onHandle ? Drag::slideRotate : Drag::slide;
            dragStartValue = o.slideFret;
            dragStartValue2 = o.slideSlantDeg;

            if (onHandle)
                beginParameterGesture (ParamIDs::slideSlant);
        }
        else
        {
            drag = Drag::capo;
            dragStartValue = o.capoFret;
            beginParameterGesture (ParamIDs::capoFret);
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

    // A string grabbed where it crosses the nut is its nut slot (section 4).
    if (r == GuitarRegion::strings && juce::isPositiveAndBelow (stringIndex, (int) scene.nutPoints.size())
        && std::abs (toMm (e.position).x - scene.nutPoints[(size_t) stringIndex].x) < 4.0f)
        r = GuitarRegion::nut;

    // A click on the nut picks the nearest string's slot.
    if (r == GuitarRegion::nut)
    {
        const auto mm = toMm (e.position);
        float best = 1.0e9f;

        for (int s = 0; s < (int) scene.nutPoints.size(); ++s)
            if (std::abs (scene.nutPoints[(size_t) s].y - mm.y) < best)
            {
                best = std::abs (scene.nutPoints[(size_t) s].y - mm.y);
                stringIndex = s;
            }
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
        // Section 4: a nut slot is dragged down to deepen it.
        drag = Drag::nut;
        dragIndex = stringIndex;
        dragStartMm = e.position;
        const auto& depths = bench.current().setup.nutSlotDepthsMm;
        dragStartValue = juce::isPositiveAndBelow (stringIndex, depths.size()) ? depths[stringIndex]
                       : getParameterPlain (ParamIDs::setupNutDepth (stringIndex % ParamIDs::kNumNutDepths + 1).toRawUTF8());
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
    else if (drag == Drag::nut)
    {
        // 0.05 mm snap (section 4); a pixel of travel is a hundredth of a millimetre.
        const double target = WorkshopBench::snap (dragStartValue + (double) (e.position.y - dragStartMm.y) * 0.01, fine, free, 0.05);
        bench.setNutSlotDepth (dragIndex, target);
        rebuild (false);
        repaint();
    }
    else if (drag == Drag::pick)
    {
        // Along the string axis only: 1 mm snap, Shift 0.1, Alt free.
        const double scale = scaleMm();
        const double target = WorkshopBench::snap (dragStartValue + (double) (mm.x - dragStartMm.x), fine, free);
        setParameterPlain (ParamIDs::pluckPosition, juce::jlimit (0.02, 0.5, target / scale));
        repaint();
    }
    else if (drag == Drag::pickRotate || drag == Drag::slideRotate)
    {
        // Turned about its centre: 1 degree snap.
        const auto o = currentOverlay();
        const bool pick = drag == Drag::pickRotate;
        const auto path = pick ? GuitarRenderer::pickPath (scene, o.pickPositionMm, 0.0f, o.pickSizeMm)
                               : GuitarRenderer::slidePath (scene, o.slideFret, 0.0f);
        const auto c = path.getBounds().getCentre();
        const double a0 = std::atan2 (dragStartMm.y - c.y, dragStartMm.x - c.x);
        const double a1 = std::atan2 (mm.y - c.y, mm.x - c.x);
        const double delta = juce::radiansToDegrees (std::remainder (a1 - a0, juce::MathConstants<double>::twoPi));
        const double target = WorkshopBench::snap (dragStartValue2 + delta, fine, free);

        if (pick)
            setParameterPlain (ParamIDs::pickAngle, juce::jlimit (0.0, 1.0, target / 60.0));
        else
            setParameterPlain (ParamIDs::slideSlant, juce::jlimit (-30.0, 30.0, target));

        repaint();
    }
    else if (drag == Drag::slide || drag == Drag::capo)
    {
        // The fret under the pointer: x is mm from the saddle, the nut at the scale length.
        const double scale = scaleMm();
        const double x = juce::jlimit (scale * 0.25, scale * 1.2, (double) mm.x);
        const double fret = -12.0 * std::log2 (juce::jmin (1.0, x / scale));

        if (drag == Drag::slide)
            slideRestFret = (float) juce::jlimit (0.0, 24.0, fret);
        else
            setParameterPlain (ParamIDs::capoFret, x > scale + 8.0 ? 0.0 : juce::jlimit (1.0, 12.0, std::ceil (fret - 0.1)));

        repaint();
    }
}

void BenchIllustration::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::pickup || drag == Drag::saddle || drag == Drag::nut)
        bench.endGesture();

    endParameterGestures();

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
        std::vector<GuitarRegion> present;

        for (auto r : builderOrder())
            for (auto& h : scene.hits)
                if (h.region == r)
                {
                    present.push_back (r);
                    break;
                }

        if (present.empty())
            return false;

        auto it = std::find (present.begin(), present.end(), selected);
        int index = it == present.end() ? -1 : (int) std::distance (present.begin(), it);
        index = key.getModifiers().isShiftDown() ? (index <= 0 ? (int) present.size() - 1 : index - 1)
                                                 : (index + 1) % (int) present.size();
        select (present[(size_t) index], selected == GuitarRegion::strings ? juce::jmax (0, selectedString) : 0);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::escapeKey && selected != GuitarRegion::none)
    {
        select (GuitarRegion::none);
        return true;
    }

    const bool fine = key.getModifiers().isShiftDown();
    const int pickup = pickupIndexOfRegion (selected);
    const bool left = key.getKeyCode() == juce::KeyPress::leftKey, right = key.getKeyCode() == juce::KeyPress::rightKey;
    const bool up = key.getKeyCode() == juce::KeyPress::upKey, down = key.getKeyCode() == juce::KeyPress::downKey;

    // Keyboard parity for the nut and the accessories (sections 4 and 10): the
    // same snaps as a drag, one undo entry per key.
    if (selected == GuitarRegion::nut && selectedString >= 0 && (up || down || left || right))
    {
        if (left || right)
        {
            select (GuitarRegion::nut, juce::jlimit (0, (int) scene.nutPoints.size() - 1, selectedString + (right ? -1 : 1)));
            return true;
        }

        const auto& depths = bench.current().setup.nutSlotDepthsMm;
        const double from = juce::isPositiveAndBelow (selectedString, depths.size()) ? depths[selectedString]
                          : getParameterPlain (ParamIDs::setupNutDepth (selectedString % ParamIDs::kNumNutDepths + 1).toRawUTF8());
        bench.setNutSlotDepth (selectedString, from + (down ? 0.05 : -0.05) * (fine ? 0.1 : 1.0));
        rebuild (false);
        repaint();
        return true;
    }

    if ((selected == GuitarRegion::pick || selected == GuitarRegion::slideBar || selected == GuitarRegion::capo)
        && (up || down || left || right))
    {
        const double step = fine ? 0.1 : 1.0;
        const auto o = currentOverlay();

        auto nudge = [this] (const char* id, double plain)
        {
            beginParameterGesture (id);
            setParameterPlain (id, plain);
            endParameterGestures();
        };

        if (selected == GuitarRegion::pick && (left || right))
            nudge (ParamIDs::pluckPosition, juce::jlimit (0.02, 0.5, ((double) o.pickPositionMm + (left ? step : -step)) / scaleMm()));
        else if (selected == GuitarRegion::pick)
            nudge (ParamIDs::pickAngle, juce::jlimit (0.0, 1.0, ((double) o.pickAngleDeg + (up ? step : -step)) / 60.0));
        else if (selected == GuitarRegion::slideBar && (up || down))
            nudge (ParamIDs::slideSlant, juce::jlimit (-30.0, 30.0, (double) o.slideSlantDeg + (up ? step : -step)));
        else if (selected == GuitarRegion::slideBar)
            slideRestFret = juce::jlimit (0.0f, 24.0f, slideRestFret + (left ? -1.0f : 1.0f) * (fine ? 0.1f : 1.0f));
        else if (selected == GuitarRegion::capo && (left || right))
            nudge (ParamIDs::capoFret, juce::jlimit (0.0, 12.0, (double) o.capoFret + (right ? 1.0 : -1.0)));

        setDescription (describeAccessory (selected));
        repaint();
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

WorkshopPanel::WorkshopPanel (LuthierAudioProcessor& p)
    : processor (p), bench (p.getBench()), illustration (p)
{
    setTitle ("Workshop");
    setDescription ("The bench: take the guitar apart, swap parts, move pickups, hear what changed.");

    addAndMakeVisible (title);
    title.setText ("WORKSHOP", juce::dontSendNotification);
    title.setFont (Fonts::display (20.0f));
    title.setColour (juce::Label::textColourId, Palette::textPrimary);

    addAndMakeVisible (guitarName);
    guitarName.setFont (Fonts::ui (13.0f, true));
    guitarName.setColour (juce::Label::textColourId, Palette::textMuted);

    addAndMakeVisible (saveAsButton);
    saveAsButton.setTooltip ("Save this guitar as a .luthierguitar file (Ctrl+G)");
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
        auto* b = categoryButtons.add (new juce::TextButton (c.name));
        addAndMakeVisible (b);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (0x57);
        b->onClick = [this, name = juce::String (c.name)] { showCategory (name); };
    }

    // Section 1: below 700 points the categories are a dropdown.
    addChildComponent (categoryBox);
    categoryBox.setTitle ("Parts category");
    for (int i = 0; i < (int) std::size (kCategories); ++i)
        categoryBox.addItem (kCategories[i].name, i + 1);
    categoryBox.onChange = [this] { showCategory (categoryBox.getText()); };

    addAndMakeVisible (illustration);
    illustration.onSelectionChanged = [this]
    {
        // Selecting a part shows its category in the drawer (section 5's Swap, done for you).
        const auto slot = slotForRegion (illustration.getSelected());
        const auto selectedRegion = illustration.getSelected();

        if (selectedRegion == GuitarRegion::pick)          showCategory ("Pick");
        else if (selectedRegion == GuitarRegion::slideBar) showCategory ("Slide");
        else if (selectedRegion == GuitarRegion::capo)     showCategory ("Capo");

        if (slot != GuitarSlot::numSlots)
            for (auto& c : kCategories)
                if (c.type == getSlotPartType (slot) && juce::String (c.name) != "Preamp")
                {
                    showCategory (c.name);
                    break;
                }

        refreshInspector();

        // Narrow: the inspector drawer opens and closes with the selection.
        if (inspectorCollapsed && (illustration.getSelected() != GuitarRegion::none) != isInspectorShowing())
            resized();

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
    swapButton.setTooltip ("Show the parts that can go in this slot");
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
    revertButton.setTooltip ("Put this slot back to what the guitar file has");
    revertButton.onClick = [this]
    {
        const auto slot = slotForRegion (illustration.getSelected());

        // Section 3.3: with a string selected, Revert clears that string's override.
        if (slot == GuitarSlot::strings && illustration.getSelectedString() >= 0
            && bench.current().stringOverrides[(size_t) juce::jlimit (0, 11, illustration.getSelectedString())].isSet())
            bench.clearStringOverride (illustration.getSelectedString());
        else if (slot != GuitarSlot::numSlots)
            bench.revert (slot);

        refreshAll();
    };

    addAndMakeVisible (savePartButton);
    savePartButton.setTooltip ("Keep this edited part in your parts folder");
    savePartButton.onClick = [this]
    {
        const auto slot = slotForRegion (illustration.getSelected());
        const auto part = slot != GuitarSlot::numSlots ? bench.current().get (slot) : nullptr;

        if (part == nullptr)
            return;

        auto* window = new juce::AlertWindow ("Save as user part", "Name the part:", juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", part->name + " (mine)");
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window, slot] (int result)
        {
            if (result == 1)
                processor.savePartAs (slot, window->getTextEditorContents ("name"));
            refreshAll();
        }), true);
    };

    addAndMakeVisible (autoZoomToggle);
    autoZoomToggle.setTooltip ("Fit the spectrum's scale to the change instead of +-12 dB");
    autoZoomToggle.onClick = [this] { autoZoom = autoZoomToggle.getToggleState(); repaint(); };

    // ---- setup strip: the parameters fret-buzz.md 1 already has ------------------------
    actionTreble = std::make_unique<LuthierKnob> ("Action T", LuthierKnob::Size::Small);
    actionBass   = std::make_unique<LuthierKnob> ("Action B", LuthierKnob::Size::Small);
    relief       = std::make_unique<LuthierKnob> ("Relief", LuthierKnob::Size::Small);

    actionTreble->attachTo (processor, ParamIDs::setupActionTreble, "String height at the 12th fret, treble side (mm)");
    actionBass->attachTo (processor, ParamIDs::setupActionBass, "String height at the 12th fret, bass side (mm)");
    relief->attachTo (processor, ParamIDs::setupRelief, "Neck relief at the 7th fret (mm)");

    for (auto* k : { actionTreble.get(), actionBass.get(), relief.get() })
        addAndMakeVisible (k);

    for (int n = 1; n <= ParamIDs::kNumNutDepths; ++n)
    {
        auto* k = nutDepths.add (new LuthierKnob ("Nut " + juce::String (n), LuthierKnob::Size::Small));
        k->attachTo (processor, ParamIDs::setupNutDepth (n), "Nut slot depth, string " + juce::String (n) + " (mm)");
        addAndMakeVisible (k);
    }

    // onboarding.md 9: hidden until owed.
    addChildComponent (firstHint);
    firstHint.onShownOrDismissed = [this] { resized(); repaint(); };

    showCategory (category);
    refreshAll();
    startTimerHz (20);
}

bool WorkshopPanel::showFirstEncounterHintIfDue()
{
    return firstHint.showIfDue();
}

void WorkshopPanel::setHintSaysEscapeCloses (bool says)
{
    firstHint.setText (juce::String (FirstEncounterHint::kWorkshopText) + (says ? " Escape closes." : ""));
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

    // workshop-ui.md 2: the pick shows while its tool is in hand.
    illustration.setPickShown (name == "Pick");

    for (auto* b : categoryButtons)
        b->setToggleState (b->getButtonText() == name, juce::dontSendNotification);

    for (int i = 0; i < categoryBox.getNumItems(); ++i)
        if (categoryBox.getItemText (i) == name)
            categoryBox.setSelectedItemIndex (i, juce::dontSendNotification);

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
                           "Change guitar family?",
                           "This will replace incompatible parts with defaults for the new family.",
                           "Change", "Cancel", this);

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

    if (slot == GuitarSlot::numSlots || field.isEmpty())
        return false;

    const auto trimmed = text.trim();

    // Section 3.3: the selected string's own override.
    if (field.startsWith (kStringFieldPrefix))
    {
        const int s = illustration.getSelectedString();

        if (s < 0)
            return false;

        auto o = bench.current().stringOverrides[(size_t) juce::jlimit (0, 11, s)];
        const auto which = field.fromFirstOccurrenceOf (kStringFieldPrefix, false, false);

        if (which == "gauge")
        {
            // "0.018", "18" (thousandths, as players say it) or "set" for the set's gauge.
            double g = trimmed.getDoubleValue();
            if (g >= 1.0) g /= 1000.0;
            o.gaugeIn = trimmed.equalsIgnoreCase ("set") ? 0.0 : juce::jlimit (0.0, 0.2, g);
        }
        else if (which == "wound")
        {
            o.wound = trimmed.equalsIgnoreCase ("wound") || trimmed.equalsIgnoreCase ("true") || trimmed.equalsIgnoreCase ("yes") ? 1
                    : trimmed.equalsIgnoreCase ("plain") || trimmed.equalsIgnoreCase ("false") || trimmed.equalsIgnoreCase ("no") ? 0 : -1;
        }
        else if (which == "material")
        {
            o.material = trimmed.equalsIgnoreCase ("set") ? juce::String() : trimmed.toLowerCase().replaceCharacter (' ', '_');
        }

        const bool ok = bench.setStringOverride (s, o);
        refreshAll();
        return ok;
    }

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

void WorkshopPanel::clickCard (int index, bool ontoSelectedString)
{
    if (! juce::isPositiveAndBelow (index, drawerParts.size()))
        return;

    const auto part = drawerParts[index];
    bench.endAudition();
    auditioning = false;
    illustration.endAuditionTint();

    // guitar-illustration.md 13.2: a strings card onto a string overrides that
    // string only (Ctrl-click with a string selected); anywhere else it replaces the set.
    if (ontoSelectedString && part->type == PartType::strings
        && illustration.getSelected() == GuitarRegion::strings && illustration.getSelectedString() >= 0)
    {
        const int s = illustration.getSelectedString();
        const auto gauges = part->numbers ("gauges_in");
        StringOverride o;
        o.gaugeIn = juce::isPositiveAndBelow (s, gauges.size()) ? gauges[s] : 0.0;
        o.material = part->text ("winding_material", "nickel_plated_steel");
        bench.setStringOverride (s, o);
        refreshAll();
        return;
    }

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
            limitMessage = "Turn on Slide Mode (S) to fit a slide.";
            limitShownAt = juce::Time::getMillisecondCounter();
            repaint();
            return;
        }

        bench.fitAccessory (part);
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
            illustration.endAuditionTint();
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

void WorkshopPanel::takeSpectrum (SpectrumDelta::Result&& result)
{
    spectrum = std::move (result);

    // guitar-illustration.md 14: the candidate's change, as a tint on the body.
    if (auditioning && spectrum.requestId == lastRequest && ! spectrum.deltaDb.empty())
    {
        double mean = 0.0;
        for (auto d : spectrum.deltaDb)
            mean += d;
        mean /= (double) spectrum.deltaDb.size();
        illustration.setAuditionTint (spectrum.noChange ? 0.0f : (float) mean, spectrum.summary);
    }

    // Section 10: announced as a sentence ("candidate is 1.8 dB brighter above
    // 2 kHz"), because a curve has no screen-reader form worth having. Only the
    // newest request's, and only when it says something new.
    if (spectrum.requestId == lastRequest && spectrum.summary.isNotEmpty() && spectrum.summary != lastAnnouncement)
    {
        lastAnnouncement = spectrum.summary;
        setDescription ("The bench: take the guitar apart, swap parts, move pickups, hear what changed. Spectrum: " + lastAnnouncement);
        juce::AccessibilityHandler::postAnnouncement ("Spectrum: " + lastAnnouncement,
                                                      juce::AccessibilityHandler::AnnouncementPriority::medium);
    }
}

bool WorkshopPanel::waitForSpectrum (int timeoutMs)
{
    const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

    while (juce::Time::getMillisecondCounter() < until)
    {
        SpectrumDelta::Result r;

        if (worker.takeResult (r))
        {
            takeSpectrum (std::move (r));
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
    guitarName.setText (g.name + (bench.isModified() ? "  (modified)" : ""), juce::dontSendNotification);

    for (int i = 0; i < slotButtons.size(); ++i)
    {
        auto* b = slotButtons[i];
        const bool filled = bench.hasSlot (i);
        b->setToggleState (filled, juce::dontSendNotification);
        b->setTooltip (filled ? "Bench slot " + WorkshopBench::slotName (i) + ": click to recall, Shift-click to clear"
                              : "Bench slot " + WorkshopBench::slotName (i) + ": click to store the guitar as it is now");
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

    // The accessories (workshop-ui.md 4): the part, and where it is.
    if (region == GuitarRegion::pick || region == GuitarRegion::slideBar || region == GuitarRegion::capo)
    {
        const auto type = region == GuitarRegion::pick ? PartType::pick : region == GuitarRegion::slideBar ? PartType::slide : PartType::capo;
        const auto part = bench.getAccessory (type);
        inspectorTitle = juce::String (getGuitarRegionName (region)) + ": " + (part != nullptr ? part->name : juce::String ("(default)"));

        const auto sentence = illustration.describeAccessory (region);
        for (auto clause : juce::StringArray::fromTokens (sentence.fromFirstOccurrenceOf (": ", false, false), ",;", ""))
            if (clause.trim().isNotEmpty())
                inspectorLines.add (clause.trim());

        inspectorLines.add (region == GuitarRegion::pick ? "Drag along the strings; turn it by its corner handle. Arrows nudge."
                          : region == GuitarRegion::slideBar ? "Drag along the neck; turn it by its end handle. Arrows nudge."
                                                             : "Drag along the neck, one fret at a time; off the end to remove it.");
        return;
    }

    if (slot == GuitarSlot::numSlots)
    {
        inspectorTitle = "Nothing selected";
        inspectorLines.add ("Click a part of the guitar, or press Tab.");
        return;
    }

    const auto part = guitar.get (slot);
    const auto label = WorkshopBench::describeSlot (slot);
    inspectorTitle = label.substring (0, 1).toUpperCase() + label.substring (1) + ": "
                   + (part != nullptr ? part->name : juce::String ("(empty)"));

    if (audition != nullptr)
        inspectorLines.add ("AUDITIONING - not fitted");

    if (part != nullptr)
    {
        inspectorLines.add (part->isFactory ? "Factory part" : "User part (unsaved edits live in the preset)");

        if (! part->suits (guitar.family))
            inspectorLines.add ("Unusual on a " + guitar.family + " guitar - it will work.");

        if (auto* object = part->fields.getDynamicObject())
            for (auto& prop : object->getProperties())
            {
                inspectorLines.add (prop.name.toString().replaceCharacter ('_', ' ') + ": " + fieldText (prop.value));

                while (inspectorFields.size() < inspectorLines.size() - 1)
                    inspectorFields.add ({});

                inspectorFields.add (prop.name.toString());
            }
    }

    // tuning-stability.md 6: the tuners' and the nut's derived figures (REALISM-C).
    inspectorLines.addArray (describeTuningFigures (processor, slot, part.get()));

    if (const int i = pickupIndexOfRegion (region); i >= 0 && part != nullptr)
    {
        const auto& pl = guitar.placements[(size_t) i];
        const auto travel = bench.getPickupTravel (i);
        inspectorLines.add ("position: " + juce::String (pl.positionMm, 1) + " mm from the saddle ("
                            + juce::String (juce::roundToInt (travel.min)) + " - " + juce::String (juce::roundToInt (travel.max)) + ")");
        inspectorLines.add ("height: " + juce::String (pl.heightTrebleMm, 1) + " mm treble / "
                            + juce::String (pl.heightBassMm, 1) + " mm bass");
    }

    if (region == GuitarRegion::strings && illustration.getSelectedString() >= 0)
    {
        // Section 3.3: the set, and this string's override fields.
        const int s = illustration.getSelectedString();
        const auto& o = guitar.stringOverrides[(size_t) juce::jlimit (0, 11, s)];
        auto addField = [this] (const juce::String& line, const juce::String& field)
        {
            inspectorLines.add (line);
            while (inspectorFields.size() < inspectorLines.size() - 1)
                inspectorFields.add ({});
            inspectorFields.add (field);
        };

        inspectorLines.add ("STRING " + juce::String (s + 1) + ": " + bench.describeString (guitar, s)
                            + (o.isSet() ? "  (override)" : ""));
        addField ("string gauge: " + (o.gaugeIn > 0.0 ? juce::String (o.gaugeIn, 3) : juce::String ("set")), juce::String (kStringFieldPrefix) + "gauge");
        addField ("string wound: " + juce::String (o.wound < 0 ? "set" : o.wound == 1 ? "wound" : "plain"), juce::String (kStringFieldPrefix) + "wound");
        addField ("string material: " + (o.material.isNotEmpty() ? o.material.replaceCharacter ('_', ' ') : juce::String ("set")),
                  juce::String (kStringFieldPrefix) + "material");
        inspectorLines.add ("Ctrl-click a strings card to put its string here; Revert clears it.");
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
    }

    if (region == GuitarRegion::nut)
    {
        juce::StringArray depths;
        for (auto d : guitar.setup.nutSlotDepthsMm)
            depths.add (juce::String (d, 2));
        inspectorLines.add ("slot depths: " + depths.joinIntoString (", ") + " mm");
    }
}

//==============================================================================
void WorkshopPanel::timerCallback()
{
    if (isShowing())
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
    {
        takeSpectrum (std::move (r));
        repaint (spectrumArea);
    }

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
        setHelpText (p.name + " - " + summaryOf (p) + ". Click to fit; hold Alt to hear it without fitting.");
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
        clickCard (card, e.mods.isCommandDown());
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

    // onboarding.md 9: the hint sits under the bench header while it shows.
    if (firstHint.isVisible())
    {
        firstHint.setBounds (area.removeFromTop (FirstEncounterHint::kHeight));
        area.removeFromTop (Metrics::gridHalf);
    }

    /*  Section 1: at 900 points and wider the inspector runs down the right with
        the spectrum under it. Narrower, the inspector becomes a drawer that
        opens beside the illustration while a part is selected, and the
        spectrum sits beside the setup strip; below 700 the parts drawer's
        categories become a dropdown. */
    const bool wide = area.getWidth() >= kWideBench;
    const bool narrow = area.getWidth() < kNarrowBench;
    inspectorCollapsed = ! wide;
    const bool inspectorOpen = wide || illustration.getSelected() != GuitarRegion::none;

    juce::Rectangle<int> right;

    if (wide)
    {
        right = area.removeFromRight (240);
        area.removeFromRight (Metrics::grid);
        spectrumArea = right.removeFromBottom (juce::jmin (180, right.getHeight() / 3));
        right.removeFromBottom (Metrics::grid);
    }

    // The setup strip (and, narrow, the spectrum beside it).
    auto bottom = area.removeFromBottom (wide ? 86 : 100);
    area.removeFromBottom (Metrics::gridHalf);

    if (! wide)
    {
        spectrumArea = bottom.removeFromRight (juce::jmin (240, bottom.getWidth() / 3));
        bottom.removeFromRight (Metrics::grid);
    }

    autoZoomToggle.setBounds (spectrumArea.getRight() - 96, spectrumArea.getY() + 2, 94, 20);

    setupArea = bottom;
    {
        auto s = setupArea.reduced (0, 2);
        s.removeFromTop (wide ? 0 : 12);
        const int knobW = juce::jmax (36, s.getWidth() / (3 + nutDepths.size()));
        for (auto* k : { actionTreble.get(), actionBass.get(), relief.get() })
            k->setBounds (s.removeFromLeft (knobW));
        for (auto* k : nutDepths)
            k->setBounds (s.removeFromLeft (knobW));
    }

    drawerArea = area.removeFromBottom (juce::jmax (110, area.getHeight() / 3));
    {
        categoryBox.setVisible (narrow);

        if (narrow)
        {
            categoryBox.setBounds (drawerArea.removeFromTop (26).removeFromLeft (200));

            for (auto* b : categoryButtons)
                b->setVisible (false);
        }
        else
        {
            // One row, or two when one would squeeze the names under 64 points.
            const int count = categoryButtons.size();
            const int rows = drawerArea.getWidth() / juce::jmax (1, count) < 64 ? 2 : 1;
            const int perRow = (count + rows - 1) / rows;
            int next = 0;

            for (int r = 0; r < rows; ++r)
            {
                auto tabs = drawerArea.removeFromTop (24);
                const int w = tabs.getWidth() / juce::jmax (1, perRow);

                for (int i = 0; i < perRow && next < count; ++i)
                {
                    categoryButtons[next]->setVisible (true);
                    categoryButtons[next++]->setBounds (tabs.removeFromLeft (w).reduced (1, 0));
                }
            }
        }
    }
    area.removeFromBottom (Metrics::gridHalf);

    if (! wide)
    {
        // The drawer inspector: beside the illustration while a part is selected.
        right = inspectorOpen ? area.removeFromRight (juce::jmin (230, area.getWidth() / 3)) : juce::Rectangle<int>();

        if (inspectorOpen)
            area.removeFromRight (Metrics::gridHalf);
    }

    inspectorArea = right;

    for (auto* b : { &swapButton, &revertButton, &savePartButton })
        b->setVisible (! inspectorArea.isEmpty());

    if (! inspectorArea.isEmpty())
    {
        auto buttons = inspectorArea.withTrimmedTop (inspectorArea.getHeight() - 28);
        const int w = buttons.getWidth() / 3;
        swapButton.setBounds (buttons.removeFromLeft (w).reduced (1, 2));
        revertButton.setBounds (buttons.removeFromLeft (w).reduced (1, 2));
        savePartButton.setBounds (buttons.reduced (1, 2));
    }

    illustrationArea = area;
    illustration.setBounds (illustrationArea);
}

void WorkshopPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    paintDrawer (g, drawerArea);

    if (! inspectorArea.isEmpty())
        paintInspector (g, inspectorArea);
    paintSpectrum (g, spectrumArea);

    LuthierLookAndFeel::drawSectionHeader (g, setupArea.withHeight (16).translated (0, -2), "Setup");

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
        g.drawText ("Turn on Slide Mode (S) to fit a slide.", inner.removeFromTop (18), juce::Justification::centredLeft, false);
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
            line = "Unusual here - " + line;
        if (! part.isFactory)
            line = "yours - " + line;
        g.drawText (line, text, juce::Justification::centredLeft, true);
    }

    // Section 9's empty user section.
    if (! anyUser && drawerParts.size() > 0 && y + cardH + 22 < inner.getBottom())
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (11.0f));
        g.drawText ("Your saved parts appear here. Edit any factory part and Save As to start.",
                    juce::Rectangle<int> (inner.getX(), y + cardH + 6, inner.getWidth(), 16), juce::Justification::centredLeft, true);
    }

    if (drawerParts.isEmpty())
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (12.0f));
        g.drawText ("No " + category.toLowerCase() + " parts are installed.", inner, juce::Justification::centred, true);
    }
}

void WorkshopPanel::paintInspector (juce::Graphics& g, juce::Rectangle<int> area)
{
    LuthierLookAndFeel::drawPanel (g, area.toFloat());
    auto inner = area.reduced (8).withTrimmedBottom (30);

    LuthierLookAndFeel::drawSectionHeader (g, inner.removeFromTop (22), "Inspector");
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

        const bool note = line.startsWith ("AUDITIONING") || line.startsWith ("Unusual");
        const bool editable = inspectorFields[i].isNotEmpty();
        g.setColour (note ? Palette::accent : editable ? Palette::textPrimary : Palette::textMuted);
        g.drawFittedText (line, row, juce::Justification::centredLeft, 1);
    }

    if (inspectorFields.joinIntoString ("").isNotEmpty() && inner.getHeight() > 16)
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (10.5f));
        g.drawFittedText ("Double-click a value to edit it. A factory part becomes your own copy.",
                          inner.removeFromTop (28), juce::Justification::topLeft, 2);
    }
}

void WorkshopPanel::paintSpectrum (juce::Graphics& g, juce::Rectangle<int> area)
{
    LuthierLookAndFeel::drawPanel (g, area.toFloat());
    auto inner = area.reduced (8);

    LuthierLookAndFeel::drawSectionHeader (g, inner.removeFromTop (20).withTrimmedRight (100), "Spectrum delta");
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
    g.drawFittedText (spectrum.summary.isNotEmpty() ? spectrum.summary
                                                    : juce::String ("Swap or audition a part to see what it changes."),
                      summaryArea, juce::Justification::centredLeft, 2);
}

} // namespace luthier
