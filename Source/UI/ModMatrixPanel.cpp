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

    // SPEC-SWEEP: MM-14 / MM-23.
    setupSlider (phaseSlider, 0.0, 360.0, 1.0, " deg");
    setupSlider (seqRateSlider, 0.1, 40.0, 0.01, " Hz");
    seqRateSlider.setSkewFactorFromMidPoint (4.0);
    phaseSlider.setTooltip ("LFO start phase: where a retrigger restarts it");
    seqRateSlider.setTooltip ("The sequencer's own step rate, used when Sync is off");

    // Every slider row says what it is (TODO V screenshots: seven unlabelled bars).
    for (auto [s, name] : { std::pair<juce::Slider*, const char*> { &rateSlider, "Rate" }, { &depthSlider, "Depth" },
                            { &symmetrySlider, "Symmetry" }, { &smoothingSlider, "Smoothing" }, { &delaySlider, "Delay" },
                            { &attackSlider, "Attack" }, { &holdSlider, "Hold" }, { &decaySlider, "Decay" },
                            { &sustainSlider, "Sustain" }, { &releaseSlider, "Release" }, { &lengthSlider, "Length" },
                            { &swingSlider, "Swing" }, { &followerAttackSlider, "Attack" }, { &followerReleaseSlider, "Release" },
                            { &thresholdSlider, "Threshold" },
                            { &phaseSlider, "Phase" }, { &seqRateSlider, "Rate" } })   // + SPEC-SWEEP's two
    {
        s->setName (name);
        s->setTitle (name);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    }

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

    // SPEC-SWEEP: MM-18 / MM-19 / MM-20 / MM-25.
    envRetriggerBox.addItem ("Legato", 1);
    envRetriggerBox.addItem ("Always", 2);
    envRetriggerBox.addItem ("One-shot", 3);
    envRetriggerBox.setTooltip ("Envelope retrigger: legato ignores overlapping notes, "
                                "one-shot runs to the end whatever the key does");

    loopModeBox.addItem ("No loop", 1);
    loopModeBox.addItem ("Loop D-S", 2);
    loopModeBox.addItem ("Loop D-R", 3);
    loopModeBox.setTooltip ("Envelope looping: decay back to attack, or through release");

    for (auto* box : { &attackCurveBox, &decayCurveBox, &releaseCurveBox })
        for (int i = 0; i < (int) ModCurve::numCurves; ++i)
            box->addItem (getModCurveName ((ModCurve) i), i + 1);

    attackCurveBox.setTooltip ("Attack curve");
    decayCurveBox.setTooltip ("Decay curve");
    releaseCurveBox.setTooltip ("Release curve");

    for (int s = 0; s < kMaxStrings; ++s)
        followerStringBox.addItem ("String " + juce::String (s + 1), s + 1);

    followerStringBox.setTooltip ("Which string a per-string follower listens to");
    followerLogButton.setTooltip ("Logarithmic (dB-like) output rather than linear");

    for (auto* box : { &shapeBox, &divisionBox, &retriggerBox, &directionBox,
                       &detectionBox, &followerSourceBox,
                       &envRetriggerBox, &loopModeBox, &attackCurveBox, &decayCurveBox,
                       &releaseCurveBox, &followerStringBox })
    {
        box->onChange = [this] { pushToSource(); };
        addChildComponent (*box);
    }

    for (auto* button : { &syncButton, &bipolarButton, &followerLogButton })
    {
        button->onClick = [this] { pushToSource(); };
        addChildComponent (*button);
    }

    history.fill (0.0f);

    setSlot (ModSourceSlots::lfoBase);
    motion.startTimerHz (*this, 20);
}

ModSourceCard::~ModSourceCard()
{
    motion.stopTimer();
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

    // SPEC-SWEEP: MM-14 / 18 / 19 / 20 / 23 / 25.
    phaseSlider.setVisible (isLfo);
    for (auto* box : { &envRetriggerBox, &loopModeBox, &attackCurveBox, &decayCurveBox, &releaseCurveBox })
        box->setVisible (isEnv);
    seqRateSlider.setVisible (isSeq);
    followerStringBox.setVisible (isFollower);
    followerLogButton.setVisible (isFollower);

    applyRangeLimits();

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
        phaseSlider.setValue (lfo.getPhaseOffsetDegrees(), juce::dontSendNotification);
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
        envRetriggerBox.setSelectedId ((int) env.getRetrigger() + 1, juce::dontSendNotification);
        loopModeBox.setSelectedId ((int) env.getLoopMode() + 1, juce::dontSendNotification);
        attackCurveBox.setSelectedId ((int) env.getStageCurve (ModEnvelope::Stage::attack) + 1, juce::dontSendNotification);
        decayCurveBox.setSelectedId ((int) env.getStageCurve (ModEnvelope::Stage::decay) + 1, juce::dontSendNotification);
        releaseCurveBox.setSelectedId ((int) env.getStageCurve (ModEnvelope::Stage::release) + 1, juce::dontSendNotification);
    }
    else if (isSeq)
    {
        auto& seq = matrix.getSequencer (slot - ModSourceSlots::seqBase);
        lengthSlider.setValue (seq.getLength(), juce::dontSendNotification);
        swingSlider.setValue (seq.getSwing(), juce::dontSendNotification);
        directionBox.setSelectedId ((int) seq.getDirection() + 1, juce::dontSendNotification);
        divisionBox.setSelectedId ((int) seq.getDivision() + 1, juce::dontSendNotification);
        syncButton.setToggleState (seq.isSynced(), juce::dontSendNotification);
        seqRateSlider.setValue (seq.getInternalRateHz(), juce::dontSendNotification);
        seqRateSlider.setEnabled (! seq.isSynced());
    }
    else if (isFollower)
    {
        auto& follower = matrix.getFollower (slot - ModSourceSlots::followerBase);
        followerSourceBox.setSelectedId ((int) follower.getSource() + 1, juce::dontSendNotification);
        detectionBox.setSelectedId ((int) follower.getDetection() + 1, juce::dontSendNotification);
        followerAttackSlider.setValue (follower.getAttackMs(), juce::dontSendNotification);
        followerReleaseSlider.setValue (follower.getReleaseMs(), juce::dontSendNotification);
        thresholdSlider.setValue (follower.getThreshold(), juce::dontSendNotification);
        followerStringBox.setSelectedId (follower.getStringIndex() + 1, juce::dontSendNotification);
        followerStringBox.setEnabled (follower.getSource() == ModEnvelopeFollower::Source::perString);
        followerLogButton.setToggleState (follower.isLogarithmic(), juce::dontSendNotification);
    }
}

/*  SPEC-SWEEP: PR-44 / AR-15 - the sliders offer the modulation family's live
    pair (advanced-ranges.md 3.4), so a locked card cannot ask for a value the
    source would refuse. */
void ModSourceCard::applyRangeLimits()
{
    const bool advanced = processor.getModMatrix().isModulationRangeAdvanced();
    shownRangeAdvanced = advanced;

    const auto lfo = ModRanges::lfoRateHz (advanced);
    const auto env = ModRanges::envelopeSeconds (advanced);
    const auto seq = ModRanges::sequencerRateHz (advanced);
    const auto fol = ModRanges::followerMs (advanced);

    rateSlider.setRange (lfo.lo, lfo.hi, 0.0);
    rateSlider.setSkewFactorFromMidPoint (2.0);

    for (auto* s : { &attackSlider, &decaySlider, &releaseSlider })
        s->setRange (env.lo, env.hi, 0.0);

    attackSlider.setSkewFactorFromMidPoint (0.2);
    decaySlider.setSkewFactorFromMidPoint (0.3);
    releaseSlider.setSkewFactorFromMidPoint (0.3);

    seqRateSlider.setRange (seq.lo, seq.hi, 0.0);
    seqRateSlider.setSkewFactorFromMidPoint (4.0);

    followerAttackSlider.setRange (fol.lo, fol.hi, 0.0);
    followerReleaseSlider.setRange (fol.lo, fol.hi, 0.0);
    followerAttackSlider.setSkewFactorFromMidPoint (20.0);
    followerReleaseSlider.setSkewFactorFromMidPoint (200.0);
}

void ModSourceCard::pushToSource()
{
    if (updating)
        return;

    // SPEC-SWEEP (UW-5): the sources tick on the audio thread, so the card
    // posts its settings as one plain edit instead of calling their setters.
    ModSourceEdit e;

    switch (kindOf (slot))
    {
        case SourceKind::lfo:
            e.kind = ModSourceEdit::Kind::lfo;
            e.index = slot - ModSourceSlots::lfoBase;
            e.lfoShape = juce::jmax (0, shapeBox.getSelectedId() - 1);
            e.lfoRateHz = rateSlider.getValue();
            e.lfoDepth = depthSlider.getValue();
            e.lfoSymmetry = symmetrySlider.getValue();
            e.lfoSmoothingMs = smoothingSlider.getValue();
            e.lfoSynced = syncButton.getToggleState();
            e.lfoBipolar = bipolarButton.getToggleState();
            e.lfoDivision = juce::jmax (0, divisionBox.getSelectedId() - 1);
            e.lfoRetrigger = juce::jmax (0, retriggerBox.getSelectedId() - 1);
            e.lfoPhaseDegrees = phaseSlider.getValue();   // SPEC-SWEEP: MM-14
            break;

        case SourceKind::envelope:
            e.kind = ModSourceEdit::Kind::envelope;
            e.index = slot - ModSourceSlots::envBase;
            e.envDelay = delaySlider.getValue();
            e.envAttack = attackSlider.getValue();
            e.envHold = holdSlider.getValue();
            e.envDecay = decaySlider.getValue();
            e.envSustain = sustainSlider.getValue();
            e.envRelease = releaseSlider.getValue();
            // SPEC-SWEEP: MM-18 / MM-19 / MM-20.
            e.envRetrigger = juce::jlimit (0, 2, envRetriggerBox.getSelectedId() - 1);
            e.envLoopMode = juce::jlimit (0, 2, loopModeBox.getSelectedId() - 1);
            e.envAttackCurve = juce::jmax (0, attackCurveBox.getSelectedId() - 1);
            e.envDecayCurve = juce::jmax (0, decayCurveBox.getSelectedId() - 1);
            e.envReleaseCurve = juce::jmax (0, releaseCurveBox.getSelectedId() - 1);
            break;

        case SourceKind::sequencer:
            e.kind = ModSourceEdit::Kind::sequencer;
            e.index = slot - ModSourceSlots::seqBase;
            e.seqLength = (int) lengthSlider.getValue();
            e.seqSwing = swingSlider.getValue();
            e.seqDirection = juce::jmax (0, directionBox.getSelectedId() - 1);
            e.seqDivision = juce::jmax (0, divisionBox.getSelectedId() - 1);
            e.seqSynced = syncButton.getToggleState();
            e.seqInternalRateHz = seqRateSlider.getValue();   // SPEC-SWEEP: MM-23
            seqRateSlider.setEnabled (! e.seqSynced);
            break;

        case SourceKind::follower:
            e.kind = ModSourceEdit::Kind::follower;
            e.index = slot - ModSourceSlots::followerBase;
            e.followerSource = juce::jmax (0, followerSourceBox.getSelectedId() - 1);
            e.followerDetection = juce::jmax (0, detectionBox.getSelectedId() - 1);
            e.followerAttackMs = followerAttackSlider.getValue();
            e.followerReleaseMs = followerReleaseSlider.getValue();
            e.followerThreshold = thresholdSlider.getValue();
            // SPEC-SWEEP: MM-25.
            e.followerString = juce::jmax (0, followerStringBox.getSelectedId() - 1);
            e.followerLogarithmic = followerLogButton.getToggleState();
            followerStringBox.setEnabled (e.followerSource == (int) ModEnvelopeFollower::Source::perString);
            break;

        case SourceKind::plain:
        default:
            break;
    }

    if (e.kind != ModSourceEdit::Kind::none)
        processor.getModMatrix().postSourceEdit (e);
}

void ModSourceCard::timerCallback()
{
    // SPEC-SWEEP: PR-44 - the range family was locked or unlocked elsewhere.
    if (processor.getModMatrix().isModulationRangeAdvanced() != shownRangeAdvanced)
        rebuildControls();

    const auto value = processor.getModMatrix().getSourceValue (slot);

    history[(size_t) historyWrite] = value;
    historyWrite = (historyWrite + 1) % (int) history.size();

    liveValue = value;
    repaint (scopeBounds);
}

juce::var ModSourceCard::dragDescriptionFor (int s)
{
    return juce::String (kModSourceDragPrefix) + juce::String (s);
}

void ModSourceCard::mouseDrag (const juce::MouseEvent& e)
{
    // Only from the header row, where nothing else takes the mouse.
    if (e.getMouseDownY() > 16 || e.getDistanceFromDragStart() < 4)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! container->isDragAndDropActive())
        {
            auto ghost = createComponentSnapshot (getLocalBounds().withHeight (juce::jmin (getHeight(), 70)));
            ghost.multiplyAllAlphas (0.6f);
            container->startDragging (dragDescriptionFor (slot), this, juce::ScaledImage (ghost), true);
        }
}

void ModSourceCard::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    auto bounds = getLocalBounds();

    // Each visible slider's name, in the space its row leaves on the left.
    g.setFont (Fonts::ui (11.0f));
    g.setColour (Palette::textMuted);

    for (auto* child : getChildren())
        if (auto* s = dynamic_cast<juce::Slider*> (child); s != nullptr && s->isVisible() && s->getName().isNotEmpty())
            g.drawText (s->getName(), juce::Rectangle<int> (0, s->getY(), kLabelWidth - 4, s->getHeight()),
                        juce::Justification::centredLeft, true);

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

    const int rowHeight = kRowHeight;

    // A slider's row leaves its label's width free on the left (paint draws it).
    auto nextRow = [&bounds, rowHeight] { return bounds.removeFromTop (rowHeight).reduced (0, 2); };
    auto sliderRow = [&nextRow] { return nextRow().withTrimmedLeft (kLabelWidth); };

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
            rateSlider.setBounds (sliderRow());
            phaseSlider.setBounds (sliderRow());   // SPEC-SWEEP: MM-14
            divisionBox.setBounds (nextRow());
            depthSlider.setBounds (sliderRow());
            symmetrySlider.setBounds (sliderRow());
            smoothingSlider.setBounds (sliderRow());
            break;

        case SourceKind::envelope:
            delaySlider.setBounds (sliderRow());
            attackSlider.setBounds (sliderRow());
            holdSlider.setBounds (sliderRow());
            decaySlider.setBounds (sliderRow());
            sustainSlider.setBounds (sliderRow());
            releaseSlider.setBounds (sliderRow());

            // SPEC-SWEEP: MM-18 / MM-19 / MM-20.
            placePair (envRetriggerBox, loopModeBox);
            {
                auto row = nextRow();
                const int third = row.getWidth() / 3;
                attackCurveBox.setBounds (row.removeFromLeft (third).reduced (1, 0));
                decayCurveBox.setBounds (row.removeFromLeft (third).reduced (1, 0));
                releaseCurveBox.setBounds (row.reduced (1, 0));
            }
            break;

        case SourceKind::sequencer:
            placePair (directionBox, syncButton);
            divisionBox.setBounds (nextRow());
            seqRateSlider.setBounds (sliderRow());   // SPEC-SWEEP: MM-23
            lengthSlider.setBounds (sliderRow());
            swingSlider.setBounds (sliderRow());
            break;

        case SourceKind::follower:
            placePair (followerSourceBox, detectionBox);
            placePair (followerStringBox, followerLogButton);   // SPEC-SWEEP: MM-25
            followerAttackSlider.setBounds (sliderRow());
            followerReleaseSlider.setBounds (sliderRow());
            thresholdSlider.setBounds (sliderRow());
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
    header.addColumn ("Destination", ColumnId::destination, 112);
    header.addColumn ("Depth", ColumnId::depth, 58);
    header.addColumn ("Offset", ColumnId::offset, 44);   // SPEC-SWEEP: MM-40
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

        case ColumnId::offset:   // SPEC-SWEEP: MM-40
            g.drawText ((route.offset > 0.0f ? "+" : "") + juce::String (juce::roundToInt (route.offset * 100.0f)) + "%",
                        area, juce::Justification::centred, false);
            break;

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
    const auto routeName = cached[(size_t) row].sourceId + " -> " + cached[(size_t) row].destinationId;

    switch (columnId)
    {
        case ColumnId::enabled:
            processor.pushUndoAction ((cached[(size_t) row].enabled ? "Turn off route " : "Turn on route ") + routeName,
                                      "mod-route-edit", {});   // action-and-undo.md 3.6
            matrix.setRouteEnabled (row, ! cached[(size_t) row].enabled);
            break;

        case ColumnId::remove:
            processor.pushUndoAction ("Remove " + cached[(size_t) row].sourceId + " from "
                                        + cached[(size_t) row].destinationId, "mod-route-delete", {});   // 3.6
            matrix.removeRoute (row);
            break;

        case ColumnId::depth:
        case ColumnId::offset:   // SPEC-SWEEP: MM-40
            editValue (row, columnId == ColumnId::offset, e);
            return;

        case ColumnId::curve:
        {
            const auto next = (ModCurve) (((int) cached[(size_t) row].curve + 1)
                                            % (int) ModCurve::numCurves);
            processor.pushUndoAction ("Change curve of " + routeName, "mod-route-edit", routeName);   // 3.6
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

void ModRouteTable::applyTypedValue (int row, bool isOffset, float value)
{
    auto& matrix = processor.getModMatrix();
    const float v = juce::jlimit (-1.0f, 1.0f, value);

    // action-and-undo.md 3.6 (integration's depth edit), for depth and offset alike.
    if (juce::isPositiveAndBelow (row, (int) cached.size()))
    {
        const auto routeName = cached[(size_t) row].sourceId + " -> " + cached[(size_t) row].destinationId;
        processor.pushUndoAction ((isOffset ? "Change offset of " : "Change depth of ") + routeName,
                                  "mod-route-edit", routeName);
    }

    if (isOffset)
        matrix.setRouteOffset (row, v);
    else
        matrix.setRouteDepth (row, v);

    refresh();

    if (onRoutesChanged)
        onRoutesChanged();
}

void ModRouteTable::editValue (int row, bool isOffset, const juce::MouseEvent& e)
{
    // Click-drag would fight the table's own mouse handling, so depth and
    // offset are typed rather than dragged.
    const auto& route = cached[(size_t) row];

    auto* editor = new juce::TextEditor();
    editor->setSize (80, 22);
    editor->setText (juce::String (isOffset ? route.offset : route.depth, 3), false);
    editor->selectAll();

    auto& box = juce::CallOutBox::launchAsynchronously (
        std::unique_ptr<juce::Component> (editor),
        getScreenBounds().withPosition (e.getScreenPosition()), nullptr);

    editor->onReturnKey = [this, editor, row, isOffset, &box]
    {
        applyTypedValue (row, isOffset, editor->getText().getFloatValue());
        box.dismiss();
    };

    editor->onEscapeKey = [&box] { box.dismiss(); };
    editor->grabKeyboardFocus();
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

    macro7Knob.attachTo (processor, ParamIDs::macroAssignA,
                         "Macro 7: a knob of your own. It does nothing until a modulation route uses it as a source.");
    macro8Knob.attachTo (processor, ParamIDs::macroAssignB,
                         "Macro 8: a knob of your own. It does nothing until a modulation route uses it as a source.");
    addAndMakeVisible (macro7Knob);
    addAndMakeVisible (macro8Knob);

    clearButton.onClick = [this]
    {
        processor.pushUndoAction ("Clear all modulation routes", "mod-route-delete", {});   // action-and-undo.md 3.6
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

        if (auto* p = processor.getState().getParameter (route.destinationId))   // action-and-undo.md 3.6
            processor.pushUndoAction ("Add " + modSourceDisplayName (card->getSlot()) + " to " + p->getName (64)
                                        + " depth 0.33", "mod-route-create", {});

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
             + 16                               // summary
             + Metrics::gridHalf
             + LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small);   // macros 7 and 8
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

    bounds.removeFromTop (Metrics::gridHalf);
    auto knobs = bounds.removeFromTop (LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small));
    const int knobWidth = juce::jmin (96, knobs.getWidth() / 2);
    macro7Knob.setBounds (knobs.removeFromLeft (knobWidth));
    macro8Knob.setBounds (knobs.removeFromLeft (knobWidth));
}

} // namespace luthier
