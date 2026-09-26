#include "NormalizationPhrase.h"
#include "../LuthierEngine.h"
#include "../Capture/PerformanceCapture.h"

namespace luthier
{

namespace
{
    // The fixed shape table (4.3): major-triad voicings stacked from the root
    // of each degree, lowest note first. Offsets are semitones from the root.
    constexpr int kShapeI[]  = { 0,  7, 12, 16, 19, 24 };
    constexpr int kShapeIV[] = { 5, 12, 17, 21, 24, 29 };
    constexpr int kShapeV[]  = { 7, 14, 19, 23, 26, 31 };

    void addChord (juce::MidiMessageSequence& seq, const int* shape, int size, int root,
                   double start, double end, juce::uint8 velocity)
    {
        for (int i = 0; i < size; ++i)
        {
            // 20 ms down-strum spread across the chord.
            const double t = start + 0.020 * (double) i / (double) juce::jmax (1, size - 1);
            const int note = juce::jlimit (0, 127, root + shape[i]);
            seq.addEvent (juce::MidiMessage::noteOn (1, note, velocity), t);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), end);
        }
    }
}

juce::MidiMessageSequence NormalizationPhrase::build (bool bassFamily, int rootNote, int numStrings)
{
    juce::MidiMessageSequence seq;
    const int chordSize = juce::jlimit (1, 6, numStrings);
    const double eighth = 60.0 / 120.0 / 2.0;   // 120 BPM eighth notes

    if (bassFamily)
    {
        // 0-4 s: root-fifth-octave eighth-note line.
        constexpr int line[] = { 0, 7, 12, 7 };

        for (int i = 0; i < 16; ++i)
        {
            const double t = eighth * i;
            const int note = juce::jlimit (0, 127, rootNote + line[i % 4]);
            const auto velocity = (juce::uint8) ((i % 2) == 0 ? 80 : 100);
            seq.addEvent (juce::MidiMessage::noteOn (1, note, velocity), t);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), t + eighth * 0.9);
        }

        // 4-5 s: a held root and octave double stop, ringing to the window end.
        seq.addEvent (juce::MidiMessage::noteOn (1, rootNote, (juce::uint8) 100), 4.0);
        seq.addEvent (juce::MidiMessage::noteOn (1, juce::jlimit (0, 127, rootNote + 12), (juce::uint8) 100), 4.0);
        seq.addEvent (juce::MidiMessage::noteOff (1, rootNote), kWindowSeconds - 0.01);
        seq.addEvent (juce::MidiMessage::noteOff (1, juce::jlimit (0, 127, rootNote + 12)), kWindowSeconds - 0.01);
    }
    else
    {
        // 0-2 s: I-IV-V-I, 0.5 s each.
        const int* shapes[] = { kShapeI, kShapeIV, kShapeV, kShapeI };

        for (int c = 0; c < 4; ++c)
            addChord (seq, shapes[c], chordSize, rootNote, 0.5 * c, 0.5 * (c + 1) - 0.005, 90);

        // 2-4 s: eight eighth notes on the middle strings (D-string octave region).
        constexpr int melody[] = { 10, 12, 14, 15, 17, 15, 14, 12 };

        for (int i = 0; i < 8; ++i)
        {
            const double t = 2.0 + eighth * i;
            const int note = juce::jlimit (0, 127, rootNote + melody[i]);
            const auto velocity = (juce::uint8) ((i % 2) == 0 ? 70 : 100);
            seq.addEvent (juce::MidiMessage::noteOn (1, note, velocity), t);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), t + eighth * 0.9);
        }

        // 4-5 s (and ringing on): one chord at velocity 100.
        addChord (seq, kShapeI, chordSize, rootNote, 4.0, kWindowSeconds - 0.01, 100);
    }

    seq.sort();
    seq.updateMatchedPairs();
    return seq;
}

int NormalizationPhrase::rootNoteFor (LuthierEngine& engine)
{
    const int strings = juce::jmax (1, engine.getNumStrings());
    const auto open = PerformanceCapture::getOpenNotes (engine.getTuningEngine(), strings);

    int lowest = 127;

    for (int s = 0; s < juce::jmin (strings, (int) open.size()); ++s)
        if (open[(size_t) s] > 0)
            lowest = juce::jmin (lowest, open[(size_t) s]);

    if (lowest == 127)
        lowest = 40;

    return juce::jlimit (0, 100, lowest + juce::jmax (0, engine.getTuningEngine().getCapoFret()));
}

juce::MidiBuffer NormalizationPhrase::buildBuffer (bool bassFamily, int rootNote, int numStrings, double sampleRate)
{
    const auto seq = build (bassFamily, rootNote, numStrings);
    juce::MidiBuffer buffer;

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        const auto& m = seq.getEventPointer (i)->message;
        buffer.addEvent (m, juce::roundToInt (m.getTimeStamp() * sampleRate));
    }

    return buffer;
}

} // namespace luthier
