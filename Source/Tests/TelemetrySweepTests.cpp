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
