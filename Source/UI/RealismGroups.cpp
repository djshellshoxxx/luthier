#include "RealismGroups.h"
#include "../PluginProcessor.h"
#include "../Presets/RealismStyles.h"
#include "../Presets/RealismStyleActions.h"

namespace luthier
{

namespace
{
    constexpr double kStaleSeconds = 2.0;   // gui-engine-dataflow.md

    double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    void styleHeading (juce::Label& label, const juce::String& text, bool sub = false)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (sub ? Fonts::ui (10.0f, true) : Fonts::sectionHeader());
        label.setColour (juce::Label::textColourId, sub ? Palette::textMuted : Palette::accent);
    }

    double plainValue (LuthierAudioProcessor& processor, const char* id)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
            return (double) p->convertFrom0to1 (p->getValue());

        return 0.0;
    }

    /** A style box: its items, and picking one runs `apply`. */
    void setUpStyleBox (juce::ComboBox& box, const juce::StringArray& names, const juce::String& tooltip,
                        std::function<void (int)> apply)
    {
        for (int i = 0; i < names.size(); ++i)
            box.addItem (names[i], i + 1);

        box.setTooltip (tooltip);
        box.onChange = [&box, apply]
        {
            if (box.getSelectedId() > 0)
                apply (box.getSelectedId() - 1);
        };
    }

    void showStyleText (juce::ComboBox& box, const juce::String& text)
    {
        if (box.getText() != text)
            box.setText (text, juce::dontSendNotification);
    }

    /** The heard pitch offsets' causes, in StabilityModel::Cause order. */
    const char* causeName (int cause)
    {
        static const char* names[] = { "settling", "nut binding", "tuner backlash", "bridge", "bend memory", "capo" };
        return names[juce::jlimit (0, 5, cause)];
    }
}

//==============================================================================
//  PositionPad
//==============================================================================
PositionPad::PositionPad (LuthierAudioProcessor& p)
    : processor (p)
{
    setTitle ("Player position");
    setTooltip ("Where you stand relative to the amp. Drag toward or away from the amp to set the "
                "distance; turn the mouse wheel to face away. A single coil is a loop antenna: "
                "turning 90 degrees nulls most of the hum.");
    startTimerHz (10);
}

PositionPad::~PositionPad() { stopTimer(); }

double PositionPad::plain (const char* id) const { return plainValue (processor, id); }

void PositionPad::write (const char* id, double value, bool gesture)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
    {
        if (gesture) p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
        if (gesture) p->endChangeGesture();
    }
}

void PositionPad::setDistanceFromY (float y)
{
    // Top is the amp. Log spacing: 0.3 m at the top, 5 m at the bottom (stock).
    auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::noisePlayerDistance));

    if (p == nullptr)
        return;

    const auto range = p->getNormalisableRange();
    const double lo = juce::jmax (0.05, (double) range.start), hi = range.end;
    const double frac = juce::jlimit (0.0, 1.0, (double) (y - 16.0f) / (padHeight - 22.0));
    const double d = lo * std::pow (hi / lo, frac);
    p->setValueNotifyingHost (p->convertTo0to1 ((float) d));
}

void PositionPad::mouseDown (const juce::MouseEvent& e)
{
    if (auto* p = processor.getState().getParameter (ParamIDs::noisePlayerDistance))
        p->beginChangeGesture();

    setDistanceFromY ((float) e.y);
}

void PositionPad::mouseDrag (const juce::MouseEvent& e) { setDistanceFromY ((float) e.y); }

void PositionPad::mouseUp (const juce::MouseEvent&)
{
    if (auto* p = processor.getState().getParameter (ParamIDs::noisePlayerDistance))
        p->endChangeGesture();
}

void PositionPad::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const double angle = juce::jlimit (0.0, 90.0, plain (ParamIDs::noisePlayerAngle) + wheel.deltaY * 30.0);
    write (ParamIDs::noisePlayerAngle, angle, true);
}

void PositionPad::timerCallback()
{
    const double a = plain (ParamIDs::noisePlayerAngle), d = plain (ParamIDs::noisePlayerDistance);

    if (a != shownAngle || d != shownDistance)
    {
        shownAngle = a;
        shownDistance = d;
        repaint();
    }
}

void PositionPad::paint (juce::Graphics& g)
{
    auto pad = getLocalBounds().removeFromTop (padHeight).toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (pad, 3.0f);

    // The amp at the top.
    auto amp = juce::Rectangle<float> (pad.getCentreX() - 18.0f, pad.getY() + 3.0f, 36.0f, 10.0f);
    g.setColour (Palette::edgeBright);
    g.fillRoundedRectangle (amp, 2.0f);
    g.setColour (Palette::textDisabled);
    g.setFont (Fonts::ui (8.0f));
    g.drawText ("AMP", amp, juce::Justification::centred);

    // The player: a circle at the distance, a line for the way the pickups face.
    auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::noisePlayerDistance));
    const double lo = p != nullptr ? juce::jmax (0.05, (double) p->getNormalisableRange().start) : 0.3;
    const double hi = p != nullptr ? (double) p->getNormalisableRange().end : 5.0;
    const double d = juce::jlimit (lo, hi, plain (ParamIDs::noisePlayerDistance));
    const float y = pad.getY() + 16.0f + (float) (std::log (d / lo) / std::log (hi / lo)) * (padHeight - 22.0f);
    const float x = pad.getCentreX();

    g.setColour (Palette::accent);
    g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);

    const float theta = (float) juce::degreesToRadians (plain (ParamIDs::noisePlayerAngle));
    g.drawLine (x, y, x + 14.0f * std::sin (theta), y - 14.0f * std::cos (theta), 2.0f);

    // The hum gain beneath: g_pos in dB.
    const double gain = NoiseFloor::positionGain (plain (ParamIDs::noisePlayerAngle), d);
    g.setColour (Palette::textMuted);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (juce::String (d, 1) + " m, " + juce::String (juce::roundToInt (plain (ParamIDs::noisePlayerAngle)))
                  + " deg: hum " + (gain >= 1.0 ? "+" : "") + juce::String (gainToDb (gain), 1) + " dB",
                getLocalBounds().removeFromBottom (14), juce::Justification::centredLeft, false);
}

//==============================================================================
//  NoiseMeter
//==============================================================================
NoiseMeter::NoiseMeter (LuthierAudioProcessor& p) : processor (p)
{
    setTitle ("Noise floor level");
    startTimerHz (10);
}

NoiseMeter::~NoiseMeter() { stopTimer(); }

bool NoiseMeter::isStale() const noexcept { return nowSeconds() - lastUpdateTime > kStaleSeconds; }

void NoiseMeter::pollNow()
{
    const auto& nf = processor.getEngine().getNoiseFloor();
    const auto updates = nf.getMeterUpdates();

    if (updates != lastUpdates)
    {
        lastUpdates = updates;
        lastUpdateTime = nowSeconds();
        shownDb = nf.getMeterDb();
    }
}

void NoiseMeter::timerCallback()
{
    pollNow();

    if (isShowing())
        repaint();
}

void NoiseMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds, 2.0f);

    const bool stale = isStale();

    // -120 to -20 dB re the reference pluck.
    const float frac = (float) juce::jlimit (0.0, 1.0, (shownDb + 120.0) / 100.0);
    g.setColour (stale ? Palette::textDisabled : Palette::secondary);
    g.fillRoundedRectangle (bounds.withWidth (bounds.getWidth() * frac).reduced (1.0f), 2.0f);

    g.setColour (stale ? Palette::textDisabled : Palette::textPrimary);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (stale ? juce::String ("noise floor: off") : "noise floor " + juce::String (shownDb, 1) + " dB re pluck",
                bounds.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
//  NoiseFloorGroup
//==============================================================================
NoiseFloorGroup::NoiseFloorGroup (LuthierAudioProcessor& p)
    : processor (p), pad (p), meter (p)
{
    styleHeading (heading, "NOISE FLOOR");
    styleHeading (guitarLabel, "Guitar", true);
    styleHeading (rigLabel, "Rig", true);

    for (auto* l : { &heading, &guitarLabel, &rigLabel })
        addAndMakeVisible (*l);

    setUpStyleBox (styleBox, RealismStyles::noiseFloorStyleNames(),
                   "Noise floor styles set every source at once; Off keeps the single-coil hum where it is",
                   [this] (int i) { applyNoiseFloorStyle (processor, i); timerCallback(); });
    styleBox.setTitle ("Noise floor style");
    addAndMakeVisible (styleBox);

    mains.attachTo (processor, ParamIDs::noiseMainsHz, "The mains frequency where you play: 60 Hz in the Americas, 50 Hz most elsewhere");
    addAndMakeVisible (mains);

    auto knob = [this] (LuthierKnob& k, const char* id, const char* tip)
    {
        k.attachTo (processor, id, tip);
        addAndMakeVisible (k);
    };

    knob (hum, ParamIDs::ampBuzz, "Single-coil hum: the pickups as a loop antenna. A humbucker cancels it.");
    knob (fluorescent, ParamIDs::noiseFluorescent, "Fluorescent tubes and dimmers buzz at twice the mains, into single coils");
    knob (passive, ParamIDs::noisePassiveHiss, "The coil and pots' own thermal hiss. 1 is physical: inaudible without a lot of gain.");
    knob (cable, ParamIDs::noiseCableMovement, "Thumps and crackle when the cable moves. Longer and cheaper cables are louder.");
    knob (radio, ParamIDs::noiseRadio, "A long cable picks up faint, garbled AM radio");
    knob (ground, ParamIDs::noiseGroundLoop, "Hum from separate earths, at the amp input: no pickup or volume knob changes it");
    knob (hiss, ParamIDs::noiseAmpHiss, "The first valve's own hiss; the amp's gain raises it. 0.5 is typical, 1 a tired tube.");
    knob (microphonics, ParamIDs::noiseMicrophonics, "The speaker shaking the first valve: a ping after loud notes");

    addAndMakeVisible (pad);
    addAndMakeVisible (meter);

    toAux8.attachTo (processor, ParamIDs::noiseFloorToAux8,
                     "Also put the noise floor (and the hum) on Aux 8, as an identification stem");
    addAndMakeVisible (toAux8);

    startTimerHz (4);
    timerCallback();
}

NoiseFloorGroup::~NoiseFloorGroup() { stopTimer(); }

void NoiseFloorGroup::timerCallback()
{
    showStyleText (styleBox, describeNoiseFloorStyle (processor));
}

int NoiseFloorGroup::preferredHeight() const
{
    const int knobs = LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small);
    return 20 + 38 + 14 + knobs + 14 + knobs + 4 + PositionPad::preferredHeight + 4
         + NoiseMeter::preferredHeight + 4 + Metrics::buttonHeight + 4;
}

void NoiseFloorGroup::resized()
{
    auto bounds = getLocalBounds();
    auto take = [&bounds] (int h) { auto r = bounds.removeFromTop (h); bounds.removeFromTop (2); return r; };

    heading.setBounds (take (18));

    {
        auto row = take (36);
        mains.setBounds (row.removeFromRight (juce::jmin (110, row.getWidth() / 3)));
        row.removeFromRight (4);
        styleBox.setBounds (row.removeFromBottom (24));
    }

    const int knobH = LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small);

    auto knobRow = [&] (std::initializer_list<LuthierKnob*> knobs)
    {
        auto row = take (knobH);
        const int w = row.getWidth() / 5;

        for (auto* k : knobs)
            k->setBounds (row.removeFromLeft (w));
    };

    guitarLabel.setBounds (take (12));
    knobRow ({ &hum, &fluorescent, &passive, &cable, &radio });
    rigLabel.setBounds (take (12));
    knobRow ({ &ground, &hiss, &microphonics });

    pad.setBounds (take (PositionPad::preferredHeight).withWidth (juce::jmax (PositionPad::padWidth + 100, bounds.getWidth())));
    meter.setBounds (take (NoiseMeter::preferredHeight));
    toAux8.setBounds (take (Metrics::buttonHeight));
}

//==============================================================================
//  PitchOffsetReadout
//==============================================================================
PitchOffsetReadout::PitchOffsetReadout (LuthierAudioProcessor& p) : processor (p)
{
    setTitle ("Tension pitch per string");
    startTimerHz (30);
}

PitchOffsetReadout::~PitchOffsetReadout() { stopTimer(); }

bool PitchOffsetReadout::isStale() const noexcept { return nowSeconds() - lastUpdateTime > kStaleSeconds; }

void PitchOffsetReadout::pollNow()
{
    auto& engine = processor.getEngine();
    const auto blocks = engine.getStabilityModel().getBlockCount();

    if (blocks != lastBlocks)
    {
        lastBlocks = blocks;
        lastUpdateTime = nowSeconds();
    }

    for (int s = 0; s < kMaxStrings; ++s)
        shown[(size_t) s] = s < engine.getNumStrings() ? engine.getString (s).getTensionCents() : 0.0;
}

void PitchOffsetReadout::timerCallback()
{
    pollNow();

    if (isShowing())
        repaint();
}

void PitchOffsetReadout::paint (juce::Graphics& g)
{
    const int n = juce::jlimit (1, kMaxStrings, processor.getEngine().getNumStrings());
    auto bounds = getLocalBounds();
    const int w = bounds.getWidth() / n;
    const bool stale = isStale();

    g.setFont (Fonts::mono (9.5f));

    for (int s = 0; s < n; ++s)
    {
        auto cell = bounds.removeFromLeft (w).reduced (1);
        g.setColour (Palette::panelSunken);
        g.fillRect (cell);
        g.setColour (stale ? Palette::textDisabled : (shown[(size_t) s] > 0.5 ? Palette::accent : Palette::textMuted));
        g.drawText ("+" + juce::String (shown[(size_t) s], 1), cell, juce::Justification::centred, false);
    }
}

//==============================================================================
//  SustainShapeGroup
//==============================================================================
SustainShapeGroup::SustainShapeGroup (LuthierAudioProcessor& p)
    : processor (p), readout (p)
{
    styleHeading (heading, "SUSTAIN SHAPE");
    styleHeading (attackLabel, "Attack", true);
    styleHeading (decayLabel, "Decay", true);
    styleHeading (releaseLabel, "Release", true);

    for (auto* l : { &heading, &attackLabel, &decayLabel, &releaseLabel })
        addAndMakeVisible (*l);

    setUpStyleBox (styleBox, RealismStyles::sustainStyleNames(),
                   "Sustain styles set all eight shape values; Legacy is the plain decay presets were made with",
                   [this] (int i) { applySustainStyle (processor, i); timerCallback(); });
    styleBox.setTitle ("Sustain style");
    addAndMakeVisible (styleBox);

    auto slider = [this] (LuthierSlider& s, const char* id, const char* tip)
    {
        s.attachTo (processor, id, tip);
        addAndMakeVisible (s);
    };

    slider (transient, ParamIDs::sustainAttackTransient,
            "A hard pick's bright, overshooting attack, and the string's metallic longitudinal 'clank'");
    slider (attackTime, ParamIDs::sustainAttackTime, "How fast the attack's extra brightness fades");
    slider (fastShare, ParamIDs::sustainFastShare,
            "How much of the energy is in the fast first stage of the decay. 0 is one straight decay.");
    slider (fastRatio, ParamIDs::sustainFastRatio, "How much faster the first stage decays than the long tail");
    slider (tension, ParamIDs::sustainTensionMod,
            "A hard-hit string plays sharp and settles into tune. 1 is physical: the wound bass strings wander most.");
    slider (releaseTime, ParamIDs::sustainReleaseTime, "The finger lifting over a few milliseconds instead of a dead stop");
    slider (releaseSag, ParamIDs::sustainReleaseSag, "The pitch sags as the fingertip rides the string off the fret");
    slider (releaseRing, ParamIDs::sustainReleaseRing, "A clumsy lift plucks the open string, quietly");

    addAndMakeVisible (readout);

    startTimerHz (4);
    timerCallback();
}

SustainShapeGroup::~SustainShapeGroup() { stopTimer(); }

void SustainShapeGroup::timerCallback()
{
    showStyleText (styleBox, describeSustainStyle (processor));
}

int SustainShapeGroup::preferredHeight() const
{
    return 20 + 26 + 3 * 14 + 8 * 24 + PitchOffsetReadout::preferredHeight + 6;
}

void SustainShapeGroup::resized()
{
    auto bounds = getLocalBounds();
    auto take = [&bounds] (int h) { auto r = bounds.removeFromTop (h); bounds.removeFromTop (2); return r; };

    heading.setBounds (take (18));
    styleBox.setBounds (take (24));

    attackLabel.setBounds (take (12));
    transient.setBounds (take (22));
    attackTime.setBounds (take (22));

    decayLabel.setBounds (take (12));
    fastShare.setBounds (take (22));
    fastRatio.setBounds (take (22));
    tension.setBounds (take (22));

    releaseLabel.setBounds (take (12));
    releaseTime.setBounds (take (22));
    releaseSag.setBounds (take (22));
    releaseRing.setBounds (take (22));

    readout.setBounds (take (PitchOffsetReadout::preferredHeight));
}

//==============================================================================
//  OffsetStrip
//==============================================================================
OffsetStrip::OffsetStrip (LuthierAudioProcessor& p) : processor (p)
{
    setTitle ("Tuning offsets per string");
    setTooltip ("Each string's offset from pitch, coloured by its main cause. Click a bar to retune that string.");
    startTimerHz (10);
}

OffsetStrip::~OffsetStrip() { stopTimer(); }

juce::Colour OffsetStrip::colourFor (int cause)
{
    switch (cause)
    {
        case StabilityModel::settle:   return Palette::secondary;
        case StabilityModel::nut:      return Palette::accent;
        case StabilityModel::backlash: return Palette::warning;
        case StabilityModel::bridge:   return Palette::accentBright;
        case StabilityModel::memory:   return Palette::secondaryDim.brighter (0.4f);
        case StabilityModel::capo:     return Palette::textMuted;
        default:                       return Palette::textDisabled;
    }
}

juce::String OffsetStrip::describe (LuthierAudioProcessor& processor, int s)
{
    const auto& model = processor.getEngine().getStabilityModel();
    juce::String text = "String " + juce::String (s + 1) + ": "
                      + juce::String (model.getTotalCents (s), 2) + " c";

    for (int c = 0; c < StabilityModel::numCauses; ++c)
    {
        const double v = model.getCauseCents (s, (StabilityModel::Cause) c);

        if (std::abs (v) >= 0.01)
            text << "\n  " << causeName (c) << " " << (v >= 0.0 ? "+" : "") << juce::String (v, 2) << " c";
    }

    return text + "\nClick to retune this string.";
}

bool OffsetStrip::isStale() const noexcept { return nowSeconds() - lastUpdateTime > kStaleSeconds; }

void OffsetStrip::pollNow()
{
    const auto& model = processor.getEngine().getStabilityModel();
    const auto blocks = model.getBlockCount();

    if (blocks != lastBlocks)
    {
        lastBlocks = blocks;
        lastUpdateTime = nowSeconds();
    }

    numStrings = juce::jlimit (1, kMaxStrings, processor.getEngine().getNumStrings());

    for (int s = 0; s < kMaxStrings; ++s)
    {
        shown[(size_t) s] = model.getTotalCents (s);

        int best = -1;
        double bestAbs = 0.0;

        for (int c = 0; c < StabilityModel::numCauses; ++c)
        {
            const double v = std::abs (model.getCauseCents (s, (StabilityModel::Cause) c));
            if (v > bestAbs) { bestAbs = v; best = c; }
        }

        dominant[(size_t) s] = best;
    }
}

void OffsetStrip::timerCallback()
{
    pollNow();

    if (isShowing())
        repaint();
}

int OffsetStrip::stringAt (juce::Point<int> p) const
{
    const int s = (p.y - 2) / (barHeight + 2);
    return juce::isPositiveAndBelow (s, numStrings) ? s : -1;
}

void OffsetStrip::mouseMove (const juce::MouseEvent& e)
{
    const int s = stringAt (e.getPosition());
    setTooltip (s >= 0 ? describe (processor, s)
                       : juce::String ("Each string's offset from pitch. Click a bar to retune that string."));
}

void OffsetStrip::mouseDown (const juce::MouseEvent& e)
{
    // tuning-stability.md 3: a command, not a value edit, so not undoable.
    const int s = stringAt (e.getPosition());

    if (s >= 0)
        processor.getEngine().getStabilityModel().requestRetune (1u << s);
}

void OffsetStrip::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced (0, 2);
    const bool stale = isStale();
    const float centre = (float) bounds.getCentreX();
    const float halfWidth = bounds.getWidth() * 0.5f - 24.0f;

    g.setFont (Fonts::mono (9.0f));

    for (int s = 0; s < numStrings; ++s)
    {
        auto row = bounds.removeFromTop (barHeight);
        bounds.removeFromTop (2);

        g.setColour (Palette::panelSunken);
        g.fillRect (row);
        g.setColour (Palette::edge);
        g.drawVerticalLine ((int) centre, (float) row.getY(), (float) row.getBottom());

        const double c = shown[(size_t) s];
        const float len = (float) juce::jlimit (-1.0, 1.0, c / 20.0) * halfWidth;   // +/-20 c full scale
        const auto colour = stale ? Palette::textDisabled : colourFor (dominant[(size_t) s]);

        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (juce::jmin (centre, centre + len), (float) row.getY() + 2.0f,
                                            std::abs (len), (float) barHeight - 4.0f));

        // gui-integration.md 21: a dot glyph per cause, so it reads in monochrome.
        if (std::abs (c) >= 0.05 && dominant[(size_t) s] >= 0)
            for (int d = 0; d <= dominant[(size_t) s]; ++d)
                g.fillEllipse ((float) row.getX() + 2.0f + (float) d * 4.0f, (float) row.getCentreY() - 1.5f, 3.0f, 3.0f);

        g.setColour (stale ? Palette::textDisabled : Palette::textMuted);
        g.drawText ((c >= 0.0 ? "+" : "") + juce::String (c, 1), row.removeFromRight (22), juce::Justification::centredRight, false);
    }
}

//==============================================================================
//  TuningStabilityGroup
//==============================================================================
TuningStabilityGroup::TuningStabilityGroup (LuthierAudioProcessor& p)
    : processor (p), strip (p)
{
    auto slider = [this] (LuthierSlider& s, const char* id, const char* tip)
    {
        s.attachTo (processor, id, tip);
        addAndMakeVisible (s);
    };

    slider (amount, ParamIDs::stabilityAmount,
            "How much the guitar goes out of tune from how it is played. 0 plays exactly in tune.");
    slider (settling, ParamIDs::stabilitySettling, "New strings go flat as they stretch in; each retune takes some out for good");
    slider (nut, ParamIDs::stabilityNutBinding, "A bent string sticks in the nut and comes back sharp until it pings free");
    slider (backlash, ParamIDs::stabilityBacklash, "A tuner brought to pitch from above gives way on the next big bend");
    slider (creep, ParamIDs::stabilitySaddleCreep,
            "A floating bridge moves every string when one is retuned; the saddles creep; a trem returns a little off");
    slider (memory, ParamIDs::stabilityBendMemory, "Heavy bends stretch the string for good, and it goes flat");
    slider (capo, ParamIDs::stabilityCapoBias, "A capo pulls the strings it clamps sharp");

    autoRetune.attachTo (processor, ParamIDs::stabilityAutoRetune,
                         "Retune all by itself: after 10 s of silence, when the transport stops, both, or never");
    addAndMakeVisible (autoRetune);

    addAndMakeVisible (strip);

    capoFigure.setFont (Fonts::ui (9.5f));
    capoFigure.setColour (juce::Label::textColourId, Palette::textDisabled);
    addAndMakeVisible (capoFigure);

    startTimerHz (2);
    timerCallback();
}

TuningStabilityGroup::~TuningStabilityGroup() { stopTimer(); }

juce::String TuningStabilityGroup::describeCapoBias (LuthierAudioProcessor& processor)
{
    const auto& hw = processor.getEngine().getStabilityModel().getHardware();
    const int fret = juce::jmax (1, processor.getEngine().getTuningEngine().getCapoFret());
    double lo = 1.0e9, hi = 0.0;

    for (int s = 0; s < hw.numStrings; ++s)
    {
        const double b = StabilityModel::capoBiasCents (hw, s, fret);
        lo = juce::jmin (lo, b);
        hi = juce::jmax (hi, b);
    }

    return "Capo bias at fret " + juce::String (fret) + ": +" + juce::String (lo, 1) + " to +" + juce::String (hi, 1) + " c";
}

void TuningStabilityGroup::timerCallback()
{
    capoFigure.setText (describeCapoBias (processor), juce::dontSendNotification);

    const int n = processor.getEngine().getNumStrings();

    if (n != shownStrings)
    {
        shownStrings = n;

        if (auto* parent = getParentComponent())
            parent->resized();
    }
}

int TuningStabilityGroup::preferredHeight() const
{
    return 7 * 24 + 38 + OffsetStrip::preferredHeightFor (juce::jmax (6, shownStrings)) + 16 + 4;
}

void TuningStabilityGroup::resized()
{
    auto bounds = getLocalBounds();
    auto take = [&bounds] (int h) { auto r = bounds.removeFromTop (h); bounds.removeFromTop (2); return r; };

    for (auto* s : { &amount, &settling, &nut, &backlash, &creep, &memory, &capo })
        s->setBounds (take (22));

    autoRetune.setBounds (take (36));
    strip.setBounds (take (OffsetStrip::preferredHeightFor (juce::jmax (6, shownStrings))));
    capoFigure.setBounds (take (14));
}

//==============================================================================
//  DecaySketch and DecayRow
//==============================================================================
DecaySketch::DecaySketch (LuthierAudioProcessor& p) : processor (p)
{
    setTitle ("Decay sketch");
    startTimerHz (4);
}

DecaySketch::~DecaySketch() { stopTimer(); }

double DecaySketch::envelopeDb (double t, double t60, double a, double rho)
{
    // sustain-and-decay.md 3: E(t) = a e^(-2t/tf) + (1 - a) e^(-2t/ts).
    const double tauS = juce::jmax (1.0e-3, t60) / 6.907755278982137;
    const double tauF = juce::jmax (1.0e-4, rho * tauS);
    const double energy = a * std::exp (-2.0 * t / tauF) + (1.0 - a) * std::exp (-2.0 * t / tauS);
    return 10.0 * std::log10 (juce::jmax (1.0e-30, energy));
}

void DecaySketch::timerCallback()
{
    // Redraws on a parameter change only (8).
    const std::array<double, 4> now { plainValue (processor, ParamIDs::sustainFastShare),
                                      plainValue (processor, ParamIDs::sustainFastRatio),
                                      plainValue (processor, ParamIDs::sustainScale),
                                      (double) processor.getEngine().getNumStrings() };

    if (now != shownValues)
    {
        shownValues = now;
        ++redraws;
        repaint();
    }
}

void DecaySketch::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds, 3.0f);

    // The open low E: the lowest string's own T60 at its pitch.
    auto& engine = processor.getEngine();
    const int low = juce::jmax (0, engine.getNumStrings() - 1);
    const auto& spec = engine.getStringSpec (low);
    const double hz = juce::jmax (20.0, engine.getTuningEngine().getEffectiveOpenFrequency (low));
    const double t60 = spec.sustainSeconds * std::pow (110.0 / hz, 0.40) * plainValue (processor, ParamIDs::sustainScale);
    const double a = plainValue (processor, ParamIDs::sustainFastShare);
    const double rho = plainValue (processor, ParamIDs::sustainFastRatio);

    auto toPoint = [&bounds] (double t, double db)
    {
        return juce::Point<float> (bounds.getX() + 2.0f + (float) (t / 4.0) * (bounds.getWidth() - 4.0f),
                                   bounds.getY() + 2.0f + (float) juce::jlimit (0.0, 1.0, -db / 60.0) * (bounds.getHeight() - 4.0f));
    };

    juce::Path legacy, current;

    for (int i = 0; i <= 80; ++i)
    {
        const double t = 4.0 * i / 80.0;
        const auto pl = toPoint (t, envelopeDb (t, t60, 0.0, rho));
        const auto pc = toPoint (t, envelopeDb (t, t60, a, rho));
        if (i == 0) { legacy.startNewSubPath (pl); current.startNewSubPath (pc); }
        else        { legacy.lineTo (pl); current.lineTo (pc); }
    }

    // Legacy dotted, current solid (8).
    juce::Path dotted;
    const float dashes[] = { 2.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dotted, legacy, dashes, 2);
    g.setColour (Palette::textDisabled);
    g.fillPath (dotted);

    g.setColour (Palette::accent);
    g.strokePath (current, juce::PathStrokeType (1.5f));

    g.setColour (Palette::textDisabled);
    g.setFont (Fonts::ui (8.5f));
    g.drawText ("open low E, 4 s, 60 dB", bounds.reduced (4.0f, 2.0f), juce::Justification::topRight, false);
}

DecayRow::DecayRow (LuthierAudioProcessor& p)
    : processor (p), sketch (p)
{
    styleHeading (heading, "Decay", true);
    addAndMakeVisible (heading);

    setUpStyleBox (styleBox, RealismStyles::sustainStyleNames(),
                   "How a plucked note decays: Legacy is one straight line; the others add the attack, "
                   "the fast first stage and the finger's release. Details on the CHARACTER tab.",
                   [this] (int i) { applySustainStyle (processor, i); timerCallback(); });
    styleBox.setTitle ("Sustain style");
    addAndMakeVisible (styleBox);
    addAndMakeVisible (sketch);

    startTimerHz (4);
    timerCallback();
}

DecayRow::~DecayRow() { stopTimer(); }

void DecayRow::timerCallback()
{
    showStyleText (styleBox, describeSustainStyle (processor));
}

void DecayRow::resized()
{
    auto bounds = getLocalBounds();
    heading.setBounds (bounds.removeFromTop (16));
    styleBox.setBounds (bounds.removeFromTop (24));
    bounds.removeFromTop (4);
    sketch.setBounds (bounds);
}

//==============================================================================
//  StabilityBadge
//==============================================================================
StabilityBadge::StabilityBadge (LuthierAudioProcessor& p, int s)
    : processor (p), stringIndex (s)
{
    label.setFont (Fonts::mono (9.5f));
    label.setColour (juce::Label::textColourId, Palette::accent);
    label.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (label);

    retune.setTooltip ("Brings this string back to pitch, the way you would at the tuner");
    retune.onClick = [this] { processor.getEngine().getStabilityModel().requestRetune (1u << stringIndex); };
    addChildComponent (retune);

    startTimerHz (10);
    timerCallback();
}

StabilityBadge::~StabilityBadge() { stopTimer(); }

void StabilityBadge::timerCallback()
{
    // 6: "+3 c" when the offset is at least a cent, with a Retune button.
    const double c = processor.getEngine().getStabilityModel().getTotalCents (stringIndex);
    const bool show = std::abs (c) >= 1.0;
    const auto text = show ? juce::String (c >= 0.0 ? "+" : "") + juce::String (juce::roundToInt (c)) + " c" : juce::String();

    if (label.getText() != text)
        label.setText (text, juce::dontSendNotification);

    if (retune.isVisible() != show)
        retune.setVisible (show);
}

void StabilityBadge::resized()
{
    auto bounds = getLocalBounds();
    retune.setBounds (bounds.removeFromRight (46).reduced (0, 1));
    label.setBounds (bounds);
}

//==============================================================================
juce::StringArray describeTuningFigures (LuthierAudioProcessor& processor, GuitarSlot slot, const Part* part)
{
    juce::StringArray lines;

    if (part == nullptr)
        return lines;

    auto hw = processor.getEngine().getStabilityModel().getHardware();
    const int low = juce::jmax (0, hw.numStrings - 1);

    if (slot == GuitarSlot::tuners)
    {
        hw.tunerRatio = part->number ("ratio", hw.tunerRatio);
        hw.tunerStability = part->number ("stability", hw.tunerStability);
        lines.add ("Backlash on the lowest string: " + juce::String (StabilityModel::backlashCents (hw, low), 1)
                   + " c; on the highest: " + juce::String (StabilityModel::backlashCents (hw, 0), 1) + " c");
    }
    else if (slot == GuitarSlot::nut)
    {
        hw.nutFriction = part->number ("friction", hw.nutFriction);
        lines.add ("Nut binding after a full bend: +" + juce::String (0.03 * hw.effectiveNutFriction() * 200.0, 1) + " c");
    }

    return lines;
}

} // namespace luthier
