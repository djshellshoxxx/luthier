#include "PreviewPlayer.h"

namespace luthier
{

namespace
{
    double nowMs() noexcept { return juce::Time::getMillisecondCounterHiRes(); }

    float raisedCosine (float x) noexcept   // 0 -> 0, 1 -> 1
    {
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, x));
    }
}

//==============================================================================
void PreviewPlayer::start (const PreviewClip* clip) noexcept
{
    if (clip == nullptr)
        return;

    pending.store (clip);
    startRequested.store (true);
    active.store (true);
}

void PreviewPlayer::stop() noexcept
{
    stopRequested.store (true);
}

bool PreviewPlayer::isActive() const noexcept
{
    return active.load() && ! stopRequested.load();
}

juce::String PreviewPlayer::getActiveKey() const
{
    if (auto* p = pending.load())
        return p->key;

    for (const auto& u : inUse)
        if (auto* c = u.load())
            return c->key;

    return {};
}

PreviewPlayer::Gate PreviewPlayer::getGate() const noexcept
{
    Gate g;
    const double now = nowMs();
    g.hostPlaying = gateHostPlaying.load (std::memory_order_relaxed);
    g.nonRealtime = gateNonRealtime.load (std::memory_order_relaxed);
    g.killActive = gateKill.load (std::memory_order_relaxed);
    g.liveNotesHeld = gateHeld.load (std::memory_order_relaxed);
    g.msSinceLiveNote = now - lastNoteMs.load (std::memory_order_relaxed);
    g.msSinceProcess = now - lastProcessMs.load (std::memory_order_relaxed);
    return g;
}

//==============================================================================
void PreviewPlayer::prepare (double rate) noexcept
{
    sampleRate.store (rate > 0.0 ? rate : 48000.0);
    dropAll();
    currentGain = targetGain.load();
}

void PreviewPlayer::releaseVoice (int index) noexcept
{
    voices[(size_t) index] = {};
    inUse[(size_t) index].store (nullptr);
}

void PreviewPlayer::dropAll() noexcept
{
    // 4.1: a rate change drops the clips; they are resampled on the next request.
    pending.store (nullptr);
    queued = nullptr;
    startRequested.store (false);
    stopRequested.store (false);

    for (int v = 0; v < 2; ++v)
        releaseVoice (v);

    active.store (false);
    progress.store (-1.0f, std::memory_order_relaxed);
}

void PreviewPlayer::publishGate (bool hostPlaying, bool nonRealtime, bool killActive,
                                 int liveNotesHeld, bool liveNoteOnThisBlock) noexcept
{
    const double now = nowMs();
    gateHostPlaying.store (hostPlaying, std::memory_order_relaxed);
    gateNonRealtime.store (nonRealtime, std::memory_order_relaxed);
    gateKill.store (killActive, std::memory_order_relaxed);
    gateHeld.store (liveNotesHeld > 0, std::memory_order_relaxed);
    lastProcessMs.store (now, std::memory_order_relaxed);

    if (liveNoteOnThisBlock)
        lastNoteMs.store (now, std::memory_order_relaxed);

    // The audio thread checks non-realtime and the kill switch itself (4.3).
    if (nonRealtime || killActive)
        stopRequested.store (true);
}

void PreviewPlayer::processBlock (juce::AudioBuffer<float>& out, int numSamples) noexcept
{
    const double rate = sampleRate.load();
    const int fadeIn = juce::jmax (1, (int) std::round (0.010 * rate));
    const int fadeOut = juce::jmax (1, (int) std::round (0.030 * rate));
    const float gainStep = (float) (1.0 / juce::jmax (1.0, 0.020 * rate));   // 20 ms full-scale ramp

    // ---- commands --------------------------------------------------------------
    if (stopRequested.exchange (false))
    {
        // A stop also cancels a start that has not been taken yet.
        pending.store (nullptr);
        startRequested.store (false);
        queued = nullptr;

        for (auto& v : voices)
            if (v.clip != nullptr && v.fadeOutPos < 0)
                v.fadeOutPos = 0;
    }

    if (startRequested.exchange (false))
    {
        // Published as in use before pending is cleared, so the pool, which reads
        // pending first, always sees the clip in one place or the other.
        if (auto* clip = pending.load())
        {
            int slot = voices[0].clip == nullptr ? 0 : (voices[1].clip == nullptr ? 1 : -1);

            if (slot < 0)
            {
                // Both busy: the older one is dropped outright (it is already fading).
                slot = voices[0].fadeOutPos >= 0 ? 0 : 1;
                releaseVoice (slot);
            }

            inUse[(size_t) slot].store (clip);
            const PreviewClip* expected = clip;
            pending.compare_exchange_strong (expected, nullptr);

            bool anySounding = false;

            for (int v = 0; v < 2; ++v)
                if (v != slot && voices[(size_t) v].clip != nullptr)
                {
                    anySounding = true;

                    if (voices[(size_t) v].fadeOutPos < 0)
                        voices[(size_t) v].fadeOutPos = 0;
                }

            voices[(size_t) slot] = { clip, 0, 0, -1 };

            // The new clip waits for the old one's fade-out (4.1).
            queued = anySounding ? clip : nullptr;
        }
    }

    const int channels = out.getNumChannels();

    if (channels == 0 || numSamples <= 0)
        return;

    bool anything = false;

    for (int s = 0; s < numSamples; ++s)
    {
        // Linear volume smoothing.
        const float target = targetGain.load (std::memory_order_relaxed);

        if (currentGain < target)       currentGain = juce::jmin (target, currentGain + gainStep);
        else if (currentGain > target)  currentGain = juce::jmax (target, currentGain - gainStep);

        bool otherFading = false;

        for (int v = 0; v < 2; ++v)
            if (voices[(size_t) v].clip != nullptr && voices[(size_t) v].clip != queued)
                otherFading = true;

        float left = 0.0f, right = 0.0f;

        for (int v = 0; v < 2; ++v)
        {
            auto& voice = voices[(size_t) v];

            if (voice.clip == nullptr)
                continue;

            // A queued clip holds at its start until the old voice has gone.
            if (voice.clip == queued && otherFading)
                continue;

            if (voice.clip == queued)
                queued = nullptr;

            const auto& audio = voice.clip->audio;

            if (voice.position >= audio.getNumSamples())
            {
                releaseVoice (v);
                continue;
            }

            float g = raisedCosine ((float) voice.fadeInPos / (float) fadeIn);

            if (voice.fadeInPos < fadeIn)
                ++voice.fadeInPos;

            if (voice.fadeOutPos >= 0)
            {
                g *= 1.0f - raisedCosine ((float) voice.fadeOutPos / (float) fadeOut);

                if (++voice.fadeOutPos >= fadeOut)
                {
                    releaseVoice (v);
                    continue;
                }
            }

            const float l = audio.getSample (0, voice.position);
            const float r = audio.getSample (juce::jmin (1, audio.getNumChannels() - 1), voice.position);
            left += l * g;
            right += r * g;
            ++voice.position;
            anything = true;
        }

        if (channels >= 2)
        {
            out.addSample (0, s, left * currentGain);
            out.addSample (1, s, right * currentGain);
        }
        else
        {
            out.addSample (0, s, (left + right) * 0.70710678f * currentGain);
        }
    }

    // Progress of the newest sounding voice.
    float p = -1.0f;

    for (const auto& v : voices)
        if (v.clip != nullptr && v.fadeOutPos < 0 && v.clip->audio.getNumSamples() > 0)
            p = (float) v.position / (float) v.clip->audio.getNumSamples();

    progress.store (p, std::memory_order_relaxed);

    if (! anything && voices[0].clip == nullptr && voices[1].clip == nullptr && pending.load() == nullptr)
        active.store (false);
}

//==============================================================================
const PreviewClip* PreviewClipPool::add (std::unique_ptr<PreviewClip> clip)
{
    clips.push_back (std::move (clip));
    return clips.back().get();
}

void PreviewClipPool::collectGarbage (const PreviewPlayer& player)
{
    // Pending first, then the voices: see PreviewPlayer::processBlock.
    const auto* p = player.getPending();
    const auto* a = player.getInUse (0);
    const auto* b = player.getInUse (1);

    clips.erase (std::remove_if (clips.begin(), clips.end(), [&] (const std::unique_ptr<PreviewClip>& c)
    {
        return c.get() != p && c.get() != a && c.get() != b;
    }), clips.end());
}

std::unique_ptr<PreviewClip> PreviewClipPool::makeClip (const juce::AudioBuffer<float>& source, double sourceRate,
                                                        double hostRate, const juce::String& key,
                                                        const juce::String& name)
{
    auto clip = std::make_unique<PreviewClip>();
    clip->key = key;
    clip->name = name;
    clip->sampleRate = hostRate;

    const int inSamples = source.getNumSamples();

    if (std::abs (hostRate - sourceRate) < 0.5 || inSamples == 0)
    {
        clip->audio.makeCopyOf (source);
        clip->sampleRate = sourceRate;

        if (clip->audio.getNumChannels() == 1)
        {
            clip->audio.setSize (2, inSamples, true, false, false);
            clip->audio.copyFrom (1, 0, clip->audio, 0, 0, inSamples);
        }

        return clip;
    }

    const double ratio = sourceRate / hostRate;
    const int outSamples = (int) std::ceil (inSamples / ratio);
    clip->audio.setSize (2, outSamples);
    clip->audio.clear();

    for (int ch = 0; ch < 2; ++ch)
    {
        juce::WindowedSincInterpolator interpolator;
        const int src = juce::jmin (ch, source.getNumChannels() - 1);

        // Padded so the interpolator's look-ahead never reads past the end.
        std::vector<float> padded ((size_t) inSamples + 256, 0.0f);
        std::copy (source.getReadPointer (src), source.getReadPointer (src) + inSamples, padded.begin());

        interpolator.process (ratio, padded.data(), clip->audio.getWritePointer (ch), outSamples);
    }

    return clip;
}

} // namespace luthier
