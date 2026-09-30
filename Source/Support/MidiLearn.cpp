#include "MidiLearn.h"

namespace luthier
{

MidiLearnManager::MidiLearnManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
}

MidiLearnManager::~MidiLearnManager()
{
    stopTimer();
    cancelPendingUpdate();
}

void MidiLearnManager::timerCallback()
{
    // The CC the audio thread caught is mapped here; the timer runs only while
    // learning (or until the caught CC is mapped).
    handleAsyncUpdate();

    if (! learning.load() && learnedCc.load() < 0)
        stopTimer();
}

//==============================================================================
void MidiLearnManager::startLearning (const juce::String& parameterId)
{
    {
        const juce::ScopedLock sl (lock);
        learningParameter = parameterId;
    }

    waitingSinceMs = juce::Time::getMillisecondCounter();   // SPEC-SWEEP: ER-38
    learning.store (true);

    // performance-budget.md 0.5: posting a message from the audio thread takes
    // the message queue's lock, so the message thread polls instead.
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        startTimerHz (30);

    sendChangeMessage();
}

void MidiLearnManager::cancelLearning()
{
    learning.store (false);

    {
        const juce::ScopedLock sl (lock);
        learningParameter.clear();
    }

    sendChangeMessage();
}

void MidiLearnManager::setArmed (bool shouldBeArmed)
{
    const bool wasArmed = armed.exchange (shouldBeArmed);

    if (shouldBeArmed && ! wasArmed)
        waitingSinceMs = juce::Time::getMillisecondCounter();   // SPEC-SWEEP: ER-38

    /*  Disarming cancels a learn in flight, and does so even when the arm flag was
        already clear.

        That case is the normal one rather than an edge: claimArmedLearn spends the
        arm and starts the learn, so by the time the user presses Escape or toggles
        the header button off, `armed` is false and `learning` is true. Keying this
        off the flag changing would leave the plugin waiting for a CC with nothing
        on screen saying so, and the next stray knob on the user's controller would
        map itself to whatever they last clicked.

        The editor therefore must not call setArmed(false) merely to take its arm
        overlay down after a successful claim; it hides the overlay directly. */
    if (! shouldBeArmed && learning.load())
        cancelLearning();

    if (wasArmed != shouldBeArmed)
        sendChangeMessage();
}

bool MidiLearnManager::claimArmedLearn (const juce::String& parameterId)
{
    if (! armed.load() || parameterId.isEmpty())
        return false;

    armed.store (false);
    startLearning (parameterId);

    // startLearning already broadcasts, so the header sees both changes at once.
    return true;
}

bool MidiLearnManager::expireIfIdle (juce::uint32 nowMs)
{
    // SPEC-SWEEP: ER-38. A learn that caught its CC has already disarmed.
    if (! armed.load() && ! learning.load())
        return false;

    if (nowMs - waitingSinceMs < kArmTimeoutMs)
        return false;

    setArmed (false);   // cancels a learn in flight as well
    return true;
}

juce::String MidiLearnManager::getLearningParameterId() const
{
    const juce::ScopedLock sl (lock);
    return learningParameter;
}

//==============================================================================
void MidiLearnManager::rebuildLookup() noexcept
{
    // Message thread, under `lock`. The table is built aside and copied in
    // under the spin lock, so the audio thread waits at most for the copy.
    std::array<LookupEntry, 128> fresh {};

    for (const auto& m : mappings)
    {
        if (! juce::isPositiveAndBelow (m.ccNumber, 128))
            continue;

        auto& e = fresh[(size_t) m.ccNumber];
        e.parameter = apvts.getParameter (m.parameterId);
        e.channel = m.channel;
        e.rangeMin = m.rangeMin;
        e.rangeMax = m.rangeMax;
        e.inverted = m.inverted;
    }

    const juce::SpinLock::ScopedLockType sl (tableLock);
    lookup = fresh;
}

void MidiLearnManager::handleAsyncUpdate()
{
    const int cc = learnedCc.exchange (-1);

    if (cc < 0)
        return;

    const auto target = getLearningParameterId();

    if (target.isNotEmpty())
    {
        if (onBeforeLearn != nullptr)   // action-and-undo.md 3.12: the learn is one undo entry
            onBeforeLearn (target, cc);

        addMapping (target, cc);
    }

    cancelLearning();
}

void MidiLearnManager::addMapping (const juce::String& parameterId, int ccNumber, int channel)
{
    if (! juce::isPositiveAndBelow (ccNumber, 128) || parameterId.isEmpty())
        return;

    {
        const juce::ScopedLock sl (lock);

        // One CC controls one parameter, and one parameter is controlled by one
        // CC. Both directions are replaced so the map can never be ambiguous.
        for (int i = mappings.size(); --i >= 0;)
            if (mappings[i].parameterId == parameterId || mappings[i].ccNumber == ccNumber)
                mappings.remove (i);

        Mapping m;
        m.parameterId = parameterId;
        m.ccNumber = ccNumber;
        m.channel = channel;
        mappings.add (m);

        rebuildLookup();
    }

    sendChangeMessage();
}

void MidiLearnManager::removeMappingForParameter (const juce::String& parameterId)
{
    {
        const juce::ScopedLock sl (lock);

        for (int i = mappings.size(); --i >= 0;)
            if (mappings[i].parameterId == parameterId)
                mappings.remove (i);

        rebuildLookup();
    }

    sendChangeMessage();
}

void MidiLearnManager::removeMappingForCc (int ccNumber)
{
    {
        const juce::ScopedLock sl (lock);

        for (int i = mappings.size(); --i >= 0;)
            if (mappings[i].ccNumber == ccNumber)
                mappings.remove (i);

        rebuildLookup();
    }

    sendChangeMessage();
}

void MidiLearnManager::clearAllMappings()
{
    {
        const juce::ScopedLock sl (lock);
        mappings.clear();
        rebuildLookup();
    }

    sendChangeMessage();
}

//==============================================================================
int MidiLearnManager::getCcForParameter (const juce::String& parameterId) const
{
    const juce::ScopedLock sl (lock);

    for (const auto& m : mappings)
        if (m.parameterId == parameterId)
            return m.ccNumber;

    return -1;
}

juce::String MidiLearnManager::getParameterForCc (int ccNumber) const
{
    const juce::ScopedLock sl (lock);

    for (const auto& m : mappings)
        if (m.ccNumber == ccNumber)
            return m.parameterId;

    return {};
}

int MidiLearnManager::getNumMappings() const
{
    const juce::ScopedLock sl (lock);
    return mappings.size();
}

MidiLearnManager::Mapping MidiLearnManager::getMapping (int index) const
{
    const juce::ScopedLock sl (lock);

    if (juce::isPositiveAndBelow (index, mappings.size()))
        return mappings.getReference (index);

    return {};
}

void MidiLearnManager::setMappingRange (const juce::String& parameterId, double min, double max, bool inverted)
{
    {
        const juce::ScopedLock sl (lock);

        for (auto& m : mappings)
        {
            if (m.parameterId == parameterId)
            {
                m.rangeMin = juce::jlimit (0.0, 1.0, min);
                m.rangeMax = juce::jlimit (0.0, 1.0, max);
                m.inverted = inverted;
            }
        }

        rebuildLookup();
    }

    sendChangeMessage();
}

//==============================================================================
void MidiLearnManager::processMidi (juce::MidiBuffer& midi, juce::MidiBuffer& scratch) noexcept
{
    if (learning.load (std::memory_order_relaxed))
    {
        // The event the const overload will learn: the first CC it does not skip.
        int learnedIndex = -1, index = 0;

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (message.isController())
            {
                const int cc = message.getControllerNumber();

                if (! (cc == 64 || cc == 66 || cc == 123 || cc == 120))
                {
                    learnedIndex = index;
                    break;
                }
            }

            ++index;
        }

        if (learnedIndex >= 0)
        {
            processMidi (static_cast<const juce::MidiBuffer&> (midi));   // learns it, maps the rest

            scratch.clear();
            index = 0;

            for (const auto metadata : midi)
                if (index++ != learnedIndex)
                    scratch.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);

            midi.clear();

            for (const auto metadata : scratch)
                midi.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);

            scratch.clear();
            return;
        }
    }

    processMidi (static_cast<const juce::MidiBuffer&> (midi));
}

void MidiLearnManager::processMidi (const juce::MidiBuffer& midi) noexcept
{
    const bool isLearningNow = learning.load (std::memory_order_relaxed);

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (! message.isController())
            continue;

        const int cc = message.getControllerNumber();
        const double value = (double) message.getControllerValue() / 127.0;

        if (isLearningNow)
        {
            // Pedals and the standard performance controllers are skipped while
            // learning: a player nudging the sustain pedal should not silently
            // steal the mapping they were about to make.
            if (cc == 64 || cc == 66 || cc == 123 || cc == 120)
                continue;

            // Mapping mutates the array, so it cannot happen here; the message
            // thread's poll (timerCallback) does it. No triggerAsyncUpdate: posting
            // a message takes a lock (performance-budget.md 0.5).
            if (juce::isPositiveAndBelow (cc, 128))
                learnedCc.store (cc);

            learning.store (false, std::memory_order_relaxed);
            continue;
        }

        if (! juce::isPositiveAndBelow (cc, 128))
            continue;

        LookupEntry m;

        {
            const juce::SpinLock::ScopedTryLockType sl (tableLock);

            if (! sl.isLocked())
                continue;   // mid-rebuild: this one message is dropped

            m = lookup[(size_t) cc];
        }

        if (m.parameter == nullptr)
            continue;

        if (m.channel != 0 && m.channel != message.getChannel())
            continue;

        double scaled = m.rangeMin + (m.rangeMax - m.rangeMin) * value;

        if (m.inverted)
            scaled = m.rangeMax - (scaled - m.rangeMin);

        m.parameter->setValueNotifyingHost ((float) juce::jlimit (0.0, 1.0, scaled));
    }
}

//==============================================================================
juce::var MidiLearnManager::toVar() const
{
    const juce::ScopedLock sl (lock);

    juce::Array<juce::var> array;

    for (const auto& m : mappings)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("parameter", m.parameterId);
        obj->setProperty ("cc", m.ccNumber);
        obj->setProperty ("channel", m.channel);
        obj->setProperty ("min", m.rangeMin);
        obj->setProperty ("max", m.rangeMax);
        obj->setProperty ("inverted", m.inverted);
        array.add (juce::var (obj));
    }

    return array;
}

void MidiLearnManager::fromVar (const juce::var& data)
{
    {
        const juce::ScopedLock sl (lock);
        mappings.clear();

        if (auto* array = data.getArray())
        {
            for (const auto& item : *array)
            {
                if (auto* obj = item.getDynamicObject())
                {
                    Mapping m;
                    m.parameterId = obj->getProperty ("parameter").toString();

                    // SPEC-SWEEP: FF-26 - file-formats.md 2 spells it `param`.
                    if (m.parameterId.isEmpty())
                        m.parameterId = obj->getProperty ("param").toString();
                    m.ccNumber = (int) obj->getProperty ("cc");
                    m.channel = (int) obj->getProperty ("channel");
                    m.rangeMin = obj->hasProperty ("min") ? (double) obj->getProperty ("min") : 0.0;
                    m.rangeMax = obj->hasProperty ("max") ? (double) obj->getProperty ("max") : 1.0;
                    m.inverted = obj->getProperty ("inverted");

                    if (m.parameterId.isNotEmpty() && juce::isPositiveAndBelow (m.ccNumber, 128))
                        mappings.add (m);
                }
            }
        }

        rebuildLookup();
    }

    sendChangeMessage();
}

} // namespace luthier
