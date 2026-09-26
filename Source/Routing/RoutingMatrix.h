#pragma once

/*  Routing state and output distribution (routing-io.md).

    The engine does not know about host buses. It renders its chain and drops
    copies into a TapBuffers. This class owns everything else the routing spec
    asks for - which layout the host negotiated, the per-aux mute/solo/gain, the
    sidechain state and metering, the MIDI-out configuration - and it is what
    actually writes the extra buses on the way out.

    Threading: the settings are plain values written from the message thread and
    read from the audio thread. Each one is a single word, each one is benign if
    it is read a block late, and none of them can be torn into an invalid state,
    so they are atomics rather than a lock. The gain trims are smoothed on the
    audio thread so a fader move cannot click.
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include "TapBuffers.h"
#include "../DSP/Common/DspCommon.h"

#include <array>
#include <atomic>

namespace luthier
{

//==============================================================================
/** The four bus layouts of routing-io 1, in the order they are advertised. */
enum class BusLayout
{
    stereoOnly = 0,   // A: main stereo
    studio,           // B: main + 7 aux stereo
    perString,        // C: main + 12 mono
    full              // D: main + 7 aux stereo + 12 mono
};

const char* getBusLayoutName (BusLayout layout) noexcept;

//==============================================================================
/** Which internal event streams are echoed to MIDI out (routing-io 6). */
struct MidiOutConfig
{
    bool enabled = false;
    bool passThrough = true;
    bool rhythmEngine = false;
    bool stringActivity = false;
    bool ccBroadcast = false;

    // midi-export.md 6's remaining sources.
    bool tunePlayback = false;     ///< the tune builder's playback
    bool luthierEvents = false;    ///< character / noise events as Luthier SysEx
    bool workshopChanges = false;  ///< part swaps and bench moves as Luthier SysEx

    // jam-mode.md 9 (FEAT-JAM): the Jam band, drums on GM channel 10, bass on 11.
    bool jamParts = false;
    int jamDrumChannel = 10, jamBassChannel = 11;

    /** CC number each macro is echoed on, or -1 for "not assigned". One per
        macro parameter (ParamIDs::kNumMacros); the routing panel shows them all. */
    static constexpr int kNumMacroCcs = 8;
    std::array<int, kNumMacroCcs> macroCc { { -1, -1, -1, -1, -1, -1, -1, -1 } };

    int channel = 1;

    bool operator== (const MidiOutConfig& o) const noexcept
    {
        return enabled == o.enabled && passThrough == o.passThrough && rhythmEngine == o.rhythmEngine
            && stringActivity == o.stringActivity && ccBroadcast == o.ccBroadcast
            && tunePlayback == o.tunePlayback && luthierEvents == o.luthierEvents
            && workshopChanges == o.workshopChanges && macroCc == o.macroCc && channel == o.channel
            && jamParts == o.jamParts && jamDrumChannel == o.jamDrumChannel && jamBassChannel == o.jamBassChannel;
    }

    bool operator!= (const MidiOutConfig& o) const noexcept { return ! operator== (o); }
};

//==============================================================================
class RoutingMatrix
{
public:
    RoutingMatrix();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    //==========================================================================
    // Layout, as negotiated with the host.

    void setActiveLayout (BusLayout layout) noexcept { activeLayout = layout; }
    BusLayout getActiveLayout() const noexcept { return activeLayout; }

    static bool layoutHasAux (BusLayout l) noexcept
    {
        return l == BusLayout::studio || l == BusLayout::full;
    }

    static bool layoutHasPerString (BusLayout l) noexcept
    {
        return l == BusLayout::perString || l == BusLayout::full;
    }

    //==========================================================================
    // Aux bus strip.

    void setAuxMuted (int bus, bool muted) noexcept;
    bool isAuxMuted (int bus) const noexcept;

    void setAuxSoloed (int bus, bool soloed) noexcept;
    bool isAuxSoloed (int bus) const noexcept;

    void setAuxGainDb (int bus, double db) noexcept;
    double getAuxGainDb (int bus) const noexcept;

    bool isAnyAuxSoloed() const noexcept;

    /** Mute and solo resolved together: an aux is audible if nothing is soloed
        and it is not muted, or if it is itself soloed. */
    bool isAuxAudible (int bus) const noexcept;

    /** Peak of the last block on this aux, for the meters. */
    double getAuxLevel (int bus) const noexcept;

    //==========================================================================
    // Per-string strip.

    void setPerStringGainDb (int stringIndex, double db) noexcept;
    double getPerStringGainDb (int stringIndex) const noexcept;
    void setPerStringMuted (int stringIndex, bool muted) noexcept;
    bool isPerStringMuted (int stringIndex) const noexcept;

    //==========================================================================
    // Sidechain (routing-io 4 and 5B).

    void setSidechainToAmp (bool on) noexcept { sidechainToAmp.store (on, std::memory_order_relaxed); }
    bool isSidechainToAmp() const noexcept { return sidechainToAmp.load (std::memory_order_relaxed); }

    void setSidechainPresent (bool present) noexcept { sidechainPresent.store (present, std::memory_order_relaxed); }
    bool isSidechainPresent() const noexcept { return sidechainPresent.load (std::memory_order_relaxed); }

    /** Updates the sidechain meter. Audio thread. */
    void meterSidechain (const float* const* channels, int numChannels, int numSamples) noexcept;
    double getSidechainLevel() const noexcept { return sidechainLevel.load (std::memory_order_relaxed); }

    //==========================================================================
    // MIDI out.

    void setMidiOutConfig (const MidiOutConfig& cfg);
    MidiOutConfig getMidiOutConfig() const;

    //==========================================================================
    /** Tells the tap buffers what is worth rendering this block. Call before the
        engine renders. */
    void updateWantedTaps (TapBuffers& taps, int numStrings) noexcept;

    /** Writes the aux and per-string buses of `buffer` from the taps, applying
        gain, mute and solo. `buffer` must be the full host buffer, and
        `getBusBuffer` is used to find each extra bus. Audio thread. */
    void distribute (juce::AudioProcessor& processor,
                     juce::AudioBuffer<float>& buffer,
                     const TapBuffers& taps,
                     int numStrings,
                     const double* noiseBus = nullptr) noexcept;

    /** Writes the monitor mix onto the monitor aux bus (live-performance 7).

        The monitor bus is the one aux the engine does not tap: it is built after
        the master, out of the main output plus the sidechain, so it is written
        here rather than read from a tap. It still passes through the same gain,
        mute and solo as every other aux, so the routing panel's monitor strip
        behaves like the rest of them. Does nothing if the host has not enabled
        the bus. Audio thread. */
    void writeMonitorBus (juce::AudioProcessor& processor,
                          juce::AudioBuffer<float>& buffer,
                          const juce::AudioBuffer<float>& monitor,
                          int numSamples) noexcept;

    /** jam-mode 7 (FEAT-JAM): writes Aux 9 "Jam Drums" and Aux 10 "Jam Bass"
        from the Jam mixer's stems, through their strips. Returns false when
        the layout has neither (Separate then falls back to Main). */
    bool writeJamBuses (juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer,
                        const float* const* drums, const float* const* bass, int numSamples) noexcept;

    //==========================================================================
    // Latency (routing-io 7).

    struct LatencyReport
    {
        int mainOut = 0;
        int auxDi = 0;
        int auxPreCab = 0;
        int perString = 0;
        int auxNoise = 0;      ///< Aux 8, the noise bus (performance-budget.md 4)
    };

    void setLatencyReport (const LatencyReport& r) noexcept;
    LatencyReport getLatencyReport() const noexcept;

    /** What is handed to the host when only one number is accepted: the largest,
        which is the main output's. */
    int getReportedLatency() const noexcept { return getLatencyReport().mainOut; }

    //==========================================================================
    // Preset state (routing-io 9). The layout is deliberately not stored: the
    // host owns the bus layout, and a preset that tried to change it would be
    // asking for something it has no right to.

    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    struct AuxState
    {
        std::atomic<bool> muted { false };
        std::atomic<bool> soloed { false };
        std::atomic<float> gainDb { 0.0f };
        std::atomic<float> level { 0.0f };
        ExpSmoother gain;
    };

    std::array<AuxState, kNumAuxStrips> auxes;

    std::array<std::atomic<float>, kNumPerStringBuses> stringGainDb;
    std::array<std::atomic<bool>, kNumPerStringBuses> stringMuted;
    std::array<ExpSmoother, kNumPerStringBuses> stringGain;

    std::atomic<bool> sidechainToAmp { false };
    std::atomic<bool> sidechainPresent { false };
    std::atomic<double> sidechainLevel { 0.0 };

    // Read by the audio thread every block: a SpinLock around a small struct
    // copy, not a CriticalSection it could block on behind the UI.
    mutable juce::SpinLock midiOutLock;
    MidiOutConfig midiOut;

    std::atomic<int> latMain { 0 }, latDi { 0 }, latPreCab { 0 }, latString { 0 }, latNoise { 0 };

    BusLayout activeLayout = BusLayout::stereoOnly;

    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingMatrix)
};

} // namespace luthier
