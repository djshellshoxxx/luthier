#include "LiveInput.h"

#include "LiveControls.h"
#include "../Support/ConfigRecovery.h"

namespace luthier
{

//==============================================================================
const char* getLiveActionId (LiveAction action) noexcept
{
    switch (action)
    {
        case LiveAction::snapshotNext:     return "live.snapshotNext";
        case LiveAction::snapshotPrevious: return "live.snapshotPrev";
        case LiveAction::snapshotByValue:  return "live.snapshotByValue";
        case LiveAction::tapTempo:         return "live.tap";
        case LiveAction::killSwitch:       return "live.kill";
        case LiveAction::panic:            return "live.panic";
        case LiveAction::setlistNext:      return "live.setlistNext";
        case LiveAction::setlistPrevious:  return "live.setlistPrev";
        case LiveAction::numActions:
        default:                           return "";
    }
}

const char* getLiveActionName (LiveAction action) noexcept
{
    switch (action)
    {
        case LiveAction::snapshotNext:     return "Next snapshot";
        case LiveAction::snapshotPrevious: return "Previous snapshot";
        case LiveAction::snapshotByValue:  return "Snapshot by CC value";
        case LiveAction::tapTempo:         return "Tap tempo";
        case LiveAction::killSwitch:       return "Kill switch (hold)";
        case LiveAction::panic:            return "Panic";
        case LiveAction::setlistNext:      return "Next setlist entry";
        case LiveAction::setlistPrevious:  return "Previous setlist entry";
        case LiveAction::numActions:
        default:                           return "";
    }
}

//==============================================================================
LiveActionMap::LiveActionMap()
    : configFile (getDefaultConfigFile())
{
    for (auto& a : actionForCc)
        a.store (-1);

    for (auto& p : pendingCount)
        p.store (0);
}

juce::File LiveActionMap::getDefaultConfigFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("config")
             .getChildFile ("live-actions.json");
}

void LiveActionMap::assign (LiveAction action, int cc)
{
    if (! juce::isPositiveAndBelow ((int) action, kNumActions) || ! juce::isPositiveAndBelow (cc, 128))
        return;

    // One CC per action and one action per CC, as MIDI Learn does for
    // parameters: the map can never be ambiguous.
    for (auto& a : actionForCc)
        if (a.load() == (int) action)
            a.store (-1);

    actionForCc[(size_t) cc].store ((int) action);
    ++version;

    if (handlers.onCcAssigned != nullptr)
        handlers.onCcAssigned (cc);
}

void LiveActionMap::clear (LiveAction action)
{
    for (auto& a : actionForCc)
        if (a.load() == (int) action)
            a.store (-1);

    ++version;
}

void LiveActionMap::clearAll()
{
    for (auto& a : actionForCc)
        a.store (-1);

    ++version;
}

int LiveActionMap::getCcFor (LiveAction action) const noexcept
{
    for (int cc = 0; cc < 128; ++cc)
        if (actionForCc[(size_t) cc].load (std::memory_order_relaxed) == (int) action)
            return cc;

    return -1;
}

int LiveActionMap::getActionForCc (int cc) const noexcept
{
    return juce::isPositiveAndBelow (cc, 128) ? actionForCc[(size_t) cc].load (std::memory_order_relaxed) : -1;
}

void LiveActionMap::beginLearning (LiveAction action) noexcept
{
    pendingLearn.store (-1);
    learningAction.store ((int) action);
    ++version;
}

void LiveActionMap::cancelLearning() noexcept
{
    learningAction.store (-1);
    ++version;
}

//==============================================================================
bool LiveActionMap::wants (int cc) const noexcept
{
    if (! juce::isPositiveAndBelow (cc, 128))
        return false;

    return actionForCc[(size_t) cc].load (std::memory_order_relaxed) >= 0
        || learningAction.load (std::memory_order_relaxed) >= 0;
}

bool LiveActionMap::handleController (int cc, int value) noexcept
{
    if (! juce::isPositiveAndBelow (cc, 128))
        return false;

    // Learning: bank select and the channel-mode messages are never learnable,
    // the first because it already picks the preset (live-performance 2).
    if (learningAction.load (std::memory_order_relaxed) >= 0
        && cc != 0 && cc != 32 && cc < 120)
    {
        const int action = learningAction.exchange (-1);

        if (action >= 0)
        {
            pendingLearn.store (action * 128 + cc);
            return true;
        }
    }

    const int action = actionForCc[(size_t) cc].load (std::memory_order_relaxed);

    if (action < 0)
        return false;

    const bool pressed = value >= 64;

    switch ((LiveAction) action)
    {
        case LiveAction::killSwitch:
            // live-performance 6: momentary, held while the footswitch is down.
            if (killSwitch != nullptr)
                killSwitch->setActive (pressed);
            break;

        case LiveAction::snapshotByValue:
            pendingSnapshotIndex.store (value, std::memory_order_relaxed);
            break;

        case LiveAction::tapTempo:
            // The tap is stamped here, so the timer's latency does not jitter
            // the tempo it works out.
            if (pressed)
                pendingTapSeconds.store (juce::Time::getMillisecondCounterHiRes() * 0.001,
                                         std::memory_order_relaxed);
            break;

        case LiveAction::snapshotNext:
        case LiveAction::snapshotPrevious:
        case LiveAction::panic:
        case LiveAction::setlistNext:
        case LiveAction::setlistPrevious:
            // A footswitch sends 127 on press and 0 on release: one action.
            if (pressed)
                pendingCount[(size_t) action].fetch_add (1, std::memory_order_relaxed);
            break;

        case LiveAction::numActions:
        default:
            break;
    }

    return true;
}

//==============================================================================
void LiveActionMap::service()
{
    if (const int learned = pendingLearn.exchange (-1); learned >= 0)
    {
        assign ((LiveAction) (learned / 128), learned % 128);
        save();
    }

    auto run = [this] (LiveAction action, const std::function<void()>& fn)
    {
        int n = pendingCount[(size_t) action].exchange (0);

        // A burst is capped: nobody steps eight snapshots in a thirtieth of a
        // second on purpose.
        for (n = juce::jmin (n, 8); --n >= 0;)
            if (fn != nullptr)
                fn();
    };

    run (LiveAction::panic,            handlers.panic);
    run (LiveAction::snapshotNext,     handlers.nextSnapshot);
    run (LiveAction::snapshotPrevious, handlers.previousSnapshot);
    run (LiveAction::setlistNext,      handlers.setlistNext);
    run (LiveAction::setlistPrevious,  handlers.setlistPrevious);

    if (const int index = pendingSnapshotIndex.exchange (-1); index >= 0 && handlers.recallSnapshot != nullptr)
        handlers.recallSnapshot (index);

    if (const double t = pendingTapSeconds.exchange (-1.0); t >= 0.0 && handlers.tapAt != nullptr)
        handlers.tapAt (t);
}

//==============================================================================
juce::var LiveActionMap::toVar() const
{
    auto* root = new juce::DynamicObject();

    for (int i = 0; i < kNumActions; ++i)
    {
        const int cc = getCcFor ((LiveAction) i);

        if (cc >= 0)
            root->setProperty (getLiveActionId ((LiveAction) i), cc);
    }

    return juce::var (root);
}

void LiveActionMap::fromVar (const juce::var& state)
{
    for (auto& a : actionForCc)
        a.store (-1);

    if (auto* object = state.getDynamicObject())
        for (int i = 0; i < kNumActions; ++i)
        {
            const juce::Identifier id (getLiveActionId ((LiveAction) i));

            if (object->hasProperty (id))
            {
                const int cc = (int) object->getProperty (id);

                if (juce::isPositiveAndBelow (cc, 128))
                    actionForCc[(size_t) cc].store (i);
            }
        }

    ++version;
}

bool LiveActionMap::load()
{
    // SPEC-SWEEP ER-65: an unreadable file is kept aside and reported.
    const auto parsed = ConfigRecovery::loadObject (configFile, "LivePerformance");

    if (parsed.getDynamicObject() == nullptr)
        return false;

    fromVar (parsed);
    return true;
}

bool LiveActionMap::save() const
{
    if (configFile == juce::File())
        return false;

    configFile.getParentDirectory().createDirectory();
    return configFile.replaceWithText (juce::JSON::toString (toVar(), false));
}

} // namespace luthier
