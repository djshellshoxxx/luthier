/*  midi-export.md 5, the import UI (MODEL-GAPS, TODO 10): File -> Import ->
    MIDI or a .mid dropped on the window, into the session, the Tune Builder
    or the looper, in either profile. */

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Export/MidiImportTargets.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    /** Four quarter notes at 120 bpm: E2, G2, A2, B2. */
    MidiPerformance phrase()
    {
        MidiPerformance p (kSr);
        p.setTempo (120.0);

        const int keys[] = { 40, 43, 45, 47 };

        for (int i = 0; i < 4; ++i)
        {
            const auto at = (juce::int64) (i * 24000);
            p.addMessage (at, juce::MidiMessage::noteOn (1, keys[i], (juce::uint8) 100));
            p.addMessage (at + 20000, juce::MidiMessage::noteOff (1, keys[i]));
        }

        return p;
    }

    juce::File writePhrase (MidiProfile profile)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-midi-import");
        dir.createDirectory();
        auto file = dir.getChildFile (juce::String ("Phrase-") + MidiProfiles::getProfileName (profile) + ".mid");

        MidiExportOptions options;
        options.profile = profile;
        juce::String error;
        MidiProfiles::exportToFile (phrase(), options, file, &error);
        return file;
    }
}

LUTHIER_TEST (MidiImport, bothProfilesGoIntoTheSession)
{
    for (auto profile : { MidiProfile::luthier, MidiProfile::generic })
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, 256);

        const auto file = writePhrase (profile);
        CHECK (MidiImportTargets::isMidiFile (file));

        const auto outcome = MidiImportTargets::importFile (processor, file, MidiImportTarget::session);
        CHECK_MSG (outcome.ok, outcome.message);
        CHECK (outcome.read.detectedProfile == profile);   // 5: auto-detected by header chunk
        CHECK (outcome.notes == 4);
        CHECK (processor.getSessionRecorder().getNumMidiEvents() == 8);
    }
}

LUTHIER_TEST (MidiImport, theTuneBuilderGetsANewTune)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 256);

    const auto outcome = MidiImportTargets::importFile (processor, writePhrase (MidiProfile::luthier), MidiImportTarget::tune);
    CHECK_MSG (outcome.ok, outcome.message);

    const auto& tune = processor.getTuneSession().getTune();
    CHECK (tune.getNumSections() == 1);
    CHECK (tune.meta.title == "Phrase-luthier");
    CHECK_NEAR (tune.meta.tempoBpm, 120.0, 1.0e-6);

    if (const auto* section = tune.getSection (0))
    {
        CHECK (section->melody.has_value());

        if (section->melody.has_value())
        {
            CHECK (section->melody->notes.size() == 4);
            juce::Array<int> pitches;

            for (const auto& n : section->melody->notes)
                pitches.add (n.pitch.value);

            CHECK (pitches.contains (40) && pitches.contains (43) && pitches.contains (45) && pitches.contains (47));
        }
    }
}

LUTHIER_TEST (MidiImport, theLooperGetsARenderedLayer)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 256);
    auto& looper = processor.getLooper();
    looper.prepare (kSr, 20.0);

    const auto outcome = MidiImportTargets::importFile (processor, writePhrase (MidiProfile::generic), MidiImportTarget::looper);
    CHECK_MSG (outcome.ok, outcome.message);

    CHECK (looper.getLoopLengthSamples() > 4 * 24000);
    const auto& layer = looper.getLayer (looper.getActiveLayer());
    CHECK (layer.getRecordedSamples() == looper.getLoopLengthSamples());

    auto& audio = const_cast<LoopLayer&> (layer).getAudio();
    CHECK_MSG (audio.getMagnitude (0, 0, looper.getLoopLengthSamples()) > 1.0e-3f, "the layer is silent");

    // A running looper is left alone.
    looper.press();
    const auto refused = MidiImportTargets::importFile (processor, writePhrase (MidiProfile::generic), MidiImportTarget::looper);
    CHECK (! refused.ok && refused.message.isNotEmpty());
}

LUTHIER_TEST (MidiImport, aDropOnTheWindowImports)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 256);

    std::unique_ptr<juce::AudioProcessorEditor> made (processor.createEditor());
    auto* editor = dynamic_cast<LuthierAudioProcessorEditor*> (made.get());
    CHECK (editor != nullptr);

    if (editor == nullptr)
        return;

    const auto file = writePhrase (MidiProfile::luthier);
    CHECK (editor->isInterestedInFileDrag ({ file.getFullPathName() }));
    CHECK (! editor->isInterestedInFileDrag ({ file.withFileExtension ("wav").getFullPathName() }));

    editor->importMidiFile (file, MidiImportTarget::session);
    CHECK (editor->getLastMidiImport().ok);
    CHECK (editor->getNotifications().getCurrentId() == "midi-import");

    // A broken file is refused with its reason on the banner.
    const auto broken = file.getSiblingFile ("Broken.mid");
    broken.replaceWithText ("not midi");
    editor->importMidiFile (broken, MidiImportTarget::tune);
    CHECK (! editor->getLastMidiImport().ok);
    CHECK (editor->getNotifications().getCurrentMessage().contains ("could not be read"));
}
