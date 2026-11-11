/*  Review screenshots for the header, the Workshop and the LIVE tab at the
    default (1200x720) and minimum (940x564) window sizes.

    Like the Screenshots suite this only writes files when LUTHIER_SCREENSHOTS
    names a folder:

        LUTHIER_SCREENSHOTS=/tmp/shots xvfb-run -a LuthierTests GuiControlsShots

    Without it each view is still rendered once and must paint something.
    Advanced Mode (and with it the LIVE tab) needs AdvancedPanel::minimumUsableWidth,
    so at the minimum width the LIVE tab is shot at that width instead, and the
    Workshop is the Easy overlay the header's wrench opens.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/HeaderBar.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    juce::Image renderShot (juce::Component& c)
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

        for (int y = 0; y < pixels.height; y += 3)
            for (int x = 0; x < pixels.width; x += 3)
                if (pixels.getPixelColour (x, y).getAlpha() != 0)
                    return true;

        return false;
    }

    template <typename T>
    T* findFirst (juce::Component& root)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                return match;

            if (auto* deeper = findFirst<T> (*child))
                return deeper;
        }

        return nullptr;
    }

    juce::KeyPress keyFor (const char* actionId)
    {
        if (const auto* binding = AccessibilitySettings::get().findShortcut (actionId))
            return binding->key;
        return {};
    }

    struct Writer
    {
        TestContext& ctx;
        juce::File folder;

        void shoot (juce::Component& c, const juce::String& name)
        {
            const auto image = renderShot (c);
            CHECK_MSG (paintedAnything (image), name + " painted nothing");

            if (folder == juce::File())
                return;

            auto file = folder.getChildFile (name + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            CHECK (out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out));
        }
    };

    std::unique_ptr<juce::AudioProcessorEditor> open (LuthierAudioProcessor& processor, int w, int h)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

        if (editor != nullptr)
        {
            editor->setVisible (true);
            editor->setSize (w, h);
        }

        return editor;
    }
}

LUTHIER_TEST (GuiControlsShots, headerWorkshopAndLiveAtDefaultAndMinimumSizes)
{
    const auto env = juce::SystemStats::getEnvironmentVariable ("LUTHIER_SCREENSHOTS", {});
    Writer writer { ctx, env.isNotEmpty() ? juce::File (env) : juce::File() };

    if (writer.folder != juce::File())
        writer.folder.createDirectory();

    struct Size { int w, h; };

    for (const auto size : { Size { 1200, 720 }, Size { 940, 564 } })
    {
        const auto tag = juce::String (size.w) + "x" + juce::String (size.h);

        // ---- Easy: the header, and the Workshop overlay -------------------------
        {
            LuthierAudioProcessor processor;
            auto editor = open (processor, size.w, size.h);
            CHECK (editor != nullptr);

            if (editor == nullptr)
                return;

            if (auto* header = findFirst<HeaderBar> (*editor))
                writer.shoot (*header, "header_easy_" + tag);
            else
                CHECK_MSG (false, "no header in the editor");

            writer.shoot (*editor, "easy_" + tag);

            editor->keyPressed (keyFor ("toggleWorkshop"));
            writer.shoot (*editor, "workshop_easy_overlay_" + tag);
        }

        // ---- Advanced: the header, the WORKSHOP tab and the LIVE tab -------------
        {
            const int w = juce::jmax (size.w, AdvancedPanel::minimumUsableWidth);
            const auto advTag = juce::String (w) + "x" + juce::String (size.h);

            LuthierAudioProcessor processor;
            auto editor = open (processor, w, size.h);

            if (editor == nullptr)
                return;

            editor->keyPressed (keyFor ("toggleAdvanced"));

            auto* panel = findFirst<AdvancedPanel> (*editor);
            CHECK_MSG (panel != nullptr && panel->isVisible(), "Advanced did not open at " + advTag);

            if (panel == nullptr || ! panel->isVisible())
                continue;

            if (auto* header = findFirst<HeaderBar> (*editor))
                writer.shoot (*header, "header_advanced_" + advTag);

            CHECK (panel->setWorkspaceTabNamed ("WORKSHOP"));
            writer.shoot (*editor, "workshop_tab_" + advTag);

            CHECK (panel->setWorkspaceTabNamed ("LIVE"));
            writer.shoot (*editor, "live_tab_" + advTag);
        }
    }
}
