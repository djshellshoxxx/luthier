#include "Looper.h"

namespace luthier
{

const char* const Looper::kFileExtension = ".luthierloop";

//==============================================================================
const char* getLayerModeName (LayerMode mode) noexcept
{
    switch (mode)
    {
        case LayerMode::overdub:  return "Overdub";
        case LayerMode::replace:  return "Replace";
        case LayerMode::playOnce: return "Play Once";
        case LayerMode::numModes:
        default:                  return "Overdub";
    }
}

//==============================================================================
void LoopLayer::prepare (int maxSamples)
{
    capacity = juce::jmax (1, maxSamples);

    audio.setSize (2, capacity, false, true, false);
    audio.clear();

    // Undo and redo hold a whole layer each. That is three times the memory per
    // layer, which is the price of being able to undo a take without a
    // re-render, and it is paid once here rather than on the audio thread.
    undoBuffer.setSize (2, capacity, false, true, false);
    redoBuffer.setSize (2, capacity, false, true, false);
    undoBuffer.clear();
    redoBuffer.clear();

    reset();
}

void LoopLayer::reset() noexcept
{
    audio.clear();
    midi.clear();

    recordedSamples = 0;
    undoSamples = 0;
    redoSamples = 0;
    undoFilled = false;
    redoFilled = false;
    readPosition = 0.0;
    playedOnce = false;

    lowCutL.reset();  lowCutR.reset();
    highCutL.reset(); highCutR.reset();
}

void LoopLayer::setRecordedSamples (int samples) noexcept
{
    recordedSamples = juce::jlimit (0, capacity, samples);
}

//==============================================================================
void LoopLayer::setLevelDb (double db) noexcept
{
    levelDb.store (juce::jlimit (-60.0, 12.0, db), std::memory_order_relaxed);
}

void LoopLayer::setPan (double p) noexcept
{
    pan.store (juce::jlimit (-1.0, 1.0, p), std::memory_order_relaxed);
}

void LoopLayer::setLowCutHz (double hz) noexcept
{
    lowCutHz.store (juce::jlimit (20.0, 2000.0, hz), std::memory_order_relaxed);
}

void LoopLayer::setHighCutHz (double hz) noexcept
{
    highCutHz.store (juce::jlimit (200.0, 20000.0, hz), std::memory_order_relaxed);
}

void LoopLayer::prepareFilters (double sampleRate) noexcept
{
    sr = juce::jmax (1.0, sampleRate);
    lastLowCut = -1.0;
    lastHighCut = -1.0;
}

//==============================================================================
void LoopLayer::record (const float* left, const float* right,
                        int position, int numSamples, int wrapLength) noexcept
{
    if (capacity <= 0 || left == nullptr)
        return;

    auto* destL = audio.getWritePointer (0);
    auto* destR = audio.getWritePointer (1);

    const bool replacing = getMode() == LayerMode::replace;

    for (int i = 0; i < numSamples; ++i)
    {
        const int index = wrapLength > 0 ? (position + i) % wrapLength : position + i;

        if (! juce::isPositiveAndBelow (index, capacity))
            break;

        const double l = (double) left[i];
        const double r = (right != nullptr) ? (double) right[i] : l;

        if (replacing)
        {
            destL[index] = (float) sanitise (l);
            destR[index] = (float) sanitise (r);
        }
        else
        {
            destL[index] = (float) sanitise ((double) destL[index] + l);
            destR[index] = (float) sanitise ((double) destR[index] + r);
        }
    }

    recordedSamples = juce::jmax (recordedSamples,
                                  juce::jmin (wrapLength > 0 ? juce::jmin (capacity, wrapLength) : capacity,
                                              position + numSamples));
}

void LoopLayer::playInto (float* left, float* right, int position, int numSamples,
                          int loopLength) noexcept
{
    if (capacity <= 0 || recordedSamples <= 0 || left == nullptr || right == nullptr)
        return;

    if (isMuted())
        return;

    const auto layerMode = getMode();

    // A play-once layer falls silent after its first pass, and arms again when
    // the loop is restarted from the top.
    if (layerMode == LayerMode::playOnce)
    {
        if (position == 0)
            playedOnce = false;
        else if (playedOnce)
            return;
    }

    // ---- filters -----------------------------------------------------------------
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

    const double panPosition = (getPan() + 1.0) * 0.25 * juce::MathConstants<double>::pi;
    const double panL = std::cos (panPosition) * juce::MathConstants<double>::sqrt2;
    const double panR = std::sin (panPosition) * juce::MathConstants<double>::sqrt2;

    const bool reverse = isReversed();
    const double rate = isHalfSpeed() ? 0.5 : 1.0;

    const auto* srcL = audio.getReadPointer (0);
    const auto* srcR = audio.getReadPointer (1);

    const int length = juce::jmax (1, juce::jmin (recordedSamples,
                                                  loopLength > 0 ? loopLength : recordedSamples));

    for (int i = 0; i < numSamples; ++i)
    {
        /*  Where in the layer to read.

            At normal speed the read follows the loop's own position, so every
            layer stays locked to every other. At half speed it cannot: the layer
            has to advance at its own rate, so it keeps a private position and
            wraps on its own. */
        double source;

        if (rate == 1.0)
        {
            source = (double) ((position + i) % length);
        }
        else
        {
            source = readPosition;
            readPosition += rate;

            if (readPosition >= (double) length)
                readPosition -= (double) length;
        }

        if (reverse)
            source = (double) (length - 1) - source;

        // Linear interpolation, because half speed lands between samples.
        const int index0 = juce::jlimit (0, length - 1, (int) source);
        const int index1 = juce::jlimit (0, length - 1, index0 + 1);
        const double fraction = source - (double) index0;

        double l = (double) srcL[index0] + ((double) srcL[index1] - (double) srcL[index0]) * fraction;
        double r = (double) srcR[index0] + ((double) srcR[index1] - (double) srcR[index0]) * fraction;

        l = lowCutL.process (l);
        l = highCutL.process (l);
        r = lowCutR.process (r);
        r = highCutR.process (r);

        left[i] = (float) sanitise ((double) left[i] + l * gain * panL);
        right[i] = (float) sanitise ((double) right[i] + r * gain * panR);
    }

    if (layerMode == LayerMode::playOnce && position + numSamples >= length)
        playedOnce = true;
}

//==============================================================================
void LoopLayer::pushUndo()
{
    if (capacity <= 0)
        return;

    for (int channel = 0; channel < 2; ++channel)
        undoBuffer.copyFrom (channel, 0, audio, channel, 0, capacity);

    undoSamples = recordedSamples;
    undoFilled = true;

    // A new take invalidates the redo: there is nothing to go forward to any
    // more, and leaving a stale one would restore a take from a different loop.
    redoFilled = false;
}

bool LoopLayer::undo()
{
    if (! undoFilled || capacity <= 0)
        return false;

    // Keep what is being undone, so redo can put it back.
    for (int channel = 0; channel < 2; ++channel)
        redoBuffer.copyFrom (channel, 0, audio, channel, 0, capacity);

    redoSamples = recordedSamples;
    redoFilled = true;

    for (int channel = 0; channel < 2; ++channel)
        audio.copyFrom (channel, 0, undoBuffer, channel, 0, capacity);

    recordedSamples = undoSamples;
    undoFilled = false;

    return true;
}

bool LoopLayer::redo()
{
    if (! redoFilled || capacity <= 0)
        return false;

    for (int channel = 0; channel < 2; ++channel)
        undoBuffer.copyFrom (channel, 0, audio, channel, 0, capacity);

    undoSamples = recordedSamples;
    undoFilled = true;

    for (int channel = 0; channel < 2; ++channel)
        audio.copyFrom (channel, 0, redoBuffer, channel, 0, capacity);

    recordedSamples = redoSamples;
    redoFilled = false;

    return true;
}

//==============================================================================
juce::var LoopLayer::settingsToVar() const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("mode", (int) getMode());
    object->setProperty ("levelDb", getLevelDb());
    object->setProperty ("pan", getPan());
    object->setProperty ("muted", isMuted());
    object->setProperty ("reversed", isReversed());
    object->setProperty ("halfSpeed", isHalfSpeed());
    object->setProperty ("lowCutHz", getLowCutHz());
    object->setProperty ("highCutHz", getHighCutHz());
    object->setProperty ("recordedSamples", recordedSamples);

    return { object };
}

void LoopLayer::settingsFromVar (const juce::var& state)
{
    auto* object = state.getDynamicObject();

    if (object == nullptr)
        return;

    setMode ((LayerMode) juce::jlimit (0, (int) LayerMode::numModes - 1,
                                       (int) object->getProperty ("mode")));

    setLevelDb ((double) object->getProperty ("levelDb"));
    setPan ((double) object->getProperty ("pan"));
    setMuted ((bool) object->getProperty ("muted"));
    setReversed ((bool) object->getProperty ("reversed"));
    setHalfSpeed ((bool) object->getProperty ("halfSpeed"));

    if (object->hasProperty ("lowCutHz"))
        setLowCutHz ((double) object->getProperty ("lowCutHz"));

    if (object->hasProperty ("highCutHz"))
        setHighCutHz ((double) object->getProperty ("highCutHz"));

    setRecordedSamples ((int) object->getProperty ("recordedSamples"));
}

//==============================================================================
Looper::Looper() = default;
Looper::~Looper() = default;

void Looper::prepare (double sampleRate, double maxSeconds)
{
    sr = juce::jmax (1.0, sampleRate);

    const double seconds = juce::jlimit (kMinLoopSeconds, kMaxLoopSeconds, maxSeconds);
    capacity = (int) (seconds * sr);

    for (auto& layer : layers)
    {
        layer.prepare (capacity);
        layer.prepareFilters (sr);
    }

    reset();
}

void Looper::reset() noexcept
{
    state.store ((int) State::stopped, std::memory_order_relaxed);
    loopLength.store (0, std::memory_order_relaxed);
    playPosition.store (0, std::memory_order_relaxed);
    activeLayer.store (0, std::memory_order_relaxed);
    pendingClose.store (false, std::memory_order_relaxed);
}

void Looper::clear()
{
    for (auto& layer : layers)
        layer.reset();

    reset();
}

LoopLayer& Looper::getLayer (int index) noexcept
{
    return layers[(size_t) juce::jlimit (0, kMaxLayers - 1, index)];
}

const LoopLayer& Looper::getLayer (int index) const noexcept
{
    return layers[(size_t) juce::jlimit (0, kMaxLayers - 1, index)];
}

void Looper::setActiveLayer (int index) noexcept
{
    activeLayer.store (juce::jlimit (0, kMaxLayers - 1, index), std::memory_order_relaxed);
}

int Looper::getNumRecordedLayers() const noexcept
{
    int count = 0;

    for (const auto& layer : layers)
        if (layer.hasContent())
            ++count;

    return count;
}

//==============================================================================
void Looper::press() noexcept
{
    switch (getState())
    {
        case State::stopped:
            // First press: start recording the loop that defines the length.
            if (getLoopLengthSamples() > 0)
            {
                // A loop already exists, so this is a play command.
                playPosition.store (0, std::memory_order_relaxed);
                state.store ((int) State::playing, std::memory_order_relaxed);
            }
            else
            {
                playPosition.store (0, std::memory_order_relaxed);
                state.store ((int) State::recordingFirst, std::memory_order_relaxed);
            }
            break;

        case State::recordingFirst:
            // Second press closes the loop. The audio thread does it at the next
            // bar line when the metronome is running, so it lands on the beat.
            pendingClose.store (true, std::memory_order_relaxed);
            break;

        case State::playing:
        {
            // Third press overdubs onto the next free layer.
            int next = getActiveLayer();

            for (int i = 0; i < kMaxLayers; ++i)
            {
                const int candidate = (getActiveLayer() + 1 + i) % kMaxLayers;

                if (! layers[(size_t) candidate].hasContent())
                {
                    next = candidate;
                    break;
                }
            }

            setActiveLayer (next);
            state.store ((int) State::overdubbing, std::memory_order_relaxed);
            break;
        }

        case State::overdubbing:
            state.store ((int) State::playing, std::memory_order_relaxed);
            break;

        default:
            break;
    }
}

void Looper::stop() noexcept
{
    state.store ((int) State::stopped, std::memory_order_relaxed);
    playPosition.store (0, std::memory_order_relaxed);
    pendingClose.store (false, std::memory_order_relaxed);
}

//==============================================================================
void Looper::processBlock (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (capacity <= 0 || buffer.getNumChannels() < 2 || numSamples <= 0)
        return;

    const auto currentState = getState();

    if (currentState == State::stopped)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    // jam-mode 11 (FEAT-JAM): a first recording waits for the band's downbeat.
    if (currentState == State::recordingFirst && recordStartDelay > 0)
    {
        const int skip = juce::jmin (recordStartDelay, numSamples);
        recordStartDelay -= skip;

        if (skip == numSamples)
            return;

        left += skip;
        right += skip;
        numSamples -= skip;
    }

    const int position = getPlayPosition();
    const int length = getLoopLengthSamples();

    // ---- recording and playback -----------------------------------------------------
    if (currentState == State::overdubbing && length > 0)
    {
        /*  Every layer, the active one included, plays what it held before this
            block; the live input, kept aside, is then recorded over the active
            one at the loop's own (wrapped) positions. Recording first and then
            playing the active layer played the live signal back on top of
            itself, and unwrapped positions put the overdub past the loop end. */
        auto& active = layers[(size_t) getActiveLayer()];

        for (int offset = 0; offset < numSamples; offset += kOverdubChunk)
        {
            const int n = juce::jmin (kOverdubChunk, numSamples - offset);
            const int at = (position + offset) % length;

            std::copy (left + offset, left + offset + n, overdubL.begin());
            std::copy (right + offset, right + offset + n, overdubR.begin());

            for (auto& layer : layers)
                layer.playInto (left + offset, right + offset, at, n, length);

            active.record (overdubL.data(), overdubR.data(), at, n, length);
        }
    }
    else
    {
        if (currentState == State::recordingFirst)
            layers[(size_t) getActiveLayer()].record (left, right, position, numSamples);

        if (currentState != State::recordingFirst && length > 0)
            for (auto& layer : layers)
                layer.playInto (left, right, position, numSamples, length);
    }

    // ---- advance ----------------------------------------------------------------------
    int next = position + numSamples;

    if (currentState == State::recordingFirst)
    {
        /*  practice-tools 2: the loop length is quantised to bars when the
            metronome is running.

            The close is requested by the button and carried out here, at the
            first bar line at or after the request. That is why the flag exists:
            a player hits the button roughly on the beat, and the loop has to
            close exactly on it. */
        // MODEL-GAPS: a set default length closes the first recording itself.
        const int fixedLength = defaultLengthSamples.load (std::memory_order_relaxed);

        if (fixedLength > 0 && next >= juce::jmin (fixedLength, capacity))
        {
            const int closeAt = juce::jlimit (1, capacity, fixedLength);

            loopLength.store (closeAt, std::memory_order_relaxed);
            layers[(size_t) getActiveLayer()].setRecordedSamples (closeAt);
            pendingClose.store (false, std::memory_order_relaxed);
            state.store ((int) State::playing, std::memory_order_relaxed);
            next -= closeAt;
        }
        else if (pendingClose.load (std::memory_order_relaxed))
        {
            const int bar = barLengthSamples.load (std::memory_order_relaxed);

            int closeAt = next;

            if (bar > 0)
            {
                // Round to the nearest bar, so closing slightly early still gives
                // the bar the player meant rather than one less.
                const int bars = juce::jmax (1, (int) std::round ((double) next / (double) bar));
                closeAt = bars * bar;
            }

            closeAt = juce::jlimit (1, capacity, closeAt);

            loopLength.store (closeAt, std::memory_order_relaxed);
            layers[(size_t) getActiveLayer()].setRecordedSamples (closeAt);

            pendingClose.store (false, std::memory_order_relaxed);
            state.store ((int) State::playing, std::memory_order_relaxed);

            next = 0;
        }
        else if (next >= capacity)
        {
            // The maximum length was reached without the player closing it.
            loopLength.store (capacity, std::memory_order_relaxed);
            layers[(size_t) getActiveLayer()].setRecordedSamples (capacity);
            state.store ((int) State::playing, std::memory_order_relaxed);
            next = 0;
        }
    }
    else if (length > 0 && next >= length)
    {
        next -= length;
    }

    playPosition.store (juce::jlimit (0, juce::jmax (1, capacity) - 1, next),
                        std::memory_order_relaxed);
}

void Looper::renderPlaybackMidi (juce::MidiBuffer& out, int numSamples) const noexcept
{
    const auto currentState = getState();
    const int length = getLoopLengthSamples();

    if ((currentState != State::playing && currentState != State::overdubbing) || length <= 0 || numSamples <= 0)
        return;

    const int position = getPlayPosition();

    for (const auto& layer : layers)
    {
        if (! layer.hasContent() || layer.isMuted())
            continue;

        const auto& sequence = layer.getMidi();

        // The block's window of the loop, in at most two pieces (it may wrap).
        for (int piece = 0; piece < 2; ++piece)
        {
            const int from = piece == 0 ? position : 0;
            const int to = piece == 0 ? juce::jmin (length, position + numSamples) : position + numSamples - length;
            const int shift = piece == 0 ? -position : length - position;

            if (to <= from)
                continue;

            for (int i = sequence.getNextIndexAtTime ((double) from); i < sequence.getNumEvents(); ++i)
            {
                const auto* e = sequence.getEventPointer (i);
                const double t = e->message.getTimeStamp();

                if (t >= (double) to)
                    break;

                if (e->message.isNoteOnOrOff())
                    out.addEvent (e->message, juce::jlimit (0, numSamples - 1, (int) t + shift));
            }
        }
    }
}

void Looper::captureMidi (const juce::MidiBuffer& midi, int numSamples) noexcept
{
    juce::ignoreUnused (numSamples);

    const auto currentState = getState();

    if (currentState != State::recordingFirst && currentState != State::overdubbing)
        return;

    auto& sequence = layers[(size_t) getActiveLayer()].getMidi();
    const int position = getPlayPosition();

    for (const auto metadata : midi)
    {
        // Timestamps are in samples from the top of the loop, so the sequence can
        // be re-rendered against a different tone later.
        sequence.addEvent (metadata.getMessage(),
                           (double) (position + metadata.samplePosition));
    }
}

//==============================================================================
namespace
{
    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer,
                   int numSamples, double sampleRate)
    {
        if (numSamples <= 0)
            return false;

        file.getParentDirectory().createDirectory();
        file.deleteFile();

        juce::WavAudioFormat format;

        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

        if (stream == nullptr)
            return false;

        // practice-tools 2: 24-bit float in the temp area; a bounce is written at
        // 24-bit, which is what anyone would import.
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), sampleRate,
                                    (unsigned int) buffer.getNumChannels(), 24, {}, 0));

        if (writer == nullptr)
            return false;

        stream.release();

        return writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
    }
}

bool Looper::writeLayersToFile (const juce::File& file,
                                const juce::Array<int>& layerIndices) const
{
    const int length = getLoopLengthSamples();

    if (length <= 0 || layerIndices.isEmpty())
        return false;

    juce::AudioBuffer<float> mix (2, length);
    mix.clear();

    for (int index : layerIndices)
    {
        const auto& layer = getLayer (index);

        if (! layer.hasContent())
            continue;

        const int count = juce::jmin (length, layer.getRecordedSamples());

        // The bounce takes each layer's own level and pan, so what is written is
        // what was being heard.
        const double gain = dbToGain (layer.getLevelDb());
        const double panPosition = (layer.getPan() + 1.0) * 0.25 * juce::MathConstants<double>::pi;

        const double panL = std::cos (panPosition) * juce::MathConstants<double>::sqrt2;
        const double panR = std::sin (panPosition) * juce::MathConstants<double>::sqrt2;

        mix.addFrom (0, 0, layer.readLeft(), count, (float) (gain * panL));
        mix.addFrom (1, 0, layer.readRight(), count, (float) (gain * panR));
    }

    return writeWav (file, mix, length, sr);
}

bool Looper::exportMixdown (const juce::File& file) const
{
    juce::Array<int> all;

    for (int i = 0; i < kMaxLayers; ++i)
        if (getLayer (i).hasContent() && ! getLayer (i).isMuted())
            all.add (i);

    return writeLayersToFile (file, all);
}

bool Looper::exportStems (const juce::File& directory) const
{
    if (getLoopLengthSamples() <= 0)
        return false;

    directory.createDirectory();

    bool wroteAny = false;

    for (int i = 0; i < kMaxLayers; ++i)
    {
        if (! getLayer (i).hasContent())
            continue;

        juce::Array<int> one;
        one.add (i);

        if (writeLayersToFile (directory.getChildFile ("Layer " + juce::String (i + 1) + ".wav"), one))
            wroteAny = true;
    }

    return wroteAny;
}

//==============================================================================
juce::File Looper::getUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Loops");
}

bool Looper::save (const juce::File& file) const
{
    const int length = getLoopLengthSamples();

    if (length <= 0)
        return false;

    /*  A loop is a folder rather than a single file: the settings and MIDI are
        JSON, and each layer's audio is a WAV beside them. Packing megabytes of
        audio into JSON would mean base64, which is a third larger and cannot be
        opened by anything else. */
    const auto folder = file.getParentDirectory()
                          .getChildFile (file.getFileNameWithoutExtension());

    folder.createDirectory();

    auto* root = new juce::DynamicObject();

    root->setProperty ("format", "luthierloop");
    root->setProperty ("sampleRate", sr);
    root->setProperty ("loopLength", length);

    juce::Array<juce::var> layerArray;

    for (int i = 0; i < kMaxLayers; ++i)
    {
        const auto& layer = getLayer (i);

        auto* entry = layer.settingsToVar().getDynamicObject();

        if (entry == nullptr)
            continue;

        if (layer.hasContent())
        {
            const auto audioName = "layer" + juce::String (i + 1) + ".wav";

            juce::AudioBuffer<float> copy (2, juce::jmin (length, layer.getRecordedSamples()));
            copy.copyFrom (0, 0, layer.readLeft(), copy.getNumSamples());
            copy.copyFrom (1, 0, layer.readRight(), copy.getNumSamples());

            if (writeWav (folder.getChildFile (audioName), copy, copy.getNumSamples(), sr))
                entry->setProperty ("audio", audioName);

            // The MIDI travels with it, so the loop can be re-rendered through a
            // different tone (practice-tools 2).
            juce::Array<juce::var> events;

            for (int e = 0; e < layer.getMidi().getNumEvents(); ++e)
            {
                const auto* event = layer.getMidi().getEventPointer (e);

                if (event == nullptr)
                    continue;

                auto* midiEntry = new juce::DynamicObject();
                midiEntry->setProperty ("t", event->message.getTimeStamp());
                midiEntry->setProperty ("b", juce::String::toHexString (
                    event->message.getRawData(), event->message.getRawDataSize(), 0));

                events.add (juce::var (midiEntry));
            }

            entry->setProperty ("midi", events);
        }

        layerArray.add (juce::var (entry));
    }

    root->setProperty ("layers", layerArray);

    return folder.getChildFile ("loop.json")
             .replaceWithText (juce::JSON::toString (juce::var (root), false));
}

int Looper::importLayer (int layerIndex, const juce::AudioBuffer<float>& source)
{
    if (! juce::isPositiveAndBelow (layerIndex, kMaxLayers) || source.getNumChannels() == 0)
        return 0;

    stop();

    auto& layer = getLayer (layerIndex);
    const int count = juce::jmin (capacity, source.getNumSamples(), layer.getAudio().getNumSamples());

    layer.getAudio().clear();

    for (int ch = 0; ch < juce::jmin (2, layer.getAudio().getNumChannels()); ++ch)
        layer.getAudio().copyFrom (ch, 0, source, juce::jmin (ch, source.getNumChannels() - 1), 0, count);

    layer.setRecordedSamples (count);

    bool others = false;

    for (int i = 0; i < kMaxLayers; ++i)
        others = others || (i != layerIndex && getLayer (i).hasContent());

    if (! others || loopLength.load (std::memory_order_relaxed) <= 0)
        loopLength.store (count, std::memory_order_relaxed);

    return count;
}

bool Looper::load (const juce::File& file)
{
    const auto folder = file.isDirectory()
                          ? file
                          : file.getParentDirectory().getChildFile (
                                file.getFileNameWithoutExtension());

    const auto json = folder.getChildFile ("loop.json");

    if (! json.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (json.loadFileAsString());
    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return false;

    clear();

    const int length = juce::jlimit (0, capacity, (int) root->getProperty ("loopLength"));
    loopLength.store (length, std::memory_order_relaxed);

    const auto* layerArray = root->getProperty ("layers").getArray();

    if (layerArray == nullptr)
        return length > 0;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (int i = 0; i < juce::jmin (kMaxLayers, layerArray->size()); ++i)
    {
        auto& layer = getLayer (i);

        layer.settingsFromVar ((*layerArray)[i]);

        auto* entry = (*layerArray)[i].getDynamicObject();

        if (entry == nullptr)
            continue;

        const auto audioName = entry->getProperty ("audio").toString();

        if (audioName.isNotEmpty())
        {
            const auto audioFile = folder.getChildFile (audioName);

            if (std::unique_ptr<juce::AudioFormatReader> reader (
                    formats.createReaderFor (audioFile));
                reader != nullptr)
            {
                const int count = juce::jmin (capacity, (int) reader->lengthInSamples);

                reader->read (&layer.getAudio(), 0, count, 0, true, true);
                layer.setRecordedSamples (count);
            }
        }

        if (const auto* events = entry->getProperty ("midi").getArray())
        {
            for (const auto& event : *events)
            {
                auto* midiEntry = event.getDynamicObject();

                if (midiEntry == nullptr)
                    continue;

                juce::MemoryBlock bytes;
                bytes.loadFromHexString (midiEntry->getProperty ("b").toString());

                if (bytes.getSize() == 0)
                    continue;

                layer.getMidi().addEvent (
                    juce::MidiMessage (bytes.getData(), (int) bytes.getSize(),
                                       (double) midiEntry->getProperty ("t")));
            }
        }
    }

    return true;
}

//==============================================================================
SessionRecorder::SessionRecorder() = default;
SessionRecorder::~SessionRecorder() = default;

bool SessionRecorder::prepare (double sampleRate, double minutes)
{
    sr = juce::jmax (1.0, sampleRate);

    const double clamped = juce::jlimit (1.0, 240.0, minutes);

    const int64_t wanted = (int64_t) (clamped * 60.0 * sr);

    /*  practice-tools 8 sizes the default buffer at about 1.4 GB, which is more
        than this machine has. A recorder that fails to allocate must fail here,
        on the message thread, rather than by throwing under the audio callback,
        so the request is capped at something that can actually be held and the
        caller is told what it got by getCapacityMinutes(). */
    constexpr int64_t kMaxSamples = 1 << 26;      // 64 M frames: about 23 minutes at 48 kHz

    {
        const juce::SpinLock::ScopedLockType sl (ringLock);

        capacity = (int) juce::jmin (wanted, kMaxSamples);

        if (capacity <= 0)
            return false;

        try
        {
            ring.setSize (2, capacity, false, true, false);
        }
        catch (...)
        {
            capacity = 0;
            return false;
        }

        ring.clear();
        writePosition.store (0, std::memory_order_relaxed);
        recorded.store (0, std::memory_order_relaxed);
    }

    reset();

    return true;
}

bool Looper::loadLayerAudio (int layerIndex, const juce::AudioBuffer<float>& source)
{
    if (capacity <= 0 || source.getNumSamples() <= 0 || getState() != State::stopped
        || ! juce::isPositiveAndBelow (layerIndex, kMaxLayers))
        return false;

    int length = loopLength.load (std::memory_order_relaxed);

    if (length <= 0)
    {
        length = juce::jmin (capacity, source.getNumSamples());
        loopLength.store (length, std::memory_order_relaxed);
    }

    auto& layer = layers[(size_t) layerIndex];
    const auto mode = layer.getMode();
    layer.setMode (LayerMode::replace);

    juce::AudioBuffer<float> fitted (2, length);
    fitted.clear();

    for (int c = 0; c < 2; ++c)
        fitted.copyFrom (c, 0, source, juce::jmin (c, source.getNumChannels() - 1), 0,
                         juce::jmin (length, source.getNumSamples()));

    layer.record (fitted.getReadPointer (0), fitted.getReadPointer (1), 0, length);
    layer.setRecordedSamples (length);
    layer.setMode (mode);
    return true;
}

void SessionRecorder::reset() noexcept
{
    writePosition.store (0, std::memory_order_relaxed);
    recorded.store (0, std::memory_order_relaxed);
    samplesSeen = 0;

    const juce::ScopedLock sl (midiLock);
    midiFifo.reset();
    midi.clear();
}

void SessionRecorder::processBlock (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    // MODEL-GAPS: MIDI only - the clock runs, so the MIDI keeps its place.
    if (isEnabled() && ! isRecordingAudio())
    {
        samplesSeen += numSamples;
        return;
    }

    if (! isEnabled() || buffer.getNumChannels() < 1)
        return;

    const juce::SpinLock::ScopedTryLockType sl (ringLock);

    if (! sl.isLocked() || capacity <= 0)
        return;

    // Nothing here allocates: the ring exists, and this is a copy into it.
    int position = juce::jlimit (0, capacity - 1, writePosition.load (std::memory_order_relaxed));

    const auto* srcL = buffer.getReadPointer (0);
    const auto* srcR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : srcL;

    auto* destL = ring.getWritePointer (0);
    auto* destR = ring.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        destL[position] = srcL[i];
        destR[position] = srcR[i];

        if (++position >= capacity)
            position = 0;
    }

    writePosition.store (position, std::memory_order_relaxed);
    recorded.store (juce::jmin (capacity, recorded.load (std::memory_order_relaxed) + numSamples),
                    std::memory_order_relaxed);

    samplesSeen += numSamples;
}

void SessionRecorder::captureMidi (const juce::MidiBuffer& incoming, int numSamples) noexcept
{
    juce::ignoreUnused (numSamples);

    if (! isEnabled() || ! isRecordingMidi())
        return;

    // MODEL-GAPS: into the fixed FIFO; the sequence (which allocates) is the
    // message thread's.
    for (const auto metadata : incoming)
    {
        if (metadata.numBytes <= 0 || metadata.numBytes > 3)
            continue;

        const auto scope = midiFifo.write (1);

        if (scope.blockSize1 + scope.blockSize2 == 0)
            return;   // full: the drain is late; dropping is better than blocking

        auto& q = midiQueue[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)];
        q.sample = samplesSeen + metadata.samplePosition;
        q.size = metadata.numBytes;
        std::memcpy (q.bytes, metadata.data, (size_t) metadata.numBytes);
    }
}

void SessionRecorder::drainMidiLocked() const
{
    const auto scope = midiFifo.read (midiFifo.getNumReady());

    auto take = [this] (int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto& q = midiQueue[(size_t) (start + i)];
            midi.addEvent (juce::MidiMessage (q.bytes, q.size, (double) q.sample));
        }
    };

    take (scope.startIndex1, scope.blockSize1);
    take (scope.startIndex2, scope.blockSize2);
}

int SessionRecorder::importMidi (const juce::MidiMessageSequence& sequence)
{
    const juce::ScopedLock sl (midiLock);
    drainMidiLocked();

    const double after = juce::jmax ((double) samplesSeen,
                                     midi.getNumEvents() > 0 ? midi.getEndTime() + 1.0 : 0.0);
    int added = 0;

    for (int i = 0; i < sequence.getNumEvents(); ++i)
        if (const auto* e = sequence.getEventPointer (i))
        {
            midi.addEvent (e->message, after);
            ++added;
        }

    midi.updateMatchedPairs();
    return added;
}

void SessionRecorder::drainMidi()
{
    const juce::ScopedLock sl (midiLock);
    drainMidiLocked();
}

int SessionRecorder::getNumMidiEvents() const
{
    const juce::ScopedLock sl (midiLock);
    drainMidiLocked();
    return midi.getNumEvents();
}

bool SessionRecorder::stop (const juce::File& directory)
{
    const bool wasOn = isEnabled();
    setEnabled (false);

    if (! wasOn || ! autoSaveOnStop)
        return false;

    return saveLastTake (directory);
}

bool SessionRecorder::saveLastTake (const juce::File& directory, double seconds) const
{
    lastSaved.clear();

    const int available = recorded.load (std::memory_order_relaxed);
    const bool haveAudio = isRecordingAudio() && available > 0 && capacity > 0;
    const bool haveMidi = isRecordingMidi() && getNumMidiEvents() > 0;   // drains the FIFO

    if (! haveAudio && ! haveMidi)
        return false;

    const int wanted = ! haveAudio ? 0
                     : (seconds > 0.0) ? juce::jmin (available, (int) (seconds * sr))
                                       : available;

    directory.createDirectory();

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");

    // Unwrap the ring into a linear buffer, oldest first.
    juce::AudioBuffer<float> take (2, wanted);

    const int writeAt = writePosition.load (std::memory_order_relaxed);
    int readAt = writeAt - wanted;

    while (readAt < 0)
        readAt += capacity;

    for (int i = 0; i < wanted; ++i)
    {
        take.setSample (0, i, ring.getSample (0, readAt));
        take.setSample (1, i, ring.getSample (1, readAt));

        if (++readAt >= capacity)
            readAt = 0;
    }

    const auto wavTarget = directory.getChildFile ("session-" + stamp + ".wav");
    const bool wroteAudio = haveAudio && writeWav (wavTarget, take, wanted, sr);

    if (wroteAudio)
        lastSaved.add (wavTarget);

    // And the MIDI beside it.
    bool wroteMidi = false;

    {
        const juce::ScopedLock sl (midiLock);

        if (haveMidi && midi.getNumEvents() > 0)
        {
            juce::MidiFile midiFile;
            juce::MidiMessageSequence sequence (midi);

            // A MIDI file's timebase is ticks, not samples.
            constexpr int ticksPerQuarter = 960;
            const double ticksPerSample = (double) ticksPerQuarter * 2.0 / sr;

            juce::MidiMessageSequence scaled;

            for (int i = 0; i < sequence.getNumEvents(); ++i)
                if (const auto* event = sequence.getEventPointer (i))
                    scaled.addEvent (event->message,
                                     event->message.getTimeStamp() * ticksPerSample
                                       - event->message.getTimeStamp());

            midiFile.setTicksPerQuarterNote (ticksPerQuarter);
            midiFile.addTrack (scaled);

            const auto midiTarget = directory.getChildFile ("session-" + stamp + ".mid");
            midiTarget.deleteFile();

            if (std::unique_ptr<juce::FileOutputStream> stream (midiTarget.createOutputStream());
                stream != nullptr)
                wroteMidi = midiFile.writeTo (*stream);
        }
    }

    if (wroteMidi)
        lastSaved.add (directory.getChildFile ("session-" + stamp + ".mid"));

    return wroteAudio || wroteMidi;
}

void SessionRecorder::cleanUpOldTempFiles (const juce::File& directory, double olderThanHours)
{
    if (! directory.isDirectory())
        return;

    const auto cutoff = juce::Time::getCurrentTime()
                          - juce::RelativeTime::hours (olderThanHours);

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*"))
    {
        const auto file = entry.getFile();

        if (file.getLastModificationTime() < cutoff)
            file.deleteFile();
    }
}

juce::File SessionRecorder::getSessionDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Sessions");
}

juce::File SessionRecorder::getTempDirectory()
{
    return getSessionDirectory().getChildFile ("tmp");
}

} // namespace luthier
