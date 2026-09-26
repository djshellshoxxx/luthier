#include "ToneMatch.h"

#include <algorithm>
#include <complex>

namespace luthier
{

//==============================================================================
void ImpulseResponse::normalise()
{
    double peak = 0.0;

    for (float sample : samples)
        peak = juce::jmax (peak, (double) std::abs (sample));

    if (peak <= 1.0e-9)
        return;

    // -0.1 dBFS, so that swapping one IR for another does not change the level
    // and the convolution has a hair of headroom.
    const double target = dbToGain (-0.1);
    const double gain = target / peak;

    for (float& sample : samples)
        sample = (float) (sample * gain);
}

//==============================================================================
juce::var IrMetadata::toVar() const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("name", name);
    object->setProperty ("type", type);
    object->setProperty ("sample_rate", sampleRate);
    object->setProperty ("length_ms", lengthMs);
    object->setProperty ("author", author);
    object->setProperty ("notes", notes);

    juce::Array<juce::var> tagArray;

    for (const auto& tag : tags)
        tagArray.add (tag);

    object->setProperty ("tags", tagArray);

    return { object };
}

IrMetadata IrMetadata::fromVar (const juce::var& state)
{
    IrMetadata metadata;

    auto* object = state.getDynamicObject();

    if (object == nullptr)
        return metadata;

    metadata.name       = object->getProperty ("name").toString();
    metadata.type       = object->getProperty ("type").toString();
    metadata.sampleRate = juce::jmax (1.0, (double) object->getProperty ("sample_rate"));
    metadata.lengthMs   = juce::jmax (0.0, (double) object->getProperty ("length_ms"));
    metadata.author     = object->getProperty ("author").toString();
    metadata.notes      = object->getProperty ("notes").toString();

    if (const auto* tagArray = object->getProperty ("tags").getArray())
        for (const auto& tag : *tagArray)
            metadata.tags.add (tag.toString());

    return metadata;
}

IrMetadata IrMetadata::forFile (const juce::File& irFile)
{
    const auto sidecar = irFile.withFileExtension (".json");

    if (sidecar.existsAsFile())
    {
        const auto parsed = juce::JSON::parse (sidecar.loadFileAsString());

        if (parsed.getDynamicObject() != nullptr)
            return fromVar (parsed);
    }

    /*  tone-match 5: a missing sidecar falls back to parsing the filename.

        IR packs are named descriptively by convention - "Marshall 4x12 Greenback
        SM57 On-Axis.wav" - so the words in the name are worth more as tags than
        nothing is. */
    IrMetadata metadata;

    metadata.name = irFile.getFileNameWithoutExtension();

    const auto lowered = metadata.name.toLowerCase();

    if (lowered.contains ("body") || lowered.contains ("acoustic"))
        metadata.type = "body";
    else if (lowered.contains ("room") || lowered.contains ("hall"))
        metadata.type = "room";
    else
        metadata.type = "cabinet";

    auto words = juce::StringArray::fromTokens (metadata.name, " -_", "");
    words.trim();
    words.removeEmptyStrings();

    for (const auto& word : words)
        if (word.length() > 2)
            metadata.tags.addIfNotAlreadyThere (word.toLowerCase());

    return metadata;
}

bool IrMetadata::saveFor (const juce::File& irFile) const
{
    const auto sidecar = irFile.withFileExtension (".json");

    sidecar.getParentDirectory().createDirectory();

    return sidecar.replaceWithText (juce::JSON::toString (toVar(), false));
}

//==============================================================================
IrSlot::IrSlot()
{
    formats.registerBasicFormats();
}

IrSlot::~IrSlot()
{
    active.store (nullptr, std::memory_order_release);
    live.reset();
    retired.reset();
}

void IrSlot::prepare (double sampleRate, int maxBlockSize)
{
    sr = juce::jmax (1.0, sampleRate);
    blockSize = juce::jmax (1, maxBlockSize);

    convolution = std::make_unique<juce::dsp::Convolution>();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = (juce::uint32) blockSize;
    spec.numChannels = 2;

    convolution->prepare (spec);

    wetBuffer.setSize (2, blockSize, false, true, false);
    wetBuffer.clear();

    // The file's rate has not changed, but the host's may have, so the response
    // is rebuilt rather than reused.
    if (! rawSamples.empty())
        rebuild();
}

void IrSlot::reset() noexcept
{
    if (convolution != nullptr)
        convolution->reset();

    wetBuffer.clear();
}

//==============================================================================
juce::String IrSlot::getName() const
{
    auto* current = active.load (std::memory_order_acquire);

    return (current != nullptr) ? current->name : juce::String();
}

double IrSlot::getLengthMs() const
{
    auto* current = active.load (std::memory_order_acquire);

    return (current != nullptr) ? current->getLengthMs() : 0.0;
}

//==============================================================================
std::vector<float> IrSlot::resample (const std::vector<float>& source,
                                     double fromRate, double toRate)
{
    if (source.empty() || fromRate <= 0.0 || toRate <= 0.0
          || std::abs (fromRate - toRate) < 1.0e-6)
        return source;

    const double ratio = toRate / fromRate;
    const int outputLength = juce::jmax (1, (int) std::ceil ((double) source.size() * ratio));

    std::vector<float> output ((size_t) outputLength, 0.0f);

    /*  Windowed sinc, as tone-match 0.2 requires, with at least 512 taps.

        Linear interpolation would be far cheaper and is what most plugins do
        here, but it is a one-pole lowpass with a corner that moves with the
        ratio: resampling a 96 kHz cabinet IR down to 48 would audibly dull it.
        A windowed sinc is flat to nearly Nyquist. */
    constexpr int halfTaps = 256;

    // Resampling downward has to band-limit to the *output* Nyquist, or
    // everything above it folds back.
    const double cutoff = juce::jmin (1.0, ratio) * 0.98;

    for (int i = 0; i < outputLength; ++i)
    {
        const double sourcePosition = (double) i / ratio;
        const int centre = (int) std::floor (sourcePosition);

        double sum = 0.0;
        double weightSum = 0.0;

        for (int tap = -halfTaps; tap <= halfTaps; ++tap)
        {
            const int index = centre + tap;

            if (! juce::isPositiveAndBelow (index, (int) source.size()))
                continue;

            const double distance = sourcePosition - (double) index;

            // sinc, scaled to the cutoff.
            const double x = juce::MathConstants<double>::pi * distance * cutoff;
            const double sinc = (std::abs (x) < 1.0e-9) ? 1.0 : std::sin (x) / x;

            // Blackman window over the tap range, which keeps the stopband down
            // far enough that the resampling adds nothing audible.
            const double windowPosition = (double) (tap + halfTaps) / (double) (2 * halfTaps);
            const double window = 0.42
                                    - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * windowPosition)
                                    + 0.08 * std::cos (4.0 * juce::MathConstants<double>::pi * windowPosition);

            const double weight = sinc * window;

            sum += (double) source[(size_t) index] * weight;
            weightSum += weight;
        }

        output[(size_t) i] = (float) ((weightSum > 1.0e-9) ? (sum / weightSum * cutoff / juce::jmin (1.0, ratio))
                                                           : 0.0);
    }

    return output;
}

//==============================================================================
bool IrSlot::load (const juce::File& file)
{
    lastError.clear();

    if (! file.existsAsFile())
    {
        lastError = "No such file: " + file.getFullPathName();
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        lastError = "Luthier cannot read " + file.getFileName()
                      + ". IRs must be WAV, AIFF or FLAC.";
        return false;
    }

    // tone-match 1: up to six channels.
    const int numChannels = juce::jlimit (1, 6, (int) reader->numChannels);
    const int length = (int) juce::jmin (reader->lengthInSamples,
                                         (juce::int64) (30.0 * reader->sampleRate));

    if (length <= 0)
    {
        lastError = file.getFileName() + " contains no audio.";
        return false;
    }

    juce::AudioBuffer<float> fileBuffer (numChannels, length);
    reader->read (&fileBuffer, 0, length, 0, true, numChannels > 1);

    rawSampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : sr;

    // ---- pick or sum the channels ----------------------------------------------
    rawSamples.assign ((size_t) length, 0.0f);

    if (channelChoice >= 0 && channelChoice < numChannels)
    {
        const auto* source = fileBuffer.getReadPointer (channelChoice);

        for (int i = 0; i < length; ++i)
            rawSamples[(size_t) i] = source[i];
    }
    else
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto* source = fileBuffer.getReadPointer (channel);

            for (int i = 0; i < length; ++i)
                rawSamples[(size_t) i] += source[i];
        }

        const float scale = 1.0f / (float) numChannels;

        for (float& sample : rawSamples)
            sample *= scale;
    }

    currentFile = file;

    rebuild();

    return true;
}

void IrSlot::unload()
{
    active.store (nullptr, std::memory_order_release);

    retired = std::move (live);
    live.reset();

    rawSamples.clear();
    currentFile = juce::File();
}

void IrSlot::rebuild()
{
    if (rawSamples.empty())
        return;

    auto built = std::make_unique<ImpulseResponse>();

    built->sampleRate = sr;
    built->sourceFile = currentFile;
    built->name = currentFile.getFileNameWithoutExtension();

    // ---- resample to the host's rate (tone-match 0.2) ------------------------------
    auto working = resample (rawSamples, rawSampleRate, sr);

    // ---- trim (tone-match 1) ----------------------------------------------------------
    const int from = juce::jlimit (0, (int) working.size(), startTrim);
    const int to = juce::jlimit (from, (int) working.size(), (int) working.size() - endTrim);

    working = std::vector<float> (working.begin() + from, working.begin() + to);

    // ---- truncate to the maximum length (tone-match 0.3) ------------------------------
    const int maxSamples = juce::jmax (1, (int) (maxSeconds * sr));

    if ((int) working.size() > maxSamples)
    {
        working.resize ((size_t) maxSamples);

        // A hard cut leaves a step at the end, which rings. Fade the last 5%.
        const int fade = juce::jmax (1, maxSamples / 20);

        for (int i = 0; i < fade; ++i)
        {
            const double position = (double) i / (double) fade;
            const double window = 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * position));

            working[(size_t) (maxSamples - fade + i)] =
                (float) ((double) working[(size_t) (maxSamples - fade + i)] * window);
        }
    }

    // ---- reverse (tone-match 1) --------------------------------------------------------
    if (reversed)
        std::reverse (working.begin(), working.end());

    // ---- predelay (tone-match 1) -------------------------------------------------------
    const int predelaySamples = juce::jlimit (0, (int) (0.1 * sr), (int) (predelayMs * 0.001 * sr));

    if (predelaySamples > 0)
        working.insert (working.begin(), (size_t) predelaySamples, 0.0f);

    built->samples = std::move (working);
    built->normalise();

    if (built->samples.empty())
        return;

    // ---- hand it to the audio thread --------------------------------------------------
    if (convolution != nullptr)
    {
        juce::AudioBuffer<float> irBuffer (1, (int) built->samples.size());

        std::copy (built->samples.begin(), built->samples.end(), irBuffer.getWritePointer (0));

        convolution->loadImpulseResponse (std::move (irBuffer), sr,
                                          juce::dsp::Convolution::Stereo::yes,
                                          juce::dsp::Convolution::Trim::no,
                                          juce::dsp::Convolution::Normalise::no);
    }

    /*  The swap.

        The previous response is kept in `retired` rather than freed, because the
        audio thread may still be inside a block that is reading it. It is freed
        on the *next* swap, by which time many blocks have gone by. That is the
        whole of the lock-free handover rule 1 of section 0 asks for. */
    retired = std::move (live);
    live = std::move (built);

    active.store (live.get(), std::memory_order_release);
}

//==============================================================================
void IrSlot::setChannel (int channel)
{
    channelChoice = juce::jlimit (-1, 5, channel);

    // The channel choice is applied when the file is read, so it needs a reload.
    if (currentFile.existsAsFile())
        load (currentFile);
}

void IrSlot::setGainTrimDb (double db) noexcept
{
    gainTrimDb.store (juce::jlimit (-24.0, 24.0, db), std::memory_order_relaxed);
}

void IrSlot::setStartTrim (int samples)
{
    startTrim = juce::jmax (0, samples);
    rebuild();
}

void IrSlot::setEndTrim (int samples)
{
    endTrim = juce::jmax (0, samples);
    rebuild();
}

void IrSlot::setPredelayMs (double ms)
{
    predelayMs = juce::jlimit (0.0, 100.0, ms);
    rebuild();
}

void IrSlot::setReversed (bool shouldReverse)
{
    reversed = shouldReverse;
    rebuild();
}

void IrSlot::setMix (double mix) noexcept
{
    mixAmount.store (juce::jlimit (0.0, 1.0, mix), std::memory_order_relaxed);
}

void IrSlot::setMaxSeconds (double seconds)
{
    maxSeconds = juce::jlimit (0.05, 10.0, seconds);
    rebuild();
}

int IrSlot::getLatencySamples() const noexcept
{
    // tone-match 0.4: a user IR reports the same latency as the built-in
    // cabinet convolution, whatever its own length. The convolution runs in
    // zero-latency mode, so there is nothing to add.
    return 0;
}

void IrSlot::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (! isEngaged() || convolution == nullptr || channels == nullptr
          || numChannels < 1 || numSamples <= 0)
        return;

    const double mix = mixAmount.load (std::memory_order_relaxed);

    if (mix <= 0.0)
        return;

    const int channelsToUse = juce::jmin (numChannels, wetBuffer.getNumChannels());
    const int samplesToUse = juce::jmin (numSamples, wetBuffer.getNumSamples());

    if (channelsToUse < 1 || samplesToUse < 1)
        return;

    // The wet path is convolved in its own buffer, so the dry is still available
    // to blend against.
    for (int channel = 0; channel < channelsToUse; ++channel)
        wetBuffer.copyFrom (channel, 0, channels[channel], samplesToUse);

    {
        juce::dsp::AudioBlock<float> block (wetBuffer.getArrayOfWritePointers(),
                                            (size_t) channelsToUse, (size_t) samplesToUse);

        juce::dsp::ProcessContextReplacing<float> context (block);
        convolution->process (context);
    }

    const double gain = dbToGain (gainTrimDb.load (std::memory_order_relaxed));

    for (int channel = 0; channel < channelsToUse; ++channel)
    {
        auto* destination = channels[channel];
        const auto* wet = wetBuffer.getReadPointer (channel);

        for (int i = 0; i < samplesToUse; ++i)
        {
            const double dry = (double) destination[i];
            const double processed = (double) wet[i] * gain;

            destination[i] = (float) sanitise (dry * (1.0 - mix) + processed * mix);
        }
    }
}

void IrSlot::processReplacing (const float* input, float* modelOutput, int numSamples) noexcept
{
    if (! isEngaged() || convolution == nullptr || input == nullptr || modelOutput == nullptr || numSamples <= 0)
        return;

    const double mix = mixAmount.load (std::memory_order_relaxed);

    if (mix <= 0.0)
        return;

    const int samplesToUse = juce::jmin (numSamples, wetBuffer.getNumSamples());

    if (samplesToUse < 1)
        return;

    wetBuffer.copyFrom (0, 0, input, samplesToUse);

    {
        float* channels[] = { wetBuffer.getWritePointer (0) };
        juce::dsp::AudioBlock<float> block (channels, 1, (size_t) samplesToUse);
        juce::dsp::ProcessContextReplacing<float> context (block);
        convolution->process (context);
    }

    const double gain = dbToGain (gainTrimDb.load (std::memory_order_relaxed));
    const auto* wet = wetBuffer.getReadPointer (0);

    for (int i = 0; i < samplesToUse; ++i)
        modelOutput[i] = (float) sanitise ((double) modelOutput[i] * (1.0 - mix) + (double) wet[i] * gain * mix);
}

//==============================================================================
juce::var IrSlot::toVar() const
{
    auto* object = new juce::DynamicObject();

    // tone-match 7: relative to the IR root when it lives there, absolute
    // otherwise.
    object->setProperty ("file", IrLibraryPaths::toPresetPath (currentFile));
    object->setProperty ("engaged", engaged.load (std::memory_order_relaxed));
    object->setProperty ("channel", channelChoice);
    object->setProperty ("gainTrimDb", getGainTrimDb());
    object->setProperty ("startTrim", startTrim);
    object->setProperty ("endTrim", endTrim);
    object->setProperty ("predelayMs", predelayMs);
    object->setProperty ("reversed", reversed);
    object->setProperty ("mix", getMix());
    object->setProperty ("maxSeconds", maxSeconds);

    return { object };
}

void IrSlot::fromVar (const juce::var& state)
{
    auto* object = state.getDynamicObject();

    if (object == nullptr)
        return;

    channelChoice = juce::jlimit (-1, 5, (int) object->getProperty ("channel"));
    startTrim     = juce::jmax (0, (int) object->getProperty ("startTrim"));
    endTrim       = juce::jmax (0, (int) object->getProperty ("endTrim"));
    predelayMs    = juce::jlimit (0.0, 100.0, (double) object->getProperty ("predelayMs"));
    reversed      = (bool) object->getProperty ("reversed");

    if (object->hasProperty ("maxSeconds"))
        maxSeconds = juce::jlimit (0.05, 10.0, (double) object->getProperty ("maxSeconds"));

    setGainTrimDb ((double) object->getProperty ("gainTrimDb"));
    setMix (object->hasProperty ("mix") ? (double) object->getProperty ("mix") : 1.0);

    const auto path = object->getProperty ("file").toString();

    if (path.isNotEmpty())
    {
        const auto file = IrLibraryPaths::fromPresetPath (path);

        if (file.existsAsFile())
        {
            load (file);
            setEngaged ((bool) object->getProperty ("engaged"));
            return;
        }

        /*  tone-match 7: a missing IR falls back to the built-in model with a
            warning rather than failing to load the preset. The error is left set
            so the header can show the banner the spec asks for. */
        lastError = "The preset's IR is missing: " + path
                      + ". The built-in cabinet is being used instead.";
    }

    setEngaged (false);
}

//==============================================================================
void Capture::prepare (double sampleRate, double maxSeconds)
{
    sr = juce::jmax (1.0, sampleRate);

    capacity = (int) (juce::jlimit (kMinSeconds, kMaxSeconds, maxSeconds) * sr);

    buffer.setSize (2, juce::jmax (1, capacity), false, true, false);
    buffer.clear();

    reset();
}

void Capture::reset() noexcept
{
    recording.store (false, std::memory_order_relaxed);
    complete.store (false, std::memory_order_relaxed);
    recorded.store (0, std::memory_order_relaxed);
    wanted = capacity;
}

void Capture::start (double seconds) noexcept
{
    wanted = juce::jlimit (1, capacity, (int) (seconds * sr));

    recorded.store (0, std::memory_order_relaxed);
    complete.store (false, std::memory_order_relaxed);
    recording.store (true, std::memory_order_relaxed);
}

void Capture::stop() noexcept
{
    recording.store (false, std::memory_order_relaxed);

    if (recorded.load (std::memory_order_relaxed) > 0)
        complete.store (true, std::memory_order_relaxed);
}

double Capture::getProgress() const noexcept
{
    return (wanted > 0)
             ? juce::jlimit (0.0, 1.0, (double) recorded.load (std::memory_order_relaxed)
                                         / (double) wanted)
             : 0.0;
}

void Capture::processBlock (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (! isRecording() || channels == nullptr || numChannels < 1 || capacity <= 0)
        return;

    int position = recorded.load (std::memory_order_relaxed);

    const int count = juce::jmin (numSamples, wanted - position);

    if (count <= 0)
    {
        recording.store (false, std::memory_order_relaxed);
        complete.store (true, std::memory_order_relaxed);
        return;
    }

    // A copy into a buffer that already exists: nothing here allocates.
    for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
        buffer.copyFrom (channel, position, channels[channel], count);

    if (numChannels == 1)
        buffer.copyFrom (1, position, channels[0], count);

    position += count;
    recorded.store (position, std::memory_order_relaxed);

    if (position >= wanted)
    {
        recording.store (false, std::memory_order_relaxed);
        complete.store (true, std::memory_order_relaxed);
    }
}

void Capture::autoTrim (double thresholdDb)
{
    const int length = recorded.load (std::memory_order_relaxed);

    if (length <= 0)
        return;

    const double threshold = dbToGain (thresholdDb);

    int first = 0;

    while (first < length
             && std::abs (buffer.getSample (0, first)) < threshold
             && std::abs (buffer.getSample (1, first)) < threshold)
        ++first;

    int last = length - 1;

    while (last > first
             && std::abs (buffer.getSample (0, last)) < threshold
             && std::abs (buffer.getSample (1, last)) < threshold)
        --last;

    const int trimmed = last - first + 1;

    // Nothing to trim at either end. (Returning whenever the start had no
    // silence skipped the tail trim too.)
    if (trimmed <= 0 || trimmed == length)
        return;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        if (first > 0)
            std::memmove (data, data + first, (size_t) trimmed * sizeof (float));

        juce::FloatVectorOperations::clear (data + trimmed, length - trimmed);
    }

    recorded.store (trimmed, std::memory_order_relaxed);
}

bool Capture::save (const juce::File& file) const
{
    const int length = recorded.load (std::memory_order_relaxed);

    if (length <= 0)
        return false;

    file.getParentDirectory().createDirectory();
    file.deleteFile();

    juce::WavAudioFormat format;

    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

    if (stream == nullptr)
        return false;

    // tone-match 4: 32-bit float, so a capture is never clipped by its own file.
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.get(), sr, 2, 32, {}, 0));

    if (writer == nullptr)
        return false;

    stream.release();

    return writer->writeFromAudioSampleBuffer (buffer, 0, length);
}

juce::File Capture::getCaptureDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Captures");
}

//==============================================================================
const char* CabMatch::getTestSignalName (TestSignal signal) noexcept
{
    switch (signal)
    {
        case TestSignal::sineSweep:      return "Exponential Sine Sweep";
        case TestSignal::mls:            return "MLS";
        case TestSignal::transientBurst: return "Transient Burst";
        case TestSignal::numSignals:
        default:                         return "Exponential Sine Sweep";
    }
}

std::vector<float> CabMatch::generateTestSignal (TestSignal signal, double sampleRate,
                                                 double seconds)
{
    const double sr = juce::jmax (1.0, sampleRate);

    switch (signal)
    {
        case TestSignal::sineSweep:
        {
            /*  An exponential sweep from 20 Hz to 20 kHz (tone-match 2).

                Exponential rather than linear because it deconvolves cleanly:
                the harmonic distortion an amp adds arrives *before* the linear
                response in the deconvolved result, so it can be windowed off.
                A linear sweep smears it across the whole impulse. */
            const int length = juce::jmax (1, (int) (seconds * sr));

            std::vector<float> output ((size_t) length, 0.0f);

            const double f1 = 20.0;
            const double f2 = juce::jmin (20000.0, sr * 0.45);
            const double duration = (double) length / sr;

            const double k = std::log (f2 / f1);

            for (int i = 0; i < length; ++i)
            {
                const double t = (double) i / sr;
                const double phase = 2.0 * juce::MathConstants<double>::pi * f1 * duration / k
                                       * (std::exp (t / duration * k) - 1.0);

                double sample = std::sin (phase);

                // Fade the ends, so the sweep neither clicks on nor off.
                const int fade = juce::jmax (1, (int) (0.02 * sr));

                if (i < fade)
                    sample *= (double) i / (double) fade;
                else if (i > length - fade)
                    sample *= (double) (length - i) / (double) fade;

                output[(size_t) i] = (float) (sample * 0.5);
            }

            return output;
        }

        case TestSignal::mls:
        {
            // A maximum length sequence: a pseudo-random binary sequence whose
            // autocorrelation is an impulse, which is what makes it usable here.
            constexpr int order = 17;                    // about 2.7 s at 48 kHz
            const int length = (1 << order) - 1;

            std::vector<float> output ((size_t) length, 0.0f);

            uint32_t state = 1;

            for (int i = 0; i < length; ++i)
            {
                // Taps for order 17: x^17 + x^14 + 1.
                const uint32_t bit = ((state >> 16) ^ (state >> 13)) & 1u;

                state = ((state << 1) | bit) & ((1u << order) - 1u);

                output[(size_t) i] = (state & 1u) ? 0.5f : -0.5f;
            }

            return output;
        }

        case TestSignal::transientBurst:
        {
            /*  tone-match 2's third option: a burst library that biases the match
                toward the frequencies a guitar actually produces.

                A string's spectrum is concentrated between about 80 Hz and 5 kHz,
                so the burst is a series of filtered impulses across that range
                rather than flat noise. What comes out is an IR fitted where it
                matters and left approximate where nothing plays. */
            const int length = juce::jmax (1, (int) (seconds * sr));

            std::vector<float> output ((size_t) length, 0.0f);

            RtRandom random (0xb0f5e7);

            const double frequencies[] = { 82.0, 110.0, 165.0, 220.0, 330.0,
                                           440.0, 660.0, 880.0, 1320.0, 2640.0, 5280.0 };

            const int numBursts = (int) (sizeof (frequencies) / sizeof (frequencies[0]));
            const int burstLength = length / juce::jmax (1, numBursts);

            for (int burst = 0; burst < numBursts; ++burst)
            {
                const double frequency = frequencies[burst];
                const int start = burst * burstLength;

                for (int i = 0; i < burstLength && start + i < length; ++i)
                {
                    const double t = (double) i / sr;

                    // A plucked-string envelope, so the burst has a transient.
                    const double envelope = std::exp (-t * 12.0);

                    double sample = std::sin (2.0 * juce::MathConstants<double>::pi * frequency * t);

                    // A little noise, so the burst excites between the harmonics.
                    sample += (random.nextDouble() * 2.0 - 1.0) * 0.08;

                    output[(size_t) (start + i)] = (float) (sample * envelope * 0.5);
                }
            }

            return output;
        }

        case TestSignal::numSignals:
        default:
            return {};
    }
}

//==============================================================================
namespace
{
    /** The next power of two at or above n. */
    int nextPowerOfTwo (int n)
    {
        int result = 1;

        while (result < n)
            result <<= 1;

        return result;
    }

    /** Forward FFT of a real signal into a complex spectrum of size `fftSize`. */
    void forwardFft (const std::vector<float>& input, int fftSize,
                     std::vector<std::complex<float>>& output)
    {
        juce::dsp::FFT fft ((int) std::log2 (fftSize));

        std::vector<float> buffer ((size_t) fftSize * 2, 0.0f);

        const int count = juce::jmin ((int) input.size(), fftSize);

        for (int i = 0; i < count; ++i)
            buffer[(size_t) i] = input[(size_t) i];

        fft.performRealOnlyForwardTransform (buffer.data(), true);

        output.assign ((size_t) (fftSize / 2 + 1), {});

        for (int bin = 0; bin <= fftSize / 2; ++bin)
            output[(size_t) bin] = { buffer[(size_t) (bin * 2)],
                                     buffer[(size_t) (bin * 2 + 1)] };
    }

    /** Inverse FFT of a half-spectrum back to a real signal. */
    void inverseFft (const std::vector<std::complex<float>>& spectrum, int fftSize,
                     std::vector<float>& output)
    {
        juce::dsp::FFT fft ((int) std::log2 (fftSize));

        std::vector<float> buffer ((size_t) fftSize * 2, 0.0f);

        for (int bin = 0; bin <= fftSize / 2 && bin < (int) spectrum.size(); ++bin)
        {
            buffer[(size_t) (bin * 2)] = spectrum[(size_t) bin].real();
            buffer[(size_t) (bin * 2 + 1)] = spectrum[(size_t) bin].imag();
        }

        fft.performRealOnlyInverseTransform (buffer.data());

        output.assign ((size_t) fftSize, 0.0f);

        for (int i = 0; i < fftSize; ++i)
            output[(size_t) i] = buffer[(size_t) i];
    }
}

ImpulseResponse CabMatch::deconvolve (const std::vector<float>& testSignal,
                                      const std::vector<float>& response,
                                      double sampleRate,
                                      TestSignal signal,
                                      double maxSeconds)
{
    ImpulseResponse result;
    result.sampleRate = sampleRate;
    result.name = "Cab Match";

    if (testSignal.empty() || response.empty())
        return result;

    const int fftSize = nextPowerOfTwo ((int) juce::jmax (testSignal.size(), response.size()) * 2);

    std::vector<std::complex<float>> testSpectrum, responseSpectrum;

    forwardFft (testSignal, fftSize, testSpectrum);
    forwardFft (response, fftSize, responseSpectrum);

    std::vector<std::complex<float>> irSpectrum ((size_t) (fftSize / 2 + 1), std::complex<float> {});

    /*  Division in the frequency domain, regularised.

        The test signal has almost no energy at some frequencies - below 20 Hz,
        above Nyquist, and in the gaps of a burst - and dividing by a near-zero
        bin turns measurement noise into an enormous spike. The regularisation
        floor scales with the signal's own peak magnitude, so it adapts to the
        level rather than being a fixed number that is wrong at every other gain.
    */
    double peakMagnitude = 0.0;

    for (const auto& bin : testSpectrum)
        peakMagnitude = juce::jmax (peakMagnitude, (double) std::abs (bin));

    const double epsilon = juce::jmax (1.0e-9, peakMagnitude * 1.0e-4);

    juce::ignoreUnused (signal);

    for (size_t bin = 0; bin < irSpectrum.size(); ++bin)
    {
        const auto test = testSpectrum[bin];
        const auto measured = responseSpectrum[bin];

        const double magnitudeSquared = (double) (test.real() * test.real()
                                                    + test.imag() * test.imag());

        // Wiener-style: multiply by the conjugate and divide by the magnitude
        // squared plus the floor, which is a division everywhere the signal is
        // strong and a gentle roll-off everywhere it is not.
        const double denominator = magnitudeSquared + epsilon * epsilon;

        const std::complex<float> conjugate (test.real(), -test.imag());
        const auto numerator = measured * conjugate;

        irSpectrum[bin] = { (float) ((double) numerator.real() / denominator),
                            (float) ((double) numerator.imag() / denominator) };
    }

    std::vector<float> impulse;
    inverseFft (irSpectrum, fftSize, impulse);

    // ---- trim and window (tone-match 2) ---------------------------------------------
    const int maxSamples = juce::jmax (1, (int) (maxSeconds * sampleRate));
    const int length = juce::jmin (maxSamples, (int) impulse.size());

    result.samples.assign (impulse.begin(), impulse.begin() + length);

    // A Hann fade-out over the last 5%, exactly as the spec asks.
    const int fade = juce::jmax (1, length / 20);

    for (int i = 0; i < fade; ++i)
    {
        const double position = (double) i / (double) fade;
        const double window = 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * position));

        result.samples[(size_t) (length - fade + i)] =
            (float) ((double) result.samples[(size_t) (length - fade + i)] * window);
    }

    result.normalise();

    return result;
}

double CabMatch::measureNull (const std::vector<float>& reference,
                              const std::vector<float>& matched)
{
    const int length = (int) juce::jmin (reference.size(), matched.size());

    if (length <= 0)
        return 0.0;

    double referenceEnergy = 0.0;
    double differenceEnergy = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const double a = (double) reference[(size_t) i];
        const double b = (double) matched[(size_t) i];

        referenceEnergy += a * a;
        differenceEnergy += (a - b) * (a - b);
    }

    if (referenceEnergy <= 1.0e-18)
        return 0.0;

    return 10.0 * std::log10 (juce::jmax (1.0e-18, differenceEnergy / referenceEnergy));
}

juce::File CabMatch::getMatchDirectory()
{
    return IrLibraryPaths::getCabinetsMatch();
}

bool CabMatch::saveIr (const ImpulseResponse& ir, const juce::File& file,
                       const IrMetadata& metadata)
{
    if (ir.isEmpty())
        return false;

    file.getParentDirectory().createDirectory();
    file.deleteFile();

    juce::WavAudioFormat format;

    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

    if (stream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.get(), ir.sampleRate, 1, 24, {}, 0));

    if (writer == nullptr)
        return false;

    stream.release();

    juce::AudioBuffer<float> buffer (1, ir.getLength());
    std::copy (ir.samples.begin(), ir.samples.end(), buffer.getWritePointer (0));

    if (! writer->writeFromAudioSampleBuffer (buffer, 0, ir.getLength()))
        return false;

    writer.reset();

    auto sidecar = metadata;
    sidecar.sampleRate = ir.sampleRate;
    sidecar.lengthMs = ir.getLengthMs();

    if (sidecar.name.isEmpty())
        sidecar.name = file.getFileNameWithoutExtension();

    sidecar.saveFor (file);

    return true;
}

//==============================================================================
int EqMatch::getTapCount (FilterLength length) noexcept
{
    switch (length)
    {
        case FilterLength::short256:  return 256;
        case FilterLength::medium1024: return 1024;
        case FilterLength::long4096:  return 4096;
        case FilterLength::numLengths:
        default:                      return 1024;
    }
}

const char* EqMatch::getDescription() noexcept
{
    return "EQ match fits Luthier's long-term spectrum to a reference. It is not "
           "a substitute for cab match: matching a magnitude spectrum reproduces "
           "tone but not time, so a cabinet's ringing and its early reflections "
           "are not captured.";
}

std::vector<double> EqMatch::measureSpectrum (const std::vector<float>& signal,
                                              double sampleRate, int numBins)
{
    std::vector<double> spectrum ((size_t) juce::jmax (1, numBins), 0.0);

    if (signal.empty() || sampleRate <= 0.0)
        return spectrum;

    constexpr int fftSize = 4096;
    constexpr int hop = fftSize / 2;

    juce::dsp::FFT fft ((int) std::log2 (fftSize));

    std::vector<float> window ((size_t) fftSize);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = (float) (0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi
                                                               * (double) i / (double) (fftSize - 1))));

    std::vector<double> accumulated ((size_t) (fftSize / 2 + 1), 0.0);
    int windowCount = 0;

    std::vector<float> buffer ((size_t) fftSize * 2, 0.0f);

    for (int start = 0; start + fftSize <= (int) signal.size(); start += hop)
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);

        for (int i = 0; i < fftSize; ++i)
            buffer[(size_t) i] = signal[(size_t) (start + i)] * window[(size_t) i];

        fft.performFrequencyOnlyForwardTransform (buffer.data(), true);

        for (int bin = 0; bin <= fftSize / 2; ++bin)
            accumulated[(size_t) bin] += (double) buffer[(size_t) bin];

        ++windowCount;
    }

    if (windowCount == 0)
        return spectrum;

    for (auto& value : accumulated)
        value /= (double) windowCount;

    /*  Fold the linear bins into log-spaced bands.

        Hearing is logarithmic in frequency, and so is every tone control anyone
        would want to match: a linear-bin fit would spend most of its resolution
        above 10 kHz, where almost nothing about a guitar cabinet lives. */
    const double lowHz = 20.0;
    const double highHz = juce::jmin (20000.0, sampleRate * 0.49);

    const double binWidth = sampleRate / (double) fftSize;

    for (int band = 0; band < numBins; ++band)
    {
        const double from = lowHz * std::pow (highHz / lowHz, (double) band / (double) numBins);
        const double to = lowHz * std::pow (highHz / lowHz, (double) (band + 1) / (double) numBins);

        const int firstBin = juce::jlimit (0, fftSize / 2, (int) (from / binWidth));
        const int lastBin = juce::jlimit (firstBin, fftSize / 2, (int) (to / binWidth));

        double sum = 0.0;
        int count = 0;

        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            sum += accumulated[(size_t) bin];
            ++count;
        }

        spectrum[(size_t) band] = (count > 0) ? (sum / (double) count) : 0.0;
    }

    return spectrum;
}

ImpulseResponse EqMatch::fit (const std::vector<float>& reference,
                              const std::vector<float>& current,
                              double sampleRate,
                              const Options& options)
{
    ImpulseResponse result;
    result.sampleRate = sampleRate;
    result.name = "EQ Match";

    const int taps = getTapCount (options.length);

    if (reference.empty() || current.empty())
        return result;

    constexpr int numBins = 96;

    const auto referenceSpectrum = measureSpectrum (reference, sampleRate, numBins);
    const auto currentSpectrum = measureSpectrum (current, sampleRate, numBins);

    // ---- the correction, in dB per band --------------------------------------------
    std::vector<double> correctionDb ((size_t) numBins, 0.0);

    const double lowHz = 20.0;
    const double highHz = juce::jmin (20000.0, sampleRate * 0.49);

    double averageCorrection = 0.0;
    int correctedBands = 0;

    for (int band = 0; band < numBins; ++band)
    {
        const double centre = lowHz * std::pow (highHz / lowHz,
                                                ((double) band + 0.5) / (double) numBins);

        // Outside the requested band, no correction at all.
        if (centre < options.lowHz || centre > options.highHz)
            continue;

        const double referenceLevel = juce::jmax (1.0e-9, referenceSpectrum[(size_t) band]);
        const double currentLevel = juce::jmax (1.0e-9, currentSpectrum[(size_t) band]);

        double difference = 20.0 * std::log10 (referenceLevel / currentLevel);

        // A band where either signal is essentially silent tells us nothing, and
        // correcting it would apply enormous gain to noise.
        if (referenceLevel < 1.0e-6 || currentLevel < 1.0e-6)
            difference = 0.0;

        // Clamped, so one dead band cannot ask for 60 dB of boost.
        difference = juce::jlimit (-24.0, 24.0, difference);

        correctionDb[(size_t) band] = difference * juce::jlimit (0.0, 1.0, options.aggressiveness);

        averageCorrection += correctionDb[(size_t) band];
        ++correctedBands;
    }

    // tone-match 3: preserve dynamics corrects the shape only, so the average
    // gain is taken back out.
    if (options.preserveDynamics && correctedBands > 0)
    {
        averageCorrection /= (double) correctedBands;

        for (auto& value : correctionDb)
            if (value != 0.0)
                value -= averageCorrection;
    }

    // ---- build the filter ------------------------------------------------------------
    const int fftSize = nextPowerOfTwo (taps * 4);

    std::vector<std::complex<float>> spectrum ((size_t) (fftSize / 2 + 1), std::complex<float> {});

    const double binWidth = sampleRate / (double) fftSize;

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        const double frequency = (double) bin * binWidth;

        // Read the correction curve at this frequency, interpolating between
        // bands so the filter is smooth rather than stepped.
        double gainDb = 0.0;

        if (frequency >= lowHz && frequency <= highHz)
        {
            const double position = std::log (frequency / lowHz) / std::log (highHz / lowHz)
                                      * (double) numBins - 0.5;

            const int lower = juce::jlimit (0, numBins - 1, (int) std::floor (position));
            const int upper = juce::jlimit (0, numBins - 1, lower + 1);
            const double fraction = juce::jlimit (0.0, 1.0, position - (double) lower);

            gainDb = correctionDb[(size_t) lower]
                       + (correctionDb[(size_t) upper] - correctionDb[(size_t) lower]) * fraction;
        }

        const double magnitude = dbToGain (gainDb);

        spectrum[(size_t) bin] = { (float) magnitude, 0.0f };
    }

    /*  Make it minimum phase.

        A zero-phase filter built by inverse-transforming a magnitude is
        symmetrical around zero, so half of it is at negative time: applying it
        directly would smear transients backwards, which is audible as pre-ring
        on a pick attack. The cepstral method folds the non-causal half onto the
        causal one, which gives the same magnitude with all the energy at or
        after time zero.
    */
    {
        std::vector<std::complex<float>> logSpectrum ((size_t) (fftSize / 2 + 1), std::complex<float> {});

        for (size_t bin = 0; bin < logSpectrum.size(); ++bin)
            logSpectrum[bin] = { (float) std::log (juce::jmax (1.0e-9,
                                                               (double) spectrum[bin].real())),
                                 0.0f };

        std::vector<float> cepstrum;
        inverseFft (logSpectrum, fftSize, cepstrum);

        // Fold: double the causal half, keep the ends, zero the rest.
        for (int i = 1; i < fftSize / 2; ++i)
        {
            cepstrum[(size_t) i] *= 2.0f;
            cepstrum[(size_t) (fftSize - i)] = 0.0f;
        }

        std::vector<std::complex<float>> foldedSpectrum;
        forwardFft (cepstrum, fftSize, foldedSpectrum);

        for (size_t bin = 0; bin < spectrum.size() && bin < foldedSpectrum.size(); ++bin)
        {
            const auto value = std::exp (std::complex<double> (foldedSpectrum[bin].real(),
                                                               foldedSpectrum[bin].imag()));

            spectrum[bin] = { (float) value.real(), (float) value.imag() };
        }
    }

    std::vector<float> impulse;
    inverseFft (spectrum, fftSize, impulse);

    result.samples.assign (impulse.begin(),
                           impulse.begin() + juce::jmin (taps, (int) impulse.size()));

    // Window the tail, so truncating to `taps` does not ripple the response.
    const int fade = juce::jmax (1, (int) result.samples.size() / 8);
    const int length = (int) result.samples.size();

    for (int i = 0; i < fade; ++i)
    {
        const double position = (double) i / (double) fade;
        const double window = 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * position));

        result.samples[(size_t) (length - fade + i)] =
            (float) ((double) result.samples[(size_t) (length - fade + i)] * window);
    }

    return result;
}

double EqMatch::magnitudeAt (const ImpulseResponse& ir, double frequencyHz, double sampleRate)
{
    if (ir.isEmpty() || sampleRate <= 0.0)
        return 0.0;

    // A direct DFT at one frequency: cheaper than a whole FFT when the caller
    // wants a handful of points for a curve display.
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequencyHz / sampleRate;

    double real = 0.0, imaginary = 0.0;

    for (size_t i = 0; i < ir.samples.size(); ++i)
    {
        const double angle = omega * (double) i;

        real += (double) ir.samples[i] * std::cos (angle);
        imaginary -= (double) ir.samples[i] * std::sin (angle);
    }

    return 20.0 * std::log10 (juce::jmax (1.0e-9, std::sqrt (real * real + imaginary * imaginary)));
}

//==============================================================================
juce::File IrLibraryPaths::getRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("IRs");
}

juce::File IrLibraryPaths::getBodies()          { return getRoot().getChildFile ("Bodies"); }
juce::File IrLibraryPaths::getBodiesAcoustic()  { return getBodies().getChildFile ("Acoustic"); }
juce::File IrLibraryPaths::getBodiesElectric()  { return getBodies().getChildFile ("Electric"); }
juce::File IrLibraryPaths::getCabinets()        { return getRoot().getChildFile ("Cabinets"); }
juce::File IrLibraryPaths::getCabinetsUser()    { return getCabinets().getChildFile ("User"); }
juce::File IrLibraryPaths::getCabinetsMatch()   { return getCabinets().getChildFile ("Cab Match"); }
juce::File IrLibraryPaths::getRooms()           { return getRoot().getChildFile ("Rooms"); }
juce::File IrLibraryPaths::getSpecial()         { return getRoot().getChildFile ("Special"); }

void IrLibraryPaths::ensureExists()
{
    for (const auto& folder : { getBodiesAcoustic(), getBodiesElectric(),
                                getCabinetsUser(), getCabinetsMatch(),
                                getRooms(), getSpecial() })
        folder.createDirectory();
}

juce::Array<juce::File> IrLibraryPaths::findAll()
{
    juce::Array<juce::File> found;

    const auto root = getRoot();

    if (! root.isDirectory())
        return found;

    for (const auto& entry : juce::RangedDirectoryIterator (root, true, "*.wav;*.aif;*.aiff;*.flac"))
        found.add (entry.getFile());

    return found;
}

juce::String IrLibraryPaths::toPresetPath (const juce::File& file)
{
    if (file == juce::File())
        return {};

    const auto root = getRoot();

    // tone-match 7: relative when it lives under the registered root, so a
    // preset shared between machines still finds its IR.
    if (file.isAChildOf (root))
        return file.getRelativePathFrom (root).replaceCharacter ('\\', '/');

    return file.getFullPathName();
}

juce::File IrLibraryPaths::fromPresetPath (const juce::String& path)
{
    if (path.isEmpty())
        return {};

    if (juce::File::isAbsolutePath (path))
        return juce::File (path);

    return getRoot().getChildFile (path);
}

} // namespace luthier
