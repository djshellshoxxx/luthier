#include "OutputNormalization.h"
#include "../PluginProcessor.h"
#include "../DSP/Master/Bs1770Meter.h"
#include "../Accessibility/Localisation.h"

#include <chrono>
#include <thread>

namespace luthier
{

namespace
{
    std::atomic<int> liveInstances { 0 };

    /** 3.3: the parts of a preset var that are identity, not sound. */
    juce::var stripPresetIdentity (const juce::var& presetVar)
    {
        auto copy = presetVar.clone();

        if (auto* o = copy.getDynamicObject())
            for (const char* key : { "parameters", "name", "category", "author", "description",
                                     "tags", "pluginVersion", "midiMap" })
                o->removeProperty (key);

        return copy;
    }

    juce::String signedDb (double db, int decimals)
    {
        const auto rounded = std::abs (db) < 0.5 * std::pow (10.0, -decimals) ? 0.0 : db;
        return (rounded >= 0.0 ? "+" : "-") + juce::String (std::abs (rounded), decimals);
    }
}

//==============================================================================
std::function<OutputNormalization::Defaults()>& OutputNormalization::defaultsProvider()
{
    static std::function<Defaults()> provider;
    return provider;
}

std::atomic<int>& OutputNormalization::offlineWaitCount() noexcept
{
    static std::atomic<int> c { 0 };
    return c;
}

int OutputNormalization::getLiveInstanceCount() noexcept
{
    return liveInstances.load();
}

double OutputNormalization::snapTarget (double lufs) noexcept
{
    double best = kDefaultTarget;

    for (double t : kTargets)
        if (std::abs (t - lufs) < std::abs (best - lufs))
            best = t;

    return best;
}

OutputNormalization::OutputNormalization (LuthierAudioProcessor& p)
    : processor (p)
{
    ++liveInstances;
    tracker = std::make_unique<ConfigChangeTracker> (processor);

    // 6: a new instance starts from the user's last choice. Only for an
    // instance made on the message thread: offline render and export
    // instances are made on workers, and are told their state explicitly.
    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        if (auto& provider = defaultsProvider())
        {
            const auto d = provider();
            targetLufs.store (snapTarget (d.targetLufs));
            getNormalizer().setTargetLufs (targetLufs.load());

            if (d.enabled)
            {
                // The calibrator starts from the timer, not the constructor.
                enabled.store (true);
                getNormalizer().setEnabled (true);
                tracker->setEnabled (true);
                tracker->markConfigDirty (true);
                startTimerHz (10);
            }
        }
    }
}

OutputNormalization::~OutputNormalization()
{
    stopTimer();
    calibrator.reset();
    tracker.reset();
    --liveInstances;
}

LoudnessNormalizer& OutputNormalization::getNormalizer() noexcept
{
    return processor.getEngine().getMasterBus().getNormalizer();
}

juce::AudioProcessor& OutputNormalization::getShapeProcessor() noexcept
{
    return processor;
}

void OutputNormalization::ensureCalibrator()
{
    if (calibrator != nullptr || calibrationMode.load())
        return;

    publishStructural (captureStructural());
    calibrator = std::make_unique<NormalizationCalibrator> (*this);
}

//==============================================================================
void OutputNormalization::setEnabled (bool on, bool fromUser)
{
    if (calibrationMode.load() && on)
        return;

    if (on == enabled.load())
        return;

    enabled.store (on);
    getNormalizer().setEnabled (on);
    tracker->setEnabled (on);

    if (on)
    {
        ensureCalibrator();
        tracker->markConfigDirty (true);   // 2.3.1: switching on
        startTimerHz (10);
    }
    else
    {
        stopTimer();
        morphing = false;
    }

    {
        const std::lock_guard<std::mutex> sl (statusLock);
        status.enabled = on;
    }

    // 6: the host marks the project dirty.
    processor.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));

    if (on && fromUser && onEnabledByUser)
        onEnabledByUser();

    if (onSettingsChanged)
        onSettingsChanged();
}

void OutputNormalization::setTargetLufs (double lufs)
{
    const double snapped = snapTarget (lufs);

    if (snapped == targetLufs.load())
        return;

    targetLufs.store (snapped);
    getNormalizer().setTargetLufs (snapped);

    bool have = false;
    double measured = 0.0;
    std::uint8_t flags = 0;

    {
        const std::lock_guard<std::mutex> sl (statusLock);
        status.targetLufs = snapped;
        have = haveMeasurement;
        measured = lastMeasuredLufs;
        flags = lastFlags;
    }

    // 2.2: a new target glides; it needs no new render.
    if (have)
        publishGainFor (measured, flags);

    processor.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));

    if (onSettingsChanged)
        onSettingsChanged();
}

void OutputNormalization::setCalibrationRenderMode (bool on)
{
    calibrationMode.store (on);

    if (on)
    {
        stopTimer();
        enabled.store (false);
        getNormalizer().setEnabled (false);
        tracker->setEnabled (false);
        calibrator.reset();
    }
}

void OutputNormalization::mergeCalibrationDirect (juce::MidiBuffer& direct) const
{
    if (calibrationDirect != nullptr)
        direct.addEvents (*calibrationDirect, 0, -1, 0);
}

//==============================================================================
void OutputNormalization::publishGainFor (double measuredLufs, std::uint8_t baseFlags)
{
    std::uint8_t flags = (std::uint8_t) (baseFlags & (LoudnessNormalizer::flagUnmeasurable
                                                      | LoudnessNormalizer::flagEstimate
                                                      | LoudnessNormalizer::flagFailed));
    std::int32_t gain = 0;

    if ((flags & (LoudnessNormalizer::flagUnmeasurable | LoudnessNormalizer::flagFailed)) == 0)
        gain = LoudnessNormalizer::gainForMeasurement (targetLufs.load(), measuredLufs, flags);

    {
        const std::lock_guard<std::mutex> sl (statusLock);

        if ((flags & (LoudnessNormalizer::flagUnmeasurable | LoudnessNormalizer::flagFailed)) == 0)
            status.gainDb = gain / 100.0;

        status.flags = flags;
        status.measuredLufs = measuredLufs;
        status.updatedMs = juce::Time::getMillisecondCounter();
    }

    getNormalizer().publishResult (publishSerial.fetch_add (1) + 1, gain, flags);
}

void OutputNormalization::handleMeasurement (std::uint32_t, const NormalizationCalibrator::Measurement& m)
{
    std::uint8_t flags = 0;

    if (m.failed && ! m.estimate)
        flags |= LoudnessNormalizer::flagFailed;

    if (m.estimate)
        flags |= LoudnessNormalizer::flagEstimate;

    if (m.ok && m.unmeasurable)
        flags |= LoudnessNormalizer::flagUnmeasurable;

    {
        const std::lock_guard<std::mutex> sl (statusLock);
        status.source = m.source;
        status.hash = m.hash;

        if (m.ok)
        {
            haveMeasurement = ! m.unmeasurable;
            lastMeasuredLufs = m.measuredLufs;
            lastFlags = flags;
        }
    }

    if (morphing)
        return;   // 3.2: the morph's own gain rules while it is between presets

    publishGainFor (m.measuredLufs, flags);
}

//==============================================================================
void OutputNormalization::processBlockStart (std::int64_t timelineStart, int numSamples, double sampleRate,
                                             bool nonRealtime) noexcept
{
    auto& master = processor.getEngine().getMasterBus();
    master.setBlockTimeline (timelineStart);

    std::int64_t fireSample = 0;

    if (! tracker->process (timelineStart, numSamples, sampleRate, fireSample))
        return;

    lastFiredSerial.store (tracker->getRequestSerial(), std::memory_order_release);
    const bool loadRequest = loadRequestPending.exchange (false, std::memory_order_acq_rel);

    /*  4.6: the one sanctioned wait on the audio thread, and only when the
        host says the render is not realtime. The result then glides from its
        request's exact timeline sample, so an offline render's gain curve
        does not depend on the worker's timing or the block size. */
    if (nonRealtime && calibrator != nullptr)
    {
        const auto serial = tracker->getRequestSerial();
        const auto deadline = std::chrono::steady_clock::now()
                                + std::chrono::milliseconds ((int) (1000.0 * (NormalizationCalibrator::getRenderTimeoutSeconds() + 2.0)));
        offlineWaitCount().fetch_add (1, std::memory_order_relaxed);

        while ((std::int32_t) (calibrator->getHandledSerial() - serial) < 0
                 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for (std::chrono::milliseconds (1));

        master.setResultStartSample (fireSample);

        if (loadRequest)
            master.getNormalizer().snapNextResult();
    }
}

//==============================================================================
juce::var OutputNormalization::captureStructural()
{
    auto& presets = processor.getPresetManager();
    presets.captureExtraState();

    auto* root = new juce::DynamicObject();
    root->setProperty ("preset", stripPresetIdentity (presets.toVar()));
    root->setProperty ("modulation", processor.getModMatrix().toVar());
    root->setProperty ("routing", processor.getRouting().toVar());
    root->setProperty ("character", processor.getEngine().getCharacterEngine().toVar());

    // tone-match 7 IR slots, plus 3.3's size and mtime of each referenced file.
    auto* irs = new juce::DynamicObject();

    auto slotVar = [] (IrSlot& slot)
    {
        auto v = slot.toVar();

        if (auto* o = v.getDynamicObject())
        {
            const auto file = slot.getFile();

            if (file.existsAsFile())
            {
                o->setProperty ("fileSize", (juce::int64) file.getSize());
                o->setProperty ("fileModified", file.getLastModificationTime().toMilliseconds());
            }
        }

        return v;
    };

    irs->setProperty ("body", slotVar (processor.getBodyIrSlot()));
    irs->setProperty ("cab1", slotVar (processor.getCabIrSlot (0)));
    irs->setProperty ("cab2", slotVar (processor.getCabIrSlot (1)));
    root->setProperty ("toneMatch", juce::var (irs));

    return juce::var (root);
}

void OutputNormalization::publishStructural (const juce::var& structural)
{
    const auto fingerprint = juce::JSON::toString (structural, true).hashCode64();

    const std::lock_guard<std::mutex> sl (structuralLock);
    structuralSnapshot = structural;
    structuralFingerprint = fingerprint;
}

NormalizationSoundState OutputNormalization::captureSoundState() const
{
    NormalizationSoundState s;
    const auto& all = processor.getParameters();
    s.values.reserve ((size_t) all.size());

    for (auto* p : all)
        s.values.push_back (p->getValue());

    {
        const std::lock_guard<std::mutex> sl (structuralLock);
        s.structural = structuralSnapshot;
    }

    s.valid = s.structural.isObject();
    s.rateFamily = NormalizationSoundState::rateFamilyFor (processor.getEngine().getSampleRate());
    return s;
}

NormalizationSoundState OutputNormalization::soundStateFromPreset (const juce::var& presetVar) const
{
    NormalizationSoundState s;
    const auto& all = processor.getParameters();
    auto* values = presetVar.getProperty ("parameters", {}).getDynamicObject();

    for (auto* p : all)
    {
        float v = p->getDefaultValue();

        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (values != nullptr && values->hasProperty (withId->paramID))
                v = (float) (double) values->getProperty (withId->paramID);

        s.values.push_back (v);
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("preset", stripPresetIdentity (presetVar));
    s.structural = juce::var (root);
    s.valid = presetVar.isObject();
    s.rateFamily = NormalizationSoundState::rateFamilyFor (processor.getEngine().getSampleRate());
    return s;
}

void OutputNormalization::notifyConfigurationChanged (bool prefetch)
{
    if (! enabled.load() || calibrationMode.load())
        return;

    publishStructural (captureStructural());

    // Offline, a load's request is waited for and its result snaps with the
    // load, cached or not (4.6); the prefetch shortcut is for realtime.
    if (prefetch)
        loadRequestPending.store (true, std::memory_order_release);

    if (prefetch && ! processor.isNonRealtime())
    {
        // The load is applied to the engine now rather than on the bridge's
        // next async pass (as the header's own load path does), so the hash is
        // the loaded sound's: per-string state is read back from the engine.
        if (juce::MessageManager::existsAndIsCurrentThread())
        {
            processor.getParameterBridge().applyAllNow();
            publishStructural (captureStructural());
        }

        // 4.4 prefetch / 2.2: a cached gain rides the load itself.
        const auto state = captureSoundState();
        const auto hash = NormalizationCalibrator::hashSoundState (state, processor);
        NormalizationCalibrator::Measurement m;

        if (NormalizationCalibrator::lookupCached (hash, m) && m.ok && ! m.unmeasurable)
        {
            std::uint8_t flags = 0;
            const auto gain = LoudnessNormalizer::gainForMeasurement (targetLufs.load(), m.measuredLufs, flags);
            getNormalizer().applyLoadGain (gain);

            const std::lock_guard<std::mutex> sl (statusLock);
            haveMeasurement = true;
            lastMeasuredLufs = m.measuredLufs;
            lastFlags = 0;
            status.gainDb = gain / 100.0;
            status.flags = flags;
            status.measuredLufs = m.measuredLufs;
            status.source = m.source;
            status.hash = hash;
            status.updatedMs = juce::Time::getMillisecondCounter();
        }
    }

    tracker->markConfigDirty (true);
}

void OutputNormalization::prefetchPresets (const juce::Array<juce::var>& presetVars)
{
    if (! enabled.load() || calibrator == nullptr)
        return;

    for (const auto& v : presetVars)
    {
        auto state = soundStateFromPreset (v);
        const auto hash = NormalizationCalibrator::hashSoundState (state, processor);
        NormalizationCalibrator::Measurement m;

        if (! NormalizationCalibrator::lookupCached (hash, m))
            calibrator->calibrateAsync (std::move (state), {});
    }
}

//==============================================================================
void OutputNormalization::timerCallback()
{
    if (! enabled.load() || calibrationMode.load())
        return;

    ensureCalibrator();
    updateMorph();

    // 2.3.1: structural events the parameters do not show (a mod route, an
    // IR, a part swap). Four times a second is plenty for a user's edit, and
    // a preset load also arrives through notifyConfigurationChanged. Skipped
    // while a preset morph is between its presets (3.2).
    static constexpr int kPollTicks = 3;

    if (++pollTick < kPollTicks || morphing)
        return;

    pollTick = 0;

    const auto structural = captureStructural();
    const auto fingerprint = juce::JSON::toString (structural, true).hashCode64();
    bool changed = false;

    {
        const std::lock_guard<std::mutex> sl (structuralLock);
        changed = fingerprint != structuralFingerprint;
    }

    if (changed)
    {
        publishStructural (structural);
        tracker->markConfigDirty (true);
    }
}

void OutputNormalization::updateMorph()
{
    auto& morph = processor.getPresetMorph();
    auto* positionParam = processor.getState().getRawParameterValue (ParamIDs::presetMorphPosition);
    const double position = positionParam != nullptr ? (double) positionParam->load() : 0.0;

    const bool between = morph.isEnabled() && morph.hasBothSlots() && position > 0.0 && position < 1.0;

    if (! between)
    {
        if (morphing)
        {
            morphing = false;
            lastMorphGainDb = 1.0e9;
            tracker->markConfigDirty (true);   // settle on the endpoint's own calibration
        }

        return;
    }

    morphing = true;

    for (int side = 0; side < 2; ++side)
    {
        auto state = soundStateFromPreset (morph.getSlotState ((PresetMorph::Slot) side));
        const auto hash = NormalizationCalibrator::hashSoundState (state, processor);

        {
            const std::lock_guard<std::mutex> sl (morphLock);

            if (hash == morphHash[side] && (morphKnown[side] || morphPending[side]))
                continue;

            morphHash[side] = hash;
            morphKnown[side] = false;
            morphPending[side] = false;
        }

        NormalizationCalibrator::Measurement m;

        if (NormalizationCalibrator::lookupCached (hash, m) && m.ok)
        {
            const std::lock_guard<std::mutex> sl (morphLock);
            morphLufs[side] = m.measuredLufs;
            morphKnown[side] = true;
        }
        else if (calibrator != nullptr)
        {
            {
                const std::lock_guard<std::mutex> sl (morphLock);
                morphPending[side] = true;
            }

            calibrator->calibrateAsync (std::move (state), [this, side, hash] (const NormalizationCalibrator::Measurement& r)
            {
                const std::lock_guard<std::mutex> sl (morphLock);

                if (morphHash[side] == hash && (r.ok || r.estimate))
                {
                    morphLufs[side] = r.measuredLufs;
                    morphKnown[side] = true;
                }

                morphPending[side] = false;
            });
        }
    }

    double lufsA = 0.0, lufsB = 0.0;

    {
        const std::lock_guard<std::mutex> sl (morphLock);

        if (! morphKnown[0] || ! morphKnown[1])
            return;   // hold until both endpoints are measured

        lufsA = morphLufs[0];
        lufsB = morphLufs[1];
    }

    // 3.2: lerp in dB between the endpoints' own gains.
    std::uint8_t flagsA = 0, flagsB = 0;
    const double gainA = LoudnessNormalizer::gainForMeasurement (targetLufs.load(), lufsA, flagsA) / 100.0;
    const double gainB = LoudnessNormalizer::gainForMeasurement (targetLufs.load(), lufsB, flagsB) / 100.0;
    const double gain = gainA + (gainB - gainA) * position;
    const auto centi = (std::int32_t) std::lround (gain * 100.0);

    if (std::abs (gain - lastMorphGainDb) < 0.005)
        return;

    lastMorphGainDb = gain;

    {
        const std::lock_guard<std::mutex> sl (statusLock);
        status.gainDb = centi / 100.0;
        status.flags = 0;
        status.updatedMs = juce::Time::getMillisecondCounter();
    }

    getNormalizer().publishResult (publishSerial.fetch_add (1) + 1, centi, 0);
}

//==============================================================================
juce::var OutputNormalization::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("enabled", enabled.load());
    root->setProperty ("targetLufs", targetLufs.load());

    const std::lock_guard<std::mutex> sl (statusLock);

    if (haveMeasurement && status.hash.isNotEmpty())
    {
        auto* cal = new juce::DynamicObject();
        std::uint8_t flags = 0;
        cal->setProperty ("hash", status.hash);
        cal->setProperty ("measuredLufs", lastMeasuredLufs);
        cal->setProperty ("gainCentiDb", (int) LoudnessNormalizer::gainForMeasurement (targetLufs.load(), lastMeasuredLufs, flags));
        cal->setProperty ("flags", (int) (flags | lastFlags));
        cal->setProperty ("revision", NormalizationCalibrator::kCalibrationRevision);
        root->setProperty ("calibration", juce::var (cal));
    }

    {
        const std::lock_guard<std::mutex> ml (morphLock);
        juce::Array<juce::var> morphArray;

        for (int side = 0; side < 2; ++side)
            if (morphKnown[side])
            {
                auto* m = new juce::DynamicObject();
                m->setProperty ("hash", morphHash[side]);
                m->setProperty ("measuredLufs", morphLufs[side]);
                morphArray.add (juce::var (m));
            }

        if (morphArray.size() == 2)
            root->setProperty ("morph", morphArray);
    }

    return juce::var (root);
}

void OutputNormalization::restoreFromSession (const juce::var& state, bool hasKey)
{
    if (calibrationMode.load())
        return;

    if (! hasKey)
    {
        // 6: every project saved before this feature loads off, bit-identical.
        if (enabled.load())
        {
            enabled.store (false);
            getNormalizer().setEnabled (false);
            tracker->setEnabled (false);
            stopTimer();
        }

        return;
    }

    const double target = snapTarget ((double) state.getProperty ("targetLufs", kDefaultTarget));
    targetLufs.store (target);
    getNormalizer().setTargetLufs (target);

    {
        const std::lock_guard<std::mutex> sl (statusLock);
        status.targetLufs = target;
    }

    // Seed the caches from the session so the recorded gain is what this
    // machine uses for that hash, whatever its own caches say (6).
    const auto cal = state.getProperty ("calibration", {});

    if (cal.isObject() && (int) cal.getProperty ("revision", 0) == NormalizationCalibrator::kCalibrationRevision)
    {
        const auto hash = cal.getProperty ("hash", {}).toString();
        const double measured = (double) cal.getProperty ("measuredLufs", -120.0);

        if (hash.length() == 64 && std::isfinite (measured))
        {
            NormalizationCalibrator::Measurement m;
            m.ok = true;
            m.measuredLufs = measured;
            m.unmeasurable = measured <= Bs1770Meter::kAbsoluteGateLufs;
            m.source = NormalizationCalibrator::Source::session;
            NormalizationCalibrator::storeInMemory (hash, m);

            // The first block plays at the stored gain, no glide (ON-18).
            getNormalizer().snapTo ((std::int32_t) (int) cal.getProperty ("gainCentiDb", 0));

            const std::lock_guard<std::mutex> sl (statusLock);
            haveMeasurement = ! m.unmeasurable;
            lastMeasuredLufs = measured;
            lastFlags = 0;
            status.hash = hash;
            status.measuredLufs = measured;
            status.gainDb = (int) cal.getProperty ("gainCentiDb", 0) / 100.0;
            status.source = NormalizationCalibrator::Source::session;
            status.updatedMs = juce::Time::getMillisecondCounter();
        }
    }

    if (auto* morphArray = state.getProperty ("morph", {}).getArray())
    {
        const std::lock_guard<std::mutex> sl (morphLock);

        for (int side = 0; side < juce::jmin (2, morphArray->size()); ++side)
        {
            const auto& m = morphArray->getReference (side);
            morphHash[side] = m.getProperty ("hash", {}).toString();
            morphLufs[side] = (double) m.getProperty ("measuredLufs", -120.0);
            morphKnown[side] = morphHash[side].isNotEmpty();

            NormalizationCalibrator::Measurement seeded;
            seeded.ok = true;
            seeded.measuredLufs = morphLufs[side];
            NormalizationCalibrator::storeInMemory (morphHash[side], seeded);
        }
    }

    const bool on = (bool) state.getProperty ("enabled", false);

    if (on != enabled.load())
    {
        enabled.store (on);
        getNormalizer().setEnabled (on);
        tracker->setEnabled (on);

        if (on)
        {
            ensureCalibrator();
            startTimerHz (10);
        }
        else
        {
            stopTimer();
        }

        const std::lock_guard<std::mutex> sl (statusLock);
        status.enabled = on;
    }

    // Verify the stored calibration in the background (a changed IR, a new
    // revision): a mismatch recalibrates and glides.
    if (on)
    {
        publishStructural (captureStructural());
        tracker->markConfigDirty (true);
    }

    if (onSettingsChanged)
        onSettingsChanged();
}

//==============================================================================
OutputNormalization::Status OutputNormalization::getStatus() const
{
    Status s;

    {
        const std::lock_guard<std::mutex> sl (statusLock);
        s = status;
        s.enabled = enabled.load();
        s.targetLufs = targetLufs.load();

        const bool rendering = calibrator != nullptr && calibrator->isRendering();

        if (! s.enabled)
            s.state = State::off;
        else if (morphing)
            s.state = State::morphing;
        else if ((s.flags & LoudnessNormalizer::flagUnmeasurable) != 0)
            s.state = State::unmeasurable;
        else if ((s.flags & LoudnessNormalizer::flagEstimate) != 0)
            s.state = State::estimate;
        else if (rendering || ! haveMeasurement)
            s.state = State::measuring;
        else if ((s.flags & (LoudnessNormalizer::flagClampedHigh | LoudnessNormalizer::flagClampedLow)) != 0)
            s.state = State::clamped;
        else
            s.state = State::applied;
    }

    auto& master = const_cast<LuthierAudioProcessor&> (processor).getEngine().getMasterBus();
    s.appliedGainDb = master.getNormalizer().getCurrentGainDb();
    s.truePeakReductionDb = master.isNormalizationPathActive() ? master.getGainReductionDb() : 0.0;
    s.requests = tracker->getNumRequests();
    return s;
}

juce::String OutputNormalization::readoutText (const Status& s)
{
    // 5.2, through the locale catalog (options.audio.normalization.readout.*).
    const auto gain = signedDb (s.gainDb, 1);

    switch (s.state)
    {
        case State::off:          return tr ("options.audio.normalization.readout.off");
        case State::measuring:    return tr ("options.audio.normalization.readout.measuring");
        case State::applied:      return tr ("options.audio.normalization.readout.applied",
                                             { { "gain", gain }, { "lufs", juce::String (s.measuredLufs, 1) } });
        case State::clamped:      return tr ((s.flags & LoudnessNormalizer::flagClampedHigh) != 0
                                               ? "options.audio.normalization.readout.clampedQuiet"
                                               : "options.audio.normalization.readout.clampedLoud", { { "gain", gain } });
        case State::estimate:     return tr ("options.audio.normalization.readout.estimate",
                                             { { "gain", signedDb (std::round (s.gainDb), 0) } });
        case State::unmeasurable: return tr ("options.audio.normalization.readout.unmeasurable");
        case State::morphing:     return tr ("options.audio.normalization.readout.morphing", { { "gain", gain } });
    }

    return {};
}

juce::String OutputNormalization::badgeText (const Status& s)
{
    switch (s.state)
    {
        case State::off:          return {};
        case State::measuring:    return "N ...";
        case State::applied:      return "N " + signedDb (s.gainDb, 1);
        case State::morphing:     return "N " + signedDb (s.gainDb, 1);
        case State::clamped:      return "N " + signedDb (s.gainDb, 0) + "!";
        case State::estimate:     return "N ~" + signedDb (std::round (s.gainDb), 0);
        case State::unmeasurable: return "N 0";
    }

    return {};
}

juce::String OutputNormalization::badgeTooltip (const Status& s)
{
    return tr ("badge.normalization.tooltip", { { "gain", signedDb (s.gainDb, 1) },
                                                { "target", juce::String (s.targetLufs, 0) } });
}

juce::String OutputNormalization::accessibleBadgeName (const Status& s)
{
    return tr ("badge.normalization.name", { { "sign", s.gainDb < 0.0 ? "minus" : "plus" },
                                             { "value", juce::String (std::abs (s.gainDb), 1) } });
}

double OutputNormalization::getPreviewGainOffsetDb (double clipTruePeakDbtp) const noexcept
{
    if (! enabled.load())
        return 0.0;

    // 10: previews are rendered at -18 LUFS; the offset brings them to the
    // target, never past -1 dBTP.
    const double offset = targetLufs.load() + 18.0;
    return juce::jmin (offset, -1.0 - clipTruePeakDbtp);
}

} // namespace luthier
