#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

namespace luthier
{

//==============================================================================
LuthierAudioProcessor::LuthierAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LUTHIER", Parameters::createLayout()),
      bridge (apvts, engine),
      presets (*this, apvts, engine),
      midiLearn (apvts)
{
    FactoryPresets::setProcessorForRanges (this);
    presets.ensureFactoryPresetsInstalled();
    presets.refresh();

    bridge.cachePointers();

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
    midiCapture.prepare (sampleRate, 60.0);
    diagnostics.prepare (sampleRate);

    samplePosition = 0;

    bridge.applyAllNow();
    presets.applyExtraState();

    updateLatency();

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
}

bool LuthierAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
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
}

//==============================================================================
void LuthierAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

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

    // MIDI Learn gets first look, so a CC being learned is not also acted on.
    midiLearn.processMidi (midiMessages);

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

    bridge.applyToEngine();

    engine.processBlock (buffer, midiMessages);

    samplePosition += numSamples;

    // The engine's latency can change when an IR finishes loading or the
    // oversampling factor changes, so it is re-reported rather than assumed fixed.
    updateLatency();
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

    presets.applyExtraState();
    bridge.applyAllNow();
}

//==============================================================================
void LuthierAudioProcessor::timerCallback()
{
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
