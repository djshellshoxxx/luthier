/*  The standalone application (installer.md 3.1, file-formats.md 1).

    JUCE's own StandaloneFilterApp ignores its command line and allows any number
    of instances, so a double-clicked preset launched a second, empty Luthier.
    CMake sets JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1 and this class replaces
    it: the same window and plugin holder as JUCE's, plus

    - initialise (commandLine) opens the files the launch names, once the window
      (and so the processor and editor) exist, through the editor's openFile;
    - one instance: a second launch hands its files to the running window and
      quits. On Windows and macOS that is JUCE's own mechanism
      (moreThanOneInstanceAllowed false -> anotherInstanceStarted). On Linux
      JUCE's MessageManager::broadcastMessage is an empty TODO, so JUCE's
      mechanism would drop the file on the floor; there this class keeps its own
      lock and a request folder the running window polls.

    Compiled only into the standalone: the shared code sees
    JucePlugin_Build_Standalone, and nothing but the standalone's main()
    references juce_CreateApplication, so the VST3 never links this object.
*/

#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "../PluginEditor.h"
#include "../Support/OpenFile.h"

namespace luthier
{

class LuthierStandaloneApp final : public juce::JUCEApplication
                                 #if JUCE_LINUX || JUCE_BSD
                                 , private juce::Timer
                                 #endif
{
public:
    LuthierStandaloneApp()
    {
        // As JUCE's StandaloneFilterApp, so existing settings files are found.
        juce::PropertiesFile::Options options;
        options.applicationName     = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif

        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override    { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }

    /*  One Luthier: a second double-click opens its file in this window. Linux
        answers true because JUCE's hand-off does not work there (see the top of
        the file) and initialise enforces the one instance itself. */
    bool moreThanOneInstanceAllowed() override
    {
       #if JUCE_LINUX || JUCE_BSD
        return true;
       #else
        return false;
       #endif
    }

    /** Windows and macOS: a second launch's command line, delivered by JUCE. */
    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        openLater (filesFromCommandLine (commandLine, juce::File::getCurrentWorkingDirectory()));
    }

    //==========================================================================
    void initialise (const juce::String&) override
    {
        /*  The parameter array rather than the string: JUCE has already split it
            the platform's way (argv on Linux and macOS, CommandLineToArgvW's
            quoting on Windows), and re-joining it keeps a path with spaces whole. */
        const auto files = filesFromCommandLine (commandLineForArguments(),
                                                 juce::File::getCurrentWorkingDirectory());

       #if JUCE_LINUX || JUCE_BSD
        if (! instanceLock.enter (0))
        {
            // Another Luthier holds the lock: give it the files and go.
            if (! files.isEmpty() && handOff (files))
            {
                setApplicationReturnValue (0);
                quit();
                return;
            }
            // Nothing to hand over, or the hand-off failed: run as a second window.
        }
        else
        {
            clearRequests();
            startTimer (kRequestPollMs);
        }
       #endif

        mainWindow.reset (createWindow());

        if (mainWindow != nullptr)
        {
           #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            juce::Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
           #endif
            mainWindow->setVisible (true);
        }
        else
        {
            pluginHolder = createPluginHolder();
        }

        openLater (files);
    }

    void shutdown() override
    {
       #if JUCE_LINUX || JUCE_BSD
        stopTimer();
       #endif
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
    //==========================================================================
    juce::StandaloneFilterWindow* createWindow()
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
            return nullptr;   // no display, no window (JUCE's asserts here)

        return new juce::StandaloneFilterWindow (getApplicationName(),
                                                 juce::LookAndFeel::getDefaultLookAndFeel()
                                                     .findColour (juce::ResizableWindow::backgroundColourId),
                                                 createPluginHolder());
    }

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig (channels, juce::numElementsInArray (channels));
       #else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif

        return std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(),
                                                               false, juce::String{}, nullptr,
                                                               channelConfig, false);
    }

    juce::String commandLineForArguments() const
    {
        juce::StringArray arguments;

        for (const auto& argument : getCommandLineParameterArray())
            arguments.add (argument.quoted());

        return arguments.joinIntoString (" ");
    }

    /*  The window builds the processor and the editor in its constructor, but
        the processor's saved state and the first layout are still settling:
        the files open on the next message, after all of that, so a preset
        opened here is not overwritten by the state restore. */
    void openLater (const juce::Array<juce::File>& files)
    {
        if (files.isEmpty())
            return;

        juce::MessageManager::callAsync ([this, files]
        {
            if (mainWindow == nullptr)
                return;

            mainWindow->toFront (true);

            if (auto* processor = mainWindow->getAudioProcessor())
                if (auto* editor = dynamic_cast<LuthierAudioProcessorEditor*> (processor->getActiveEditor()))
                    for (const auto& file : files)
                        editor->openFile (file);
        });
    }

   #if JUCE_LINUX || JUCE_BSD
    //==========================================================================
    /*  Linux's hand-off. The lock is a file lock, so a crashed Luthier does not
        hold it. A request is a text file of absolute paths, written under a
        temporary name and renamed, so the poll never reads half of one. */
    static constexpr int kRequestPollMs = 400;

    static juce::File getRequestFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("Luthier").getChildFile ("OpenRequests");
    }

    static bool handOff (const juce::Array<juce::File>& files)
    {
        const auto folder = getRequestFolder();

        if (! folder.createDirectory())
            return false;

        const auto name = juce::Uuid().toString();
        const auto temp = folder.getChildFile (name + ".tmp");

        juce::StringArray lines;

        for (const auto& file : files)
            lines.add (file.getFullPathName());

        return temp.replaceWithText (lines.joinIntoString ("\n"))
            && temp.moveFileTo (folder.getChildFile (name + ".open"));
    }

    /** Requests left by a launch no window answered: stale, not for this one. */
    static void clearRequests()
    {
        for (const auto& entry : juce::RangedDirectoryIterator (getRequestFolder(), false, "*"))
            entry.getFile().deleteFile();
    }

    void timerCallback() override
    {
        const auto folder = getRequestFolder();

        if (! folder.isDirectory())
            return;

        juce::Array<juce::File> files;

        for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.open"))
        {
            juce::StringArray lines;
            lines.addLines (entry.getFile().loadFileAsString());
            entry.getFile().deleteFile();

            for (const auto& line : lines)
                if (line.trim().isNotEmpty() && juce::File::isAbsolutePath (line.trim()))
                    files.addIfNotAlreadyThere (juce::File (line.trim()));
        }

        openLater (files);
    }

    juce::InterProcessLock instanceLock { "LuthierStandaloneInstance" };
   #endif

    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

} // namespace luthier

// Defines juce_CreateApplication, which the standalone wrapper's main() calls.
START_JUCE_APPLICATION (luthier::LuthierStandaloneApp)

#endif
