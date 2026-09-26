#include "GuitarBodyComponent.h"
#include "RealismGroupsC.h"   // REALISM-C
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

//==============================================================================
GuitarBodyComponent::GuitarBodyComponent (LuthierAudioProcessor& p)
    : processor (p),
      animator (*this, p.getEngine().getSoundingNotes(),
                [this] (StringMotionGeometry& g) { return fillMotionGeometry (g); }),
      chordName (p)
{
    startTimerHz (30);
}

GuitarBodyComponent::~GuitarBodyComponent()
{
    stopTimer();
}

//==============================================================================
/*  The picture is guitar-illustration.md's renderer drawing the parts guitar the
    engine is playing (ground rule 5). The static scene is built when the guitar
    changes and cached as an image at this size; only the live overlay paints
    per frame (section 2). */
void GuitarBodyComponent::rebuildScene (bool force)
{
    GuitarRenderer::Options options;
    options.materials = AccessibilitySettings::get().getPalette() != PaletteId::highContrast;

    const auto& guitar = processor.getCurrentGuitar();
    const auto key = GuitarRenderer::keyFor (guitar, options);

    if (! force && key == scene.key && ! scene.hits.empty())
        return;

    auto next = GuitarRenderer::build (guitar, options);

    // A different guitar (not a resize or a palette): 12.1's crossfade from the
    // old picture, or under reduced motion an instant change with the changed
    // parts outlined (16).
    if (! scene.hits.empty() && next.key != scene.key)
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const bool reduced = AccessibilitySettings::get().isReducedMotion();

        fade.begin (cache, now, reduced);
        overlay.changed = reduced ? changedRegions (scene, next) : std::array<bool, (size_t) GuitarRegion::numRegions> {};
    }

    scene = std::move (next);
    cache = {};
    animator.resetMotion();   // animated-strings.md 10: a family switch starts from rest
    repaint();
}

void GuitarBodyComponent::rebuildCache()
{
    auto area = getLocalBounds().toFloat().reduced ((float) Metrics::gridHalf);
    area.removeFromBottom (16.0f);   // the name plate

    mmToPx = GuitarRenderer::fitTransform (scene, area);

    // animated-strings.md 4.4: while the strings animate, their speaking lengths
    // are painted per frame (layer 25a) rather than baked into the cache.
    cacheOmitsSpeaking = animator.isAnimationEnabled();

    const float scale = juce::Component::getApproximateScaleFactorForComponent (this);
    const int w = juce::jmax (1, juce::roundToInt ((float) getWidth() * scale));
    const int h = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));

    cache = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (cache);
    g.addTransform (juce::AffineTransform::scale (scale));
    GuitarRenderer::PaintLayers layers;
    layers.omitSpeakingLengths = cacheOmitsSpeaking;
    GuitarRenderer::paint (g, scene, mmToPx, layers);
    cacheScale = scale;
}

void GuitarBodyComponent::ensureTransform()
{
    if (scene.hits.empty())
        rebuildScene (true);

    auto area = getLocalBounds().toFloat().reduced ((float) Metrics::gridHalf);
    area.removeFromBottom (16.0f);   // the name plate, as rebuildCache
    mmToPx = GuitarRenderer::fitTransform (scene, area);
}

bool GuitarBodyComponent::fillMotionGeometry (StringMotionGeometry& geometry)
{
    // 12: nothing moves until the scene exists.
    if (scene.strings.empty() || getWidth() <= 0 || getHeight() <= 0)
        return false;

    if (cache.isNull())
        ensureTransform();

    const float pxPerMm = std::sqrt (std::abs (mmToPx.getDeterminant()));

    if (pxPerMm <= 0.0f)
        return false;

    geometry.numStrings = 0;

    for (const auto& line : scene.strings)
    {
        if (! juce::isPositiveAndBelow (line.index, StringMotionGeometry::kMaxStrings))
            continue;

        auto& out = geometry.strings[(size_t) line.index];
        out.nut = line.nut.transformedBy (mmToPx);
        out.bridge = line.saddle.transformedBy (mmToPx);
        out.strokeWidthPx = GuitarRenderer::stringWidthPx (line.widthMm, line.minWidthPx, pxPerMm);
        geometry.numStrings = juce::jmax (geometry.numStrings, line.index + 1);
    }

    geometry.numFrets = (float) juce::jmax (1, scene.numFrets);
    geometry.clip = getLocalBounds().toFloat();
    return geometry.numStrings > 0;
}

juce::Point<float> GuitarBodyComponent::toMm (juce::Point<float> px) const
{
    auto p = px;
    mmToPx.inverted().transformPoint (p.x, p.y);
    return p;
}

GuitarRegion GuitarBodyComponent::regionAt (juce::Point<float> px) const
{
    if (const auto* hit = GuitarRenderer::hitTest (scene, toMm (px)))
        return hit->region;

    return GuitarRegion::none;
}

int GuitarBodyComponent::knobAt (juce::Point<float> px) const
{
    const auto mm = toMm (px);

    for (int i = 0; i < (int) scene.knobCentres.size(); ++i)
        if (mm.getDistanceFrom (scene.knobCentres[(size_t) i]) <= scene.knobRadiusMm + 1.5f)
            return i;

    return -1;
}

juce::String GuitarBodyComponent::knobParameter (int knob) const
{
    // Volumes first, then tones: one volume on two- and three-knob guitars, two on four.
    const int count = (int) scene.knobCentres.size();
    const bool volume = knob == 0 || (count >= 4 && knob == 1);
    return volume ? juce::String (ParamIDs::guitarVolume) : juce::String (ParamIDs::guitarTone);
}

int GuitarBodyComponent::engineSlotFor (int fileIndex) const
{
    // The engine numbers pickups from the bridge (PartAcoustics.cpp); the file from the neck.
    const auto& guitar = processor.getCurrentGuitar();
    int slot = 0;

    for (int i = 2; i > fileIndex; --i)
        if (auto p = guitar.get (WorkshopGuitar::pickupSlot (i)); p != nullptr && p->text ("family") != "piezo")
            ++slot;

    return slot;
}

int GuitarBodyComponent::pickupIndexFor (GuitarRegion r) noexcept
{
    return r == GuitarRegion::pickupNeck ? 0 : r == GuitarRegion::pickupMiddle ? 1 : r == GuitarRegion::pickupBridge ? 2 : -1;
}

//==============================================================================
void GuitarBodyComponent::resized()
{
    rebuildScene (scene.hits.empty());
    cache = {};
}

void GuitarBodyComponent::timerCallback()
{
    // The guitar can change under us (Workshop, preset, type); twice a second is enough to notice.
    if (++ticksSinceKeyCheck >= 15)
    {
        ticksSinceKeyCheck = 0;
        rebuildScene (false);
    }

    updateLiveOverlay (juce::Time::getMillisecondCounterHiRes());
}

bool GuitarBodyComponent::isAnimating (double nowMs) const noexcept
{
    if (fade.isActive (nowMs))
        return true;

    for (int s = 0; s < 12; ++s)
        if (dots.alpha[(size_t) s] > 0.0f && dots.alpha[(size_t) s] < 1.0f)
            return true;

    return false;
}

void GuitarBodyComponent::updateLiveOverlay (double nowMs)
{
    auto& engine = processor.getEngine();
    lastFrameMs = nowMs;
    // cpu-quality-modes 6 (animated-strings 2.6): motion Off - reduced motion, or CPU
    // quality Low - means no fades and a fixed glow.
    const bool reducedMotion = StringMotionPolicy::getMotion() == StringMotionPolicy::Motion::off;

    bool changed = fade.isActive (nowMs);
    fade.finishIfDone (nowMs);

    // animated-strings.md 2.6 and 4.3: the animator reads its gates on this tick.
    animator.poll();

    if (animator.isAnimationEnabled() != cacheOmitsSpeaking)
    {
        cache = {};
        changed = true;
    }

    const bool motion = animator.isMotionActive();
    changed = changed || motion != overlay.motionActive;
    overlay.motionActive = motion;

    for (int s = 0; s < juce::jmin (12, engine.getNumStrings()); ++s)
    {
        const auto level = (float) StringMotion::normaliseLevel ((float) engine.getStringLevel (s));
        const auto fret = (float) engine.getStringFret (s);
        const auto oldLevel = overlay.stringLevel[(size_t) s];

        // While the strings animate the level only draws the ghost, which repaints
        // itself; here only the dot turning on or off matters (4.3: no full repaints).
        const bool levelChanged = (motion || overlay.reducedMotion) ? ((level > 0.01f) != (oldLevel > 0.01f))
                                                                    : std::abs (level - oldLevel) > 0.004f;

        if (levelChanged || std::abs (fret - overlay.stringFret[(size_t) s]) > 0.01f)
            changed = true;

        overlay.stringLevel[(size_t) s] = level;
        overlay.stringFret[(size_t) s] = fret;

        // Section 19: the dot is on from the first frame and fades over 60 ms after.
        changed = dots.update (s, level, fret, nowMs, reducedMotion) || changed;
    }

    dots.copyTo (overlay);

    const auto slideFret = (float) engine.getSlideEngine().getOverlayFret();

    if (std::abs (slideFret - overlay.slideFret) > 0.01f)
    {
        overlay.slideFret = slideFret;
        changed = true;
    }

    // The capo on the neck (TODO G) and the slide's slant and material
    // (gui-integration.md 21), as the bench draws them.
    const int capoFret = engine.getTuningEngine().getCapoFret();
    const auto capoMask = engine.getTuningEngine().getCapoStringMask();
    const auto slant = (float) engine.getSlideEngine().getSettings().slantDegrees;
    const auto slideColour = juce::Colour (getSlideMaterial (engine.getSlideEngine().getBar().material).colour);

    if (capoFret != overlay.capoFret || capoMask != overlay.capoMask || slant != overlay.slideSlantDeg || slideColour != overlay.slideColour)
    {
        overlay.capoFret = capoFret;
        overlay.capoMask = capoMask;
        overlay.slideSlantDeg = slant;
        overlay.slideColour = slideColour;
        changed = true;
    }

    changed = changed || reducedMotion != overlay.reducedMotion;
    overlay.reducedMotion = reducedMotion;

    // piano-roll-chord-display.md 4, 7: the chord name is part of the live pass.
    const bool nameWas = chordName.getFader().isVisible (nowMs - 34.0);
    chordName.tick (nowMs);

    if (nameWas || chordName.getFader().isVisible (nowMs))
        repaint (getChordNameArea().getSmallestIntegerContainer().expanded (8));

    if (changed)
        repaint();
}

void GuitarBodyComponent::setGhostDots (const std::vector<std::pair<int, double>>& dots)
{
    ghostFrets.fill (-1.0f);
    const int capo = processor.getEngine().getTuningEngine().getCapoFret();

    for (const auto& [string, fret] : dots)
        if (juce::isPositiveAndBelow (string, 12))
            ghostFrets[(size_t) string] = (float) (fret + capo);

    repaint();
}

juce::Rectangle<float> GuitarBodyComponent::getChordNameArea() const
{
    juce::Rectangle<float> body;

    for (const auto& hit : scene.hits)
        if (hit.region == GuitarRegion::body)
            body = body.isEmpty() ? hit.area.getBounds() : body.getUnion (hit.area.getBounds());

    if (body.isEmpty() || scene.nutPoints.empty())
        return {};

    juce::Point<float> nut;

    for (const auto& p : scene.nutPoints)
        nut += p;

    nut /= (float) scene.nutPoints.size();

    const auto bodyPx = body.transformedBy (mmToPx);
    return ChordNameOverlay::lowerBout (bodyPx, nut.transformedBy (mmToPx)).getIntersection (getLocalBounds().toFloat());
}

//==============================================================================
void GuitarBodyComponent::paint (juce::Graphics& g)
{
    if (scene.hits.empty())
        rebuildScene (true);

    const float scale = juce::Component::getApproximateScaleFactorForComponent (this);

    if (cache.isNull() || std::abs (scale - cacheScale) > 0.01f)
        rebuildCache();

    g.drawImage (cache, getLocalBounds().toFloat());

    // animated-strings.md 4.4: layer 25a, the speaking lengths, under every overlay.
    if (cacheOmitsSpeaking)
    {
        const auto start = juce::Time::getHighResolutionTicks();

        GuitarRenderer::SpeakingStyle style;
        style.highContrast = AccessibilitySettings::get().getPalette() == PaletteId::highContrast;
        style.highContrastColour = Palette::textPrimary;
        GuitarRenderer::paintSpeakingLengths (g, scene, mmToPx, animator.getFrameToPaint(), style);

        animator.notePaintMilliseconds (juce::Time::highResolutionTicksToSeconds (
                                            juce::Time::getHighResolutionTicks() - start) * 1000.0);
    }

    // 12.1: the old guitar fading out over the new one.
    if (const float a = fade.alpha (lastFrameMs); a > 0.0f && fade.previous.isValid())
    {
        g.setOpacity (a);
        g.drawImage (fade.previous, getLocalBounds().toFloat());
        g.setOpacity (1.0f);
    }

    overlay.accent = Palette::accent;
    overlay.ghostFret = ghostFrets;
    overlay.ghostColour = Palette::textPrimary;
    overlay.changedColour = Palette::secondary;
    overlay.hovered = hoveredRegion;
    GuitarRenderer::paintOverlay (g, scene, mmToPx, overlay);

    // piano-roll-chord-display.md 4: the chord name, over the lower bout.
    chordName.paint (g, getChordNameArea(), (float) getHeight(), lastFrameMs);

    // ---- name plate -------------------------------------------------------------------
    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (11.0f, true));
    const auto& guitar = processor.getCurrentGuitar();
    g.drawText (guitar.name.isNotEmpty() ? guitar.name : processor.getEngine().getGuitarSpec().name,
                getLocalBounds().removeFromBottom (16).reduced (Metrics::grid, 0),
                juce::Justification::centredLeft, true);
}

//==============================================================================
juce::String GuitarBodyComponent::describeHoverTarget (juce::Point<float> position) const
{
    const auto region = regionAt (position);

    switch (region)
    {
        case GuitarRegion::headstock:
        case GuitarRegion::tuners:
        case GuitarRegion::nut:
            return "Click the headstock for tuning, temperament and per-string detune.";

        case GuitarRegion::bridge:
        case GuitarRegion::tailpiece:
        {
            const auto bridge = Parameters::bridgeTypeNames()[getBridgeTypeIndex()];

            return WhammyPopover::isWhammyFitted (const_cast<LuthierAudioProcessor&> (processor))
                     ? bridge + " - click the bridge for whammy range and spring tension."
                     : bridge + " - no arm fitted, so there is nothing to set here.";
        }

        case GuitarRegion::pickupNeck:
        case GuitarRegion::pickupMiddle:
        case GuitarRegion::pickupBridge:
        {
            const int i = pickupIndexFor (region);
            const auto& guitar = processor.getCurrentGuitar();
            const auto p = guitar.get (WorkshopGuitar::pickupSlot (i));
            return (p != nullptr ? p->name : juce::String ("Pickup")) + " at "
                   + juce::String (juce::roundToInt (guitar.placements[(size_t) i].positionMm))
                   + " mm from the saddle - click to select it, right-click for its type.";
        }

        case GuitarRegion::controls:
            return "Drag a knob up or down to set the guitar's volume or tone.";

        case GuitarRegion::selector:
            return "Click the switch to change the pickup position.";

        case GuitarRegion::strings:
        {
            const auto strings = processor.getCurrentGuitar().get (GuitarSlot::strings);
            return "The strings" + (strings != nullptr ? ": " + strings->name : juce::String())
                   + ". Swap them in the Workshop.";
        }

        case GuitarRegion::none:
        case GuitarRegion::body:
        case GuitarRegion::pickguard:
        case GuitarRegion::soundhole:
        case GuitarRegion::neck:
        case GuitarRegion::fretboard:
        case GuitarRegion::jack:
        case GuitarRegion::pick:
        case GuitarRegion::slideBar:
        case GuitarRegion::capo:
        case GuitarRegion::numRegions:
            break;
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

void GuitarBodyComponent::mouseMove (const juce::MouseEvent& e)
{
    if (scene.hits.empty())
        rebuildScene (true);

    if (cache.isNull())
        rebuildCache();

    const auto region = regionAt (e.position);

    if (region != hoveredRegion)
    {
        hoveredRegion = region;
        repaint();
    }

    const auto wanted = describeHoverTarget (e.position);

    if (wanted != getTooltip())
        setTooltip (wanted);
}

void GuitarBodyComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredRegion = GuitarRegion::none;
    repaint();
}

void GuitarBodyComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& state = processor.getState();

    // 16: the reduced-motion outline of what changed lasts until the next click.
    overlay.changed = {};

    // ---- knobs: drawn on top of the body, tested first -------------------------------
    if (const int knob = knobAt (e.position); knob >= 0)
    {
        draggingKnob = knob;
        dragStartY = e.y;

        if (auto* p = state.getParameter (knobParameter (knob)))
            dragStartValue = p->getValue();

        return;
    }

    const auto region = regionAt (e.position);

    // ---- selector switch -------------------------------------------------------------
    if (region == GuitarRegion::selector)
    {
        const int fitted = processor.getEngine().getGuitarSpec().numPickups;

        if (auto* p = state.getParameter (ParamIDs::pickupSelector); p != nullptr && fitted > 1)
        {
            const int positions = (fitted >= 3) ? 5 : 3;
            const int current = (int) std::round (p->getValue() * (float) (positions - 1));
            const int next = (current + 1) % positions;

            p->setValueNotifyingHost ((float) next / (float) juce::jmax (1, positions - 1));
        }

        repaint();
        return;
    }

    // ---- pickups -----------------------------------------------------------------------
    if (const int i = pickupIndexFor (region); i >= 0)
    {
        const int slot = engineSlotFor (i);

        if (e.mods.isPopupMenu())
        {
            showParameterContextMenu (*this, processor, ParamIDs::pickupType (slot));
            return;
        }

        // Clicking a pickup selects that pickup alone, as the switch on the real instrument would.
        if (auto* p = state.getParameter (ParamIDs::pickupSelector))
        {
            const int target = (i == 2) ? (int) PickupSelector::Bridge
                             : (i == 0) ? (int) PickupSelector::Neck
                                        : (int) PickupSelector::Middle;

            p->setValueNotifyingHost ((float) target / (float) ((int) PickupSelector::NumSelections - 1));
        }

        if (onPickupSelected)
            onPickupSelected (slot);

        repaint();
        return;
    }

    // ---- headstock and bridge, gui-integration.md 3.1 ----------------------------------
    if (region == GuitarRegion::headstock || region == GuitarRegion::tuners || region == GuitarRegion::nut)
    {
        showTuningPopover();
        return;
    }

    if (region == GuitarRegion::bridge || region == GuitarRegion::tailpiece)
    {
        showWhammyPopover();
        return;
    }
}

juce::Rectangle<int> GuitarBodyComponent::screenAreaOf (GuitarRegion region) const
{
    for (auto& h : scene.hits)
        if (h.region == region)
            return h.area.getBounds().transformedBy (mmToPx).toNearestInt() + getScreenPosition();

    return getScreenBounds();
}

void GuitarBodyComponent::showTuningPopover()
{
    auto popover = std::make_unique<TuningPopover> (processor);
    juce::CallOutBox::launchAsynchronously (std::move (popover), screenAreaOf (GuitarRegion::headstock), nullptr);
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
    juce::CallOutBox::launchAsynchronously (std::move (popover), screenAreaOf (GuitarRegion::bridge), nullptr);
}

void GuitarBodyComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0)
        return;

    if (auto* p = processor.getState().getParameter (knobParameter (draggingKnob)))
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

        /*  action-and-undo.md 4: one entry per string, grouped over 200 ms, so a
            drag is one action - and a wheel or keyboard change, which never
            started a drag, still makes one. */
        slider->onValueChange = [this, i, slider]
        {
            processor.pushUndoAction ("Detune string " + juce::String (i + 1), "string-detune", juce::String (i));

            // tuning-stability.md 5: a lower detune is a string brought down to pitch.
            auto& engine = processor.getEngine();
            const double before = engine.getStabilityBasePitch (i);
            engine.getTuningEngine().setDetuneCents (i, slider->getValue());
            engine.getStabilityModel().onTuningChanged (i, before, engine.getStabilityBasePitch (i));
            refreshNoteNames();
            repaint();
        };

        addAndMakeVisible (slider);

        // tuning-stability.md 6: the string's offset, and its Retune.
        addAndMakeVisible (stabilityBadges.add (new StabilityBadge (processor, i)));
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

    return { 0, 0, 300 + StabilityBadge::preferredWidth, height };
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

    for (int i = 0; i < detuneSliders.size(); ++i)
    {
        auto row = bounds.removeFromTop (kRowHeight);
        row.removeFromLeft (kNoteColumnWidth);          // the note name paint() draws

        if (auto* badge = stabilityBadges[i])
            badge->setBounds (row.removeFromRight (StabilityBadge::preferredWidth));

        detuneSliders[i]->setBounds (row);
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
