#include "TestFramework.h"
#include "../Notation/TabDirectiveApplier.h"
#include "../Notation/PerformanceScore.h"
#include "../Notation/TabDocument.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    ScoreNote noteAt (int fret)
    {
        ScoreNote n;
        n.fret = fret;
        return n;
    }

    PerformanceScore scoreWithIntroAndVerse()
    {
        PerformanceScore score;
        score.clear();
        auto& track = score.addTrack ("Guitar");

        ScoreMeasure intro;
        intro.sectionName = "Intro";
        intro.voices.resize (1);
        intro.voices[0].notes.push_back (noteAt (7));
        track.measures.push_back (intro);

        ScoreMeasure verse;
        verse.sectionName = "Verse";
        verse.voices.resize (1);
        verse.voices[0].notes.push_back (noteAt (5));
        track.measures.push_back (verse);
        return score;
    }
}

LUTHIER_TEST (TabDirectiveApplier, sectionHarmonicInstructionTouchesOnlyMatchingSection)
{
    auto score = scoreWithIntroAndVerse();
    NormalizedTabDocument doc;

    TabDirective d;
    d.canonicalName = "naturalHarmonic";
    d.scope = DirectiveScope::section;
    d.sectionName = "Intro";
    d.confidence = TabConfidence::high;
    doc.metadata.directives.push_back (d);

    TabImportDiagnostics diagnostics;
    TabDirectiveApplier::apply (doc, score, diagnostics);

    const auto& track = score.getTrack (0);
    CHECK (track.measures[0].voices[0].notes[0].hasTechnique (ScoreTechnique::Type::naturalHarmonic));
    CHECK (! track.measures[1].voices[0].notes[0].hasTechnique (ScoreTechnique::Type::naturalHarmonic));
}

LUTHIER_TEST (TabDirectiveApplier, partScopedInstructionTouchesOnlyRequestedGuitar)
{
    PerformanceScore score;
    score.clear();

    for (int i = 0; i < 2; ++i)
    {
        auto& track = score.addTrack ("Guitar " + juce::String (i + 1));
        ScoreMeasure intro;
        intro.sectionName = "Intro";
        intro.voices.resize (1);
        intro.voices[0].notes.push_back (noteAt (7));
        track.measures.push_back (intro);
    }

    NormalizedTabDocument doc;
    TabDirective d;
    d.canonicalName = "naturalHarmonic";
    d.scope = DirectiveScope::section;
    d.sectionName = "Intro";
    d.partIndex = 2;
    d.confidence = TabConfidence::high;
    doc.metadata.directives.push_back (d);

    TabImportDiagnostics diagnostics;
    TabDirectiveApplier::apply (doc, score, diagnostics);

    CHECK (! score.getTrack (0).measures[0].voices[0].notes[0].hasTechnique (ScoreTechnique::Type::naturalHarmonic));
    CHECK (score.getTrack (1).measures[0].voices[0].notes[0].hasTechnique (ScoreTechnique::Type::naturalHarmonic));
}

LUTHIER_TEST (TabDirectiveApplier, duplicateInstructionDoesNotDuplicateTechnique)
{
    auto score = scoreWithIntroAndVerse();
    auto& n = score.getTrack (0).measures[0].voices[0].notes[0];
    ScoreTechnique existing;
    existing.type = ScoreTechnique::Type::naturalHarmonic;
    n.techniques.push_back (existing);

    NormalizedTabDocument doc;
    TabDirective d;
    d.canonicalName = "naturalHarmonic";
    d.scope = DirectiveScope::section;
    d.sectionName = "Intro";
    d.confidence = TabConfidence::high;
    doc.metadata.directives.push_back (d);

    TabImportDiagnostics diagnostics;
    TabDirectiveApplier::apply (doc, score, diagnostics);

    int count = 0;
    for (const auto& t : n.techniques)
        if (t.type == ScoreTechnique::Type::naturalHarmonic)
            ++count;
    CHECK (count == 1);
}

LUTHIER_TEST (TabDirectiveApplier, unsupportedTremoloDirectiveIsPreservedAsWarningNotInventedTechnique)
{
    auto score = scoreWithIntroAndVerse();
    NormalizedTabDocument doc;
    TabDirective d;
    d.canonicalName = "tremoloPicking";
    d.scope = DirectiveScope::section;
    d.sectionName = "Intro";
    d.rawText = "pick the following notes as fast as possible aka tremolo picking";
    d.confidence = TabConfidence::high;
    doc.metadata.directives.push_back (d);

    TabImportDiagnostics diagnostics;
    TabDirectiveApplier::apply (doc, score, diagnostics);

    CHECK (diagnostics.warnings.joinIntoString (" ").containsIgnoreCase ("tremolo"));
    CHECK (score.getTrack (0).measures[0].voices[0].notes[0].techniques.empty());
}
