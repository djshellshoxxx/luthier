#pragma once

/*  cpu-quality-modes.md 6: AnimationPolicy, the one motion switch.

    Everything on screen that moves asks this class how much it may move. Its
    inputs are Reduced motion (AccessibilitySettings), the most restrictive CPU
    quality level among the open editors in the process, and the load
    governor's relief level (7). Its output is one of three motion levels:

        condition                        motion   Decorative   Transition   LiveReadout
        High, no reduced motion, relief 0  Full    <= 60 Hz     as asked     as asked
        Medium, or relief 1              Limited  <= 30 Hz     as asked     as asked
        Reduced motion                   Off      none         0 ms         as asked
        Low, or relief >= 2              Off      none         0 ms         <= 10 Hz, stepped

    Classes of motion:
      Decorative   motion for feel (glows, pulses, animated strings, scopes)
      Transition   an ease between two states (fades, slides, tweens)
      LiveReadout  a value that updates (meters, playheads, CPU %)

    ---------------------------------------------------------------------------
    ADOPTING IT (every animated component must; CQ-22 fails the build if not)
    ---------------------------------------------------------------------------
    1. Hold a Registration as a member, and let it drive your timer:

           class MyMeter : public juce::Component, private juce::Timer
           {
               AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "MyMeter" };

               MyMeter()               { motion.startTimerHz (*this, 30); }   // not startTimerHz (30)
               void paint (juce::Graphics& g) override
               {
                   AnimationPolicy::notePaint (*this);                          // first line of paint()
                   ...
               }
           };

       The registration starts, retunes or stops the timer whenever the policy
       changes (a Decorative component at Off has no running timer at all).

    2. Ask the policy for the details where they matter:
         AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition)   - fade or jump?
         AnimationPolicy::get().transitionMs (80)                            - ease length
         AnimationPolicy::get().isReadoutStepped()                           - no ballistics
         AnimationPolicy::get().getStringsStyle()                            - animated strings

    3. A component that is static at Off but must still notice a state change
       (the fretboard's sounding frets) passes an `onStaticPoll` callback: the
       policy's own 4 Hz poll calls it while the component's timer is stopped,
       and the component repaints only if what it shows really changed.

    4. A timer that only polls state (undo buttons, auto-dismiss, option
       refresh) is not animation: add its class to kPollOnlyAllowList in
       AnimationPolicy.cpp with a one-line reason instead.

    Message thread only, except notePaint's counter.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Support/QualityProfile.h"
#include <functional>
#include <map>
#include <atomic>

namespace luthier
{

class AnimationPolicy : private juce::ChangeListener,
                        private juce::Timer
{
public:
    enum MotionClass { Decorative = 0, Transition = 1, LiveReadout = 2 };

    /** Animated strings (animated-strings 2.6): Medium forces their low style;
        Off disables them. */
    enum class StringsStyle { Full, LowStyle, Off };

    static AnimationPolicy& get();

    //==========================================================================
    MotionLevel getMotion() const noexcept { return motion; }

    /** Low or relief >= 2: live readouts at <= 10 Hz, stepped, no ballistics. */
    bool isReadoutStepped() const noexcept { return readoutStepped; }

    bool mayAnimate (MotionClass c) const noexcept;
    int frameRateHz (MotionClass c, int requestedHz) const noexcept;
    int transitionMs (int requestedMs) const noexcept;
    StringsStyle getStringsStyle() const noexcept;

    //==========================================================================
    // Inputs.

    /** One editor's CPU quality level and relief. The most restrictive of all
        sources wins. Pass the same key to update; removeSource on close. */
    void setSource (const void* key, QualityLevel level, int reliefLevel);
    void removeSource (const void* key);

    /** Relief for the process-wide source (cpu-quality-modes 7's API). */
    void setReliefLevel (int reliefLevel);

    /** Re-reads Reduced motion (normally automatic, via AccessibilitySettings). */
    void refreshReducedMotion();

    //==========================================================================
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void motionPolicyChanged() = 0;
    };

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    //==========================================================================
    /** The RAII member that puts a component in the registry (CQ-22). */
    class Registration : private Listener
    {
    public:
        Registration (juce::Component& owner, MotionClass motionClass, const char* name,
                      std::function<void()> onPolicyChange = {},
                      std::function<void()> onStaticPoll = {});
        ~Registration() override;

        /** Starts `timer` at the rate the policy allows for `requestedHz`
            (0 = stopped), and keeps it retuned as the policy changes. */
        void startTimerHz (juce::Timer& timer, int requestedHz);

        /** Stops the timer and forgets the request. */
        void stopTimer();

        int getRequestedHz() const noexcept { return requestedHz; }
        int getAppliedHz() const noexcept { return appliedHz; }
        MotionClass getMotionClass() const noexcept { return motionClass; }
        bool isStaticPollWanted() const noexcept { return onStaticPoll != nullptr && requestedHz > 0 && appliedHz == 0; }

    private:
        friend class AnimationPolicy;
        void motionPolicyChanged() override;
        void apply();

        juce::Component& owner;
        MotionClass motionClass;
        const char* name;
        std::function<void()> onPolicyChange, onStaticPoll;
        juce::Timer* timer = nullptr;
        int requestedHz = 0, appliedHz = 0;
        std::atomic<int> paints { 0 };

        JUCE_DECLARE_NON_COPYABLE (Registration)
    };

    /** First line of every registered component's paint(). */
    static void notePaint (const juce::Component& c) noexcept;

    //==========================================================================
    // For the tests (CQ-21..24) and the debug window.

    struct RegistryInfo
    {
        juce::String name;
        MotionClass motionClass = Decorative;
        const juce::Component* component = nullptr;
        int paints = 0;
        bool timerRunning = false;
        int appliedHz = 0;
    };

    std::vector<RegistryInfo> getRegistry() const;
    void resetPaintCounts();

    /** The poll-only allow-list: class name -> reason (CQ-22). */
    static const std::vector<std::pair<const char*, const char*>>& getPollOnlyAllowList();

    /** The pure truth table (CQ-21). */
    struct State
    {
        MotionLevel motion = MotionLevel::Full;
        bool readoutStepped = false;
    };

    static State compute (bool reducedMotion, QualityLevel level, int relief) noexcept;
    static int frameRateFor (const State& s, MotionClass c, int requestedHz) noexcept;

private:
    AnimationPolicy();
    ~AnimationPolicy() override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshReducedMotion(); }
    void timerCallback() override;   // the static poll (allow-listed)
    void recompute();
    void updateStaticPoll();

    MotionLevel motion = MotionLevel::Full;
    bool readoutStepped = false;
    bool reducedMotion = false;

    struct Source { QualityLevel level = QualityLevel::High; int relief = 0; };
    std::map<const void*, Source> sources;

    juce::ListenerList<Listener> listeners;
    std::map<const juce::Component*, Registration*> registry;
    bool listeningToAccessibility = false;

    JUCE_DECLARE_NON_COPYABLE (AnimationPolicy)
};

} // namespace luthier
