#include "RealismGroups.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    constexpr double kStaleSeconds = 2.0;   // gui-engine-dataflow.md

    double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    void styleHeading (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (Fonts::sectionHeader());
        label.setColour (juce::Label::textColourId, Palette::accent);
    }

    void styleReadout (juce::Label& label)
    {
        label.setFont (juce::Font (juce::FontOptions (9.5f)));
        label.setColour (juce::Label::textColourId, Palette::textMuted);
        label.setJustificationType (juce::Justification::topLeft);
    }

    /** Writes a plain value to a parameter as one gesture. */
    void writePlain (LuthierAudioProcessor& processor, const char* id, double plain)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) plain));
            p->endChangeGesture();
        }
    }

    juce::String stringName (LuthierAudioProcessor& processor, int s)
    {
        return TuningEngine::describeFrequency (processor.getEngine().getTuningEngine().getEffectiveOpenFrequency (s));
    }
}

//==============================================================================
//  STRING AGING
//==============================================================================
StringAgingGroup::StringAgingGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    styleHeading (heading, "STRING AGING");
    addAndMakeVisible (heading);

    auto attach = [this] (auto& control, const char* id, const char* tip)
    {
        control.attachTo (processor, id, tip);
        addAndMakeVisible (control);
    };

    attach (hours, ParamIDs::stringAgeHours,
            "Hours the set has been played. Fresh 0, broken in 12, old 120. Mirrors the STRINGS age in Column 1.");
    attach (coating, ParamIDs::stringCoating,
            "A polymer coating keeps grime and sweat out, so the set dulls three to five times slower. It does not stop fatigue.");
    attach (corrosivity, ParamIDs::stringCorrosivity,
            "Your sweat's chemistry: 1 is average. Corrosive hands rust plain strings fast.");
    attach (detail, ParamIDs::stringAgeDetail,
            "0 is the old three-step table, all strings alike. 1 is the full model: wound strings dull faster, plain ones rust, each string its own age.");
    attach (accrual, ParamIDs::stringAgeAccrual,
            "Lets the strings age as you play them - played time, per string, not the clock. Never moves the String Age control.");

    for (int s = 0; s < kMaxStrings; ++s)
    {
        auto* button = restringButtons.add (new juce::TextButton ("Restring"));
        button->setTooltip ("Replace this string only: it becomes new, the others keep their age.");
        button->onClick = [this, s] { restring (s); };
        addAndMakeVisible (button);
    }

    restringAllButton.setTooltip ("A new set: every string back to zero hours.");
    restringAllButton.onClick = [this] { restringAll(); };
    addAndMakeVisible (restringAllButton);

    startTimerHz (4);
}

StringAgingGroup::~StringAgingGroup()
{
    stopTimer();
}

void StringAgingGroup::restring (int stringIndex)
{
    const LuthierAudioProcessor::ScopedUndoAction undo (processor, "Restring string " + juce::String (stringIndex + 1));
    processor.getEngine().getStringAging().requestRestring (stringIndex);
}

void StringAgingGroup::restringAll()
{
    const LuthierAudioProcessor::ScopedUndoAction undo (processor, "Restring all");
    writePlain (processor, ParamIDs::stringAgeHours, 0.0);
    processor.getEngine().getStringAging().requestRestringAll();
}

bool StringAgingGroup::isStale() const noexcept
{
    return nowSeconds() - lastChange > kStaleSeconds;
}

void StringAgingGroup::timerCallback()
{
    const auto serial = processor.getEngine().getStringAging().getPublishSerial();

    if (serial != lastSerial)
    {
        lastSerial = serial;
        lastChange = nowSeconds();
    }

    const int n = processor.getEngine().getNumStrings();

    for (int s = 0; s < restringButtons.size(); ++s)
        restringButtons[s]->setVisible (s < n);

    if (isShowing())
        repaint();
}

int StringAgingGroup::preferredHeight() const
{
    return 20 + 3 * 24 + 2 * 38 + kMaxStrings * rowHeight + Metrics::buttonHeight + 12;
}

void StringAgingGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (2);
        return r;
    };

    heading.setBounds (take (20));
    hours.setBounds (take (22));

    {
        auto r = take (36);
        coating.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        accrual.setBounds (r);
    }

    corrosivity.setBounds (take (22));
    detail.setBounds (take (22));

    const int n = processor.getEngine().getNumStrings();

    for (int s = 0; s < restringButtons.size(); ++s)
    {
        if (s >= n)
            continue;

        auto r = bounds.removeFromTop (rowHeight);
        restringButtons[s]->setBounds (r.removeFromRight (70).reduced (0, 1));
    }

    bounds.removeFromTop (4);
    restringAllButton.setBounds (bounds.removeFromTop (Metrics::buttonHeight).removeFromLeft (110));
}

void StringAgingGroup::paint (juce::Graphics& g)
{
    // One row per string: effective hours and a brightness bar.
    if (restringButtons.isEmpty())
        return;

    const auto& aging = processor.getEngine().getStringAging();
    const bool stale = isStale();
    const int n = processor.getEngine().getNumStrings();

    for (int s = 0; s < juce::jmin (n, restringButtons.size()); ++s)
    {
        const auto buttonBounds = restringButtons[s]->getBounds();
        auto row = juce::Rectangle<int> (0, buttonBounds.getY(), buttonBounds.getX() - 4, buttonBounds.getHeight());

        const double h = aging.getPublishedHours (s);
        const double b = aging.getPublishedBrightness (s);

        g.setColour (stale ? Palette::textDisabled : Palette::textMuted);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (stringName (processor, s) + "  " + juce::String (h, 1) + " h",
                    row.removeFromLeft (juce::jmin (110, row.getWidth() / 2)), juce::Justification::centredLeft);

        auto bar = row.reduced (0, 4).toFloat();
        g.setColour (Palette::panelSunken);
        g.fillRoundedRectangle (bar, 2.0f);

        g.setColour (stale ? Palette::textDisabled : Palette::secondary);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.0, 1.0, b)), 2.0f);

        g.setColour (Palette::textPrimary.withAlpha (stale ? 0.4f : 0.8f));
        g.drawText (juce::String (juce::roundToInt (100.0 * b)) + " %", bar.toNearestInt(), juce::Justification::centred);
    }
}

//==============================================================================
//  ENVIRONMENT
//==============================================================================
void EnvironmentSparkline::push (float cents)
{
    ring[(size_t) head] = cents;
    head = (head + 1) % kPoints;
    count = juce::jmin (kPoints, count + 1);
    repaint();
}

void EnvironmentSparkline::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds, 3.0f);

    if (count < 2)
        return;

    float lo = 0.0f, hi = 0.0f;

    for (int i = 0; i < count; ++i)
    {
        lo = juce::jmin (lo, ring[(size_t) i]);
        hi = juce::jmax (hi, ring[(size_t) i]);
    }

    const float span = juce::jmax (1.0f, hi - lo);
    auto area = bounds.reduced (3.0f);
    juce::Path path;

    for (int i = 0; i < count; ++i)
    {
        const int index = (head - count + i + kPoints) % kPoints;
        const float x = area.getX() + area.getWidth() * (float) (kPoints - count + i) / (float) (kPoints - 1);
        const float y = area.getBottom() - area.getHeight() * (ring[(size_t) index] - lo) / span;

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    // The zero line, where the guitar was tuned.
    const float zeroY = area.getBottom() - area.getHeight() * (0.0f - lo) / span;
    g.setColour (Palette::edge);
    g.drawHorizontalLine ((int) zeroY, area.getX(), area.getRight());

    g.setColour (stale ? Palette::textDisabled : Palette::secondary);
    g.strokePath (path, juce::PathStrokeType (1.2f));
}

EnvironmentGroup::EnvironmentGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    styleHeading (heading, "ENVIRONMENT");
    addAndMakeVisible (heading);

    auto attach = [this] (auto& control, const char* id, const char* tip)
    {
        control.attachTo (processor, id, tip);
        addAndMakeVisible (control);
    };

    attach (temperature, ParamIDs::envTemperatureC,
            "The room. Strings follow in seconds, the neck in minutes: heat makes a guitar go flat, wound strings most. "
            "Freezing and nylon's glass transition are not modelled.");
    attach (tunedAt, ParamIDs::envTunedAtC, "The temperature the guitar was tuned at. Retune sets it.");
    attach (humidity, ParamIDs::envHumidityPct,
            "The humidity the wood has acclimatised to. Humid wood raises an acoustic's action and lowers its top's modes; dry wood back-bows the neck.");
    attach (profile, ParamIDs::envProfile,
            "A session: stage lights warm the rig, an evening outdoors cools it, a cold case warms up on the stand.");
    attach (clock, ParamIDs::envClock,
            "Host timeline follows the song position, so a bounce matches playback. Free-running counts from load.");

    retuneButton.setTooltip ("Tune up: every string back in tune at the current temperatures, and the tuners start drifting again.");
    retuneButton.onClick = [this] { retune(); };
    addAndMakeVisible (retuneButton);

    styleReadout (readout);
    styleReadout (centsReadout);
    addAndMakeVisible (readout);
    addAndMakeVisible (centsReadout);

    sparkline.setTitle ("Low E offset, last 60 s");
    addAndMakeVisible (sparkline);

    // gui-integration.md 14's empty-state style.
    convolutionNote.setText ("Body shift applies to modal bodies and to string coupling", juce::dontSendNotification);
    convolutionNote.setFont (juce::Font (juce::FontOptions (9.5f)).italicised());
    convolutionNote.setColour (juce::Label::textColourId, Palette::textDisabled);
    addChildComponent (convolutionNote);

    startTimerHz (10);
}

EnvironmentGroup::~EnvironmentGroup()
{
    stopTimer();
}

void EnvironmentGroup::retune()
{
    auto& engine = processor.getEngine();
    auto& env = engine.getEnvironment();

    double mean = 0.0;
    const int n = juce::jmax (1, engine.getNumStrings());

    for (int s = 0; s < n; ++s)
        mean += env.getPublishedStringTemp (s) / n;

    const double shown = std::round (mean * 10.0) / 10.0;

    const LuthierAudioProcessor::ScopedUndoAction undo (processor, "Retune");
    env.requestRetune (shown);
    engine.getCharacterEngine().retune();
    writePlain (processor, ParamIDs::envTunedAtC, shown);
}

bool EnvironmentGroup::isStale() const noexcept
{
    return nowSeconds() - lastChange > kStaleSeconds;
}

void EnvironmentGroup::timerCallback()
{
    auto& engine = processor.getEngine();
    const auto& env = engine.getEnvironment();
    const auto serial = env.getPublishSerial();

    if (serial != lastSerial)
    {
        lastSerial = serial;
        lastChange = nowSeconds();
    }

    const bool stale = isStale();
    const int n = engine.getNumStrings();
    double stringMean = 0.0;

    for (int s = 0; s < n; ++s)
        stringMean += env.getPublishedStringTemp (s) / juce::jmax (1, n);

    readout.setText ("strings " + juce::String (stringMean, 1) + " C   neck " + juce::String (env.getPublishedNeckTemp(), 1)
                       + " C   body " + juce::String (env.getPublishedBodyTemp(), 1) + " C   wood RH "
                       + juce::String (env.getPublishedRh(), 1) + " %",
                     juce::dontSendNotification);

    juce::String cents;

    for (int s = 0; s < n; ++s)
    {
        const double c = env.getPublishedCents (s);
        cents << stringName (processor, s) << " " << (c >= 0.0 ? "+" : "") << juce::String (c, 1) << "   ";
    }

    centsReadout.setText ("cents  " + cents.trimEnd(), juce::dontSendNotification);

    const auto colour = stale ? Palette::textDisabled : Palette::textMuted;
    readout.setColour (juce::Label::textColourId, colour);
    centsReadout.setColour (juce::Label::textColourId, colour);

    sparkline.push ((float) env.getPublishedCents (juce::jmax (0, n - 1)));
    sparkline.setStale (stale);

    convolutionNote.setVisible (engine.getBodyEngine().getMode() == BodyEngine::Mode::Convolution);
}

int EnvironmentGroup::preferredHeight() const
{
    return 20 + 3 * 24 + 38 + Metrics::buttonHeight + 4 + 14 + 14 + 40 + 14 + 8;
}

void EnvironmentGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (2);
        return r;
    };

    heading.setBounds (take (20));
    temperature.setBounds (take (22));
    tunedAt.setBounds (take (22));
    humidity.setBounds (take (22));

    {
        auto r = take (36);
        profile.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        clock.setBounds (r);
    }

    retuneButton.setBounds (take (Metrics::buttonHeight).removeFromLeft (90));
    readout.setBounds (take (14));
    centsReadout.setBounds (take (14));
    sparkline.setBounds (take (40));
    convolutionNote.setBounds (take (14));
}

//==============================================================================
//  BODY COUPLING
//==============================================================================
void BodyModeList::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds.toFloat(), 3.0f);

    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    auto header = bounds.removeFromTop (rowHeight).reduced (4, 0);
    g.setColour (Palette::textDisabled);
    g.drawText ("Hz        Q       Q'      mass kg    |Y| m/Ns", header, juce::Justification::centredLeft);

    g.setColour (Palette::textMuted);

    for (int k = 0; k < juce::jmin (maxRows, modes.size()); ++k)
    {
        const auto& m = modes.getReference (k);
        auto row = bounds.removeFromTop (rowHeight).reduced (4, 0);
        g.drawText (juce::String (m.hz, 1).paddedRight (' ', 10) + juce::String (m.q, 1).paddedRight (' ', 8)
                      + juce::String (m.loadedQ, 1).paddedRight (' ', 8) + juce::String (m.massKg, 3).paddedRight (' ', 11)
                      + juce::String (m.peakAdmittance, 4) + (m.isAir ? "  air" : ""),
                    row, juce::Justification::centredLeft);
    }
}

void WolfMap::setMap (const std::vector<float>& cells, int numStrings)
{
    map = cells;
    strings = numStrings;
    repaint();
}

float WolfMap::getCell (int s, int fret) const noexcept
{
    if (! juce::isPositiveAndBelow (s, strings) || ! juce::isPositiveAndBelow (fret, kFrets + 1))
        return 0.0f;

    return map[(size_t) (s * (kFrets + 1) + fret)];
}

void WolfMap::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds, 3.0f);

    if (strings <= 0)
        return;

    auto area = bounds.reduced (4.0f);
    const float cellW = area.getWidth() / (float) (kFrets + 1);
    const float cellH = area.getHeight() / (float) strings;

    for (int s = 0; s < strings; ++s)
    {
        for (int f = 0; f <= kFrets; ++f)
        {
            const float loss = getCell (s, f);
            auto cell = juce::Rectangle<float> (area.getX() + f * cellW, area.getY() + s * cellH, cellW, cellH).reduced (0.75f);

            if (loss >= kWarnLoss)
            {
                // gui-integration 21: colour and a dot, so it reads without colour.
                g.setColour (Palette::warning);
                g.fillRect (cell);
                g.setColour (Palette::panelSunken);
                g.fillEllipse (cell.withSizeKeepingCentre (3.0f, 3.0f));
            }
            else
            {
                g.setColour (Palette::accent.withAlpha (juce::jlimit (0.08f, 0.9f, loss / kWarnLoss)));
                g.fillRect (cell);
            }
        }
    }
}

BodyCouplingGroup::BodyCouplingGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    styleHeading (heading, "BODY COUPLING");
    addAndMakeVisible (heading);

    auto attach = [this] (auto& control, const char* id, const char* tip)
    {
        control.attachTo (processor, id, tip);
        addAndMakeVisible (control);
    };

    attach (mass, ParamIDs::bodyModeMassScale,
            "How heavy the body's modes are at the bridge. Lighter couples harder: stronger wolves, more sympathetic ring.");
    attach (q, ParamIDs::bodyModeQScale, "How sharply the body's modes ring. A light, lively top is high Q.");
    attach (tuning, ParamIDs::bodyModeFreqScale,
            "Moves every body mode. +-10 % is the spread between two nominally identical guitars - and it moves the wolf.");
    attach (modes, ParamIDs::bodyCouplingModes, "How many of the body's strongest modes load the strings.");

    mapHeading.setText ("Wolf map: predicted sustain loss per fret (dot: over 30 %)", juce::dontSendNotification);
    styleReadout (mapHeading);
    addAndMakeVisible (mapHeading);

    modeList.setTooltip ("The body seen from the bridge: each mode's frequency, Q, Q loaded by the strings, effective mass and peak admittance.");
    wolfMap.setTooltip ("Where a string partial meets a strong, light body mode the note loses sustain: a wolf. "
                        "Computed from the modes, not measured.");
    addAndMakeVisible (modeList);
    addAndMakeVisible (wolfMap);

    tapButton.setTooltip ("Knock on the top: the open strings near a body mode answer.");
    tapButton.onClick = [this] { processor.getEngine().requestBodyTap (0.8); };
    addAndMakeVisible (tapButton);

    startTimerHz (4);
    refreshNow();
}

BodyCouplingGroup::~BodyCouplingGroup()
{
    stopTimer();
}

std::vector<float> BodyCouplingGroup::computeWolfMap (const LuthierAudioProcessor& p, int& numStrings)
{
    auto& processor = const_cast<LuthierAudioProcessor&> (p);
    auto& engine = processor.getEngine();
    const auto& bank = engine.getBodyCoupling();
    const auto& design = bank.getStagedDesign();

    numStrings = juce::jmin (engine.getNumStrings(), design.numStrings);

    std::array<double, kMaxStrings> openHz {}, sigma {};

    for (int s = 0; s < numStrings; ++s)
    {
        openHz[(size_t) s] = engine.getTuningEngine().getEffectiveOpenFrequency (s);

        // The string's own open T60 as StringEngine sets it (engine spec 5).
        const auto& str = engine.getString (s);
        const double t60 = str.getPhysical().sustainSeconds * str.getAgingSustain()
                           * std::pow (110.0 / juce::jmax (20.0, openHz[(size_t) s]), 0.40);
        sigma[(size_t) s] = 6.907755 / juce::jmax (0.05, t60);
    }

    std::vector<float> map ((size_t) (numStrings * (WolfMap::kFrets + 1)), 0.0f);

    if (numStrings > 0)
        BodyCouplingBank::wolfMap (design, bank.getModeCount(), engine.getBodyCouplingScaling(), bank.getAmount(),
                                   openHz.data(), sigma.data(), numStrings, WolfMap::kFrets, map.data());

    return map;
}

void BodyCouplingGroup::refreshNow()
{
    auto& engine = processor.getEngine();
    const auto& bank = engine.getBodyCoupling();
    const auto& design = bank.getStagedDesign();
    const auto scaling = engine.getBodyCouplingScaling();

    // A digest of everything the list and the map depend on.
    juce::uint64 digest = 1469598103934665603ull;
    auto mix = [&digest] (double x) { digest = (digest ^ (juce::uint64) (juce::int64) std::llround (x * 1.0e6)) * 1099511628211ull; };

    mix (scaling.plateFreq); mix (scaling.airFreq); mix (scaling.q); mix (scaling.airQ); mix (scaling.mass);
    mix (bank.getAmount()); mix (bank.getModeCount()); mix (design.count); mix (design.coupling);
    mix (engine.getNumStrings());

    for (int k = 0; k < design.count; ++k)
    {
        mix (design.f[(size_t) k]);
        mix (design.m[(size_t) k]);
    }

    for (int s = 0; s < engine.getNumStrings(); ++s)
        mix (engine.getTuningEngine().getEffectiveOpenFrequency (s));

    if (digest == lastDigest)
        return;

    lastDigest = digest;

    juce::Array<BodyCouplingBank::ModeView> views;

    for (int k = 0; k < juce::jmin (bank.getModeCount(), design.count); ++k)
        views.add (BodyCouplingBank::viewMode (design, k, scaling));

    modeList.setModes (views);

    int n = 0;
    const auto map = computeWolfMap (processor, n);
    wolfMap.setMap (map, n);
}

void BodyCouplingGroup::timerCallback()
{
    if (isShowing())
        refreshNow();
}

int BodyCouplingGroup::preferredHeight() const
{
    return 20 + 3 * 24 + 38 + (BodyModeList::maxRows + 1) * BodyModeList::rowHeight + 4 + 14 + 96 + 4
           + Metrics::buttonHeight + 8;
}

void BodyCouplingGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (2);
        return r;
    };

    heading.setBounds (take (20));
    mass.setBounds (take (22));
    q.setBounds (take (22));
    tuning.setBounds (take (22));
    modes.setBounds (take (36).removeFromLeft (160));
    modeList.setBounds (take ((BodyModeList::maxRows + 1) * BodyModeList::rowHeight));
    bounds.removeFromTop (2);
    mapHeading.setBounds (take (14));
    wolfMap.setBounds (take (96));
    tapButton.setBounds (take (Metrics::buttonHeight).removeFromLeft (80));
}

} // namespace luthier
