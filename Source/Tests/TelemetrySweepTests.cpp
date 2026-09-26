/*  SPEC-SWEEP: updates-telemetry.md 3 and 4 (UT-2, UT-18). */

#include "TestFramework.h"

#include "../Updates/Telemetry.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    class CountingTransport final : public Transport
    {
    public:
        Result get (const juce::String&, int) override { ++gets; return answer; }

        Result post (const juce::String&, const juce::String& body, int) override
        {
            ++posts;
            lastBody = body;
            return answer;
        }

        int gets = 0, posts = 0;
        juce::String lastBody;
        Result answer;
    };
}

//==============================================================================
/*  UT-2: a record carries no personal data, whatever a call site passes. */
LUTHIER_TEST (Telemetry, recordsCarryNoPersonalData)
{
    Telemetry telemetry;

    const juce::String presetName ("Sarahs Wedding Set");
    const juce::String path ("/home/sarah/Documents/Luthier/Presets/wedding.luthierpreset");
    const juce::String email ("sarah@example.invalid");

    telemetry.record (Telemetry::Category::usage, "sweep.privacy",
                      { { "presetName", presetName },
                        { "host", path },
                        { "licenseKey", "ABCD-EFGH-IJKL" },
                        { "error", email },
                        { "panel", "rhythm" },
                        { "sampleRate", "48000" } });

    const auto log = telemetry.readTelemetryLog();
    juce::String line;

    for (const auto& l : log)
        if (l.contains ("sweep.privacy"))
            line = l;

    CHECK_MSG (line.isNotEmpty(), "the record never reached the local log");
    CHECK (! line.contains (presetName));
    CHECK (! line.contains ("/home/sarah"));
    CHECK (! line.contains ("ABCD-EFGH"));
    CHECK (! line.contains (email));
    CHECK (line.contains ("rhythm"));
    CHECK (line.contains ("48000"));

    CHECK (! Telemetry::isAllowedField ("presetName", "x"));
    CHECK (Telemetry::isAllowedField ("panel", "rhythm"));
    CHECK (! Telemetry::isAllowedField ("panel", "C:\\Users\\sarah"));
}

/*  UT-18: a crash upload tries three times, then leaves the dump on disk; a
    successful one sends once and removes it. */
LUTHIER_TEST (Telemetry, crashUploadTriesThreeTimesThenKeepsTheDump)
{
    const auto dir = Telemetry::getDiagnosticsDirectory();
    dir.createDirectory();

    // Named and dated to sort after any real dump, then removed.
    const auto dump = dir.getChildFile ("crash-29991231235959.dmp");
    CHECK (dump.replaceWithText ("stack: frame0 frame1\nbuild: sweep\n"));
    dump.setLastModificationTime (juce::Time::getCurrentTime() + juce::RelativeTime::days (3650));

    {
        Telemetry telemetry;

        if (telemetry.getPolicy().present && ! telemetry.getPolicy().allowCrashUpload)
        {
            dump.deleteFile();
            return;   // a managed machine forbids it; nothing to test
        }

        auto fake = std::make_unique<CountingTransport>();
        auto* transport = fake.get();
        transport->answer.succeeded = false;
        transport->answer.error = "unreachable";

        telemetry.setTransport (std::move (fake));
        telemetry.setCrashUploadEnabled (true);

        CHECK (telemetry.getPendingCrashReport() == dump);
        CHECK (! telemetry.uploadPendingCrashReport());
        CHECK_MSG (transport->posts == 3, "posted " + juce::String (transport->posts) + " times");
        CHECK (dump.existsAsFile());

        transport->posts = 0;
        transport->answer.succeeded = true;
        transport->answer.statusCode = 200;

        CHECK (telemetry.uploadPendingCrashReport());
        CHECK (transport->posts == 1);
        CHECK (! dump.existsAsFile());
    }

    dump.deleteFile();
}

//==============================================================================
/*  UT-21 / UT-22: the PRIVACY page explains each category, its toggles write
    the switches, View shows the log and Clear empties it. */
#include "../PluginProcessor.h"
#include "../UI/OptionsPages.h"

namespace
{
    template <typename T>
    void collectAllOf (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collectAllOf<T> (*child, found);
        }
    }
}

LUTHIER_TEST (Telemetry, thePrivacyPageExplainsSwitchesAndShowsTheLog)
{
    LuthierAudioProcessor processor;
    auto& telemetry = processor.getTelemetry();
    const auto saved = telemetry.toVar();

    PrivacyPage page (processor);
    page.setSize (700, 900);
    page.refresh();

    juce::Array<juce::Label*> labels;
    collectAllOf (page, labels);

    int explanations = 0;
    for (auto* l : labels)
        if (l->getText().contains ("identifies you") || l->getText().contains ("never contain audio"))
            ++explanations;

    CHECK (explanations == 3);

    juce::Array<juce::ToggleButton*> toggles;
    collectAllOf (page, toggles);

    auto toggle = [&toggles] (const juce::String& text) -> juce::ToggleButton*
    {
        for (auto* t : toggles)
            if (t->getButtonText() == text)
                return t;
        return nullptr;
    };

    auto* usage = toggle ("Usage telemetry");
    auto* crash = toggle ("Crash reports");
    CHECK (usage != nullptr && crash != nullptr);

    if (telemetry.isManagedByPolicy() || usage == nullptr || crash == nullptr)
        return;

    usage->setToggleState (true, juce::sendNotificationSync);
    CHECK (telemetry.isCategoryEnabled (Telemetry::Category::usage));
    usage->setToggleState (false, juce::sendNotificationSync);
    CHECK (! telemetry.isCategoryEnabled (Telemetry::Category::usage));

    crash->setToggleState (true, juce::sendNotificationSync);
    CHECK (telemetry.isCrashUploadEnabled());
    crash->setToggleState (false, juce::sendNotificationSync);

    // View / Clear.
    telemetry.record (Telemetry::Category::usage, "sweep.view");
    telemetry.checkForUpdate (Version::parse ("0.0.1"), true);   // no transport in tests: a logged, unsent call

    juce::Array<juce::TextButton*> buttons;
    collectAllOf (page, buttons);

    juce::TextButton* view = nullptr;
    juce::TextButton* clear = nullptr;
    for (auto* b : buttons)
    {
        if (b->getButtonText() == "View last upload")     view = b;
        if (b->getButtonText() == "Clear all local logs") clear = b;
    }

    CHECK (view != nullptr && clear != nullptr);

    juce::Array<juce::TextEditor*> editors;
    collectAllOf (page, editors);

    if (view != nullptr && clear != nullptr)
    {
        view->onClick();

        bool shown = false;
        for (auto* e : editors)
            shown = shown || e->getText().isNotEmpty() && e->isReadOnly();

        CHECK_MSG (shown || telemetry.readNetworkLog().isEmpty(), "View did not show the log");

        clear->onClick();
        CHECK (telemetry.readTelemetryLog().isEmpty());
    }

    telemetry.fromVar (saved);
    telemetry.saveSettings();
}

//==============================================================================
/*  UT-16 / UT-19 / UT-30: the crash writer names its file crash-<timestamp>.dmp
    and carries stack, build and host - never preset names, audio or MIDI. */
#include "../Updates/CrashWriter.h"

LUTHIER_TEST (Telemetry, crashDumpsContainNoAudioMidiOrPresets)
{
    LuthierAudioProcessor processor;

    // A distinctive preset name loaded, and sentinel patterns in audio / MIDI.
    {
        auto& presets = processor.getPresetManager();
        presets.fromVar (presets.toVar ("Zorblax Sentinel Preset"));
        CHECK (presets.getCurrentPresetName() == "Zorblax Sentinel Preset");
    }

    juce::AudioBuffer<float> audio (2, 256);
    for (int i = 0; i < 256; ++i)
        audio.setSample (0, i, 0.123456f);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 99), 0);
    processor.prepareToPlay (48000.0, 256);
    processor.processBlock (audio, midi);

    CrashWriter::setInfo ({ "Luthier test", "TestHost", "Standalone" });

    juce::TemporaryFile dir;
    dir.getFile().createDirectory();

    const auto dump = CrashWriter::writeDump (dir.getFile(), juce::SystemStats::getStackBacktrace());
    CHECK (dump.existsAsFile());
    CHECK (dump.getFileName().matchesWildcard ("crash-??????????????.dmp", true));

    const auto text = dump.loadFileAsString();
    CHECK (text.contains ("stack:"));
    CHECK (text.contains ("build: Luthier test"));
    CHECK (text.contains ("host: TestHost"));

    CHECK (! text.contains ("Zorblax"));
    CHECK (! text.contains ("MThd"));
    CHECK (! text.contains ("0.123456"));

    dir.getFile().deleteRecursively();
}

//==============================================================================
/*  UT-26: a policy file switches things off, the switches refuse to turn back
    on, and the PRIVACY page says it is managed and greys the locked toggles. */
LUTHIER_TEST (Telemetry, aPolicyLocksThePrivacyPage)
{
    juce::TemporaryFile policyFile (".json");
    CHECK (policyFile.getFile().replaceWithText (R"({ "allow_usage_telemetry": false, "allow_crash_upload": false })"));
    Policy::setPolicyFileForTesting (policyFile.getFile());

    {
        LuthierAudioProcessor processor;
        auto& telemetry = processor.getTelemetry();
        const auto saved = telemetry.toVar();
        telemetry.refreshPolicy();

        CHECK (telemetry.isManagedByPolicy());

        telemetry.setCategoryEnabled (Telemetry::Category::usage, true);
        telemetry.setCrashUploadEnabled (true);
        CHECK (! telemetry.isCrashUploadEnabled());

        PrivacyPage page (processor);
        page.setSize (700, 900);
        page.refresh();

        juce::Array<juce::Label*> labels;
        collectAllOf (page, labels);

        bool managed = false;
        for (auto* l : labels)
            managed = managed || l->getText().startsWith ("Managed by policy");

        CHECK (managed);

        juce::Array<juce::ToggleButton*> toggles;
        collectAllOf (page, toggles);

        for (auto* t : toggles)
        {
            if (t->getButtonText() == "Usage telemetry" || t->getButtonText() == "Crash reports")
                CHECK_MSG (! t->isEnabled(), t->getButtonText() + " is not locked");

            if (t->getButtonText() == "Diagnostics telemetry")
                CHECK (t->isEnabled());
        }

        telemetry.setCategoryEnabled (Telemetry::Category::usage, false);
        telemetry.fromVar (saved);
    }

    Policy::setPolicyFileForTesting ({});
}

//==============================================================================
/*  UT-4: the update check's network call runs on a worker thread. */
namespace
{
    class ThreadRecordingTransport final : public Transport
    {
    public:
        Result get (const juce::String&, int) override
        {
            onMessageThread = juce::MessageManager::getInstance()->isThisTheMessageThread();
            called.signal();

            Result r;
            r.succeeded = false;
            r.error = "offline";
            return r;
        }

        Result post (const juce::String&, const juce::String&, int) override { return {}; }

        std::atomic<bool> onMessageThread { true };
        juce::WaitableEvent called;
    };
}

LUTHIER_TEST (Telemetry, theUpdateCheckRunsOffTheMessageThread)
{
    Telemetry telemetry;

    if (telemetry.isManagedByPolicy() && ! telemetry.getPolicy().allowUpdateCheck)
        return;

    auto fake = std::make_unique<ThreadRecordingTransport>();
    auto* transport = fake.get();
    telemetry.setTransport (std::move (fake));
    telemetry.setUpdateCheckEnabled (true);

    telemetry.checkForUpdateAsync (Version::parse ("0.0.1"), true, nullptr);

    CHECK_MSG (transport->called.wait (5000), "the check never reached the transport");
    CHECK (! transport->onMessageThread.load());

    juce::Thread::sleep (50);   // let the worker return before the Telemetry goes
}
