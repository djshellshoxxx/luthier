#include "PreviewRenderer.h"
#include "../../PluginProcessor.h"
#include "../../Parameters.h"
#include <juce_cryptography/juce_cryptography.h>

namespace luthier
{

namespace
{
    //==========================================================================
    // 5.1 item 1: the canonical JSON. Sorted keys and %.9g numbers, so the same
    // sound always hashes the same whatever order or precision wrote it.
    void writeCanonical (juce::String& out, const juce::var& v)
    {
        if (auto* o = v.getDynamicObject())
        {
            juce::StringArray keys;

            for (const auto& p : o->getProperties())
                keys.add (p.name.toString());

            keys.sort (false);
            out << "{";

            for (int i = 0; i < keys.size(); ++i)
            {
                if (i > 0)
                    out << ",";

                out << juce::JSON::toString (juce::var (keys[i])) << ":";
                writeCanonical (out, o->getProperty (keys[i]));
            }

            out << "}";
        }
        else if (auto* a = v.getArray())
        {
            out << "[";

            for (int i = 0; i < a->size(); ++i)
            {
                if (i > 0)
                    out << ",";

                writeCanonical (out, (*a)[i]);
            }

            out << "]";
        }
        else if (v.isBool())
        {
            out << ((bool) v ? "true" : "false");
        }
        else if (v.isInt() || v.isInt64() || v.isDouble())
        {
            char buffer[64];
            std::snprintf (buffer, sizeof (buffer), "%.9g", (double) v);
            out << buffer;
        }
        else if (v.isVoid() || v.isUndefined())
        {
            out << "null";
        }
        else
        {
            out << juce::JSON::toString (v);
        }
    }

    /** Audio files a preset names (user IRs), for 5.1 item 3. */
    void collectAudioFiles (const juce::var& v, juce::StringArray& out)
    {
        if (auto* o = v.getDynamicObject())
        {
            for (const auto& p : o->getProperties())
                collectAudioFiles (p.value, out);
        }
        else if (auto* a = v.getArray())
        {
            for (const auto& e : *a)
                collectAudioFiles (e, out);
        }
        else if (v.isString())
        {
            const auto s = v.toString();

            for (auto* ext : { ".wav", ".aif", ".aiff", ".flac" })
                if (s.endsWithIgnoreCase (ext))
                    out.addIfNotAlreadyThere (s);
        }
    }

    juce::String guitarComponent (const juce::var& json)
    {
        const auto block = json.getProperty ("guitar", {});
        juce::String reference;
        juce::var overrideGuitar;

        if (auto* object = block.getDynamicObject())
        {
            reference = object->getProperty ("reference").toString();

            if (object->getProperty ("override").getDynamicObject() != nullptr)
                overrideGuitar = object->getProperty ("override");
        }

        juce::String out;

        if (! overrideGuitar.isVoid())
        {
            out << "override:";
            writeCanonical (out, overrideGuitar);
        }
        else
        {
            if (reference.isEmpty())
            {
                // As takeGuitarBlock does: a pre-Workshop preset names its type only.
                const int numTypes = juce::jmax (1, Parameters::guitarTypeNames().size());
                double normalised = 0.0;

                if (auto* params = json.getProperty ("parameters", {}).getDynamicObject())
                    normalised = (double) params->getProperty (ParamIDs::guitarType);

                const int type = juce::jlimit (0, numTypes - 1, (int) std::round (normalised * (numTypes - 1)));
                const auto path = LuthierAudioProcessor::getFactoryGuitarPath ((GuitarType) type);
                reference = path.isNotEmpty() ? "Factory/" + path : juce::String ("type:") + juce::String (type);
            }

            const auto file = LuthierAudioProcessor::resolveGuitarFile (reference);

            if (file.existsAsFile())
            {
                out << "file:";
                writeCanonical (out, juce::JSON::parse (file.loadFileAsString()));
            }
            else
            {
                out << "missing:" << reference;
            }
        }

        out << "|capo:" << block.getProperty ("capo", {}).toString();
        return out;
    }

    //==========================================================================
    /** Ogg's page CRC: polynomial 0x04c11db7, unreflected, zero start. */
    juce::uint32 oggCrc (const juce::uint8* data, size_t size)
    {
        static const auto table = []
        {
            std::array<juce::uint32, 256> t {};

            for (juce::uint32 i = 0; i < 256; ++i)
            {
                juce::uint32 r = i << 24;

                for (int j = 0; j < 8; ++j)
                    r = (r & 0x80000000u) ? ((r << 1) ^ 0x04c11db7u) : (r << 1);

                t[i] = r;
            }

            return t;
        }();

        juce::uint32 crc = 0;

        for (size_t i = 0; i < size; ++i)
            crc = (crc << 8) ^ table[((crc >> 24) ^ data[i]) & 0xff];

        return crc;
    }

    /** JUCE's writer seeds the stream serial from a random number; every page
        is rewritten with a fixed one and its CRC recomputed (PB-01). */
    void fixOggSerial (juce::MemoryBlock& block, juce::uint32 serial)
    {
        auto* data = static_cast<juce::uint8*> (block.getData());
        const size_t size = block.getSize();
        size_t pos = 0;

        while (pos + 27 <= size && std::memcmp (data + pos, "OggS", 4) == 0)
        {
            const int segments = data[pos + 26];

            if (pos + 27 + (size_t) segments > size)
                break;

            size_t body = 0;

            for (int s = 0; s < segments; ++s)
                body += data[pos + 27 + (size_t) s];

            const size_t pageSize = 27 + (size_t) segments + body;

            if (pos + pageSize > size)
                break;

            for (int b = 0; b < 4; ++b)
            {
                data[pos + 14 + (size_t) b] = (juce::uint8) ((serial >> (8 * b)) & 0xff);
                data[pos + 22 + (size_t) b] = 0;
            }

            const auto crc = oggCrc (data + pos, pageSize);

            for (int b = 0; b < 4; ++b)
                data[pos + 22 + (size_t) b] = (juce::uint8) ((crc >> (8 * b)) & 0xff);

            pos += pageSize;
        }
    }

    /** The offline playhead for a preset whose rhythm engine is on (3.1). */
    class OfflinePlayHead : public juce::AudioPlayHead
    {
    public:
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (100.0);
            info.setIsPlaying (true);
            info.setTimeInSamples (samplePosition);
            info.setTimeInSeconds ((double) samplePosition / PreviewRenderer::kSampleRate);
            info.setPpqPosition ((double) samplePosition / PreviewRenderer::kSampleRate * 100.0 / 60.0);
            info.setTimeSignature (TimeSignature { 4, 4 });
            return info;
        }

        juce::int64 samplePosition = 0;
    };
}

//==============================================================================
PreviewRenderer::PreviewRenderer (Factory f) : factory (std::move (f)) {}

PreviewRenderer::~PreviewRenderer()
{
    releaseInstance();
}

void PreviewRenderer::releaseInstance()
{
    reader.reset();
    instance.reset();
    rangeInstance.reset();
}

std::unique_ptr<LuthierAudioProcessor> PreviewRenderer::makeInstance() const
{
    const LuthierAudioProcessor::ScopedOfflineRenderConstruction scope;
    auto made = factory ? factory() : std::make_unique<LuthierAudioProcessor>();

    if (made != nullptr)
        made->setOfflineRenderMode();

    return made;
}

void PreviewRenderer::adoptInstance (std::unique_ptr<LuthierAudioProcessor> p)
{
    instance = std::move (p);
    reader = instance != nullptr ? std::make_unique<PresetFeatureReader> (*instance) : nullptr;
}

std::unique_ptr<LuthierAudioProcessor> PreviewRenderer::takeInstance()
{
    reader.reset();
    return std::move (instance);
}

LuthierAudioProcessor& PreviewRenderer::getRangeSource()
{
    if (rangeInstance == nullptr)
        rangeInstance = makeInstance();

    return *rangeInstance;
}

LuthierAudioProcessor& PreviewRenderer::getInstance()
{
    if (instance == nullptr)
        adoptInstance (makeInstance());

    return *instance;
}

//==============================================================================
juce::String PreviewRenderer::canonicalSoundJson (const juce::var& json)
{
    static const juce::StringArray excluded { "name", "category", "author", "description", "tags",
                                              "uid", "previewPhrase", "meta", "pluginVersion" };
    juce::String out;

    if (auto* o = json.getDynamicObject())
    {
        auto* copy = new juce::DynamicObject();

        for (const auto& p : o->getProperties())
            if (! excluded.contains (p.name.toString()))
                copy->setProperty (p.name, p.value);

        writeCanonical (out, juce::var (copy));
    }
    else
    {
        writeCanonical (out, json);
    }

    return out;
}

juce::String PreviewRenderer::computeSoundHash (const juce::var& json, PreviewPhrase::Id phrase,
                                                const juce::String& pluginVersion, int renderRevision)
{
    juce::String text;
    text << canonicalSoundJson (json) << "\n";
    text << guitarComponent (json) << "\n";

    juce::StringArray audioFiles;
    collectAudioFiles (json, audioFiles);
    audioFiles.sort (false);

    for (const auto& path : audioFiles)
    {
        const juce::File f (juce::File::isAbsolutePath (path) ? path : juce::String());

        if (f.existsAsFile())
            text << "ir:" << path << ":" << f.getSize() << ":" << f.getLastModificationTime().toMilliseconds() << "\n";
    }

    text << PreviewPhrase::getIdString (phrase) << "\n" << pluginVersion << "\n" << renderRevision;

    return juce::SHA256 (text.toUTF8()).toHexString();
}

PreviewPhrase::Context PreviewRenderer::phraseContext (const juce::var& json, const PresetFeatures& f)
{
    PreviewPhrase::Context c;
    c.name = json.getProperty ("name", {}).toString();
    c.category = json.getProperty ("category", {}).toString();
    c.previewPhrase = json.getProperty ("previewPhrase", {}).toString();

    if (auto* tags = json.getProperty ("tags", {}).getArray())
        for (const auto& t : *tags)
            c.tags.add (t.toString());

    c.bassFamily = f.family == PresetFeatures::bass;
    c.nylon = f.nylon;
    c.acousticBody = f.acousticBody;
    c.slide = f.slideOn;
    c.slapArmed = f.techniques[PresetFeatures::slap];
    c.rhythmEngineOn = f.rhythmEngineOn;
    c.drive = f.drive;
    c.lowestOpenMidi = f.lowestOpenMidi;
    c.highestMidi = f.highestMidi;

    // 3.1's ambient row also takes a room blend above 0.5.
    if (f.roomBlend > 0.5 && ! c.tags.contains ("ambient"))
        c.tags.add ("ambient");

    return c;
}

PreviewPhrase::Id PreviewRenderer::choosePhrase (const juce::var& json, const PresetFeatures& f)
{
    return PreviewPhrase::choose (phraseContext (json, f));
}

std::array<float, PreviewResult::kNumPeaks> PreviewRenderer::computePeaks (const juce::AudioBuffer<float>& clip)
{
    std::array<float, PreviewResult::kNumPeaks> peaks {};
    const int n = clip.getNumSamples();

    if (n == 0)
        return peaks;

    for (int p = 0; p < PreviewResult::kNumPeaks; ++p)
    {
        const int start = (int) ((juce::int64) n * p / PreviewResult::kNumPeaks);
        const int end = juce::jmax (start + 1, (int) ((juce::int64) n * (p + 1) / PreviewResult::kNumPeaks));
        float peak = 0.0f;

        for (int ch = 0; ch < clip.getNumChannels(); ++ch)
            peak = juce::jmax (peak, clip.getMagnitude (ch, start, juce::jmin (n, end) - start));

        peaks[(size_t) p] = peak;
    }

    return peaks;
}

void PreviewRenderer::applyEndFade (juce::AudioBuffer<float>& clip, double sampleRate)
{
    const int n = clip.getNumSamples();
    const int silent = juce::jmin (n, (int) std::round (0.010 * sampleRate));
    const int fade = juce::jmin (n - silent, (int) std::round ((PreviewPhrase::kFadeSeconds - 0.010) * sampleRate));
    const int fadeStart = n - silent - fade;

    for (int ch = 0; ch < clip.getNumChannels(); ++ch)
    {
        auto* d = clip.getWritePointer (ch);

        for (int i = 0; i < fade; ++i)
        {
            const double g = 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * (i + 1) / (double) fade));
            d[fadeStart + i] = (float) (d[fadeStart + i] * g);
        }

        for (int i = n - silent; i < n; ++i)
            d[i] = 0.0f;
    }
}

juce::MemoryBlock PreviewRenderer::encodeOgg (const juce::AudioBuffer<float>& clip, double sampleRate, juce::uint32 serial)
{
    juce::OggVorbisAudioFormat format;
    juce::MemoryBlock best;

    // q0.5 first (3.2 step 8); lower qualities only if 64 KB is exceeded.
    for (int quality = 5; quality >= 0; --quality)
    {
        juce::MemoryBlock block;

        {
            std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream> (block, false);
            auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                              .withSampleRate (sampleRate)
                                                              .withNumChannels (clip.getNumChannels())
                                                              .withBitsPerSample (16)
                                                              .withQualityOptionIndex (quality));

            if (writer == nullptr)
                return {};

            writer->writeFromAudioSampleBuffer (clip, 0, clip.getNumSamples());
        }

        fixOggSerial (block, serial);
        best = block;

        if ((int) block.getSize() <= kMaxOggBytes)
            break;
    }

    return best;
}

bool PreviewRenderer::decodeOgg (const void* data, size_t size, juce::AudioBuffer<float>& out, double& sampleRate)
{
    juce::OggVorbisAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> readerPtr (format.createReaderFor (
        new juce::MemoryInputStream (data, size, false), true));

    if (readerPtr == nullptr || readerPtr->lengthInSamples <= 0 || readerPtr->lengthInSamples > 48000 * 10)
        return false;

    out.setSize ((int) juce::jmax (1u, readerPtr->numChannels), (int) readerPtr->lengthInSamples);
    out.clear();

    if (! readerPtr->read (&out, 0, (int) readerPtr->lengthInSamples, 0, true, true))
        return false;

    sampleRate = readerPtr->sampleRate;
    return true;
}

//==============================================================================
PreviewResult PreviewRenderer::render (const juce::var& json, const std::atomic<bool>* cancel, double timeoutSeconds)
{
    juce::TemporaryFile temp (".luthierpreset");

    if (! temp.getFile().replaceWithText (juce::JSON::toString (json, false)))
    {
        PreviewResult r;
        r.error = "could not write a temporary file";
        return r;
    }

    return renderLoaded (temp.getFile(), cancel, timeoutSeconds);
}

PreviewResult PreviewRenderer::render (const juce::File& file, const std::atomic<bool>* cancel, double timeoutSeconds)
{
    return renderLoaded (file, cancel, timeoutSeconds);
}

PreviewResult PreviewRenderer::renderLoaded (const juce::File& file, const std::atomic<bool>* cancel, double timeoutSeconds)
{
    PreviewResult result;
    const auto startMs = juce::Time::getMillisecondCounterHiRes();

    const auto json = juce::JSON::parse (file.loadFileAsString());

    if (json.getDynamicObject() == nullptr)
    {
        result.error = "the preset file is not readable";
        return result;
    }

    /*  A fresh instance per job (DECISIONS, FEAT-BROWSER): a reused one carries
        the last job's engine state - drift, noise and modulation phases that
        reset() does not rewind - and PB-01 wants identical inputs to give
        identical bytes. A render instance costs ~50 ms to build, well inside
        11's 400 ms cold-create budget. */
    adoptInstance (makeInstance());
    auto& p = getInstance();

    result.params = reader->read (json);
    result.phrase = choosePhrase (json, result.params);
    result.soundHash = computeSoundHash (json, result.phrase);

    // 1. reset, 2. load through the instance's own PresetManager.
    p.releaseResources();
    p.reset();
    p.panic();

    auto& manager = p.getPresetManager();

    if (! manager.loadPreset (file))
    {
        result.error = manager.getLastLoadError().isNotEmpty() ? manager.getLastLoadError()
                                                               : juce::String ("the preset could not be loaded");
        return result;
    }

    p.getParameterBridge().applyAllNow();

    // A guitar the load could not find is replaced by the fallback: approximate.
    {
        const auto block = json.getProperty ("guitar", {});
        const auto reference = block.getProperty ("reference", {}).toString();
        const bool hasOverride = block.getProperty ("override", {}).getDynamicObject() != nullptr;

        if (reference.isNotEmpty() && ! hasOverride && ! LuthierAudioProcessor::resolveGuitarFile (reference).existsAsFile())
            result.approximate = true;
    }

    // 3. prepare at 48 kHz in 256-sample blocks.
    OfflinePlayHead playHead;
    p.setPlayHead (result.params.rhythmEngineOn ? &playHead : nullptr);
    p.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    p.setNonRealtime (true);
    p.prepareToPlay (kSampleRate, kBlockSize);


    const auto built = PreviewPhrase::build (result.phrase, phraseContext (json, result.params));
    result.lowestNote = built.lowestNote;

    const int channels = juce::jmax (2, juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()));
    juce::AudioBuffer<float> block (channels, kBlockSize);
    juce::MidiBuffer midi;

    const int settle = (int) std::round (kSettleSeconds * kSampleRate);
    const int total = (int) std::round (PreviewPhrase::kMaxClipSeconds * kSampleRate);

    juce::AudioBuffer<float> rendered (2, total);
    rendered.clear();

    auto timedOut = [&]
    {
        return (juce::Time::getMillisecondCounterHiRes() - startMs) > timeoutSeconds * 1000.0;
    };

    auto finishEarly = [&] (bool cancelled)
    {
        p.releaseResources();
        p.setPlayHead (nullptr);
        result.cancelled = cancelled;
        result.timedOut = ! cancelled;
        result.error = cancelled ? "cancelled" : "rendering took longer than 10 seconds";
        result.renderMs = juce::Time::getMillisecondCounterHiRes() - startMs;
        return result;
    };

    // 4. Discard 0.5 s of settling; its last 200 ms is the rig's idle noise.
    const int idleSamples = (int) std::round (0.2 * kSampleRate);
    juce::AudioBuffer<float> idle (2, idleSamples);
    idle.clear();

    for (int pos = 0; pos < settle; pos += kBlockSize)
    {
        const int n = juce::jmin (kBlockSize, settle - pos);
        block.setSize (channels, n, false, false, true);
        block.clear();
        midi.clear();
        p.processBlock (block, midi);

        for (int i = 0; i < n; ++i)
        {
            const int at = pos + i - (settle - idleSamples);

            if (at >= 0 && at < idleSamples)
                for (int ch = 0; ch < 2; ++ch)
                    idle.setSample (ch, at, block.getSample (juce::jmin (ch, block.getNumChannels() - 1), i));
        }

        if ((cancel != nullptr && cancel->load()) || timedOut())
            return finishEarly (cancel != nullptr && cancel->load());
    }

    // 5. The phrase and its tail.
    int eventIndex = 0;
    const auto& seq = built.sequence;

    // The render stops once the tail has sat 60 dB under the peak for longer
    // than the clip keeps after it: those samples would be trimmed anyway, so
    // the clip is the same, and a short preset renders in less time.
    const int noteEndSample = (int) (built.noteEndSeconds * kSampleRate);
    const int keepAfter = (int) ((PreviewPhrase::kFadeSeconds + 0.1) * kSampleRate);
    float runningPeak = 0.0f;
    int lastLoud = 0;

    for (int pos = 0; pos < total; pos += kBlockSize)
    {
        const int n = juce::jmin (kBlockSize, total - pos);
        block.setSize (channels, n, false, false, true);
        block.clear();
        midi.clear();

        const double blockEnd = (double) (pos + n) / kSampleRate;

        while (eventIndex < seq.getNumEvents())
        {
            const auto* e = seq.getEventPointer (eventIndex);

            if (e->message.getTimeStamp() >= blockEnd)
                break;

            const int offset = juce::jlimit (0, n - 1, (int) std::round (e->message.getTimeStamp() * kSampleRate) - pos);
            midi.addEvent (e->message, offset);
            ++eventIndex;
        }

        playHead.samplePosition = settle + pos;
        p.processBlock (block, midi);

        for (int ch = 0; ch < 2; ++ch)
            rendered.copyFrom (ch, pos, block, juce::jmin (ch, block.getNumChannels() - 1), 0, n);

        runningPeak = juce::jmax (runningPeak, rendered.getMagnitude (pos, n));

        for (int i = 0; i < n; ++i)
            if (std::abs (rendered.getSample (0, pos + i)) > runningPeak * 0.001f
                || std::abs (rendered.getSample (1, pos + i)) > runningPeak * 0.001f)
                lastLoud = pos + i;

        if (pos + n > noteEndSample && pos + n - juce::jmax (lastLoud, noteEndSample) > keepAfter)
            break;

        if (testDelayPerBlockMs.load() > 0.0)
            juce::Thread::sleep ((int) testDelayPerBlockMs.load());

        if ((cancel != nullptr && cancel->load()) || timedOut())
            return finishEarly (cancel != nullptr && cancel->load());
    }

    p.releaseResources();
    p.setPlayHead (nullptr);

    // A NaN from the engine would poison every meter below; treat it as a failure.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < total; ++i)
            if (! std::isfinite (rendered.getSample (ch, i)))
            {
                result.error = "the engine produced a non-finite sample";
                return result;
            }

    // The clip ends when the tail has fallen 60 dB under the peak, 2.0-4.0 s.
    {
        const float peak = rendered.getMagnitude (0, total);
        int end = total;

        if (peak > 0.0f)
        {
            const float floor = peak * 0.001f;
            const int noteEnd = (int) (built.noteEndSeconds * kSampleRate);
            int last = noteEnd;

            for (int i = total - 1; i > noteEnd; --i)
                if (std::abs (rendered.getSample (0, i)) > floor || std::abs (rendered.getSample (1, i)) > floor)
                {
                    last = i;
                    break;
                }

            end = last + (int) (PreviewPhrase::kFadeSeconds * kSampleRate);
        }

        end = juce::jlimit ((int) (2.0 * kSampleRate), total, end);
        result.clip.setSize (2, end);

        for (int ch = 0; ch < 2; ++ch)
            result.clip.copyFrom (ch, 0, rendered, ch, 0, end);
    }

    applyEndFade (result.clip, kSampleRate);

    // 6. Analyse before any gain.
    result.features = ToneFeatures::analyse (result.clip, kSampleRate, built.noteEndSeconds,
                                             ToneFeatures::idleFloorDb (idle));

    // 7. -18 LUFS integrated, lowered further for a -3 dBTP ceiling. No limiter.
    const double loudness = result.features.loudnessLufs;

    if (loudness <= -69.0)
    {
        result.error = "the preset made no sound";
        result.renderMs = juce::Time::getMillisecondCounterHiRes() - startMs;
        return result;
    }

    double gainDb = kTargetLufs - loudness;
    const double truePeak = ToneFeatures::truePeakDb (result.clip) + gainDb;

    if (truePeak > kCeilingDbtp)
        gainDb -= truePeak - kCeilingDbtp;

    result.gainDb = gainDb;
    result.clip.applyGain ((float) juce::Decibels::decibelsToGain (gainDb));

    // 8. Ogg Vorbis and the peak envelope.
    juce::uint32 serial = 0;

    for (int i = 0; i < 8; ++i)
        serial = (serial << 4) | (juce::uint32) juce::CharacterFunctions::getHexDigitValue (result.soundHash[i]);

    result.ogg = encodeOgg (result.clip, kSampleRate, serial);
    result.peaks = computePeaks (result.clip);
    result.renderMs = juce::Time::getMillisecondCounterHiRes() - startMs;

    if (result.ogg.isEmpty())
    {
        result.error = "the preview could not be encoded";
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace luthier
