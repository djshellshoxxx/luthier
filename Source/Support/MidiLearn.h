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
    };

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
    std::array<LookupEntry, 128> lookup {};

    /** A CC caught while learning, handed to the message thread (-1 = none). */
    std::atomic<int> learnedCc { -1 };

    void rebuildLookup() noexcept;
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnManager)
};

} // namespace luthier
