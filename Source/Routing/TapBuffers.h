#pragma once

/*  Tap-point storage for the auxiliary and per-string outputs (routing-io 2, 3).

    The engine renders one signal chain. Every point along that chain which the
    routing spec exposes as its own output is copied into this object as it is
    produced, and the processor hands those copies to the host's extra buses.

    Two rules shape the design:

      1. Nothing here allocates once prepare() has run. The buffers are sized
         for the largest block the engine was prepared for and then only ever
         re-interpreted at a shorter length.

      2. A tap that nobody is listening to is not written. `wanted` is set by
         the processor from the negotiated bus layout and the mute state, and
         the engine checks it before doing any work that exists only to feed a
         tap. That is what makes "muted aux buses do not process their tap
         point" true rather than aspirational.
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/Common/DspCommon.h"

#include <array>

namespace luthier
{

//==============================================================================
/** The seven auxiliary stereo buses, in the order routing-io.md lists them. */
enum class AuxBus
{
    di = 0,         // raw pickup output, post-cable-sim, pre-amp
    ampPreCab,      // amp output before the cabinet
    cabMic1,        // cabinet mic 1 alone
    cabMic2,        // cabinet mic 2 alone
    roomMic,        // room mics alone
    wetFx,          // reverb and delay tails alone
    monitor         // the monitor bus from live-performance.md
};

inline constexpr int kNumAuxBuses = 7;
inline constexpr int kNumPerStringBuses = kMaxStrings;

const char* getAuxBusName (int index) noexcept;
const char* getAuxBusTapDescription (int index) noexcept;

//==============================================================================
class TapBuffers
{
public:
    TapBuffers() { wanted.fill (false); }

    /** Allocates for the worst case. Message thread only. */
    void prepare (int maxBlockSize)
    {
        maxBlock = juce::jmax (1, maxBlockSize);

        aux.setSize (kNumAuxBuses * 2, maxBlock, false, true, false);
        perString.setSize (kNumPerStringBuses, maxBlock, false, true, false);

        aux.clear();
        perString.clear();

        active = 0;
    }

    void releaseResources()
    {
        aux.setSize (0, 0);
        perString.setSize (0, 0);
        maxBlock = 0;
        active = 0;
    }

    /** Called once per host block, before the engine runs. Clears only what is
        wanted, so an unused tap costs nothing at all.

        If the host hands the engine a larger block than it promised in
        prepareToPlay, the engine splits it and these buffers cannot hold the
        whole thing. Rather than allocate on the audio thread, the tail of that
        one block is left silent on the aux buses; the main output is unaffected.
        Hosts that keep their promise never take that path. */
    void beginBlock (int numSamples) noexcept
    {
        active = juce::jlimit (0, maxBlock, numSamples);
        offset = 0;

        if (active <= 0)
            return;

        for (int bus = 0; bus < kNumAuxBuses; ++bus)
            if (wanted[(size_t) bus])
                for (int ch = 0; ch < 2; ++ch)
                    juce::FloatVectorOperations::clear (aux.getWritePointer (bus * 2 + ch), active);

        if (perStringWanted)
            for (int s = 0; s < kNumPerStringBuses; ++s)
                juce::FloatVectorOperations::clear (perString.getWritePointer (s), active);
    }

    int getNumSamples() const noexcept { return active; }
    int getMaxBlockSize() const noexcept { return maxBlock; }

    /** Where the next sub-block's taps land. The engine advances this as it
        splits an oversized block, so the taps stay contiguous. */
    void setWriteOffset (int newOffset) noexcept { offset = juce::jlimit (0, active, newOffset); }
    int getWriteOffset() const noexcept { return offset; }

    /** How many samples a sub-block starting at the current offset may write. */
    int getRoomAtOffset() const noexcept { return juce::jmax (0, active - offset); }

    //==========================================================================
    void setAuxWanted (int bus, bool w) noexcept
    {
        if (juce::isPositiveAndBelow (bus, kNumAuxBuses))
            wanted[(size_t) bus] = w;
    }

    bool isAuxWanted (int bus) const noexcept
    {
        return juce::isPositiveAndBelow (bus, kNumAuxBuses) && wanted[(size_t) bus];
    }

    bool isAuxWanted (AuxBus bus) const noexcept { return isAuxWanted ((int) bus); }

    void setPerStringWanted (bool w) noexcept { perStringWanted = w; }
    bool isPerStringWanted() const noexcept { return perStringWanted; }

    void setNothingWanted() noexcept
    {
        wanted.fill (false);
        perStringWanted = false;
    }

    //==========================================================================
    float* auxWrite (AuxBus bus, int channel) noexcept
    {
        return aux.getWritePointer ((int) bus * 2 + juce::jlimit (0, 1, channel)) + offset;
    }

    const float* auxRead (int bus, int channel) const noexcept
    {
        return aux.getReadPointer (juce::jlimit (0, kNumAuxBuses - 1, bus) * 2
                                     + juce::jlimit (0, 1, channel));
    }

    float* stringWrite (int stringIndex) noexcept
    {
        return perString.getWritePointer (juce::jlimit (0, kNumPerStringBuses - 1, stringIndex)) + offset;
    }

    const float* stringRead (int stringIndex) const noexcept
    {
        return perString.getReadPointer (juce::jlimit (0, kNumPerStringBuses - 1, stringIndex));
    }

    //==========================================================================
    /** Copies a mono chain signal into both sides of an aux bus. */
    void writeAuxMono (AuxBus bus, const double* src, int numSamples) noexcept
    {
        if (! isAuxWanted ((int) bus))
            return;

        const int n = juce::jmin (numSamples, getRoomAtOffset());
        auto* l = auxWrite (bus, 0);
        auto* r = auxWrite (bus, 1);

        for (int i = 0; i < n; ++i)
        {
            const auto v = (float) sanitise (src[(size_t) i]);
            l[i] = v;
            r[i] = v;
        }
    }

    void writeAuxStereo (AuxBus bus, const float* srcL, const float* srcR, int numSamples) noexcept
    {
        if (! isAuxWanted ((int) bus))
            return;

        const int n = juce::jmin (numSamples, getRoomAtOffset());
        juce::FloatVectorOperations::copy (auxWrite (bus, 0), srcL, n);
        juce::FloatVectorOperations::copy (auxWrite (bus, 1), srcR, n);
    }

    /** Aux 6 is the wet effects alone, which is the difference between the
        signal with the tails and the same signal without them. */
    void writeAuxDifference (AuxBus bus, const float* wetL, const float* wetR,
                             const float* dryL, const float* dryR, int numSamples) noexcept
    {
        if (! isAuxWanted ((int) bus))
            return;

        const int n = juce::jmin (numSamples, getRoomAtOffset());
        auto* l = auxWrite (bus, 0);
        auto* r = auxWrite (bus, 1);

        for (int i = 0; i < n; ++i)
        {
            l[i] = (float) sanitise ((double) wetL[i] - (double) dryL[i]);
            r[i] = (float) sanitise ((double) wetR[i] - (double) dryR[i]);
        }
    }

private:
    juce::AudioBuffer<float> aux;
    juce::AudioBuffer<float> perString;

    std::array<bool, kNumAuxBuses> wanted {};
    bool perStringWanted = false;

    int maxBlock = 0;
    int active = 0;
    int offset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapBuffers)
};

} // namespace luthier
