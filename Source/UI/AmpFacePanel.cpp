#include "AmpFacePanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

#include <cmath>

namespace luthier
{

namespace
{
    /** LuthierKnob keeps a value row above its knob and a label row below it (Widgets.cpp). */
    constexpr float knobRow = 14.0f;

    /** Sag tops out near 0.55 on the heaviest voicings; 0.35 is an amp driven hard. */
    constexpr double fullGlowSag = 0.35;

    /** The glow moves in sixteenths, so a steady note does not repaint the vent every tick. */
    constexpr float glowSteps = 16.0f;

    struct ControlSetup
    {
        const char* name;
        const char* paramId;
        const char* tooltip;
    };

    // The AMP section's controls and their tooltips, in faces::AmpKnob and faces::AmpSwitch order.
    const ControlSetup knobSetups[faces::numAmpKnobs] =
    {
        { "Gain",     ParamIDs::ampGain,     "Preamp drive" },
        { "Bass",     ParamIDs::ampBass,     "Passive tone stack: the three controls interact, exactly as in the circuit" },
        { "Mid",      ParamIDs::ampMid,      "Passive tone stack midrange" },
        { "Treble",   ParamIDs::ampTreble,   "Passive tone stack treble" },
        { "Presence", ParamIDs::ampPresence, "Works inside the power amp's feedback loop, so it does more on amps that "
                                             "have plenty of feedback and almost nothing on a British top-boost combo" },
        { "Master",   ParamIDs::ampMaster,   "Power amp drive. Turn it up for power-tube saturation and sag." }
    };

    const ControlSetup switchSetups[faces::numAmpSwitches] =
    {
        { "Bright",    ParamIDs::ampBright,   "Treble bypass cap: strongest at low gain, gone as the gain comes up" },
        { "Mid boost", ParamIDs::ampMidBoost, "Midrange lift ahead of the gain" },
        { "Standby",   ParamIDs::ampStandby,  "Mutes the amp, and takes time to warm back up, like the real switch" }
    };
}

//==============================================================================
AmpFacePanel::AmpFacePanel (LuthierAudioProcessor& p, Style s)
    : processor (p), style (s)
{
    setLookAndFeel (&faceLookAndFeel);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto knob = std::make_unique<LuthierKnob> (knobSetups[i].name, LuthierKnob::Size::Small);
        knob->attachTo (processor, knobSetups[i].paramId, knobSetups[i].tooltip);

        // The face prints the label under the knob, in the plate's own ink.
        knob->setLabelText ({});

        // The knob's value and label rows hang over the face around it; only the
        // knob itself takes clicks, so they never cover a neighbour.
        knob->setInterceptsMouseClicks (false, true);

        addAndMakeVisible (*knob);
        knobs[i] = std::move (knob);
    }

    if (style == Style::section)
    {
        for (size_t i = 0; i < switches.size(); ++i)
        {
            auto toggle = std::make_unique<LuthierToggle> (switchSetups[i].name);
            toggle->attachTo (processor, switchSetups[i].paramId, switchSetups[i].tooltip);

            // Standby's lever is up while the amp plays, which is when the parameter is off.
            faces::setFaceSwitch (toggle->getButton(), faces::FaceSwitch::mini, i == (size_t) faces::standbySwitch);

            addAndMakeVisible (*toggle);
            switches[i] = std::move (toggle);
        }
    }

    refresh();
    startTimerHz (30);
}

AmpFacePanel::~AmpFacePanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
bool AmpFacePanel::parameterIsOn (const char* id) const
{
    if (auto* value = processor.getState().getRawParameterValue (id))
        return value->load() > 0.5f;

    return false;
}

AmpModel AmpFacePanel::modelFromParameter() const
{
    if (auto* value = processor.getState().getRawParameterValue (ParamIDs::ampModel))
        return (AmpModel) juce::jlimit (0, (int) AmpModel::NumModels - 1, juce::roundToInt (value->load()));

    return AmpModel::FenderTwin;
}

void AmpFacePanel::applyModel()
{
    // visual-polish.md 3: the model's knob caps, and its kind of standby switch.
    faceLookAndFeel.setCap (faces::knobCapFor (shownModel));

    if (auto* standby = switches[(size_t) faces::standbySwitch].get())
        faces::setFaceSwitch (standby->getButton(),
                              faces::standbyIsRocker (shownModel) ? faces::FaceSwitch::rocker : faces::FaceSwitch::mini, true);

    // A combo and a head put their controls in different places.
    resized();
    repaint();
}

void AmpFacePanel::refresh()
{
    if (const auto model = modelFromParameter(); model != shownModel)
    {
        shownModel = model;
        applyModel();
    }

    // visual-polish.md 2: the pilot light follows Standby.
    if (const bool standby = parameterIsOn (ParamIDs::ampStandby); standby != shownStandby)
    {
        shownStandby = standby;
        repaint();
    }

    // visual-polish.md 4: the valves glow with the drive the amp reports.
    const double sag = processor.getEngine().getAmpEngine().getSagAmount();
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    if (sag != lastSag)
    {
        lastSag = sag;
        lastSagChange = now;
    }

    const bool stale = sag > 1.0e-4 && now - lastSagChange > staleAfterSeconds;
    const float drive = std::round (juce::jlimit (0.0f, 1.0f, (float) (sag / fullGlowSag)) * glowSteps) / glowSteps;

    if (drive != shownDrive || stale != shownStale)
    {
        shownDrive = drive;
        shownStale = stale;

        // Only the vent: the rest of the face has not changed.
        if (const auto vent = getFaceLayout().vent; ! vent.isEmpty())
            repaint (vent.getSmallestIntegerContainer().expanded (1));
    }

    // visual-polish.md 4: the VU needle reads the master output, the header
    // meter's source, at its 30 Hz (gui-engine-dataflow.md 2).
    const auto& master = processor.getEngine().getMasterBus();
    const double rms = juce::jmax (master.getRmsLeft(), master.getRmsRight());

    if (rms != lastRms)
    {
        lastRms = rms;
        lastRmsChange = now;
    }

    const bool vuStale = rms > 1.0e-6 && now - lastRmsChange > staleAfterSeconds;
    vuTarget = faces::vuPositionFor (rms);

    if (AccessibilitySettings::get().isReducedMotion())
    {
        // accessibility.md 5: no easing, a hard set, and fewer of them.
        if (++reducedMotionTicks >= 3)
        {
            reducedMotionTicks = 0;
            shownVu = vuTarget;
        }
    }
    else
    {
        reducedMotionTicks = 0;
        shownVu += (vuTarget - shownVu) * vuBallistics;
    }

    if (std::abs (shownVu - paintedVu) > 1.0f / 256.0f || vuStale != shownVuStale)
    {
        paintedVu = shownVu;
        shownVuStale = vuStale;

        // Only the meter: the dial under the needle is part of the cached face.
        if (const auto meter = getFaceLayout().meter; ! meter.isEmpty())
            repaint (meter.getSmallestIntegerContainer().expanded (1));
    }
}

//==============================================================================
juce::Rectangle<float> AmpFacePanel::getFaceBounds() const
{
    return getLocalBounds().toFloat();
}

faces::AmpFaceLayout AmpFacePanel::getFaceLayout() const
{
    return faces::layoutAmpFace (getFaceBounds(), shownModel, style == Style::section);
}

faces::AmpFaceState AmpFacePanel::getFaceState() const
{
    faces::AmpFaceState state;
    state.standby = shownStandby;
    state.drive = shownDrive;
    state.driveStale = shownStale;
    state.vu = paintedVu;
    state.vuStale = shownVuStale;
    state.drawKnobs = false;       // the live knobs sit on the face
    state.drawSwitches = false;    // and so do the live switches
    state.drawMeter = false;       // the needle is drawn live over the cached dial
    state.hasSwitches = style == Style::section;
    state.enabled = isEnabled();
    return state;
}

AmpFacePanel::CacheKey AmpFacePanel::keyFor (float scale) const
{
    CacheKey key;
    key.model = shownModel;
    key.standby = shownStandby;
    key.enabled = isEnabled();
    key.width = getWidth();
    key.height = getHeight();
    key.scale = scale;
    key.palette = faces::paletteDigest();
    return key;
}

void AmpFacePanel::renderFace (float scale)
{
    const int w = juce::jmax (1, juce::roundToInt ((float) getWidth() * scale));
    const int h = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));

    // A software image: it draws the same in the plugin, in the tests and in the PNG renders.
    faceImage = juce::Image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());

    {
        juce::Graphics g (faceImage);
        g.addTransform (juce::AffineTransform::scale (scale));

        // The valves are drawn live over the cached face, so the glow can change without it.
        auto state = getFaceState();
        state.drawValves = false;
        faces::paintAmpFace (g, getFaceBounds(), shownModel, state);
    }

    cachedKey = keyFor (scale);
    ++faceRenders;
}

void AmpFacePanel::paint (juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const float scale = juce::jlimit (1.0f, 4.0f, g.getInternalContext().getPhysicalPixelScaleFactor());

    // visual-polish.md 0.3: the textures are drawn once, and again only when what they depict changes.
    if (faceImage.isNull() || keyFor (scale) != cachedKey)
        renderFace (scale);

    g.drawImage (faceImage, getLocalBounds().toFloat());
    faces::paintAmpFaceValves (g, getFaceBounds(), shownModel, getFaceState());
    faces::paintAmpFaceMeter (g, getFaceBounds(), shownModel, getFaceState());
}

void AmpFacePanel::resized()
{
    const auto l = getFaceLayout();

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        // The slider lands exactly on the face's knob; the knob's rows hang above and below it.
        const auto knob = l.knobs[i];
        const auto column = l.labels[i];
        knobs[i]->setBounds (juce::Rectangle<float> (column.getX(), knob.getY() - knobRow,
                                                     column.getWidth(), knob.getHeight() + knobRow * 2.0f).toNearestInt());
    }

    for (size_t i = 0; i < switches.size(); ++i)
        if (auto* toggle = switches[i].get())
            toggle->setBounds (l.switches[i].toNearestInt());
}

void AmpFacePanel::lookAndFeelChanged()
{
    // A palette change (accessibility.md 6) reaches the knob caps and the switches.
    faceLookAndFeel.refreshColours();
    repaint();
}

void AmpFacePanel::enablementChanged()
{
    repaint();
}

} // namespace luthier
