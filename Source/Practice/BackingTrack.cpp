#include "BackingTrack.h"

namespace luthier
{

//==============================================================================
juce::var TrackMarker::toVar() const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("name", name);
    object->setProperty ("seconds", seconds);

    return { object };
}

TrackMarker TrackMarker::fromVar (const juce::var& state)
{
    TrackMarker marker;

    if (auto* object = state.getDynamicObject())
    {
        marker.name = object->getProperty ("name").toString();
        marker.seconds = juce::jmax (0.0, (double) object->getProperty ("seconds"));
    }

    return marker;
}

//==============================================================================
BackingTrackPlayer::BackingTrackPlayer()
{
    formats.registerBasicFormats();
    readerThread.startThread (juce::Thread::Priority::normal);
}

BackingTrackPlayer::~BackingTrackPlayer()
{
    unload();
    readerThread.stopThread (2000);
}

void BackingTrackPlayer::prepare (double sampleRate, int maxBlockSize)
{
    sr = juce::jmax (1.0, sampleRate);
    blockSize = juce::jmax (1, maxBlockSize);

    scratch.setSize (2, blockSize, false, true, false);
    scratch.clear();

    if (transport != nullptr)
        transport->prepareToPlay (blockSize, sr);

    lastLowCut = -1.0;
    lastHighCut = -1.0;

    shifter.prepare();   // SPEC-SWEEP PT-28/29
    shifterActive = false;

    reset();
}

void BackingTrackPlayer::reset() noexcept
{
    lowCutL.reset();  lowCutR.reset();
    highCutL.reset(); highCutR.reset();
}

//==============================================================================
bool BackingTrackPlayer::load (const juce::File& file)
{
    unload();

    if (! file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return false;

    fileSampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : sr;
    lengthSeconds = (double) reader->lengthInSamples / fileSampleRate;

    /*  A short window off the front of the file, kept resident.

        Zero-crossing snapping and tempo estimation both need to look at samples,
        and neither can afford to seek around a streaming reader while the audio
        thread is reading from it. Thirty seconds of mono at the file's own rate
        is a few megabytes, which is nothing next to the whole file, and is
        enough for both jobs. */
    {
        const int analysisSamples = (int) juce::jmin ((juce::int64) (30.0 * fileSampleRate),
                                                      reader->lengthInSamples);

        analysisSampleRate = fileSampleRate;

        if (analysisSamples > 0)
        {
            analysisBuffer.setSize (1, analysisSamples, false, true, false);
            analysisBuffer.clear();

            juce::AudioBuffer<float> temp ((int) juce::jmin ((int64_t) 2, (int64_t) reader->numChannels),
                                           analysisSamples);
            reader->read (&temp, 0, analysisSamples, 0, true, temp.getNumChannels() > 1);

            analysisBuffer.copyFrom (0, 0, temp, 0, 0, analysisSamples);

            if (temp.getNumChannels() > 1)
            {
                analysisBuffer.addFrom (0, 0, temp, 1, 0, analysisSamples);
                analysisBuffer.applyGain (0.5f);
            }
        }
    }

    auto newReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);

    // practice-tools 0.4 and 3: the file streams from disk behind a four-second
    // ring, filled by its own thread. Nothing loads the file whole.
    auto newBufferingSource = std::make_unique<juce::BufferingAudioSource> (
        newReaderSource.get(), readerThread, false,
        (int) (kRingBufferSeconds * fileSampleRate), 2, false);

    auto newTransport = std::make_unique<juce::AudioTransportSource>();
    newTransport->setSource (newBufferingSource.get(), 0, nullptr, fileSampleRate, 2);
    newTransport->prepareToPlay (blockSize, sr);

    {
        const juce::SpinLock::ScopedLockType sl (transportLock);
        readerSource = std::move (newReaderSource);
        bufferingSource = std::move (newBufferingSource);
        transport = std::move (newTransport);
    }

    currentFile = file;
    loaded.store (true, std::memory_order_relaxed);
    shifterResetPending.store (true, std::memory_order_relaxed);   // SPEC-SWEEP PT-28

    markers.clear();

    loopStart.store (0.0, std::memory_order_relaxed);
    loopEnd.store (lengthSeconds, std::memory_order_relaxed);

    detectedTempo = estimateTempo();

    return true;
}

void BackingTrackPlayer::unload()
{
    playing.store (false, std::memory_order_relaxed);
    loaded.store (false, std::memory_order_relaxed);

    // Taken out under the lock, so the audio thread is never inside them when
    // they are destroyed, and torn down after it is released.
    std::unique_ptr<juce::AudioTransportSource> oldTransport;
    std::unique_ptr<juce::BufferingAudioSource> oldBufferingSource;
    std::unique_ptr<juce::AudioFormatReaderSource> oldReaderSource;

    {
        const juce::SpinLock::ScopedLockType sl (transportLock);
        oldTransport = std::move (transport);
        oldBufferingSource = std::move (bufferingSource);
        oldReaderSource = std::move (readerSource);
    }

    if (oldTransport != nullptr)
    {
        oldTransport->stop();
        oldTransport->setSource (nullptr);
        oldTransport->releaseResources();
    }

    oldTransport.reset();
    oldBufferingSource.reset();
    oldReaderSource.reset();

    currentFile = juce::File();
    lengthSeconds = 0.0;
    detectedTempo = 0.0;

    analysisBuffer.setSize (1, 0);
    markers.clear();
}

//==============================================================================
void BackingTrackPlayer::play() noexcept
{
    if (! isLoaded() || transport == nullptr)
        return;

    transport->start();
    playing.store (true, std::memory_order_relaxed);
}

void BackingTrackPlayer::pause() noexcept
{
    if (transport != nullptr)
        transport->stop();

    playing.store (false, std::memory_order_relaxed);
}

void BackingTrackPlayer::stop() noexcept
{
    pause();

    if (transport != nullptr)
        transport->setPosition (isLoopEnabled() ? getLoopStartSeconds() : 0.0);

    shifterResetPending.store (true, std::memory_order_relaxed);   // SPEC-SWEEP PT-28
}

void BackingTrackPlayer::setPositionSeconds (double seconds)
{
    if (transport != nullptr)
        transport->setPosition (juce::jlimit (0.0, juce::jmax (0.0, lengthSeconds), seconds));

    shifterResetPending.store (true, std::memory_order_relaxed);   // SPEC-SWEEP PT-28
}

double BackingTrackPlayer::getPositionSeconds() const
{
    return (transport != nullptr) ? transport->getCurrentPosition() : 0.0;
}

//==============================================================================
void BackingTrackPlayer::setLevelDb (double db) noexcept
{
    levelDb.store (juce::jlimit (-60.0, 12.0, db), std::memory_order_relaxed);
}

void BackingTrackPlayer::setPan (double p) noexcept
{
    pan.store (juce::jlimit (-1.0, 1.0, p), std::memory_order_relaxed);
}

void BackingTrackPlayer::setLowCutHz (double hz) noexcept
{
    lowCutHz.store (juce::jlimit (20.0, 2000.0, hz), std::memory_order_relaxed);
}

void BackingTrackPlayer::setHighCutHz (double hz) noexcept
{
    highCutHz.store (juce::jlimit (200.0, 20000.0, hz), std::memory_order_relaxed);
}

void BackingTrackPlayer::setPitchShiftSemitones (double semitones) noexcept
{
    pitchSemis.store (juce::jlimit (-12.0, 12.0, semitones), std::memory_order_relaxed);
}

void BackingTrackPlayer::setTempoRatio (double ratio) noexcept
{
    tempoRatio.store (juce::jlimit (0.25, 2.0, ratio), std::memory_order_relaxed);
}

//==============================================================================
double BackingTrackPlayer::snapToZeroCrossing (double seconds) const
{
    if (analysisBuffer.getNumSamples() <= 0)
        return seconds;

    const int target = (int) (seconds * analysisSampleRate);

    if (! juce::isPositiveAndBelow (target, analysisBuffer.getNumSamples()))
        return seconds;

    const auto* data = analysisBuffer.getReadPointer (0);

    // Look a few milliseconds either way for a rising crossing. A loop point
    // that lands mid-cycle steps the waveform on the way round, which is exactly
    // the click the spec is asking to avoid.
    const int window = (int) (0.010 * analysisSampleRate);

    int best = target;
    int bestDistance = window + 1;

    const int from = juce::jmax (1, target - window);
    const int to = juce::jmin (analysisBuffer.getNumSamples() - 1, target + window);

    for (int i = from; i < to; ++i)
    {
        if (data[i - 1] <= 0.0f && data[i] > 0.0f)
        {
            const int distance = std::abs (i - target);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = i;
            }
        }
    }

    return (double) best / analysisSampleRate;
}

void BackingTrackPlayer::setLoopSeconds (double startSeconds, double endSeconds)
{
    const double from = juce::jlimit (0.0, juce::jmax (0.0, lengthSeconds), startSeconds);
    const double to = juce::jlimit (0.0, juce::jmax (0.0, lengthSeconds), endSeconds);

    loopStart.store (snapToZeroCrossing (juce::jmin (from, to)), std::memory_order_relaxed);
    loopEnd.store (snapToZeroCrossing (juce::jmax (from, to)), std::memory_order_relaxed);
}

//==============================================================================
double BackingTrackPlayer::estimateTempo()
{
    const int numSamples = analysisBuffer.getNumSamples();

    if (numSamples < (int) analysisSampleRate)
        return 0.0;

    const auto* data = analysisBuffer.getReadPointer (0);

    /*  An onset-spacing estimate.

        The energy in short windows is measured, the rises are picked out as
        onsets, and the most common spacing between them is taken as the beat.
        That is a fraction of what a real beat tracker does - no spectral flux,
        no comb filtering, no phase - but a backing track player only needs a
        number to set a metronome from, and the user can correct it. */
    const int windowSamples = juce::jmax (1, (int) (0.010 * analysisSampleRate));
    const int numWindows = numSamples / windowSamples;

    if (numWindows < 32)
        return 0.0;

    std::vector<double> energy ((size_t) numWindows, 0.0);

    for (int w = 0; w < numWindows; ++w)
    {
        double sum = 0.0;

        for (int i = 0; i < windowSamples; ++i)
        {
            const double sample = (double) data[w * windowSamples + i];
            sum += sample * sample;
        }

        energy[(size_t) w] = std::sqrt (sum / (double) windowSamples);
    }

    // Onsets: a window louder than the running average by a clear margin.
    std::vector<int> onsets;

    double average = 0.0;

    for (int w = 1; w < numWindows; ++w)
    {
        average = average * 0.95 + energy[(size_t) w] * 0.05;

        if (energy[(size_t) w] > average * 1.5
              && energy[(size_t) w] > energy[(size_t) w - 1] * 1.2
              && energy[(size_t) w] > 1.0e-4)
        {
            // No two onsets closer than 100 ms: at 300 bpm the beats are 200 ms
            // apart, so anything faster is a flam rather than a beat.
            if (onsets.empty() || (w - onsets.back()) * windowSamples
                                    > (int) (0.1 * analysisSampleRate))
                onsets.push_back (w);
        }
    }

    if (onsets.size() < 8)
        return 0.0;

    // The most common spacing, in a histogram over the plausible tempo range.
    constexpr int kMinBpm = 50, kMaxBpm = 200;

    std::array<int, kMaxBpm + 1> votes {};
    votes.fill (0);

    for (size_t i = 1; i < onsets.size(); ++i)
    {
        const double gapSeconds = (double) (onsets[i] - onsets[i - 1])
                                    * (double) windowSamples / analysisSampleRate;

        if (gapSeconds <= 0.0)
            continue;

        const int bpm = (int) std::round (60.0 / gapSeconds);

        if (bpm >= kMinBpm && bpm <= kMaxBpm)
        {
            // Neighbouring bins vote too, so a hand-played track whose beats
            // scatter by a bpm or two still lands on one peak.
            for (int b = juce::jmax (kMinBpm, bpm - 2); b <= juce::jmin (kMaxBpm, bpm + 2); ++b)
                votes[(size_t) b] += (b == bpm) ? 3 : 1;
        }
    }

    int bestBpm = 0, bestVotes = 0;

    for (int bpm = kMinBpm; bpm <= kMaxBpm; ++bpm)
    {
        if (votes[(size_t) bpm] > bestVotes)
        {
            bestVotes = votes[(size_t) bpm];
            bestBpm = bpm;
        }
    }

    // Not enough agreement is reported as "no idea" rather than as a guess.
    return (bestVotes >= 6) ? (double) bestBpm : 0.0;
}

//==============================================================================
void BackingTrackPlayer::addMarker (const juce::String& name, double seconds)
{
    TrackMarker marker;
    marker.name = name.isNotEmpty() ? name : ("Mark " + juce::String ((int) markers.size() + 1));
    marker.seconds = juce::jlimit (0.0, juce::jmax (0.0, lengthSeconds), seconds);

    markers.push_back (marker);

    std::sort (markers.begin(), markers.end(),
               [] (const TrackMarker& a, const TrackMarker& b) { return a.seconds < b.seconds; });
}

bool BackingTrackPlayer::removeMarker (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) markers.size()))
        return false;

    markers.erase (markers.begin() + index);
    return true;
}

TrackMarker BackingTrackPlayer::getMarker (int index) const
{
    return juce::isPositiveAndBelow (index, (int) markers.size())
             ? markers[(size_t) index] : TrackMarker {};
}

bool BackingTrackPlayer::jumpToMarker (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) markers.size()))
        return false;

    setPositionSeconds (markers[(size_t) index].seconds);
    return true;
}

//==============================================================================
void BackingTrackPlayer::setPlaylist (const juce::Array<juce::File>& files)
{
    playlist = files;
    playlistPosition = 0;

    if (! playlist.isEmpty())
        load (playlist.getReference (0));
}

bool BackingTrackPlayer::nextInPlaylist()
{
    if (playlistPosition + 1 >= playlist.size())
        return false;

    ++playlistPosition;

    const bool wasPlaying = isPlaying();

    if (! load (playlist.getReference (playlistPosition)))
        return false;

    // practice-tools 3: gapless. The next track starts the moment it is open, so
    // the only silence is the time the reader takes to fill its ring.
    if (wasPlaying)
        play();

    return true;
}

bool BackingTrackPlayer::previousInPlaylist()
{
    if (playlistPosition <= 0)
        return false;

    --playlistPosition;

    const bool wasPlaying = isPlaying();

    if (! load (playlist.getReference (playlistPosition)))
        return false;

    if (wasPlaying)
        play();

    return true;
}

//==============================================================================
void BackingTrackPlayer::processBlock (juce::AudioBuffer<float>& destination, int numSamples) noexcept
{
    if (destination.getNumChannels() < 2 || numSamples <= 0)
        return;

    destination.clear();

    const juce::SpinLock::ScopedTryLockType sl (transportLock);

    if (! sl.isLocked() || ! isLoaded() || transport == nullptr || ! isPlaying())
        return;

    // ---- pull from the streaming source ------------------------------------------
    /*  SPEC-SWEEP PT-28/29 (practice-tools 3): through the pitch and tempo
        shift when either is set. The shifter pulls as much input as it needs,
        a block at a most at a time, so the loop points still apply per pull. */
    const double tempo = getTempoRatio();
    const double semitones = getPitchShiftSemitones();

    if (TimePitchShifter::isNeutral (tempo, semitones))
    {
        shifterActive = false;

        for (int done = 0; done < numSamples;)
        {
            const int count = juce::jmin (numSamples - done, juce::jmax (1, blockSize));
            pullFromTransport (destination.getWritePointer (0, done), destination.getWritePointer (1, done), count);
            done += count;
        }
    }
    else
    {
        if (! shifterActive || shifterResetPending.exchange (false, std::memory_order_relaxed))
        {
            shifter.reset();
            shifterActive = true;
        }

        shifter.process (destination.getWritePointer (0), destination.getWritePointer (1), numSamples,
                         tempo, semitones, blockSize,
                         [this] (float* l, float* r, int count) { pullFromTransport (l, r, count); });
    }

    // ---- tone and level ------------------------------------------------------------
    const double low = lowCutHz.load (std::memory_order_relaxed);
    const double high = highCutHz.load (std::memory_order_relaxed);

    if (low != lastLowCut)
    {
        lowCutL.setHighpass (sr, low, 0.707);
        lowCutR.setHighpass (sr, low, 0.707);
        lastLowCut = low;
    }

    if (high != lastHighCut)
    {
        highCutL.setLowpass (sr, high, 0.707);
        highCutR.setLowpass (sr, high, 0.707);
        lastHighCut = high;
    }

    const double gain = dbToGain (getLevelDb());
    const bool sumToMono = isMonoSummed();

    const double panPosition = (getPan() + 1.0) * 0.25 * juce::MathConstants<double>::pi;
    const double panL = std::cos (panPosition) * juce::MathConstants<double>::sqrt2;
    const double panR = std::sin (panPosition) * juce::MathConstants<double>::sqrt2;

    auto* left = destination.getWritePointer (0);
    auto* right = destination.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        double l = (double) left[i];
        double r = (double) right[i];

        if (sumToMono)
        {
            const double m = (l + r) * 0.5;
            l = m;
            r = m;
        }

        l = lowCutL.process (l);
        l = highCutL.process (l);
        r = lowCutR.process (r);
        r = highCutR.process (r);

        left[i] = (float) sanitise (l * gain * panL);
        right[i] = (float) sanitise (r * gain * panR);
    }
}

//==============================================================================
void BackingTrackPlayer::pullFromTransport (float* left, float* right, int count) noexcept
{
    count = juce::jlimit (0, scratch.getNumSamples(), count);

    if (count == 0 || transport == nullptr || scratch.getNumChannels() < 2)
        return;

    juce::AudioSourceChannelInfo info (&scratch, 0, count);
    transport->getNextAudioBlock (info);

    juce::FloatVectorOperations::copy (left, scratch.getReadPointer (0), count);
    juce::FloatVectorOperations::copy (right, scratch.getReadPointer (1), count);

    // ---- loop points --------------------------------------------------------------
    if (isLoopEnabled())
    {
        const double end = getLoopEndSeconds();
        const double start = getLoopStartSeconds();

        if (end > start && transport->getCurrentPosition() >= end)
            transport->setPosition (start);
    }
    else if (transport->hasStreamFinished())
    {
        playing.store (false, std::memory_order_relaxed);
    }
}

} // namespace luthier
