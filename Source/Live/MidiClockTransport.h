#pragma once

/*
    SPEC-SWEEP (input-routing IR-16, §1.6): the transport half of incoming MIDI
    clock. MidiClockTempo (host-integration HI-32) already turns the clock into
    a tempo; this follows where the clock is:

      0xFA  start                               -> position 0, running
      0xFB  continue                            -> running from where it stopped
      0xFC  stop                                -> not running
      0xF2  song position pointer (sixteenths)  -> position
      0xF8  timing clock (24 per beat)          -> position advances 1/24 beat

    While the host transport is stopped and the clock is running, the processor
    drives the rhythm engine's grid from here, so a drum machine or sequencer
    can start, stop and relocate the strumming. Audio thread only; no allocation.
*/

#include <juce_audio_basics/juce_audio_basics.h>

namespace luthier
{

class MidiClockTransport
{
public:
    static constexpr int kClocksPerBeat = 24;
    static constexpr double kStaleSeconds = 0.5;   ///< no clock for this long: not following

    void prepare (double sampleRate) noexcept { sr = juce::jmax (1.0, sampleRate); reset(); }
    void reset() noexcept { running = false; ppqAtLastEvent = 0.0; lastClockSample = -1; lastEventSample = -1; }

    /** Reads this block's real-time messages; @p blockStart is the absolute sample
        position of the block's first sample. */
    void process (const juce::MidiBuffer& midi, int numSamples, juce::int64 blockStart) noexcept
    {
        for (const auto metadata : midi)
        {
            if (metadata.numBytes < 1)
                continue;

            const juce::int64 when = blockStart + juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                                                metadata.samplePosition);

            switch (metadata.data[0])
            {
                case 0xf8:
                    if (running && lastClockSample >= 0)
                        ppqAtLastEvent += 1.0 / kClocksPerBeat;

                    lastClockSample = when;
                    lastEventSample = when;
                    break;

                case 0xfa:  running = true;  ppqAtLastEvent = 0.0; lastClockSample = -1; lastEventSample = when; break;
                case 0xfb:  running = true;  lastClockSample = -1; lastEventSample = when; break;
                case 0xfc:  running = false; break;

                case 0xf2:
                    if (metadata.numBytes >= 3)
                    {
                        const int sixteenths = (metadata.data[1] & 0x7f) | ((metadata.data[2] & 0x7f) << 7);
                        ppqAtLastEvent = (double) sixteenths / 4.0;
                        lastClockSample = -1;
                        lastEventSample = when;
                    }
                    break;

                default:
                    break;
            }
        }
    }

    /** Started (or continued), and a clock or start arrived recently. */
    bool isRunning (juce::int64 atSample) const noexcept
    {
        return running && lastEventSample >= 0 && (double) (atSample - lastEventSample) < kStaleSeconds * sr;
    }

    /** The position in beats at @p atSample, moving on at @p bpm since the last
        clock but never past the next one's position. */
    double getPpqAt (juce::int64 atSample, double bpm) const noexcept
    {
        if (! running || lastEventSample < 0 || bpm <= 0.0)
            return ppqAtLastEvent;

        const double ahead = (double) (atSample - lastEventSample) * bpm / (60.0 * sr);
        return ppqAtLastEvent + juce::jlimit (0.0, 1.0 / kClocksPerBeat, ahead);
    }

private:
    double sr = 48000.0;
    bool running = false;
    double ppqAtLastEvent = 0.0;
    juce::int64 lastClockSample = -1, lastEventSample = -1;
};

} // namespace luthier
