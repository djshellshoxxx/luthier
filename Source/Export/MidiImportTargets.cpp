#include "MidiImportTargets.h"

#include "../PluginProcessor.h"
#include "../Tune/TuneTemplates.h"
#include "../Tune/TuneMelody.h"

#include <map>

namespace luthier
{

const char* getMidiImportTargetName (MidiImportTarget t) noexcept
{
    switch (t)
    {
        case MidiImportTarget::tune:   return "Tune Builder (as a new tune)";
        case MidiImportTarget::looper: return "Looper (as a layer)";
        case MidiImportTarget::session:
        case MidiImportTarget::numTargets:
        default:                       return "Current session (session recorder)";
    }
}

bool MidiImportTargets::isMidiFile (const juce::File& file)
{
    return file.existsAsFile() && (file.hasFileExtension ("mid") || file.hasFileExtension ("midi"));
}

//==============================================================================
namespace
{
    struct ImportedNote
    {
        juce::int64 start = 0, end = 0;
        int pitch = 60, velocity = 100;
    };

    /** The performance's notes, paired on (channel, pitch). */
    std::vector<ImportedNote> notesOf (const MidiPerformance& performance)
    {
        std::vector<ImportedNote> notes;
        std::map<int, size_t> open;   // channel * 128 + pitch -> index

        for (const auto& m : performance.getMessages())
        {
            const auto& msg = m.message;
            const int key = msg.getChannel() * 128 + msg.getNoteNumber();

            if (msg.isNoteOn())
            {
                open[key] = notes.size();
                notes.push_back ({ m.sample, -1, msg.getNoteNumber(), msg.getVelocity() });
            }
            else if (msg.isNoteOff())
            {
                if (auto it = open.find (key); it != open.end())
                {
                    notes[it->second].end = m.sample;
                    open.erase (it);
                }
            }
        }

        const auto length = performance.getLengthInSamples();

        for (auto& n : notes)
            if (n.end < n.start)
                n.end = juce::jmax (n.start + 1, length);

        return notes;
    }
}

juce::AudioBuffer<float> MidiImportTargets::render (const MidiPerformance& performance, GuitarType guitar,
                                                    double sampleRate, double maxSeconds)
{
    return render (performance, guitar, sampleRate, maxSeconds, AutoArticulationSettings {});
}

juce::AudioBuffer<float> MidiImportTargets::render (const MidiPerformance& performance, GuitarType guitar,
                                                    double sampleRate, double maxSeconds,
                                                    const AutoArticulationSettings& assist)
{
    constexpr int block = 512;

    LuthierEngine engine;
    engine.prepare (sampleRate, block);
    engine.setGuitarType (guitar);
    engine.reset();

    // FEAT-ASSIST (auto-articulation.md 5): Luthier-profile NOTE events are pre-articulated.
    engine.setAutoArticulation (assist);
    engine.setAssistPreArticulated (performance.countEvents (LuthierEventClass::note) > 0);

    // The notes and a second of ring-out, up to the looper's length.
    const auto length = (juce::int64) juce::jmin ((double) performance.getLengthInSamples() + sampleRate,
                                                  maxSeconds * sampleRate);
    juce::AudioBuffer<float> out (2, (int) juce::jmax ((juce::int64) 1, length));
    out.clear();

    const auto& messages = performance.getMessages();
    size_t next = 0;
    juce::AudioBuffer<float> buffer (2, block);

    for (juce::int64 at = 0; at < length; at += block)
    {
        const int n = (int) juce::jmin ((juce::int64) block, length - at);
        juce::MidiBuffer midi;

        while (next < messages.size() && messages[next].sample < at + n)
        {
            if (MidiPerformance::isChannelVoiceMessage (messages[next].message))
                midi.addEvent (messages[next].message, (int) juce::jmax ((juce::int64) 0, messages[next].sample - at));

            ++next;
        }

        buffer.setSize (2, n, false, false, true);
        buffer.clear();
        engine.processBlock (buffer, midi);

        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, (int) at, buffer, c, 0, n);
    }

    return out;
}

MidiImportOutcome MidiImportTargets::importPerformance (LuthierAudioProcessor& processor, const MidiPerformance& performance,
                                                        MidiImportTarget target, const juce::String& name)
{
    MidiImportOutcome outcome;
    const auto notes = notesOf (performance);
    outcome.notes = (int) notes.size();

    if (notes.empty())
    {
        outcome.message = name + " has no notes to import.";
        return outcome;
    }

    switch (target)
    {
        case MidiImportTarget::tune:
        {
            // A new tune: one section long enough for the notes, whose melody
            // is the notes as the TUNE tab's Record takes them.
            auto tune = TuneTemplateLibrary::createBlank();
            tune.meta.title = name;
            tune.meta.tempoBpm = juce::jlimit (Tune::kMinTempo, Tune::kMaxTempo, performance.getTempoAt (0.0));

            const auto sig = performance.getTimeSignatureAt (0.0);
            tune.meta.timeSigNumerator = sig.numerator;
            tune.meta.timeSigDenominator = sig.denominator;

            const double lastBeat = performance.sampleToBeat ((double) notes.back().end);
            const double beatsPerBar = juce::jmax (1.0, tune.getBeatsPerBar());

            TuneSection section;
            section.name = "Imported";
            section.lengthBars = juce::jlimit (1, Tune::kMaxBars, (int) std::ceil (lastBeat / beatsPerBar + 1.0e-9));
            tune.arrangement.sections.push_back (section);
            tune.arrangement.setlist.push_back ({ section.name, 1, {} });

            auto& session = processor.getTuneSession();
            session.newTune (tune);
            session.beginRecording (0);

            for (const auto& n : notes)
                session.recordNote (n.pitch, n.velocity, performance.sampleToBeat ((double) n.start),
                                    performance.sampleToBeat ((double) n.end));

            outcome.ok = session.finishRecording (QuantiseGrid::sixteenth, false, false);
            outcome.message = outcome.ok ? name + " is a new tune in the Tune Builder."
                                         : name + " could not be made into a tune.";
            break;
        }

        case MidiImportTarget::looper:
        {
            auto& looper = processor.getLooper();

            if (looper.getState() != Looper::State::stopped)
            {
                outcome.message = "Stop the looper to load " + name + " into it.";
                break;
            }

            const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
            const auto audio = render (performance, processor.getEngine().getGuitarType(), rate, Looper::kMaxLoopSeconds,
                                       processor.getEngine().getMidiInterpreter().getAutoArticulation());   // FEAT-ASSIST

            outcome.ok = looper.loadLayerAudio (looper.getActiveLayer(), audio);
            outcome.message = outcome.ok ? name + " is on looper layer " + juce::String (looper.getActiveLayer() + 1) + "."
                                         : name + " could not be loaded into the looper.";
            break;
        }

        case MidiImportTarget::session:
        case MidiImportTarget::numTargets:
        default:
        {
            juce::MidiMessageSequence sequence;

            for (const auto& m : performance.getMessages())
                if (MidiPerformance::isChannelVoiceMessage (m.message))
                    sequence.addEvent (m.message, (double) m.sample);

            sequence.updateMatchedPairs();
            outcome.ok = processor.getSessionRecorder().importMidi (sequence) > 0;
            outcome.message = outcome.ok ? name + " was added to the session." : name + " has nothing to add.";
            break;
        }
    }

    return outcome;
}

MidiImportOutcome MidiImportTargets::importFile (LuthierAudioProcessor& processor, const juce::File& file,
                                                 MidiImportTarget target)
{
    const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    MidiPerformance performance (rate);
    const auto read = MidiProfiles::importFromFile (file, performance, rate);

    if (! read.ok)
    {
        MidiImportOutcome refused;
        refused.read = read;
        refused.message = file.getFileName() + " could not be read: " + read.error;
        return refused;
    }

    auto outcome = importPerformance (processor, performance, target, file.getFileNameWithoutExtension());
    outcome.read = read;

    if (outcome.ok && read.detectedProfile == MidiProfile::luthier)
        outcome.message << " (Luthier profile)";

    if (outcome.ok && ! read.warnings.isEmpty())
        outcome.message << " " << read.warnings.joinIntoString (" ");

    return outcome;
}

} // namespace luthier
