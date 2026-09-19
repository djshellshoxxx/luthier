#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

namespace luthier
{

//==============================================================================
/*  Advertises layouts A to D of routing-io 1 as one superset of buses, and
        lets the host disable the ones it cannot use.

        Every bus past the main output is created disabled. A host that only does
        stereo therefore sees layout A and never has to say so; a host with
        flexible routing enables what it wants and gets B, C or D. The alternative
        - four separately declared layouts - is not something the VST3 or AU bus
    model can express.

    The sidechain is an input bus rather than the main input because the plugin
    is an instrument: its main input carries nothing, and a host should not have
    to route audio into an instrument just to feed a detector.
*/
juce::AudioProcessor::BusesProperties LuthierAudioProcessor::buildBusesProperties()
{
    auto props = BusesProperties()
                   .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                   .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false);

    for (int bus = 0; bus < kNumAuxBuses; ++bus)
        props = props.withOutput (getAuxBusName (bus), juce::AudioChannelSet::stereo(), false);

    for (int s = 0; s < kNumPerStringBuses; ++s)
        props = props.withOutput ("String " + juce::String (s + 1),
                                  juce::AudioChannelSet::mono(), false);

    return props;
}

LuthierAudioProcessor::LuthierAudioProcessor()
    : AudioProcessor (buildBusesProperties()),
      apvts (*this, nullptr, "LUTHIER", Parameters::createLayout()),
      bridge (apvts, engine),
      presets (*this, apvts, engine),
      midiLearn (apvts),
      snapshots (*this)
{
    FactoryPresets::setProcessorForRanges (this);
    presets.ensureFactoryPresetsInstalled();
    presets.refresh();

    bridge.cachePointers();
    bridge.setModMatrix (&modMatrix);

    // The bank stores the snapshot blobs for the modules it does not own, and
    // hands them back at the crossfade midpoint for this to unpack.
    snapshots.onNonParameterState = [this] (const Snapshot& snapshot)
    {
        applySnapshotModules (snapshot);
    };

    // live-performance 11: pedal calibrations are user-global, so they come from
    // the user's config rather than from whatever preset happens to load first.
    expression.load();

    // accessibility 9 and updates-telemetry 6: both of these describe the person
    // rather than the sound, so they are user-global too.
    AccessibilitySettings::get().load();
    Localisation::get().setLocale (Localisation::get().getLocale());

    telemetry.loadSettings();
    telemetry.setTransport (createHttpsTransport());
    license.load();

    // tone-match 5: the IR folder tree exists before the user goes looking for
    // somewhere to put a file.
    IrLibraryPaths::ensureExists();

    // practice-tools 8: yesterday's unsaved session buffers go.
    SessionRecorder::cleanUpOldTempFiles (SessionRecorder::getTempDirectory());

    // 30 Hz is fast enough for the meters and the data stream, and slow enough
    // that it costs nothing.
    startTimerHz (30);
}

LuthierAudioProcessor::~LuthierAudioProcessor()
{
    stopTimer();

    if (diagnostics.isCrashLogEnabled())
        diagnostics.flushCrashLog (diagnostics.buildTroubleshootingReport (
            juce::JSON::toString (presets.toVar(), false), engine.getValidator().getSummary()));
}

//==============================================================================
void LuthierAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    engine.prepare (sampleRate, samplesPerBlock);
    sidechainCopy.setSize (2, juce::jmax (1, samplesPerBlock), false, true, false);
    sidechainCopy.clear();
    routing.prepare (sampleRate, samplesPerBlock);
    routing.setActiveLayout (getNegotiatedLayout());
    routing.setSidechainPresent (hasSidechainInput());
    midiOutRouter.prepare (sampleRate, samplesPerBlock);
    modMatrix.prepare (sampleRate, samplesPerBlock, apvts);
    transportWasRunning = false;
    midiCapture.prepare (sampleRate, 60.0);
    diagnostics.prepare (sampleRate);

    killSwitch.prepare (sampleRate);
    monitorMix.prepare (sampleRate, samplesPerBlock);
    monitorBuffer.setSize (2, juce::jmax (1, samplesPerBlock), false, true, false);
    monitorBuffer.clear();

    // ---- practice tools ------------------------------------------------------------
    metronome.prepare (sampleRate, samplesPerBlock);

    // A short loop by default: the whole maximum is a quarter of a gigabyte per
    // layer, and a user who wants four minutes can ask for it.
    looper.prepare (sampleRate, 30.0);

    backingTrack.prepare (sampleRate, samplesPerBlock);

    clickBuffer.setSize (1, juce::jmax (1, samplesPerBlock), false, true, false);
    backingBuffer.setSize (2, juce::jmax (1, samplesPerBlock), false, true, false);
    clickBuffer.clear();
    backingBuffer.clear();

    // ---- tone match ----------------------------------------------------------------
    bodyIr.prepare (sampleRate, samplesPerBlock);

    for (auto& slot : cabIr)
        slot.prepare (sampleRate, samplesPerBlock);

    capture.prepare (sampleRate);

    samplePosition = 0;

    bridge.applyAllNow();
    presets.applyExtraState();

    updateLatency();
    updateRoutingLatencyReport();

    Diagnostics::HostInfo info;
    info.hostName = juce::PluginHostType().getHostDescription();
    info.pluginFormat = juce::AudioProcessor::getWrapperTypeDescription (wrapperType);
    info.sampleRate = sampleRate;
    info.blockSize = samplesPerBlock;
    info.numInputChannels = getTotalNumInputChannels();
    info.numOutputChannels = getTotalNumOutputChannels();
    info.reportedLatencySamples = reportedLatency;
    diagnostics.setHostInfo (info);

    diagnostics.logValue (LogCategory::Engine, "prepareToPlay sampleRate", sampleRate);
    diagnostics.logValue (LogCategory::Engine, "prepareToPlay blockSize", (double) samplesPerBlock);
}

void LuthierAudioProcessor::releaseResources()
{
    engine.releaseResources();
    routing.reset();
    midiOutRouter.reset();
    modMatrix.reset();
    killSwitch.reset();
    monitorMix.reset();

    metronome.reset();
    looper.reset();
    backingTrack.reset();

    bodyIr.reset();

    for (auto& slot : cabIr)
        slot.reset();
}

bool LuthierAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto main = layouts.getMainOutputChannelSet();

    // Rule 2 of routing-io 0: the minimal stereo case must always work, so the
    // main output is the only bus with a hard requirement.
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono())
        return false;

    // The sidechain is optional; if the host enables it, it has to be mono or
    // stereo, because that is all a detector can meaningfully read.
    if (layouts.inputBuses.size() > 0)
    {
        const auto sc = layouts.getChannelSet (true, 0);

        if (! sc.isDisabled()
              && sc != juce::AudioChannelSet::mono()
              && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    for (int bus = 1; bus < layouts.outputBuses.size(); ++bus)
    {
        const auto set = layouts.getChannelSet (false, bus);

        if (set.isDisabled())
            continue;

        const bool isAuxBus = (bus - 1) < kNumAuxBuses;

        // Aux buses are stereo pairs; per-string buses are mono. Accepting the
        // wrong width would silently drop or duplicate a channel.
        if (isAuxBus)
        {
            if (set != juce::AudioChannelSet::stereo())
                return false;
        }
        else if (set != juce::AudioChannelSet::mono())
        {
            return false;
        }
    }

    return true;
}

//==============================================================================
BusLayout LuthierAudioProcessor::getNegotiatedLayout() const noexcept
{
    bool anyAux = false, anyPerString = false;

    for (int bus = 1; bus < getBusCount (false); ++bus)
    {
        if (getChannelCountOfBus (false, bus) <= 0)
            continue;

        if ((bus - 1) < kNumAuxBuses)
            anyAux = true;
        else
            anyPerString = true;
    }

    if (anyAux && anyPerString) return BusLayout::full;
    if (anyAux)                 return BusLayout::studio;
    if (anyPerString)           return BusLayout::perString;

    return BusLayout::stereoOnly;
}

bool LuthierAudioProcessor::hasSidechainInput() const noexcept
{
    return getBusCount (true) > 0 && getChannelCountOfBus (true, 0) > 0;
}

double LuthierAudioProcessor::getTailLengthSeconds() const
{
    // Long enough for a cathedral reverb tail plus a ringing open string.
    return 12.0;
}

void LuthierAudioProcessor::updateLatency()
{
    const int latency = engine.getLatencySamples();

    if (latency != reportedLatency)
    {
        reportedLatency = latency;
        setLatencySamples (latency);
    }

    updateRoutingLatencyReport();
}

void LuthierAudioProcessor::updateRoutingLatencyReport()
{
    // Routing-io 7. JUCE exposes a single latency value to the host, so this is
    // what the ROUTING panel shows the user for each output; the number handed
    // to the host stays the main output's, which is the largest.
    RoutingMatrix::LatencyReport report;
    report.mainOut = engine.getLatencySamples();
    report.auxDi = engine.getLatencySamples (AuxBus::di);
    report.auxPreCab = engine.getLatencySamples (AuxBus::ampPreCab);
    report.perString = engine.getPerStringLatencySamples();

    routing.setLatencyReport (report);
}

//==============================================================================
void LuthierAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // ---- routing, before anything reads or writes audio -----------------------
    routing.setActiveLayout (getNegotiatedLayout());
    routing.setSidechainPresent (hasSidechainInput());
    routing.updateWantedTaps (engine.getTapBuffers(), engine.getNumStrings());

    // The sidechain arrives on input bus 0, in channels the main output is about
    // to be written into, so it is copied out first - see sidechainCopy.
    auto sidechainIn = getBusBuffer (buffer, true, 0);
    const int sidechainChannels = juce::jmin (sidechainIn.getNumChannels(),
                                              sidechainCopy.getNumChannels());
    const int sidechainSamples = juce::jmin (numSamples, sidechainCopy.getNumSamples());

    if (hasSidechainInput() && sidechainChannels > 0 && sidechainSamples > 0)
    {
        for (int ch = 0; ch < sidechainChannels; ++ch)
            sidechainCopy.copyFrom (ch, 0, sidechainIn, ch, 0, sidechainSamples);

        engine.setSidechainInput (sidechainCopy.getArrayOfReadPointers(),
                                  sidechainChannels, sidechainSamples);
        routing.meterSidechain (sidechainCopy.getArrayOfReadPointers(),
                                sidechainChannels, sidechainSamples);
    }
    else
    {
        engine.setSidechainInput (nullptr, 0, 0);
        routing.meterSidechain (nullptr, 0, 0);
    }

    engine.setSidechainToAmp (routing.isSidechainToAmp() && hasSidechainInput());

    // MIDI out echoes what the host sent, so it has to be copied before the
    // engine reads the buffer and the router rewrites it.
    const auto midiOutConfig = routing.getMidiOutConfig();

    if (midiOutConfig.enabled)
        midiOutRouter.captureInput (midiMessages);

    // Host tempo, for tempo-synced delays and tremolo.
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpm = position->getBpm())
                hostTempo.store (*bpm);
        }
    }

    engine.setTempoBpm (hostTempo.load());

    // The rhythm engine's grid is locked to the host's own position, which is
    // what makes its scheduling sample-accurate rather than merely periodic.
    {
        double ppq = 0.0;
        bool playing = false;

        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                playing = position->getIsPlaying();

                if (auto value = position->getPpqPosition())
                    ppq = *value;
            }
        }

        engine.setTransportPosition (ppq, playing);
    }

    // MIDI Learn gets first look, so a CC being learned is not also acted on.
    midiLearn.processMidi (midiMessages);

    // live-performance 2: program change and bank select drive the live surface,
    // and are consumed so nothing downstream sees them as musical events.
    handleLiveMidi (midiMessages);

    midiCapture.capture (midiMessages, samplePosition);
    logMidiForDiagnostics (midiMessages);

    // The audition player and the fretboard preview inject their own MIDI.
    processAuditionMidi (midiMessages, numSamples);

    {
        const juce::ScopedTryLock sl (previewLock);

        if (sl.isLocked() && ! previewMidi.isEmpty())
        {
            for (const auto metadata : previewMidi)
                midiMessages.addEvent (metadata.getMessage(),
                                       juce::jlimit (0, juce::jmax (0, numSamples - 1), metadata.samplePosition));

            previewMidi.clear();
        }
    }

    // ---- modulation ------------------------------------------------------------
    // Sources first, then the matrix, then the bridge: the bridge reads every
    // parameter through ModMatrix::apply, so the offsets have to be current
    // before it runs.
    feedModulationSources (midiMessages);

    {
        ModBlockContext modContext;
        buildModBlockContext (buffer, numSamples, modContext);
        modMatrix.processBlock (numSamples, modContext);
    }

    bridge.applyToEngine();

    // The engine only ever writes the main output pair; every other bus belongs
    // to the routing matrix, and a bus nobody writes must be cleared rather than
    // left holding the previous block.
    {
        auto mainOut = getBusBuffer (buffer, false, 0);
        engine.processBlock (mainOut, midiMessages);
    }

    // ---- tone match ----------------------------------------------------------------
    /*  tone-match 1: a user cabinet IR replaces the model's, so it goes on the
        main output after the engine has produced it.

        The body IR slot is handled inside the engine, where the body is; this is
        the cabinet pair, which is the last thing before the master and therefore
        the last thing this can reach. */
    {
        auto mainOut = getBusBuffer (buffer, false, 0);

        for (auto& slot : cabIr)
            slot.process (mainOut.getArrayOfWritePointers(),
                          mainOut.getNumChannels(), numSamples);
    }

    // ---- the live surface --------------------------------------------------------
    // The recall crossfade is carried by the audio thread's own clock, so that it
    // takes the same time whatever the host's UI thread happens to be doing.
    snapshots.advance ((double) numSamples / juce::jmax (1.0, currentSampleRate));

    // live-performance 6: the kill switch cuts the main output only, and does it
    // before the aux taps are distributed - the DI and per-string stems are for
    // re-amping, and silencing them because the player hit a kill switch on stage
    // would be wrong.
    {
        auto mainOut = getBusBuffer (buffer, false, 0);
        killSwitch.processBlock (mainOut);
    }

    // ---- practice tools ----------------------------------------------------------
    /*  practice-tools 0.1 and 0.2: a closed panel costs nothing, and the click
        goes to the monitor rather than to the audience.

        The looper sits here, after the kill switch, so that what it records is
        what was heard. The metronome and the backing track are rendered into
        their own buffers and mixed into the monitor below; only the backing
        track reaches the main output, and only because playing along to one
        through the main out is what a practising guitarist expects. */
    bool haveClick = false;

    if (practicePanelOpen)
    {
        auto mainOut = getBusBuffer (buffer, false, 0);

        looper.processBlock (mainOut, numSamples);
        looper.captureMidi (midiMessages, numSamples);

        if (metronome.isEnabled())
        {
            metronome.processBlock (clickBuffer.getWritePointer (0), numSamples);
            haveClick = true;
        }

        if (backingTrack.isPlaying())
        {
            backingTrack.processBlock (backingBuffer, numSamples);

            for (int channel = 0; channel < juce::jmin (2, mainOut.getNumChannels()); ++channel)
                mainOut.addFrom (channel, 0, backingBuffer, channel, 0, numSamples);
        }

        // practice-tools 8: the session recorder takes what the plugin produced.
        sessionRecorder.processBlock (mainOut, numSamples);
    }

    routing.distribute (*this, buffer, engine.getTapBuffers(), engine.getNumStrings());

    // live-performance 7: the monitor mix is the performer's own, so it goes to
    // its own bus and never into the main output.
    {
        const bool haveSidechain = hasSidechainInput() && sidechainChannels > 0;

        if (monitorMix.isActive (haveSidechain, haveClick))
        {
            auto mainOut = getBusBuffer (buffer, false, 0);

            monitorMix.processBlock (monitorBuffer, mainOut,
                                     haveSidechain ? &sidechainCopy : nullptr,
                                     haveClick ? clickBuffer.getReadPointer (0) : nullptr,
                                     numSamples);

            routing.writeMonitorBus (*this, buffer, monitorBuffer, numSamples);
        }
    }

    // ---- MIDI out --------------------------------------------------------------
    // Always called: when MIDI out is off it clears the buffer, which is what
    // stops the host's own events leaking back out as an accidental echo.
    for (int m = 0; m < 6; ++m)
        if (auto* raw = apvts.getRawParameterValue (ParamIDs::macroByIndex (m)))
            midiOutRouter.setMacroValue (m, raw->load());

    midiOutRouter.emit (midiMessages, midiOutConfig, engine.getStringActivity(), numSamples);

    samplePosition += numSamples;

    // The engine's latency can change when an IR finishes loading or the
    // oversampling factor changes, so it is re-reported rather than assumed fixed.
    updateLatency();
}

//==============================================================================
void LuthierAudioProcessor::feedModulationSources (const juce::MidiBuffer& midi) noexcept
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            modMatrix.noteOn (message.getNoteNumber(), message.getFloatVelocity());
        }
        else if (message.isNoteOff())
        {
            modMatrix.noteOff();
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            modMatrix.allNotesOff();
        }
        else if (message.isController())
        {
            modMatrix.setControllerValue (message.getControllerNumber(),
                                          (double) message.getControllerValue() / 127.0);
        }
        else if (message.isPitchWheel())
        {
            // Bipolar, because that is what a pitch wheel is: centre is zero,
            // not a half.
            modMatrix.setPitchBend (((double) message.getPitchWheelValue() - 8192.0) / 8192.0);
        }
        else if (message.isChannelPressure())
        {
            modMatrix.setAftertouch ((double) message.getChannelPressureValue() / 127.0);
        }
        else if (message.isAftertouch())
        {
            modMatrix.setPolyAftertouch ((double) message.getAfterTouchValue() / 127.0);
        }
    }

    for (int m = 0; m < ParamIDs::kNumMacros; ++m)
        if (auto* raw = apvts.getRawParameterValue (ParamIDs::macroByIndex (m)))
            modMatrix.setMacroValue (m, (double) raw->load());
}

void LuthierAudioProcessor::buildModBlockContext (const juce::AudioBuffer<float>& output,
                                                  int numSamples,
                                                  ModBlockContext& context) noexcept
{
    context.bpm = hostTempo.load();
    context.positionBeats = -1.0;
    context.transportRunning = false;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            context.transportRunning = position->getIsPlaying();

            if (auto ppq = position->getPpqPosition())
                context.positionBeats = *ppq;
        }
    }

    context.transportJustStarted = context.transportRunning && ! transportWasRunning;
    transportWasRunning = context.transportRunning;

    // The followers watch the previous block's output. Measuring this block
    // would mean rendering it before deciding how to modulate it, which is
    // circular; one block of lag is what every envelope follower in a plugin
    // chain has, and at control rate it is inaudible.
    double peak = 0.0, sumSquares = 0.0;
    const int channels = juce::jmin (2, output.getNumChannels());

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = output.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const double v = (double) data[i];
            peak = juce::jmax (peak, std::abs (v));
            sumSquares += v * v;
        }
    }

    const double sampleCount = juce::jmax (1.0, (double) (numSamples * juce::jmax (1, channels)));

    context.mainOutputPeak = peak;
    context.mainOutputMeanSquare = sumSquares / sampleCount;

    // The pickup tap is the DI, which the routing taps already carry when it is
    // being rendered; when it is not, the main output stands in for it.
    context.pickupPeak = peak;
    context.pickupMeanSquare = context.mainOutputMeanSquare;

    context.sidechainPeak = routing.getSidechainLevel();
    context.sidechainMeanSquare = context.sidechainPeak * context.sidechainPeak * 0.5;

    for (int st = 0; st < kMaxStrings; ++st)
        context.perStringPeak[(size_t) st] = engine.getStringLevel (st);
}

//==============================================================================
void LuthierAudioProcessor::logMidiForDiagnostics (const juce::MidiBuffer& midi) noexcept
{
    if (! diagnostics.isEnabled())
        return;

    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();

        if (m.isNoteOn())
            diagnostics.logValue (LogCategory::Midi,
                                  "note on", (double) m.getNoteNumber(), samplePosition);
        else if (m.isNoteOff())
            diagnostics.logValue (LogCategory::Midi,
                                  "note off", (double) m.getNoteNumber(), samplePosition);
        else if (m.isController())
            diagnostics.logValue (LogCategory::Midi,
                                  "cc", (double) m.getControllerNumber(), samplePosition);
        else if (m.isPitchWheel())
            diagnostics.logValue (LogCategory::Midi,
                                  "pitch bend", (double) m.getPitchWheelValue(), samplePosition);
    }
}

//==============================================================================
void LuthierAudioProcessor::startAudition (AuditionPhrase::Type type)
{
    auditionType = type;
    uiState.auditionType = type;

    auditionSequence = AuditionPhrase::build (type, hostTempo.load());
    auditionEndSeconds = auditionSequence.getEndTime() + 0.25;
    auditionEventIndex = 0;
    auditionPositionSeconds = 0.0;

    auditionActive.store (true);

    diagnostics.log (LogCategory::Engine, "audition started", samplePosition);
}

void LuthierAudioProcessor::stopAudition()
{
    if (! auditionActive.exchange (false))
        return;

    // Release anything the phrase left holding.
    const juce::ScopedLock sl (previewLock);

    for (int note = 0; note < 128; ++note)
        previewMidi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
}

void LuthierAudioProcessor::processAuditionMidi (juce::MidiBuffer& midi, int numSamples)
{
    if (! auditionActive.load() || numSamples <= 0)
        return;

    const double blockSeconds = (double) numSamples / currentSampleRate;
    const double blockStart = auditionPositionSeconds;
    const double blockEnd = blockStart + blockSeconds;

    while (auditionEventIndex < auditionSequence.getNumEvents())
    {
        const auto* event = auditionSequence.getEventPointer (auditionEventIndex);

        if (event == nullptr)
        {
            ++auditionEventIndex;
            continue;
        }

        const double timestamp = event->message.getTimeStamp();

        if (timestamp >= blockEnd)
            break;

        const int offset = juce::jlimit (0, numSamples - 1,
                                         (int) ((timestamp - blockStart) * currentSampleRate));
        midi.addEvent (event->message, offset);
        ++auditionEventIndex;
    }

    auditionPositionSeconds = blockEnd;

    if (auditionPositionSeconds >= auditionEndSeconds)
        auditionActive.store (false);
}

//==============================================================================
void LuthierAudioProcessor::triggerPreviewNote (int stringIndex, double fretPosition, double velocity)
{
    const auto& tuning = engine.getTuningEngine();
    const double hz = tuning.computeFrequency (stringIndex, fretPosition);
    const int note = juce::jlimit (0, 127, (int) std::round (hzToMidi (hz, tuning.getConcertA())));

    const juce::ScopedLock sl (previewLock);

    // Guitar-controller convention: channel = string + 1, so the note lands on the
    // string the user actually clicked rather than wherever the voicer would put it.
    previewMidi.addEvent (juce::MidiMessage::noteOn (juce::jlimit (1, 16, stringIndex + 1), note,
                                                     (float) juce::jlimit (0.05, 1.0, velocity)), 0);
}

void LuthierAudioProcessor::releasePreviewNote (int stringIndex)
{
    const int note = engine.getStringMidiNote (stringIndex);

    if (note < 0)
        return;

    const juce::ScopedLock sl (previewLock);
    previewMidi.addEvent (juce::MidiMessage::noteOff (juce::jlimit (1, 16, stringIndex + 1), note), 0);
}

//==============================================================================
juce::String LuthierAudioProcessor::applyGenreKit (int kitIndex)
{
    if (! juce::isPositiveAndBelow (kitIndex, genreKits.getNumKits()))
        return {};

    const auto& kit = genreKits.getKit (kitIndex);

    GenreKitLibrary::apply (kit, engine.getRhythmEngine(), patternLibrary);

    // rhythm-engine 7: the rig is a soft reference. It is reported, never loaded.
    return kit.preferredPreset;
}

//==============================================================================
// The live surface (live-performance.md).
//==============================================================================

void LuthierAudioProcessor::handleLiveMidi (juce::MidiBuffer& midi) noexcept
{
    if (midi.isEmpty())
        return;

    juce::MidiBuffer kept;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        // live-performance 2: program change is the snapshot index.
        if (message.isProgramChange())
        {
            pendingSnapshotRecall.store (message.getProgramChangeNumber(),
                                         std::memory_order_relaxed);
            continue;
        }

        // Bank select picks the preset. Loading one touches the file system, so
        // the audio thread only records the request and the timer acts on it.
        if (message.isController() && message.getControllerNumber() == 0)
        {
            pendingPresetSelect.store (message.getControllerValue(), std::memory_order_relaxed);
            continue;
        }

        kept.addEvent (message, metadata.samplePosition);
    }

    midi.swapWith (kept);
}

void LuthierAudioProcessor::applySnapshotModules (const Snapshot& snapshot)
{
    if (snapshot.modMatrix.getDynamicObject() != nullptr)
        modMatrix.fromVar (snapshot.modMatrix);

    if (snapshot.rhythm.getDynamicObject() != nullptr)
        engine.getRhythmEngine().fromVar (snapshot.rhythm);

    if (snapshot.bypasses.getDynamicObject() != nullptr)
        if (auto* object = snapshot.bypasses.getDynamicObject();
            object != nullptr && object->hasProperty ("character"))
            engine.getCharacterEngine().fromVar (object->getProperty ("character"));
}

bool LuthierAudioProcessor::captureSnapshot (int index, const juce::String& label, int colourTag)
{
    if (! snapshots.capture (index, label, colourTag))
        return false;

    // The modules that do not live in the parameter tree are captured alongside
    // it, so that a snapshot is the whole instrument rather than just its knobs.
    auto snapshot = snapshots.getSnapshot (index);

    snapshot.modMatrix = modMatrix.toVar();
    snapshot.rhythm = engine.getRhythmEngine().toVar();

    // The character engine rides along in the blob the bank keeps for whatever
    // else a snapshot needs, so that switching snapshots does not silently
    // reroll the instrument.
    {
        auto* extras = new juce::DynamicObject();
        extras->setProperty ("character", engine.getCharacterEngine().toVar());

        snapshot.bypasses = juce::var (extras);
    }

    return snapshots.setSnapshot (index, snapshot);
}

bool LuthierAudioProcessor::recallSnapshot (int index)
{
    return snapshots.recall (index);
}

void LuthierAudioProcessor::nextSnapshot()
{
    const int count = snapshots.getNumSnapshots();

    if (count > 0)
        recallSnapshot ((snapshots.getCurrentSnapshot() + 1) % count);
}

void LuthierAudioProcessor::previousSnapshot()
{
    const int count = snapshots.getNumSnapshots();

    if (count > 0)
        recallSnapshot ((snapshots.getCurrentSnapshot() + count - 1) % count);
}

//==============================================================================
void LuthierAudioProcessor::tapTempoNow()
{
    // The plugin's own clock, so that tapping works identically whether or not
    // the host is running.
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    if (tapTempo.tap (now))
    {
        // live-performance 5: a tapped tempo drives the rhythm engine when the
        // host is stopped, so the engine is told about it straight away.
        engine.setTempoBpm (getEffectiveTempo());
    }
}

double LuthierAudioProcessor::getEffectiveTempo() const noexcept
{
    return tapTempo.getEffectiveBpm (hostTempo.load(), transportWasRunning);
}

//==============================================================================
void LuthierAudioProcessor::setLiveMode (bool shouldBeLive)
{
    uiState.liveMode = shouldBeLive;
}

//==============================================================================
bool LuthierAudioProcessor::loadSetlist (const juce::File& file)
{
    Setlist loaded;

    if (! loaded.loadFrom (file))
        return false;

    setlist.setSetlist (loaded);

    return applyCurrentSetlistEntry();
}

bool LuthierAudioProcessor::applyCurrentSetlistEntry()
{
    const auto* entry = setlist.getCurrentEntry();

    if (entry == nullptr)
        return false;

    const auto& data = setlist.getCurrentEntryData();

    if (data.getDynamicObject() == nullptr)
        return false;

    if (! presets.fromVar (data))
        return false;

    bridge.applyAllNow();

    // The preset carries its own snapshot bank, so the entry's snapshot index
    // only means anything once that bank has been loaded.
    if (entry->snapshotIndex > 0)
        recallSnapshot (entry->snapshotIndex);

    // live-performance 4: the next entry is read now, so that stepping onto it
    // costs nothing.
    setlist.preloadNext();

    return true;
}

//==============================================================================
void LuthierAudioProcessor::panic()
{
    stopAudition();
    engine.panic();

    const juce::ScopedLock sl (previewLock);
    previewMidi.clear();

    diagnostics.log (LogCategory::Engine, "panic", samplePosition);
}

void LuthierAudioProcessor::resetEverything()
{
    pushUndoState ("Reset");

    panic();
    presets.resetToDefaults();
    midiLearn.clearAllMappings();
    lockedParameters.clear();

    uiState = UiState {};

    bridge.applyAllNow();
    engine.reset();

    diagnostics.log (LogCategory::Engine, "reset to defaults", samplePosition);
}

void LuthierAudioProcessor::hardResetAndClearCaches()
{
    resetEverything();

    // Remove the things a normal reset deliberately leaves alone.
    auto diagFolder = Diagnostics::getDiagnosticsFolder();

    if (diagFolder.isDirectory())
    {
        for (const auto& e : juce::RangedDirectoryIterator (diagFolder, false, "*.txt", juce::File::findFiles))
            e.getFile().deleteFile();
    }

    auto cacheFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                         .getChildFile ("Luthier").getChildFile ("Cache");

    if (cacheFolder.isDirectory())
        cacheFolder.deleteRecursively();

    diagnostics.setCrashLogEnabled (false);
    diagnostics.setEnabled (false);
    diagnostics.reset();

    undoStack.clear();
    undoPosition = -1;
    slotA.reset();
    slotB.reset();
    slotBActive = false;

    // Reinstate the factory bank in case it was the thing that was broken.
    presets.ensureFactoryPresetsInstalled();
    presets.refresh();
}

void LuthierAudioProcessor::randomiseParameters()
{
    pushUndoState ("Randomise");

    presets.randomise ((uint64_t) juce::Time::currentTimeMillis(), lockedParameters);
    bridge.applyAllNow();

    diagnostics.log (LogCategory::Engine, "randomised", samplePosition);
}

void LuthierAudioProcessor::setParameterLocked (const juce::String& paramId, bool locked)
{
    if (locked)
        lockedParameters.addIfNotAlreadyThere (paramId);
    else
        lockedParameters.removeString (paramId);
}

bool LuthierAudioProcessor::isParameterLocked (const juce::String& paramId) const
{
    return lockedParameters.contains (paramId);
}

//==============================================================================
juce::MemoryBlock LuthierAudioProcessor::captureStateBlock()
{
    juce::MemoryBlock block;
    getStateInformation (block);
    return block;
}

std::unique_ptr<juce::AudioProcessor> LuthierAudioProcessor::createOfflineInstance()
{
    return std::make_unique<LuthierAudioProcessor>();
}

//==============================================================================
void LuthierAudioProcessor::storeToSlot (bool useSlotB)
{
    auto& target = useSlotB ? slotB : slotA;
    target = captureStateBlock();
}

void LuthierAudioProcessor::recallSlot (bool useSlotB)
{
    auto& source = useSlotB ? slotB : slotA;

    if (source.getSize() > 0)
        setStateInformation (source.getData(), (int) source.getSize());
}

void LuthierAudioProcessor::copyAtoB()
{
    slotB = slotBActive ? slotA : captureStateBlock();

    if (! slotBActive)
        slotA = slotB;
}

void LuthierAudioProcessor::setSlotBActive (bool b)
{
    if (b == slotBActive)
        return;

    // Store what is on screen into the slot being left, then recall the other.
    storeToSlot (slotBActive);
    slotBActive = b;
    recallSlot (slotBActive);
}

//==============================================================================
void LuthierAudioProcessor::pushUndoState (const juce::String& description)
{
    // Drop anything ahead of the current position: a new edit after an undo
    // starts a new branch.
    while (undoStack.size() > undoPosition + 1)
        undoStack.removeLast();

    UndoEntry entry;
    entry.state = captureStateBlock();
    entry.description = description;

    undoStack.add (std::move (entry));

    while (undoStack.size() > kMaxUndoSteps)
        undoStack.remove (0);

    undoPosition = undoStack.size() - 1;
}

void LuthierAudioProcessor::undo()
{
    if (! canUndo())
        return;

    // The first undo has to capture where we are now, or there would be nothing
    // to redo back to.
    if (undoPosition == undoStack.size() - 1)
    {
        UndoEntry current;
        current.state = captureStateBlock();
        current.description = "Current";
        undoStack.add (std::move (current));
    }

    --undoPosition;

    const auto& entry = undoStack.getReference (undoPosition);
    setStateInformation (entry.state.getData(), (int) entry.state.getSize());
}

void LuthierAudioProcessor::redo()
{
    if (! canRedo())
        return;

    ++undoPosition;

    const auto& entry = undoStack.getReference (undoPosition);
    setStateInformation (entry.state.getData(), (int) entry.state.getSize());
}

juce::String LuthierAudioProcessor::getUndoDescription() const
{
    if (! canUndo())
        return {};

    return undoStack.getReference (undoPosition).description;
}

juce::String LuthierAudioProcessor::getRedoDescription() const
{
    if (! canRedo())
        return {};

    return undoStack.getReference (undoPosition + 1).description;
}

//==============================================================================
int LuthierAudioProcessor::getNumPrograms()
{
    return juce::jmax (1, presets.getNumPresets());
}

int LuthierAudioProcessor::getCurrentProgram()
{
    return juce::jmax (0, presets.getCurrentPresetIndex());
}

void LuthierAudioProcessor::setCurrentProgram (int index)
{
    if (presets.loadPreset (index))
        bridge.applyAllNow();
}

const juce::String LuthierAudioProcessor::getProgramName (int index)
{
    if (const auto* info = presets.getPreset (index))
        return info->name;

    return "Init";
}

void LuthierAudioProcessor::changeProgramName (int, const juce::String&)
{
    // Presets are files; renaming is done through Save As rather than in place.
}

//==============================================================================
void LuthierAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    presets.captureExtraState();

    auto* root = new juce::DynamicObject();

    root->setProperty ("preset", presets.toVar (presets.getCurrentPresetName()));
    root->setProperty ("midiLearn", midiLearn.toVar());

    auto* ui = new juce::DynamicObject();
    ui->setProperty ("advancedMode", uiState.advancedMode);
    ui->setProperty ("tooltipsEnabled", uiState.tooltipsEnabled);
    ui->setProperty ("selectedString", uiState.selectedString);
    ui->setProperty ("advancedTab", uiState.advancedTab);
    ui->setProperty ("easterEggFound", uiState.easterEggFound);
    ui->setProperty ("editorWidth", uiState.editorWidth);
    ui->setProperty ("editorHeight", uiState.editorHeight);
    ui->setProperty ("auditionType", (int) uiState.auditionType);
    root->setProperty ("ui", juce::var (ui));

    juce::Array<juce::var> locks;

    for (const auto& id : lockedParameters)
        locks.add (id);

    root->setProperty ("lockedParameters", locks);
    root->setProperty ("slotBActive", slotBActive);
    root->setProperty ("routing", routing.toVar());
    root->setProperty ("modulation", modMatrix.toVar());
    root->setProperty ("rhythm", engine.getRhythmEngine().toVar());

    // live-performance 1: the snapshot bank travels inside the preset.
    root->setProperty ("snapshots", snapshots.toVar());
    root->setProperty ("liveMode", uiState.liveMode);

    // character-wear 1: the seed and the wear map are the instrument's identity,
    // so they belong to the preset rather than to the user.
    root->setProperty ("character", engine.getCharacterEngine().toVar());

    // tone-match 7: the IR slots store their file by path plus their settings.
    {
        auto* irs = new juce::DynamicObject();

        irs->setProperty ("body", bodyIr.toVar());
        irs->setProperty ("cab1", cabIr[0].toVar());
        irs->setProperty ("cab2", cabIr[1].toVar());

        root->setProperty ("toneMatch", juce::var (irs));
    }

    // practice-tools 1: the metronome's settings are part of the session.
    root->setProperty ("metronome", metronome.toVar());

    const auto json = juce::JSON::toString (juce::var (root), false);

    destData.reset();
    destData.append (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void LuthierAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    const juce::String json (juce::CharPointer_UTF8 (static_cast<const char*> (data)),
                             (size_t) sizeInBytes);

    const auto parsed = juce::JSON::parse (json);
    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return;

    if (root->hasProperty ("preset"))
        presets.fromVar (root->getProperty ("preset"));

    if (root->hasProperty ("midiLearn"))
        midiLearn.fromVar (root->getProperty ("midiLearn"));

    if (auto* ui = root->getProperty ("ui").getDynamicObject())
    {
        uiState.advancedMode = ui->getProperty ("advancedMode");
        uiState.tooltipsEnabled = ui->hasProperty ("tooltipsEnabled") ? (bool) ui->getProperty ("tooltipsEnabled") : true;
        uiState.selectedString = (int) ui->getProperty ("selectedString");
        uiState.advancedTab = (int) ui->getProperty ("advancedTab");
        uiState.easterEggFound = ui->getProperty ("easterEggFound");
        uiState.editorWidth = juce::jmax (900, (int) ui->getProperty ("editorWidth"));
        uiState.editorHeight = juce::jmax (540, (int) ui->getProperty ("editorHeight"));
        uiState.auditionType = (AuditionPhrase::Type) juce::jlimit (
            0, (int) AuditionPhrase::Type::NumTypes - 1, (int) ui->getProperty ("auditionType"));
        auditionType = uiState.auditionType;
    }

    lockedParameters.clear();

    if (auto* locks = root->getProperty ("lockedParameters").getArray())
        for (const auto& l : *locks)
            lockedParameters.add (l.toString());

    slotBActive = root->getProperty ("slotBActive");

    // Routing-io 9: mute, solo, gain, sidechain-to-amp and the MIDI-out
    // assignments travel with the preset. The bus layout does not.
    if (root->hasProperty ("routing"))
        routing.fromVar (root->getProperty ("routing"));

    // modulation-matrix 6: the full matrix travels with the preset. Unknown
    // destinations are dropped and reported rather than refused.
    if (root->hasProperty ("modulation"))
        modMatrix.fromVar (root->getProperty ("modulation"));

    // rhythm-engine 9: the pattern, voicing settings and genre kit travel with
    // the preset as one blob.
    if (root->hasProperty ("rhythm"))
        engine.getRhythmEngine().fromVar (root->getProperty ("rhythm"));

    // live-performance 1 and 11: the snapshots and the live-mode preference are
    // both per-preset. A preset saved before snapshots existed simply has none,
    // which the spec treats as one implicit snapshot equal to the preset.
    if (root->hasProperty ("snapshots"))
        snapshots.fromVar (root->getProperty ("snapshots"));
    else
        snapshots.clear();

    uiState.liveMode = (bool) root->getProperty ("liveMode");

    if (root->hasProperty ("character"))
        engine.getCharacterEngine().fromVar (root->getProperty ("character"));

    if (auto* irs = root->getProperty ("toneMatch").getDynamicObject())
    {
        bodyIr.fromVar (irs->getProperty ("body"));
        cabIr[0].fromVar (irs->getProperty ("cab1"));
        cabIr[1].fromVar (irs->getProperty ("cab2"));
    }

    if (root->hasProperty ("metronome"))
        metronome.fromVar (root->getProperty ("metronome"));

    presets.applyExtraState();
    bridge.applyAllNow();
}

//==============================================================================
void LuthierAudioProcessor::timerCallback()
{
    // live-performance 2: carry out whatever the MIDI thread asked for.
    if (const int snapshot = pendingSnapshotRecall.exchange (-1, std::memory_order_relaxed);
        snapshot >= 0)
    {
        recallSnapshot (snapshot);
    }

    if (const int bank = pendingPresetSelect.exchange (-1, std::memory_order_relaxed);
        bank >= 0)
    {
        if (presets.loadPreset (bank))
            bridge.applyAllNow();
    }

    if (diagnostics.isCrashLogEnabled())
    {
        presets.captureExtraState();

        diagnostics.flushCrashLog (diagnostics.buildTroubleshootingReport (
            juce::JSON::toString (presets.toVar(), false),
            engine.getValidator().getSummary()));
    }
}

//==============================================================================
juce::AudioProcessorEditor* LuthierAudioProcessor::createEditor()
{
    return new LuthierAudioProcessorEditor (*this);
}

} // namespace luthier

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new luthier::LuthierAudioProcessor();
}
