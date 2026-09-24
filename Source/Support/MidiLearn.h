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

#include "ThreadProbe.h"

namespace luthier
{

class MidiLearnManager : public juce::ChangeBroadcaster,
                         private juce::Timer
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

    /*  performance-budget.md 0.4 / 0.5: the audio thread publishes a learned CC
        in pendingLearnCc and nothing else; this, on the message thread, maps it.
        The timer that learning starts calls it; tests may call it directly. */
    void servicePendingLearn();
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

    /*  action-and-undo.md 3.12: called on the message thread just before a
        learn completes (parameter, CC), so the owner can push an undo entry. */
    std::function<void (const juce::String&, int)> onBeforeLearn;

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

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& data);

private:
    juce::AudioProcessorValueTreeState& apvts;

    mutable ThreadProbe::ProbedCriticalSection lock;
    juce::Array<Mapping> mappings;

    std::atomic<bool> learning { false };
    std::atomic<bool> armed { false };
    juce::String learningParameter;

    std::atomic<int> pendingLearnCc { -1 };   // performance-budget.md 0.4
    void timerCallback() override { servicePendingLearn(); }

    // Lock-free lookup used on the audio thread: CC number to mapping index.
    std::array<std::atomic<int>, 128> ccToMapping {};

    void rebuildLookup() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnManager)
};

} // namespace luthier
