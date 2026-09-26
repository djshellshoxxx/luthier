#include "PianoRollModel.h"
#include "../Model/Playing/TuningEngine.h"

namespace luthier
{

juce::Colour StringColours::forString (int stringIndex, int numStrings)
{
    // Hues spread over the wheel, the low string warm and the high one cool,
    // at a saturation and brightness that read on dark, light and contrast.
    const int n = juce::jmax (1, numStrings);
    const float t = (float) juce::jlimit (0, n - 1, n - 1 - stringIndex) / (float) juce::jmax (1, n - 1);
    return juce::Colour::fromHSV (0.02f + 0.72f * t, 0.72f, 0.92f, 1.0f);
}

PianoRollModel::Range PianoRollModel::rangeFor (const TuningEngine& tuning, int numStrings)
{
    Range r;
    const double a = tuning.getConcertA() > 1.0 ? tuning.getConcertA() : 440.0;
    int low = 127, high = 0;

    for (int s = 0; s < juce::jmax (1, numStrings); ++s)
    {
        const double hz = tuning.getEffectiveOpenFrequency (s);   // the capo'd open note

        if (hz <= 1.0)
            continue;

        const int open = (int) std::lround (69.0 + 12.0 * std::log2 (hz / a));
        const int atNut = open - tuning.getCapoFretFor (s);
        low = juce::jmin (low, open);
        high = juce::jmax (high, atNut + tuning.getStringTuning (s).maxFrets);
    }

    if (low > high)
        return r;

    r.lowestPlayable = juce::jlimit (0, 127, low);
    r.highestPlayable = juce::jlimit (r.lowestPlayable, 127, high);
    r.drawLow = r.lowestPlayable - r.lowestPlayable % 12;
    r.drawHigh = juce::jmin (127, r.highestPlayable + (11 - r.highestPlayable % 12));
    return r;
}

float PianoRollModel::keyAlpha (int note, double nowMs) const noexcept
{
    const auto& k = getKey (note);

    if (k.string >= 0)
        return kLitAlpha;

    if (k.releasedMs < 0.0)
        return 0.0f;

    const double t = (nowMs - k.releasedMs) / kReleaseMs;
    return t >= 1.0 ? 0.0f : kLitAlpha * (float) (1.0 - t);
}

int PianoRollModel::countLit (double nowMs) const noexcept
{
    int n = 0;

    for (int note = 0; note < 128; ++note)
        n += isLit (note, nowMs) ? 1 : 0;

    return n;
}

void PianoRollModel::update (double nowMs, const SoundingNotes::Frame& frame, bool stale)
{
    if (! initialised)
    {
        openBar.fill (-1);
        barStart.fill (-1);
        initialised = true;
    }

    if (frame.numStrings > 0)
        numStrings = frame.numStrings;

    // Which string sounds each note now (the lowest index wins a shared note).
    std::array<int, 128> soundingBy;
    soundingBy.fill (-1);

    for (int s = 0; s < SoundingNotes::kMaxStrings; ++s)
    {
        const auto& str = frame.strings[(size_t) s];
        const int note = (! stale && s < frame.numStrings) ? str.note : -1;

        if (juce::isPositiveAndBelow (note, 128) && soundingBy[(size_t) note] < 0)
            soundingBy[(size_t) note] = s;

        // The roll: a bar per note, from its start to its note-off.
        auto& open = openBar[(size_t) s];
        const bool sameNote = open >= 0 && bars[(size_t) open].note == note && barStart[(size_t) s] == str.startSample;

        if (open >= 0 && ! sameNote)
        {
            bars[(size_t) open].endMs = nowMs;
            open = -1;
        }

        if (note >= 0 && open < 0)
        {
            bars.push_back ({ note, s, nowMs, -1.0, false });
            open = (int) bars.size() - 1;
            barStart[(size_t) s] = str.startSample;
        }

        if (note >= 0 && open >= 0 && std::abs (str.bendCents) > kBendTickCents)
            bars[(size_t) open].bent = true;
    }

    for (int note = 0; note < 128; ++note)
    {
        auto& k = keys[(size_t) note];
        const int s = soundingBy[(size_t) note];

        if (s >= 0)
        {
            k.string = s;
            k.colour = StringColours::forString (s, numStrings);
            k.releasedMs = -1.0;
        }
        else if (k.string >= 0)
        {
            k.string = -1;
            k.releasedMs = nowMs;   // fades over 150 ms
        }
    }

    // Four seconds of history; the indices of open bars move with the erase.
    const double cutoff = nowMs - kRollSeconds * 1000.0;
    int dropped = 0;

    while (dropped < (int) bars.size() && bars[(size_t) dropped].endMs >= 0.0 && bars[(size_t) dropped].endMs < cutoff)
        ++dropped;

    if (dropped > 0)
    {
        bars.erase (bars.begin(), bars.begin() + dropped);

        for (auto& o : openBar)
            if (o >= 0)
                o -= dropped;
    }
}

} // namespace luthier
