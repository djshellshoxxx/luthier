#include "StageTouches.h"
#include "Theme.h"
#include "UiPreferences.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
//  VuMeter
//==============================================================================
VuMeter::VuMeter (LuthierAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
    setTitle ("VU meter");
    setTooltip ("Output level with VU ballistics: 0 VU is -18 dBFS");
    motion.startTimerHz (*this, 30);   // cpu-quality-modes 6
}

VuMeter::~VuMeter()
{
    motion.stopTimer();
}

bool VuMeter::isEnabledByUser()
{
    return UiPreferences::get().getBool ("appearance.vuMeter", true);
}

void VuMeter::setEnabledByUser (bool enabled)
{
    UiPreferences::get().setBool ("appearance.vuMeter", enabled);
}

float VuMeter::scalePosition (double vu) noexcept
{
    // A VU scale is compressed at the bottom: roughly linear in amplitude.
    const double lo = std::pow (10.0, kMinVu / 20.0), hi = std::pow (10.0, kMaxVu / 20.0);
    const double a = std::pow (10.0, juce::jlimit (kMinVu, kMaxVu, vu) / 20.0);
    return (float) ((a - lo) / (hi - lo));
}

void VuMeter::update (double rms, double nowMs)
{
    const double dt = lastTickMs > 0.0 ? juce::jlimit (0.0, 200.0, nowMs - lastTickMs) : 33.0;
    lastTickMs = nowMs;

    // A reading that does not move for a second while not silent is stale.
    if (rms != lastLevel)
    {
        lastLevel = rms;
        lastChangeMs = nowMs;
    }

    const bool wasStale = stale;
    stale = rms > 1.0e-6 && nowMs - lastChangeMs > kStaleMs;

    const double targetVu = juce::jlimit (kMinVu - 3.0, kMaxVu + 1.0, gainToDb (juce::jmax (1.0e-9, rms)) - kReferenceDbfs);

    // VU ballistics: 99% of a step in 300 ms, the same both ways; none while
    // readouts are stepped (cpu-quality-modes 6, Low).
    const double k = AnimationPolicy::get().isReadoutStepped() ? 1.0 : 1.0 - std::exp (-dt * 4.6 / kIntegrationMs);
    const double before = needleVu;
    needleVu += (targetVu - needleVu) * k;

    if (std::abs (needleVu - before) > 0.02 || stale != wasStale)
        repaint();
}

void VuMeter::timerCallback()
{
    if (const bool wanted = isEnabledByUser(); wanted != isVisible() && getParentComponent() != nullptr)
        setVisible (wanted);

    const auto& master = processor.getEngine().getMasterBus();
    update (juce::jmax (master.getRmsLeft(), master.getRmsRight()), juce::Time::getMillisecondCounterHiRes());
}

void VuMeter::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    // The face: cream under the needle, as on a tape machine; flat in High contrast.
    const auto face = Palette::textured ? juce::Colour (0xffe9dcbc) : Palette::panelSunken;
    const auto ink = Palette::textured ? juce::Colour (0xff2a1a0c) : Palette::textPrimary;

    g.setColour (Palette::edge);
    g.fillRoundedRectangle (b, 4.0f);
    auto inner = b.reduced (2.0f);

    if (Palette::textured)
    {
        juce::ColourGradient glow (face.brighter (0.15f), inner.getCentreX(), inner.getBottom(),
                                   face.darker (0.12f), inner.getCentreX(), inner.getY(), true);
        g.setGradientFill (glow);
    }
    else
        g.setColour (face);

    g.fillRoundedRectangle (inner, 3.0f);

    // The arc and its ticks, pivoting below the face.
    const juce::Point<float> pivot { inner.getCentreX(), inner.getBottom() + inner.getHeight() * 0.25f };
    const float radius = inner.getHeight() * 0.95f;
    const float sweep = 0.78f;   // radians either side of vertical
    auto angleOf = [sweep] (double vu) { return -sweep + 2.0f * sweep * scalePosition (vu); };

    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius * 0.86f, radius * 0.86f, 0.0f, angleOf (kMinVu), angleOf (0.0), true);
    g.setColour (ink.withAlpha (0.8f));
    g.strokePath (arc, juce::PathStrokeType (1.2f));

    juce::Path red;
    red.addCentredArc (pivot.x, pivot.y, radius * 0.86f, radius * 0.86f, 0.0f, angleOf (0.0), angleOf (kMaxVu), true);
    g.setColour (Palette::clip);
    g.strokePath (red, juce::PathStrokeType (2.2f));

    g.setFont (Fonts::mono (8.0f));

    for (double vu : { -20.0, -10.0, -7.0, -5.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0 })
    {
        const float a = angleOf (vu);
        const bool labelled = vu == -20.0 || vu == -10.0 || vu == -5.0 || vu == 0.0 || vu == 3.0;
        const auto p1 = pivot.getPointOnCircumference (radius * (labelled ? 0.79f : 0.83f), a);
        const auto p2 = pivot.getPointOnCircumference (radius * 0.90f, a);
        g.setColour (vu > 0.0 ? Palette::clip : ink);
        g.drawLine ({ p1, p2 }, 1.0f);

        // Only the landmarks are numbered: the scale crowds toward the top.
        if (labelled)
        {
            const auto t = pivot.getPointOnCircumference (radius * 1.0f, a);
            g.drawText (vu > 0.0 ? "+" + juce::String ((int) vu) : juce::String ((int) vu),
                        juce::Rectangle<float> (t.x - 10.0f, t.y - 5.0f, 20.0f, 10.0f), juce::Justification::centred, false);
        }
    }

    g.setFont (Fonts::display (10.0f));
    g.setColour (ink.withAlpha (0.7f));
    g.drawText ("VU", inner.withTrimmedTop (inner.getHeight() * 0.62f), juce::Justification::centred, false);

    // The needle: grey when stale.
    const float a = angleOf (needleVu);
    g.saveState();
    g.reduceClipRegion (inner.toNearestInt());
    g.setColour (stale ? Palette::textDisabled : juce::Colour (0xff1a1a1a).interpolatedWith (ink, Palette::textured ? 0.0f : 1.0f));
    g.drawLine ({ pivot, pivot.getPointOnCircumference (radius * 0.95f, a) }, 1.4f);
    g.restoreState();
}

//==============================================================================
//  RoomLight
//==============================================================================
RoomLight::RoomLight (LuthierAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
    refresh();
    motion.startTimerHz (*this, 10);   // cpu-quality-modes 6
}

RoomLight::~RoomLight()
{
    motion.stopTimer();
}

void RoomLight::refresh()
{
    auto plain = [this] (const char* id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));
        return p != nullptr ? (float) p->getValue() : 0.0f;
    };

    const bool on = plain (ParamIDs::roomOn) > 0.5f;
    const float w = on ? juce::jlimit (0.0f, 1.0f, (float) processor.getState().getParameterRange (ParamIDs::roomBlend)
                                                        .convertFrom0to1 (plain (ParamIDs::roomBlend))) : 0.0f;
    const float s = on ? plain (ParamIDs::roomSize) : 0.0f;   // the size choice, 0 (small) to 1 (largest)

    if (std::abs (w - warmth) > 0.005f || std::abs (s - spread) > 0.005f)
    {
        warmth = w;
        spread = s;
        repaint();
    }
}

void RoomLight::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6
    if (warmth <= 0.0f || ! Palette::textured)
        return;

    // A warm pool of light from below, wider with a bigger room, brighter with more of it.
    auto b = getLocalBounds().toFloat();
    const float radius = b.getWidth() * (0.35f + 0.65f * spread);
    const auto warm = juce::Colour (0xffe9a34c).withAlpha (0.06f + 0.30f * warmth);

    juce::ColourGradient light (warm, b.getCentreX(), b.getBottom(),
                                warm.withAlpha (0.0f), b.getCentreX() + radius, b.getBottom(), true);
    g.setGradientFill (light);
    g.fillRoundedRectangle (b.reduced (1.0f), Metrics::panelCorner);
}

} // namespace luthier
