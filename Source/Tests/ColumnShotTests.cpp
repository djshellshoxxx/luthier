/*  The Advanced view's scrolling columns, rendered at the default window and at
    the largest 5:3 window that fits a 1920x1080 screen, for review by eye.

    Like Screenshots.everyPanelInEveryPalette it only writes files when
    LUTHIER_SCREENSHOTS names a folder:

        LUTHIER_SCREENSHOTS=/tmp/shots xvfb-run -a LuthierTests ColumnShots

    Without it the test still renders each view once and checks it painted.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/AdvancedPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    juce::Image renderColumnsView (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB, juce::jmax (1, c.getWidth()), juce::jmax (1, c.getHeight()), true,
                           juce::SoftwareImageType());
        juce::Graphics g (image);
        c.paintEntireComponent (g, true);
        return image;
    }

    bool paintedAnything (const juce::Image& image)
    {
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < pixels.height; y += 4)
            for (int x = 0; x < pixels.width; x += 4)
                if (pixels.getPixelColour (x, y).getAlpha() != 0)
                    return true;

        return false;
    }

    template <typename T>
    T* findFirst (juce::Component& root)
    {
        if (auto* t = dynamic_cast<T*> (&root))
            return t;

        for (auto* child : root.getChildren())
            if (auto* t = findFirst<T> (*child))
                return t;

        return nullptr;
    }
}

LUTHIER_TEST (ColumnShots, advancedColumnsAtDefaultAndLargestWindow)
{
    const auto env = juce::SystemStats::getEnvironmentVariable ("LUTHIER_SCREENSHOTS", {});
    juce::File folder;

    if (env.isNotEmpty())
    {
        folder = juce::File (env);
        folder.createDirectory();
    }

    for (auto size : { juce::Point<int> (1200, 720), juce::Point<int> (1666, 1000) })
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        CHECK (editor != nullptr);

        if (editor == nullptr)
            continue;

        editor->setVisible (true);
        editor->setSize (size.x, size.y);

        if (const auto* binding = AccessibilitySettings::get().findShortcut ("toggleAdvanced"))
            editor->keyPressed (binding->key);

        auto* panel = findFirst<AdvancedPanel> (*editor);
        CHECK (panel != nullptr);

        if (panel == nullptr)
            continue;

        const auto tag = juce::String (size.x) + "x" + juce::String (size.y);

        auto shoot = [&] (const juce::String& name)
        {
            const auto image = renderColumnsView (*editor);
            CHECK_MSG (paintedAnything (image), name + " painted nothing");

            if (folder == juce::File())
                return;

            auto file = folder.getChildFile ("advanced_" + tag + "_" + name + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            CHECK (out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out));
        };

        // MOD keeps all three columns on screen; WORKSHOP takes columns 3 and 4.
        panel->setWorkspaceTabNamed ("MOD");
        shoot ("columns_top");

        // Halfway down every column: both the above and below indicators show.
        for (int c = 0; c < 3; ++c)
            if (auto* viewport = dynamic_cast<juce::Viewport*> (panel->getColumnViewport (c)))
                if (auto* content = viewport->getViewedComponent())
                    viewport->setViewPosition (0, (content->getHeight() - viewport->getMaximumVisibleHeight()) / 2);

        shoot ("columns_middle");

        panel->setWorkspaceTabNamed ("WORKSHOP");
        shoot ("workshop");
    }
}
