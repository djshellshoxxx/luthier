#include "RoutingPanel.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    /** Draws a small square toggle button in the mixer-strip style. */
    void drawMiniButton (juce::Graphics& g, juce::Rectangle<int> bounds,
                         const juce::String& text, bool on, juce::Colour onColour)
    {
        const auto area = bounds.toFloat().reduced (1.0f);

        g.setColour (on ? onColour : Palette::panelSunken);
        g.fillRoundedRectangle (area, 2.0f);

        g.setColour (on ? onColour.brighter (0.3f) : Palette::edge);
        g.drawRoundedRectangle (area, 2.0f, 1.0f);

        g.setColour (on ? Palette::backgroundDeep : Palette::textMuted);
        g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());
        g.drawText (text, bounds, juce::Justification::centred, false);
    }

    /** A horizontal level meter, drawn on a decibel scale because a linear one
        spends most of its length on signals nobody can hear. */
    void drawMeter (juce::Graphics& g, juce::Rectangle<int> bounds, float level)
    {
        g.setColour (Palette::panelSunken);
        g.fillRect (bounds);

        const auto db = juce::Decibels::gainToDecibels (level, -60.0f);
        const auto norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);

        if (norm > 0.0f)
        {
            auto filled = bounds.toFloat().withWidth (bounds.toFloat().getWidth() * norm);

            g.setColour (level >= 1.0f ? Palette::clip
                                       : (db > -6.0f ? Palette::warning : Palette::success));
            g.fillRect (filled);
        }

        g.setColour (Palette::edge);
        g.drawRect (bounds, 1);
    }

    juce::String formatSamples (int samples, double sampleRate)
    {
        const double ms = (sampleRate > 0.0) ? (1000.0 * (double) samples / sampleRate) : 0.0;

        return juce::String (samples) + " sm (" + juce::String (ms, 2) + " ms)";
    }
}

//==============================================================================
AuxStrip::AuxStrip (LuthierAudioProcessor& p, int busIndex)
    : processor (p), bus (busIndex)
{
    setTooltip (juce::String (getAuxBusName (bus)) + " - " + getAuxBusTapDescription (bus));

    gain.setRange (-60.0, 12.0, 0.1);
    gain.setValue (routing().getAuxGainDb (bus), juce::dontSendNotification);
    gain.setDoubleClickReturnValue (true, 0.0);
    gain.setTextValueSuffix (" dB");

    gain.onValueChange = [this]
    {
        routing().setAuxGainDb (bus, gain.getValue());
    };

    addAndMakeVisible (gain);
}

AuxStrip::~AuxStrip() = default;

RoutingMatrix& AuxStrip::routing()
{
    return processor.getRouting();
}

void AuxStrip::refresh()
{
    const auto level = (float) routing().getAuxLevel (bus);
    const bool mute = routing().isAuxMuted (bus);
    const bool solo = routing().isAuxSoloed (bus);
    const bool audible = routing().isAuxAudible (bus);

    // Meters fall back smoothly rather than snapping, so a short note still
    // leaves something readable behind.
    const float decayed = juce::jmax (level, displayedLevel * 0.78f);

    if (std::abs (decayed - displayedLevel) > 0.001f
          || mute != lastMute || solo != lastSolo || audible != lastAudible)
    {
        displayedLevel = decayed;
        lastMute = mute;
        lastSolo = solo;
        lastAudible = audible;
        repaint();
    }
}

void AuxStrip::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (Palette::textPrimary.withAlpha (lastAudible ? 1.0f : 0.45f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (getAuxBusName (bus), bounds.removeFromLeft (92).reduced (2, 0),
                juce::Justification::centredLeft, true);

    drawMiniButton (g, muteBounds, "M", lastMute, Palette::clip);
    drawMiniButton (g, soloBounds, "S", lastSolo, Palette::warning);
    drawMeter (g, meterBounds, lastAudible ? displayedLevel : 0.0f);
}

void AuxStrip::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromLeft (92);

    muteBounds = bounds.removeFromLeft (20).reduced (0, 6);
    soloBounds = bounds.removeFromLeft (20).reduced (0, 6);

    bounds.removeFromLeft (Metrics::gridHalf);

    meterBounds = bounds.removeFromRight (56).reduced (0, 10);

    bounds.removeFromRight (Metrics::gridHalf);
    gain.setBounds (bounds.reduced (0, 4));
}

void AuxStrip::mouseDown (const juce::MouseEvent& e)
{
    if (muteBounds.contains (e.getPosition()))
    {
        routing().setAuxMuted (bus, ! routing().isAuxMuted (bus));
        refresh();
    }
    else if (soloBounds.contains (e.getPosition()))
    {
        const bool wasSoloed = routing().isAuxSoloed (bus);

        // Plain click is exclusive solo; a modifier adds to the solo group, which
        // is how every mixer behaves and what people's hands already expect.
        if (! e.mods.isCommandDown() && ! e.mods.isShiftDown())
            for (int other = 0; other < kNumAuxStrips; ++other)
                routing().setAuxSoloed (other, false);

        routing().setAuxSoloed (bus, ! wasSoloed);

        if (auto* parent = getParentComponent())
            parent->repaint();
    }
}

//==============================================================================
PerStringStrip::PerStringStrip (LuthierAudioProcessor& p)
    : processor (p)
{
}

PerStringStrip::~PerStringStrip() = default;

void PerStringStrip::refresh()
{
    const int numStrings = processor.getEngine().getNumStrings();

    if (numStrings != lastNumStrings)
    {
        lastNumStrings = numStrings;
        repaint();
    }
}

juce::Rectangle<int> PerStringStrip::cellBounds (int stringIndex) const
{
    auto bounds = getLocalBounds().reduced (0, 16);

    const int columns = 6;
    const int w = juce::jmax (1, bounds.getWidth() / columns);
    const int h = juce::jmax (1, bounds.getHeight() / 2);

    const int col = stringIndex % columns;
    const int row = stringIndex / columns;

    return { bounds.getX() + col * w, bounds.getY() + row * h, w, h };
}

void PerStringStrip::paint (juce::Graphics& g)
{
    auto header = getLocalBounds().removeFromTop (14);

    g.setColour (Palette::textMuted);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("PER-STRING OUTPUTS", header, juce::Justification::centredLeft, false);

    const int numStrings = processor.getEngine().getNumStrings();

    for (int s = 0; s < kNumPerStringBuses; ++s)
    {
        const auto cell = cellBounds (s).reduced (2);
        const bool present = s < numStrings;
        const bool muted = processor.getRouting().isPerStringMuted (s);

        // A bus the current guitar has no string for is drawn dimmed, because it
        // is not silent by accident - it has nothing to carry.
        const auto fill = ! present ? Palette::panelSunken
                                    : (muted ? Palette::clip.withAlpha (0.35f)
                                             : Palette::panelRaised);

        g.setColour (fill);
        g.fillRoundedRectangle (cell.toFloat(), 2.0f);

        g.setColour (Palette::edge);
        g.drawRoundedRectangle (cell.toFloat(), 2.0f, 1.0f);

        g.setColour (present ? (muted ? Palette::textMuted : Palette::textPrimary)
                             : Palette::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (juce::String (s + 1), cell, juce::Justification::centred, false);
    }
}

void PerStringStrip::resized()
{
}

void PerStringStrip::mouseDown (const juce::MouseEvent& e)
{
    const int numStrings = processor.getEngine().getNumStrings();

    for (int s = 0; s < numStrings; ++s)
    {
        if (cellBounds (s).contains (e.getPosition()))
        {
            processor.getRouting().setPerStringMuted (s, ! processor.getRouting().isPerStringMuted (s));
            repaint();
            return;
        }
    }
}

//==============================================================================
RoutingPanel::RoutingPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    auto configureLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (10.0f)));
        label.setColour (juce::Label::textColourId, Palette::textMuted);
        addAndMakeVisible (label);
    };

    configureLabel (layoutLabel, {});
    configureLabel (latencyLabel, {});
    configureLabel (sidechainLabel, "SIDECHAIN");

    for (int bus = 0; bus < kNumAuxStrips; ++bus)
        addAndMakeVisible (auxStrips.add (new AuxStrip (processor, bus)));

    perStringStrip = std::make_unique<PerStringStrip> (processor);
    addAndMakeVisible (*perStringStrip);

    // --- sidechain ------------------------------------------------------------
    sidechainToAmp = std::make_unique<LuthierToggle> ("SIDECHAIN TO AMP");
    sidechainToAmp->setTooltip ("Feeds the sidechain input into the amp in place of the "
                                "strings, for re-amping a recorded DI.");
    sidechainToAmp->getButton().setClickingTogglesState (true);
    sidechainToAmp->getButton().onClick = [this]
    {
        if (! updatingControls)
            processor.getRouting().setSidechainToAmp (sidechainToAmp->getButton().getToggleState());
    };
    addAndMakeVisible (*sidechainToAmp);

    // --- MIDI out -------------------------------------------------------------
    auto makeMidiToggle = [this] (std::unique_ptr<LuthierToggle>& toggle,
                                  const juce::String& text, const juce::String& tip)
    {
        toggle = std::make_unique<LuthierToggle> (text);
        toggle->setTooltip (tip);
        toggle->getButton().setClickingTogglesState (true);
        toggle->getButton().onClick = [this] { updateMidiOutFromControls(); };
        addAndMakeVisible (*toggle);
    };

    makeMidiToggle (midiOutEnable, "MIDI OUT",
                    "Master switch. With this off the plugin emits no MIDI at all.");
    makeMidiToggle (midiPassThrough, "PASS-THRU",
                    "Echoes incoming MIDI unchanged, at its original timestamps.");
    makeMidiToggle (midiRhythm, "RHYTHM",
                    "Sends the rhythm engine's generated events, so a strum can be bounced to MIDI.");
    makeMidiToggle (midiStringActivity, "STRINGS",
                    "Sends a note per string that is actually ringing.");
    makeMidiToggle (midiCcBroadcast, "MACRO CC",
                    "Echoes each macro as a CC on the number assigned below.");
    makeMidiToggle (midiTunePlayback, "TUNE",
                    "Sends the tune builder's playback, each event on its own sample.");
    makeMidiToggle (midiLuthierEvents, "EVENTS",
                    "Sends character and noise events (squeaks, pick, buzz, clank) as Luthier SysEx. "
                    "Other hosts drop SysEx; nothing else depends on it.");
    makeMidiToggle (midiWorkshop, "WORKSHOP",
                    "Sends Workshop part changes as Luthier SysEx, for automation lanes.");

    midiChannel.setTooltip ("MIDI channel for generated events. Pass-through keeps its own channel.");

    for (int ch = 1; ch <= 16; ++ch)
        midiChannel.addItem (juce::String (ch), ch);

    midiChannel.onChange = [this] { updateMidiOutFromControls(); };
    addAndMakeVisible (midiChannel);

    for (int m = 0; m < ParamIDs::kNumMacros; ++m)
    {
        macroCcLabels[m].setText (juce::String (ParamIDs::macroByIndex (m)).fromFirstOccurrenceOf ("_", false, false).toUpperCase(),
                                  juce::dontSendNotification);
        macroCcLabels[m].setFont (juce::Font (juce::FontOptions (9.0f)));
        macroCcLabels[m].setColour (juce::Label::textColourId, Palette::textMuted);
        addAndMakeVisible (macroCcLabels[m]);

        macroCc[m].addItem ("off", 1);

        for (int cc = 0; cc < 128; ++cc)
            macroCc[m].addItem ("CC " + juce::String (cc), cc + 2);

        macroCc[m].onChange = [this] { updateMidiOutFromControls(); };
        addAndMakeVisible (macroCc[m]);
    }

    refreshFromRouting();

    // Column 4 sizes the tab from this; without it the panel sat at 80 points.
    setSize (480, preferredHeight());

    startTimerHz (15);
}

RoutingPanel::~RoutingPanel()
{
    stopTimer();
}

void RoutingPanel::updateMidiOutFromControls()
{
    if (updatingControls)
        return;

    // From the current config: the MIDI OUT tab edits the same one.
    auto cfg = processor.getRouting().getMidiOutConfig();
    cfg.enabled = midiOutEnable->getButton().getToggleState();
    cfg.passThrough = midiPassThrough->getButton().getToggleState();
    cfg.rhythmEngine = midiRhythm->getButton().getToggleState();
    cfg.stringActivity = midiStringActivity->getButton().getToggleState();
    cfg.ccBroadcast = midiCcBroadcast->getButton().getToggleState();
    cfg.tunePlayback = midiTunePlayback->getButton().getToggleState();
    cfg.luthierEvents = midiLuthierEvents->getButton().getToggleState();
    cfg.workshopChanges = midiWorkshop->getButton().getToggleState();
    cfg.channel = juce::jlimit (1, 16, midiChannel.getSelectedId());

    for (int m = 0; m < ParamIDs::kNumMacros; ++m)
    {
        const int id = macroCc[m].getSelectedId();
        cfg.macroCc[(size_t) m] = (id <= 1) ? -1 : id - 2;
    }

    processor.getRouting().setMidiOutConfig (cfg);
    shownMidiOut = cfg;
}

void RoutingPanel::refreshFromRouting()
{
    // Guards the onClick handlers, so restoring the controls from a preset does
    // not immediately write them back.
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    const auto cfg = processor.getRouting().getMidiOutConfig();

    midiOutEnable->getButton().setToggleState (cfg.enabled, juce::dontSendNotification);
    midiPassThrough->getButton().setToggleState (cfg.passThrough, juce::dontSendNotification);
    midiRhythm->getButton().setToggleState (cfg.rhythmEngine, juce::dontSendNotification);
    midiStringActivity->getButton().setToggleState (cfg.stringActivity, juce::dontSendNotification);
    midiCcBroadcast->getButton().setToggleState (cfg.ccBroadcast, juce::dontSendNotification);
    midiTunePlayback->getButton().setToggleState (cfg.tunePlayback, juce::dontSendNotification);
    midiLuthierEvents->getButton().setToggleState (cfg.luthierEvents, juce::dontSendNotification);
    midiWorkshop->getButton().setToggleState (cfg.workshopChanges, juce::dontSendNotification);
    shownMidiOut = cfg;
    midiChannel.setSelectedId (juce::jlimit (1, 16, cfg.channel), juce::dontSendNotification);

    for (int m = 0; m < ParamIDs::kNumMacros; ++m)
    {
        const int cc = cfg.macroCc[(size_t) m];
        macroCc[m].setSelectedId (cc < 0 ? 1 : cc + 2, juce::dontSendNotification);
    }

    sidechainToAmp->getButton().setToggleState (processor.getRouting().isSidechainToAmp(),
                                                juce::dontSendNotification);
}

void RoutingPanel::timerCallback()
{
    const auto layout = processor.getRouting().getActiveLayout();

    if (layout != lastLayout)
    {
        lastLayout = layout;

        perStringStrip->setVisible (RoutingMatrix::layoutHasPerString (layout));

        /*  The panel's height depends on the layout. Ask for the new one here
            rather than waiting for the column: the parent is a Viewport's
            content holder, whose resized() lays nothing out. */
        if (getHeight() != preferredHeight())
            setSize (juce::jmax (1, getWidth()), preferredHeight());

        if (auto* parent = getParentComponent())
            parent->resized();

        resized();
        repaint();
    }

    layoutLabel.setText ("LAYOUT  " + juce::String (getBusLayoutName (layout))
                           + (processor.getRouting().isSidechainPresent() ? "  + SIDECHAIN" : ""),
                         juce::dontSendNotification);

    const auto report = processor.getRouting().getLatencyReport();
    const double sr = processor.getEngine().getSampleRate();

    latencyLabel.setText ("LATENCY  main " + formatSamples (report.mainOut, sr)
                            + "   DI " + formatSamples (report.auxDi, sr)
                            + "   strings " + formatSamples (report.perString, sr),
                          juce::dontSendNotification);

    for (auto* strip : auxStrips)
        strip->refresh();

    perStringStrip->refresh();

    const auto level = (float) processor.getRouting().getSidechainLevel();
    const float decayed = juce::jmax (level, sidechainLevel * 0.78f);

    if (std::abs (decayed - sidechainLevel) > 0.001f)
    {
        sidechainLevel = decayed;
        repaint (sidechainMeterBounds);
    }

    // The routing state can change underneath the UI when a preset loads.
    const auto cfg = processor.getRouting().getMidiOutConfig();

    if (cfg != shownMidiOut
          || processor.getRouting().isSidechainToAmp() != sidechainToAmp->getButton().getToggleState())
    {
        refreshFromRouting();
    }
}

int RoutingPanel::preferredHeight() const
{
    int height = 18                                   // layout readout
                 + 14                                 // latency readout
                 + kNumAuxStrips * AuxStrip::preferredHeight
                 + Metrics::grid
                 + Metrics::buttonHeight               // sidechain toggle
                 + 18                                  // sidechain meter row
                 + Metrics::grid
                 + Metrics::buttonHeight * 3 + Metrics::gridHalf * 2   // midi toggles
                 + 44                                  // macro CC table
                 + Metrics::grid;

    if (RoutingMatrix::layoutHasPerString (lastLayout))
        height += PerStringStrip::preferredHeight + Metrics::gridHalf;

    return height;
}

void RoutingPanel::paint (juce::Graphics& g)
{
    if (! sidechainMeterBounds.isEmpty())
    {
        drawMeter (g, sidechainMeterBounds,
                   processor.getRouting().isSidechainPresent() ? sidechainLevel : 0.0f);

        if (! processor.getRouting().isSidechainPresent())
        {
            g.setColour (Palette::textDisabled);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText ("no sidechain bus", sidechainMeterBounds,
                        juce::Justification::centred, false);
        }
    }
}

void RoutingPanel::resized()
{
    auto bounds = getLocalBounds();

    layoutLabel.setBounds (bounds.removeFromTop (18));
    latencyLabel.setBounds (bounds.removeFromTop (14));

    for (auto* strip : auxStrips)
        strip->setBounds (bounds.removeFromTop (AuxStrip::preferredHeight));

    if (perStringStrip->isVisible())
    {
        bounds.removeFromTop (Metrics::gridHalf);
        perStringStrip->setBounds (bounds.removeFromTop (PerStringStrip::preferredHeight));
    }

    bounds.removeFromTop (Metrics::grid);

    {
        auto row = bounds.removeFromTop (18);
        sidechainLabel.setBounds (row.removeFromLeft (92));
        sidechainMeterBounds = row.reduced (0, 3);
    }

    sidechainToAmp->setBounds (bounds.removeFromTop (Metrics::buttonHeight));

    bounds.removeFromTop (Metrics::grid);

    {
        auto row = bounds.removeFromTop (Metrics::buttonHeight);
        const int w = juce::jmax (1, row.getWidth() / 3);
        midiOutEnable->setBounds (row.removeFromLeft (w).reduced (1));
        midiPassThrough->setBounds (row.removeFromLeft (w).reduced (1));
        midiChannel.setBounds (row.reduced (1));
    }

    bounds.removeFromTop (Metrics::gridHalf);

    {
        auto row = bounds.removeFromTop (Metrics::buttonHeight);
        const int w = juce::jmax (1, row.getWidth() / 3);
        midiRhythm->setBounds (row.removeFromLeft (w).reduced (1));
        midiStringActivity->setBounds (row.removeFromLeft (w).reduced (1));
        midiCcBroadcast->setBounds (row.reduced (1));
    }

    bounds.removeFromTop (Metrics::gridHalf);

    {
        auto row = bounds.removeFromTop (Metrics::buttonHeight);
        const int w = juce::jmax (1, row.getWidth() / 3);
        midiTunePlayback->setBounds (row.removeFromLeft (w).reduced (1));
        midiLuthierEvents->setBounds (row.removeFromLeft (w).reduced (1));
        midiWorkshop->setBounds (row.reduced (1));
    }

    bounds.removeFromTop (Metrics::gridHalf);

    {
        auto table = bounds.removeFromTop (40);
        const int w = juce::jmax (1, table.getWidth() / 3);

        for (int m = 0; m < ParamIDs::kNumMacros; ++m)
        {
            const int col = m % 3;
            const int rowIndex = m / 3;
            const int h = table.getHeight() / 2;

            juce::Rectangle<int> cell (table.getX() + col * w,
                                       table.getY() + rowIndex * h, w, h);

            macroCcLabels[m].setBounds (cell.removeFromLeft (44).reduced (1));
            macroCc[m].setBounds (cell.reduced (1));
        }
    }
}

} // namespace luthier
