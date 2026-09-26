#include "ChordNaming.h"
#include "../Rhythm/ChordDetector.h"
#include "../Tune/TuneTheory.h"

#include <algorithm>
#include <array>

namespace luthier::ChordNaming
{

juce::String spell (int pitchClass, Spelling spelling)
{
    const int pc = ((pitchClass % 12) + 12) % 12;

    if (spelling == Spelling::sharpsExceptBbEb)
        return (pc == 10 || pc == 3) ? tunetheory::spellPitchClass (pc, true)
                                     : tunetheory::spellPitchClass (pc, false);

    return tunetheory::spellPitchClass (pc, spelling == Spelling::flats);
}

juce::String nameFor (const int* midiNotes, int numNotes, Spelling spelling)
{
    if (midiNotes == nullptr || numNotes <= 0)
        return {};

    // The pitch classes, in the order their lowest notes sound (bass first).
    std::array<int, 128> sorted {};
    int count = 0;

    for (int i = 0; i < numNotes && count < 128; ++i)
        if (juce::isPositiveAndBelow (midiNotes[i], 128))
            sorted[(size_t) count++] = midiNotes[i];

    if (count == 0)
        return {};

    std::sort (sorted.begin(), sorted.begin() + count);

    juce::Array<int> classes;

    for (int i = 0; i < count; ++i)
        classes.addIfNotAlreadyThere (sorted[(size_t) i] % 12);

    auto listed = [&classes, spelling]
    {
        juce::StringArray names;

        for (int pc : classes)
            names.add (spell (pc, spelling));

        return names.joinIntoString (" ");
    };

    if (classes.size() == 1)
        return spell (classes[0], spelling);

    if (classes.size() == 2)
    {
        const int up = ((classes[1] - classes[0]) % 12 + 12) % 12;

        if (up == 7)
            return spell (classes[0], spelling) + "5";

        if (up == 5)   // the fifth under the root: still a power chord, on its fifth
            return spell (classes[1], spelling) + "5/" + spell (classes[0], spelling);

        return listed();
    }

    ChordDetector detector;
    const auto symbol = detector.detect (sorted.data(), count);

    if (! symbol.isKnown() || symbol.confidence < ChordDetector::kConfidenceFloor)
        return listed();

    auto name = spell (symbol.root, spelling) + getChordTemplate (symbol.templateIndex).suffix;

    if (symbol.isSlash())
        name << "/" << spell (symbol.bass, spelling);

    return name;
}

} // namespace luthier::ChordNaming
