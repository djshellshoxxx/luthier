/*  The guitar in presets and state: guitar-workshop.md 6-8, file-formats.md 2. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Support/ThreadProbe.h"

#include <thread>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    float plainValue (LuthierAudioProcessor& p, const char* id)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
        return parameter->convertFrom0to1 (parameter->getValue());
    }

    void setPlain (LuthierAudioProcessor& p, const char* id, float value)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    /** A pickup part other than the one fitted in `slot`. */
    PartPtr anotherPickup (LuthierAudioProcessor& p, GuitarSlot slot)
    {
        const auto fitted = p.getCurrentGuitar().get (slot);

        for (const auto& part : p.getPartLibrary().getParts (PartType::pickup))
            if (fitted == nullptr || part->name != fitted->name)
                return part;

        return nullptr;
    }

    /** Free stores the default guitar type (a Pro one) but plays its substitute, which
        a restored state then re-applies; these tests are about the workshop's own
        round trips, so in Free they start from a guitar the edition offers. Pro is
        unchanged. */
    void startFromAnEditionGuitar (LuthierAudioProcessor& p)
    {
        if constexpr (! luthier::edition::isPro)
        {
            auto* type = p.getState().getParameter (ParamIDs::guitarType);
            type->setValueNotifyingHost (type->convertTo0to1 ((float) GuitarType::Telecaster));
            p.getParameterBridge().applyAllNow();
        }
        else
            juce::ignoreUnused (p);
    }

    juce::MemoryBlock stateOf (LuthierAudioProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }
}

//==============================================================================
LUTHIER_TEST (WorkshopPresets, aFreshInstanceNamesItsFactoryGuitar)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    CHECK (processor.hasPartsGuitar());
    CHECK_MSG (processor.getGuitarReference().startsWith ("Factory/"),
               "reference is \"" + processor.getGuitarReference() + "\"");
    CHECK (! processor.isGuitarEdited());

    const auto block = processor.getGuitarBlock();
    CHECK (block.getProperty ("override", juce::var ("absent")).isVoid());
}

LUTHIER_TEST (WorkshopPresets, anEditedGuitarTravelsWholeInTheState)
{
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);
    startFromAnEditionGuitar (source);

    auto guitar = source.getCurrentGuitar();
    const auto other = anotherPickup (source, GuitarSlot::pickupBridge);
    CHECK (other != nullptr);

    guitar.parts[(size_t) GuitarSlot::pickupBridge] = other;
    source.applyEditedGuitar (guitar);

    CHECK (source.isGuitarEdited());

    const auto state = stateOf (source);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.isGuitarEdited());
    CHECK_MSG (restored.getCurrentGuitar() == source.getCurrentGuitar(), "the edited guitar came back different");
    CHECK (restored.getCurrentGuitar().get (GuitarSlot::pickupBridge)->name == other->name);
}

LUTHIER_TEST (WorkshopPresets, aStateLoadKeepsItsOwnRefinements)
{
    // The parameters that overlap parts are refinements saved with the preset;
    // loading the preset, or re-applying everything, must not reset them to the
    // guitar's own values.
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);

    setPlain (source, ParamIDs::setupActionTreble, 2.4f);
    source.getParameterBridge().applyAllNow();
    CHECK_MSG (std::abs (plainValue (source, ParamIDs::setupActionTreble) - 2.4f) < 0.01f,
               "a full apply reset the action to " + juce::String (plainValue (source, ParamIDs::setupActionTreble)));

    const auto state = stateOf (source);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK_MSG (std::abs (plainValue (restored, ParamIDs::setupActionTreble) - 2.4f) < 0.01f,
               "the restored action is " + juce::String (plainValue (restored, ParamIDs::setupActionTreble)));
}

LUTHIER_TEST (WorkshopPresets, choosingAGuitarTypeFitsItsParts)
{
    // The user picking a type is not a state load: the guitar's own values are
    // written, so the controls show the guitar that is playing.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setPlain (processor, ParamIDs::setupActionTreble, 2.9f);

    auto* type = processor.getState().getParameter (ParamIDs::guitarType);
    // As the header's selector writes it: inside a gesture.
    type->beginChangeGesture();
    // Free plays only its six guitars (a Pro type plays as a substitute), so there
    // the type picked is one of them.
    const auto chosen = luthier::edition::isPro ? GuitarType::Classical : GuitarType::Auditorium;
    const char* chosenName = luthier::edition::isPro ? "Classical" : "Auditorium";
    type->setValueNotifyingHost (type->convertTo0to1 ((float) chosen));
    type->endChangeGesture();
    processor.getParameterBridge().applyAllNow();

    CHECK_MSG (processor.getGuitarReference().contains (chosenName),
               "reference is \"" + processor.getGuitarReference() + "\"");
    CHECK_MSG (std::abs (plainValue (processor, ParamIDs::setupActionTreble)
                         - (float) processor.getCurrentGuitar().setup.actionTrebleMm) < 0.01f,
               "action treble reads " + juce::String (plainValue (processor, ParamIDs::setupActionTreble)));
}

LUTHIER_TEST (WorkshopPresets, aMissingGuitarFileFallsBackToItsType)
{
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);
    startFromAnEditionGuitar (source);

    auto preset = source.getPresetManager().toVar ("Missing guitar");
    auto* guitarBlock = new juce::DynamicObject();
    guitarBlock->setProperty ("reference", "User/No Such Guitar 9431.luthierguitar");
    guitarBlock->setProperty ("override", juce::var());
    preset.getDynamicObject()->setProperty ("guitar", juce::var (guitarBlock));

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.takeGuitarNotices();

    CHECK (processor.getPresetManager().fromVar (preset));
    processor.getParameterBridge().applyAllNow();

    CHECK (processor.hasPartsGuitar());

    const auto notices = processor.takeGuitarNotices();
    CHECK_MSG (notices.joinIntoString (";").contains ("No Such Guitar 9431"),
               "notices: " + notices.joinIntoString ("; "));
}

LUTHIER_TEST (WorkshopPresets, saveAsGuitarWritesAFileAndPointsThePresetAtIt)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    startFromAnEditionGuitar (processor);

    auto guitar = processor.getCurrentGuitar();
    guitar.parts[(size_t) GuitarSlot::pickupBridge] = anotherPickup (processor, GuitarSlot::pickupBridge);
    processor.applyEditedGuitar (guitar);

    const juce::String name = "Luthier Test Guitar " + juce::String (juce::Random::getSystemRandom().nextInt (1000000));
    const auto file = processor.saveGuitarAs (name);

    CHECK (file.existsAsFile());
    CHECK (! processor.isGuitarEdited());
    CHECK (processor.getGuitarReference() == "User/" + file.getFileName());

    // A new instance loading this state finds the guitar by its reference.
    const auto state = stateOf (processor);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.getCurrentGuitar().get (GuitarSlot::pickupBridge)->name
           == processor.getCurrentGuitar().get (GuitarSlot::pickupBridge)->name);
    CHECK (! restored.isGuitarEdited());

    file.deleteFile();
}

LUTHIER_TEST (WorkshopPresets, saveAsPartMakesAUserPartAndFitsIt)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const juce::String name = "Luthier Test Pickup " + juce::String (juce::Random::getSystemRandom().nextInt (1000000));
    const auto saved = processor.savePartAs (GuitarSlot::pickupBridge, name);

    CHECK (saved != nullptr);

    if (saved != nullptr)
    {
        CHECK (! saved->isFactory);
        CHECK (saved->file.existsAsFile());
        CHECK (processor.getCurrentGuitar().get (GuitarSlot::pickupBridge)->name == name);
        CHECK (processor.getPartLibrary().find (PartType::pickup, name) != nullptr);

        saved->file.deleteFile();
        processor.getPartLibrary().refresh();
    }
}

//==============================================================================
namespace
{
    /*  Renders `engine` on its own thread, as a host would, until `out` is
        full, and calls `midway` from this thread once `atSample` samples are
        down. Returns the sample count at which `midway` began. */
    template <typename Fn>
    int renderWithChangeMidway (LuthierEngine& engine, std::vector<float>& out, int atSample, Fn&& midway)
    {
        constexpr int block = 256;
        std::atomic<int> written { 0 };

        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buffer (2, block);

            while (written.load() + block <= (int) out.size())
            {
                juce::MidiBuffer midi;

                if (written.load() == 0)
                    for (int note : { 45, 52, 57 })
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

                engine.processBlock (buffer, midi);
                std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + block, out.begin() + written.load());
                written += block;

                // A little slower than flat out, so the message thread gets a look in.
                std::this_thread::yield();
            }
        });

        while (written.load() < atSample)
            std::this_thread::yield();

        const int startedAt = written.load();
        midway();

        audio.join();
        return startedAt;
    }

    float largestStep (const std::vector<float>& x, int from, int to)
    {
        float largest = 0.0f;

        for (int i = juce::jmax (1, from); i < juce::jmin ((int) x.size(), to); ++i)
            largest = juce::jmax (largest, std::abs (x[(size_t) i] - x[(size_t) i - 1]));

        return largest;
    }
}

LUTHIER_TEST (WorkshopSwap, aPartSwapDuringANoteIsClickFree)
{
    constexpr double sr = 48000.0;

    PartLibrary library;
    library.refresh();

    WorkshopGuitar before;
    PartLibrary::LoadReport report;
    CHECK (library.loadGuitar (PartLibrary::getFactoryGuitarsFolder()
                                 .getChildFile (LuthierAudioProcessor::getFactoryGuitarPath (GuitarType::LesPaul)),
                               before, report));

    // A different bridge pickup and heavier strings: a swap that moves the sound.
    auto after = before;

    for (const auto& part : library.getParts (PartType::pickup))
        if (part->name != before.get (GuitarSlot::pickupBridge)->name)
            { after.parts[(size_t) GuitarSlot::pickupBridge] = part; break; }

    /*  The engine parks only an audio thread it has seen within 200 ms, and waits
        at most 250 ms for it (a stalled host must not hang the UI). On a loaded
        shared machine the test's own audio thread can be descheduled across that
        window, in which case the swap goes ahead unparked, as it would in a
        stalled host - a timing gate, not a click. Such a render is retried; a
        parked swap that steps is a real click and fails at once. */
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        LuthierEngine engine;
        engine.prepare (sr, 256);
        engine.applyWorkshopGuitar (mapSpec (before), GuitarType::LesPaul);

        std::vector<float> out ((size_t) (sr * 1.5), 0.0f);

        const int swapAt = renderWithChangeMidway (engine, out, (int) (sr * 0.6), [&]
        {
            engine.applyWorkshopGuitar (mapSpec (after), GuitarType::LesPaul);
        });

        // The audio thread really was parked: some silence around the change.
        int longestSilence = 0, run = 0;

        for (int i = swapAt - 512; i < swapAt + (int) (sr * 0.1); ++i)
        {
            run = out[(size_t) i] == 0.0f ? run + 1 : 0;
            longestSilence = juce::jmax (longestSilence, run);
        }

        if (longestSilence < 64 && attempt < 3)
        {
            std::cout << "    swap not parked (longest run of zeros " << longestSilence << "); retrying" << std::endl;
            continue;
        }

        CHECK_MSG (longestSilence >= 64, "no parked silence; longest run of zeros " + juce::String (longestSilence));

        // The note's own steepest step while it rings, before the swap.
        const float natural = largestStep (out, (int) (sr * 0.3), swapAt - 512);

        // Around the swap: the fade out, the parked silence, the fade back in.
        const float atSwap = largestStep (out, swapAt - 512, swapAt + (int) (sr * 0.1));

        CHECK_MSG (natural > 1.0e-4f, "the note is not sounding before the swap");
        CHECK_MSG (atSwap <= natural + 0.001f,
                   "the swap stepped by " + juce::String (atSwap, 5) + " against the note's own "
                     + juce::String (natural, 5) + " (-60 dBFS allowance)");
        break;
    }
}

LUTHIER_TEST (WorkshopSwap, aNotePlayedWhileParkedIsKeptNotDropped)
{
    // DECISIONS C-09: MIDI arriving while a structural change parks the audio
    // thread plays when the engine comes back, rather than being lost.
    constexpr double sr = 48000.0;
    constexpr int block = 256;

    LuthierEngine engine;
    engine.prepare (sr, block);

    std::atomic<bool> stop { false }, sendNote { false }, noteSent { false };
    std::atomic<int> blocksAfterNote { 0 };
    float peakAfter = 0.0f;

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, block);

        while (! stop.load())
        {
            juce::MidiBuffer midi;

            if (sendNote.exchange (false))
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, 0.9f), 0);
                noteSent = true;
            }

            engine.processBlock (buffer, midi);

            if (noteSent.load())
            {
                peakAfter = juce::jmax (peakAfter, buffer.getMagnitude (0, 0, block));
                ++blocksAfterNote;
            }

            std::this_thread::yield();
        }
    });

    // Let the audio thread run, then park it and play a note into the parked engine.
    std::this_thread::sleep_for (std::chrono::milliseconds (50));

    {
        LuthierEngine::ScopedStructuralChange change (engine);
        sendNote = true;

        while (! noteSent.load())
            std::this_thread::yield();

        // Stay parked for a few more blocks.
        std::this_thread::sleep_for (std::chrono::milliseconds (20));
    }

    const int blocksAtUnpark = blocksAfterNote.load();

    while (blocksAfterNote.load() < blocksAtUnpark + (int) (0.3 * sr / block))
        std::this_thread::yield();

    stop = true;
    audio.join();

    CHECK_MSG (peakAfter > 1.0e-3f, "the note played while parked never sounded (peak " + juce::String (peakAfter, 6) + ")");
}

LUTHIER_TEST (WorkshopSwap, aChangeFromTheAudioThreadItselfDoesNotWait)
{
    // The offline renderer and most tests change the guitar on the thread
    // that renders. Parking would wait for itself; it must apply at once.
    LuthierEngine engine;
    engine.prepare (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    engine.processBlock (buffer, midi);

    const auto start = juce::Time::getMillisecondCounterHiRes();
    engine.setGuitarType (GuitarType::Telecaster);
    const auto took = juce::Time::getMillisecondCounterHiRes() - start;

    CHECK (engine.getGuitarType() == GuitarType::Telecaster);
    // Machine-relative wall-clock bound: enforced only under LUTHIER_PERF=1 (the nightly).
    if (luthier::tests::perfRunRequested())
        CHECK_MSG (took < 200.0, "the change waited " + juce::String (took, 1) + " ms");
}

LUTHIER_TEST (WorkshopPresets, oldPickupPlacementParametersBecomeTheGuitars)
{
    // guitar-workshop.md 9: a preset from before the Workshop stored pickup
    // position and height as parameters; they become the guitar's placements.
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);

    auto preset = source.getPresetManager().toVar ("Before the Workshop");
    preset.getDynamicObject()->removeProperty ("guitar");

    auto* parameters = preset.getProperty ("parameters", {}).getDynamicObject();
    parameters->setProperty (ParamIDs::pickupPosition (0), 0.5);   // 0.02 + 0.5 x 0.46 = 0.25 of the scale
    parameters->setProperty (ParamIDs::pickupHeight (0), 0.0);     // the old range's floor, 1 mm

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    CHECK (processor.getPresetManager().fromVar (preset));
    processor.getParameterBridge().applyAllNow();

    // Engine slot 0 is the bridge-most fitted pickup, the file's bridge slot (index 2).
    const auto& guitar = processor.getCurrentGuitar();
    const double scale = guitar.get (GuitarSlot::neck)->number ("scale_length_mm", 648.0);

    CHECK_MSG (std::abs (guitar.placements[2].positionMm - 0.25 * scale) < 0.5,
               "bridge pickup at " + juce::String (guitar.placements[2].positionMm) + " mm, expected "
                 + juce::String (0.25 * scale));
    CHECK (std::abs (guitar.placements[2].heightTrebleMm - 1.0) < 0.01);
    CHECK_MSG (processor.isGuitarEdited(), "the migrated placements would be lost on re-save");
}

LUTHIER_TEST (WorkshopSwap, aSwapMapsOnceNotPerBlock)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto guitar = processor.getCurrentGuitar();
    guitar.parts[(size_t) GuitarSlot::pickupBridge] = anotherPickup (processor, GuitarSlot::pickupBridge);

    const int before = ThreadProbe::mapSpecCalls.load();
    processor.applyEditedGuitar (guitar);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);

    for (int block = 0; block < 100; ++block)
    {
        processor.getEngine().processBlock (buffer, midi);
        midi.clear();
    }

    processor.getParameterBridge().applyAllNow();   // a re-apply of the same guitar maps nothing

    CHECK_MSG (ThreadProbe::mapSpecCalls.load() - before == 1,
               "the mapping ran " + juce::String (ThreadProbe::mapSpecCalls.load() - before) + " times");
}

LUTHIER_TEST (WorkshopSwap, noFileIsTouchedFromTheAudioThreadDuringASwap)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    auto edited = processor.getCurrentGuitar();
    edited.parts[(size_t) GuitarSlot::pickupBridge] = anotherPickup (processor, GuitarSlot::pickupBridge);

    const int before = ThreadProbe::audioThreadFileAccesses.load();
    std::atomic<bool> running { true };

    std::thread audio ([&]
    {
        ThreadProbe::markAsAudioThread();
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);

        while (running.load())
        {
            processor.getEngine().processBlock (buffer, midi);
            midi.clear();
            std::this_thread::yield();
        }

        ThreadProbe::markAsAudioThread (false);
    });

    juce::Thread::sleep (20);

    // A part swap, then a whole other guitar. The files are read here, on the
    // message thread, before the swap; the audio thread only ever sees the result.
    processor.applyEditedGuitar (edited);

    WorkshopGuitar dreadnought;
    PartLibrary::LoadReport loaded;
    processor.getPartLibrary().loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile (
                                               LuthierAudioProcessor::getFactoryGuitarPath (GuitarType::Dreadnought)),
                                           dreadnought, loaded);
    processor.applyEditedGuitar (dreadnought);

    running = false;
    audio.join();

    CHECK_MSG (ThreadProbe::audioThreadFileAccesses.load() == before,
               juce::String (ThreadProbe::audioThreadFileAccesses.load() - before) + " file accesses from the audio thread");

    // And the probe does see one when it happens.
    ThreadProbe::markAsAudioThread();
    PartLibrary::LoadReport report;
    WorkshopGuitar scratch;
    processor.getPartLibrary().loadGuitar (processor.getGuitarFile(), scratch, report);
    ThreadProbe::markAsAudioThread (false);

    CHECK (ThreadProbe::audioThreadFileAccesses.load() == before + 1);
}

//==============================================================================
LUTHIER_TEST (WorkshopCapo, aPartialCapoClampsOnlyItsStrings)
{
    // ambiguity-resolutions.md 4.5: the Partial 3-String Capo covers A, D and G,
    // which are strings 4, 3 and 2 with string 0 the high E (engine.md 1).
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto& tuning = processor.getEngine().getTuningEngine();

    // The first full apply sets the tuning preset and the seeded realism detune,
    // so the open pitches are read after it, not before.
    processor.getParameterBridge().applyAllNow();

    double open[6];
    for (int s = 0; s < 6; ++s)
        open[s] = tuning.getEffectiveOpenFrequency (s);

    const auto partial = processor.getPartLibrary().find (PartType::capo, "Partial 3-String Capo");
    CHECK (partial != nullptr);
    processor.setCapoPart (partial);

    auto* capo = processor.getState().getParameter (ParamIDs::capoFret);
    capo->setValueNotifyingHost (capo->convertTo0to1 (2.0f));
    processor.getParameterBridge().applyAllNow();

    const double wholeTone = std::pow (2.0, 2.0 / 12.0);

    for (int s = 0; s < 6; ++s)
    {
        const bool clamped = s >= 2 && s <= 4;
        const double expected = open[s] * (clamped ? wholeTone : 1.0);

        CHECK_MSG (std::abs (tuning.getEffectiveOpenFrequency (s) / expected - 1.0) < 1.0e-6,
                   "string " + juce::String (s) + (clamped ? " should be capo'd" : " should ring open"));
        CHECK (tuning.getHighestPlayableFret (s) == tuning.getStringTuning (s).maxFrets - (clamped ? 2 : 0));
    }

    // Fret 3 on an open (unclamped) string is fret 3 from the nut.
    CHECK (std::abs (tuning.computeFrequency (0, 3.0, 0.0) / (open[0] * std::pow (2.0, 3.0 / 12.0)) - 1.0) < 1.0e-3);
}

LUTHIER_TEST (WorkshopCapo, theCapoTravelsWithThePreset)
{
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);
    source.setCapoPart (source.getPartLibrary().find (PartType::capo, "Partial 3-String Capo"));

    const auto state = stateOf (source);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.getCapoPart() != nullptr && restored.getCapoPart()->name == "Partial 3-String Capo");
    CHECK (restored.getEngine().getTuningEngine().getCapoStringMask() == 0b11100u);

    // A full capo, and a preset that names none, clamp every string.
    LuthierAudioProcessor fresh;
    CHECK (fresh.getEngine().getTuningEngine().getCapoStringMask() == TuningEngine::kAllStrings);
}

LUTHIER_TEST (WorkshopFamily, aFamilySwitchKeepsWhatSection12_4Keeps)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    startFromAnEditionGuitar (processor);

    setPlain (processor, ParamIDs::roomSize, 2.0f);
    const float room = plainValue (processor, ParamIDs::roomSize);
    const auto presetName = processor.getPresetManager().getCurrentPresetName();
    const auto seed = processor.getCurrentGuitar().seed;
    processor.takeGuitarNotices();

    CHECK (processor.switchGuitarFamily ("bass"));
    processor.getParameterBridge().applyAllNow();

    CHECK (processor.getCurrentGuitar().family == "bass");
    CHECK (processor.getCurrentGuitar().getStringCount() == 4);
    CHECK (processor.getEngine().getNumStrings() == 4);
    // Free: the bass family's template is a Pro guitar, so the bridge then plays (and
    // reloads) the nearest Free bass, whose file carries its own seed. The seed
    // carry-over is a Pro behaviour here.
    if constexpr (luthier::edition::isPro)
        CHECK (processor.getCurrentGuitar().seed == seed);
    CHECK (processor.getPresetManager().getCurrentPresetName() == presetName);
    CHECK_MSG (std::abs (plainValue (processor, ParamIDs::roomSize) - room) < 1.0e-4f, "the room changed with the family");

    // The banner (section 12.1) says what was replaced.
    const auto notices = processor.takeGuitarNotices().joinIntoString ("|");
    CHECK_MSG (notices.contains ("Family changed to Bass. Replaced parts:"), "notices: " + notices);

    // A full re-apply keeps the switched guitar rather than reloading the old one.
    processor.getParameterBridge().applyAllNow();
    CHECK (processor.getCurrentGuitar().family == "bass");

    // And it round-trips through the state.
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.getCurrentGuitar().family == "bass");
}

LUTHIER_TEST (WorkshopPresets, choosingATypeGivesItsStringCount)
{
    // A bass type plays four strings, not six guitar-tuned ones: picking a type
    // brings a tuning that can hold its strings, and a full apply keeps it.
    struct Case { GuitarType type; int strings; };

    // Free plays only its six guitars: the Pro-only types play as a substitute
    // there, so Free checks its own (a bass among them).
    std::vector<Case> cases;

    if constexpr (luthier::edition::isPro)
        cases = { Case { GuitarType::PrecisionBass, 4 }, Case { GuitarType::FiveStringBass, 5 },
                  Case { GuitarType::SevenString, 7 }, Case { GuitarType::EightString, 8 },
                  Case { GuitarType::TwelveString, 12 }, Case { GuitarType::Stratocaster, 6 } };
    else
        cases = { Case { GuitarType::JazzBass, 4 }, Case { GuitarType::Telecaster, 6 },
                  Case { GuitarType::SG, 6 }, Case { GuitarType::Auditorium, 6 } };

    for (auto c : cases)
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        auto* type = processor.getState().getParameter (ParamIDs::guitarType);
        type->setValueNotifyingHost (type->convertTo0to1 ((float) c.type));
        processor.getParameterBridge().applyAllNow();
        processor.getParameterBridge().applyAllNow();

        CHECK_MSG (processor.getEngine().getNumStrings() == c.strings,
                   juce::String (GuitarLibrary::getName (c.type)) + " plays " + juce::String (processor.getEngine().getNumStrings())
                     + " strings, want " + juce::String (c.strings));
    }

    // A tuning that fits is kept: Drop D survives a change between 6-string guitars.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    setPlain (processor, ParamIDs::tuningPreset, (float) (int) TuningPreset::DropD);
    auto* type = processor.getState().getParameter (ParamIDs::guitarType);
    type->setValueNotifyingHost (type->convertTo0to1 ((float) GuitarType::LesPaul));
    processor.getParameterBridge().applyAllNow();
    CHECK ((int) plainValue (processor, ParamIDs::tuningPreset) == (int) TuningPreset::DropD);
}
