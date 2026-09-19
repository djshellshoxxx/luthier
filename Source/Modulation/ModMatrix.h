#pragma once

/*  The modulation matrix (modulation-matrix.md sections 0, 2, 3, 4, 6).

    Shape of the thing:

      - Every source writes one float into a flat slot array, once per
        control-rate tick. Slots are identified by an integer, and the interned
        string ids from the spec ("lfo1", "cc74") are resolved to those integers
        once, when a route is compiled - never on the audio thread.

      - Every destination is an APVTS parameter, addressed by its index in the
        processor's parameter list. Modulation offsets live in a flat array of
        the same length, so applying modulation to a parameter is one array read
        and a clamp.

      - Routes are compiled on the message thread into a plain vector of
        integers and floats, and handed to the audio thread by flipping an
        atomic index between two tables. The audio thread never takes a lock,
        never allocates and never touches a juce::String.

    Interpolation: the spec asks for destinations to interpolate linearly
    between control-rate ticks. The offsets array holds the interpolated value,
    stepped once per tick toward the newly computed target, so a parameter read
    at any point in the block sees a value on the ramp rather than a stair.
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include "ModSources.h"

#include <atomic>
#include <vector>

namespace luthier
{

//==============================================================================
/** Slot numbering for every modulation source. Contiguous so the audio thread
    can index an array rather than look anything up. */
namespace ModSourceSlots
{
    inline constexpr int lfoBase        = 0;    inline constexpr int numLfos = 8;
    inline constexpr int envBase        = 8;    inline constexpr int numEnvelopes = 4;
    inline constexpr int seqBase        = 12;   inline constexpr int numSequencers = 2;
    inline constexpr int followerBase   = 14;   inline constexpr int numFollowers = 2;

    inline constexpr int notePitch      = 16;
    inline constexpr int noteVelocity   = 17;
    inline constexpr int noteTrigger    = 18;
    inline constexpr int notesHeld      = 19;
    inline constexpr int aftertouch     = 20;
    inline constexpr int polyAftertouch = 21;

    inline constexpr int macroBase      = 22;   inline constexpr int numMacros = 8;

    inline constexpr int randomPerNote  = 30;
    inline constexpr int randomPerBar   = 31;
    inline constexpr int randomSmooth   = 32;

    inline constexpr int pitchBend      = 33;
    inline constexpr int modWheel       = 34;
    inline constexpr int channelPressure = 35;

    inline constexpr int ccBase         = 36;   inline constexpr int numCcs = 128;

    /** 14-bit CC pairs: entry n is MSB controller n, LSB controller n + 32. */
    inline constexpr int cc14Base       = 164;  inline constexpr int numCc14 = 32;

    inline constexpr int count          = 196;
}

/** Resolves an interned source id to its slot, or -1 if it names nothing. */
int modSourceSlotForId (const juce::String& sourceId) noexcept;

/** The canonical id for a slot, as it is written into a preset. */
juce::String modSourceIdForSlot (int slot);

/** A human-readable name for the UI. */
juce::String modSourceDisplayName (int slot);

//==============================================================================
/** One row of the routing table (modulation-matrix 3). */
struct ModRoute
{
    juce::String sourceId;
    int sourceChannel = 0;
    juce::String destinationId;
    float depth = 0.0f;    // -1 .. +1
    float offset = 0.0f;   // -1 .. +1
    ModCurve curve = ModCurve::linear;
    bool enabled = true;
};

//==============================================================================
/** What the matrix needs to know about the block it is about to modulate. */
struct ModBlockContext
{
    double bpm = 120.0;
    double positionBeats = -1.0;      // negative when the host is not playing
    bool transportRunning = false;
    bool transportJustStarted = false;

    // Signals the envelope followers can watch.
    double mainOutputPeak = 0.0;
    double mainOutputMeanSquare = 0.0;
    double sidechainPeak = 0.0;
    double sidechainMeanSquare = 0.0;
    double pickupPeak = 0.0;
    double pickupMeanSquare = 0.0;

    std::array<double, kMaxStrings> perStringPeak {};
};

//==============================================================================
class ModMatrix
{
public:
    /** The spec's stress test uses a thousand routes, so the ceiling is set
        above it rather than at it. */
    static constexpr int kMaxRoutes = 1024;

    /** modulation-matrix 0.4: a destination can be driven by up to eight
        sources at once. Routes past the eighth for a destination are rejected
        when they are added, rather than silently ignored later. */
    static constexpr int kMaxRoutesPerDestination = 8;

    ModMatrix();
    ~ModMatrix();

    //==========================================================================
    void prepare (double sampleRate, int blockSize,
                  juce::AudioProcessorValueTreeState& state);

    void reset() noexcept;
    void releaseResources();

    /** Control rate in samples, as modulation-matrix 0.1 defines it: a
        thirty-second of the block size, floored at 128. */
    int getControlRateSamples() const noexcept { return controlRateSamples; }
    double getControlRateHz() const noexcept { return controlRateHz; }

    //==========================================================================
    // Routes. Message thread only.

    /** Adds a route, returning false if the table is full, the ids do not
        resolve, or the destination already has its eight sources. */
    bool addRoute (const ModRoute& route);

    void removeRoute (int index);
    void clearRoutes();

    void setRouteEnabled (int index, bool enabled);
    void setRouteDepth (int index, float depth);
    void setRouteOffset (int index, float offset);
    void setRouteCurve (int index, ModCurve curve);

    int getNumRoutes() const;
    ModRoute getRoute (int index) const;
    std::vector<ModRoute> getRoutes() const;

    /** Replaces the whole table at once, as a preset load does. Routes that do
        not resolve are dropped and counted. */
    void setRoutes (const std::vector<ModRoute>& routes);
    int getNumDroppedOnLoad() const noexcept { return droppedOnLoad; }
    juce::StringArray getUnknownDestinations() const;

    /** How many routes currently drive this destination. */
    int getRouteCountForDestination (const juce::String& destinationId) const;

    //==========================================================================
    // Per-block work. Audio thread.

    /** Advances every source and recomputes the destination offsets. Call once
        per block, before the parameter bridge reads anything. */
    void processBlock (int numSamples, const ModBlockContext& context) noexcept;

    /** Applies the accumulated modulation for a parameter and clamps to its
        range. `parameterIndex` is the parameter's index in the processor's
        parameter list; -1 means "no modulation", which is the fast path for the
        overwhelming majority of parameters. */
    float apply (int parameterIndex, float baseValue) const noexcept;

    /** True when anything at all is routed, so callers can skip the work
        entirely on the common case of an unmodulated preset. */
    bool isActive() const noexcept { return anyRoutes.load (std::memory_order_relaxed); }

    /** True when this specific parameter has at least one route. */
    bool isDestinationModulated (int parameterIndex) const noexcept;

    /** The current modulation offset for a parameter, in parameter units, for
        the coloured arcs the UI draws around modulated controls. */
    float getOffsetFor (int parameterIndex) const noexcept;

    //==========================================================================
    // Events the sources react to.

    void noteOn (int midiNote, double velocity) noexcept;
    void noteOff() noexcept;
    void allNotesOff() noexcept;
    void setAftertouch (double value) noexcept;
    void setPolyAftertouch (double value) noexcept;
    void setPitchBend (double bipolar) noexcept;
    void setControllerValue (int ccNumber, double value) noexcept;
    void setMacroValue (int macroIndex, double value) noexcept;

    //==========================================================================
    ModLfo& getLfo (int index) noexcept;
    ModEnvelope& getEnvelope (int index) noexcept;
    ModStepSequencer& getSequencer (int index) noexcept;
    ModEnvelopeFollower& getFollower (int index) noexcept;
    ModRandomSource& getRandomSource() noexcept { return randomSource; }

    /** The live value of a source slot, for the UI's source cards. */
    float getSourceValue (int slot) const noexcept;

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    struct CompiledRoute
    {
        int sourceSlot = 0;
        int destinationIndex = 0;
        float depth = 0.0f;
        float offset = 0.0f;
        ModCurve curve = ModCurve::linear;
    };

    struct DestinationInfo
    {
        float minimum = 0.0f;
        float maximum = 1.0f;
        float range = 1.0f;
        bool discrete = false;
        int numSteps = 0;
    };

    /** Two tables, one live and one being built, so the message thread can
        rebuild the routing without the audio thread ever waiting. */
    struct Table
    {
        std::vector<CompiledRoute> routes;
        std::vector<int> touchedDestinations;
    };

    void rebuildTable();
    int destinationIndexFor (const juce::String& parameterId) const;
    void updateSources (const ModBlockContext& context) noexcept;

    juce::AudioProcessorValueTreeState* apvts = nullptr;

    double sampleRate = 44100.0;
    int blockSizeSamples = 512;
    int controlRateSamples = 128;
    double controlRateHz = 344.0;

    // --- sources ---------------------------------------------------------------
    std::array<ModLfo, ModSourceSlots::numLfos> lfos;
    std::array<ModEnvelope, ModSourceSlots::numEnvelopes> envelopes;
    std::array<ModStepSequencer, ModSourceSlots::numSequencers> sequencers;
    std::array<ModEnvelopeFollower, ModSourceSlots::numFollowers> followers;
    ModRandomSource randomSource;

    std::array<std::atomic<float>, ModSourceSlots::count> sourceValues;

    // Note-derived and controller state, written from the audio thread.
    double lastNotePitch = 0.5, lastNoteVelocity = 0.0;
    int notesHeldCount = 0;
    int noteTriggerTicks = 0;
    double aftertouchValue = 0.0, polyAftertouchValue = 0.0, pitchBendValue = 0.0;
    std::array<std::atomic<float>, 128> ccValues;
    std::array<std::atomic<float>, ModSourceSlots::numMacros> macroValues;

    double lastBarPosition = -1.0;

    // --- destinations -----------------------------------------------------------
    std::vector<DestinationInfo> destinations;
    std::vector<std::atomic<float>> currentOffsets;
    std::vector<std::atomic<float>> targetOffsets;
    std::vector<std::atomic<bool>> destinationModulated;

    juce::HashMap<juce::String, int> parameterIndexById;

    // --- routes ------------------------------------------------------------------
    mutable juce::CriticalSection routeLock;
    std::vector<ModRoute> routes;
    juce::StringArray unknownDestinations;
    int droppedOnLoad = 0;

    Table tables[2];
    std::atomic<int> liveTable { 0 };
    std::atomic<bool> anyRoutes { false };

    int samplesUntilTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModMatrix)
};

} // namespace luthier
