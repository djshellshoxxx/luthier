#include "JamMidiExport.h"
#include "JamStyle.h"
#include "../DSP/Jam/JamDrumKit.h"

namespace luthier
{

constexpr int JamMidiExport::kDragChoices[];

juce::MidiFile JamMidiExport::build (const std::vector<JamCaptureEvent>& captured, const JamMidiExportOptions& options)
{
    // Only the latest run of the band: a restart puts its clock back to bar 1.
    std::vector<JamCaptureEvent> all;
    {
        size_t first = 0;

        for (size_t i = 1; i < captured.size(); ++i)
            if (captured[i].ppq < captured[i - 1].ppq - 4.0 * juce::jmax (1, captured[i].barLengthQuarters))
                first = i;

        all.assign (captured.begin() + (std::ptrdiff_t) first, captured.end());
    }

    juce::MidiFile file;
    file.setTicksPerQuarterNote (options.ticksPerQuarter);

    // The last N bars: counted back from the last event's bar.
    std::vector<JamCaptureEvent> events;

    if (! all.empty())
    {
        double lastPpq = all.front().ppq;

        for (const auto& e : all)
            lastPpq = juce::jmax (lastPpq, e.ppq);

        const double barLength = (double) juce::jmax (1, all.back().barLengthQuarters);
        const double lastBar = std::floor (lastPpq / barLength + 1.0e-9) * barLength;
        const double from = options.bars > 0 ? lastBar - (options.bars - 1) * barLength : -1.0e300;

        for (const auto& e : all)
            if (e.ppq >= from - 1.0e-9)
                events.push_back (e);
    }

    juce::MidiMessageSequence drums, bass;
    drums.addEvent (juce::MidiMessage::textMetaEvent (3, "Jam Drums"), 0.0);
    bass.addEvent (juce::MidiMessage::textMetaEvent (3, "Jam Bass"), 0.0);

    if (events.empty())
    {
        drums.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);
        file.addTrack (drums);
        file.addTrack (bass);
        return file;
    }

    double start = events.front().ppq;

    for (const auto& e : events)
        start = juce::jmin (start, e.ppq);

    const double barLength = (double) juce::jmax (1, events.front().barLengthQuarters);
    start = std::floor (start / barLength + 1.0e-9) * barLength;

    auto tickOf = [&] (double ppq) { return juce::jmax (0.0, std::round ((ppq - start) * options.ticksPerQuarter)); };

    // The tempo map and meter, in the first track.
    double lastBpm = -1.0;
    int lastBar = -1;

    for (const auto& e : events)
    {
        if (std::abs (e.bpm - lastBpm) > 1.0e-6)
        {
            lastBpm = e.bpm;
            drums.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::llround (60000000.0 / juce::jmax (1.0, e.bpm))), tickOf (e.ppq));
        }

        if (e.barLengthQuarters != lastBar)
        {
            lastBar = e.barLengthQuarters;
            drums.addEvent (juce::MidiMessage::timeSignatureMetaEvent (juce::jmax (1, e.barLengthQuarters), 4), tickOf (e.ppq));
        }
    }

    for (const auto& e : events)
    {
        const double tick = tickOf (e.ppq);

        if (e.part == 2)
        {
            if (options.luthierProfile)
            {
                // The Luthier profile's text meta at each change (9).
                const auto text = juce::String ("LUTHIER: JAM style=") + JamStyleLibrary::getStyleChoiceNames()[juce::jlimit (0, 10, (int) e.style)]
                                  + " variation=" + (e.variation == 0 ? "A" : "B")
                                  + " intensity=" + juce::String (e.intensity)
                                  + " kit=" + getJamKitName (e.kit);
                drums.addEvent (juce::MidiMessage::textMetaEvent (1, text), tick);
            }

            continue;
        }

        auto& sequence = e.part == 0 ? drums : bass;
        const int channel = e.part == 0 ? options.drumChannel : options.bassChannel;

        if (e.velocity > 0)
            sequence.addEvent (juce::MidiMessage::noteOn (channel, e.note, (juce::uint8) e.velocity), tick);
        else
            sequence.addEvent (juce::MidiMessage::noteOff (channel, e.note), tick);
    }

    drums.updateMatchedPairs();
    bass.updateMatchedPairs();
    file.addTrack (drums);
    file.addTrack (bass);
    return file;
}

juce::String JamMidiExport::suggestedName (const JamMidiExportOptions& options)
{
    return options.bars > 0 ? "Jam - last " + juce::String (options.bars) + " bars.mid" : juce::String ("Jam - all.mid");
}

juce::File JamMidiExport::write (const JamCapture& capture, const JamMidiExportOptions& options, const juce::File& destination)
{
    const auto events = capture.copyLastBars (0);

    if (events.empty())
        return {};

    const auto file = destination != juce::File()
                        ? destination
                        : juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (suggestedName (options));

    const auto midi = build (events, options);
    file.deleteFile();

    if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
    {
        if (midi.writeTo (*stream, 1))
        {
            stream->flush();
            return file;
        }
    }

    return {};
}

} // namespace luthier
