#include "SetupGroup.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
//  BuzzHeatmap
//==============================================================================
BuzzHeatmap::BuzzHeatmap (LuthierAudioProcessor& p)
    : processor (p)
{
    setTitle ("Buzz heatmap");
    setTooltip ("Each fret, each string: how close it is to buzzing right now. "
                "Filled with a dot is buzzing; outlined is within 0.05 mm.");
    motion.startTimerHz (*this, 30);
}

BuzzHeatmap::~BuzzHeatmap()
{
    motion.stopTimer();
}

BuzzHeatmap::CellState BuzzHeatmap::stateFor (float excessMm) noexcept
{
    if (excessMm > 0.0f)
        return CellState::buzzing;

    if (excessMm > -kNearMm)
        return CellState::near;

    return CellState::clear;
}

bool BuzzHeatmap::isStale() const noexcept
{
    // SPEC-SWEEP (GD-13): stale once the fade has finished.
    return freshnessFor (juce::Time::getMillisecondCounterHiRes() * 0.001 - lastChange) <= 0.0f;
}

void BuzzHeatmap::timerCallback()
{
    // A digest of the whole map, so an unchanging map (no audio, or nothing
    // near a fret) is what "stale" means.
    const auto& buzz = processor.getEngine().getFretBuzz();
    juce::uint64 digest = 1469598103934665603ull;

    for (int s = 0; s < processor.getEngine().getNumStrings(); ++s)
        for (int f = 1; f <= SetupGeometry::kMaxFrets; ++f)
        {
            const auto cell = (juce::uint64) (juce::int64) std::llround (buzz.getHeat (s, f) * 1000.0f);
            digest = (digest ^ cell) * 1099511628211ull;
        }

    if (digest != lastDigest)
    {
        lastDigest = digest;
        lastChange = juce::Time::getMillisecondCounterHiRes() * 0.001;
    }

    if (isShowing())
        repaint();
}

void BuzzHeatmap::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds, 3.0f);

    const auto& engine = processor.getEngine();
    const auto& buzz = engine.getFretBuzz();
    const int strings = juce::jmax (1, engine.getNumStrings());
    const int frets = buzz.getGeometry().numFrets;

    auto area = bounds.reduced (4.0f);
    const float cellW = area.getWidth() / (float) frets;
    const float cellH = area.getHeight() / (float) strings;
    const bool stale = isStale();
    const float freshness = freshnessFor (juce::Time::getMillisecondCounterHiRes() * 0.001 - lastChange);   // GD-13

    for (int s = 0; s < strings; ++s)
    {
        for (int f = 1; f <= frets; ++f)
        {
            auto cell = juce::Rectangle<float> (area.getX() + (f - 1) * cellW, area.getY() + s * cellH,
                                                cellW, cellH).reduced (0.75f);

            const auto state = stateFor (buzz.getHeat (s, f));

            if (stale || state == CellState::clear)
            {
                g.setColour (Palette::edge.withAlpha (0.35f));
                g.fillRect (cell);
                continue;
            }

            // SPEC-SWEEP (GD-13): a going-stale map fades over the empty cell.
            g.setColour (Palette::edge.withAlpha (0.35f));
            g.fillRect (cell);

            if (state == CellState::buzzing)
            {
                g.setColour (Palette::accent.withMultipliedAlpha (freshness));
                g.fillRect (cell);

                g.setColour (Palette::panelSunken.withMultipliedAlpha (freshness));
                g.fillEllipse (cell.withSizeKeepingCentre (3.0f, 3.0f));
            }
            else
            {
                g.setColour (Palette::warning.withMultipliedAlpha (freshness));
                g.drawRect (cell, 1.0f);
            }
        }
    }

    // Fret markers at 3, 5, 7, 9, 12 so the map reads as a neck.
    g.setColour (Palette::textDisabled);

    for (int marker : { 3, 5, 7, 9, 12, 15, 17, 19 })
        if (marker <= frets)
            g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f)
                             .withCentre ({ area.getX() + (marker - 0.5f) * cellW, bounds.getBottom() - 2.0f }));
}

//==============================================================================
//  SetupGroup
//==============================================================================
SetupGroup::SetupGroup (LuthierAudioProcessor& p)
    : processor (p), heatmap (p)
{
    heading.setText ("SETUP", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    for (int i = 0; i < kNumSetupStyles; ++i)
        styleBox.addItem (getSetupStyle (i).name, i + 1);

    styleBox.setTitle ("Setup style");
    styleBox.setTooltip ("A tech's setups: each sets the action and the relief together");
    styleBox.onChange = [this]
    {
        if (styleBox.getSelectedId() > 0)
            applySetupStyle (styleBox.getSelectedId() - 1);
    };
    addAndMakeVisible (styleBox);

    auto attach = [this] (LuthierSlider& slider, const juce::String& id, const char* tip)
    {
        slider.attachTo (processor, id, tip);
        addAndMakeVisible (slider);
    };

    attach (actionTreble, ParamIDs::setupActionTreble, "String to fret at the 12th fret, high E. Low is fast and rattles.");
    attach (actionBass, ParamIDs::setupActionBass, "The same on the low E, which always sits higher");
    attach (relief, ParamIDs::setupRelief, "The neck's bow at fret 7. Negative is back-bow, which buzzes.");
    attach (fretHeight, ParamIDs::setupFretHeight, "Taller frets make a harder, brighter contact when they buzz");
    attach (threshold, ParamIDs::setupBuzzThreshold,
            "A sensitivity trim of up to 0.15 mm. Not a mute: a bad setup still buzzes at 0.");

    for (int n = 1; n <= ParamIDs::kNumNutDepths; ++n)
    {
        auto* slider = nutDepths.add (new LuthierSlider ("Nut " + juce::String (n)));
        attach (*slider, ParamIDs::setupNutDepth (n), "Nut slot depth for this string: open-string clearance");
    }

    sitarMode.attachTo (processor, ParamIDs::setupSitarMode,
                        "A curved jawari bridge: continuous, shimmering buzz on purpose");
    addAndMakeVisible (sitarMode);

    addAndMakeVisible (heatmap);

    startTimerHz (4);
    timerCallback();
}

SetupGroup::~SetupGroup()
{
    stopTimer();
}

void SetupGroup::applySetupStyle (int index)
{
    const auto& style = getSetupStyle (index);
    const LuthierAudioProcessor::ScopedUndoAction undo (processor, juce::String ("Setup style ") + style.name);

    auto& state = processor.getState();

    auto write = [&state] (const char* id, double plain)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) plain));
            parameter->endChangeGesture();
        }
    };

    write (ParamIDs::setupStyle, (double) index);
    write (ParamIDs::setupActionTreble, style.actionTreble);
    write (ParamIDs::setupActionBass, style.actionBass);
    write (ParamIDs::setupRelief, style.relief);

    timerCallback();
}

juce::String SetupGroup::describeSetupStyle() const
{
    auto& state = processor.getState();

    auto plain = [&state] (const char* id)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
            return (double) parameter->convertFrom0to1 (parameter->getValue());

        return 0.0;
    };

    const auto& style = getSetupStyle (juce::roundToInt (plain (ParamIDs::setupStyle)));

    const bool matches = std::abs (plain (ParamIDs::setupActionTreble) - style.actionTreble) < 0.005
                      && std::abs (plain (ParamIDs::setupActionBass) - style.actionBass) < 0.005
                      && std::abs (plain (ParamIDs::setupRelief) - style.relief) < 0.005;

    return juce::String (style.name) + (matches ? "" : " (modified)");
}

void SetupGroup::timerCallback()
{
    const auto text = describeSetupStyle();

    if (styleBox.getText() != text)
        styleBox.setText (text, juce::dontSendNotification);
}

int SetupGroup::preferredHeight() const
{
    return 20 + 26 + 5 * 24 + 3 * 24 + Metrics::buttonHeight + 4 + BuzzHeatmap::preferredHeight + 8;
}

void SetupGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (2);
        return r;
    };

    heading.setBounds (take (20));
    styleBox.setBounds (take (24).reduced (0, 1));

    for (auto* s : { &actionTreble, &actionBass, &relief, &fretHeight, &threshold })
        s->setBounds (take (22));

    // Nut depths two to a row: six full-width rows would push the heatmap -
    // the part that explains all of this - off the bottom of the panel.
    for (int row = 0; row < 3; ++row)
    {
        auto r = take (22);
        nutDepths[row * 2]->setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        nutDepths[row * 2 + 1]->setBounds (r);
    }

    sitarMode.setBounds (take (Metrics::buttonHeight).removeFromLeft (140));
    heatmap.setBounds (take (BuzzHeatmap::preferredHeight));
}

} // namespace luthier
