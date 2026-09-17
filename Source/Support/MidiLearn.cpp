#include "MidiLearn.h"

namespace luthier
{

MidiLearnManager::MidiLearnManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    for (auto& slot : ccToMapping)
        slot.store (-1);
}

MidiLearnManager::~MidiLearnManager() = default;

//==============================================================================
void MidiLearnManager::startLearning (const juce::String& parameterId)
{
    {
        const juce::ScopedLock sl (lock);
        learningParameter = parameterId;
    }

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

juce::String MidiLearnManager::getLearningParameterId() const
{
    const juce::ScopedLock sl (lock);
    return learningParameter;
}

//==============================================================================
void MidiLearnManager::rebuildLookup() noexcept
{
    for (auto& slot : ccToMapping)
        slot.store (-1, std::memory_order_relaxed);

    for (int i = 0; i < mappings.size(); ++i)
    {
        const int cc = mappings.getReference (i).ccNumber;

        if (juce::isPositiveAndBelow (cc, 128))
            ccToMapping[(size_t) cc].store (i, std::memory_order_relaxed);
    }
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
    }

    sendChangeMessage();
}

//==============================================================================
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

            const auto target = getLearningParameterId();

            if (target.isNotEmpty())
            {
                // Mapping mutates the array, so it cannot happen here on the audio
                // thread; it is deferred to the message thread.
                juce::MessageManager::callAsync ([this, target, cc]
                {
                    addMapping (target, cc);
                    cancelLearning();
                });
            }

            learning.store (false, std::memory_order_relaxed);
            continue;
        }

        if (! juce::isPositiveAndBelow (cc, 128))
            continue;

        const int index = ccToMapping[(size_t) cc].load (std::memory_order_relaxed);

        if (index < 0)
            continue;

        // Read the mapping without locking: the array is only mutated on the
        // message thread, and a torn read here would at worst apply a stale range
        // for one message.
        const auto& m = mappings.getReference (juce::jlimit (0, juce::jmax (0, mappings.size() - 1), index));

        if (m.channel != 0 && m.channel != message.getChannel())
            continue;

        double scaled = m.rangeMin + (m.rangeMax - m.rangeMin) * value;

        if (m.inverted)
            scaled = m.rangeMax - (scaled - m.rangeMin);

        if (auto* param = apvts.getParameter (m.parameterId))
            param->setValueNotifyingHost ((float) juce::jlimit (0.0, 1.0, scaled));
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
