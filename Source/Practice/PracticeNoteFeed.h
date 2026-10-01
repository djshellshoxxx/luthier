#pragma once

/*  The notes the player plays, handed from the audio thread to the practice
    drawer's trainers (SPEC-SWEEP PT-34; practice-tools 4 and input-routing 1,
    which lists practice as a consumer of the input).

    The audio thread pushes the note number of every incoming note-on while
    the drawer is open; the drawer's timer pops them on the message thread and
    gives them to whichever tab is showing. Single producer, single consumer,
    fixed size: a full feed drops the newest notes rather than blocking.
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace luthier
{

class PracticeNoteFeed
{
public:
    static constexpr int kCapacity = 64;

    /** Audio thread: every note-on in the host's MIDI, before anything else is merged in. */
    void pushNoteOns (const juce::MidiBuffer& midi) noexcept
    {
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOn())
                push (message.getNoteNumber());
        }
    }

    void push (int midiNote) noexcept
    {
        const auto scope = fifo.write (1);

        if (scope.blockSize1 > 0)
            notes[(size_t) scope.startIndex1] = midiNote;
        else if (scope.blockSize2 > 0)
            notes[(size_t) scope.startIndex2] = midiNote;
    }

    /** Message thread: the next played note, or false when there is none. */
    bool pop (int& midiNote) noexcept
    {
        const auto scope = fifo.read (1);

        if (scope.blockSize1 > 0)
        {
            midiNote = notes[(size_t) scope.startIndex1];
            return true;
        }

        if (scope.blockSize2 > 0)
        {
            midiNote = notes[(size_t) scope.startIndex2];
            return true;
        }

        return false;
    }

    int getNumReady() const noexcept { return fifo.getNumReady(); }

private:
    juce::AbstractFifo fifo { kCapacity };
    std::array<int, kCapacity> notes {};
};

} // namespace luthier
