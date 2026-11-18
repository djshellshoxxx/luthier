/*  Inputs LuthierFuzz (Source/Fuzz) found to break a parser, replayed on every
    run. Each lives in Fixtures/Fuzz/<target>/; the sanitizer CI build catches
    undefined behaviour coming back, the normal build that the parser still
    refuses or reads the file without crashing or producing out-of-range data.
*/
#include "TestFramework.h"
#include "../Notation/GuitarProLegacyReader.h"
#include "../Notation/PerformanceScore.h"

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
