// Luthier™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary
// OWNER: WP5
// Regression coverage for bounded footswitch output admission.
#include "TestFramework.h"
#include "../ReferencePedals/Midi/MidiRouter.h"
#include "../ReferencePedals/Midi/MidiMap.h"
#include <array>
#include <memory>

using namespace luthier::reference;
using namespace luthier::reference::midi;

#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
#endif

LUTHIER_TEST (RefPedalMidiRelease, saturatedOutputRetainsEveryHeldRelease)
{
    // Exercise individual note releases, panic, transport stop and mapped CC
    // releases after a busy pedal has consumed the ordinary output capacity.
    for (int releaseKind = 0; releaseKind < 4; ++releaseKind)
    {
        MidiRouter router;
        router.prepare (48000, 128);
        auto map = std::make_unique<MapSnap>();
        map->keyboardSlots = kMaxSlots;
        for (int slot = 0; slot < kMaxSlots; ++slot)
            map->footCc[0][(size_t) (64 + slot)] = (int8_t) slot;
        router.publishMap (std::move (map));

        const bool useCc = releaseKind == 3;
        juce::MidiBuffer input;
        for (int slot = 0; slot < kMaxSlots; ++slot)
            input.addEvent (useCc ? juce::MidiMessage::controllerEvent (1, 64 + slot, 127)
                                  : juce::MidiMessage::noteOn (1, 36 + slot, (juce::uint8) 100), 0);

        RouteOut out;
        std::array<bool, kMaxSlots> delivered {};
        auto applyOutput = [&]
        {
            CHECK (out.nFoot >= 0 && out.nFoot <= kMaxRouteFoot);
            for (int i = 0; i < out.nFoot; ++i)
            {
                const int slot = out.footSlot[i];
                CHECK (slot >= 0 && slot < kMaxSlots);
                CHECK (out.foot[i].offset >= 0 && out.foot[i].offset < 128);
                if (slot >= 0 && slot < kMaxSlots)
                {
                    CHECK (delivered[(size_t) slot] != out.foot[i].press);
                    delivered[(size_t) slot] = out.foot[i].press;
                }
            }
        };
        router.process (input, {}, 0, 128, out);
        CHECK (out.nFoot == kMaxSlots);
        applyOutput();

        input.clear();
        for (int i = 0; i < kMaxRouteFoot; ++i)
        {
            const bool press = (i & 1) != 0;
            input.addEvent (useCc ? juce::MidiMessage::controllerEvent (1, 65, press ? 127 : 0)
                                  : press ? juce::MidiMessage::noteOn (1, 37, (juce::uint8) 100)
                                          : juce::MidiMessage::noteOff (1, 37), i);
        }
        if (releaseKind == 1)
            input.addEvent (juce::MidiMessage::allNotesOff (1), 100);
        else if (releaseKind == 2)
            input.addEvent (juce::MidiMessage::midiStop(), 100);
        else
            for (int slot = 0; slot < kMaxSlots; ++slot)
                input.addEvent (useCc ? juce::MidiMessage::controllerEvent (1, 64 + slot, 0)
                                      : juce::MidiMessage::noteOff (1, 36 + slot), 80 + slot);

#if LUTHIER_ALLOCATION_COUNTER
        const auto before = luthier::tests::allocationsOnThisThread();
#endif
        router.process (input, {}, 128, 128, out);
#if LUTHIER_ALLOCATION_COUNTER
        CHECK (luthier::tests::allocationsOnThisThread() == before);
#endif
        applyOutput();
        CHECK (router.droppedCount() > 0);
        for (const auto held : delivered)
            CHECK (! held);

        // No new input is needed to discharge a release, and no duplicate
        // release is allowed in a later callback.
        input.clear();
        router.process (input, {}, 256, 128, out);
        CHECK (out.nFoot == 0);
    }
}

LUTHIER_TEST (RefPedalMidiRelease, refusedPressDoesNotBlockTheNextNoteOn)
{
    MidiRouter router;
    router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>();
    map->keyboardSlots = 2;
    router.publishMap (std::move (map));

    juce::MidiBuffer input;
    for (int i = 0; i < kMaxRouteFoot; ++i)
        input.addEvent ((i & 1) == 0 ? juce::MidiMessage::noteOn (1, 37, (juce::uint8) 100)
                                    : juce::MidiMessage::noteOff (1, 37), i);
    input.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 80);
    RouteOut out;
    router.process (input, {}, 0, 128, out);
    CHECK (router.droppedCount() > 0);
    for (int i = 0; i < out.nFoot; ++i)
        CHECK (out.footSlot[i] != 0);

    // A press discarded under overload never reached the pedal. A fresh
    // note-on must therefore work without a fictional intervening release.
    input.clear();
    input.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 7);
    router.process (input, {}, 128, 128, out);
    CHECK (out.nFoot == 1);
    if (out.nFoot == 1)
    {
        CHECK (out.footSlot[0] == 0);
        CHECK (out.foot[0].press);
        CHECK (out.foot[0].offset == 7);
    }
    input.clear();
    input.addEvent (juce::MidiMessage::noteOff (1, 36), 9);
    router.process (input, {}, 256, 128, out);
    CHECK (out.nFoot == 1);
    if (out.nFoot == 1)
    {
        CHECK (out.footSlot[0] == 0);
        CHECK (! out.foot[0].press);
        CHECK (out.foot[0].offset == 9);
    }
}
