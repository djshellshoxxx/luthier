#include "NormalizationCalibrator.h"
#include "OutputNormalization.h"
#include "NormalizationPhrase.h"
#include "ErrorLog.h"
#include "IrLibrary.h"
#include "../DSP/Master/Bs1770Meter.h"
#include "../DSP/Master/LoudnessRoles.h"
#include "../PluginProcessor.h"

#include <juce_cryptography/juce_cryptography.h>

#include <chrono>
#include <random>
#include <thread>

namespace luthier
{

//==============================================================================
namespace
{
    //  3.3: canonical JSON - sorted keys, numbers as %.9g - so the same state
    //  always hashes the same whatever order its objects were built in.
    void writeCanonical (const juce::var& v, juce::String& out)
    {
        if (auto* object = v.getDynamicObject())
        {
            juce::StringArray keys;

            for (const auto& p : object->getProperties())
                keys.add (p.name.toString());

            keys.sort (false);
            out << "{";

            for (int i = 0; i < keys.size(); ++i)
            {
                if (i > 0)
                    out << ",";

                out << juce::JSON::toString (juce::var (keys[i]), true) << ":";
                writeCanonical (object->getProperty (juce::Identifier (keys[i])), out);
            }

            out << "}";
        }
        else if (auto* array = v.getArray())
        {
            out << "[";

            for (int i = 0; i < array->size(); ++i)
            {
                if (i > 0)
                    out << ",";

                writeCanonical (array->getReference (i), out);
            }

            out << "]";
        }
        else if (v.isBool())
        {
            out << ((bool) v ? "true" : "false");
        }
        else if (v.isInt() || v.isInt64())
        {
            out << juce::String ((juce::int64) v);
        }
        else if (v.isDouble())
        {
            char buffer[64];
            std::snprintf (buffer, sizeof (buffer), "%.9g", (double) v);
            out << buffer;
        }
        else if (v.isString())
        {
            out << juce::JSON::toString (v, true);
        }
        else
        {
            out << "null";
        }
    }

    bool isDiscreteParameter (const juce::AudioProcessorParameter& p)
    {
        return p.isDiscrete() || p.isBoolean();
    }

    /** 3.3: a Config value quantised to 1/1024 of its normalised range;
        choices and bools exact. */
    float quantise (const juce::AudioProcessorParameter& p, float v)
    {
        if (isDiscreteParameter (p))
            return v;

        return (float) (std::round ((double) v * 1024.0) / 1024.0);
    }

    juce::String idOf (const juce::AudioProcessorParameter* p)
    {
        if (auto* withId = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p))
            return withId->paramID;

        return {};
    }

    /** The value a parameter takes in the reference render (4.3 setup). */
    float renderValue (const juce::AudioProcessorParameter& p, const juce::String& id, float live)
    {
        switch (LoudnessRoles::roleFor (id))
        {
            case LoudnessRole::performance:
                return p.getDefaultValue();

            case LoudnessRole::mix:
                if (id == "master_gain")
                    if (auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (&p))
                        return ranged->convertTo0to1 (0.0f);

                if (id == "limiter_on")
                    return 0.0f;

                return p.getDefaultValue();

            case LoudnessRole::config:
            default:
                return quantise (p, live);
        }
    }

    //==========================================================================
    // 4.4 caches, shared by every instance in the process.

    struct Caches
    {
        std::mutex lock;

        std::list<std::pair<juce::String, NormalizationCalibrator::Measurement>> lru;
        std::map<juce::String, decltype (lru)::iterator> lruIndex;

        struct FactoryEntry { double lufs; int guitarType; int ampModel; double drive; juce::String preset; };
        std::map<juce::String, FactoryEntry> factory;
        bool factoryLoaded = false;
        juce::File factoryFileOverride;

        juce::File diskOverride;
    };

    Caches& caches()
    {
        static Caches c;
        return c;
    }

    void loadFactoryLocked (Caches& c)
    {
        if (c.factoryLoaded)
            return;

        c.factoryLoaded = true;
        c.factory.clear();

        const auto file = c.factoryFileOverride != juce::File()
                            ? c.factoryFileOverride
                            : IrLibrary::getResourcesFolder().getChildFile ("NormalizationFactory.json");

        if (! file.existsAsFile())
            return;

        const auto json = juce::JSON::parse (file);

        if ((int) json.getProperty ("revision", 0) != NormalizationCalibrator::kCalibrationRevision)
            return;

        if (auto* entries = json.getProperty ("entries", {}).getDynamicObject())
            for (const auto& p : entries->getProperties())
                c.factory[p.name.toString()] = { (double) p.value.getProperty ("lufs", -120.0),
                                                 (int) p.value.getProperty ("guitarType", -1),
                                                 (int) p.value.getProperty ("ampModel", -1),
                                                 (double) p.value.getProperty ("drive", 0.0),
                                                 p.value.getProperty ("preset", {}).toString() };
    }

    NormalizationCalibrator::Measurement fromLufs (double lufs)
    {
        NormalizationCalibrator::Measurement m;
        m.ok = true;
        // Rounded, so a value that went through a JSON cache is the same
        // double as the one that did not (4.6: the gain is a pure function).
        m.measuredLufs = std::round (lufs * 1.0e4) / 1.0e4;
        m.unmeasurable = lufs <= Bs1770Meter::kAbsoluteGateLufs;
        return m;
    }

    /** A fixed transport for the reference render: 100 BPM, stopped. */
    class FixedPlayHead final : public juce::AudioPlayHead
    {
    public:
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (100.0);
            info.setIsPlaying (false);
            info.setPpqPosition (0.0);
            info.setTimeInSamples (0);
            return info;
        }
    };
}

//==============================================================================
const char* NormalizationCalibrator::sourceName (Source s) noexcept
{
    switch (s)
    {
        case Source::factory:  return "factory";
        case Source::memory:   return "memory";
        case Source::disk:     return "disk";
        case Source::render:   return "render";
        case Source::estimate: return "estimate";
        case Source::session:  return "session";
        case Source::none:
        default: break;
    }

    return "none";
}

std::atomic<int>& NormalizationCalibrator::renderCount() noexcept
{
    static std::atomic<int> c { 0 };
    return c;
}

std::atomic<bool>& NormalizationCalibrator::failRendersForTesting() noexcept
{
    static std::atomic<bool> f { false };
    return f;
}

std::atomic<int>& NormalizationCalibrator::maxInjectedDelayMsForTesting() noexcept
{
    static std::atomic<int> d { 0 };
    return d;
}

std::atomic<double>& NormalizationCalibrator::renderTimeoutOverrideForTesting() noexcept
{
    static std::atomic<double> t { 0.0 };
    return t;
}

double NormalizationCalibrator::getRenderTimeoutSeconds() noexcept
{
    const double o = renderTimeoutOverrideForTesting().load();
    return o > 0.0 ? o : kRenderTimeoutSeconds;
}

juce::String NormalizationCalibrator::getEditionName()
{
    // editions.md has no Edition.h in this build yet: one edition, named here
    // so a second one (Free) gets its own hashes and factory table (11).
   #ifdef LUTHIER_EDITION_NAME
    return LUTHIER_EDITION_NAME;
   #else
    return "standard";
   #endif
}

//==============================================================================
NormalizationCalibrator::NormalizationCalibrator (OutputNormalization& o)
    : juce::Thread ("Luthier Normalization"), owner (o)
{
    startThread (juce::Thread::Priority::low);
}

NormalizationCalibrator::~NormalizationCalibrator()
{
    signalThreadShouldExit();
    cancelRender.store (true);
    stopThread (15000);
}

void NormalizationCalibrator::calibrateAsync (NormalizationSoundState state, std::function<void (const Measurement&)> done)
{
    const std::lock_guard<std::mutex> sl (jobLock);
    jobs.emplace_back (std::move (state), std::move (done));
}

void NormalizationCalibrator::run()
{
    std::uint32_t lastRequest = 0;

    while (! threadShouldExit())
    {
        wait (10);

        const auto serial = owner.getTracker().getRequestSerial();

        if (serial != lastRequest)
        {
            lastRequest = serial;
            handleLiveRequest (serial);
            continue;   // a live request always goes before background jobs
        }

        std::pair<NormalizationSoundState, std::function<void (const Measurement&)>> job;

        {
            const std::lock_guard<std::mutex> sl (jobLock);

            if (jobs.empty())
                continue;

            job = std::move (jobs.front());
            jobs.erase (jobs.begin());
        }

        const auto live = owner.getTracker().getRequestSerial();
        const auto m = measureState (job.first, [this, live]
        {
            // A background job yields to a live request (the calibration lane
            // is top priority, 4.3).
            return threadShouldExit() || owner.getTracker().getRequestSerial() != live;
        });

        if (m.failed && owner.getTracker().getRequestSerial() != live && ! threadShouldExit())
        {
            // Pre-empted: re-queue it behind the live request.
            calibrateAsync (std::move (job.first), std::move (job.second));
            continue;
        }

        if (job.second)
            job.second (m);
    }
}

void NormalizationCalibrator::handleLiveRequest (std::uint32_t serial)
{
    const auto state = owner.captureSoundState();

    auto superseded = [this, serial]
    {
        return threadShouldExit() || owner.getTracker().getRequestSerial() != serial;
    };

    const auto m = state.valid ? measureState (state, superseded) : Measurement {};

    // 2.3.4: a result for a request that has since been superseded is dropped.
    if (! superseded())
        owner.handleMeasurement (serial, m);

    handledSerial.store (serial, std::memory_order_release);
}

NormalizationCalibrator::Measurement NormalizationCalibrator::measureState (const NormalizationSoundState& state,
                                                                            const std::function<bool()>& superseded)
{
    auto& shape = owner.getShapeProcessor();
    const auto hash = hashSoundState (state, shape);

    Measurement m;

    if (lookupCached (hash, m))
    {
        m.hash = hash;
        storeInMemory (hash, m);
        return m;
    }

    rendering.store (true, std::memory_order_release);
    m = renderAndMeasure (makeRenderState (state, shape), superseded, 48000.0 * state.rateFamily);
    rendering.store (false, std::memory_order_release);
    m.hash = hash;

    if (m.ok)
    {
        storeInMemory (hash, m);
        storeOnDisk (hash, m);
        return m;
    }

    if (superseded())
        return m;   // cancelled, not failed

    // 12 / 4.5: log it and fall back to the estimate.
    {
        auto* context = new juce::DynamicObject();
        context->setProperty ("hash", hash);
        ErrorLog::write (ErrorLog::Severity::warn, "Normalization", "CALIBRATION_FAILED",
                         "The loudness calibration render failed; using an estimate", juce::var (context));
    }

    auto paramValue = [&] (const char* id) -> float
    {
        const auto& all = shape.getParameters();

        for (int i = 0; i < all.size(); ++i)
            if (idOf (all[i]) == id && i < (int) state.values.size())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (all[i]))
                    return ranged->convertFrom0to1 (state.values[(size_t) i]);

        return 0.0f;
    };

    auto paramNormalised = [&] (const char* id) -> double
    {
        const auto& all = shape.getParameters();

        for (int i = 0; i < all.size(); ++i)
            if (idOf (all[i]) == id && i < (int) state.values.size())
                return (double) state.values[(size_t) i];

        return 0.0;
    };

    Measurement estimate;

    if (estimateFor ((int) std::lround (paramValue ("guitar_type")), (int) std::lround (paramValue ("amp_model")),
                     paramNormalised ("amp_gain"), estimate))
    {
        estimate.hash = hash;
        return estimate;
    }

    m.failed = true;
    return m;
}

//==============================================================================
juce::String NormalizationCalibrator::hashSoundState (const NormalizationSoundState& state, const juce::AudioProcessor& shape)
{
    const auto canonical = canonicalSoundState (state, shape);
    const auto utf8 = canonical.toUTF8();
    return juce::SHA256 (utf8.getAddress(), std::strlen (utf8.getAddress())).toHexString();
}

juce::String NormalizationCalibrator::canonicalSoundState (const NormalizationSoundState& state, const juce::AudioProcessor& shape)
{
    auto* params = new juce::DynamicObject();
    const auto& all = shape.getParameters();

    for (int i = 0; i < all.size() && i < (int) state.values.size(); ++i)
    {
        const auto* p = all[i];
        const auto id = idOf (p);

        if (id.isEmpty() || LoudnessRoles::roleFor (id) != LoudnessRole::config)
            continue;   // Performance and Mix are replaced by fixed values: they cancel

        // Only values away from the default, so a parameter another feature
        // appends at its default leaves every existing hash alone.
        const float q = quantise (*p, state.values[(size_t) i]);

        if (q != quantise (*p, p->getDefaultValue()))
            params->setProperty (id, (double) q);
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("edition", getEditionName());
    root->setProperty ("revision", kCalibrationRevision);
    root->setProperty ("params", juce::var (params));
    // Per-string detune the engine derives from parameters with a random draw
    // (string age, realism detune) is not configuration: the same preset
    // loaded twice draws it twice. The parameters that cause it are hashed.
    auto structural = state.structural.clone();

    if (auto* strings = structural.getProperty ("preset", {}).getProperty ("strings", {}).getDynamicObject())
    {
        strings->removeProperty ("fineTuneCents");
        strings->removeProperty ("realismDetuneCents");
    }

    // B-07: the save format's migration marker is not configuration either.
    if (auto* preset = structural.getProperty ("preset", {}).getDynamicObject())
        preset->removeProperty ("doublerMigrated");

    root->setProperty ("structural", structural);

    if (state.rateFamily != 1)
        root->setProperty ("rateFamily", state.rateFamily);

    juce::String canonical;
    writeCanonical (juce::var (root), canonical);
    return canonical;
}

juce::MemoryBlock NormalizationCalibrator::makeRenderState (const NormalizationSoundState& state, const juce::AudioProcessor& shape)
{
    auto structural = state.structural.clone();
    auto* root = new juce::DynamicObject();

    if (auto* s = structural.getDynamicObject())
        for (const auto& p : s->getProperties())
            root->setProperty (p.name, p.value);

    auto preset = root->getProperty ("preset");

    if (! preset.isObject())
        preset = juce::var (new juce::DynamicObject());

    auto* values = new juce::DynamicObject();
    const auto& all = shape.getParameters();

    for (int i = 0; i < all.size(); ++i)
    {
        const auto id = idOf (all[i]);

        if (id.isEmpty() || id == "preset_morph_position")
            continue;

        const float live = i < (int) state.values.size() ? state.values[(size_t) i] : all[i]->getDefaultValue();
        values->setProperty (id, (double) renderValue (*all[i], id, live));
    }

    preset.getDynamicObject()->setProperty ("parameters", juce::var (values));

    // Whatever it was built from, it has to pass the preset loader's checks.
    preset.getDynamicObject()->setProperty ("magic", PresetManager::kMagic);
    preset.getDynamicObject()->setProperty ("schemaVersion", PresetManager::kSchemaVersion);

    root->setProperty ("preset", preset);

    const auto json = juce::JSON::toString (juce::var (root), true);
    juce::MemoryBlock block;
    block.append (json.toRawUTF8(), json.getNumBytesAsUTF8());
    return block;
}

NormalizationCalibrator::Measurement NormalizationCalibrator::renderAndMeasure (const juce::MemoryBlock& stateBlock,
                                                                                std::function<bool()> shouldCancel,
                                                                                double renderRate)
{
    Measurement m;
    m.source = Source::render;
    renderCount().fetch_add (1);

    const auto started = std::chrono::steady_clock::now();
    auto elapsed = [started]
    {
        return std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    };

    if (const int maxDelay = maxInjectedDelayMsForTesting().load(); maxDelay > 0)
    {
        static std::mt19937 rng (12345);
        static std::mutex rngLock;
        int ms = 0;
        {
            const std::lock_guard<std::mutex> sl (rngLock);
            ms = (int) (rng() % (unsigned) (maxDelay + 1));
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (ms));
    }

    if (failRendersForTesting().load())
    {
        m.failed = true;
        return m;
    }

    try
    {
        const double sr = renderRate > 0.0 ? renderRate : 48000.0;
        constexpr int block = 256;

        auto processor = std::make_unique<LuthierAudioProcessor>();
        processor->getOutputNormalization().setCalibrationRenderMode (true);
        processor->setNonRealtime (true);
        processor->setStateInformation (stateBlock.getData(), (int) stateBlock.getSize());
        processor->getOutputNormalization().setCalibrationRenderMode (true);

        FixedPlayHead playHead;
        processor->setPlayHead (&playHead);
        processor->prepareToPlay (sr, block);

        auto& engine = processor->getEngine();
        const bool bass = engine.getGuitarSpec().category == GuitarCategory::Bass;
        const auto phrase = NormalizationPhrase::buildBuffer (bass, NormalizationPhrase::rootNoteFor (engine),
                                                              engine.getNumStrings(), sr);

        const int channels = juce::jmax (2, processor->getTotalNumInputChannels(), processor->getTotalNumOutputChannels());
        juce::AudioBuffer<float> buffer (channels, block);
        juce::MidiBuffer hostMidi, direct;

        Bs1770Meter meter;
        meter.prepare (sr, 2);

        const int settle = (int) (NormalizationPhrase::kSettleSeconds * sr);
        const int window = (int) (NormalizationPhrase::kWindowSeconds * sr);
        bool finite = true;

        for (int pos = -settle; pos < window; pos += block)
        {
            if ((shouldCancel && shouldCancel()) || elapsed() > getRenderTimeoutSeconds())
            {
                processor->setPlayHead (nullptr);
                m.failed = true;
                return m;
            }

            const int n = juce::jmin (block, window - pos);
            buffer.setSize (channels, n, false, false, true);
            buffer.clear();
            hostMidi.clear();
            direct.clear();

            if (pos >= 0)
                direct.addEvents (phrase, pos, n, -pos);

            processor->getOutputNormalization().setCalibrationDirectMidi (direct.isEmpty() ? nullptr : &direct);
            processor->processBlock (buffer, hostMidi);
            processor->getOutputNormalization().setCalibrationDirectMidi (nullptr);

            if (pos < 0)
                continue;

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    if (! std::isfinite (buffer.getSample (ch, i)))
                        finite = false;

            meter.process (buffer.getReadPointer (0), buffer.getReadPointer (1), n);
        }

        processor->setPlayHead (nullptr);
        processor->releaseResources();

        if (! finite)
        {
            m.failed = true;
            return m;
        }

        m = fromLufs (meter.getIntegratedLufs());
        m.source = Source::render;
        return m;
    }
    catch (...)
    {
        m.failed = true;
        return m;
    }
}

//==============================================================================
bool NormalizationCalibrator::lookupCached (const juce::String& hash, Measurement& out)
{
    auto& c = caches();

    {
        const std::lock_guard<std::mutex> sl (c.lock);

        if (auto it = c.lruIndex.find (hash); it != c.lruIndex.end())
        {
            c.lru.splice (c.lru.begin(), c.lru, it->second);
            out = it->second->second;
            out.unmeasurable = out.measuredLufs <= Bs1770Meter::kAbsoluteGateLufs;
            out.source = Source::memory;
            return true;
        }

        loadFactoryLocked (c);

        if (auto it = c.factory.find (hash); it != c.factory.end())
        {
            out = fromLufs (it->second.lufs);
            out.source = Source::factory;
            return true;
        }
    }

    // Disk: a cache, not user data. Anything unreadable is simply a miss (12).
    const auto file = getDiskCacheFolder().getChildFile (hash.substring (0, 16) + ".json");

    if (file.existsAsFile())
    {
        const auto json = juce::JSON::parse (file);

        if (json.isObject()
             && json.getProperty ("hash", {}).toString() == hash
             && (int) json.getProperty ("revision", 0) == kCalibrationRevision
             && json.hasProperty ("measuredLufs"))
        {
            const double lufs = (double) json.getProperty ("measuredLufs", -120.0);

            if (std::isfinite (lufs))
            {
                out = fromLufs (lufs);
                out.source = Source::disk;
                return true;
            }
        }
    }

    return false;
}

void NormalizationCalibrator::storeInMemory (const juce::String& hash, const Measurement& m)
{
    if (! m.ok || m.estimate)
        return;

    auto& c = caches();
    const std::lock_guard<std::mutex> sl (c.lock);

    if (auto it = c.lruIndex.find (hash); it != c.lruIndex.end())
    {
        c.lru.splice (c.lru.begin(), c.lru, it->second);
        return;
    }

    c.lru.emplace_front (hash, m);
    c.lruIndex[hash] = c.lru.begin();

    while ((int) c.lru.size() > kLruCapacity)
    {
        c.lruIndex.erase (c.lru.back().first);
        c.lru.pop_back();
    }
}

void NormalizationCalibrator::storeOnDisk (const juce::String& hash, const Measurement& m)
{
    if (! m.ok || m.estimate)
        return;

    const auto folder = getDiskCacheFolder();

    if (! folder.createDirectory())
        return;

    auto* o = new juce::DynamicObject();
    o->setProperty ("hash", hash);
    o->setProperty ("measuredLufs", m.measuredLufs);
    o->setProperty ("gainCentiDb", (int) std::lround ((OutputNormalization::kDefaultTarget - m.measuredLufs) * 100.0));
    o->setProperty ("revision", kCalibrationRevision);

    // file-formats 13: written beside, then moved over, so a reader never sees half.
    const auto file = folder.getChildFile (hash.substring (0, 16) + ".json");
    const auto temp = file.getSiblingFile (file.getFileName() + ".tmp" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 20)));

    if (temp.replaceWithText (juce::JSON::toString (juce::var (o), true)))
        if (! temp.moveFileTo (file))
            temp.deleteFile();
}

bool NormalizationCalibrator::estimateFor (int guitarType, int ampModel, double drive, Measurement& out)
{
    auto& c = caches();
    const std::lock_guard<std::mutex> sl (c.lock);
    loadFactoryLocked (c);

    const Caches::FactoryEntry* best = nullptr;
    double bestScore = 1.0e9;

    for (const auto& [hash, e] : c.factory)
    {
        // Same guitar and amp first, then the nearest drive (4.5).
        const double score = (e.guitarType == guitarType ? 0.0 : 100.0)
                           + (e.ampModel == ampModel ? 0.0 : 10.0)
                           + std::abs (e.drive - drive);

        if (score < bestScore)
        {
            bestScore = score;
            best = &e;
        }
    }

    if (best == nullptr)
        return false;

    out = fromLufs (best->lufs);
    out.estimate = true;
    out.source = Source::estimate;
    return true;
}

//==============================================================================
juce::File NormalizationCalibrator::getDiskCacheFolder()
{
    auto& c = caches();

    {
        const std::lock_guard<std::mutex> sl (c.lock);

        if (c.diskOverride != juce::File())
            return c.diskOverride;
    }

    // Under the cache folder Diagnostics "Reset all settings and clear caches"
    // deletes (4.4); the preview cache root of preset-browser-previews.md
    // lives there too.
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier").getChildFile ("Cache").getChildFile ("normalization");
}

void NormalizationCalibrator::setDiskCacheFolderForTesting (const juce::File& folder)
{
    auto& c = caches();
    const std::lock_guard<std::mutex> sl (c.lock);
    c.diskOverride = folder;
}

void NormalizationCalibrator::clearDiskCache()
{
    const auto folder = getDiskCacheFolder();

    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.json"))
        f.deleteFile();
}

juce::File NormalizationCalibrator::getFactoryTableFile()
{
    auto& c = caches();

    {
        const std::lock_guard<std::mutex> sl (c.lock);

        if (c.factoryFileOverride != juce::File())
            return c.factoryFileOverride;
    }

    return IrLibrary::getResourcesFolder().getChildFile ("NormalizationFactory.json");
}

void NormalizationCalibrator::setFactoryTableFileForTesting (const juce::File& file)
{
    auto& c = caches();
    const std::lock_guard<std::mutex> sl (c.lock);
    c.factoryFileOverride = file;
    c.factoryLoaded = false;
}

void NormalizationCalibrator::reloadFactoryTable()
{
    auto& c = caches();
    const std::lock_guard<std::mutex> sl (c.lock);
    c.factoryLoaded = false;
    loadFactoryLocked (c);
}

int NormalizationCalibrator::getNumFactoryEntries()
{
    auto& c = caches();
    const std::lock_guard<std::mutex> sl (c.lock);
    loadFactoryLocked (c);
    return (int) c.factory.size();
}

void NormalizationCalibrator::addFactoryEntry (const juce::String& hash, double measuredLufs, int guitarType,
                                               int ampModel, double drive, const juce::String& preset)
{
    auto& c = caches();
    const std::lock_guard<std::mutex> sl (c.lock);
    loadFactoryLocked (c);
    c.factory[hash] = { measuredLufs, guitarType, ampModel, drive, preset };
}

bool NormalizationCalibrator::writeFactoryTable (const juce::File& file)
{
    auto& c = caches();
    auto* entries = new juce::DynamicObject();

    {
        const std::lock_guard<std::mutex> sl (c.lock);

        for (const auto& [hash, e] : c.factory)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("lufs", std::round (e.lufs * 100.0) / 100.0);
            o->setProperty ("guitarType", e.guitarType);
            o->setProperty ("ampModel", e.ampModel);
            o->setProperty ("drive", std::round (e.drive * 1000.0) / 1000.0);
            o->setProperty ("preset", e.preset);
            entries->setProperty (hash, juce::var (o));
        }
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("description", "output-normalization.md 4.4: the factory calibration table. "
                                      "Generated by luthier-render --calibrate-factory; see "
                                      "scripts/regen_normalization_factory.sh.");
    root->setProperty ("revision", kCalibrationRevision);
    root->setProperty ("edition", getEditionName());
    root->setProperty ("entries", juce::var (entries));

    return file.replaceWithText (juce::JSON::toString (juce::var (root), false));
}

void NormalizationCalibrator::clearMemoryCache()
{
    auto& c = caches();
    const std::lock_guard<std::mutex> sl (c.lock);
    c.lru.clear();
    c.lruIndex.clear();
}

} // namespace luthier
