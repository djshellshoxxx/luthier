/* Q2 coverage additions. Linux build/run pending; see docs/review/CODEX_COVERAGE.md. */
#include "TestFramework.h"
#include "../DSP/String/FractionalDelayLine.h"
#include "../DSP/Whammy/WhammyEngine.h"
#include "../DSP/Jam/KitRoom.h"
#include "../Model/Playing/TechniqueTriggers.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (CodexQA_Coverage, integerDelaySurvivesCircularWrap)
{
    using Mode = FractionalDelayLine::Interpolation;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (auto mode : { Mode::Lagrange3, Mode::Lagrange5 })
        {
            FractionalDelayLine line;
            line.prepare (sr, 1000.0);
            line.setInterpolation (mode);
            const int capacity = line.getBufferSize();
            // Integer samples are an exact oracle, including both safe read limits.
            for (int n = 0; n < capacity * 3; ++n)
            {
                line.write ((double) (n + 1));
                for (int delay : { 1, 7, (int) line.getMaxDelay() })
                    CHECK_NEAR (line.read ((double) delay),
                                n + 2 >= delay ? (double) (n + 2 - delay) : 0.0, 1.0e-12);
            }
        }
}

LUTHIER_TEST (CodexQA_Coverage, harmonicTapDoesNotAdvanceAllpassState)
{
    FractionalDelayLine tapped, control;
    tapped.prepare (48000.0, 30.0);
    control.prepare (48000.0, 30.0);
    tapped.setInterpolation (FractionalDelayLine::Interpolation::Allpass1);
    control.setInterpolation (FractionalDelayLine::Interpolation::Allpass1);
    for (int n = 0; n < 8192; ++n)
    {
        const double input = 0.25 * std::sin (0.071 * n);
        tapped.write (input);
        control.write (input);
        const double delay = n % 97 < 48 ? 101.25 : 67.8;
        // readTap is a stateless observer even while the main allpass changes delay.
        CHECK (std::isfinite (tapped.readTap (23.375)));
        CHECK (std::isfinite (tapped.readTap (57.125)));
        CHECK_NEAR (tapped.read (delay), control.read (delay), 0.0);
    }
}

LUTHIER_TEST (CodexQA_Coverage, delayResetClearsEveryInterpolationMode)
{
    using Mode = FractionalDelayLine::Interpolation;
    for (auto mode : { Mode::Allpass1, Mode::Lagrange3, Mode::Lagrange5 })
    {
        FractionalDelayLine line;
        line.prepare (48000.0, 1000.0);
        line.setInterpolation (mode);
        for (int n = 0; n < 200; ++n)
        {
            line.write (0.5);
            (void) line.read (7.25);
        }
        line.reset();
        for (int n = 0; n < line.getBufferSize() * 2; ++n)
        {
            CHECK_NEAR (line.read (7.25), 0.0, 0.0);
            CHECK_NEAR (line.readTap (11.75), 0.0, 0.0);
            line.write (0.0);
        }
    }
}

LUTHIER_TEST (CodexQA_Coverage, whammyPerStringControlAndTransposeAreIndependent)
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        WhammyEngine whammy;
        whammy.prepare (sr, 6);
        whammy.setBridgeType (WhammyEngine::BridgeType::TransTrem);
        whammy.setRange (12.0, 12.0);
        whammy.setPerStringEnabled (true);
        whammy.setTransposeLock (3);
        whammy.setTransposeLockEnabled (true);
        whammy.setStringPosition (0, -0.5);
        whammy.setStringPosition (1, 0.25);
        whammy.updateBlock ((int) (sr * 0.25)); // 50 smoothing time constants.
        CHECK_NEAR (whammy.getCentOffset (0), -300.0, 1.0e-8);
        CHECK_NEAR (whammy.getCentOffset (1), 600.0, 1.0e-8);
        for (int s = 2; s < 6; ++s)
            CHECK_NEAR (whammy.getCentOffset (s), 300.0, 1.0e-8);
        // Removing strings must not expose stale offsets to consumers.
        whammy.setNumStrings (4);
        whammy.updateBlock (1);
        CHECK_NEAR (whammy.getCentOffset (4), 0.0, 0.0);
        CHECK_NEAR (whammy.getCentOffset (5), 0.0, 0.0);
        whammy.setTransposeLockEnabled (false);
        whammy.setDownOnly (true);
        whammy.updateBlock (1);
        CHECK_NEAR (whammy.getCentOffset (1), 0.0, 0.0);
        CHECK (whammy.getCentOffset (0) < 0.0);
    }
}

LUTHIER_TEST (CodexQA_Coverage, triggerResetDropsPendingButtonAndHeldController)
{
    TechniqueTriggers triggers;
    TechniqueTriggerConfig config;
    config.armed = true;
    config.source = TriggerSource::controller;
    config.triggerCc = 20;
    triggers.configure (TechniqueId::scrape, config);
    juce::MidiBuffer midi, filtered;
    filtered.ensureSize (1024);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 11);
    (void) triggers.process (midi, filtered);
    CHECK (triggers.isHeld (TechniqueId::scrape, 0));
    triggers.request (TechniqueId::scrape, 1, true);
    triggers.reset();
    juce::MidiBuffer empty;
    (void) triggers.process (empty, filtered);
    CHECK (triggers.getNumEvents() == 0);
    CHECK (! triggers.isHeld (TechniqueId::scrape, 0));
    CHECK (! triggers.isHeld (TechniqueId::scrape, 1));
    // Reset also clears CC edge memory, so the same high CC is a new press.
    (void) triggers.process (midi, filtered);
    CHECK (triggers.getNumEvents() == 1);
    CHECK (triggers.getEvent (0).on);
    CHECK (triggers.getEvent (0).offset == 11);
    config.armed = false;
    triggers.configure (TechniqueId::scrape, config);
    CHECK (! triggers.isHeld (TechniqueId::scrape, 0));
    CHECK (&triggers.process (midi, filtered) == &midi);
    CHECK (triggers.getNumEvents() == 0);
}

LUTHIER_TEST (CodexQA_Coverage, keyswitchFilterPreservesUnrelatedMidiBytesAndOffsets)
{
    TechniqueTriggers triggers;
    TechniqueTriggerConfig config;
    config.armed = true;
    config.keyswitches[0] = TechniqueKeyswitch::scrape;
    triggers.configure (TechniqueId::scrape, config);
    juce::MidiBuffer midi, expected, filtered;
    filtered.ensureSize (2048);
    expected.addEvent (juce::MidiMessage::noteOn (2, 64, (juce::uint8) 90), 3);
    expected.addEvent (juce::MidiMessage::controllerEvent (3, 7, 103), 19);
    expected.addEvent (juce::MidiMessage::pitchWheel (4, 9000), 25);
    expected.addEvent (juce::MidiMessage::noteOff (2, 64), 31);
    for (const auto event : expected)
        midi.addEvent (event.data, event.numBytes, event.samplePosition);
    midi.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::scrape, (juce::uint8) 127), 7);
    // MIDI note-on velocity zero must release and must also be consumed.
    midi.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::scrape, (juce::uint8) 0), 23);
    const auto& actual = triggers.process (midi, filtered);
    CHECK (&actual == &filtered);
    CHECK (actual.getNumEvents() == expected.getNumEvents());
    auto want = expected.begin();
    for (const auto got : actual)
    {
        if (want == expected.end()) { CHECK (false); break; }
        const auto event = *want;
        CHECK (got.samplePosition == event.samplePosition);
        CHECK (got.numBytes == event.numBytes);
        if (got.numBytes == event.numBytes)
            for (int b = 0; b < got.numBytes; ++b)
                CHECK (got.data[b] == event.data[b]);
        ++want;
    }
    CHECK (want == expected.end());
    CHECK (triggers.getNumEvents() == 2);
    CHECK (triggers.getEvent (0).on);
    CHECK (triggers.getEvent (0).offset == 7);
    CHECK (! triggers.getEvent (1).on);
    CHECK (triggers.getEvent (1).offset == 23);
    CHECK (! triggers.isHeld (TechniqueId::scrape, 0));
}

LUTHIER_TEST (CodexQA_Coverage, kitRoomResetReplaysImpulseAndAddsToOutput)
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        KitRoom room;
        room.prepare (sr);
        room.setDecay (0.4);
        const int count = (int) (sr * 0.2);
        std::vector<double> left ((size_t) count), right ((size_t) count);
        for (int n = 0; n < count; ++n)
            room.process (n == 0 ? 0.5 : 0.0, left[(size_t) n], right[(size_t) n]);
        CHECK_FINITE (left.data(), count);
        CHECK_FINITE (right.data(), count);
        CHECK (peak (left.data(), count) > 0.0);
        CHECK (peak (right.data(), count) > 0.0);
        room.reset();
        for (int n = 0; n < count; ++n)
        {
            double l = 0.125, r = -0.25;
            room.process (n == 0 ? 0.5 : 0.0, l, r);
            CHECK_NEAR (l, 0.125 + left[(size_t) n], 0.0);
            CHECK_NEAR (r, -0.25 + right[(size_t) n], 0.0);
        }
        room.reset();
        for (int n = 0; n < count; ++n)
        {
            double l = 0.0, r = 0.0;
            room.process (0.0, l, r);
            CHECK_NEAR (l, 0.0, 0.0);
            CHECK_NEAR (r, 0.0, 0.0);
        }
    }
}
