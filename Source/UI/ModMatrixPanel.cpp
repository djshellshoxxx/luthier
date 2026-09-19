#include "ModMatrixPanel.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    /** Which family a slot belongs to, which decides what the card shows. */
    enum class SourceKind { lfo, envelope, sequencer, follower, plain };

    SourceKind kindOf (int slot) noexcept
    {
        using namespace ModSourceSlots;

        if (slot >= lfoBase && slot < lfoBase + numLfos)                return SourceKind::lfo;
        if (slot >= envBase && slot < envBase + numEnvelopes)           return SourceKind::envelope;
        if (slot >= seqBase && slot < seqBase + numSequencers)          return SourceKind::sequencer;
        if (slot >= followerBase && slot < followerBase + numFollowers) return SourceKind::follower;

        return SourceKind::plain;
    }

    /** The slots offered in the selector. Every CC is a legal source, but a
        selector with a hundred and sixty entries is not a usable control, so the
        list stops at the ones a user actually picks from a menu. */
    juce::Array<int> selectableSlots()
    {
        using namespace ModSourceSlots;

        juce::Array<int> slots;

        for (int i = 0; i < numLfos; ++i)       slots.add (lfoBase + i);
        for (int i = 0; i < numEnvelopes; ++i)  slots.add (envBase + i);
        for (int i = 0; i < numSequencers; ++i) slots.add (seqBase + i);
        for (int i = 0; i < numFollowers; ++i)  slots.add (followerBase + i);
        for (int i = 0; i < numMacros; ++i)     slots.add (macroBase + i);

        for (int slot : { notePitch, noteVelocity, noteTrigger, notesHeld,
                          aftertouch, polyAftertouch, pitchBend, modWheel,
                          channelPressure, randomPerNote, randomPerBar, randomSmooth })
            slots.add (slot);

        return slots;
    }
}

//==============================================================================
ModSourceCard::ModSourceCard (LuthierAudioProcessor& p)
    : processor (p)
{
    auto setupSlider = [this] (juce::Slider& s, double min, double max, double interval,
                               const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 16);
        s.setRange (min, max, interval);
        s.setTextValueSuffix (suffix);
        s.onValueChange = [this] { pushToSource(); };
        addChildComponent (s);
    };

    setupSlider (rateSlider, 0.01, 40.0, 0.01, " Hz");
    setupSlider (depthSlider, 0.0, 1.0, 0.001, {});
    setupSlider (symmetrySlider, 0.01, 0.99, 0.01, {});
    setupSlider (smoothingSlider, 0.0, 500.0, 1.0, " ms");

    setupSlider (delaySlider, 0.0, 30.0, 0.001, " s");
    setupSlider (attackSlider, 0.0, 30.0, 0.001, " s");
    setupSlider (holdSlider, 0.0, 30.0, 0.001, " s");
    setupSlider (decaySlider, 0.0, 30.0, 0.001, " s");
    setupSlider (sustainSlider, 0.0, 1.0, 0.001, {});
    setupSlider (releaseSlider, 0.0, 30.0, 0.001, " s");

    setupSlider (lengthSlider, 4.0, 64.0, 1.0, " steps");
    setupSlider (swingSlider, 0.0, 0.75, 0.01, {});

    setupSlider (followerAttackSlider, 0.1, 500.0, 0.1, " ms");
    setupSlider (followerReleaseSlider, 1.0, 5000.0, 1.0, " ms");
    setupSlider (thresholdSlider, 0.0, 1.0, 0.001, {});

    // Rate is logarithmic: an LFO spends most of its useful life under 10 Hz.
    rateSlider.setSkewFactorFromMidPoint (2.0);
    attackSlider.setSkewFactorFromMidPoint (0.2);
    decaySlider.setSkewFactorFromMidPoint (0.3);
    releaseSlider.setSkewFactorFromMidPoint (0.3);
    followerAttackSlider.setSkewFactorFromMidPoint (20.0);
    followerReleaseSlider.setSkewFactorFromMidPoint (200.0);

    for (int i = 0; i < (int) ModLfo::Shape::numShapes; ++i)
        shapeBox.addItem (ModLfo::getShapeName ((ModLfo::Shape) i), i + 1);

    for (int i = 0; i < (int) ModSyncDivision::numDivisions; ++i)
        divisionBox.addItem (getModSyncDivisionName ((ModSyncDivision) i), i + 1);

    retriggerBox.addItem ("Free run", 1);
    retriggerBox.addItem ("On note", 2);
    retriggerBox.addItem ("On transport", 3);
    retriggerBox.addItem ("Lock to grid", 4);

    directionBox.addItem ("Forward", 1);
    directionBox.addItem ("Reverse", 2);
    directionBox.addItem ("Ping-pong", 3);
    directionBox.addItem ("Random", 4);
    directionBox.addItem ("Brownian", 5);

    detectionBox.addItem ("Peak", 1);
    detectionBox.addItem ("RMS", 2);
    detectionBox.addItem ("True peak", 3);

    followerSourceBox.addItem ("Main output", 1);
    followerSourceBox.addItem ("Sidechain", 2);
    followerSourceBox.addItem ("Per-string", 3);
    followerSourceBox.addItem ("Pickup", 4);

    for (auto* box : { &shapeBox, &divisionBox, &retriggerBox, &directionBox,
                       &detectionBox, &followerSourceBox })
    {
        box->onChange = [this] { pushToSource(); };
        addChildComponent (*box);
    }

    for (auto* button : { &syncButton, &bipolarButton })
    {
        button->onClick = [this] { pushToSource(); };
        addChildComponent (*button);
    }

    history.fill (0.0f);

    setSlot (ModSourceSlots::lfoBase);
    startTimerHz (20);
}

ModSourceCard::~ModSourceCard()
{
    stopTimer();
}

void ModSourceCard::setSlot (int newSlot)
{
    slot = juce::jlimit (0, ModSourceSlots::count - 1, newSlot);
    history.fill (0.0f);
    historyWrite = 0;
    rebuildControls();
    resized();
    repaint();
}

void ModSourceCard::rebuildControls()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    const auto kind = kindOf (slot);

    const bool isLfo = (kind == SourceKind::lfo);
    const bool isEnv = (kind == SourceKind::envelope);
    const bool isSeq = (kind == SourceKind::sequencer);
    const bool isFollower = (kind == SourceKind::follower);

    shapeBox.setVisible (isLfo);
    rateSlider.setVisible (isLfo);
    symmetrySlider.setVisible (isLfo);
    smoothingSlider.setVisible (isLfo);
    bipolarButton.setVisible (isLfo);
    depthSlider.setVisible (isLfo);
    retriggerBox.setVisible (isLfo);
    syncButton.setVisible (isLfo || isSeq);
    divisionBox.setVisible (isLfo || isSeq);

    delaySlider.setVisible (isEnv);
    attackSlider.setVisible (isEnv);
    holdSlider.setVisible (isEnv);
    decaySlider.setVisible (isEnv);
    sustainSlider.setVisible (isEnv);
    releaseSlider.setVisible (isEnv);

    lengthSlider.setVisible (isSeq);
    swingSlider.setVisible (isSeq);
    directionBox.setVisible (isSeq);

    followerSourceBox.setVisible (isFollower);
    detectionBox.setVisible (isFollower);
    followerAttackSlider.setVisible (isFollower);
    followerReleaseSlider.setVisible (isFollower);
    thresholdSlider.setVisible (isFollower);

    auto& matrix = processor.getModMatrix();

    if (isLfo)
    {
        auto& lfo = matrix.getLfo (slot - ModSourceSlots::lfoBase);
        shapeBox.setSelectedId ((int) lfo.getShape() + 1, juce::dontSendNotification);
        rateSlider.setValue (lfo.getRateHz(), juce::dontSendNotification);
        depthSlider.setValue (lfo.getDepth(), juce::dontSendNotification);
        symmetrySlider.setValue (lfo.getSymmetry(), juce::dontSendNotification);
        smoothingSlider.setValue (lfo.getSmoothingMs(), juce::dontSendNotification);
        syncButton.setToggleState (lfo.isSynced(), juce::dontSendNotification);
        bipolarButton.setToggleState (lfo.isBipolar(), juce::dontSendNotification);
        divisionBox.setSelectedId ((int) lfo.getSyncDivision() + 1, juce::dontSendNotification);
        retriggerBox.setSelectedId ((int) lfo.getRetrigger() + 1, juce::dontSendNotification);
    }
    else if (isEnv)
    {
        auto& env = matrix.getEnvelope (slot - ModSourceSlots::envBase);
        delaySlider.setValue (env.getDelaySeconds(), juce::dontSendNotification);
        attackSlider.setValue (env.getAttackSeconds(), juce::dontSendNotification);
        holdSlider.setValue (env.getHoldSeconds(), juce::dontSendNotification);
        decaySlider.setValue (env.getDecaySeconds(), juce::dontSendNotification);
        sustainSlider.setValue (env.getSustainLevel(), juce::dontSendNotification);
        releaseSlider.setValue (env.getReleaseSeconds(), juce::dontSendNotification);
    }
    else if (isSeq)
    {
        auto& seq = matrix.getSequencer (slot - ModSourceSlots::seqBase);
        lengthSlider.setValue (seq.getLength(), juce::dontSendNotification);
        swingSlider.setValue (seq.getSwing(), juce::dontSendNotification);
        directionBox.setSelectedId ((int) seq.getDirection() + 1, juce::dontSendNotification);
        divisionBox.setSelectedId ((int) seq.getDivision() + 1, juce::dontSendNotification);
        syncButton.setToggleState (seq.isSynced(), juce::dontSendNotification);
    }
    else if (isFollower)
    {
        auto& follower = matrix.getFollower (slot - ModSourceSlots::followerBase);
        followerSourceBox.setSelectedId ((int) follower.getSource() + 1, juce::dontSendNotification);
        detectionBox.setSelectedId ((int) follower.getDetection() + 1, juce::dontSendNotification);
        followerAttackSlider.setValue (follower.getAttackMs(), juce::dontSendNotification);
        followerReleaseSlider.setValue (follower.getReleaseMs(), juce::dontSendNotification);
        thresholdSlider.setValue (follower.getThreshold(), juce::dontSendNotification);
    }
}

void ModSourceCard::pushToSource()
{
    if (updating)
        return;

    auto& matrix = processor.getModMatrix();

    switch (kindOf (slot))
    {
        case SourceKind::lfo:
        {
            auto& lfo = matrix.getLfo (slot - ModSourceSlots::lfoBase);
            lfo.setShape ((ModLfo::Shape) juce::jmax (0, shapeBox.getSelectedId() - 1));
            lfo.setRateHz (rateSlider.getValue());
            lfo.setDepth (depthSlider.getValue());
            lfo.setSymmetry (symmetrySlider.getValue());
            lfo.setSmoothingMs (smoothingSlider.getValue());
            lfo.setSynced (syncButton.getToggleState());
            lfo.setBipolar (bipolarButton.getToggleState());
            lfo.setSyncDivision ((ModSyncDivision) juce::jmax (0, divisionBox.getSelectedId() - 1));
            lfo.setRetrigger ((ModLfo::Retrigger) juce::jmax (0, retriggerBox.getSelectedId() - 1));
            break;
        }

        case SourceKind::envelope:
        {
            auto& env = matrix.getEnvelope (slot - ModSourceSlots::envBase);
            env.setDelaySeconds (delaySlider.getValue());
            env.setAttackSeconds (attackSlider.getValue());
            env.setHoldSeconds (holdSlider.getValue());
            env.setDecaySeconds (decaySlider.getValue());
            env.setSustainLevel (sustainSlider.getValue());
            env.setReleaseSeconds (releaseSlider.getValue());
            break;
        }

        case SourceKind::sequencer:
        {
            auto& seq = matrix.getSequencer (slot - ModSourceSlots::seqBase);
            seq.setLength ((int) lengthSlider.getValue());
            seq.setSwing (swingSlider.getValue());
            seq.setDirection ((ModStepSequencer::Direction) juce::jmax (0, directionBox.getSelectedId() - 1));
            seq.setDivision ((ModSyncDivision) juce::jmax (0, divisionBox.getSelectedId() - 1));
            seq.setSynced (syncButton.getToggleState());
            break;
        }

        case SourceKind::follower:
        {
            auto& follower = matrix.getFollower (slot - ModSourceSlots::followerBase);
            follower.setSource ((ModEnvelopeFollower::Source) juce::jmax (0, followerSourceBox.getSelectedId() - 1));
            follower.setDetection ((ModEnvelopeFollower::Detection) juce::jmax (0, detectionBox.getSelectedId() - 1));
            follower.setAttackMs (followerAttackSlider.getValue());
            follower.setReleaseMs (followerReleaseSlider.getValue());
            follower.setThreshold (thresholdSlider.getValue());
            break;
        }

        case SourceKind::plain:
        default:
            break;
    }
}

void ModSourceCard::timerCallback()
{
    const auto value = processor.getModMatrix().getSourceValue (slot);

    history[(size_t) historyWrite] = value;
    historyWrite = (historyWrite + 1) % (int) history.size();

    liveValue = value;
    repaint (scopeBounds);
}

void ModSourceCard::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    auto header = bounds.removeFromTop (16);

    g.setColour (Palette::accent);
    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    g.drawText (modSourceDisplayName (slot), header, juce::Justification::centredLeft, false);

    g.setColour (Palette::textMuted);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (juce::String (liveValue, 3), header, juce::Justification::centredRight, false);

    // ---- the little scope ------------------------------------------------------
    // Seeing what a source is actually doing is worth more than any number of
    // controls describing what it should be doing.
    if (! scopeBounds.isEmpty())
    {
        g.setColour (Palette::panelSunken);
        g.fillRect (scopeBounds);

        g.setColour (Palette::edge);
        g.drawRect (scopeBounds, 1);

        // Zero line.
        g.setColour (Palette::edge.withAlpha (0.6f));
        g.drawHorizontalLine (scopeBounds.getCentreY(),
                              (float) scopeBounds.getX(), (float) scopeBounds.getRight());

        juce::Path trace;
        const int n = (int) history.size();

        for (int i = 0; i < n; ++i)
        {
            const float v = history[(size_t) ((historyWrite + i) % n)];
            const float x = (float) scopeBounds.getX()
                              + (float) scopeBounds.getWidth() * ((float) i / (float) (n - 1));

            // Sources are bipolar or unipolar; both fit if the centre is zero
            // and the full height is two units.
            const float y = scopeBounds.getCentreY()
                              - v * (float) scopeBounds.getHeight() * 0.45f;

            if (i == 0)
                trace.startNewSubPath (x, y);
            else
                trace.lineTo (x, y);
        }

        g.setColour (Palette::secondary);
        g.strokePath (trace, juce::PathStrokeType (1.2f));
    }
}

void ModSourceCard::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (16);

    scopeBounds = bounds.removeFromTop (34).reduced (0, 2);
    bounds.removeFromTop (2);

    const int rowHeight = 18;

    auto nextRow = [&bounds, rowHeight] { return bounds.removeFromTop (rowHeight).reduced (0, 1); };

    auto placePair = [&nextRow] (juce::Component& a, juce::Component& b)
    {
        auto row = nextRow();
        const int half = row.getWidth() / 2;
        a.setBounds (row.removeFromLeft (half).reduced (1, 0));
        b.setBounds (row.reduced (1, 0));
    };

    switch (kindOf (slot))
    {
        case SourceKind::lfo:
            placePair (shapeBox, retriggerBox);
            placePair (syncButton, bipolarButton);
            rateSlider.setBounds (nextRow());
            divisionBox.setBounds (nextRow());
            depthSlider.setBounds (nextRow());
            symmetrySlider.setBounds (nextRow());
            smoothingSlider.setBounds (nextRow());
            break;

        case SourceKind::envelope:
            delaySlider.setBounds (nextRow());
            attackSlider.setBounds (nextRow());
            holdSlider.setBounds (nextRow());
            decaySlider.setBounds (nextRow());
            sustainSlider.setBounds (nextRow());
            releaseSlider.setBounds (nextRow());
            break;

        case SourceKind::sequencer:
            placePair (directionBox, syncButton);
            divisionBox.setBounds (nextRow());
            lengthSlider.setBounds (nextRow());
            swingSlider.setBounds (nextRow());
            break;

        case SourceKind::follower:
            placePair (followerSourceBox, detectionBox);
            followerAttackSlider.setBounds (nextRow());
            followerReleaseSlider.setBounds (nextRow());
            thresholdSlider.setBounds (nextRow());
            break;

        case SourceKind::plain:
        default:
            break;
    }
}

//==============================================================================
ModRouteTable::ModRouteTable (LuthierAudioProcessor& p)
    : processor (p)
{
    table.setModel (this);
    table.setHeaderHeight (18);
    table.setRowHeight (20);
    table.getViewport()->setScrollBarsShown (true, false);
    table.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);

    auto& header = table.getHeader();
    header.addColumn ("Source", ColumnId::source, 78);
    header.addColumn ("Destination", ColumnId::destination, 128);
    header.addColumn ("Depth", ColumnId::depth, 66);
    header.addColumn ("Curve", ColumnId::curve, 50);
    header.addColumn ("On", ColumnId::enabled, 26);
    header.addColumn ("", ColumnId::remove, 22);

    addAndMakeVisible (table);
    refresh();
}

ModRouteTable::~ModRouteTable()
{
    table.setModel (nullptr);
}

void ModRouteTable::refresh()
{
    cached = processor.getModMatrix().getRoutes();
    table.updateContent();
    repaint();
}

int ModRouteTable::getNumRows()
{
    return (int) cached.size();
}

void ModRouteTable::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    juce::ignoreUnused (width, height);

    if (selected)
        g.fillAll (Palette::accentDim.withAlpha (0.35f));
    else if (row % 2)
        g.fillAll (Palette::panel.withAlpha (0.45f));
}

void ModRouteTable::paintCell (juce::Graphics& g, int row, int columnId,
                               int width, int height, bool)
{
    if (! juce::isPositiveAndBelow (row, (int) cached.size()))
        return;

    const auto& route = cached[(size_t) row];
    const juce::Rectangle<int> area (0, 0, width, height);

    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (route.enabled ? Palette::textPrimary : Palette::textDisabled);

    switch (columnId)
    {
        case ColumnId::source:
            g.drawText (modSourceDisplayName (modSourceSlotForId (route.sourceId)),
                        area.reduced (3, 0), juce::Justification::centredLeft, true);
            break;

        case ColumnId::destination:
        {
            // The parameter's display name, not its id: "Amp Gain", not
            // "amp_gain".
            juce::String name = route.destinationId;

            if (auto* param = processor.getState().getParameter (route.destinationId))
                name = param->getName (30);

            g.drawText (name, area.reduced (3, 0), juce::Justification::centredLeft, true);
            break;
        }

        case ColumnId::depth:
        {
            // Drawn as a centred bar so the sign reads at a glance.
            auto bar = area.reduced (3, 5);

            g.setColour (Palette::panelSunken);
            g.fillRect (bar);

            const float centre = (float) bar.getCentreX();
            const float extent = (float) bar.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, route.depth);

            g.setColour (route.enabled ? Palette::secondary : Palette::textDisabled);
            g.fillRect (juce::Rectangle<float> (juce::jmin (centre, centre + extent),
                                                (float) bar.getY(),
                                                std::abs (extent),
                                                (float) bar.getHeight()));

            g.setColour (Palette::textPrimary);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (juce::String (juce::roundToInt (route.depth * 100.0f)) + "%",
                        area, juce::Justification::centred, false);
            break;
        }

        case ColumnId::curve:
            g.drawText (getModCurveName (route.curve), area, juce::Justification::centred, false);
            break;

        case ColumnId::enabled:
        {
            auto box = area.reduced (6, 5);
            g.setColour (route.enabled ? Palette::success : Palette::panelSunken);
            g.fillRoundedRectangle (box.toFloat(), 2.0f);
            g.setColour (Palette::edge);
            g.drawRoundedRectangle (box.toFloat(), 2.0f, 1.0f);
            break;
        }

        case ColumnId::remove:
            g.setColour (Palette::clip.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText ("x", area, juce::Justification::centred, false);
            break;

        default:
            break;
    }
}

void ModRouteTable::cellClicked (int row, int columnId, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, (int) cached.size()))
        return;

    auto& matrix = processor.getModMatrix();

    switch (columnId)
    {
        case ColumnId::enabled:
            matrix.setRouteEnabled (row, ! cached[(size_t) row].enabled);
            break;

        case ColumnId::remove:
            matrix.removeRoute (row);
            break;

        case ColumnId::depth:
        {
            // Click-drag would fight the table's own mouse handling, so depth is
            // typed rather than dragged.
            auto* editor = new juce::TextEditor();
            editor->setSize (80, 22);
            editor->setText (juce::String (cached[(size_t) row].depth, 3), false);
            editor->selectAll();

            auto& box = juce::CallOutBox::launchAsynchronously (
                std::unique_ptr<juce::Component> (editor),
                getScreenBounds().withPosition (e.getScreenPosition()), nullptr);

            editor->onReturnKey = [this, editor, row, &box]
            {
                processor.getModMatrix().setRouteDepth (row, editor->getText().getFloatValue());
                refresh();

                if (onRoutesChanged)
                    onRoutesChanged();

                box.dismiss();
            };

            editor->onEscapeKey = [&box] { box.dismiss(); };
            editor->grabKeyboardFocus();
            return;
        }

        case ColumnId::curve:
        {
            const auto next = (ModCurve) (((int) cached[(size_t) row].curve + 1)
                                            % (int) ModCurve::numCurves);
            matrix.setRouteCurve (row, next);
            break;
        }

        default:
            return;
    }

    refresh();

    if (onRoutesChanged)
        onRoutesChanged();
}

void ModRouteTable::resized()
{
    table.setBounds (getLocalBounds());
}

//==============================================================================
ModMatrixPanel::ModMatrixPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    const auto slots = selectableSlots();

    for (int i = 0; i < slots.size(); ++i)
        sourceSelector.addItem (modSourceDisplayName (slots[i]), i + 1);

    sourceSelector.setSelectedId (1, juce::dontSendNotification);
    sourceSelector.onChange = [this]
    {
        const auto all = selectableSlots();
        const int index = sourceSelector.getSelectedId() - 1;

        if (juce::isPositiveAndBelow (index, all.size()))
            card->setSlot (all[index]);
    };

    addAndMakeVisible (sourceSelector);

    card = std::make_unique<ModSourceCard> (processor);
    addAndMakeVisible (*card);

    routeTable = std::make_unique<ModRouteTable> (processor);
    routeTable->onRoutesChanged = [this]
    {
        summaryLabel.setText (juce::String (processor.getModMatrix().getNumRoutes())
                                + " route(s)", juce::dontSendNotification);
    };
    addAndMakeVisible (*routeTable);

    addButton.onClick = [this] { showAddRouteMenu(); };
    addAndMakeVisible (addButton);

    clearButton.onClick = [this]
    {
        processor.getModMatrix().clearRoutes();
        routeTable->refresh();

        if (routeTable->onRoutesChanged)
            routeTable->onRoutesChanged();
    };
    addAndMakeVisible (clearButton);

    summaryLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    summaryLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    summaryLabel.setText (juce::String (processor.getModMatrix().getNumRoutes()) + " route(s)",
                          juce::dontSendNotification);
    addAndMakeVisible (summaryLabel);
}

ModMatrixPanel::~ModMatrixPanel() = default;

void ModMatrixPanel::showAddRouteMenu()
{
    // The destination list is every automatable parameter, grouped by the prefix
    // of its id, which is already how the parameter names are organised.
    juce::PopupMenu menu;
    menu.addSectionHeader ("Modulate with " + modSourceDisplayName (card->getSlot()));

    juce::HashMap<juce::String, int> groupIds;
    juce::StringArray groupOrder;
    juce::Array<juce::String> destinationIds;

    for (auto* param : processor.getParameters())
    {
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
        {
            const auto id = withId->paramID;
            const auto group = id.upToFirstOccurrenceOf ("_", false, false);

            destinationIds.add (id);

            if (! groupIds.contains (group))
            {
                groupIds.set (group, 1);
                groupOrder.add (group);
            }
        }
    }

    int itemId = 1;
    juce::Array<juce::String> byItemId;
    byItemId.add ({});   // ids start at one

    for (const auto& group : groupOrder)
    {
        juce::PopupMenu sub;

        for (const auto& id : destinationIds)
        {
            if (! id.startsWith (group))
                continue;

            if (auto* param = processor.getState().getParameter (id))
            {
                sub.addItem (++itemId, param->getName (32));
                byItemId.add (id);
            }
        }

        menu.addSubMenu (group.substring (0, 1).toUpperCase() + group.substring (1), sub);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton),
                        [this, byItemId] (int result)
    {
        if (result <= 1 || result >= byItemId.size() + 1)
            return;

        ModRoute route;
        route.sourceId = modSourceIdForSlot (card->getSlot());
        route.destinationId = byItemId[result - 1];
        route.depth = 0.33f;
        route.enabled = true;

        processor.getModMatrix().addRoute (route);
        routeTable->refresh();

        if (routeTable->onRoutesChanged)
            routeTable->onRoutesChanged();
    });
}

int ModMatrixPanel::preferredHeight() const
{
    return 22                                   // source selector
             + ModSourceCard::preferredHeight
             + Metrics::gridHalf
             + 160                              // route table
             + Metrics::buttonHeight
             + 16;                              // summary
}

void ModMatrixPanel::paint (juce::Graphics&)
{
}

void ModMatrixPanel::resized()
{
    auto bounds = getLocalBounds();

    sourceSelector.setBounds (bounds.removeFromTop (22).reduced (0, 1));
    card->setBounds (bounds.removeFromTop (ModSourceCard::preferredHeight));

    bounds.removeFromTop (Metrics::gridHalf);

    routeTable->setBounds (bounds.removeFromTop (160));

    {
        auto row = bounds.removeFromTop (Metrics::buttonHeight);
        const int half = row.getWidth() / 2;
        addButton.setBounds (row.removeFromLeft (half).reduced (1));
        clearButton.setBounds (row.reduced (1));
    }

    summaryLabel.setBounds (bounds.removeFromTop (16));
}

} // namespace luthier
