#pragma once

/*  preset-browser-previews.md 4.1: the clips the player may be holding.

    A PreviewClip is immutable once made: a stereo buffer already at the host
    rate. The message thread makes one, adds it here and hands the pointer to
    the player; the pool deletes it only once it is neither pending nor in use
    by either voice (the GuitarSpec swap pattern, ui-wiring.md 4.3). The audio
    thread never allocates or frees one.
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

namespace luthier
{

struct PreviewClip
{
    juce::AudioBuffer<float> audio;   ///< stereo, at `sampleRate`
    double sampleRate = 48000.0;
    juce::String key;                 ///< the preset the clip belongs to
    juce::String name;                ///< for the "Previewing <name>" announcement
};

class PreviewPlayer;

class PreviewClipPool
{
public:
    PreviewClipPool() = default;

    /** Takes ownership and returns the pointer to hand to the player. */
    const PreviewClip* add (std::unique_ptr<PreviewClip> clip);

    /** Frees every clip the player no longer refers to. Message thread. */
    void collectGarbage (const PreviewPlayer& player);

    /** Frees everything regardless: only once the audio thread is stopped. */
    void clear() { clips.clear(); }

    int size() const noexcept { return (int) clips.size(); }

    /** Makes a host-rate clip from a 48 kHz one with WindowedSincInterpolator. */
    static std::unique_ptr<PreviewClip> makeClip (const juce::AudioBuffer<float>& source, double sourceRate,
                                                  double hostRate, const juce::String& key,
                                                  const juce::String& name);

private:
    std::vector<std::unique_ptr<PreviewClip>> clips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreviewClipPool)
};

} // namespace luthier
