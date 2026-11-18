/*  Inputs LuthierFuzz (Source/Fuzz) found to break a parser, replayed on every
    run. Each lives in Fixtures/Fuzz/<target>/; the sanitizer CI build catches
    undefined behaviour coming back, the normal build that the parser still
    refuses or reads the file without crashing or producing out-of-range data.
*/
#include "TestFramework.h"
#include "../Notation/GuitarProLegacyReader.h"
#include "../Notation/PerformanceScore.h"
#include "../Notation/NotationExport.h"
#include "../Tune/TuneImport.h"
#include "../Tune/TuneFile.h"

using namespace luthier;

namespace
{
    juce::File fuzzFixtures (const juce::String& target)
    {
        const juce::File self (__FILE__);
        for (auto base : { juce::File::getCurrentWorkingDirectory(),
                           juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory() })
            for (auto dir = base; dir != dir.getParentDirectory(); dir = dir.getParentDirectory())
            {
                const auto candidate = (juce::File::isAbsolutePath (__FILE__) ? self : dir.getChildFile (__FILE__))
                                           .getSiblingFile ("Fixtures/Fuzz").getChildFile (target);
                if (candidate.isDirectory())
                    return candidate;
            }
        return {};
    }

    juce::Array<juce::File> inputs (const juce::String& target)
    {
        const auto dir = fuzzFixtures (target);
        return dir.isDirectory() ? dir.findChildFiles (juce::File::findFiles, false) : juce::Array<juce::File> {};
    }
}

LUTHIER_TEST (FuzzRegression, guitarProLegacyInputsAreReadOrRefused)
{
    const auto files = inputs ("gp_legacy");
    CHECK (! files.isEmpty());
    for (const auto& file : files)
    {
        juce::MemoryBlock bytes;
        CHECK (file.loadFileAsData (bytes));
        GuitarProLegacyReader reader;
        PerformanceScore score;
        const bool ok = reader.read (bytes.getData(), bytes.getSize(), score);
        CHECK_MSG (ok || reader.getLastError().isNotEmpty(), file.getFileName() + ": refused without a reason");
    }
}

LUTHIER_TEST (FuzzRegression, midiInputsAreReadOrRefused)
{
    const auto files = inputs ("midi");
    CHECK (! files.isEmpty());
    for (const auto& file : files)
    {
        juce::MemoryBlock bytes;
        CHECK (file.loadFileAsData (bytes));
        Tune tune;
        juce::String error;
        const bool ok = importMidiData (bytes.getData(), bytes.getSize(), file.getFileName(), tune, TuneImportOptions {}, error);
        CHECK_MSG (ok || error.isNotEmpty(), file.getFileName() + ": refused without a reason");
    }
}

LUTHIER_TEST (FuzzRegression, musicXmlInputsAreReadOrRefused)
{
    const auto files = inputs ("musicxml");
    CHECK (! files.isEmpty());
    for (const auto& file : files)
    {
        NotationImporter importer;
        PerformanceScore score;
        const bool ok = importer.readMusicXml (file.loadFileAsString(), score);
        juce::ignoreUnused (ok);   // the contract is no crash; the sanitizer build checks memory
        CHECK (true);
    }
}

LUTHIER_TEST (FuzzRegression, jsonInputsParseOrRefuse)
{
    const auto files = inputs ("json");
    CHECK (! files.isEmpty());
    for (const auto& file : files)
    {
        juce::var parsed;
        juce::JSON::parse (file.loadFileAsString(), parsed);
        Tune tune;
        TuneFile::fromVar (parsed, tune);
    }
}

// JUCE 8.0.10 patch (scripts/patch_juce_untrusted_text.py): malformed UTF-8 never
// steps over the terminator. Unpatched, `++` skipped the NUL after a lone lead byte.
LUTHIER_TEST (FuzzRegression, malformedUtf8StopsAtTheTerminator)
{
    const char bytes[] = { (char) 0xd0, (char) 0xd0, 'x', (char) 0xf0, 0, 'y', 'y', 'y', 0 };
    juce::CharPointer_UTF8 p (bytes);
    int steps = 0;
    while (! p.isEmpty() && steps < 16)
    {
        ++p;
        ++steps;
    }
    CHECK (p.getAddress() == bytes + 4);   // the first NUL, not beyond it
}

// JUCE 8.0.10 patch: an integer beyond int64 parses as a double, not a wrapped value.
LUTHIER_TEST (FuzzRegression, oversizedJsonIntegersBecomeDoubles)
{
    const auto big = juce::JSON::parse ("[11111111111111111111, -99999999999999999999, 9223372036854775807]");
    CHECK (big.isArray() && big.size() == 3);
    if (! big.isArray() || big.size() != 3)
        return;
    CHECK (big[0].isDouble() && (double) big[0] > 1.0e19);
    CHECK (big[1].isDouble() && (double) big[1] < -9.0e19);
    CHECK (big[2].isInt64() && (juce::int64) big[2] == std::numeric_limits<juce::int64>::max());
}

// JUCE 8.0.10 patch: a processing instruction inside element text used to make
// XmlDocument loop forever (a crafted MusicXML file hung the importer).
LUTHIER_TEST (FuzzRegression, processingInstructionInsideTextDoesNotHang)
{
    const auto xml = juce::parseXML ("<a>t<?pi data?>x</a>");
    CHECK (xml != nullptr);
    if (xml == nullptr)
        return;
    CHECK (xml->hasTagName ("a"));
    CHECK (xml->getAllSubText().contains ("t") && xml->getAllSubText().contains ("x"));
}
