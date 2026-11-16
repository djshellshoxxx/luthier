#include "TestFramework.h"
#include "../Support/DeferredEngineMidi.h"
#include <array>

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (DeferredEngineMidi, missedReleasePrecedesCurrentInputAndIsConsumedOnce)
{
    DeferredEngineMidi carry;
    carry.prepare();
    juce::MidiBuffer missed, current;
    missed.addEvent (juce::MidiMessage::noteOff (1, 72), 23);
    carry.defer (missed);
    current.addEvent (juce::MidiMessage::noteOn (1, 74, (juce::uint8) 100), 0);
    current.addEvent (juce::MidiMessage::noteOff (1, 74), 21);
    auto& result = carry.take (current);
    CHECK (result.getNumEvents() == 3);
    auto it = result.begin();
    CHECK ((*it).samplePosition == 0 && (*it).getMessage().isNoteOff() && (*it).getMessage().getNoteNumber() == 72);
    ++it;
    CHECK ((*it).samplePosition == 0 && (*it).getMessage().isNoteOn());
    ++it;
    CHECK ((*it).samplePosition == 21 && (*it).getMessage().isNoteOff());
    CHECK (current.getNumEvents() == 2); // Echo/capture consumers retain their original input.
    current.clear();
    CHECK (&carry.take (current) == &current);
}

LUTHIER_TEST (DeferredEngineMidi, repeatedMissesKeepOrderAndResetDropsHistory)
{
    DeferredEngineMidi carry;
    carry.prepare();
    juce::MidiBuffer input;
    input.addEvent (juce::MidiMessage::noteOn (2, 48, (juce::uint8) 100), 17);
    carry.defer (input);
    input.clear();
    input.addEvent (juce::MidiMessage::noteOff (2, 48), 9);
    carry.defer (input);
    input.clear();
    auto& output = carry.take (input);
    CHECK (output.getNumEvents() == 2);
    auto it = output.begin();
    CHECK ((*it).samplePosition == 0 && (*it).getMessage().isNoteOn());
    ++it;
    CHECK ((*it).samplePosition == 0 && (*it).getMessage().isNoteOff());
    input.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    carry.defer (input);
    carry.reset();
    input.clear();
    CHECK (carry.take (input).isEmpty());
}

LUTHIER_TEST (DeferredEngineMidi, overflowReleasesEveryChannelAndPedal)
{
    for (bool overflowWhenTaking : {false, true})
    {
        DeferredEngineMidi carry;
        carry.prepare();
        juce::MidiBuffer large, empty;
        for (int i = 0; i < 9000; ++i)
            large.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        if (overflowWhenTaking)
        {
            empty.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            carry.defer (empty);
            empty.clear();
        }
        else carry.defer (large);
        auto& output = carry.take (overflowWhenTaking ? large : empty);
        CHECK (output.getNumEvents() == 80);
        std::array<int, 16> masks {};
        bool allAtStart = true;
        for (const auto event : output)
        {
            const auto message = event.getMessage();
            allAtStart &= event.samplePosition == 0 && message.isController() && message.getControllerValue() == 0;
            const int cc = message.getControllerNumber();
            const int bit = cc == 64 ? 1 : cc == 66 ? 2 : cc == 67 ? 4 : cc == 123 ? 8 : cc == 120 ? 16 : 0;
            masks[(size_t) message.getChannel() - 1] |= bit;
        }
        CHECK (allAtStart);
        CHECK (std::all_of (masks.begin(), masks.end(), [] (int mask) { return mask == 31; }));
    }
}
