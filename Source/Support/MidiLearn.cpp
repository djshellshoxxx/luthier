#include "MidiLearn.h"

namespace luthier
{

MidiLearnManager::MidiLearnManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    // SPEC-SWEEP (UW-29): the user's global mappings are there from the start.
    loadGlobalMappings();

    const juce::ScopedLock sl (lock);
    mergeGlobalMappings();
    rebuildLookup();
}

MidiLearnManager::~MidiLearnManager()
{
    cancelPendingUpdate();
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
    auto freshStorage = std::make_unique<std::array<LookupEntry, kNumSources>>();   // 20 kB: off the stack
    auto& fresh = *freshStorage;

    for (const auto& m : mappings)
    {
        if (! juce::isPositiveAndBelow (m.ccNumber, kNumSources))
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
        addMapping (target, cc);

    cancelLearning();
}

void MidiLearnManager::addMapping (const juce::String& parameterId, int ccNumber, int channel)
{
    if (! juce::isPositiveAndBelow (ccNumber, kNumSources) || parameterId.isEmpty())   // SPEC-SWEEP IR-4
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
        // The event the const overload will learn: the first one it does not skip.
        int learnedIndex = -1, index = 0;
        const bool notes = learnNotes.load (std::memory_order_relaxed);

        for (const auto metadata : midi)
        {
            double unused = 0.0;
            const int key = sourceKeyFor (metadata.getMessage(), unused, notes);

            if (key >= 0 && ! (key == 64 || key == 66 || key == 123 || key == 120)
                  && ! (key >= kNoteBase && ! metadata.getMessage().isNoteOn()))
            {
                learnedIndex = index;
                break;
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
    bool isLearningNow = learning.load (std::memory_order_relaxed);
    const bool notes = learnNotes.load (std::memory_order_relaxed);

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        // SPEC-SWEEP (IR-4): CCs, program changes, pressure, poly aftertouch
        // and (when allowed) notes, each under its own source key.
        double value = 0.0;
        const int cc = sourceKeyFor (message, value, notes);

        if (cc < 0)
            continue;

        if (isLearningNow)
        {
            // Pedals and the standard performance controllers are skipped while
            // learning: a player nudging the sustain pedal should not silently
            // steal the mapping they were about to make. A note is learned from
            // its note-on, never its note-off.
            if (cc == 64 || cc == 66 || cc == 123 || cc == 120
                  || (cc >= kNoteBase && ! message.isNoteOn()))
                continue;

            // Mapping mutates the array, so it cannot happen here; the message
            // thread does it (handleAsyncUpdate), and cancelling the updater in
            // the destructor means it never runs on a deleted manager.
            if (juce::isPositiveAndBelow (cc, kNumSources))
            {
                learnedCc.store (cc);
                triggerAsyncUpdate();
            }

            learning.store (false, std::memory_order_relaxed);
            isLearningNow = false;   // the rest of the block maps as usual
            continue;
        }

        if (! juce::isPositiveAndBelow (cc, kNumSources))
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
// SPEC-SWEEP (UW-29)
namespace
{
    juce::File& globalFileOverride()
    {
        static juce::File file;
        return file;
    }
}

juce::File MidiLearnManager::getGlobalMappingsFile()
{
    if (globalFileOverride() != juce::File())
        return globalFileOverride();

    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier").getChildFile ("config").getChildFile ("midi-global.json");
}

void MidiLearnManager::setGlobalMappingsFileForTesting (const juce::File& file)
{
    globalFileOverride() = file;
}

void MidiLearnManager::loadGlobalMappings()
{
    const auto file = getGlobalMappingsFile();
    const juce::ScopedLock sl (lock);
    globalMappings.clear();

    if (! file.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());   // kept: getArray points into it

    if (auto* array = parsed.getArray())
        for (const auto& item : *array)
            if (auto* obj = item.getDynamicObject())
            {
                Mapping m;
                m.parameterId = obj->getProperty ("parameter").toString();
                m.ccNumber = (int) obj->getProperty ("cc");
                m.channel = (int) obj->getProperty ("channel");
                m.rangeMin = obj->hasProperty ("min") ? (double) obj->getProperty ("min") : 0.0;
                m.rangeMax = obj->hasProperty ("max") ? (double) obj->getProperty ("max") : 1.0;
                m.inverted = obj->getProperty ("inverted");
                m.global = true;

                if (m.parameterId.isNotEmpty() && juce::isPositiveAndBelow (m.ccNumber, kNumSources))
                    globalMappings.add (m);
            }
}

void MidiLearnManager::saveGlobalMappings() const
{
    juce::Array<juce::var> array;

    {
        const juce::ScopedLock sl (lock);

        for (const auto& m : globalMappings)
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
    }

    const auto file = getGlobalMappingsFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (array)));
}

void MidiLearnManager::mergeGlobalMappings()
{
    for (const auto& g : globalMappings)
    {
        bool taken = false;

        for (const auto& m : mappings)
            taken = taken || m.parameterId == g.parameterId || m.ccNumber == g.ccNumber;

        if (! taken && apvts.getParameter (g.parameterId) != nullptr)
            mappings.add (g);
    }
}

void MidiLearnManager::setMappingGlobal (const juce::String& parameterId, bool isGlobal)
{
    {
        const juce::ScopedLock sl (lock);

        for (int i = globalMappings.size(); --i >= 0;)
            if (globalMappings.getReference (i).parameterId == parameterId)
                globalMappings.remove (i);

        for (auto& m : mappings)
            if (m.parameterId == parameterId)
            {
                m.global = isGlobal;

                if (isGlobal)
                    globalMappings.add (m);
            }
    }

    saveGlobalMappings();
    sendChangeMessage();
}

bool MidiLearnManager::isMappingGlobal (const juce::String& parameterId) const
{
    const juce::ScopedLock sl (lock);

    for (const auto& m : mappings)
        if (m.parameterId == parameterId)
            return m.global;

    return false;
}

//==============================================================================
// SPEC-SWEEP (IR-4)
int MidiLearnManager::sourceKeyFor (const juce::MidiMessage& m, double& value, bool includeNotes) noexcept
{
    if (m.isController())
    {
        value = (double) m.getControllerValue() / 127.0;
        return m.getControllerNumber();
    }

    if (m.isProgramChange())
    {
        value = 1.0;   // a program change is a press: it sets the top of the range
        return kProgramBase + m.getProgramChangeNumber();
    }

    if (m.isChannelPressure())
    {
        value = (double) m.getChannelPressureValue() / 127.0;
        return kChannelPressure;
    }

    if (m.isAftertouch())
    {
        value = (double) m.getAfterTouchValue() / 127.0;
        return kPolyBase + m.getNoteNumber();
    }

    if (includeNotes && (m.isNoteOn() || m.isNoteOff()))
    {
        value = m.isNoteOn() ? 1.0 : 0.0;
        return kNoteBase + m.getNoteNumber();
    }

    return -1;
}

juce::String MidiLearnManager::describeSource (int key)
{
    if (juce::isPositiveAndBelow (key, 128))
        return "CC " + juce::String (key);

    if (key >= kProgramBase && key < kChannelPressure)
        return "Program " + juce::String (key - kProgramBase);

    if (key == kChannelPressure)
        return "Pressure";

    if (key >= kPolyBase && key < kNoteBase)
        return "Poly AT " + juce::MidiMessage::getMidiNoteName (key - kPolyBase, true, true, 4);

    if (key >= kNoteBase && key < kNumSources)
        return "Note " + juce::MidiMessage::getMidiNoteName (key - kNoteBase, true, true, 4);

    return {};
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

        if (m.global)
            obj->setProperty ("global", true);   // SPEC-SWEEP UW-29

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
                    m.global = obj->getProperty ("global");   // SPEC-SWEEP UW-29

                    if (m.parameterId.isNotEmpty() && juce::isPositiveAndBelow (m.ccNumber, kNumSources))
                        mappings.add (m);
                }
            }
        }

        mergeGlobalMappings();   // SPEC-SWEEP UW-29: under what the loaded state maps
        rebuildLookup();
    }

    sendChangeMessage();
}

} // namespace luthier
