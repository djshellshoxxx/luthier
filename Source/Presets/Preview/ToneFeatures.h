#pragma once

/*  preset-browser-previews.md 6.2: what a preview sounds like, measured.

    Deterministic, offline, message- or worker-thread only. The analysis runs on
    the preview before its loudness gain, so a feature never depends on where
    the normaliser happened to put the level.

    Also home to the two meters 3.2 step 7 needs: BS.1770-4 integrated loudness
    and the 4x-oversampled true peak.
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace luthier
{

struct ToneFeatures
{
    static constexpr int kNumSpectral = 10;

    double centroidLog2Hz = 0.0;   ///< median spectral centroid, log2 Hz
    double rolloffLog2Hz = 0.0;    ///< median 85 % rolloff, log2 Hz
    double flatness = 0.0;         ///< 1-5 kHz spectral flatness, 0-1 (a grit proxy)
    double lowBand = 0.0;          ///< energy share below 250 Hz
    double midBand = 0.0;          ///< 250 Hz - 2 kHz
    double highBand = 0.0;         ///< above 2 kHz
    double crestDb = 0.0;          ///< peak over RMS
    double attack = 0.0;           ///< mean onset strength, dB per frame
    double tailSeconds = 0.0;      ///< time to fall 30 dB after the last note-off
    double width = 0.0;            ///< 1 minus the L/R correlation
    double loudnessLufs = -70.0;   ///< before normalisation

    bool valid = false;

    /** The ten spectral features, in the order 6.5's vector uses. */
    std::array<double, kNumSpectral> spectralVector() const noexcept
    {
        return { centroidLog2Hz, rolloffLog2Hz, flatness, lowBand, midBand, highBand,
                 crestDb, attack, tailSeconds, width };
    }

    static const char* spectralName (int index) noexcept;

    juce::var toVar() const;
    static ToneFeatures fromVar (const juce::var&);

    /** Measures a stereo (or mono) buffer. `noteEndSeconds` is when the phrase's
        last note-off falls, measured from the buffer start. */
    static ToneFeatures analyse (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                 double noteEndSeconds);

    /** BS.1770-4 integrated loudness (K-weighted, gated), in LUFS. */
    static double integratedLoudness (const juce::AudioBuffer<float>& buffer, double sampleRate);

    /** The true peak over all channels with 4x oversampling, in dBTP. */
    static double truePeakDb (const juce::AudioBuffer<float>& buffer);
};

} // namespace luthier
