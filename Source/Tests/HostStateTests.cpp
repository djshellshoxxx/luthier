/*  Host-integration regressions found by the CLAP validator (host-integration.md 3,
    state-model.md): what a host restores must survive what a host does next.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../Presets/ExactRestore.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    juce::MemoryBlock saveState (LuthierAudioProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }

    juce::String asText (const juce::MemoryBlock& block)
    {
        return juce::String::fromUTF8 (static_cast<const char*> (block.getData()), (int) block.getSize());
    }

    /** Equal states; when not, both are left in the temp folder to diff. */
    bool sameState (const juce::String& a, const juce::String& b, const char* name)
    {
        if (a == b)
            return true;

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);
        dir.getChildFile (juce::String (name) + "-a.json").replaceWithText (a);
        dir.getChildFile (juce::String (name) + "-b.json").replaceWithText (b);
        return false;
    }
}

LUTHIER_TEST (HostState, aSessionSurvivesThePrepareThatFollowsIt)
{
    // Hosts restore a session and then prepare (and prepare again on a sample
    // rate change). prepare() used to reset the LFOs' custom shapes, the
    // envelopes' curves, the sequencers' steps and the followers' sources.
    auto source = std::make_unique<LuthierAudioProcessor>();
    source->prepareToPlay (48000.0, 256);

    auto& lfo = source->getModMatrix().getLfo (0);
    for (int i = 0; i < ModLfo::kNumBreakpoints; ++i)
        lfo.setBreakpoint (i, i % 2 == 0 ? 0.9 : -0.3);

    auto step = source->getModMatrix().getSequencer (0).getStep (3);
    step.value = 0.123;
    source->getModMatrix().getSequencer (0).setStep (3, step);

    const auto saved = saveState (*source);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->setStateInformation (saved.getData(), (int) saved.getSize());
    restored->prepareToPlay (44100.0, 512);

    for (int i = 0; i < ModLfo::kNumBreakpoints; ++i)
        CHECK_NEAR (restored->getModMatrix().getLfo (0).getBreakpoint (i), i % 2 == 0 ? 0.9 : -0.3, 1.0e-9);

    CHECK_NEAR (restored->getModMatrix().getSequencer (0).getStep (3).value, 0.123, 1.0e-9);
    CHECK (sameState (asText (saveState (*restored)), asText (saved), "luthier-restored"));
}

LUTHIER_TEST (HostState, anUnpreparedInstanceSavesTheSameStateAsAPreparedOne)
{
    // A host may save before it ever prepares (clap-validator
    // state-reproducibility-flush). The defaults must not depend on prepare().
    auto prepared = std::make_unique<LuthierAudioProcessor>();
    prepared->prepareToPlay (48000.0, 256);

    auto unprepared = std::make_unique<LuthierAudioProcessor>();

    CHECK (sameState (asText (saveState (*prepared)), asText (saveState (*unprepared)), "luthier-unprepared"));
}

//==============================================================================
/*  ExactRestore.h, for every ranged parameter, skewed ranges included:

      - A value that arrived through a set (a host's plain write, or a raw
        normalised one as a hand-edited preset holds) is stored exactly as
        getValue() reports it - what clap-validator compares after a reload -
        and a restore reads it back to the bit. The direct setValueNotifyingHost
        path drifts by a float ULP on skewed ranges, which made save -> restore
        -> save differ.

      - A constructor default never went through the round trip and can be
        unreachable; it is stored where the parameter reads it back, and that
        restores exactly too.

    Either way the plain value moves by at most a few plain-range ULPs (three
    on tap_duration's 10-2000 ms range: 0.0002 ms), so nothing audible or
    displayed changes. */
LUTHIER_TEST (HostState, aRestoredNormalisedValueReadsBackExactly)
{
    auto ulpsApart = [] (float a, float b)
    {
        long n = 0;
        for (float x = a; x != b && n < 64; x = std::nextafter (x, b))
            ++n;
        return n;
    };

    LuthierAudioProcessor p;
    juce::Random rng (0x5EED);
    juce::StringArray inexact, moved, rewritten, defaultsMoved;
    int checked = 0;

    for (auto* param : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);

        if (ranged == nullptr || ParamIDs::isJamTransient (ranged->getParameterID()))
            continue;

        // A bool keeps the normalised float it is given, so a raw draw reads
        // back as itself; the helper leaves it alone. Floats are the subject.
        const bool isFloat = dynamic_cast<juce::AudioParameterFloat*> (param) != nullptr;
        long worstUlps = 0, worstPlainUlps = 0, worstDefaultUlps = 0;

        for (int k = -1; k < 256; ++k)
        {
            // k = -1: the constructor default, untouched. Even draws: what a
            // host wrote as a plain value. Odd draws: a raw normalised value.
            if (k >= 0)
                ranged->setValueNotifyingHost (k % 2 == 0 || ! isFloat
                                                   ? ranged->convertTo0to1 (ranged->convertFrom0to1 (rng.nextFloat()))
                                                   : rng.nextFloat());

            const float live = ranged->getValue();
            const double stored = ExactRestore::storable (*ranged, live);   // what toVar writes

            if (k >= 0 && (float) stored != live)
                rewritten.add (ranged->getParameterID());
            else if (k < 0)
                worstDefaultUlps = ulpsApart ((float) stored, live);

            ranged->setValueNotifyingHost (rng.nextFloat());   // scramble
            ExactRestore::applyNormalised (*ranged, stored);
            ++checked;

            worstUlps = std::max (worstUlps, ulpsApart (ranged->getValue(), (float) stored));
            worstPlainUlps = std::max (worstPlainUlps,
                                       ulpsApart (ranged->convertFrom0to1 (ranged->getValue()),
                                                  ranged->convertFrom0to1 (live)));
        }

        if (worstUlps > 0)
            inexact.add (ranged->getParameterID() + " (" + juce::String (worstUlps) + " ulp)");

        if (worstPlainUlps > 4)
            moved.add (ranged->getParameterID() + " (" + juce::String (worstPlainUlps) + " plain ulp)");

        if (worstDefaultUlps > 4)
            defaultsMoved.add (ranged->getParameterID() + " (" + juce::String (worstDefaultUlps) + " ulp)");
    }

    rewritten.removeDuplicates (false);

    CHECK (checked > 0);
    CHECK_MSG (rewritten.isEmpty(), "a set value was not stored as getValue() reports it: " + rewritten.joinIntoString (", "));
    CHECK_MSG (inexact.isEmpty(), "restored values not read back exactly: " + inexact.joinIntoString (", "));
    CHECK_MSG (moved.isEmpty(), "restore moved the plain value by more than 4 ulps: " + moved.joinIntoString (", "));
    CHECK_MSG (defaultsMoved.isEmpty(), "a default was stored more than 4 ulps away: " + defaultsMoved.joinIntoString (", "));
}

LUTHIER_TEST (HostState, theMorphSliderAndTheCharacterAmountAreSaved)
{
    auto source = std::make_unique<LuthierAudioProcessor>();

    auto set = [] (LuthierAudioProcessor& p, const char* id, float normalised)
    {
        if (auto* param = p.getState().getParameter (id))
            param->setValueNotifyingHost (normalised);
    };

    set (*source, ParamIDs::presetMorphPosition, 0.3f);
    set (*source, ParamIDs::macroCharacter, 0.8f);

    const auto saved = saveState (*source);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->setStateInformation (saved.getData(), (int) saved.getSize());

    CHECK_NEAR (restored->getState().getParameter (ParamIDs::presetMorphPosition)->getValue(), 0.3f, 1.0e-4);
    CHECK_NEAR (restored->getState().getParameter (ParamIDs::macroCharacter)->getValue(), 0.8f, 1.0e-4);
}

LUTHIER_TEST (HostState, aHostWritingAGuitarTypeWithItsPartsKeepsTheParts)
{
    // A session or automation lands the guitar type and the body together, in
    // any order: the body the host wrote is kept (guitar-workshop 0.6's
    // shortcut writes only what the host did not).
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (48000.0, 256);

    auto* body = p->getState().getParameter (ParamIDs::bodyDepth);
    auto* type = p->getState().getParameter (ParamIDs::guitarType);
    CHECK (body != nullptr && type != nullptr);

    if (body == nullptr || type == nullptr)
        return;

    body->setValueNotifyingHost (0.013f);
    type->setValueNotifyingHost (type->convertTo0to1 (type->convertFrom0to1 (type->getValue()) + 3.0f));
    p->getParameterBridge().applyAllNow();

    CHECK_NEAR (body->getValue(), 0.013f, 1.0e-4);

    // A player's pick in the header (a gesture) loads the new guitar's parts.
    type->beginChangeGesture();
    type->setValueNotifyingHost (type->convertTo0to1 (type->convertFrom0to1 (type->getValue()) - 2.0f));
    type->endChangeGesture();
    p->getParameterBridge().applyAllNow();

    CHECK_MSG (std::abs (body->getValue() - 0.013f) > 1.0e-3, "the picked guitar did not bring its own body");
}

LUTHIER_TEST (HostState, parameterTextRoundTripsStably)
{
    // host-integration 3 (clap-validator param-conversions): value -> text ->
    // value -> text must not drift.
    LuthierAudioProcessor p;
    juce::Random rng (1234);

    for (auto* param : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);

        if (ranged == nullptr)
            continue;

        for (int k = 0; k < 8; ++k)
        {
            const float v = rng.nextFloat();
            const auto text = ranged->getText (v, 64);
            const float back = ranged->getValueForText (text);
            CHECK_MSG (ranged->getText (back, 64) == text,
                       ranged->getName (64) + ": '" + text + "' -> '" + ranged->getText (back, 64) + "'");
        }
    }
}



LUTHIER_TEST (HostState, processingDoesNotMoveParameters)
{
    // clap-validator param-set-wrong-namespace: a host that sends nothing
    // must see every parameter where it left it, before and after processing.
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->setRateAndBufferSizeDetails (48000.0, 512);

    std::vector<float> before;
    for (auto* param : p->getParameters())
        before.push_back (param->getValue());

    p->prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (p->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;

    for (int b = 0; b < 20; ++b)
    {
        buffer.clear();
        p->processBlock (buffer, midi);
    }

    juce::StringArray moved;
    for (int i = 0; i < p->getParameters().size(); ++i)
        if (std::abs (p->getParameters()[i]->getValue() - before[(size_t) i]) > 1.0e-6f)
            moved.add (p->getParameters()[i]->getName (64) + " " + juce::String (before[(size_t) i]) + " -> "
                       + juce::String (p->getParameters()[i]->getValue()));

    CHECK_MSG (moved.isEmpty(), "parameters moved by themselves: " + moved.joinIntoString (", "));
}

LUTHIER_TEST (HostState, theSameParametersGiveTheSameStateHoweverTheyArrived)
{
    // clap-validator state-reproducibility-flush: one instance gets random
    // values while processing, another gets them without ever processing.
    // Their saved states must match.
    juce::Random rng (777);
    std::vector<float> values;

    auto first = std::make_unique<LuthierAudioProcessor>();
    for (int i = 0; i < first->getParameters().size(); ++i)
        values.push_back (rng.nextFloat());

    first->prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (first->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;

    for (int i = 0; i < first->getParameters().size(); ++i)
        first->getParameters()[i]->setValueNotifyingHost (values[(size_t) i]);

    for (int b = 0; b < 5; ++b)
    {
        buffer.clear();
        first->processBlock (buffer, midi);
    }

    auto second = std::make_unique<LuthierAudioProcessor>();

    for (int i = 0; i < second->getParameters().size(); ++i)
        second->getParameters()[i]->setValueNotifyingHost (values[(size_t) i]);

    CHECK (sameState (asText (saveState (*first)), asText (saveState (*second)), "luthier-arrival"));
}

//==============================================================================
// HI-8: the version string must name the exact build, not just "1.0.0", so a
// support thread can tell two builds with the same marketing version apart.
LUTHIER_TEST (HostState, versionCarriesABuildString)
{
    const auto full = getFullVersionString();

    CHECK (full.startsWith (JucePlugin_VersionString));
    CHECK (full.contains ("+"));
    CHECK (full.fromLastOccurrenceOf ("+", false, false).isNotEmpty());
}

//==============================================================================
// HI-20: the root JSON now carries a format version, so a later build can tell
// an old blob apart from one of its own.
LUTHIER_TEST (HostState, theStateCarriesAFormatVersion)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    const auto saved = saveState (p);
    const auto parsed = juce::JSON::parse (asText (saved));
    auto* root = parsed.getDynamicObject();

    CHECK (root != nullptr);
    CHECK (root != nullptr && root->hasProperty ("formatVersion"));
    CHECK (root != nullptr
           && (int) root->getProperty ("formatVersion") == LuthierAudioProcessor::kCurrentStateFormatVersion);
}

//==============================================================================
// HI-24: a root-level key this build does not recognise (a section a newer
// build added) used to vanish silently on the next save. It now travels
// through unchanged.
LUTHIER_TEST (HostState, unknownSectionsSurviveWriteBack)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    auto saved = juce::JSON::parse (asText (saveState (p)));
    auto* root = saved.getDynamicObject();
    CHECK (root != nullptr);

    if (root == nullptr)
        return;

    // Simulate a newer build's blob: a section this one has never heard of,
    // plus a format version ahead of what it understands.
    root->setProperty ("futureFeature", juce::var (juce::String ("keep-me")));
    root->setProperty ("formatVersion", LuthierAudioProcessor::kCurrentStateFormatVersion + 1);

    const auto withExtra = juce::JSON::toString (saved, false);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 256);
    restored.setStateInformation (withExtra.toRawUTF8(), (int) withExtra.getNumBytesAsUTF8());

    CHECK (restored.getUnknownHostSections().contains ("futureFeature"));
    CHECK (restored.getUnknownHostSections()["futureFeature"].toString() == "keep-me");

    // A newer-than-understood format version raises one banner.
    const auto notices = restored.takeGuitarNotices();
    CHECK (! notices.isEmpty());

    const auto reSaved = juce::JSON::parse (asText (saveState (restored)));
    auto* reRoot = reSaved.getDynamicObject();

    CHECK (reRoot != nullptr && reRoot->hasProperty ("futureFeature"));
    CHECK (reRoot != nullptr && reRoot->getProperty ("futureFeature").toString() == "keep-me");
}

//==============================================================================
// HI-25: loading a blob older than the current format used to migrate it in
// place with nothing kept of the original.
LUTHIER_TEST (HostState, anOldBlobIsBackedUpBeforeMigration)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    auto saved = juce::JSON::parse (asText (saveState (p)));
    auto* root = saved.getDynamicObject();
    CHECK (root != nullptr);

    if (root == nullptr)
        return;

    root->removeProperty ("formatVersion");   // a pre-versioning blob

    const auto old = juce::JSON::toString (saved, false);

    auto before = Diagnostics::getDiagnosticsFolder().findChildFiles (
        juce::File::findFiles, false, "state-backup-*.json");

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 256);
    restored.setStateInformation (old.toRawUTF8(), (int) old.getNumBytesAsUTF8());

    auto after = Diagnostics::getDiagnosticsFolder().findChildFiles (
        juce::File::findFiles, false, "state-backup-*.json");

    CHECK (after.size() > before.size());

    for (auto& f : after)
        if (! before.contains (f))
            f.deleteFile();
}

//==============================================================================
// QA-48: bypassed output must be bit-identical to no plugin at all. Luthier is
// an instrument with no main input, so that means silence - not whatever the
// buffer already held (the JUCE default merely passes the buffer through).
LUTHIER_TEST (HostState, bypassOutputsSilence)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.getWritePointer (ch)[0] = 0.5f;   // garbage the JUCE default would pass through

    p.processBlockBypassed (buffer, midi);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        CHECK_NEAR (buffer.getMagnitude (ch, 0, buffer.getNumSamples()), 0.0f, 1.0e-9f);
}

//==============================================================================
// HI-45: getNumPrograms/getProgramName/setCurrentProgram enumerate and load
// the preset bank by index (host-integration.md section 12), untested before.
LUTHIER_TEST (HostState, programsEnumerateFactoryPresetsAndLoadByIndex)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    const int factoryCount = p.getPresetManager().getNumPresets();
    CHECK_MSG (factoryCount > 1, "no factory presets to enumerate, so this proves nothing");

    if (factoryCount <= 1)
        return;

    CHECK (p.getNumPrograms() == factoryCount);

    for (int i = 0; i < factoryCount; ++i)
    {
        const auto* info = p.getPresetManager().getPreset (i);
        CHECK (info != nullptr);

        if (info == nullptr)
            continue;

        CHECK (p.getProgramName (i) == info->name);
    }

    p.setCurrentProgram (factoryCount - 1);
    CHECK (p.getCurrentProgram() == factoryCount - 1);
    CHECK (p.getPresetManager().getCurrentPresetIndex() == factoryCount - 1);

    p.setCurrentProgram (0);
    CHECK (p.getCurrentProgram() == 0);
}
