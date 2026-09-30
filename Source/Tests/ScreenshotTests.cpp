/*  Screenshots of every panel in every palette (TODO V, qa-polish.md 4).

    Renders the Easy window at 1200x720 (the default) and 1280x800, the Advanced
    window at 1600x900 and 1280x800 with each column-4 tab, the Workshop overlay,
    each Options page and every other overlay, in the Default, Light and
    High-contrast palettes, to PNG files for review by eye.

    Writing some ninety PNGs takes time the everyday run should not pay, so the
    test only renders when LUTHIER_SCREENSHOTS names a folder:

        LUTHIER_SCREENSHOTS=/tmp/shots xvfb-run -a LuthierTests Screenshots

    Without it the test still opens every view once in the default palette and
    checks each painted something, which is cheap and catches a view that
    collapses to nothing.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/Overlays.h"
#include "../Presets/PresetLibrary.h"   // FEAT-BROWSER
#include "../UI/OptionsPages.h"
#include "../UI/Theme.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    struct PaletteScope
    {
        PaletteId original = AccessibilitySettings::get().getPalette();
        PaletteColours saved = Palette::current();
        bool textured = Palette::textured;

        void use (PaletteId id)
        {
            auto& settings = AccessibilitySettings::get();
            settings.setPalette (id);
            Palette::apply (settings.getColours(), id != PaletteId::highContrast);
        }

        ~PaletteScope()
        {
            AccessibilitySettings::get().setPalette (original);
            Palette::apply (saved, textured);
        }
    };

    juce::Image render (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB, juce::jmax (1, c.getWidth()), juce::jmax (1, c.getHeight()), true,
                           juce::SoftwareImageType());
        juce::Graphics g (image);
        c.paintEntireComponent (g, true);
        return image;
    }

    bool drewSomething (const juce::Image& image)
    {
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < pixels.height; y += 4)
            for (int x = 0; x < pixels.width; x += 4)
                if (pixels.getPixelColour (x, y).getAlpha() != 0)
                    return true;

        return false;
    }

    template <typename T>
    void collect (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collect<T> (*child, found);
        }
    }

    template <typename T>
    T* findOne (juce::Component& root)
    {
        juce::Array<T*> found;
        collect<T> (root, found);
        return found.isEmpty() ? nullptr : found.getFirst();
    }

    juce::KeyPress shortcutFor (const char* actionId)
    {
        if (const auto* binding = AccessibilitySettings::get().findShortcut (actionId))
            return binding->key;
        return {};
    }

    const char* paletteName (PaletteId id)
    {
        return id == PaletteId::light ? "light" : id == PaletteId::highContrast ? "contrast" : "default";
    }

    struct Shooter
    {
        TestContext& ctx;
        juce::File folder;       ///< invalid: check only, write nothing
        juce::String prefix;
        int shots = 0;

        void shoot (juce::Component& c, const juce::String& name)
        {
            const auto image = render (c);
            CHECK_MSG (drewSomething (image), prefix + name + " painted nothing");
            ++shots;

            if (folder == juce::File())
                return;

            auto file = folder.getChildFile (prefix + name + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            CHECK (out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out));
        }
    };

    std::unique_ptr<juce::AudioProcessorEditor> openEditor (LuthierAudioProcessor& processor, int w, int h, bool advanced)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

        if (editor == nullptr)
            return {};

        editor->setVisible (true);
        editor->setSize (w, h);

        if (advanced)
            editor->keyPressed (shortcutFor ("toggleAdvanced"));

        return editor;
    }

    void shootWindows (Shooter& s, bool full)
    {
        // ---- Easy ----------------------------------------------------------------------
        for (auto size : { juce::Point<int> (1200, 720), juce::Point<int> (1280, 800) })
        {
            LuthierAudioProcessor processor;
            processor.prepareToPlay (kSr, kBlock);
            auto editor = openEditor (processor, size.x, size.y, false);

            if (editor == nullptr)
                continue;

            const auto tag = juce::String (size.x) + "x" + juce::String (size.y);
            s.shoot (*editor, "easy_" + tag);

            if (! full && size.x != 1200)
                continue;

            auto* host = findOne<OverlayHost> (*editor);

            if (host == nullptr)
                continue;

            // The Easy wrench's Workshop, and every overlay with a shortcut.
            const char* const actions[] = { "help", "presetBrowser", "export", "debugPanel", "saveAs", "toggleWorkshop" };

            for (auto* action : actions)
            {
                const auto key = shortcutFor (action);

                if (! key.isValid() || ! editor->keyPressed (key) || ! host->isShowingOverlay())
                    continue;

                // FEAT-BROWSER: the browser's rows fill from the library index; parse it here.
                if (auto* browser = dynamic_cast<PresetBrowserPanel*> (host->getCurrentOverlay()))
                {
                    processor.getPresetLibrary().refreshSynchronously();
                    browser->refilter();
                    render (*browser);
                }

                s.shoot (*editor, "easy_" + tag + "_overlay_" + action);
                editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            }

            // Options, page by page.
            if (editor->keyPressed (shortcutFor ("options")) && host->isShowingOverlay())
            {
                auto* options = host->getCurrentOverlay();
                juce::Array<juce::TextButton*> tabs;

                for (auto* child : options->getChildren())
                    if (auto* b = dynamic_cast<juce::TextButton*> (child))
                        if (b->getRadioGroupId() != 0 && b->onClick != nullptr)
                            tabs.add (b);

                for (auto* tab : tabs)
                {
                    tab->onClick();
                    s.shoot (*editor, "easy_" + tag + "_options_" + tab->getButtonText().toLowerCase().replaceCharacter (' ', '_'));
                }

                editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            }
        }

        // ---- Advanced ------------------------------------------------------------------
        for (auto size : { juce::Point<int> (1600, 900), juce::Point<int> (1280, 800) })
        {
            if (! full && size.x != 1600)
                continue;

            LuthierAudioProcessor processor;
            processor.prepareToPlay (kSr, kBlock);
            auto editor = openEditor (processor, size.x, size.y, true);

            if (editor == nullptr)
                continue;

            auto* panel = findOne<AdvancedPanel> (*editor);

            if (panel == nullptr)
                continue;

            const auto tag = juce::String (size.x) + "x" + juce::String (size.y);

            for (int t = 0; t < panel->getNumWorkspaceTabs(); ++t)
            {
                panel->setWorkspaceTab (t);
                s.shoot (*editor, "advanced_" + tag + "_" + panel->getWorkspaceTabName (t).toLowerCase().replaceCharacter (' ', '_'));
            }
        }
    }
}

//==============================================================================
LUTHIER_TEST (Screenshots, everyPanelInEveryPalette)
{
    const auto env = juce::SystemStats::getEnvironmentVariable ("LUTHIER_SCREENSHOTS", {});
    juce::File folder;

    if (env.isNotEmpty())
    {
        folder = juce::File (env);
        folder.createDirectory();
    }

    PaletteScope scope;

    for (auto id : { PaletteId::defaultDark, PaletteId::light, PaletteId::highContrast })
    {
        if (folder == juce::File() && id != PaletteId::defaultDark)
            break;

        scope.use (id);
        Shooter s { ctx, folder, juce::String (paletteName (id)) + "_" };
        shootWindows (s, folder != juce::File());
        CHECK (s.shots > 10);
    }
}
