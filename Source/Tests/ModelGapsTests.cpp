/*  MODEL-GAPS workstream (TODO 2k, 2d): the gaps the coverage refresh found.

    part-acoustics 2.1's chambering into the feedback loop, the capture's
    chord / BASS_TECH / slide calls from the engine, the FeedbackLed's rate,
    the migration backup on load, notation export off the message thread,
    the doubler's defaults, the rhythm engine's bass pattern and the sustain
    controls as modulation destinations.
*/

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../Model/Workshop/PartAcoustics.h"
#include "../Capture/PerformanceCapture.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    PartLibrary& gapsLibrary()
    {
        static PartLibrary lib = []
        {
            PartLibrary l;
            l.refreshFrom (PartLibrary::getFactoryPartsFolder(),
                           juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-no-user-parts"));
            return l;
        }();

        return lib;
    }

    WorkshopGuitar gapsFactory (const juce::String& relativePath)
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;
        gapsLibrary().loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile (relativePath), g, report);
        return g;
    }

    PartPtr gapsWithField (const PartPtr& part, const char* field, const juce::var& value)
    {
        auto copy = std::make_shared<Part> (*part);
        auto fields = juce::JSON::parse (juce::JSON::toString (part->fields));
        fields.getDynamicObject()->setProperty (field, value);
        copy->fields = fields;
        return copy;
    }
}

//==============================================================================
/*  part-acoustics 2.1: "Feedback coupling feeds ambiguity-resolutions.md 1's
    feedback path gain" - solid lowest, then chambered, semi-hollow, hollow. */
LUTHIER_TEST (ModelGaps, chamberingFeedsTheFeedbackCoupling)
{
    const auto base = gapsFactory ("Electric/Vintage Double-Cut.luthierguitar");
    CHECK (base.get (GuitarSlot::body) != nullptr);

    FeedbackSettings fb;
    fb.amount = 0.5;

    double previous = -1.0;

    for (const char* chambering : { "solid", "chambered", "semi_hollow", "hollow" })
    {
        auto g = base;
        g.parts[(size_t) GuitarSlot::body] = gapsWithField (base.get (GuitarSlot::body), "chambering", chambering);

        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setFeedback (fb);
        engine.applyWorkshopGuitar (mapSpec (g));

        const double k = engine.getFeedbackLoop().getCoupling (0);
        CHECK_MSG (k > previous, juce::String (chambering) + ": k_couple " + juce::String (k, 4)
                                    + " is not above the previous row's " + juce::String (previous, 4));
        previous = k;
    }

    // The solid body is the reference: exactly the loop the compiled guitars had.
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setFeedback (fb);
        const double compiled = engine.getFeedbackLoop().getCoupling (0);

        auto g = base;
        g.parts[(size_t) GuitarSlot::body] = gapsWithField (base.get (GuitarSlot::body), "chambering", "solid");
        engine.applyWorkshopGuitar (mapSpec (g));
        CHECK_NEAR (engine.getFeedbackLoop().getCoupling (0), compiled, 1.0e-12);
    }
}

//==============================================================================
/*  notation-export 4: "In Mono mode, chord extraction runs offline on the
    captured PerformanceScore using a template match against detected pitch
    classes per beat." A take with no chord track of its own gets its chords
    from its notes; one with a chord track keeps it. */
LUTHIER_TEST (ModelGaps, aMonoTakeGetsItsChordsOffline)
{
    PerformanceCapture capture;
    capture.prepare (48000.0);

    CaptureClock clock;
    clock.sampleRate = 48000.0;
    clock.transportPlaying = true;
    clock.bpm = 120.0;

    // Beats 0-3: A C E ringing (Am). Beats 4-7: G B D (G). One note at a time, as Mono plays.
    struct N { double beat; int note; int string; };
    const N notes[] = { { 0.0, 45, 0 }, { 0.5, 48, 1 }, { 1.0, 52, 2 }, { 4.0, 43, 3 }, { 4.5, 47, 4 }, { 5.0, 50, 5 } };

    for (const auto& n : notes)
    {
        clock.blockStartPpq = n.beat;
        clock.blockStartSample = (juce::int64) (n.beat * 24000.0);
        capture.beginBlock (clock);
        capture.noteOn (0, n.string, n.note, 2.0, 0.8f);
    }

    for (int s = 0; s < 6; ++s)
    {
        clock.blockStartPpq = s < 3 ? 3.9 : 7.9;
        clock.blockStartSample = (juce::int64) (clock.blockStartPpq * 24000.0);
        capture.beginBlock (clock);
        capture.noteOff (0, s);
    }

    capture.drain();

    auto chordNames = [] (const PerformanceScore& score)
    {
        juce::StringArray names;

        for (const auto& m : score.getTrack (0).measures)
            for (const auto& c : m.chordSymbols)
                names.add (c.second);

        return names;
    };

    PerformanceScore score;
    capture.toScore (score);
    const auto names = chordNames (score);
    CHECK_MSG (names.contains ("Am") && names.contains ("G"), "extracted: " + names.joinIntoString (", "));

    // Off: nothing is made up.
    CaptureScoreOptions none;
    none.extractChordsWhenMissing = false;
    capture.toScore (score, none);
    CHECK (chordNames (score).isEmpty());
}
