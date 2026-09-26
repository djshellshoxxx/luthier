#include "NoiseGroups.h"
#include "../PluginProcessor.h"
#include "UiPreferences.h"

namespace luthier
{

//==============================================================================
//  NoiseEventStrip
//==============================================================================
NoiseEventStrip::NoiseEventStrip (LuthierAudioProcessor& p)
    : processor (p)
{
    setTitle ("Noise events");
    setTooltip ("The last eight seconds of playing noise: squeak, click, chirp, scrape, buzz "
                "and clank, as ticks. Taller is louder.");
    motion.startTimerHz (*this, 30);
}

NoiseEventStrip::~NoiseEventStrip()
{
    motion.stopTimer();
}

juce::Colour NoiseEventStrip::colourFor (NoiseClass c)
{
    switch (c)
    {
        case NoiseClass::squeak:     return Palette::secondary;
        case NoiseClass::pickClick:  return Palette::accent;
        case NoiseClass::pickChirp:  return Palette::accentBright;
        case NoiseClass::pickScrape: return Palette::warning.darker (0.3f);
        case NoiseClass::fretBuzz:   return Palette::warning;
        case NoiseClass::clank:      return Palette::textMuted;
        case NoiseClass::numClasses:
        default:                     return Palette::textDisabled;
    }
}

bool NoiseEventStrip::isStale() const noexcept
{
    return juce::Time::getMillisecondCounterHiRes() * 0.001 - lastEventTime > kStaleSeconds;
}

int NoiseEventStrip::pollNow()
{
    std::array<NoiseEngine::EventRecord, 64> records;
    auto& pool = processor.getEngine().getPlayingNoise().getPool();

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (;;)
    {
        const int n = pool.drainEvents (records.data(), (int) records.size());

        for (int i = 0; i < n; ++i)
            shown.add ({ now, records[(size_t) i].noiseClass, records[(size_t) i].level });

        if (n > 0)
            lastEventTime = now;

        if (n < (int) records.size())
            break;
    }

    // Drop what has scrolled off the left.
    while (! shown.isEmpty() && now - shown.getFirst().time > kWindowSeconds)
        shown.remove (0);

    return shown.size();
}

bool NoiseEventStrip::isEnabledByUser()
{
    return UiPreferences::get().getBool ("appearance.noiseStrip", true);
}

void NoiseEventStrip::setEnabledByUser (bool enabled)
{
    UiPreferences::get().setBool ("appearance.noiseStrip", enabled);
}

std::array<int, (size_t) NoiseClass::numClasses> NoiseEventStrip::getClassCounts() const
{
    std::array<int, (size_t) NoiseClass::numClasses> counts {};

    for (const auto& tick : shown)
        if (juce::isPositiveAndBelow ((int) tick.noiseClass, (int) NoiseClass::numClasses))
            ++counts[(size_t) tick.noiseClass];

    return counts;
}

void NoiseEventStrip::timerCallback()
{
    // gui-integration 5: Options -> Appearance can hide the strip.
    if (const bool wanted = isEnabledByUser(); wanted != isVisible() && getParentComponent() != nullptr)
        setVisible (wanted);

    // Drained even when hidden, or the ring fills and the first thing a user
    // sees on opening the tab is a burst of stale events.
    // cpu-quality-modes 7, E1 (was performance-budget.md 8 step 1): under CPU
    // load the display drain halves.
    if (processor.getQualityController().getReliefLevel() >= 1 && (++reliefTick & 1) != 0)
        return;

    const int before = shown.size();
    pollNow();

    // gui-integration 21 / ui-wiring 10: under reduced motion the strip is a
    // static count, redrawn at most five times a second and only on change.
    if (! AnimationPolicy::get().mayAnimate (AnimationPolicy::Decorative))
    {
        const auto counts = getClassCounts();

        if (isShowing() && counts != shownCounts && juce::Time::getMillisecondCounter() - lastStaticPaint >= 200)
        {
            shownCounts = counts;
            lastStaticPaint = juce::Time::getMillisecondCounter();
            repaint();
        }

        return;
    }

    if (isShowing() && (shown.size() != before || ! shown.isEmpty()))
        repaint();
}

void NoiseEventStrip::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds, 3.0f);

    // Reduced motion: a static count per kind instead of scrolling ticks.
    if (! AnimationPolicy::get().mayAnimate (AnimationPolicy::Decorative))
    {
        static const char* const names[] = { "squeak", "click", "chirp", "scrape", "buzz", "clank" };
        const auto counts = getClassCounts();
        auto area = bounds.reduced (6.0f, 0.0f);
        g.setFont (Fonts::mono (10.0f));

        for (int c = 0; c < (int) NoiseClass::numClasses && c < 6; ++c)
        {
            const auto cell = area.removeFromLeft (area.getWidth() / (float) juce::jmax (1, 6 - c));
            g.setColour (counts[(size_t) c] > 0 ? colourFor ((NoiseClass) c) : Palette::textDisabled);
            g.drawText (juce::String (names[c]) + " " + juce::String (counts[(size_t) c]), cell, juce::Justification::centredLeft, true);
        }

        return;
    }

    const bool stale = isStale();
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (const auto& tick : shown)
    {
        const double age = now - tick.time;
        const float x = bounds.getRight() - 2.0f - (float) (age / kWindowSeconds) * (bounds.getWidth() - 4.0f);

        // Height by level, on a dB scale: noise spans 40 dB and linear would
        // show only the loudest.
        const float db = (float) gainToDb (juce::jmax (1.0e-6, (double) tick.level));
        const float h = juce::jlimit (3.0f, bounds.getHeight() - 4.0f,
                                      juce::jmap (db, -60.0f, 0.0f, 3.0f, bounds.getHeight() - 4.0f));

        g.setColour (stale ? Palette::textDisabled : colourFor (tick.noiseClass));
        g.fillRect (juce::Rectangle<float> (x - 1.0f, bounds.getBottom() - 2.0f - h, 2.0f, h));
    }

    if (shown.isEmpty())
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (10.0f));
        g.drawText ("No playing noise in the last 8 s", bounds, juce::Justification::centred);
    }
}

//==============================================================================
//  NoiseGroups
//==============================================================================
const NoiseGroups::SqueakStyle& NoiseGroups::getSqueakStyle (int index)
{
    // string-squeak.md 8. Silent sets the amount only.
    static const SqueakStyle styles[kNumSqueakStyles] =
    {
        { "Silent",            0.00, -1.0, -1.0, -1.0 },
        { "Studio (polished)", 0.15, 0.35, 0.60, 0.35 },
        { "Natural",           0.35, 0.65, 0.35, 0.50 },
        { "Folk / close-mic",  0.60, 0.80, 0.20, 0.65 },
        { "Exaggerated",       1.00, 1.00, 0.05, 0.90 },
    };

    return styles[juce::jlimit (0, kNumSqueakStyles - 1, index)];
}

NoiseGroups::NoiseGroups (LuthierAudioProcessor& p)
    : processor (p), eventStrip (p)
{
    auto heading = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (Fonts::sectionHeader());
        label.setColour (juce::Label::textColourId, Palette::accent);
        addAndMakeVisible (label);
    };

    heading (stringNoiseHeading, "STRING NOISE");
    heading (pickHeading, "PICK");

    // ---- STRING NOISE --------------------------------------------------------------
    for (int i = 0; i < kNumSqueakStyles; ++i)
        styleBox.addItem (getSqueakStyle (i).name, i + 1);

    styleBox.setTooltip ("Squeak styles set amount, probability, moisture and pressure together");
    styleBox.setTitle ("Squeak style");
    styleBox.onChange = [this]
    {
        if (styleBox.getSelectedId() > 0)
            applySqueakStyle (styleBox.getSelectedId() - 1);
    };
    addAndMakeVisible (styleBox);

    auto attach = [this] (LuthierSlider& slider, const char* id, const char* tip)
    {
        slider.attachTo (processor, id, tip);
        addAndMakeVisible (slider);
    };

    attach (squeakAmount, ParamIDs::squeakAmount,
            "How loud a finger squeaks along a wound string. Plain strings never squeak.");
    attach (squeakProbability, ParamIDs::squeakProbability,
            "The chance a shift squeaks at all. Not every shift does.");
    attach (squeakMoisture, ParamIDs::squeakMoisture,
            "Dry fingers catch and squeak; damp ones slide quietly and duller");
    attach (squeakPressure, ParamIDs::squeakPressure,
            "How hard the finger presses while it moves. Easing off is how players keep shifts quiet.");
    attach (squeakMinTravel, ParamIDs::squeakMinTravel,
            "Shifts shorter than this never squeak, so small moves do not chatter");

    // string-squeak.md 9: a mirror of the string part. Its canonical home is
    // the Workshop; until the Workshop exists it edits the string material
    // parameter that Column 1 also shows.
    windingMaterial.attachTo (processor, ParamIDs::stringMaterial,
                              "The winding sets the squeak: flatwound almost none, coated little, "
                              "80/20 bronze and stainless the most");
    addAndMakeVisible (windingMaterial);

    addAndMakeVisible (eventStrip);

    // ---- PICK ---------------------------------------------------------------------------
    pickMaterial.attachTo (processor, ParamIDs::pickMaterial,
                           "Density sets the click's pitch, damping its length, the surface the chirp");
    addAndMakeVisible (pickMaterial);

    // pick-noise.md 8 / gui-integration 19: a mirror of the RHYTHM tab's STRUM group.
    strikerDown.attachTo (processor, ParamIDs::strumStrikerDown, "What crosses the strings on a down-strum");
    strikerUp.attachTo (processor, ParamIDs::strumStrikerUp, "What crosses the strings on an up-strum");
    addAndMakeVisible (strikerDown);
    addAndMakeVisible (strikerUp);

    attach (pickThickness, ParamIDs::pickThickness, "0.38 to 3 mm. Thicker is louder, lower and shorter.");
    attach (pickTip, ParamIDs::pickTipRadius, "Sharp tips click brighter and let go faster");
    attach (pickBevel, ParamIDs::pickBevel, "A bevelled edge releases the string more gradually");
    attach (pickWear, ParamIDs::pickWear, "Wear rounds the tip and roughens the surface");
    attach (pickAngle, ParamIDs::pickAngle,
            "0 to 60 degrees. Flat to the string clicks hardest; angled, it slides and chirps.");
    attach (pickClick, ParamIDs::pickClickAmount, "The attack transient of pick on string");
    attach (pickChirp, ParamIDs::pickChirpAmount, "The scrape as the pick leaves a wound string");
    attach (pickScrape, ParamIDs::pickScrapeAmount, "A deliberate rake down the wound strings");

    startTimerHz (4);
    timerCallback();
}

NoiseGroups::~NoiseGroups()
{
    stopTimer();
}

void NoiseGroups::applySqueakStyle (int index)
{
    const auto& style = getSqueakStyle (index);
    auto& state = processor.getState();

    const LuthierAudioProcessor::ScopedUndoAction undo (processor, juce::String ("Squeak style ") + style.name);

    auto write = [&state] (const char* id, double plain)
    {
        if (plain < 0.0)
            return;

        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) plain));
            parameter->endChangeGesture();
        }
    };

    write (ParamIDs::squeakStyle, (double) index);
    write (ParamIDs::squeakAmount, style.amount);
    write (ParamIDs::squeakProbability, style.probability);
    write (ParamIDs::squeakMoisture, style.moisture);
    write (ParamIDs::squeakPressure, style.pressure);

    timerCallback();
}

juce::String NoiseGroups::describeSqueakStyle() const
{
    auto& state = processor.getState();

    auto plain = [&state] (const char* id)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
            return (double) parameter->convertFrom0to1 (parameter->getValue());

        return 0.0;
    };

    const int index = juce::roundToInt (plain (ParamIDs::squeakStyle));
    const auto& style = getSqueakStyle (index);

    auto same = [] (double a, double b) { return b < 0.0 || std::abs (a - b) < 0.005; };

    const bool matches = same (plain (ParamIDs::squeakAmount), style.amount)
                      && same (plain (ParamIDs::squeakProbability), style.probability)
                      && same (plain (ParamIDs::squeakMoisture), style.moisture)
                      && same (plain (ParamIDs::squeakPressure), style.pressure);

    return juce::String (style.name) + (matches ? "" : " (modified)");
}

void NoiseGroups::timerCallback()
{
    const auto text = describeSqueakStyle();

    if (styleBox.getText() != text)
        styleBox.setText (text, juce::dontSendNotification);
}

int NoiseGroups::preferredHeight() const
{
    constexpr int row = 24, heading = 20, choice = 38;

    return heading + row + 5 * row + choice + NoiseEventStrip::preferredHeight + 8
         + heading + choice + 2 * choice + 8 * row + 8;
}

void NoiseGroups::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void NoiseGroups::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (2);
        return r;
    };

    stringNoiseHeading.setBounds (take (20));
    styleBox.setBounds (take (24).reduced (0, 1));

    for (auto* s : { &squeakAmount, &squeakProbability, &squeakMoisture, &squeakPressure, &squeakMinTravel })
        s->setBounds (take (22));

    windingMaterial.setBounds (take (36));
    eventStrip.setBounds (take (NoiseEventStrip::preferredHeight));

    bounds.removeFromTop (6);

    pickHeading.setBounds (take (20));
    pickMaterial.setBounds (take (36));
    strikerDown.setBounds (take (36));
    strikerUp.setBounds (take (36));

    for (auto* s : { &pickThickness, &pickTip, &pickBevel, &pickWear, &pickAngle,
                     &pickClick, &pickChirp, &pickScrape })
        s->setBounds (take (22));
}

} // namespace luthier
