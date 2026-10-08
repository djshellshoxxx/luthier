/*  installer.md 1.3 / 13.6: the standalone opens Luthier files.

    JUCE's default standalone application ignores its command line, so a file
    double-clicked in the file manager (luthier.desktop: Exec=luthier %f)
    started the plugin and dropped the file. This is that application with
    two additions: the file on the command line opens once the window is up,
    and a second launch with a file hands it to the running instance
    (anotherInstanceStarted) rather than opening a second window. Routing a
    file to its loader is FileOpenRouter's, which the tests cover.

    Compiled only into the standalone (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP is
    set for the plugin target in CMakeLists.txt).
*/

#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "PluginProcessor.h"
#include "Support/FileOpenRouter.h"

namespace luthier
{

class LuthierStandaloneApp final : public juce::JUCEApplication
{
public:
    LuthierStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = JucePlugin_Name;
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #endif

        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override    { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }

    // One window: a second launch passes its file here instead.
    bool moreThanOneInstanceAllowed() override { return false; }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        openFromCommandLine (commandLine);

        if (mainWindow != nullptr)
            mainWindow->toFront (true);
    }

    void initialise (const juce::String& commandLine) override
    {
        if (! juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            mainWindow = std::make_unique<juce::StandaloneFilterWindow> (
                getApplicationName(),
                juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                createPluginHolder());

            mainWindow->setVisible (true);
        }
        else
        {
            pluginHolder = createPluginHolder();
        }

        // After the processor has restored its last state, so the file wins.
        juce::MessageManager::callAsync ([this, commandLine] { openFromCommandLine (commandLine); });
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        return std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false, juce::String {},
                                                               nullptr, juce::Array<juce::StandalonePluginHolder::PluginInOuts> {},
                                                               false);
    }

    LuthierAudioProcessor* getProcessor()
    {
        auto* holder = mainWindow != nullptr ? mainWindow->pluginHolder.get() : pluginHolder.get();
        return holder != nullptr ? dynamic_cast<LuthierAudioProcessor*> (holder->processor.get()) : nullptr;
    }

    void openFromCommandLine (const juce::String& commandLine)
    {
        const auto file = FileOpenRouter::fileFromCommandLine (commandLine);

        if (file == juce::File())
            return;

        auto* processor = getProcessor();

        if (processor == nullptr)
            return;

        juce::String error;

        if (! FileOpenRouter::open (*processor, file, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Could not open " + file.getFileName(), error);
    }

    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

} // namespace luthier

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new luthier::LuthierStandaloneApp(); }

#endif
