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
