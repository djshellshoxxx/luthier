#include "PluginProcessor.h"
#include "Updates/CrashWriter.h"   // SPEC-SWEEP: UT-16
#include "Workshop/FamilyDefaults.h"   // guitar-illustration.md 12.3 (VISUAL-WORKSHOP-QA)
#include "Presets/FactoryPresets.h"
#include "Support/ErrorLog.h"
#include "Model/Guitar/BassDefaults.h"   // MODEL-GAPS

/*  The test runner and the offline renderer build this file, so that the things
    only the processor owns - the undo stack, uiState, A/B slots, snapshot recall,
    the state-model.md load flows - can be tested rather than only inspected.

    They do not build Source/UI, so the editor is compiled out for them. This is
    the only place the processor knows about the editor at all. */
#if ! LUTHIER_HEADLESS
 #include "PluginEditor.h"
#endif

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

    // Aux 8 (pick-noise 1.3), last so that no earlier bus number moves.
    props = props.withOutput (getAuxBusName (kNoiseAux), juce::AudioChannelSet::stereo(), false);

    // jam-mode.md 7 (FEAT-JAM): Aux 9 and 10, after Aux 8, so nothing moves.
    props = props.withOutput (getAuxBusName (kJamDrumsAux), juce::AudioChannelSet::stereo(), false);
    props = props.withOutput (getAuxBusName (kJamBassAux), juce::AudioChannelSet::stereo(), false);

    return props;
}

LuthierAudioProcessor::LuthierAudioProcessor()
    : AudioProcessor (buildBusesProperties()),
      apvts (*this, nullptr, "LUTHIER", Parameters::createLayout()),
      bridge (apvts, engine),
      presets (*this, apvts, engine, ranges),
      midiLearn (apvts),
      snapshots (*this)
{
    FactoryPresets::setProcessorForRanges (this);

    // installer.md 6: the user folder tree, config/plugin.json and the
    // .installed_version marker (first run / upgrade detection).
    installLayoutResult = InstallLayout::ensure (InstallLayout::defaultRoot(), JucePlugin_VersionString);

    for (int m = 0; m < ParamIDs::kNumMacros; ++m)
        macroValues[(size_t) m] = apvts.getRawParameterValue (ParamIDs::macroByIndex (m));

    // notation-export 6.1 (MODEL-GAPS): the engine reports what it plays - string,
    // fret and technique - straight to the capture.
    engine.setPerformanceCapture (&performanceCapture);

    // guitar-workshop.md 0.6: a guitar type loads its factory guitar file.
    partLibrary.refresh();

    for (const auto& error : partLibrary.getScanErrors())
        ErrorLog::write (ErrorLog::Severity::warn, "Workshop", "PART_UNREADABLE", error);

    bridge.onLoadGuitarType = [this] (GuitarType type) { return loadGuitarForType (type); };
    bridge.beforeStructuralChange = [this] { fadeOutBeforeStructuralChange(); };
    bridge.afterStructuralChange  = [this] { fadeInAfterStructuralChange(); };
    presets.onBeforeLoad = [this] { fadeOutBeforeStructuralChange(); };
    presets.onAfterLoad  = [this] { fadeInAfterStructuralChange(); };
    setCapoPart (partLibrary.getDefault (PartType::capo));
    presets.captureGuitarBlock = [this] { return getGuitarBlock(); };
    presets.onGuitarBlockLoaded = [this] (const juce::var& block)
    {
        takeGuitarBlock (block);

        // SPEC-SWEEP: PR-44 - the end of every load: the preset's ranges are
        // in, so the modulation family's setter clamps follow them.
        modMatrix.setModulationRangeAdvanced (ranges.isFamilyAdvanced (RangeFamily::modulation));
    };

    // action-and-undo.md 3.12: a completed learn is one entry.
    midiLearn.onBeforeLearn = [this] (const juce::String& id, int cc)
    {
        auto* parameter = apvts.getParameter (id);
        pushUndoAction ("Learn CC " + juce::String (cc) + " -> " + (parameter != nullptr ? parameter->getName (64) : id),
                        "midi-learn", {});
    };

    // A preset's pedals come with their settings; build them keeping those.
    presets.onPedalTypesLoaded = [this] { bridge.adoptPedalTypesFromParameters(); };

    // SPEC-SWEEP: SM-1/SM-16/FF-24..29 - the processor's preset blocks, with
    // the modules' as-built state as the default for a block a file lacks.
    captureDefaultPresetBlocks();
    presets.capturePresetBlocks = [this] (juce::DynamicObject& root) { writePresetBlocks (root); };
    presets.onPresetBlocksLoaded = [this] (const juce::DynamicObject& root) { readPresetBlocks (root); };
    // output-normalization.md 4.4: a preset load is a discrete configuration
    // event, and a cached gain rides the load.
    presets.onPresetLoaded = [this]
    {
        presetFileLoaded();   // SPEC-SWEEP: SM-46 - the layers a user-facing load clears (A/B compare)
        outputNormalization.notifyConfigurationChanged (true);
    };
    presets.ensureFactoryPresetsInstalled();
    presets.refresh();

    bridge.cachePointers();
    bridge.setModMatrix (&modMatrix);
    micLegacyAutomation = std::make_unique<MicLegacyAutomation> (apvts);   // mic-placement.md 4

    // SPEC-SWEEP TM-6 (tone-match 1): the body IR slot runs inside the engine,
    // where the body is.
    engine.setBodyIrSlot (&bodyIr);

    // practice-tools 12.1: the routine runner drives the processor's own tools,
    // the history is the saved one, and the saved defaults apply at start.
    practiceRunner.setTargets (getPracticeTargets());
    practiceStats.load();

    {
        PracticeDefaults defaults;
        juce::String error;

        if (PracticeDefaults::load (PracticeDefaults::getDefaultsFile(), defaults, error))
            defaults.applyTo (getPracticeTargets());
    }

    // jam-mode.md (FEAT-JAM): the factory styles in their slots, the preset's
    // jam block, and the tune's chord map for every timeline built.
    for (int i = 0; i < jam::kNumFactoryStyles; ++i)
        jam.setStyleSlot (i, jamStyles.getFactoryStyle (i));

    jamStyles.loadOverrides (JamStyleLibrary::getFactoryOverrideFolder());

    for (int i = 0; i < jam::kNumFactoryStyles; ++i)
        jam.setStyleSlot (i, jamStyles.getFactoryStyle (i));

    jam.setStyleSlot (jam::kUserStyleIndex, jamStyles.getFactoryStyle (0));
    jamOutputRaw = apvts.getRawParameterValue (ParamIDs::jamOutput);
    presets.captureJamBlock = [this] { return getJamBlock(); };
    presets.onJamBlockLoaded = [this] (const juce::var& block) { setJamBlock (block); };
    presets.keepOnLoad = [this] (const juce::String& id) { return id == ParamIDs::jamEnabled && jam.isBandRunning(); };
    tuneSession.onTimelineBuilt = [this] (const TuneTimeline& timeline) { onJamTimeline (timeline); };

    // tune-builder 8: the tune drives the rhythm engine's pattern and kit.
    tuneSession.attachPlayer (&tunePlayer);
    tuneSession.attachRhythm (&engine.getRhythmEngine(), &genreKits, &patternLibrary);

    // The bank stores the snapshot blobs for the modules it does not own, and
    // hands them back at the crossfade midpoint for this to unpack.
    snapshots.onNonParameterState = [this] (const Snapshot& snapshot)
    {
        applySnapshotModules (snapshot);
    };

    // live-performance 11: pedal calibrations are user-global, so they come from
    // the user's config rather than from whatever preset happens to load first.
    expression.load();
    expressionStage.update (expression);   // SPEC-SWEEP IR-11

    // SPEC-SWEEP: LP-11 - live actions on learned CCs. User-global like the
    // calibrations; the audio thread flips atomics, the timer acts.
    liveActions.setKillSwitch (&killSwitch);
    liveActions.handlers.nextSnapshot     = [this] { nextSnapshot(); };
    liveActions.handlers.previousSnapshot = [this] { previousSnapshot(); };
    liveActions.handlers.recallSnapshot   = [this] (int index) { recallSnapshot (index); };
    liveActions.handlers.tapAt            = [this] (double t) { tapTempoAt (t); };
    liveActions.handlers.panic            = [this] { panic(); };
    liveActions.handlers.setlistNext      = [this] { if (setlist.next()) applyCurrentSetlistEntry(); };
    liveActions.handlers.setlistPrevious  = [this] { if (setlist.previous()) applyCurrentSetlistEntry(); };
    liveActions.handlers.onCcAssigned     = [this] (int cc) { midiLearn.removeMappingForCc (cc); };
    liveActions.load();
    expressionInput.syncWith (expression);

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

    // guitar-workshop 0.6 / host-integration 3: the default guitar's parts are
    // the overlapping parameters' starting values. Written here, before a host
    // reads anything, so the first prepare does not move parameters under it
    // (clap-validator: parameters must not change by themselves).
    if (auto* type = apvts.getRawParameterValue (ParamIDs::guitarType))
        loadGuitarForType ((GuitarType) juce::jlimit (0, (int) GuitarType::NumTypes - 1, (int) type->load()));

    // action-and-undo.md 3.1: one undo entry per parameter gesture.
    for (auto* parameter : getParameters())
        parameter->addListener (this);

    // SPEC-SWEEP (include.md INC-29): the troubleshooting report's LICENCE and
    // MIDI sections. Message thread, while a report is built; never the key.
    diagnostics.setReportSectionsProvider ([this]
    {
        auto text = [this] (const char* id)
        {
            auto* p = apvts.getParameter (id);
            return p != nullptr ? p->getCurrentValueAsText() : juce::String ("?");
        };

        const auto state = license.getState();
        juce::String r;

        r << "---- LICENCE ---------------------------------------------------\n"
          << "State:            " << License::getStateName (state) << "\n";

        if (state == License::State::activated || state == License::State::grace)
            r << "Revalidate in:    " << license.getDaysUntilRevalidation() << " days\n";

        r << "\n"
          << "---- MIDI ------------------------------------------------------\n"
          << "Playing mode:     " << text (ParamIDs::playingMode) << "\n"
          << "MPE:              " << text (ParamIDs::mpeEnabled) << "\n"
          << "MIDI Learn:       " << midiLearn.getNumMappings() << " mapping(s)\n"
          << "Wrapper:          " << juce::AudioProcessor::getWrapperTypeDescription (wrapperType) << "\n";

        return r;
    });

    // 30 Hz is fast enough for the meters and the data stream, and slow enough
    // that it costs nothing.
    startTimerHz (30);
}

LuthierAudioProcessor::~LuthierAudioProcessor()
{
    stopTimer();

    FactoryPresets::forgetProcessorForRanges (this);   // SPEC-SWEEP: use-after-free found by the sweep's tests

    for (auto* parameter : getParameters())
        parameter->removeListener (this);

    if (diagnostics.isCrashLogEnabled())
        diagnostics.flushCrashLog (diagnostics.buildTroubleshootingReport (
            juce::JSON::toString (presets.toVar(), false), engine.getValidator().getSummary()));
}

//==============================================================================
void LuthierAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    humCapture.prepare (sampleRate);   // tune-builder 13 (TUNE-HELP-ONBOARDING)

    // gui-integration 15: left for the window to find, because there may not be
    // one right now. claimSampleRateChange decides whether it is worth saying.
    preparedSampleRate.store (sampleRate, std::memory_order_relaxed);

    // The per-string state (gauges, fine tune, realism detune) as it stands -
    // a restored session's, or the loaded guitar's - so the applyExtraState
    // below puts back what the engine had rather than stale defaults.
    // Only once there is something to keep: a fresh instance's saved extras
    // are the defaults, and would overwrite what the guitar and parameters set.
    const bool keepExtraState = initialStateApplied;

    if (keepExtraState)
        presets.captureExtraState();

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
    performanceCapture.prepare (sampleRate);

    tunePlayer.prepare (sampleRate, samplesPerBlock);

    // jam-mode 13 (FEAT-JAM): a re-prepare rebuilds the band and sends it back to Armed.
    jam.prepare (sampleRate, samplesPerBlock);
    jamNotes.ensureSize (8192);
    jamTuneBass.ensureSize (4096);
    jamScratch.ensureSize (TunePlayer::kRecommendedMidiBytes);
    jamMain.setSize (2, juce::jmax (1, samplesPerBlock), false, true, false);
    jamMain.clear();

    for (auto* tuneBuffer : { &tuneToEngine, &tuneToMidiOut, &tuneDirect })
        tuneBuffer->ensureSize (TunePlayer::kRecommendedMidiBytes);

    sliceMidi.ensureSize (8192);
    sliceMidiOut.ensureSize (8192);
    liveMidiKept.ensureSize (8192);
    controllerScratch.ensureSize (8192);   // SPEC-SWEEP CT-4
    captureStringCount = -1;   // re-sent at the next drain
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
    testSignalBuffer.setSize (1, juce::jmax (1, samplesPerBlock), false, true, false);   // SPEC-SWEEP TM-17
    tuneClick.prepare (sampleRate, samplesPerBlock);
    tuneClickBuffer.setSize (1, juce::jmax (1, samplesPerBlock), false, true, false);
    tuneClickRinging = false;
    backingBuffer.setSize (2, juce::jmax (1, samplesPerBlock), false, true, false);
    clickBuffer.clear();
    backingBuffer.clear();

    // ---- tone match ----------------------------------------------------------------
    bodyIr.prepare (sampleRate, samplesPerBlock);

    for (auto& slot : cabIr)
        slot.prepare (sampleRate, samplesPerBlock);

    capture.prepare (sampleRate);

    samplePosition = 0;

    // The guitar may have been built into the engine before it knew its sample
    // rate (the constructor loads the default one): rebuild the engine side
    // now. Parameters are not written - they are the player's or the host's.
    if (partsGuitarLoaded)
        engine.applyWorkshopGuitar (mapSpec (currentGuitar), engine.getGuitarType());

    bridge.applyAllNow();

    if (presets.hasExtraState())
        presets.applyExtraState();

    initialStateApplied = true;

    // cpu-quality-modes 2.5 / 2.6: re-read the machine's setting (another
    // process may have changed it) and apply the level as a hard switch.
    PerformanceSettings::get().reloadIfChanged();
    cpuLoad.reset();
    samplesSinceStringDrop = (juce::int64) sampleRate;   // E3 is armed from the start
    lastNonRealtime = isNonRealtime();
    qualityController.setNonRealtime (lastNonRealtime);
    applyQualityForBlock (true);

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

    // output-normalization.md 12: the calibration is kept across a rate change;
    // a new rate family (NormalizationSoundState::rateFamily) re-measures.
    outputNormalization.getTracker().markConfigDirty (true);
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
    // SPEC-SWEEP HI-10 (host-integration 2): stereo only - a mono main out is
    // refused, so the host shows its "not supported" message.
    if (main != juce::AudioChannelSet::stereo())
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

        // Aux pairs, then the per-string buses, then Aux 8 (a pair again).
        const bool isAuxBus = (bus - 1) < kNumAuxBuses || (bus - 1) >= kNumAuxBuses + kNumPerStringBuses;

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
int LuthierAudioProcessor::setRanges (const RangeState& newState)
{
    ranges = newState;
    const int clamped = ranges.applyTo (apvts);

    // SPEC-SWEEP: PR-44 / AR-15 - advanced-ranges.md 2.1: the modulation
    // family is setter clamps in the matrix, not parameter ranges.
    return clamped + modMatrix.setModulationRangeAdvanced (ranges.isFamilyAdvanced (RangeFamily::modulation));
}

int LuthierAudioProcessor::changeRanges (const RangeState& newState, const juce::String& undoDescription)
{
    pushUndoState (undoDescription);

    const int clamped = setRanges (newState);
    bridge.applyAllNow();

    diagnostics.logValue (LogCategory::Engine, "ranges changed, values clamped", clamped, samplePosition);
    return clamped;
}

//==============================================================================
juce::String LuthierAudioProcessor::getFactoryGuitarPath (GuitarType type)
{
    // guitar-workshop.md 0.6: the enum survives as a shortcut to a file.
    switch (type)
    {
        case GuitarType::Stratocaster:     return "Electric/Vintage Double-Cut.luthierguitar";
        case GuitarType::Telecaster:       return "Electric/Classic T-Style.luthierguitar";
        case GuitarType::LesPaul:          return "Electric/Vintage Single-Cut.luthierguitar";
        case GuitarType::SG:               return "Electric/Double-Cut Devil.luthierguitar";
        case GuitarType::ES335:            return "Electric/Semi-Hollow 335.luthierguitar";
        case GuitarType::Jazzmaster:       return "Electric/Offset Modern.luthierguitar";
        case GuitarType::Explorer:         return "Electric/Angular Korina.luthierguitar";
        case GuitarType::IbanezRG:         return "Electric/Superstrat Locking.luthierguitar";
        case GuitarType::SevenString:      return "Electric/7-String Modern.luthierguitar";
        case GuitarType::EightString:      return "Electric/8-String Modern.luthierguitar";
        case GuitarType::BaritoneElectric: return "Electric/Baritone Electric.luthierguitar";
        case GuitarType::Dreadnought:      return "Acoustic/Dreadnought.luthierguitar";
        case GuitarType::Auditorium:       return "Acoustic/Grand Auditorium.luthierguitar";
        case GuitarType::Jumbo:            return "Acoustic/Jumbo.luthierguitar";
        case GuitarType::Parlor:           return "Acoustic/Parlor.luthierguitar";
        case GuitarType::Classical:        return "Classical/Classical.luthierguitar";
        case GuitarType::Flamenco:         return "Classical/Flamenca Blanca.luthierguitar";
        case GuitarType::TwelveString:     return "Acoustic/12-String Jumbo.luthierguitar";
        case GuitarType::Resonator:        return "Resonator/Resonator Steel.luthierguitar";
        case GuitarType::PrecisionBass:    return "Bass/P-Style Bass.luthierguitar";
        case GuitarType::JazzBass:         return "Bass/J-Style Bass.luthierguitar";
        case GuitarType::Rickenbacker:     return "Bass/Hollow Violin-Style Bass.luthierguitar";
        case GuitarType::FiveStringBass:   return "Bass/Five-String Bass.luthierguitar";
        case GuitarType::FretlessBass:     return "Bass/Fretless Bass.luthierguitar";
        case GuitarType::Custom:
        case GuitarType::NumTypes:
        default:                           return {};
    }
}

juce::File LuthierAudioProcessor::resolveGuitarReference (const juce::String& reference)
{
    if (reference.isEmpty())
        return {};

    // file-formats.md 2 writes "Factory/..." or "User/..."; the origin is a
    // hint, not a rule - a user guitar of the same name wins, as a part does.
    auto relative = reference;

    for (const auto* prefix : { "Factory/", "User/" })
        if (relative.startsWithIgnoreCase (prefix))
            relative = relative.substring ((int) std::strlen (prefix));

    // Guitars renamed by the trademark sweep keep loading under their old names.
    relative = PartLibrary::renamedFactoryGuitar (relative);

    for (const auto& root : { PartLibrary::getUserGuitarsFolder(), PartLibrary::getFactoryGuitarsFolder() })
    {
        const auto file = root.getChildFile (relative);

        if (file.existsAsFile())
            return file;
    }

    // A user guitar saved flat, referenced with its family folder, or the reverse.
    const auto flat = PartLibrary::getUserGuitarsFolder().getChildFile (relative.fromLastOccurrenceOf ("/", false, false));

    if (flat.existsAsFile())
        return flat;

    // ambiguity-resolutions 7: a name from before the parts model, through the
    // migration table.
    const auto migrated = PartLibrary::migratedGuitar (relative);

    if (migrated.isNotEmpty())
    {
        const auto file = PartLibrary::getFactoryGuitarsFolder().getChildFile (migrated);

        if (file.existsAsFile())
            return file;
    }

    return {};
}

void LuthierAudioProcessor::takeGuitarBlock (const juce::var& block)
{
    const auto type = (int) apvts.getRawParameterValue (ParamIDs::guitarType)->load();

    juce::String reference;
    juce::var override;

    if (auto* object = block.getDynamicObject())
    {
        reference = object->getProperty ("reference").toString();

        if (object->getProperty ("override").getDynamicObject() != nullptr)
            override = object->getProperty ("override");
    }

    // A preset saved before the Workshop names its guitar by type only.
    if (reference.isEmpty() && override.isVoid())
    {
        const auto path = getFactoryGuitarPath ((GuitarType) type);
        reference = path.isNotEmpty() ? "Factory/" + path : juce::String();
    }

    guitarReference = reference;
    guitarOverride = override;
    guitarSourceType = type;
    guitarParametersFromState = true;
    pendingGuitarKeep.clear();

    // A factory preset, or a reset: the guitar's own parts win over the layout
    // defaults in the parameter block, and the recipe's own values go back on top.
    guitarPartsWin = false;

    if ((bool) block.getProperty ("partsWin", false))
    {
        guitarPartsWin = true;
        guitarParametersFromState = false;
        loadedGuitarKey.clear();

        if (auto* keep = block.getProperty ("keep", {}).getDynamicObject())
            pendingGuitarKeep = keep->getProperties();
    }

    // Old placements to migrate edit the guitar, even when it is already loaded.
    if (presets.hasLegacyPickupPlacements())
        loadedGuitarKey.clear();

    // The capo travels with the guitar; a preset without one gets the default.
    const auto capoName = block.getProperty ("capo", {}).toString();
    auto capo = capoName.isNotEmpty() ? partLibrary.find (PartType::capo, capoName) : nullptr;
    setCapoPart (capo != nullptr ? capo : partLibrary.getDefault (PartType::capo));

    // The slide travels the same way; a preset without one keeps the default bar.
    const auto slideName = block.getProperty ("slide", {}).toString();
    setSlidePart (slideName.isNotEmpty() ? partLibrary.find (PartType::slide, slideName) : nullptr);
}

juce::var LuthierAudioProcessor::getGuitarBlock() const
{
    auto* block = new juce::DynamicObject();
    block->setProperty ("reference", guitarReference);
    block->setProperty ("override", guitarOverride.isVoid() ? juce::var() : guitarOverride);

    if (capoPart != nullptr)
        block->setProperty ("capo", capoPart->name);

    if (slidePart != nullptr)
        block->setProperty ("slide", slidePart->name);

    return juce::var (block);
}

SlideBar LuthierAudioProcessor::slideBarFor (const Part* part)
{
    SlideBar bar;

    if (part == nullptr)
        return bar;

    // slide-guitar.md 2.1's materials, by the part's `material` field.
    const auto m = part->text ("material", "glass").toLowerCase();
    bar.material = m == "glass_thick" || m == "thick_glass" ? SlideMaterial::glassThick
                 : m == "brass"                           ? SlideMaterial::brass
                 : m == "steel" || m == "chrome"          ? SlideMaterial::steel
                 : m == "ceramic" || m == "porcelain"     ? SlideMaterial::ceramic
                 : m == "bone"                            ? SlideMaterial::bone
                 : m == "dobro_bar" || m == "brass_plated_steel" ? SlideMaterial::dobroBar
                                                          : SlideMaterial::glass;
    bar.massGrams = juce::jlimit (5.0, 500.0, part->number ("mass_g", bar.massGrams));
    bar.lengthMm = juce::jlimit (20.0, 150.0, part->number ("length_mm", bar.lengthMm));
    bar.diameterMm = juce::jlimit (8.0, 40.0, part->number ("diameter_mm", bar.diameterMm));
    return bar;
}

void LuthierAudioProcessor::setSlidePart (const PartPtr& part)
{
    slidePart = part;

    // Only a different bar is a structural change: a state load passes here every time.
    const auto bar = slideBarFor (part.get());
    const auto& now = engine.getSlideEngine().getBar();

    if (bar.material != now.material || bar.massGrams != now.massGrams
        || bar.lengthMm != now.lengthMm || bar.diameterMm != now.diameterMm)
        engine.setSlideBar (bar);
}

void LuthierAudioProcessor::setCapoPart (const PartPtr& capo)
{
    capoPart = capo;

    juce::uint32 mask = TuningEngine::kAllStrings;

    if (capo != nullptr && capo->text ("type", "full") == "partial")
    {
        // Indexed as everywhere in the engine: string 0 is the high E (engine.md 1).
        const auto strings = capo->fields.getProperty ("string_mask", {});
        mask = 0;

        if (auto* flags = strings.getArray())
            for (int s = 0; s < juce::jmin (32, flags->size()); ++s)
                if ((bool) (*flags)[s])
                    mask |= (1u << s);
    }

    engine.getTuningEngine().setCapoStringMask (mask);

    // tuning-stability.md 2.6 (REALISM-C): the capo's pressure, and its gap -
    // trigger 6 mm, screw 4 mm, partial 6 mm; 5 mm with no capo part.
    const auto name = capo != nullptr ? capo->name.toLowerCase() : juce::String();
    const double gap = capo == nullptr ? 5.0 : name.contains ("screw") ? 4.0 : 6.0;
    engine.setCapoHardware (capo != nullptr ? capo->number ("pressure", 0.7) : 0.7, gap);
}

bool LuthierAudioProcessor::loadGuitarForType (GuitarType type)
{
    // The bridge asks on every full apply; only a new source loads anything.
    const bool writeParameters = ! std::exchange (guitarParametersFromState, false);

    if ((int) type != guitarSourceType)
    {
        // The user picked a guitar type: its factory file, as shipped.
        const auto path = getFactoryGuitarPath (type);
        guitarReference = path.isNotEmpty() ? "Factory/" + path : juce::String();
        guitarOverride = juce::var();
        guitarSourceType = (int) type;
    }

    // Which values the guitar load itself replaces: only those get the recipe's
    // value back. Anything else the recipe set was never touched by the load,
    // and may have been changed since by the player or the host.
    std::vector<float> before;

    if (! pendingGuitarKeep.isEmpty())
        for (auto* p : getParameters())
            before.push_back (p->getValue());

    const bool loaded = loadGuitarFrom (guitarReference, guitarOverride, type, writeParameters);
    guitarPartsWin = false;

    /*  No parts guitar for this type (Custom has no factory file; or none is
        installed): the bridge falls back to the compiled guitar, so the previous
        parts guitar is no longer the instrument. Left marked as loaded, the next
        prepareToPlay rebuilt the engine from it - a preset on the Custom type
        played the Strat's parts after a transport restart, and its saved session
        did not (state round trip 0.285 apart, BETA_TEST_REPORT B-17). */
    if (! loaded)
    {
        partsGuitarLoaded = false;
        loadedGuitarKey.clear();
    }

    if (! pendingGuitarKeep.isEmpty())
    {
        const auto keep = std::exchange (pendingGuitarKeep, {});
        const auto& params = getParameters();

        for (int i = 0; i < juce::jmin ((int) before.size(), params.size()); ++i)
        {
            auto* p = dynamic_cast<juce::RangedAudioParameter*> (params[i]);

            if (p == nullptr || p->getValue() == before[(size_t) i] || p->getParameterID() == ParamIDs::guitarType)
                continue;

            if (const auto* value = keep.getVarPointer (p->getParameterID()))
                p->setValueNotifyingHost ((float) juce::jlimit (0.0, 1.0, (double) *value));
        }
    }

    return loaded;
}

bool LuthierAudioProcessor::loadGuitarFrom (const juce::String& reference, const juce::var& override,
                                            GuitarType type, bool writeParameters)
{
    const auto key = override.isVoid() ? reference
                                       : reference + "|" + juce::String (juce::JSON::toString (override, true).hashCode64());

    if (partsGuitarLoaded && key == loadedGuitarKey && (int) engine.getGuitarType() == (int) type)
        return true;

    WorkshopGuitar guitar;
    PartLibrary::LoadReport report;
    bool built = false;

    // 8: the override wins, and needs no files at all.
    if (! override.isVoid())
        built = partLibrary.buildGuitar (override, guitar, report);

    if (! built)
    {
        auto file = resolveGuitarReference (reference);

        // installer.md 8: a pre-parts guitar name resolved through the
        // migration table raises the one info banner.
        if (file.existsAsFile())
        {
            auto relative = reference;

            for (const auto* prefix : { "Factory/", "User/" })
                if (relative.startsWithIgnoreCase (prefix))
                    relative = relative.substring ((int) std::strlen (prefix));

            const auto target = PartLibrary::migratedGuitar (relative);

            if (target.isNotEmpty() && target != relative)
                presets.noteMigration ("guitar " + reference);
        }

        if (! file.existsAsFile())
        {
            const auto path = getFactoryGuitarPath (type);

            if (path.isEmpty())
                return false;

            file = PartLibrary::getFactoryGuitarsFolder().getChildFile (path);

            if (! file.existsAsFile())
            {
                // No factory content installed: the compiled guitar stands in, and the
                // log says why the instrument is not the parts one.
                ErrorLog::write (ErrorLog::Severity::warn, "Workshop", "FACTORY_GUITAR_MISSING",
                                 "Factory guitar " + path + " is not installed; using the built-in instrument.");
                return false;
            }

            if (reference.isNotEmpty())
            {
                // ambiguity-resolutions 7 / gui-integration 15: the preset loads,
                // on its type's factory guitar, and says so in the spec's words.
                const auto name = reference.fromLastOccurrenceOf ("/", false, false)
                                           .upToLastOccurrenceOf (".luthierguitar", false, true);
                report.missing.add ("Guitar '" + (name.isNotEmpty() ? name : reference)
                                    + "' not found, loaded closest factory match. "
                                      "Open Workshop to save your customization as a guitar.");
            }
        }

        if (! partLibrary.loadGuitar (file, guitar, report))
            return false;
    }

    // An old preset's pickup position and height parameters become this
    // guitar's placements (guitar-workshop.md 9's migration).
    const auto legacy = presets.takeLegacyPickupPlacements();
    bool migrated = false;

    for (int engineSlot = 0; engineSlot < (int) legacy.size(); ++engineSlot)
    {
        if (! legacy[(size_t) engineSlot].present)
            continue;

        // Engine slot 0 is the bridge-most fitted pickup.
        int seen = 0;

        for (int fileIndex = 2; fileIndex >= 0; --fileIndex)
        {
            if (guitar.get (WorkshopGuitar::pickupSlot (fileIndex)) == nullptr)
                continue;

            if (seen++ == engineSlot)
            {
                auto& placement = guitar.placements[(size_t) fileIndex];
                const double scale = guitar.get (GuitarSlot::neck) != nullptr
                                       ? guitar.get (GuitarSlot::neck)->number ("scale_length_mm", 648.0) : 648.0;

                placement.positionMm = legacy[(size_t) engineSlot].positionFraction * scale;
                placement.heightTrebleMm = placement.heightBassMm = legacy[(size_t) engineSlot].heightMm;
                migrated = true;
                break;
            }
        }
    }

    // The migrated placements are an edit of the file's guitar; the preset
    // keeps them by carrying the guitar whole.
    if (migrated)
        guitarOverride = guitar.toEmbeddedVar();

    {
        // A guitar-type load keeps values the host wrote with the type; a
        // workshop edit or family switch is the player's and always writes.
        const juce::ScopedValueSetter<bool> keep (guitarLoadKeepsHostWrites, true);
        applyGuitar (guitar, type, report, writeParameters);
    }

    loadedGuitarKey = guitarOverride.isVoid() ? reference
                                              : reference + "|" + juce::String (juce::JSON::toString (guitarOverride, true).hashCode64());
    return true;
}

void LuthierAudioProcessor::applyEditedGuitar (const WorkshopGuitar& guitar)
{
    guitarOverride = guitar.toEmbeddedVar();
    guitarSourceType = (int) engine.getGuitarType();

    applyGuitar (guitar, engine.getGuitarType(), {}, true);

    loadedGuitarKey = guitarReference + "|" + juce::String (juce::JSON::toString (guitarOverride, true).hashCode64());
    presets.markModified();
}

void LuthierAudioProcessor::auditionGuitar (const WorkshopGuitar* candidate)
{
    if (! partsGuitarLoaded)
        return;

    // The engine plays the candidate; nothing else learns of it.
    engine.applyWorkshopGuitar (mapSpec (candidate != nullptr ? *candidate : currentGuitar), engine.getGuitarType());
}

bool LuthierAudioProcessor::switchGuitarFamily (const juce::String& family)
{
    WorkshopGuitar switched;
    juce::String banner;

    if (! partLibrary.switchFamily (currentGuitar, family, switched, banner))
        return false;

    pushUndoBoundary ("Change guitar family to " + family, UndoHistory::kFamilySwitchClass);   // action-and-undo.md 3.4 / 5

    // The guitar now stands for its template's type.
    const auto path = PartLibrary::getFamilyTemplate (family);
    auto type = engine.getGuitarType();

    for (int t = 0; t < (int) GuitarType::NumTypes; ++t)
        if (getFactoryGuitarPath ((GuitarType) t) == path)
            type = (GuitarType) t;

    guitarReference = "Factory/" + path;
    guitarOverride = switched.toEmbeddedVar();
    guitarSourceType = (int) type;

    applyGuitar (switched, type, {}, true);
    loadedGuitarKey = guitarReference + "|" + juce::String (juce::JSON::toString (guitarOverride, true).hashCode64());

    // The type parameter follows, so the bridge sees nothing new to load.
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::guitarType)))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (int) type));

    // guitar-illustration.md 12.3: the amp follows the family when it does not suit it.
    if (const auto amp = FamilyDefaults::applyAmpDefaults (apvts, family); amp.isNotEmpty())
        banner << " " << amp;

    guitarNotices.addIfNotAlreadyThere (banner);
    presets.markModified();
    return true;
}

juce::File LuthierAudioProcessor::saveGuitarAs (const juce::String& name, bool bundleParts)
{
    const auto safeName = juce::File::createLegalFileName (name.trim());

    if (safeName.isEmpty() || ! partsGuitarLoaded)
        return {};

    auto guitar = currentGuitar;
    guitar.name = name.trim();

    const auto folder = PartLibrary::getUserGuitarsFolder();
    folder.createDirectory();

    const auto file = folder.getChildFile (safeName + WorkshopGuitar::kExtension);

    if (! guitar.save (file))
    {
        ErrorLog::write (ErrorLog::Severity::warn, "Workshop", "GUITAR_SAVE_FAILED",
                         "Could not write " + file.getFullPathName());
        return {};
    }

    /*  6: "Bundle parts" writes the referenced parts beside the guitar, in
        their category folders, so the folder is shareable on its own. */
    if (bundleParts)
    {
        const auto bundle = folder.getChildFile (safeName + " parts");

        for (const auto& part : guitar.parts)
            if (part != nullptr)
                part->save (bundle.getChildFile (getPartCategoryFolder (part->type))
                                  .getChildFile (juce::File::createLegalFileName (part->name) + Part::kExtension));
    }

    currentGuitar = guitar;
    guitarReference = "User/" + file.getFileName();
    guitarOverride = juce::var();
    loadedGuitarKey = guitarReference;
    presets.markModified();
    return file;
}

PartPtr LuthierAudioProcessor::savePartAs (GuitarSlot slot, const juce::String& name)
{
    const auto fitted = currentGuitar.get (slot);
    const auto safeName = juce::File::createLegalFileName (name.trim());

    if (fitted == nullptr || safeName.isEmpty())
        return nullptr;

    auto part = std::make_shared<Part> (*fitted);
    part->name = name.trim();
    part->isFactory = false;
    part->isCategoryDefault = false;

    const auto file = PartLibrary::getUserPartsFolder()
                        .getChildFile (getPartCategoryFolder (part->type))
                        .getChildFile (safeName + Part::kExtension);

    if (! part->save (file))
    {
        ErrorLog::write (ErrorLog::Severity::warn, "Workshop", "PART_SAVE_FAILED",
                         "Could not write " + file.getFullPathName());
        return nullptr;
    }

    partLibrary.refresh();

    auto saved = partLibrary.find (part->type, part->name);

    if (saved == nullptr)
        return nullptr;

    // The guitar now names the saved part, so it is no longer an unsaved edit of it.
    auto guitar = currentGuitar;
    guitar.parts[(size_t) slot] = saved;
    applyEditedGuitar (guitar);
    return saved;
}

void LuthierAudioProcessor::applyGuitar (const WorkshopGuitar& guitar, GuitarType standsFor,
                                         const PartLibrary::LoadReport& report, bool writeParameters)
{
    currentGuitar = guitar;
    partsGuitarLoaded = true;

    const auto derived = mapSpec (guitar);

    // TODO 6e / DECISIONS C-09 (MODEL-GAPS): a swap that keeps the structure is
    // built here and taken at the audio thread's next block boundary, with the
    // strings still sounding; anything else parks the engine as before.
    if (! engine.swapPartsAtBlockBoundary (derived, standsFor))
        engine.applyWorkshopGuitar (derived, standsFor);

    // strum-dynamics 4 / bass-techniques 8: the family's strum defaults.
    const bool isBass = guitar.family == "bass";

    if (writeParameters)
    {
        writeGuitarParameters (derived);

        if (isBass != strumFamilyIsBass)
        {
            retargetStrumDefaults (strumFamilyIsBass, isBass);

            // bass-techniques 8 (MODEL-GAPS): the rest of the family's defaults.
            // A value the host wrote with the type stays the host's (clap-validator
            // found setup_style overwritten after a state reload).
            // The family's defaults are the guitar's own values, not the host's:
            // unstamped, like writeGuitarParameters' (or switching back reads
            // them as host writes and keeps them).
            bridge.setStampingWrites (false);
            BassFamilyDefaults::retarget (apvts, strumFamilyIsBass, isBass, [this] (const juce::String& id)
            {
                return guitarLoadKeepsHostWrites && bridge.writtenSinceGuitarType (id);
            });
            bridge.setStampingWrites (true);
        }
    }

    strumFamilyIsBass = isBass;

    // gui-integration 15's missing-part banner, and the log (error-recovery.md).
    for (const auto& message : report.missing)
    {
        guitarNotices.addIfNotAlreadyThere (message);
        ErrorLog::write (ErrorLog::Severity::warn, "Workshop", "PART_MISSING", message);
    }

    for (const auto& message : report.errors)
        ErrorLog::write (ErrorLog::Severity::warn, "Workshop", "GUITAR_UNREADABLE", message);

    if (report.stringExcess > 0)
        ErrorLog::write (ErrorLog::Severity::info, "Workshop", "STRING_COUNT_MISMATCH",
                         "The neck and bridge disagree by " + juce::String (report.stringExcess)
                           + " strings; the guitar has " + juce::String (derived.numStrings) + ".");
}

void LuthierAudioProcessor::retargetStrumDefaults (bool fromBass, bool toBass)
{
    /*  strum-dynamics 4 / bass-techniques 8: a bass strums slower and misses
        less. What is still on the old family's default moves to the new one's;
        what the user set stays. Written without gestures, like the rest of a
        guitar load. */
    auto plain = [this] (const char* id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
        return p != nullptr ? (double) p->convertFrom0to1 (p->getValue()) : 0.0;
    };

    auto write = [this] (const char* id, double v)
    {
        // The host's value, written with the type, stays (as writeGuitarParameters).
        if (guitarLoadKeepsHostWrites && bridge.writtenSinceGuitarType (id))
            return;

        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            bridge.setStampingWrites (false);
            p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
            bridge.setStampingWrites (true);
        }
    };

    auto& rhythm = engine.getRhythmEngine();

    StrumSettings current;
    current.crossingSps     = plain (ParamIDs::strumCrossingSps);
    current.acceleration    = plain (ParamIDs::strumAcceleration);
    current.upVelocityRatio = plain (ParamIDs::strumUpVelocityRatio);
    current.tilt            = plain (ParamIDs::strumTilt);
    current.missProbability = plain (ParamIDs::strumMissProbability);
    current.strikerDown     = (Striker) juce::roundToInt (plain (ParamIDs::strumStrikerDown));
    current.strikerUp       = (Striker) juce::roundToInt (plain (ParamIDs::strumStrikerUp));
    current.chuckAmount     = plain (ParamIDs::chuckAmount);
    current.chuckDamping    = plain (ParamIDs::chuckDamping);
    current.evenness        = rhythm.getStrumEvenness();

    const auto next = StrumSettings::retargetDefaults (current, fromBass, toBass);

    write (ParamIDs::strumCrossingSps,     next.crossingSps);
    write (ParamIDs::strumAcceleration,    next.acceleration);
    write (ParamIDs::strumUpVelocityRatio, next.upVelocityRatio);
    write (ParamIDs::strumTilt,            next.tilt);
    write (ParamIDs::strumMissProbability, next.missProbability);
    rhythm.setStrumEvenness (next.evenness);
}

juce::StringArray LuthierAudioProcessor::takeGuitarNotices()
{
    auto out = guitarNotices;
    guitarNotices.clear();
    return out;
}

void LuthierAudioProcessor::writeGuitarParameters (const DerivedAcoustics& d)
{
    /*  Parts are the instrument; the parameters that overlap them are live
        refinements, initialised from the parts on every guitar load so the
        controls show the guitar that is playing. Written without gestures:
        a guitar load is one action, and its own undo entry (if any) is the
        caller's. Values outside a parameter's range clamp to it. */
    auto write = [this] (const juce::String& id, double plain)
    {
        // A value the host wrote after the guitar type is the host's (a session
        // restoring both, or automation landing together): keep it.
        if (! guitarPartsWin && guitarLoadKeepsHostWrites && bridge.writtenSinceGuitarType (id))
            return;

        // The guitar's own values: not stamped as the host's writes.
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            bridge.setStampingWrites (false);
            p->setValueNotifyingHost (p->convertTo0to1 ((float) plain));
            bridge.setStampingWrites (true);
        }
    };

    write (ParamIDs::stringMaterial, (int) d.stringMaterial);
    write (ParamIDs::fretless, d.spec.fretless ? 1.0 : 0.0);

    /*  The tuning follows the guitar only when it cannot hold the guitar's
        strings: a Drop D survives a change between six-strings, a bass gets a
        bass tuning. Before this a bass type kept Standard and played six
        guitar-tuned strings. A 12-string's tuning names its six courses. */
    {
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::tuningPreset));
        const auto current = (TuningPreset) (choice != nullptr ? choice->getIndex() : 0);
        const int courses = d.numStrings == 12 ? 6 : d.numStrings;

        if (TuningEngine::getPresetStringCount (current) != courses)
            write (ParamIDs::tuningPreset, (int) d.spec.tuning);
    }
    write (ParamIDs::bridgeType, (int) d.spec.bridge);

    write (ParamIDs::bodyTopWood, (int) d.body.topWood);
    write (ParamIDs::bodyBackWood, (int) d.body.backWood);
    write (ParamIDs::bodyBracing, (int) d.body.bracing);
    write (ParamIDs::bodyWidth, d.body.scaleWidth);
    write (ParamIDs::bodyDepth, d.body.scaleDepth);
    write (ParamIDs::bodyAge, d.body.age);

    if (d.body.topThicknessMm > 0.0)
        write (ParamIDs::bodyTopThick, d.body.topThicknessMm);

    for (int slot = 0; slot < juce::jmin (3, d.numPickups); ++slot)
    {
        write (ParamIDs::pickupType (slot), (int) d.pickups[(size_t) slot].spec.type);
        write (ParamIDs::pickupMagnet (slot), (int) d.pickups[(size_t) slot].spec.magnet);
    }

    // The wiring part is the circuit (part-acoustics.md 7).
    write (ParamIDs::circuitVolumePot, d.wiring.volumePot);
    write (ParamIDs::circuitTonePot, d.wiring.tonePot);
    write (ParamIDs::circuitToneCap, d.wiring.toneCap * 1.0e9);
    write (ParamIDs::circuitPotTaper, (int) d.wiring.taper);
    write (ParamIDs::circuitTrebleBleed, (int) d.wiring.bleed);
    write (ParamIDs::circuitActive, d.wiring.active ? 1.0 : 0.0);

    // The setup is the guitar's (fret-buzz.md), measured by its tech.
    write (ParamIDs::setupActionTreble, d.setup.actionTreble);
    write (ParamIDs::setupActionBass, d.setup.actionBass);
    write (ParamIDs::setupRelief, d.setup.relief);
    write (ParamIDs::setupFretHeight, d.setup.fretHeight);

    for (int n = 1; n <= ParamIDs::kNumNutDepths; ++n)
        write (ParamIDs::setupNutDepth (n), d.setup.nutDepth[(size_t) (n - 1)]);
}

//==============================================================================
double LuthierAudioProcessor::claimSampleRateChange() noexcept
{
    const double now = preparedSampleRate.load (std::memory_order_relaxed);

    /*  Not prepared yet. Recording zero here would make the first real rate look
        like a change from nothing, so this leaves the stored value alone. */
    if (now <= 0.0)
        return 0.0;

    const double known = sampleRateKnownToUi.exchange (now);

    // The first rate the window ever sees is the rate it opened at, not a change.
    if (known <= 0.0)
        return 0.0;

    return juce::approximatelyEqual (known, now) ? 0.0 : now;
}

//==============================================================================
BusLayout LuthierAudioProcessor::getNegotiatedLayout() const noexcept
{
    bool anyAux = false, anyPerString = false;

    for (int bus = 1; bus < getBusCount (false); ++bus)
    {
        if (getChannelCountOfBus (false, bus) <= 0)
            continue;

        if ((bus - 1) < kNumAuxBuses || (bus - 1) >= kNumAuxBuses + kNumPerStringBuses)
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

    // performance-budget.md 4: Aux 8 sums the noise generators at the string
    // stage (pre-body), so it carries the per-string taps' latency.
    report.auxNoise = engine.getPerStringLatencySamples();

    routing.setLatencyReport (report);
}

//==============================================================================
void LuthierAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // cpu-quality-modes 2.5 and 4: one level per block, and Luthier's own share
    // of the block's time stamped around all of it.
    const auto qualityStartTicks = juce::Time::getHighResolutionTicks();
    applyQualityForBlock (false);

    struct LoadStamp
    {
        LuthierAudioProcessor& p;
        juce::int64 start;
        int numSamples;

        ~LoadStamp()
        {
            const double busy = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
            p.stampBlockLoad (busy, numSamples);
        }
    } loadStamp { *this, qualityStartTicks, buffer.getNumSamples() };

    const int numSamples = buffer.getNumSamples();
    const int maxSlice = juce::jmax (1, currentBlockSize);

    lastAudioCallbackMs.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
    audioThreadId.store (juce::Thread::getCurrentThreadId(), std::memory_order_relaxed);

    if (numSamples <= maxSlice)
    {
        processSlice (buffer, midiMessages);
        applyDeclick (buffer);
        return;
    }

    // Bigger than prepareToPlay promised: rendered in slices, each slice's MIDI
    // at its own offset, and what each slice leaves in its MIDI buffer (the MIDI
    // out) put back at the slice's place in the host block.
    sliceMidiOut.clear();

    for (int offset = 0; offset < numSamples;)
    {
        const int count = juce::jmin (maxSlice, numSamples - offset);

        // A view onto the host's memory: this constructor does not allocate.
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(),
                                        buffer.getNumChannels(), offset, count);

        sliceMidi.clear();
        sliceMidi.addEvents (midiMessages, offset, count, -offset);

        processSlice (slice, sliceMidi);

        sliceMidiOut.addEvents (sliceMidi, 0, count, offset);
        offset += count;
    }

    midiMessages.swapWith (sliceMidiOut);
    applyDeclick (buffer);
}

//==============================================================================
void LuthierAudioProcessor::applyDeclick (juce::AudioBuffer<float>& buffer) noexcept
{
    const int state = declickState.load (std::memory_order_acquire);

    if (state == declickIdle)
        return;

    const int n = buffer.getNumSamples();
    const int channels = getTotalNumOutputChannels();

    // 5 ms each way: long enough to hide a cut mid-cycle, short enough that a
    // preset switch still feels immediate.
    const float step = 1.0f / (float) juce::jmax (1.0, 0.005 * currentSampleRate);

    if (state == declickSilent)
    {
        for (int ch = 0; ch < juce::jmin (channels, buffer.getNumChannels()); ++ch)
            buffer.clear (ch, 0, n);
        return;
    }

    const bool out = (state == declickFadingOut);
    float g = declickGain;

    for (int i = 0; i < n; ++i)
    {
        g = out ? juce::jmax (0.0f, g - step) : juce::jmin (1.0f, g + step);

        for (int ch = 0; ch < juce::jmin (channels, buffer.getNumChannels()); ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * g);
    }

    declickGain = g;

    if (out && g <= 0.0f)
        declickState.store (declickSilent, std::memory_order_release);
    else if (! out && g >= 1.0f)
        declickState.store (declickIdle, std::memory_order_release);
}

void LuthierAudioProcessor::fadeOutBeforeStructuralChange()
{
    // Nested (a preset load applies structure inside itself): outermost only.
    if (declickDepth++ > 0)
        return;

    // Only worth waiting for when a different thread is rendering right now.
    const bool audioRunning = ! isNonRealtime()
        && juce::Time::getMillisecondCounterHiRes() - lastAudioCallbackMs.load() < 250.0
        && audioThreadId.load() != juce::Thread::getCurrentThreadId();

    if (! audioRunning)
        return;

    declickState.store (declickFadingOut, std::memory_order_release);

    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 60.0;

    while (declickState.load (std::memory_order_acquire) == declickFadingOut
           && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::Thread::sleep (1);
}

void LuthierAudioProcessor::fadeInAfterStructuralChange()
{
    if (--declickDepth > 0)
        return;

    int expected = declickSilent;

    if (! declickState.compare_exchange_strong (expected, declickFadingIn))
    {
        // Timed out mid fade-out (a stalled host): fade in from wherever it got to.
        expected = declickFadingOut;
        declickState.compare_exchange_strong (expected, declickFadingIn);
    }
}

void LuthierAudioProcessor::processSlice (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
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

        // tune-builder 13: the audio input, for a sung melody, while the TUNE tab's Sing is on.
        humCapture.pushAudio (sidechainCopy.getArrayOfReadPointers(), sidechainChannels, sidechainSamples);
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

    // jam-mode 10 (FEAT-JAM): the band's parameters, and the wall clock taps
    // are mapped against.
    const auto jamSettings = bridge.readJam();
    jamBlockWallMs.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);

    if (midiOutConfig.enabled)
        midiOutRouter.captureInput (midiMessages);

    // Host tempo, for tempo-synced delays and tremolo.
    bool hostPlaying = false;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpm = position->getBpm())
                hostTempo.store (*bpm);

            hostPlaying = position->getIsPlaying();
        }
    }

    // live-performance 5: a tapped tempo wins while the host is stopped (or the
    // plugin is off the host's clock). Setting only the host's tempo here
    // overwrote the tap on the very next block.
    blockTempo = tapTempo.getEffectiveBpm (hostTempo.load(), hostPlaying);

    /*  SPEC-SWEEP HI-32 (host-integration 7): with the host's transport stopped
        (the standalone app), an incoming MIDI clock is the tempo. */
    midiClock.process (midiMessages, (double) samplePosition / juce::jmax (1.0, currentSampleRate), currentSampleRate);
    midiClockBpm.store (hostPlaying ? 0.0 : midiClock.getBpm(), std::memory_order_relaxed);

    if (midiClockBpm.load (std::memory_order_relaxed) > 0.0)
        blockTempo = midiClockBpm.load (std::memory_order_relaxed);

    engine.setTempoBpm (blockTempo);

    // The rhythm engine's grid is locked to the host's own position, which is
    // what makes its scheduling sample-accurate rather than merely periodic.
    {
        double ppq = 0.0;
        double hostSeconds = -1.0;   // environment.md 3.4 (REALISM-A)
        bool playing = false, hasPosition = false;

        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                playing = position->getIsPlaying();

                if (auto value = position->getPpqPosition())
                {
                    ppq = *value;
                    hasPosition = true;
                }

                if (auto seconds = position->getTimeInSeconds())
                    hostSeconds = *seconds;

                // SPEC-SWEEP HI-29: the host's metre, for the practice click.
                if (auto signature = position->getTimeSignature())
                {
                    hostTimeSigNumerator = signature->numerator;
                    hostTimeSigDenominator = signature->denominator;
                }
            }
        }

        engine.setTransportPosition (ppq, playing);
        engine.setHostTimeSeconds (hostSeconds, playing && hostSeconds >= 0.0);

        /*  tune-builder 3.6 and 8: the tune plays against the host's clock
            while the host plays and its own otherwise; its own clock then
            drives the rhythm engine too. Record takes the player's notes
            before any of the tune's are merged in. */
        TunePlayer::HostInfo host;
        host.hasPosition = hasPosition;
        host.isPlaying = playing;
        host.ppqPosition = ppq;
        host.bpm = hostTempo.load();

        tuneToEngine.clear();
        tuneToMidiOut.clear();
        tunePlayer.setTempoScale (1.0 + tuneModValue (ParamIDs::tuneTempoDrift) / 100.0);   // tune-builder 14
        tunePlayer.renderBlock (numSamples, host, tuneToEngine, tuneToMidiOut);
        tunePlayer.captureInput (midiMessages);

        const auto& tuneTransport = tunePlayer.getBlockTransport();

        if (tuneTransport.running && ! tuneTransport.followingHost)
        {
            engine.setTempoBpm (tuneTransport.bpm);
            engine.setTransportPosition (tuneTransport.ppq, true);
        }

        // jam-mode 2.3 (FEAT-JAM): while the band runs its own clock (no host,
        // no tune clock), its grid is the rhythm engine's.
        if (! playing && ! (tuneTransport.running && ! tuneTransport.followingHost) && jamSettings.enabled)
        {
            double jamPpq = 0.0, jamBpm = 120.0;

            if (jam.getOwnClock (jamPpq, jamBpm))
            {
                engine.setTempoBpm (jamBpm);
                engine.setTransportPosition (jamPpq, true);
            }
        }

        // 8: a section asking for a state boundary restarts the pattern and
        // the modulation envelopes.
        if (tunePlayer.crossedStateBoundary())
        {
            engine.getRhythmEngine().reset();
            modMatrix.resetEnvelopes();
            tuneStateBoundaries.fetch_add (1, std::memory_order_relaxed);   // observable (TUNE-HELP-ONBOARDING test)
        }
    }

    // SPEC-SWEEP: LP-33 / LP-34 - calibrated pedals are remapped before
    // anything downstream (MIDI Learn, modulation) sees them.
    expressionInput.processMidi (midiMessages);

    // MIDI Learn gets first look, so a CC being learned is not also acted on.
    // SPEC-SWEEP (CT-11): the latency wizard measures the notes as they arrived.
    if (latencyListening.load (std::memory_order_relaxed))
        captureLatencyMeasurements (midiMessages);

    // SPEC-SWEEP (IR-11): calibrated expression pedals first, so everything after
    // - MIDI Learn included - sees the pedal's real travel as a clean 0..127.
    expressionStage.process (midiMessages, controllerScratch);

    midiLearn.processMidi (midiMessages, controllerScratch);   // SPEC-SWEEP IR-3: consumes what it learns

    // live-performance 2: program change and bank select drive the live surface,
    // and are consumed so nothing downstream sees them as musical events.
    handleLiveMidi (midiMessages);

    // SPEC-SWEEP (CT-4 / IR-5): the controller stage - the chosen profile's
    // latency budget moves the block's events earlier (never before the block).
    controllerStage.compensateLatency (midiMessages, numSamples, currentSampleRate, controllerScratch);

    midiCapture.capture (midiMessages, samplePosition);
    logMidiForDiagnostics (midiMessages);

    // SPEC-SWEEP PT-34 (practice-tools 4): the player's own notes - before the
    // audition and preview notes are merged in - answer the drawer's trainers.
    if (practicePanelOpen)
        practiceNoteFeed.pushNoteOns (midiMessages);

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

    /*  tune-builder 8: the chord channel joins the host MIDI, where the rhythm
        engine reads the held chord and strums it; melody, bass and layers go
        to the engine as direct notes, which the rhythm engine's stream does
        not replace. Merged after MIDI Learn, so the tune's controllers can
        never be learned. */
    tuneDirect.clear();

    for (const auto metadata : tuneToEngine)
    {
        const auto message = metadata.getMessage();

        if (message.getChannel() == TuneMidiOptions {}.chordChannel)
            midiMessages.addEvent (message, metadata.samplePosition);
        else
            tuneDirect.addEvent (message, metadata.samplePosition);
    }

    // jam-mode 11 (FEAT-JAM): no double drums or bass. While the Jam drums are
    // heard the tune's percussion layer stays out of the engine; the tune's
    // bass line on a guitar plays through the Jam bass instead of its own line.
    jamTuneBass.clear();
    const bool jamOn = jamSettings.enabled;
    const bool jamTakesTuneBass = jamOn && tunePlayer.isPlaying()   // the band gates it on its own start
                                  && ! tunePlayer.isBassToEngine() && jamTuneHasBass.load (std::memory_order_relaxed)
                                  && ! engine.getRhythmEngine().isBassFamily();
    const bool replacePercussion = jamOn && jam.areDrumsAudible() && tunePlayer.isPlaying();
    jamReplacesPercussion.store (replacePercussion, std::memory_order_relaxed);

    if (replacePercussion && ! tuneDirect.isEmpty())
    {
        const int percussionChannel = TuneMidiOptions {}.layerChannelBase + (int) LayerType::percussion;
        jamScratch.clear();

        for (const auto metadata : tuneDirect)
        {
            const auto message = metadata.getMessage();

            if (! (message.isNoteOn() && message.getChannel() == percussionChannel))
                jamScratch.addEvent (message, metadata.samplePosition);
        }

        tuneDirect.swapWith (jamScratch);
    }

    for (const auto metadata : tuneDirect)   // observable: what percussion reached the engine (JM-42)
        if (metadata.getMessage().isNoteOn()
              && metadata.getMessage().getChannel() == TuneMidiOptions {}.layerChannelBase + (int) LayerType::percussion)
            tunePercussionToEngine.fetch_add (1, std::memory_order_relaxed);

    if (jamTakesTuneBass)
        for (const auto metadata : tuneToMidiOut)
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOnOrOff() && message.getChannel() == TuneMidiOptions {}.bassChannel)
                jamTuneBass.addEvent (message, metadata.samplePosition);
        }

    // What the band listens to (3.1): the block's notes after MIDI Learn, the
    // live consumers and the tune's chord channel, plus the looper's playback.
    jamNotes.clear();

    if (jamOn)
    {
        for (const auto metadata : midiMessages)
            if (metadata.getMessage().isNoteOnOrOff() || metadata.getMessage().isAllNotesOff())
                jamNotes.addEvent (metadata.getMessage(), metadata.samplePosition);

        if (practicePanelOpen)
            looper.renderPlaybackMidi (jamNotes, numSamples);
    }
    // output-normalization.md 4.3: the calibration render's phrase, as written.
    outputNormalization.mergeCalibrationDirect (tuneDirect);

    engine.setDirectMidi (tuneDirect.isEmpty() ? nullptr : &tuneDirect);

    // SPEC-SWEEP (RE-41): the rhythm engine's strokes feed MIDI out's rhythm source.
    engine.setRhythmMidiOut ((midiOutConfig.enabled && midiOutConfig.rhythmEngine)
                                 ? &midiOutRouter.getRhythmBuffer() : nullptr);

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

    // The message thread may be rebuilding engine structure (ParameterBridge::
    // getEngineLock). The audio thread never waits for it: this block is silent.
    const juce::ScopedTryLock engineLock (bridge.getEngineLock());

    if (engineLock.isLocked())
    {
        bridge.applyToEngine();

        // SPEC-SWEEP (CT-7): a newly chosen controller profile, then the UI's
        // queued commands (UW-5), both after the bridge so they have the last word
        // for this block and neither is ever written from the message thread.
        controllerStage.applyPending (engine.getMidiInterpreter());
        engineCommands.drain ([this] (const EngineCommand& c) { applyEngineCommand (c); });
    }

    // The engine only ever writes the main output pair; every other bus belongs
    // to the routing matrix, and a bus nobody writes must be cleared rather than
    // left holding the previous block.
    // notation-export 6.4: the block's place on the host's clock, for the capture.
    {
        CaptureClock clock;
        clock.blockStartSample = samplePosition;
        clock.sampleRate = currentSampleRate;
        clock.bpm = hostTempo.load();

        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                clock.transportPlaying = position->getIsPlaying();

                if (auto ppq = position->getPpqPosition())
                    clock.blockStartPpq = *ppq;

                if (auto signature = position->getTimeSignature())
                {
                    clock.timeSigNumerator = signature->numerator;
                    clock.timeSigDenominator = signature->denominator;
                }
            }
        }

        performanceCapture.beginBlock (clock);
    }

    // output-normalization.md 4.1: the change tracker, and the master bus's timeline.
    outputNormalization.processBlockStart (samplePosition, numSamples, currentSampleRate, isNonRealtime());

    {
        auto mainOut = getBusBuffer (buffer, false, 0);

        // mic-placement.md 9: a user IR in a cab slot has its placement baked
        // in, so that mic's placement stage stands aside.
        for (int slot = 0; slot < 2; ++slot)
            engine.getCabinetEngine().setPlacementBypassed (slot, cabIr[(size_t) slot].isEngaged());

        if (engineLock.isLocked())
            engine.processBlock (mainOut, midiMessages);
        else
            mainOut.clear();
    }

    // ---- jam mode (jam-mode.md, FEAT-JAM) ---------------------------------------------
    // The band renders its stems here; they are mixed after the looper (7).
    // Off, it is not called at all (0.7).
    if (jamOn)
    {
        if (! jamWasEnabled)
            jam.reset();

        JamEngine::BlockContext ctx;
        ctx.latency = reportedLatency;

        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                ctx.hasPlayHead = true;
                ctx.hostPlaying = position->getIsPlaying();

                if (auto value = position->getPpqPosition())      { ctx.hostHasPpq = true; ctx.hostPpq = *value; }
                if (auto value = position->getBpm())              ctx.hostBpm = *value;
                if (auto value = position->getPpqPositionOfLastBarStart()) { ctx.hostHasBarStart = true; ctx.hostBarStartPpq = *value; }

                if (auto signature = position->getTimeSignature())
                {
                    ctx.hostHasMeter = true;
                    ctx.hostNumerator = signature->numerator;
                    ctx.hostDenominator = signature->denominator;
                }
            }
        }

        const auto& tuneTransport = tunePlayer.getBlockTransport();
        ctx.tuneRunning = tuneTransport.running;
        ctx.tuneFollowingHost = tuneTransport.followingHost;
        ctx.tunePlaying = tunePlayer.isPlaying();
        ctx.tunePpq = tuneTransport.ppq;
        ctx.tuneBpm = tuneTransport.bpm;
        ctx.tuneBeatsPerBar = tuneSession.getTune().getBeatsPerBar();
        ctx.effectiveTempo = tapTempo.getEffectiveBpm (hostTempo.load(), hostPlaying);
        ctx.rhythmDriving = engine.getRhythmEngine().isDriving();
        ctx.rhythmChord = engine.getRhythmEngine().getCurrentChord();
        ctx.playerIsBass = engine.getRhythmEngine().isBassFamily();
        ctx.tuneBassActive = jamTakesTuneBass;

        // Count-ins always use the band's sticks (11).
        if (tunePlayer.isCountingIn())
        {
            const auto& clicks = tunePlayer.getBlockClicks();
            jam.addStickClicks (clicks.offsets.data(), clicks.downbeat.data(), clicks.count);
        }

        jam.setMidiChannels (midiOutConfig.jamDrumChannel, midiOutConfig.jamBassChannel);
        jam.setSettings (jamSettings);
        // jam-mode 13: CPU relief's step between 4 and 5 halves the cymbal banks;
        // applied with step 4 so the ladder keeps its numbering. Bass and timing never degrade.
        jam.setReducedCymbals (qualityController.getEffectiveLevel() == QualityLevel::Low);   // jam-mode 4.5 under cpu-quality-modes: Low thins the cymbals
        jam.process (ctx, jamNotes, &jamTuneBass, numSamples);
    }
    else if (jamWasEnabled)
    {
        jam.reset();
        jamReplacesPercussion.store (false, std::memory_order_relaxed);
    }

    jamWasEnabled = jamOn;

    // 6.1: what the engine actually played - string, fret and technique, after
    // voicing - is reported by the engine itself from triggerNote (MODEL-GAPS).

    // piano-roll-chord-display.md 2: what sounds, for the roll and the chord name.
    soundingPublisher.publish (engine, samplePosition, soundingNotes);

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

        // SPEC-SWEEP TM-17: a Cab Match pass starts its capture in the block
        // its test signal starts, so both share sample zero.
        {
            double captureSeconds = 0.0;

            if (cabMatchSignal.takeStart (captureSeconds))
                capture.start (captureSeconds);
        }

        // tone-match 4: the capture takes what the plugin produced, or the
        // reference return on the sidechain. It was never fed, so every
        // tone-match wizard waited at "Recording..." for ever.
        if (capture.isRecording())
        {
            if (capture.getSource() == Capture::Source::sidechain)
            {
                if (hasSidechainInput() && sidechainChannels > 0 && sidechainSamples >= numSamples)
                    capture.processBlock (sidechainCopy.getArrayOfReadPointers(), sidechainChannels, numSamples);
            }
            else
            {
                capture.processBlock (mainOut.getArrayOfReadPointers(), mainOut.getNumChannels(), numSamples);
            }
        }
    }

    // ---- the live surface --------------------------------------------------------
    // The recall crossfade is carried by the audio thread's own clock, so that it
    // takes the same time whatever the host's UI thread happens to be doing; the
    // timer applies it (SnapshotBank::noteAudioTime).
    snapshots.noteAudioTime ((double) numSamples / juce::jmax (1.0, currentSampleRate));

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

        // jam-mode 11 (FEAT-JAM): while the band plays, loops are whole bars
        // and a first recording starts on the next downbeat.
        if (jamOn && jam.isBandRunning())
        {
            double intoBar = 0.0, barQuarters = 4.0, samplesPerQuarter = 24000.0;

            if (jam.getBarPosition (jam.getSampleClock() - numSamples, intoBar, barQuarters, samplesPerQuarter))
            {
                const double barSamples = barQuarters * samplesPerQuarter;
                looper.setBarLengthSamples ((int) std::llround (barSamples));

                const int looperState = (int) looper.getState();

                if (looperState == (int) Looper::State::recordingFirst && jamLooperState != looperState)
                {
                    const auto toBarLine = (int) std::llround ((barQuarters - intoBar) * samplesPerQuarter);
                    looper.setRecordStartDelay (toBarLine >= (int) std::llround (barSamples) - 1 ? 0 : toBarLine);
                }

                jamLooperState = looperState;
            }
        }

        looper.processBlock (mainOut, numSamples);
        looper.captureMidi (midiMessages, numSamples);

        if (metronome.isEnabled())
        {
            metronome.followTempo (blockTempo);   // SPEC-SWEEP PT-6
            metronome.followTimeSignature (hostTimeSigNumerator, hostTimeSigDenominator);   // SPEC-SWEEP HI-29
            metronome.processBlock (clickBuffer.getWritePointer (0), numSamples);

            // 11: the click goes quiet while the band's drums are heard (the
            // visual beat keeps running).
            haveClick = ! (jamOn && jam.areDrumsAudible() && doesJamSilenceMetronome());
        }

        if (backingTrack.isPlaying())
        {
            backingTrack.processBlock (backingBuffer, numSamples);

            for (int channel = 0; channel < juce::jmin (2, mainOut.getNumChannels()); ++channel)
                mainOut.addFrom (channel, 0, backingBuffer, channel, 0, numSamples);
        }

        // jam-mode 7 (FEAT-JAM): the band joins after the looper (loops are the
        // guitar only) and before the session recorder (takes include it).
        if (jamOn)
            mixJam (mainOut, numSamples);

        // practice-tools 8: the session recorder takes what the plugin produced,
        // and the MIDI that played it (MODEL-GAPS: it was never given the MIDI).
        sessionRecorder.captureMidi (midiMessages, numSamples);   // before the block advances its clock
        sessionRecorder.processBlock (mainOut, numSamples);
    }
    else
    {
        if (jamOn)
        {
            auto mainOut = getBusBuffer (buffer, false, 0);
            mixJam (mainOut, numSamples);   // FEAT-JAM
        }

        if (latencyListening.load (std::memory_order_relaxed) && metronome.isEnabled())
        {
            // SPEC-SWEEP (CT-11): the wizard's click, with the drawer shut.
            metronome.processBlock (clickBuffer.getWritePointer (0), numSamples);
            haveClick = true;
        }
    }

    // tune-builder 3.6: the tune's count-in and metronome, on the tune's own
    // grid, in the practice metronome's sound and level.
    {
        const auto& clicks = tunePlayer.getBlockClicks();

        // FEAT-JAM (jam-mode 11): the band's sticks count in; its drums replace the click.
        const bool jamTakesClick = jamOn && (tunePlayer.isCountingIn()
                                             || (jam.areDrumsAudible() && doesJamSilenceMetronome()));

        if ((clicks.count > 0 || tuneClickRinging) && ! jamTakesClick)
        {
            tuneClick.setSound (metronome.getSound());
            tuneClick.setLevelDb (metronome.getLevelDb());

            auto* tuneClicks = tuneClickBuffer.getWritePointer (0);
            tuneClick.renderClicksAt (tuneClicks, numSamples, clicks.offsets.data(),
                                      clicks.downbeat.data(), clicks.count);

            if (haveClick)
                clickBuffer.addFrom (0, 0, tuneClicks, numSamples);
            else
                clickBuffer.copyFrom (0, 0, tuneClicks, numSamples);

            haveClick = true;

            // A click rings past its block; one quiet block ends it.
            tuneClickRinging = juce::FloatVectorOperations::findMaximum (tuneClicks, numSamples) > 1.0e-5f
                            || clicks.count > 0;
        }
    }

    /*  practice-tools 0.2: the click goes to the monitor bus unless the player
        has sent it to the main out - and it goes there anyway when there is no
        monitor bus to hear it on (the standalone app, a stereo-only layout). */
    const bool clickOnMain = haveClick && (clickToMain.load (std::memory_order_relaxed)
                                           || ! RoutingMatrix::layoutHasAux (routing.getActiveLayout()));

    if (clickOnMain)
    {
        auto mainOut = getBusBuffer (buffer, false, 0);

        for (int channel = 0; channel < juce::jmin (2, mainOut.getNumChannels()); ++channel)
            mainOut.addFrom (channel, 0, clickBuffer, 0, 0, numSamples);

        haveClick = false;   // not on the monitor as well
    }

    routing.distribute (*this, buffer, engine.getTapBuffers(), engine.getNumStrings(),
                        engine.getNoiseBusData());

    /*  SPEC-SWEEP TM-17 (tone-match 2): the Cab Match test signal goes out to
        the rig on Aux 1 (DI), in place of what the tap put there - or on the
        main output when the host gave no aux bus (the standalone app). */
    if (cabMatchSignal.isPlaying() && numSamples <= testSignalBuffer.getNumSamples())
    {
        auto* signal = testSignalBuffer.getWritePointer (0);

        if (cabMatchSignal.render (signal, numSamples))
        {
            auto out = getBusCount (false) > 1 && getBus (false, 1) != nullptr && getBus (false, 1)->isEnabled()
                         ? getBusBuffer (buffer, false, 1)
                         : getBusBuffer (buffer, false, 0);

            for (int channel = 0; channel < out.getNumChannels(); ++channel)
                out.copyFrom (channel, 0, signal, numSamples);
        }
    }

    // jam-mode 7 (FEAT-JAM): Aux 9 "Jam Drums" and Aux 10 "Jam Bass".
    if (jamOn && jamSettings.output != (int) JamEngine::Output::main)
    {
        const float* drums[] = { jam.getDrums (0), jam.getDrums (1) };
        const float* bass[] = { jam.getBass (0), jam.getBass (1) };
        routing.writeJamBuses (*this, buffer, drums, bass, numSamples);
    }

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
    static_assert (MidiOutConfig::kNumMacroCcs == ParamIDs::kNumMacros, "one MIDI-out CC per macro");

    for (int m = 0; m < ParamIDs::kNumMacros; ++m)
        if (auto* raw = macroValues[(size_t) m])
            midiOutRouter.setMacroValue (m, raw->load());

    // RE-41, rhythm-engine.md 9: the RHYTHM source was carrying nothing because
    // nobody converted the engine's PlayEvents into MIDI for it.
    {
        auto& rhythmBuffer = midiOutRouter.getRhythmBuffer();
        const auto& rhythmEvents = engine.getRhythmEvents();
        const int lastSample = juce::jmax (0, numSamples - 1);

        for (int i = 0; i < rhythmEvents.getNumNoteOns(); ++i)
        {
            const auto& e = rhythmEvents.getNoteOn (i);
            rhythmBuffer.addEvent (juce::MidiMessage::noteOn (juce::jlimit (1, 16, e.stringIndex + 1),
                                                              juce::jlimit (0, 127, e.midiNote),
                                                              (float) juce::jlimit (0.0, 1.0, e.velocity)),
                                   juce::jlimit (0, lastSample, e.sampleOffset));
        }

        for (int i = 0; i < rhythmEvents.getNumNoteOffs(); ++i)
        {
            const auto& e = rhythmEvents.getNoteOff (i);
            rhythmBuffer.addEvent (juce::MidiMessage::noteOff (juce::jlimit (1, 16, e.stringIndex + 1),
                                                               juce::jlimit (0, 127, e.midiNote)),
                                   juce::jlimit (0, lastSample, e.sampleOffset));
        }
    }

    midiOutRouter.emit (midiMessages, midiOutConfig, engine.getStringActivity(), numSamples);

    // midi-export 10 / tune-builder 8: the tune's parts, when MIDI out carries them.
    if (midiOutConfig.enabled && midiOutConfig.tunePlayback)
        midiMessages.addEvents (tuneToMidiOut, 0, numSamples, 0);

    // jam-mode 9 (FEAT-JAM): the band, drums on GM channel 10, bass on 11.
    if (jamOn && midiOutConfig.enabled && midiOutConfig.jamParts)
        midiMessages.addEvents (jam.getMidiOut(), 0, numSamples, 0);
    sendLuthierSysEx (midiOutConfig, midiMessages, numSamples);

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
        if (auto* raw = macroValues[(size_t) m])
            modMatrix.setMacroValue (m, (double) raw->load());
}

void LuthierAudioProcessor::buildModBlockContext (const juce::AudioBuffer<float>& output,
                                                  int numSamples,
                                                  ModBlockContext& context) noexcept
{
    context.bpm = blockTempo;
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

    // Built here, handed over whole: the audio thread never sees it half-made.
    auto sequence = std::make_shared<const juce::MidiMessageSequence> (AuditionPhrase::build (type, hostTempo.load()));
    std::shared_ptr<const juce::MidiMessageSequence> displaced;

    {
        const juce::SpinLock::ScopedLockType sl (auditionLock);
        displaced = std::move (auditionWaiting);
        auditionWaiting = std::move (sequence);
        auditionHasWaiting = true;
    }

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

    // A new phrase comes in at the block boundary; the old one is retired for
    // the timer to free, so nothing is freed here.
    {
        const juce::SpinLock::ScopedTryLockType sl (auditionLock);

        if (sl.isLocked() && auditionHasWaiting && auditionRetired == nullptr)
        {
            auditionRetired = std::move (auditionSequence);
            auditionSequence = std::move (auditionWaiting);
            auditionHasWaiting = false;
            auditionEventIndex = 0;
            auditionPositionSeconds = 0.0;
            auditionEndSeconds = auditionSequence != nullptr ? auditionSequence->getEndTime() + 0.25 : 0.0;
        }
    }

    if (auditionSequence == nullptr)
        return;

    const auto& phrase = *auditionSequence;

    const double blockSeconds = (double) numSamples / currentSampleRate;
    const double blockStart = auditionPositionSeconds;
    const double blockEnd = blockStart + blockSeconds;

    while (auditionEventIndex < phrase.getNumEvents())
    {
        const auto* event = phrase.getEventPointer (auditionEventIndex);

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
// piano-roll-chord-display.md 3: the keys play through the preview MIDI, the
// same path (and the same try-lock merge) as the fretboard's clicks.
void LuthierAudioProcessor::playKeyboardNote (int midiNote, float velocity)
{
    if (! juce::isPositiveAndBelow (midiNote, 128))
        return;

    const juce::ScopedLock sl (previewLock);
    previewMidi.addEvent (juce::MidiMessage::noteOn (1, midiNote, juce::jlimit (0.05f, 1.0f, velocity)), 0);
}

void LuthierAudioProcessor::releaseKeyboardNote (int midiNote)
{
    if (! juce::isPositiveAndBelow (midiNote, 128))
        return;

    const juce::ScopedLock sl (previewLock);
    previewMidi.addEvent (juce::MidiMessage::noteOff (1, midiNote), 0);
}

void LuthierAudioProcessor::playKeyboardChord (const juce::Array<int>& midiNotes, float velocity)
{
    const juce::ScopedLock sl (previewLock);

    for (int note : midiNotes)
        if (juce::isPositiveAndBelow (note, 128))
            previewMidi.addEvent (juce::MidiMessage::noteOn (1, note, juce::jlimit (0.05f, 1.0f, velocity)), 0);
}

void LuthierAudioProcessor::releaseKeyboardChord (const juce::Array<int>& midiNotes)
{
    const juce::ScopedLock sl (previewLock);

    for (int note : midiNotes)
        if (juce::isPositiveAndBelow (note, 128))
            previewMidi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
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

    // SPEC-SWEEP (IR-14): in "PC only" mode Bank Select is just another CC.
    const bool bankPlusPc = bankSelectsPreset.load (std::memory_order_relaxed);

    auto isLiveControl = [bankPlusPc] (const juce::MidiMessage& m)
    {
        return m.isProgramChange() || (bankPlusPc && m.isController() && m.getControllerNumber() == 0);
    };

    // SPEC-SWEEP: LP-11 - a CC assigned to (or being learned for) a live action.
    auto isLiveAction = [this] (const juce::MidiMessage& m)
    {
        return m.isController() && liveActions.wants (m.getControllerNumber());
    };

    // Most blocks carry neither: nothing to take out, nothing to copy.
    bool any = false;

    for (const auto metadata : midi)
        if (isLiveControl (metadata.getMessage()) || isLiveAction (metadata.getMessage()))
            any = true;

    if (! any)
        return;

    // A member sized in prepareToPlay: a local MidiBuffer allocated on the
    // audio thread on every block that had MIDI.
    auto& kept = liveMidiKept;
    kept.clear();

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
        if (bankPlusPc && message.isController() && message.getControllerNumber() == 0)
        {
            pendingPresetSelect.store (message.getControllerValue(), std::memory_order_relaxed);
            continue;
        }

        // SPEC-SWEEP: LP-11 - consumed, so a footswitch does not also reach the
        // engine as a controller.
        if (message.isController()
            && liveActions.handleController (message.getControllerNumber(), message.getControllerValue()))
            continue;

        kept.addEvent (message, metadata.samplePosition);
    }

    // SPEC-SWEEP (input-routing RT safety): copied back rather than swapped, so
    // the pre-sized buffer stays ours and the host's storage stays the host's.
    midi.clear();

    for (const auto metadata : kept)
        midi.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);

    kept.clear();
}

void LuthierAudioProcessor::applySnapshotModules (const Snapshot& snapshot)
{
    if (snapshot.modMatrix.getDynamicObject() != nullptr)
    {
        modMatrix.setModulationRangeAdvanced (ranges.isFamilyAdvanced (RangeFamily::modulation));   // SPEC-SWEEP: PR-44
        modMatrix.fromVar (snapshot.modMatrix);
    }

    if (snapshot.rhythm.getDynamicObject() != nullptr)
        engine.getRhythmEngine().fromVar (snapshot.rhythm);

    if (snapshot.bypasses.getDynamicObject() != nullptr)
        if (auto* object = snapshot.bypasses.getDynamicObject();
            object != nullptr && object->hasProperty ("character"))
            engine.getCharacterEngine().fromVar (object->getProperty ("character"));

    // jam-mode 12 (FEAT-JAM): the jam block recalls with the snapshot; the band
    // keeps playing (11).
    if (auto* object = snapshot.bypasses.getDynamicObject(); object != nullptr && object->hasProperty ("jam"))
        setJamBlock (object->getProperty ("jam"));

    // tune-builder 14: a snapshot switches the tune to its section (a footswitch
    // can move a live rig between sections). Acted on by the timer.
    if (auto* object = snapshot.bypasses.getDynamicObject(); object != nullptr && object->hasProperty ("tune"))
        requestTuneSnapshotState (object->getProperty ("tune"));
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
        extras->setProperty ("jam", getJamBlock());   // FEAT-JAM (jam-mode 12)
        extras->setProperty ("tune", captureTuneSnapshotState());   // tune-builder 14

        snapshot.bypasses = juce::var (extras);
    }

    return snapshots.setSnapshot (index, snapshot);
}

bool LuthierAudioProcessor::recallSnapshot (int index)
{
    // ambiguity-resolutions 8: a snapshot recall during a preset morph cancels
    // the morph and settles on the recalled state.
    if (presetMorph.isEnabled())
        presetMorph.cancel();

    return snapshots.recall (index);
}

// action-and-undo.md 3.7
bool LuthierAudioProcessor::captureSnapshotAsUserAction (int index, const juce::String& label)
{
    pushUndoAction ("Save snapshot " + juce::String (index + 1) + (label.isNotEmpty() ? " " + label : juce::String()),
                    "snapshot-save", {});
    return captureSnapshot (index, label);
}

bool LuthierAudioProcessor::recallSnapshotAsUserAction (int index)
{
    if (! juce::isPositiveAndBelow (index, snapshots.getNumSnapshots()) || snapshots.getSnapshot (index).isEmpty())
        return false;

    pushUndoAction ("Recall snapshot " + juce::String (index + 1) + " " + snapshots.getSnapshot (index).label,
                    "snapshot-recall", {});
    return recallSnapshot (index);
}

void LuthierAudioProcessor::renameSnapshotAsUserAction (int index, const juce::String& label)
{
    pushUndoAction ("Rename snapshot " + juce::String (index + 1) + " to " + label,
                    "snapshot-rename", juce::String (index));   // grouped: typing
    snapshots.setLabel (index, label);
}

void LuthierAudioProcessor::setSnapshotColourAsUserAction (int index, int colourTag)
{
    pushUndoAction ("Change snapshot " + juce::String (index + 1) + " colour", "snapshot-color", {});
    snapshots.setColourTag (index, colourTag);
}

void LuthierAudioProcessor::deleteSnapshotAsUserAction (int index, bool removeSlot)
{
    pushUndoAction ("Delete snapshot " + juce::String (index + 1), "snapshot-delete", {});

    if (removeSlot)
        snapshots.remove (index);
    else
        snapshots.setSnapshot (index, Snapshot {});
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
    tapTempoAt (juce::Time::getMillisecondCounterHiRes() * 0.001);
}

void LuthierAudioProcessor::tapTempoAt (double now)
{
    // jam-mode 2.1 Tap In (FEAT-JAM): the tap on the band's sample clock.
    {
        const double sinceBlock = juce::jmax (0.0, now * 1000.0 - jamBlockWallMs.load (std::memory_order_relaxed));
        jam.tapAtSample (jam.getSampleClock() + (int64_t) std::llround (juce::jmin (sinceBlock, 1000.0) * 0.001 * currentSampleRate));
    }

    if (tapTempo.tap (now))
    {
        // live-performance 5: a tapped tempo drives the rhythm engine when the
        // host is stopped, so the engine is told about it straight away.
        engine.setTempoBpm (getEffectiveTempo());
    }
}

double LuthierAudioProcessor::getEffectiveTempo() const noexcept
{
    // SPEC-SWEEP HI-32: a running MIDI clock, while the host is stopped.
    if (const double clock = midiClockBpm.load (std::memory_order_relaxed); clock > 0.0)
        return clock;

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

    // SPEC-SWEEP: FF-35/SM-31 - a refused setlist, or one with entries whose
    // preset is gone, says so in a warning banner.
    if (! loaded.loadFrom (file))
    {
        stateWarnings.addIfNotAlreadyThere (loaded.getLoadError());
        return false;
    }

    if (const int missing = loaded.getNumUnresolvedEntries(); missing > 0)
        stateWarnings.addIfNotAlreadyThere (juce::String (missing) + (missing == 1 ? " entry" : " entries")
                                              + " in " + file.getFileNameWithoutExtension()
                                              + " could not be found and " + (missing == 1 ? "is" : "are")
                                              + " marked missing.");

    // action-and-undo.md 3.10 / 5: one boundary entry for the load and the
    // preset load it implies.
    pushUndoBoundary ("Load setlist " + file.getFileNameWithoutExtension());

    setlist.setSetlist (loaded);
    setlistFile = file;

    // output-normalization.md 4.4: a setlist step never waits on a measurement.
    {
        juce::Array<juce::var> presetVars;

        for (int i = 0; i < loaded.getNumEntries(); ++i)
        {
            const juce::File presetFile (loaded.getEntry (i).presetPath);

            if (juce::File::isAbsolutePath (loaded.getEntry (i).presetPath) && presetFile.existsAsFile())
                presetVars.add (juce::JSON::parse (presetFile));
        }

        outputNormalization.prefetchPresets (presetVars);
    }

    return applyCurrentSetlistEntry (false);
}

bool LuthierAudioProcessor::applyCurrentSetlistEntry (bool asUndoStep)
{
    const auto* entry = setlist.getCurrentEntry();

    if (entry == nullptr)
        return false;

    const auto& data = setlist.getCurrentEntryData();

    if (data.getDynamicObject() == nullptr)
        return false;

    // action-and-undo.md 3.10: a setlist step drives a preset-load boundary.
    if (asUndoStep)
        pushUndoBoundary ("Setlist step " + juce::String (setlist.getPosition() + 1) + ": " + entry->getDisplayName());

    if (! presets.fromVar (data))
        return false;

    presets.applyExtraState();   // as setStateInformation and loadPreset do
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

    // SPEC-SWEEP (UW-5): the engine is the audio thread's; the release happens
    // at the top of the next block rather than under its feet. The deferred
    // panic command also stops the riff player (riff-library 5.3).
    postEngineCommand (EngineCommand::make (EngineCommand::Type::panic));

    // jam-mode 2.2 (FEAT-JAM): a 5 ms choke of the band, jam_play off, Armed.
    jam.requestPanic();

    if (auto* play = apvts.getParameter (ParamIDs::jamPlay))
        if (play->getValue() > 0.5f)
            play->setValueNotifyingHost (0.0f);

    const juce::ScopedLock sl (previewLock);
    previewMidi.clear();

    diagnostics.log (LogCategory::Engine, "panic", samplePosition);
}

//==============================================================================
// SPEC-SWEEP (UW-5 / CB-17): the command queue's consumer, on the audio thread.
void LuthierAudioProcessor::applyEngineCommand (const EngineCommand& command) noexcept
{
    const int numStrings = engine.getNumStrings();

    switch (command.type)
    {
        case EngineCommand::Type::panic:
            engine.getRiffPlayer().stop();   // riff-library 5.3: stop the riff on panic (RT-safe: two atomic stores)
            engine.panic();
            break;

        case EngineCommand::Type::stringMute:
            if (juce::isPositiveAndBelow (command.index, numStrings))
                engine.getString (command.index).setDamping (command.value > 0.5f ? StringEngine::Damping::Choked
                                                                                   : StringEngine::Damping::Open, 1.0);
            break;

        case EngineCommand::Type::stringDetune:
        {
            // tuning-stability.md 5: a lower detune is a string brought down to pitch.
            const double before = engine.getStabilityBasePitch (command.index);
            engine.getTuningEngine().setDetuneCents (command.index, (double) command.value);
            engine.getStabilityModel().onTuningChanged (command.index, before,
                                                        engine.getStabilityBasePitch (command.index));
            break;
        }

        case EngineCommand::Type::rhythmReset:
            engine.getRhythmEngine().reset();
            break;

        case EngineCommand::Type::aftertouchTarget:
            engine.getMidiInterpreter().setAftertouchTarget ((MidiTarget) command.index);
            break;

        case EngineCommand::Type::none:
        default:
            break;
    }
}

void LuthierAudioProcessor::setStringMuted (int stringIndex, bool muted)
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    stringMuted[(size_t) stringIndex].store (muted);
    postEngineCommand (EngineCommand::make (EngineCommand::Type::stringMute, stringIndex, muted ? 1.0f : 0.0f));
}

bool LuthierAudioProcessor::isStringMuted (int stringIndex) const noexcept
{
    return juce::isPositiveAndBelow (stringIndex, kMaxStrings) && stringMuted[(size_t) stringIndex].load();
}

void LuthierAudioProcessor::setAftertouchBends (bool shouldBend)
{
    aftertouchBends.store (shouldBend);
    postEngineCommand (EngineCommand::make (EngineCommand::Type::aftertouchTarget,
                                            (int) (shouldBend ? MidiTarget::PitchBend : MidiTarget::VibratoDepth)));
}

void LuthierAudioProcessor::setLatencyWizardListening (bool listening)
{
    latencyListening.store (listening);

    if (! listening)
        latencyFifo.finishedRead (latencyFifo.getNumReady());   // discard; reset() is not thread-safe
}

void LuthierAudioProcessor::captureLatencyMeasurements (const juce::MidiBuffer& midi) noexcept
{
    if (! metronome.isEnabled())
        return;

    // The metronome's phase is where the previous block left it: this block's
    // first sample. A note answers the nearest click - late ones are positive.
    const double bpm = juce::jmax (1.0, metronome.getTempo());
    const double samplesPerBeat = 60.0 / bpm * juce::jmax (1.0, currentSampleRate);
    const double phaseAtStart = metronome.getBeatPhase();

    for (const auto metadata : midi)
    {
        if (! metadata.getMessage().isNoteOn())
            continue;

        double phase = phaseAtStart + (double) metadata.samplePosition / samplesPerBeat;
        phase -= std::floor (phase);

        if (phase > 0.5)
            phase -= 1.0;

        const float ms = (float) (phase * samplesPerBeat / juce::jmax (1.0, currentSampleRate) * 1000.0);

        int start1, size1, start2, size2;
        latencyFifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 + size2 > 0)
        {
            latencyMeasurements[(size_t) (size1 > 0 ? start1 : start2)] = ms;
            latencyFifo.finishedWrite (1);
        }
    }
}

int LuthierAudioProcessor::drainLatencyMeasurements (LatencyWizard& wizard)
{
    int start1, size1, start2, size2;
    latencyFifo.prepareToRead (latencyFifo.getNumReady(), start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i) wizard.addMeasurement ((double) latencyMeasurements[(size_t) (start1 + i)]);
    for (int i = 0; i < size2; ++i) wizard.addMeasurement ((double) latencyMeasurements[(size_t) (start2 + i)]);

    latencyFifo.finishedRead (size1 + size2);
    return size1 + size2;
}

void LuthierAudioProcessor::setStringDetuneCents (int stringIndex, double cents)
{
    postEngineCommand (EngineCommand::make (EngineCommand::Type::stringDetune, stringIndex, (float) cents));
}

void LuthierAudioProcessor::applyControllerProfile (const ControllerProfile& profile)
{
    // The two things the parameter bridge re-applies every block follow the
    // profile as parameters, so the bridge carries the profile's values rather
    // than overwriting them (CT-7). Set before publishing, so the block that
    // applies the profile already reads them.
    const auto rt = ControllerRtSettings::fromProfile (profile);

    if (auto* mpe = apvts.getParameter (ParamIDs::mpeEnabled))
        mpe->setValueNotifyingHost (rt.impliesMpe() ? 1.0f : 0.0f);

    if (auto* bend = apvts.getParameter (ParamIDs::bendRange))
        bend->setValueNotifyingHost (bend->convertTo0to1 ((float) rt.bendSemis));

    controllerStage.setProfile (profile);
}

void LuthierAudioProcessor::resetEverything()
{
    pushUndoState ("Reset everything");

    panic();
    presets.resetToDefaults();
    midiLearn.clearAllMappings();
    lockedParameters.clear();

    // cpu-quality-modes 10: Reset never reads or writes the CPU quality.
    const auto keptQualityOverride = uiState.qualityOverride;
    uiState = UiState {};
    uiState.qualityOverride = keptQualityOverride;

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

    undoHistory.clear();
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

    presets.randomise ((uint64_t) juce::Time::currentTimeMillis(), lockedParameters,
                       randomiseRespectsStock);
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
    auto instance = std::make_unique<LuthierAudioProcessor>();

    /*  The exporter builds, renders and destroys this instance on its own
        thread. Its 30 Hz timer (preset and snapshot recall, morph, capture
        drain) would run on the message thread against the same instance
        while the worker renders it, and stopTimer in its destructor, off the
        message thread, does not wait for a callback already running. An
        offline render needs none of it. */
    instance->stopTimer();
    return instance;
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
    {
        // SPEC-SWEEP (USER_MANUAL UM-8): a slot's blob carries the slotBActive
        // flag it was captured with; recalling it must not flip which slot is
        // showing, or the next A/B press is ignored as "already there".
        const bool showing = slotBActive;
        restoreState (source.getData(), (int) source.getSize(), RestoreScope::soundOnly);   // output-normalization.md 6
        slotBActive = showing;
    }
}

void LuthierAudioProcessor::copyAtoB()
{
    /*  SPEC-SWEEP (USER_MANUAL UM-8): "A>B copies the current one across" - the
        sound on screen goes into both slots. With B showing, this used to copy
        the stored A into B, which the next switch to A then overwrote with B's
        screen state, so the button did nothing from B. */
    slotA = captureStateBlock();
    slotB = slotA;
}

void LuthierAudioProcessor::setSlotBActive (bool b)
{
    if (b == slotBActive)
        return;

    // SPEC-SWEEP: ER-54, error-recovery 9: an empty B says so. The toggle still
    // happens - B starts as the current state and is filled on the way out.
    if (b && slotB.getSize() == 0)
        stateNotices.addIfNotAlreadyThere ("B slot is empty; save current state to B first.");

    // Store what is on screen into the slot being left, then recall the other.
    storeToSlot (slotBActive);
    slotBActive = b;
    recallSlot (slotBActive);
}

//==============================================================================
//==============================================================================
void LuthierAudioProcessor::parameterValueChanged (int, float)
{
    /*  Deliberately empty.

        Every writer reaches a parameter through here - the user's mouse, host
        automation, a learned CC, the modulation matrix - so there is nothing in a
        value change that says who caused it. action-and-undo.md 3.1 only wants the
        user's own edits, so the undo entry is driven by the gesture below
        instead. */
}

void LuthierAudioProcessor::parameterGestureChanged (int parameterIndex, bool gestureIsStarting)
{
    const auto& all = getParameters();

    if (! juce::isPositiveAndBelow (parameterIndex, all.size()))
        return;

    auto* parameter = all[parameterIndex];

    if (parameter == nullptr)
        return;

    // jam-mode 12 (FEAT-JAM): START, STOP and FILL are transport, never undo entries.
    if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
        if (ParamIDs::isJamTransient (withId->paramID))
        {
            gestureParameterIndex = -1;
            return;
        }

    /*  section 11: the host owns its automation lane, and the user reverses a
        host-written change with the host's own undo. A gesture arriving from
        anywhere but the message thread is not the plugin's UI, and
        captureStateBlock is not safe to call off it in any case. */
    if (! juce::MessageManager::existsAndIsCurrentThread() || gestureUndoSuppressed)
    {
        gestureParameterIndex = -1;
        return;
    }

    if (gestureIsStarting)
    {
        // Captured now, while the value is still what it was before the drag.
        gestureStartState = captureStateBlock();
        gestureStartValue = parameter->getValue();
        gestureParameterIndex = parameterIndex;
        gestureParameterName = parameter->getName (64);
        gestureStartMs = undoHistory.now();
        return;
    }

    // An end without a matching start, or a start we could not capture.
    if (gestureParameterIndex != parameterIndex || gestureStartState.getSize() == 0)
    {
        gestureParameterIndex = -1;
        return;
    }

    const float endValue = parameter->getValue();

    gestureParameterIndex = -1;

    /*  A click that selected a knob without moving it is not a change, and
        pushing it would fill the stack with entries that undo to themselves. */
    if (std::abs (endValue - gestureStartValue) < 1.0e-6f)
    {
        gestureStartState.reset();
        return;
    }

    // action-and-undo.md 3.1 / 3.3: "Change X from A to B", "Turn on/off X".
    auto textOf = [parameter] (float v)
    {
        const auto label = parameter->getLabel();
        return parameter->getText (v, 32) + (label.isNotEmpty() ? " " + label : juce::String());
    };

    /*  3.1 / 3.2 / 4: gestures on the same parameter within 200 ms (wheel
        ticks, a scroll through a choice) merge. A toggle never groups (3.3).
        One gesture stays one entry however long it pauses: workshop-ui.md 8's
        "gesture = one entry" rule, applied to every control alike. */
    UndoHistory::Entry entry;
    entry.before = std::move (gestureStartState);
    entry.startMs = gestureStartMs;
    entry.timeMs = undoHistory.now();

    if (parameter->isBoolean())
    {
        entry.description = (endValue >= 0.5f ? "Turn on " : "Turn off ") + gestureParameterName;
    }
    else
    {
        entry.actionClass = "param";
        entry.target = parameter->getName (64);

        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            entry.target = withId->paramID;

        entry.subject = "Change " + gestureParameterName;
        entry.fromText = textOf (gestureStartValue);
        entry.toText = textOf (endValue);
        entry.description = entry.subject + " from " + entry.fromText + " to " + entry.toText;

        // action-and-undo.md 5: choosing a guitar is a guitar load, a boundary.
        if (entry.target == ParamIDs::guitarType)
        {
            entry.boundary = true;
            entry.description = "Load guitar " + entry.toText;
        }
    }

    gestureStartState.reset();

    undoHistory.push (std::move (entry));
}

/*  The stack (Support/UndoHistory) holds one entry per action, each carrying
    the state from before that action. An earlier version kept "the current
    state" as an extra entry and was off by one both ways; Undo.
    stepsOneActionAtATimeBothWays pins the fix. */
void LuthierAudioProcessor::pushUndoState (const juce::String& description)
{
    pushUndoAction (description, {}, {});
}

void LuthierAudioProcessor::pushUndoAction (const juce::String& description,
                                            const juce::String& actionClass, const juce::String& target)
{
    UndoHistory::Entry entry;
    entry.description = description;
    entry.actionClass = actionClass;
    entry.target = target;
    entry.startMs = entry.timeMs = undoHistory.now();

    // A repeat within 200 ms merges and keeps the first before-state, so
    // capturing another would be wasted work on every slider tick.
    if (! undoHistory.wouldMerge (actionClass, target, entry.startMs))
        entry.before = captureStateBlock();

    undoHistory.push (std::move (entry));
}

void LuthierAudioProcessor::pushUndoBoundary (const juce::String& description, const juce::String& actionClass)
{
    UndoHistory::Entry entry;
    entry.actionClass = actionClass;
    entry.before = captureStateBlock();
    entry.description = description;
    entry.boundary = true;
    entry.startMs = entry.timeMs = undoHistory.now();

    undoHistory.push (std::move (entry));
}

void LuthierAudioProcessor::pushUndoCallback (const juce::String& description, const juce::String& actionClass,
                                              const juce::String& target, std::function<void()> undoFn,
                                              std::function<void()> redoFn)
{
    UndoHistory::Entry entry;
    entry.description = description;
    entry.actionClass = actionClass;
    entry.target = target;
    entry.undoAction = std::move (undoFn);
    entry.redoAction = std::move (redoFn);
    entry.startMs = entry.timeMs = undoHistory.now();

    undoHistory.push (std::move (entry));
}

void LuthierAudioProcessor::undoOnce (bool crossBoundary)
{
    auto* entry = undoHistory.stepBack (crossBoundary);

    if (entry == nullptr)
        return;

    if (entry->undoAction != nullptr)
    {
        entry->undoAction();
        return;
    }

    // Where we are now is what redo comes back to.
    entry->after = captureStateBlock();
    applyUndoState (entry->before);

    // action-and-undo.md 8: undoing a family switch says what it cannot keep.
    if (entry->actionClass == UndoHistory::kFamilySwitchClass)
        guitarNotices.addIfNotAlreadyThere (UndoHistory::kFamilySwitchUndoWarning);
}

void LuthierAudioProcessor::undo()               { undoOnce (false); }
void LuthierAudioProcessor::undoAcrossBoundary() { undoOnce (true); }

void LuthierAudioProcessor::undoSteps (int steps)
{
    // Step by step, so every entry gets its redo state and callback entries
    // (outside the blob) run their own undo.
    for (int i = 0; i < steps && undoHistory.canUndoAcrossBoundary(); ++i)
        undoOnce (true);
}

void LuthierAudioProcessor::redo()
{
    if (const auto* entry = undoHistory.stepForward())
    {
        if (entry->redoAction != nullptr)
            entry->redoAction();
        else
            applyUndoState (entry->after);
    }
}

// action-and-undo.md 3.17 / 7: see UndoState::withSessionLayers.
void LuthierAudioProcessor::applyUndoState (const juce::MemoryBlock& state)
{
    juce::Array<juce::var> locks;

    for (const auto& id : lockedParameters)
        locks.add (id);

    juce::NamedValueSet keep;
    keep.set ("liveMode", uiState.liveMode);
    keep.set ("slotBActive", slotBActive);
    keep.set ("lockedParameters", locks);
    keep.set ("clickToMain", isClickToMain());

    const auto restored = UndoState::withSessionLayers (state, keep, { "ui", "tune", "metronome" });

    const juce::ScopedValueSetter<bool> guard (restoringForUndo, true);
    const juce::ScopedValueSetter<bool> keepTune (restoringPluginUndo, true);   // the tune keeps its own undo
    restoreState (restored.getData(), (int) restored.getSize(), RestoreScope::soundOnly);   // output-normalization.md 6: undo keeps the live setting
}

// action-and-undo.md 3.8
bool LuthierAudioProcessor::loadPresetAsUserAction (int index)
{
    if (presets.getPreset (index) == nullptr)
        return false;

    pushUndoBoundary ("Load preset " + presets.getPreset (index)->name);   // action-and-undo.md 5

    if (! presets.loadPreset (index))
        return false;

    bridge.applyAllNow();
    return true;
}

bool LuthierAudioProcessor::stepPresetAsUserAction (bool forward)
{
    const int count = presets.getNumPresets();
    const int current = presets.getCurrentPresetIndex();

    if (count <= 0)
        return false;

    return loadPresetAsUserAction (forward ? (current + 1) % count
                                           : (current <= 0 ? count - 1 : current - 1));
}

juce::String LuthierAudioProcessor::getUndoDescription() const
{
    if (! canUndo())
        return {};

    return undoHistory.peekUndo()->description;
}

juce::String LuthierAudioProcessor::getRedoDescription() const
{
    const auto* entry = undoHistory.peekRedo();
    return entry != nullptr ? entry->description : juce::String();
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
    /*  A program change arriving on the heels of a state restore is the host
        tidying up after itself, not the user asking for a different sound. See
        ignoreNextProgramChange. */
    if (ignoreNextProgramChange.exchange (false))
        return;

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
    // A parameter change the bridge has not built yet (a guitar type, a tuning)
    // is part of what the host is saving: build it first.
    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        // Never prepared (a host may save first): build the instrument now, as
        // prepareToPlay would, so the state is the one a prepared instance saves.
        // (No applyExtraState: with no session restored it holds only the
        // defaults, and would overwrite what the guitar load just set.)
        if (! initialStateApplied)
        {
            bridge.applyAllNow();
            initialStateApplied = true;
        }

        bridge.flushPendingStructuralChange();
    }

    presets.captureExtraState();

    auto* root = new juce::DynamicObject();

    root->setProperty ("preset", presets.toVar (presets.getCurrentPresetName()));

    /*  Merge: the preset block carries MIDI Learn only when there is a mapping
        (a preset without one keeps the controller setup, live-performance 11),
        so the session keeps the exact mappings as well - undo (action-and-undo
        3.12), A/B and the host session restore them, an empty set included.
        restoreState reads this after the preset block, so it wins. */
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

    if (! uiState.riffs.isVoid())
        ui->setProperty ("riffs", uiState.riffs);   // riff-library 8
    // ui-wiring 17 / workshop-ui 7: the bench's A/B slots are workspace - in
    // the plugin state, not in presets, and not restored by undo ("ui" is a
    // session layer there).
    {
        juce::Array<juce::var> bench;

        for (const auto& slot : uiState.benchSlots)
            bench.add (slot);

        ui->setProperty ("benchSlots", bench);
    }

    // piano-roll-chord-display.md 6: the strip's state and the latched keys.
    ui->setProperty ("pianoRollExpanded", uiState.pianoRollExpanded);
    ui->setProperty ("pianoRollHeight", uiState.pianoRollHeight);
    ui->setProperty ("pianoLatch", uiState.pianoLatch);
    ui->setProperty ("pianoShowFingering", uiState.pianoShowFingering);
    {
        juce::Array<juce::var> latched;

        for (int note : uiState.pianoLatchedNotes)
            latched.add (note);

        ui->setProperty ("pianoLatchedNotes", latched);
    }

    ui->setProperty ("practiceDrawerOpen", uiState.practiceDrawerOpen);   // onboarding 11
    ui->setProperty ("qualityOverride", qualityOverrideKey (uiState.qualityOverride));   // cpu-quality-modes 3

    ui->setProperty ("playingGroupCollapsed", uiState.playingGroupCollapsed);   // FEAT-ASSIST
    root->setProperty ("ui", juce::var (ui));

    // ui-wiring 17: the setlist reference, with its entries inline so a missing
    // file or an unsaved edit still restores (and setlist edits can be undone,
    // action-and-undo.md 3.10).
    {
        auto* set = new juce::DynamicObject();
        set->setProperty ("file", setlistFile.getFullPathName());
        set->setProperty ("data", setlist.getSetlist().toVar());
        set->setProperty ("position", setlist.getPosition());
        root->setProperty ("setlist", juce::var (set));
    }

    juce::Array<juce::var> locks;

    for (const auto& id : lockedParameters)
        locks.add (id);

    root->setProperty ("lockedParameters", locks);
    root->setProperty ("slotBActive", slotBActive);
    // SPEC-SWEEP: SM-1/SM-16 - routing, modulation, rhythm, snapshots, MIDI
    // Learn, character and tone-match are inside the "preset" block now
    // (PresetBlocks.cpp); the top-level keys below are only read, for sessions
    // saved before.
    // SPEC-SWEEP (CT-2): the controller profile the player chose.
    if (controllerStage.getProfileId().isNotEmpty())
        root->setProperty ("controllerProfile", controllerStage.getProfileId());

    root->setProperty ("aftertouchBends", doesAftertouchBend());   // SPEC-SWEEP PT-23
    root->setProperty ("bankSelectsPreset", doesBankSelectChoosePreset());   // SPEC-SWEEP IR-14
    root->setProperty ("liveMode", uiState.liveMode);

    // tuning-stability.md 7 (REALISM-C): the strings' wear and the capo
    // compensation are the session's, not the preset's. (REALISM-A's aging and
    // environment ride in the preset's character block: PresetBlocks.cpp.)
    root->setProperty ("stability", engine.getStabilityModel().toVar());

    // practice-tools 1: the metronome's settings are part of the session.
    root->setProperty ("metronome", metronome.toVar());
    root->setProperty ("clickToMain", isClickToMain());

    // output-normalization.md 6: session state, not preset data.
    root->setProperty ("normalization", outputNormalization.toVar());

    // tune-builder 15: the tune being built is part of the session.
    root->setProperty ("tune", tuneSession.toState());

    // ambiguity-resolutions 5.2: the morph slider is a host parameter, so the
    // session keeps it even though a preset (which it morphs between) does not.
    if (auto* morph = apvts.getRawParameterValue (ParamIDs::presetMorphPosition))
        root->setProperty ("presetMorphPosition", (double) morph->load());

    const auto json = juce::JSON::toString (juce::var (root), false);

    destData.reset();
    destData.append (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

// ==== BEGIN REALISM-A state ====
void LuthierAudioProcessor::applyRealismCharacterBlock (const juce::var& characterBlock)
{
    auto* block = characterBlock.getDynamicObject();

    // string-aging.md 8: an absent block is all zeros.
    engine.getStringAging().fromVar (block != nullptr ? block->getProperty ("aging") : juce::var());

    if (block != nullptr && block->hasProperty ("environment"))
        engine.getEnvironment().fromVar (block->getProperty ("environment"));

    if (block == nullptr)
        return;

    auto setPlain = [this] (const char* id, float plain)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    /*  environment.md 6: the old cold / warm choice becomes the temperature
        whose mean steady-state offset across this guitar's strings is the old
        +-2.5 x amount cents, tuned at 22 C, Static. The old offset was
        immediate, so the parts start settled. */
    if (block->hasProperty ("temperature"))
    {
        const auto& character = engine.getCharacterEngine();
        const double target = character.isEnabled()
                                ? CharacterEngine::legacyTemperatureOffsetCents (character.getTemperature(), character.getAmount())
                                : 0.0;
        const double slope = engine.getEnvironment().steadyCentsPerKelvin();

        if (target != 0.0 && slope != 0.0)
        {
            setPlain (ParamIDs::envTemperatureC, (float) (EnvironmentModel::kRoomC + target / slope));
            setPlain (ParamIDs::envTunedAtC, (float) EnvironmentModel::kRoomC);
            setPlain (ParamIDs::envProfile, 0.0f);
            engine.getEnvironment().requestSettle();
        }
    }

    /*  The old humidity multipliers never reached the audio, so every legacy
        humidity is 45 %: mapping dry or humid to 30 or 70 would change how a
        saved preset sounds. */
    if (block->hasProperty ("humidity"))
    {
        if (engine.getCharacterEngine().getHumidity() != Humidity::normal)
            ErrorLog::write (ErrorLog::Severity::info, "Environment", "LEGACY_HUMIDITY",
                             "A legacy humidity setting was loaded as 45 % RH, which is how it sounded");

        setPlain (ParamIDs::envHumidityPct, (float) EnvironmentModel::kReferenceRh);
    }
}
// ==== END REALISM-A state ====

void LuthierAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    restoreState (data, sizeInBytes, RestoreScope::full);
}

void LuthierAudioProcessor::restoreState (const void* data, int sizeInBytes, RestoreScope scope)
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
        uiState.playingGroupCollapsed = (bool) ui->getProperty ("playingGroupCollapsed");   // FEAT-ASSIST
        uiState.easterEggFound = ui->getProperty ("easterEggFound");
        uiState.editorWidth = juce::jmax (900, (int) ui->getProperty ("editorWidth"));
        uiState.editorHeight = juce::jmax (540, (int) ui->getProperty ("editorHeight"));
        uiState.auditionType = (AuditionPhrase::Type) juce::jlimit (
            0, (int) AuditionPhrase::Type::NumTypes - 1, (int) ui->getProperty ("auditionType"));
        auditionType = uiState.auditionType;

        // riff-library 8: the riff browser's view; audition is never restored playing.
        uiState.riffs = ui->getProperty ("riffs");
        engine.getRiffPlayer().stop();
        if (auto* bench = ui->getProperty ("benchSlots").getArray())   // ui-wiring 17
            for (int i = 0; i < juce::jmin (bench->size(), (int) uiState.benchSlots.size()); ++i)
                uiState.benchSlots[(size_t) i] = bench->getReference (i);

        // piano-roll-chord-display.md 6
        uiState.pianoRollExpanded = ui->hasProperty ("pianoRollExpanded") ? (bool) ui->getProperty ("pianoRollExpanded") : true;
        uiState.pianoRollHeight = ui->hasProperty ("pianoRollHeight") ? juce::jlimit (40, 140, (int) ui->getProperty ("pianoRollHeight")) : 72;
        uiState.pianoLatch = ui->getProperty ("pianoLatch");
        uiState.pianoShowFingering = ui->getProperty ("pianoShowFingering");
        uiState.pianoLatchedNotes.clear();

        if (auto* latched = ui->getProperty ("pianoLatchedNotes").getArray())
            for (const auto& n : *latched)
                if (juce::isPositiveAndBelow ((int) n, 128))
                    uiState.pianoLatchedNotes.addIfNotAlreadyThere ((int) n);
        uiState.practiceDrawerOpen = (bool) ui->getProperty ("practiceDrawerOpen");   // onboarding 11

        // cpu-quality-modes 3: the per-instance override (host session only).
        const auto q = ui->getProperty ("qualityOverride").toString();
        setQualityOverride (q == "high" ? QualityOverride::High : q == "medium" ? QualityOverride::Medium
                          : q == "low" ? QualityOverride::Low : q == "auto" ? QualityOverride::Auto
                                                                            : QualityOverride::Global);
    }

    // ui-wiring 17: the setlist. Re-applied only when it differs, because
    // setSetlist reads the entries' preset files.
    if (auto* set = root->getProperty ("setlist").getDynamicObject())
    {
        const auto data = set->getProperty ("data");
        const int position = (int) set->getProperty ("position");

        if (juce::JSON::toString (data, true) != juce::JSON::toString (setlist.getSetlist().toVar(), true)
              || position != setlist.getPosition())
        {
            Setlist restored;
            restored.fromVar (data);
            setlist.setSetlist (restored);

            if (position > 0)
                setlist.goTo (position);
        }

        const auto path = set->getProperty ("file").toString();
        setlistFile = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
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
    {
        modMatrix.setModulationRangeAdvanced (ranges.isFamilyAdvanced (RangeFamily::modulation));   // SPEC-SWEEP: PR-44
        modMatrix.fromVar (root->getProperty ("modulation"));
    }

    // rhythm-engine 9: the pattern, voicing settings and genre kit travel with
    // the preset as one blob.
    if (root->hasProperty ("rhythm"))
        engine.getRhythmEngine().fromVar (root->getProperty ("rhythm"));

    // live-performance 1 and 11: the snapshots and the live-mode preference are
    // both per-preset. A preset saved before snapshots existed simply has none,
    // which the spec treats as one implicit snapshot equal to the preset.
    // (SPEC-SWEEP: a session saved before SM-1 has them here rather than in its
    // preset block, which has already set them to their defaults.)
    if (root->hasProperty ("snapshots"))
        snapshots.fromVar (root->getProperty ("snapshots"));

    uiState.liveMode = (bool) root->getProperty ("liveMode");

    // (SPEC-SWEEP: a session saved before SM-1; the preset block has already
    // applied its own character, aging and environment.)
    if (root->hasProperty ("character"))
    {
        engine.getCharacterEngine().fromVar (root->getProperty ("character"));
        applyRealismCharacterBlock (root->getProperty ("character"));   // REALISM-A
    }
    // tuning-stability.md 7 (REALISM-C): after the preset, whose load reset them.
    if (root->hasProperty ("stability"))
        engine.getStabilityModel().fromVar (root->getProperty ("stability"));

    if (auto* irs = root->getProperty ("toneMatch").getDynamicObject())
    {
        bodyIr.fromVar (irs->getProperty ("body"));
        cabIr[0].fromVar (irs->getProperty ("cab1"));
        cabIr[1].fromVar (irs->getProperty ("cab2"));
    }

    if (root->hasProperty ("metronome"))
        metronome.fromVar (root->getProperty ("metronome"));

    setClickToMain (root->hasProperty ("clickToMain") && (bool) root->getProperty ("clickToMain"));

    // The tune keeps its own undo stack (TUNE-HELP-ONBOARDING, DECISIONS "TUNE in
    // the plugin"): a plugin undo or redo leaves the tune as it is.
    if (root->hasProperty ("tune") && ! restoringPluginUndo)
        tuneSession.restoreState (root->getProperty ("tune"));

    presets.applyExtraState();
    bridge.applyAllNow();
    initialStateApplied = true;

    // SPEC-SWEEP (IR-14)
    setBankSelectChoosesPreset (! root->hasProperty ("bankSelectsPreset") || (bool) root->getProperty ("bankSelectsPreset"));

    // SPEC-SWEEP (PT-23)
    setAftertouchBends (root->hasProperty ("aftertouchBends") && (bool) root->getProperty ("aftertouchBends"));

    // SPEC-SWEEP (CT-2): re-apply the controller profile the session was using.
    if (root->hasProperty ("controllerProfile"))
    {
        const ControllerProfileLibrary library;
        const int index = library.indexOf (root->getProperty ("controllerProfile").toString());

        if (index >= 0)
            applyControllerProfile (library.getProfile (index));
    }

    if (root->hasProperty ("presetMorphPosition"))
        if (auto* morph = apvts.getParameter (ParamIDs::presetMorphPosition))
            morph->setValueNotifyingHost (morph->convertTo0to1 ((float) (double) root->getProperty ("presetMorphPosition")));

    // jam-mode 10 (FEAT-JAM): opening a project never starts the band.
    for (auto* id : { ParamIDs::jamPlay, ParamIDs::jamFillNow })
        if (auto* p = apvts.getParameter (id))
            if (p->getValue() > 0.5f)
                p->setValueNotifyingHost (0.0f);
    // output-normalization.md 6: the host path restores the setting (a state
    // without the key loads off); undo, redo and A/B keep the live one. Either
    // way the sound just changed, which is a configuration event.
    if (scope == RestoreScope::full)
        outputNormalization.restoreFromSession (root->getProperty ("normalization"), root->hasProperty ("normalization"));

    outputNormalization.notifyConfigurationChanged (false);

    // Whatever the host sends next, this state is the one the user saved.
    // (An undo is not a host restore: action-and-undo.md.)
    if (! restoringForUndo)
        ignoreNextProgramChange.store (true);
}

//==============================================================================
void LuthierAudioProcessor::drainPerformanceCapture()
{
    // The instrument the take is on, re-sent when it changes (a guitar, a
    // tuning or a capo change between drains).
    auto& tuning = engine.getTuningEngine();
    const int strings = engine.getNumStrings();
    const auto open = PerformanceCapture::getOpenNotes (tuning, strings);

    if (strings != captureStringCount || open != captureOpenNotes
          || tuning.getCapoFret() != captureCapo || tuning.getCapoStringMask() != captureCapoMask)
    {
        captureStringCount = strings;
        captureOpenNotes = open;
        captureCapo = tuning.getCapoFret();
        captureCapoMask = tuning.getCapoStringMask();
        performanceCapture.setTuning (open, strings, captureCapo, captureCapoMask);
    }

    performanceCapture.drain();
}

void LuthierAudioProcessor::updatePresetMorph()
{
    if (! presetMorph.isEnabled())
        return;

    if (auto* position = apvts.getRawParameterValue (ParamIDs::presetMorphPosition))
        presetMorph.apply ((double) position->load());
}

void LuthierAudioProcessor::updateSnapshotMorph()
{
    // SPEC-SWEEP: LP-16 - the snapshot morph is a parameter, so host
    // automation, MIDI Learn and the modulation matrix (LFO, mod wheel, an
    // expression pedal, the sidechain follower) all drive it. The matrix's
    // offset is read here as the bridge would on the audio thread.
    if (! snapshots.isMorphEnabled())
    {
        lastSnapshotMorph = -1.0f;
        return;
    }

    auto* raw = apvts.getRawParameterValue (ParamIDs::snapshotMorph);

    if (raw == nullptr)
        return;

    float value = raw->load();

    if (auto* parameter = apvts.getParameter (ParamIDs::snapshotMorph))
        value = modMatrix.apply (parameter->getParameterIndex(), value);

    value = juce::jlimit (0.0f, 1.0f, value);

    if (std::abs (value - lastSnapshotMorph) < 1.0e-4f)
        return;

    lastSnapshotMorph = value;
    snapshots.setMorphPosition ((double) value);
}

bool LuthierAudioProcessor::savePracticeStats() const
{
    juce::String error;
    return practiceStats.save (practiceStatsFile, error);
}

PracticeTargets LuthierAudioProcessor::getPracticeTargets() noexcept
{
    PracticeTargets targets;
    targets.metronome       = &metronome;
    targets.looper          = &looper;
    targets.backingTrack    = &backingTrack;
    targets.scaleTrainer    = &scaleTrainer;
    targets.earTrainer      = &earTrainer;
    targets.progression     = &progression;
    targets.sessionRecorder = &sessionRecorder;
    return targets;
}

void LuthierAudioProcessor::serviceTune()
{
    // tune-builder 8: a bass plays the tune's bass line itself; any other
    // instrument leaves it to MIDI out.
    tunePlayer.setBassToEngine (engine.getGuitarSpec().category == GuitarCategory::Bass);
    tuneSession.setFeelOffset (tuneModValue (ParamIDs::tuneFeelMod));   // tune-builder 14
    applyPendingTuneSection();
    tuneSession.service();
}

// SPEC-SWEEP (IR-11 / LP-34): the expression calibration's audio-thread table
// follows the set, and the wizard hears the pedal it is listening to.
void LuthierAudioProcessor::serviceExpressionCalibration()
{
    expressionStage.update (expression);

    const auto stage = expression.getWizardStage();
    const bool listening = stage == ExpressionCalibrationSet::WizardStage::heel
                        || stage == ExpressionCalibrationSet::WizardStage::toe;

    expressionStage.setObservedCc (listening ? expression.getWizardCc() : -1);
    expressionStage.drainObserved (expression);
}

void LuthierAudioProcessor::timerCallback()
{
    // ambiguity-resolutions 5.2: the morph follows its (automatable) slider.
    updatePresetMorph();

    // What the audio thread retired: the audition phrase, the riff player's riffs.
    {
        std::shared_ptr<const juce::MidiMessageSequence> done;

        {
            const juce::SpinLock::ScopedLockType sl (auditionLock);
            done = std::move (auditionRetired);
        }
    }

    engine.getRiffPlayer().collectGarbage();   // riff-library 5.3

    // live-performance 1: an in-flight snapshot recall, on the audio clock.
    snapshots.advancePending();

    // SPEC-SWEEP: LP-11 / LP-16 / LP-33 - live actions, the automatable
    // morph and the pedal wizard.
    liveActions.service();
    updateSnapshotMorph();
    expressionInput.syncWith (expression);
    expressionInput.feedWizard (expression);

    // SPEC-SWEEP: UT-16 - the crash dump writer, once crash reports are on.
    if (! CrashWriter::isInstalled() && telemetry.isCrashUploadEnabled())
    {
        CrashWriter::setInfo ({ juce::String ("Luthier ") + JucePlugin_VersionString,
                                juce::PluginHostType().getHostDescription(),
                                juce::AudioProcessor::getWrapperTypeDescription (wrapperType) });
        CrashWriter::install();
    }

    serviceTune();
    serviceJam();   // FEAT-JAM

    serviceExpressionCalibration();   // SPEC-SWEEP IR-11 / LP-34

    // SPEC-SWEEP (CT-14, controllers 5): a note a string could not reach was
    // clipped into range rather than dropped; the diagnostic log says so.
    if (const auto clipped = engine.getMidiInterpreter().getClippedNoteCount(); clipped != loggedClippedNotes)
    {
        diagnostics.log (LogCategory::Midi, (juce::String ((int) (clipped - loggedClippedNotes))
                                              + " controller note(s) out of their string's range, clipped")
                                              .toRawUTF8(),
                         samplePosition);
        loggedClippedNotes = clipped;
    }

    // SPEC-SWEEP (PT-21): a CC mapped to a macro (controller profiles map CCs to
    // Drive, Tone, Space, Body, Attack) moves that macro parameter here, on the
    // message thread, so the host sees the change like any other edit.
    {
        const std::pair<MidiTarget, const char*> macroTargets[] = {
            { MidiTarget::Drive, ParamIDs::macroDrive }, { MidiTarget::Tone, ParamIDs::macroTone },
            { MidiTarget::Space, ParamIDs::macroSpace }, { MidiTarget::Body, ParamIDs::macroBody },
            { MidiTarget::Attack, ParamIDs::macroAttack } };

        for (const auto& [target, id] : macroTargets)
            if (const float v = engine.getMidiInterpreter().takeMacroTarget (target); v >= 0.0f)
                if (auto* p = apvts.getParameter (id))
                    p->setValueNotifyingHost (v);
    }

    // notation-export 6.2: the capture drains at 10 Hz.
    if (++captureDrainTick >= 3)
    {
        captureDrainTick = 0;
        drainPerformanceCapture();
    }

    // performance-budget.md 0.4: the looper's MIDI FIFO, filled on the audio thread.
    looper.drainPendingMidi();

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
   #if LUTHIER_HEADLESS
    return nullptr;
   #else
    return new LuthierAudioProcessorEditor (*this);
   #endif
}

//==============================================================================
void LuthierAudioProcessor::postWorkshopChange (const juce::String& slotId, const juce::String& fitted,
                                                const juce::String& was)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    workshopFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 + size2 < 1)
        return;   // sixteen part swaps inside one block: the oldest are enough

    auto& change = workshopChanges[(size_t) (size1 > 0 ? start1 : start2)];
    slotId.copyToUTF8 (change.slot, sizeof (change.slot));
    fitted.copyToUTF8 (change.fit, sizeof (change.fit));
    was.copyToUTF8 (change.was, sizeof (change.was));

    workshopFifo.finishedWrite (1);
}

double LuthierAudioProcessor::temperatureCelsius (Temperature t) noexcept
{
    switch (t)
    {
        case Temperature::cold: return 10.0;
        case Temperature::warm: return 32.0;
        case Temperature::room:
        case Temperature::numTemperatures:
        default:                return 21.5;
    }
}

double LuthierAudioProcessor::humidityPercent (Humidity h) noexcept
{
    switch (h)
    {
        case Humidity::dry:   return 25.0;
        case Humidity::humid: return 70.0;
        case Humidity::normal:
        case Humidity::numHumidities:
        default:              return 45.0;
    }
}

void LuthierAudioProcessor::sendCharacterChanges() noexcept
{
    /*  midi-export 2.1: "CHARACTER - seed changes, environment changes
        (temperature, humidity) as they occur". Stated once when the source
        comes on, then on each change, at the block's first sample (they are
        set from the message thread, between blocks). */
    using Field = LuthierSysExOut::Field;
    const auto& character = engine.getCharacterEngine();

    const auto seed = character.getSeed();

    if (! characterStated || seed != sentCharacterSeed)
    {
        char text[24];
        std::snprintf (text, sizeof (text), "%llu", (unsigned long long) seed);
        sysExOut.push (LuthierEventClass::character, 0, { Field::makeWord ("what", "seed"), Field::makeWord ("seed", text) });
        sentCharacterSeed = seed;
    }

    const int temperature = (int) character.getTemperature();
    const int humidity = (int) character.getHumidity();

    if (! characterStated || temperature != sentTemperature || humidity != sentHumidity)
    {
        sysExOut.push (LuthierEventClass::character, 0,
                       { Field::makeWord ("what", "environment"),
                         Field::makeReal ("temp", temperatureCelsius ((Temperature) temperature)),
                         Field::makeReal ("humidity", humidityPercent ((Humidity) humidity)) });
        sentTemperature = temperature;
        sentHumidity = humidity;
    }

    characterStated = true;
}

void LuthierAudioProcessor::sendLuthierSysEx (const MidiOutConfig& config, juce::MidiBuffer& midi,
                                              int numSamples) noexcept
{
    using Field = LuthierSysExOut::Field;
    const bool on = config.enabled;

    // Drained every block whether or not they are sent, so switching the
    // source on never releases a backlog of stale changes.
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        workshopFifo.prepareToRead (workshopFifo.getNumReady(), start1, size1, start2, size2);

        auto send = [&] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                const auto& change = workshopChanges[(size_t) (start + i)];

                if (on && config.workshopChanges)
                    sysExOut.push (LuthierEventClass::workshop, 0,
                                   { Field::makeWord ("slot", change.slot), Field::makeWord ("fit", change.fit),
                                     Field::makeWord ("was", change.was) });
            }
        };

        send (start1, size1);
        send (start2, size2);
        workshopFifo.finishedRead (size1 + size2);
    }

    if (on && config.luthierEvents)
        sendCharacterChanges();   // MODEL-GAPS
    else
        characterStated = false;  // restated when the source comes back on

    if (on && config.luthierEvents)
    {
        const auto& pool = engine.getNoisePool();
        const int last = juce::jmax (0, numSamples - 1);

        for (int i = 0; i < pool.getNumBlockTriggers(); ++i)
        {
            const auto& t = pool.getBlockTrigger (i);
            const int at = juce::jlimit (0, last, t.offset);

            switch (t.noiseClass)
            {
                case NoiseClass::squeak:
                case NoiseClass::pickScrape:
                    sysExOut.push (LuthierEventClass::squeak, at,
                                   { Field::makeWord ("trigger", t.noiseClass == NoiseClass::squeak ? "shift" : "drag"),
                                     Field::makeInt ("str", t.stringIndex),
                                     Field::makeReal ("dur", t.durationMs),
                                     Field::makeReal ("intensity", t.level) });
                    break;

                case NoiseClass::pickClick:
                    sysExOut.push (LuthierEventClass::pick, at, { Field::makeInt ("str", t.stringIndex) });
                    break;

                case NoiseClass::fretBuzz:
                    sysExOut.push (LuthierEventClass::buzz, at,
                                   { Field::makeInt ("str", t.stringIndex),
                                     Field::makeReal ("dur", t.durationMs),
                                     Field::makeReal ("intensity", t.level) });
                    break;

                case NoiseClass::clank:
                    sysExOut.push (LuthierEventClass::clank, at,
                                   { Field::makeWord ("trigger", "land"),
                                     Field::makeInt ("mask", 1 << juce::jlimit (0, 11, t.stringIndex)),
                                     Field::makeReal ("intensity", t.level) });
                    break;

                case NoiseClass::pickChirp:    // the same pluck as its click
                case NoiseClass::numClasses:
                    break;
            }
        }
    }

    if (on)
        sysExOut.appendTo (midi, numSamples);
    else
        sysExOut.clear();
}

//==============================================================================
// cpu-quality-modes: the quality level, offline rendering and E3.

void LuthierAudioProcessor::setNonRealtime (bool isOffline) noexcept
{
    juce::AudioProcessor::setNonRealtime (isOffline);
    qualityController.setNonRealtime (isOffline);
}

void LuthierAudioProcessor::setQualityOverride (QualityOverride o)
{
    uiState.qualityOverride = o;
    qualityController.setOverride (o);
}

void LuthierAudioProcessor::applyQualityForBlock (bool forceHard) noexcept
{
    // 2.6: a change of isNonRealtime() is a hard switch, so a render does not
    // depend on the level live playback was at.
    const bool offline = isNonRealtime();
    bool hard = forceHard;

    if (offline != lastNonRealtime)
    {
        lastNonRealtime = offline;
        qualityController.setNonRealtime (offline);
        hard = true;
    }

    const auto level = qualityController.getEffectiveLevel();

    if ((int) level != lastAppliedQuality || hard)
    {
        // The message thread may be rebuilding engine structure; never wait for
        // it. A block that cannot take the lock applies the level next block.
        const juce::ScopedTryLock structureLock (bridge.getEngineLock());

        if (! structureLock.isLocked())
            return;

        const auto profile = QualityProfile::forLevel (level);
        engine.applyQuality (profile, hard);
        modMatrix.setControlIntervalMultiplier (profile.modIntervalMultiplier, profile.modFastLfoHz);
        lastAppliedQuality = (int) level;
        appliedQuality.store ((int) level, std::memory_order_relaxed);
    }
}

void LuthierAudioProcessor::stampBlockLoad (double busySeconds, int numSamples) noexcept
{
    if (currentSampleRate <= 0.0 || numSamples <= 0)
        return;

    cpuLoad.addBlock (busySeconds, (double) numSamples / currentSampleRate);

    // 7, E3: Luthier is over its whole block budget for 200 ms. Decided here,
    // on the audio thread, so it works while the message thread is blocked.
    samplesSinceStringDrop += numSamples;

    if (! isNonRealtime()
        && QualityController::isGovernorEnabledGlobally()
        && PerformanceSettings::get().isEmergencyStringDrop()
        && cpuLoad.hasShortWindowOnAudioThread()
        && cpuLoad.getShortMeanOnAudioThread() > 1.0
        && samplesSinceStringDrop >= (juce::int64) (0.2 * currentSampleRate))
    {
        samplesSinceStringDrop = 0;

        if (engine.dropLeastRecentString())
            qualityController.noteStringDropped();
    }
}

} // namespace luthier

//==============================================================================
#if ! LUTHIER_HEADLESS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new luthier::LuthierAudioProcessor();
}
#endif
