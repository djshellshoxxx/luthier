/*  Shared by the sizing tests (EditorSizingTests, LayoutIntegrityTests) and the
    sizing screenshot sweep (ScreenshotTests): the window sizes the GUI has to
    work at, and a board loaded with the six pedals of a first-release rig.

    The sizes are the effective minimum (940x564: 940 wide at the editor's 5:3),
    the default (1200x720) and the largest 5:3 window a 1920x1080 screen with a
    taskbar (1920x1000 usable) can hold (1666x1000).
*/

#pragma once

#include "TestFramework.h"
#include "RefPedalUiTestSupport.h"
#include "../PluginProcessor.h"
#include "../ReferencePedals/PedalRegistry.h"

#include <array>

namespace luthier::tests::sizing
{
    struct WindowSize { int width, height; const char* tag; };

    inline constexpr std::array<WindowSize, 3> kReviewSizes {{ { 940, 564, "940x564" },
                                                               { 1200, 720, "1200x720" },
                                                               { 1666, 1000, "1666x1000" } }};

    /*  Six generic pedals: the board the first release ships with is six pedals
        long, and the generic faces share the common 75x125 mm enclosure. */
    inline constexpr std::array<const char*, 6> kSixPedals { "generic.compressor", "generic.boost", "generic.overdrive",
                                                             "generic.chorus", "generic.delay", "generic.reverb" };

    inline bool loadSixPedals (TestContext& context, reference::BoardModel& model)
    {
        reference::PedalRegistry::registerAll();

        for (const auto& id : model.getOrder())
            if (! requireReferenceEdit (context, model, reference::cmd::Remove { id }))
                return false;

        for (auto* id : kSixPedals)
            if (! requireReferenceEdit (context, model, reference::cmd::Add { id }))
                return false;

        return model.getInstanceCount() == (int) kSixPedals.size();
    }

    template <typename T>
    T* findChild (juce::Component& root)
    {
        if (auto* match = dynamic_cast<T*> (&root))
            return match;

        for (auto* child : root.getChildren())
            if (auto* match = findChild<T> (*child))
                return match;

        return nullptr;
    }
}
