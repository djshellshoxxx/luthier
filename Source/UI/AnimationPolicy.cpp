#include "AnimationPolicy.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    constexpr int kDecorativeFullHz = 60;
    constexpr int kDecorativeLimitedHz = 30;
    constexpr int kSteppedReadoutHz = 10;
    constexpr int kStaticPollHz = 4;

    int policyAnimationMs (int normalMs) noexcept
    {
        return AnimationPolicy::get().transitionMs (normalMs);
    }
}

//==============================================================================
/*  cpu-quality-modes 6 / CQ-22: timers that only poll state are not animation.
    Each class here keeps its timer at every motion level; the reason says why
    that is not motion. Anything that moves, glows, scrolls or eases on screen
    registers with AnimationPolicy instead. */
const std::vector<std::pair<const char*, const char*>>& AnimationPolicy::getPollOnlyAllowList()
{
    static const std::vector<std::pair<const char*, const char*>> list =
    {
        { "LuthierAudioProcessorEditor", "4 Hz: notices, drawer requests, tooltip setting; the footer badge draws the live values" },
        { "AnimationPolicy",       "its own 4 Hz static poll, which exists so static components can stop their timers" },
        { "StringRow",             "8 Hz: note-name and tension text only when they change" },
        { "CompactRack",           "4 Hz: pedal names and bypass state" },
        { "EasyPanel",             "10 Hz: control sync and the chord-name text; nothing moves" },
        { "LivePanel",             "6 Hz: snapshot bank labels and button enables" },
        { "MidiOutPanel",          "4 Hz: routing config re-sync and capture text" },
        { "NoiseGroups",           "4 Hz: squeak-style combo text" },
        { "NotificationCentre",    "one-shot auto-dismiss timeout" },
        { "InlineNotice",          "one-shot auto-dismiss timeout" },
        { "PedalSlotComponent",    "10 Hz: rebuilds controls on a slot-type change; bypass LED state" },
        { "PracticeSetupPanel",    "2 Hz: stats, progress and runner status text" },
        { "RangeTabButton",        "4 Hz: padlock state" },
        { "SetupGroup",            "4 Hz: setup-style combo text" },
        { "SlapGroup",             "4 Hz: shows or hides with the guitar family" },
        { "SlideGroup",            "4 Hz: shows or hides with slide mode" },
        { "StrumGroup",            "5 Hz: control sync" },
        { "StringMaskSelector",    "10 Hz: E-Bow string-mask parameter sync" },
        { "WorkshopPanel",         "20 Hz: guitar-key change, spectrum worker result, audition end" },
        { "ChordAndTabPanel",      "12 Hz: records tab columns from live notes (a capture, not a display)" },
        { "Registration",          "AnimationPolicy's own API: it starts the timers it was asked to" },
        { "QualityEditorLink",     "4 Hz: feeds this editor's level into the policy and drains notices" },
        { "DecayRow",              "4 Hz: sustain-style combo text" },
        { "DecaySketch",           "4 Hz: redraws only on a parameter change" },
        { "NoiseFloorGroup",       "4 Hz: noise-floor style combo text" },
        { "PositionPad",           "10 Hz: redraws only when the player's position parameters change" },
        { "RightHandToolSelector", "4 Hz: syncs the selected tool with its parameter" },
        { "SustainShapeGroup",     "4 Hz: sustain-style combo text" },
        { "TuningStabilityGroup",  "2 Hz: capo-bias text and string count" }
    };

    return list;
}

//==============================================================================
AnimationPolicy& AnimationPolicy::get()
{
    static AnimationPolicy instance;
    return instance;
}

AnimationPolicy::AnimationPolicy()
{
    reducedMotion = AccessibilitySettings::get().isReducedMotion();
    AccessibilitySettings::get().addChangeListener (this);
    listeningToAccessibility = true;

    // cpu-quality-modes 4: getAnimationMs delegates to transitionMs.
    AccessibilitySettings::animationMsHook.store (&policyAnimationMs);

    recompute();
}

AnimationPolicy::~AnimationPolicy()
{
    AccessibilitySettings::animationMsHook.store (nullptr);

    if (listeningToAccessibility)
        AccessibilitySettings::get().removeChangeListener (this);
}

//==============================================================================
AnimationPolicy::State AnimationPolicy::compute (bool reduced, QualityLevel level, int relief) noexcept
{
    State s;

    if (level == QualityLevel::Low || relief >= 2)
    {
        s.motion = MotionLevel::Off;
        s.readoutStepped = true;
    }
    else if (reduced)
    {
        s.motion = MotionLevel::Off;
    }
    else if (level == QualityLevel::Medium || relief == 1)
    {
        s.motion = MotionLevel::Limited;
    }

    return s;
}

int AnimationPolicy::frameRateFor (const State& s, MotionClass c, int requestedHz) noexcept
{
    requestedHz = juce::jmax (0, requestedHz);

    switch (c)
    {
        case Decorative:
            return s.motion == MotionLevel::Off     ? 0
                 : s.motion == MotionLevel::Limited ? juce::jmin (requestedHz, kDecorativeLimitedHz)
                                                    : juce::jmin (requestedHz, kDecorativeFullHz);

        case Transition:
            return s.motion == MotionLevel::Off ? 0 : requestedHz;

        case LiveReadout:
        default:
            return s.readoutStepped ? juce::jmin (requestedHz, kSteppedReadoutHz) : requestedHz;
    }
}

AnimationPolicy::State AnimationPolicy::live() const noexcept
{
    return compute (AccessibilitySettings::get().isReducedMotion(), combinedLevel, combinedRelief);
}

bool AnimationPolicy::mayAnimate (MotionClass c) const noexcept
{
    return c == LiveReadout || live().motion != MotionLevel::Off;
}

int AnimationPolicy::frameRateHz (MotionClass c, int requestedHz) const noexcept
{
    return frameRateFor (live(), c, requestedHz);
}

int AnimationPolicy::transitionMs (int requestedMs) const noexcept
{
    return live().motion == MotionLevel::Off ? 0 : juce::jmax (0, requestedMs);
}

AnimationPolicy::StringsStyle AnimationPolicy::getStringsStyle() const noexcept
{
    const auto m = live().motion;
    return m == MotionLevel::Off     ? StringsStyle::Off
         : m == MotionLevel::Limited ? StringsStyle::LowStyle
                                     : StringsStyle::Full;
}

//==============================================================================
void AnimationPolicy::setSource (const void* key, QualityLevel level, int reliefLevel)
{
    auto& s = sources[key];
    s.level = level;
    s.relief = juce::jlimit (0, 2, reliefLevel);
    recompute();
}

void AnimationPolicy::removeSource (const void* key)
{
    sources.erase (key);
    recompute();
}

void AnimationPolicy::setReliefLevel (int reliefLevel)
{
    auto& s = sources[this];
    s.relief = juce::jlimit (0, 2, reliefLevel);
    recompute();
}

void AnimationPolicy::refreshReducedMotion()
{
    reducedMotion = AccessibilitySettings::get().isReducedMotion();
    recompute();
}

void AnimationPolicy::recompute()
{
    auto level = QualityLevel::High;
    int relief = 0;

    for (const auto& [key, s] : sources)
    {
        juce::ignoreUnused (key);
        level = (QualityLevel) juce::jmax ((int) level, (int) s.level);
        relief = juce::jmax (relief, s.relief);
    }

    combinedLevel = level;
    combinedRelief = relief;
    const auto st = compute (reducedMotion, level, relief);

    if (st.motion == motion && st.readoutStepped == readoutStepped)
        return;

    motion = st.motion;
    readoutStepped = st.readoutStepped;

    listeners.call ([] (Listener& l) { l.motionPolicyChanged(); });
    updateStaticPoll();
}

void AnimationPolicy::updateStaticPoll()
{
    bool wanted = false;

    for (const auto& [c, r] : registry)
    {
        juce::ignoreUnused (c);
        wanted = wanted || r->isStaticPollWanted();
    }

    if (wanted && ! isTimerRunning())
        startTimerHz (kStaticPollHz);
    else if (! wanted && isTimerRunning())
        stopTimer();
}

void AnimationPolicy::timerCallback()
{
    // Copy first: a callback may add or remove registrations.
    std::vector<Registration*> due;

    for (const auto& [c, r] : registry)
    {
        juce::ignoreUnused (c);

        if (r->isStaticPollWanted())
            due.push_back (r);
    }

    for (auto* r : due)
        if (isRegistered (r) && r->onStaticPoll)
            r->onStaticPoll();

    updateStaticPoll();
}

bool AnimationPolicy::isRegistered (const Registration* r) const
{
    const auto range = registry.equal_range (&r->owner);

    for (auto it = range.first; it != range.second; ++it)
        if (it->second == r)
            return true;

    return false;
}

//==============================================================================
void AnimationPolicy::notePaint (const juce::Component& c) noexcept
{
    const auto range = get().registry.equal_range (&c);

    for (auto it = range.first; it != range.second; ++it)
        it->second->paints.fetch_add (1, std::memory_order_relaxed);
}

std::vector<AnimationPolicy::RegistryInfo> AnimationPolicy::getRegistry() const
{
    std::vector<RegistryInfo> out;

    for (const auto& [c, r] : registry)
    {
        RegistryInfo info;
        info.name = r->name;
        info.motionClass = r->motionClass;
        info.component = c;
        info.paints = r->paints.load (std::memory_order_relaxed);
        info.timerRunning = r->timer != nullptr && r->timer->isTimerRunning();
        info.appliedHz = r->appliedHz;
        out.push_back (info);
    }

    return out;
}

void AnimationPolicy::resetPaintCounts()
{
    for (auto& [c, r] : registry)
    {
        juce::ignoreUnused (c);
        r->paints.store (0, std::memory_order_relaxed);
    }
}

//==============================================================================
AnimationPolicy::Registration::Registration (juce::Component& o, MotionClass cls, const char* n,
                                             std::function<void()> changeFn,
                                             std::function<void()> staticPollFn)
    : owner (o), motionClass (cls), name (n),
      onPolicyChange (std::move (changeFn)), onStaticPoll (std::move (staticPollFn))
{
    auto& policy = AnimationPolicy::get();
    policy.registry.emplace (&owner, this);
    policy.addListener (this);
    apply();
}

AnimationPolicy::Registration::~Registration()
{
    auto& policy = AnimationPolicy::get();
    policy.removeListener (this);

    const auto range = policy.registry.equal_range (&owner);

    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second == this)
        {
            policy.registry.erase (it);
            break;
        }
    }

    policy.updateStaticPoll();
}

void AnimationPolicy::Registration::startTimerHz (juce::Timer& t, int hz)
{
    timer = &t;
    requestedHz = juce::jmax (0, hz);
    apply();
}

void AnimationPolicy::Registration::stopTimer()
{
    requestedHz = 0;
    apply();
}

void AnimationPolicy::Registration::apply()
{
    const int hz = requestedHz > 0 ? AnimationPolicy::get().frameRateHz (motionClass, requestedHz) : 0;
    appliedHz = hz;

    /*  At Off a Decorative or Transition component is buffered to an image, so
        a neighbour's repaint (a meter under an overlay, say) reuses the cache
        instead of calling its paint(): it paints only when it changes itself. */
    const bool buffer = motionClass != LiveReadout && AnimationPolicy::get().getMotion() == MotionLevel::Off;

    if (buffer != buffered)
    {
        buffered = buffer;
        owner.setBufferedToImage (buffer);
    }

    if (timer != nullptr)
    {
        if (hz > 0)
        {
            if (! timer->isTimerRunning() || timer->getTimerInterval() != juce::jmax (1, 1000 / hz))
                timer->startTimerHz (hz);
        }
        else
        {
            timer->stopTimer();
        }
    }

    AnimationPolicy::get().updateStaticPoll();
}

void AnimationPolicy::Registration::motionPolicyChanged()
{
    apply();

    if (onPolicyChange)
        onPolicyChange();
}

} // namespace luthier
