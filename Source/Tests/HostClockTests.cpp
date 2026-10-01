/*  Host-clock input resilience (Codex QA: host clock input validation).

    A misbehaving host can report a non-finite or non-positive tempo, or a
    non-finite transport position. These must never reach the tempo, tune and
    rhythm calculations. The pure validators are checked at their boundaries,
    and the processor path is driven with a NaN BPM to prove the last valid
    tempo is retained rather than clobbered. */

#include "TestFramework.h"

#include "../Support/HostClock.h"
#include "../PluginProcessor.h"

#include <cmath>
#include <limits>

using namespace luthier;

LUTHIER_TEST (HostClock, rejectsInvalidTempoAndTransportPositions)
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto infinity = std::numeric_limits<double>::infinity();

    CHECK (! HostClock::isValidTempo (nan));
    CHECK (! HostClock::isValidTempo (infinity));
    CHECK (! HostClock::isValidTempo (-infinity));
    CHECK (! HostClock::isValidTempo (0.0));
    CHECK (! HostClock::isValidTempo (-120.0));
    CHECK (HostClock::isValidTempo (20.0));
    CHECK (HostClock::isValidTempo (300.0));

    CHECK (! HostClock::isValidPosition (nan));
    CHECK (! HostClock::isValidPosition (infinity));
    CHECK (! HostClock::isValidPosition (-infinity));
    CHECK (HostClock::isValidPosition (-4.0));
    CHECK (HostClock::isValidPosition (0.0));
    CHECK (HostClock::isValidPosition (123456.75));
}

namespace
{
    /** A fixture host whose reported tempo the test can set to any value,
        including a non-finite one. */
    struct TempoHead : juce::AudioPlayHead
    {
        double bpm = 120.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (true);
            info.setBpm (bpm);
            info.setPpqPosition (0.0);
            info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
            return info;
        }
    };
}

// The processor boundary: a NaN BPM from the host must not clobber the last
// valid tempo (the gatekeeper's missing processor-path coverage for #29).
LUTHIER_TEST (HostClock, processorRetainsLastValidTempoWhenHostReportsNaN)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;

    LuthierAudioProcessor processor;
    TempoHead head;
    processor.setPlayHead (&head);
    processor.prepareToPlay (sr, block);

    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                 processor.getTotalNumInputChannels(), 2), block);
    juce::MidiBuffer midi;

    // A valid tempo establishes the retained value.
    head.bpm = 142.0;
    buffer.clear();
    processor.processBlock (buffer, midi);
    CHECK_NEAR (processor.getHostTempo(), 142.0, 1.0e-6);

    // A NaN tempo from the host must be rejected: the last valid tempo stands.
    head.bpm = std::numeric_limits<double>::quiet_NaN();
    buffer.clear();
    midi.clear();
    processor.processBlock (buffer, midi);
    CHECK (std::isfinite (processor.getHostTempo()));
    CHECK_NEAR (processor.getHostTempo(), 142.0, 1.0e-6);

    processor.setPlayHead (nullptr);
    processor.releaseResources();
}
