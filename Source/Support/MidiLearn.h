#pragma once

/*  MIDI Learn (build spec: right-click any control -> MIDI Learn -> move a CC).

    Holds the CC-to-parameter map, the "armed" state while the user is teaching a
    control, and the application of incoming CCs to parameters. The map is stored
    with the plugin state, not with the preset, so a controller setup survives
    changing sounds - with an option to store it per-preset for users who want it.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace luthier
{

class MidiLearnManager : public juce::ChangeBroadcaster,
                         private juce::AsyncUpdater
{
public:
    struct Mapping
    {
        juce::String parameterId;
        int    ccNumber = -1;
        int    channel = 0;        ///< 0 = omni.
        double rangeMin = 0.0;
        double rangeMax = 1.0;
        bool   inverted = false;
        bool   global = false;     ///< SPEC-SWEEP UW-29: kept across presets, in the user's settings
    };

    /*  SPEC-SWEEP (UW-29, ui-wiring 8): a mapping can be global - saved in the
        user's settings rather than the session or preset, and merged under
        whatever a loaded preset maps (a preset's own mapping for the same
        parameter or source wins). */
    void setMappingGlobal (const juce::String& parameterId, bool isGlobal);
    bool isMappingGlobal (const juce::String& parameterId) const;
    static juce::File getGlobalMappingsFile();
    static void setGlobalMappingsFileForTesting (const juce::File& file);

    /*  SPEC-SWEEP (IR-4, input-routing 1.1 step 2): what a mapping listens to.
        `Mapping::ccNumber` is a source key: 0-127 are CCs (as they always were,
        so saved maps keep their meaning), then program changes, channel
        pressure, poly aftertouch per note, and notes. */
    static constexpr int kProgramBase     = 128;   ///< + program number
    static constexpr int kChannelPressure = 256;
    static constexpr int kPolyBase        = 257;   ///< + note
    static constexpr int kNoteBase        = 385;   ///< + note
    static constexpr int kNumSources      = 513;

    /** The source key a message maps to (value 0..1 in @p value), or -1. Notes
        count only when @p includeNotes; a note-off is value 0. */
    static int sourceKeyFor (const juce::MidiMessage& message, double& value, bool includeNotes) noexcept;

    /** "CC 7", "Program 12", "Pressure", "Poly AT C3", "Note C3". */
    static juce::String describeSource (int key);

    /** Options > MIDI "Learn notes too": whether a note can be learned (and a
        note mapping plays its parameter). Off by default: notes play. */
    void setLearnNotes (bool shouldLearnNotes) noexcept { learnNotes.store (shouldLearnNotes); }
    bool getLearnNotes() const noexcept { return learnNotes.load(); }

    explicit MidiLearnManager (juce::AudioProcessorValueTreeState& state);
    ~MidiLearnManager() override;

    //==========================================================================
    /** Arms learning for a parameter. The next CC received is mapped to it. */
    void startLearning (const juce::String& parameterId);
    void cancelLearning();
    bool isLearning() const noexcept { return learning.load(); }
    juce::String getLearningParameterId() const;

    /*  Global arm (gui-integration.md section 19, and its ground rule 4: nothing
        may be reachable only by right-click).

        Armed, the next control the user touches becomes the learn target, which
        is what the header button and Ctrl+L drive. Arming does not pick a
        parameter - a control does that by calling claimArmedLearn() when it is
        clicked - so this is a separate state from `learning`. */
    void setArmed (bool shouldBeArmed);
    bool isArmed() const noexcept { return armed.load(); }

    /** Called by a control when it is clicked. If armed, starts learning for this
        parameter, disarms, and returns true so the control swallows the click. */
    bool claimArmedLearn (const juce::String& parameterId);

    /*  SPEC-SWEEP: ER-38, error-recovery 6: an arm or a learn that has waited
        kArmTimeoutMs without catching a CC is cancelled. The window calls this
        from its timer with the millisecond counter; returns true when it
        cancelled, so the window can say so. */
    static constexpr juce::uint32 kArmTimeoutMs = 30000;
    bool expireIfIdle (juce::uint32 nowMs);

    //==========================================================================
    void addMapping (const juce::String& parameterId, int ccNumber, int channel = 0);
    void removeMappingForParameter (const juce::String& parameterId);
    void removeMappingForCc (int ccNumber);
    void clearAllMappings();

    /** The CC mapped to a parameter, or -1. */
    int getCcForParameter (const juce::String& parameterId) const;

    /** The parameter mapped to a CC, or an empty string. */
    juce::String getParameterForCc (int ccNumber) const;

    int getNumMappings() const;
    Mapping getMapping (int index) const;

    void setMappingRange (const juce::String& parameterId, double min, double max, bool inverted);

    //==========================================================================
    /** Called from processBlock with the block's MIDI. Applies mapped CCs to
        parameters and captures a CC while learning. Real-time safe: parameter
        writes use setValueNotifyingHost, which is designed for this. */
    void processMidi (const juce::MidiBuffer& midi) noexcept;

    /** SPEC-SWEEP (IR-3, input-routing 5): as above, and while learning the CC
        that is learned is taken out of @p midi, so the gesture that assigns a
        control does not also play the instrument. @p scratch must be pre-sized
        (ensureSize) by the caller; nothing allocates. */
    void processMidi (juce::MidiBuffer& midi, juce::MidiBuffer& scratch) noexcept;

    /** Message thread: finishes a learn the audio thread caught now, rather than
        when the async update arrives (tests, and anything that cannot wait). */
    void dispatchPendingLearn() { handleUpdateNowIfNeeded(); }

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& data);

private:
    juce::AudioProcessorValueTreeState& apvts;

    mutable juce::CriticalSection lock;
    juce::Array<Mapping> mappings;

    std::atomic<bool> learning { false };
    std::atomic<bool> armed { false };
    juce::uint32 waitingSinceMs = 0;   // SPEC-SWEEP: ER-38, message thread
    juce::String learningParameter;

    /*  What the audio thread reads: one plain entry per CC, rebuilt on the
        message thread under tableLock, which the audio thread only try-locks.
        It used to index `mappings` itself without the lock, while the message
        thread cleared or reallocated it (a crash on a CC during a state reload
        or a mapping edit), and to look the parameter up by its String id. */
    struct LookupEntry
    {
        juce::AudioProcessorParameter* parameter = nullptr;
        int    channel = 0;
        double rangeMin = 0.0;
        double rangeMax = 1.0;
        bool   inverted = false;
    };

    juce::SpinLock tableLock;
    std::array<LookupEntry, kNumSources> lookup {};   // SPEC-SWEEP IR-4: every source kind
    std::atomic<bool> learnNotes { false };

    juce::Array<Mapping> globalMappings;   // SPEC-SWEEP UW-29, under `lock`
    void loadGlobalMappings();
    void saveGlobalMappings() const;
    void mergeGlobalMappings();   // under `lock`

    /** A CC caught while learning, handed to the message thread (-1 = none). */
    std::atomic<int> learnedCc { -1 };

    void rebuildLookup() noexcept;
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnManager)
};

} // namespace luthier
