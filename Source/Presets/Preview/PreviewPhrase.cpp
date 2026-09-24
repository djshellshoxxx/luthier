#include "PreviewPhrase.h"

namespace luthier
{

namespace
{
    constexpr int kGuitarReferenceRoot = 40;   // E2, standard tuning's low string
    constexpr int kBassReferenceRoot   = 28;   // E1

    struct IdName { PreviewPhrase::Id id; const char* name; double bpm; };

    constexpr IdName kIds[] = {
        { PreviewPhrase::Id::acoustic_strum, "acoustic_strum", 104.0 },
        { PreviewPhrase::Id::fingerstyle,    "fingerstyle",     96.0 },
        { PreviewPhrase::Id::nylon_comp,     "nylon_comp",     110.0 },
        { PreviewPhrase::Id::rasgueado,      "rasgueado",      120.0 },
        { PreviewPhrase::Id::clean_arp,      "clean_arp",       90.0 },
        { PreviewPhrase::Id::crunch_riff,    "crunch_riff",    112.0 },
        { PreviewPhrase::Id::highgain_riff,  "highgain_riff",  120.0 },
        { PreviewPhrase::Id::lead_lick,      "lead_lick",      100.0 },
        { PreviewPhrase::Id::jazz_comp,      "jazz_comp",      120.0 },
        { PreviewPhrase::Id::funk_chop,      "funk_chop",      100.0 },
        { PreviewPhrase::Id::twang_lick,     "twang_lick",     110.0 },
        { PreviewPhrase::Id::slide_lick,     "slide_lick",      84.0 },
        { PreviewPhrase::Id::ambient_swell,  "ambient_swell",   70.0 },
        { PreviewPhrase::Id::bass_groove,    "bass_groove",    100.0 },
        { PreviewPhrase::Id::bass_slap,      "bass_slap",      100.0 },
        { PreviewPhrase::Id::bass_walk,      "bass_walk",      120.0 },
    };

    /** The whole words of the category, tags and name, lower-cased. */
    juce::StringArray wordsOf (const PreviewPhrase::Context& c)
    {
        juce::String text = c.category + " " + c.name + " " + c.tags.joinIntoString (" ");
        juce::StringArray words;

        juce::String current;

        for (auto ch : text.toLowerCase())
        {
            if (juce::CharacterFunctions::isLetterOrDigit (ch))
            {
                current += juce::String::charToString (ch);
            }
            else if (current.isNotEmpty())
            {
                words.add (current);
                current.clear();
            }
        }

        if (current.isNotEmpty())
            words.add (current);

        return words;
    }

    bool anyOf (const juce::StringArray& words, std::initializer_list<const char*> wanted)
    {
        for (auto* w : wanted)
            if (words.contains (w))
                return true;

        return false;
    }

    //==========================================================================
    /** One note or chord of a phrase, in beats, at reference pitch. */
    struct Hit
    {
        double beat;
        double lengthBeats;
        std::vector<int> notes;
        int velocity;
        double strumMs = 0.0;   ///< spread between strings; negative is an up-stroke
        int bendCents = 0;      ///< a bend to this many cents over the note's first half
        bool vibrato = false;   ///< a vibrato over the note's second half
    };

    std::vector<Hit> hitsFor (PreviewPhrase::Id id, bool rhythmEngine)
    {
        using Id = PreviewPhrase::Id;

        const std::vector<int> G      { 43, 47, 50, 55, 59, 67 };
        const std::vector<int> Cadd9  { 48, 52, 55, 62, 67 };
        const std::vector<int> D      { 50, 57, 62, 66 };
        const std::vector<int> Emaj   { 40, 47, 52, 56, 59, 64 };
        const std::vector<int> Amaj   { 45, 52, 57, 61, 64 };

        if (rhythmEngine)
        {
            // 3.1: two held chords; the preset's own pattern plays them.
            if (id == Id::bass_groove || id == Id::bass_slap || id == Id::bass_walk)
                return { { 0.0, 2.6, { 28, 35 }, 96 }, { 2.66, 2.6, { 33, 40 }, 96 } };

            return { { 0.0, 2.6, Emaj, 90 }, { 2.66, 2.6, Amaj, 90 } };
        }

        switch (id)
        {
            case Id::acoustic_strum:
                // D - DU - UDU over G, then Cadd9, then D to finish.
                return { { 0.0, 1.0, G, 100, 14.0 },  { 1.0, 0.5, G, 92, 12.0 }, { 1.5, 0.5, G, 76, -10.0 },
                         { 2.5, 0.5, Cadd9, 78, -10.0 }, { 3.0, 0.5, Cadd9, 94, 12.0 }, { 3.5, 0.5, Cadd9, 74, -10.0 },
                         { 4.0, 1.5, D, 102, 14.0 } };

            case Id::fingerstyle:
                // Travis: alternating bass under a pinched melody, C to G/B.
                return { { 0.0, 1.0, { 48, 64 }, 90 }, { 0.5, 0.5, { 60 }, 70 }, { 1.0, 1.0, { 52 }, 80 },
                         { 1.5, 0.5, { 62 }, 72 }, { 2.0, 1.0, { 47, 62 }, 90 }, { 2.5, 0.5, { 59 }, 70 },
                         { 3.0, 1.0, { 50 }, 80 }, { 3.5, 0.5, { 67 }, 76 }, { 4.0, 1.0, { 47, 55, 62 }, 88 } };

            case Id::nylon_comp:
            {
                const std::vector<int> Am7 { 45, 52, 55, 60, 64 };
                const std::vector<int> D9  { 50, 54, 60, 64 };
                return { { 0.0, 0.9, { 45 }, 88 }, { 0.5, 0.4, { 52, 55, 60, 64 }, 72, 6.0 },
                         { 1.5, 0.4, { 52, 55, 60, 64 }, 70, 6.0 }, { 2.0, 0.9, { 50 }, 88 },
                         { 2.5, 0.4, { 54, 60, 64 }, 72, 6.0 }, { 3.5, 1.4, D9, 76, 8.0 } };
            }

            case Id::rasgueado:
            {
                const std::vector<int> F { 41, 48, 53, 57, 60, 65 };
                return { { 0.0, 0.25, Emaj, 110, 7.0 }, { 0.25, 0.25, Emaj, 96, 7.0 }, { 0.5, 0.25, Emaj, 100, 7.0 },
                         { 0.75, 0.25, Emaj, 104, -6.0 }, { 1.0, 1.0, Emaj, 118, 5.0 },
                         { 2.5, 0.5, { 65 }, 90 }, { 3.0, 0.5, { 64 }, 90 }, { 3.5, 0.5, F, 104, 8.0 },
                         { 4.0, 1.5, Emaj, 112, 6.0 } };
            }

            case Id::clean_arp:
            {
                const std::vector<int> Em7 { 40, 47, 52, 55, 62, 67 };
                std::vector<Hit> h;
                const int arp1[] = { 48, 55, 62, 67, 64, 62 };
                const int arp2[] = { 40, 47, 55, 62, 67, 62 };

                for (int i = 0; i < 6; ++i)
                    h.push_back ({ i * 0.5, 4.8 - i * 0.5, { arp1[i] }, 84 - (i % 2) * 8 });

                for (int i = 0; i < 4; ++i)
                    h.push_back ({ 3.0 + i * 0.3, 1.8 - i * 0.3, { arp2[i] }, 84 - (i % 2) * 8 });

                juce::ignoreUnused (Em7);
                return h;
            }

            case Id::crunch_riff:
                return { { 0.0, 0.45, { 45, 52, 57 }, 110, 5.0 }, { 0.5, 0.45, { 45, 52, 57 }, 100, 5.0 },
                         { 1.0, 0.45, { 48, 55, 60 }, 110, 5.0 }, { 1.5, 0.95, { 50, 57, 62 }, 112, 5.0 },
                         { 2.75, 0.25, { 57 }, 100 }, { 3.0, 0.25, { 60 }, 96 }, { 3.25, 0.25, { 62 }, 100 },
                         { 3.5, 0.25, { 64 }, 98 }, { 3.75, 0.25, { 67 }, 102 }, { 4.0, 1.5, { 45, 52, 57 }, 112, 5.0 } };

            case Id::highgain_riff:
            {
                std::vector<Hit> h;

                // Palm-muted chugs on the lowest string (short, so they choke).
                for (int i = 0; i < 12; ++i)
                    if (i != 5 && i != 11)
                        h.push_back ({ i * 0.25, 0.12, { 40 }, i % 4 == 0 ? 118 : 96 });

                h.push_back ({ 1.25, 0.6, { 40, 47, 52 }, 120, 3.0 });
                h.push_back ({ 3.0, 2.0, { 43, 50, 55 }, 122, 3.0 });
                return h;
            }

            case Id::lead_lick:
                // A minor pentatonic, one whole-step bend, final vibrato.
                return { { 0.0, 0.5, { 69 }, 104 }, { 0.5, 0.5, { 72 }, 100 }, { 1.0, 1.0, { 74 }, 108, 0.0, 200 },
                         { 2.0, 0.5, { 72 }, 100 }, { 2.5, 0.5, { 69 }, 98 }, { 3.0, 0.5, { 67 }, 100 },
                         { 3.5, 1.8, { 69 }, 108, 0.0, 0, true } };

            case Id::jazz_comp:
            {
                // Dm9 G13 Cmaj9 shells, swung (the off-beat two-thirds of a beat late).
                const std::vector<int> Dm9  { 50, 53, 60, 64 };
                const std::vector<int> G13  { 43, 53, 59, 64 };
                const std::vector<int> Cma9 { 48, 52, 59, 62 };
                return { { 0.0, 1.2, Dm9, 80, 8.0 }, { 1.66, 0.3, Dm9, 64, 6.0 }, { 2.0, 1.2, G13, 82, 8.0 },
                         { 3.66, 0.3, G13, 62, 6.0 }, { 4.0, 2.0, Cma9, 84, 10.0 } };
            }

            case Id::funk_chop:
            {
                const std::vector<int> E9 { 52, 56, 62, 66, 71 };
                std::vector<Hit> h;
                const bool pattern[16] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0 };

                for (int i = 0; i < 16; ++i)
                {
                    if (pattern[i])
                        h.push_back ({ i * 0.25, 0.12, E9, i % 4 == 0 ? 108 : 92, i % 2 == 0 ? 3.0 : -3.0 });
                    else
                        h.push_back ({ i * 0.25, 0.05, E9, 26, i % 2 == 0 ? 3.0 : -3.0 });   // a ghost mute
                }

                h.push_back ({ 4.0, 1.2, E9, 104, 4.0 });
                return h;
            }

            case Id::twang_lick:
                // Hybrid-picked sixths and thirds in A, with a pedal answer.
                return { { 0.0, 0.5, { 61, 69 }, 100 }, { 0.5, 0.5, { 62, 71 }, 96 }, { 1.0, 0.5, { 64, 73 }, 104 },
                         { 1.5, 0.5, { 62, 71 }, 96 }, { 2.0, 0.5, { 61, 69 }, 100 }, { 2.5, 0.5, { 64, 68 }, 98 },
                         { 3.0, 0.5, { 61, 64 }, 100 }, { 3.5, 1.8, { 57, 64, 69 }, 106, 4.0 } };

            case Id::slide_lick:
                // Open-G style: overlapping notes glide in slide mode.
                return { { 0.0, 1.1, { 55 }, 100 }, { 1.0, 0.6, { 57 }, 96 }, { 1.5, 0.6, { 59 }, 100 },
                         { 2.0, 1.1, { 62 }, 104 }, { 3.0, 1.6, { 59, 62, 67 }, 104, 8.0, 0, true } };

            case Id::ambient_swell:
                return { { 0.0, 3.6, { 40, 47, 51, 54, 59, 66 }, 58, 40.0 } };

            case Id::bass_groove:
            {
                const int line[] = { 28, 28, 35, 40, 28, 28, 35, 38, 33, 33 };
                std::vector<Hit> h;

                for (int i = 0; i < 10; ++i)
                    h.push_back ({ i * 0.5, 0.42, { line[i] }, i % 2 == 0 ? 104 : 92 });

                return h;
            }

            case Id::bass_slap:
            {
                const int line[] = { 28, 40, 28, 28, 40, 38, 31, 43, 33, 45 };
                std::vector<Hit> h;

                for (int i = 0; i < 10; ++i)
                    h.push_back ({ i * 0.5, 0.3, { line[i] }, line[i] >= 38 ? 112 : 118 });

                return h;
            }

            case Id::bass_walk:
            {
                const int line[] = { 28, 32, 35, 37, 38, 37, 35 };
                std::vector<Hit> h;

                for (int i = 0; i < 7; ++i)
                    h.push_back ({ (double) i, 0.9, { line[i] }, 98 });

                return h;
            }

            case Id::NumIds:
            default:
                break;
        }

        return {};
    }
}

//==============================================================================
const char* PreviewPhrase::getIdString (Id id) noexcept
{
    for (const auto& e : kIds)
        if (e.id == id)
            return e.name;

    return "clean_arp";
}

PreviewPhrase::Id PreviewPhrase::fromString (const juce::String& s, Id fallback) noexcept
{
    const auto t = s.trim().toLowerCase();

    for (const auto& e : kIds)
        if (t == e.name)
            return e.id;

    return fallback;
}

double PreviewPhrase::getBpm (Id id) noexcept
{
    for (const auto& e : kIds)
        if (e.id == id)
            return e.bpm;

    return 100.0;
}

PreviewPhrase::Id PreviewPhrase::choose (const Context& c)
{
    // 1. The preset's own field.
    if (c.previewPhrase.isNotEmpty())
    {
        const auto own = fromString (c.previewPhrase);

        if (own != Id::NumIds)
            return own;
    }

    const auto words = wordsOf (c);

    /*  Bass presets pick among the bass rows only (DECISIONS, FEAT-BROWSER): a
        bass tagged "funk" would otherwise get a guitar's E9 chop an octave down. */
    if (c.bassFamily || words.contains ("bass"))
    {
        if (c.slapArmed || anyOf (words, { "slap", "pop" }))
            return Id::bass_slap;

        if (anyOf (words, { "jazz", "fretless", "walk", "walking" }))
            return Id::bass_walk;

        return Id::bass_groove;
    }

    // 2. The first table row whose words appear.
    const bool fingerWords = anyOf (words, { "fingerstyle", "folk", "parlor", "dadgad", "travis" });

    if (anyOf (words, { "acoustic", "strum", "strummed" }) && ! fingerWords)       return Id::acoustic_strum;
    if (fingerWords)                                                                 return Id::fingerstyle;
    if (anyOf (words, { "classical", "nylon" }) && ! words.contains ("flamenco"))    return Id::nylon_comp;
    if (anyOf (words, { "flamenco", "rasgueado" }))                                  return Id::rasgueado;
    if (anyOf (words, { "metal", "djent" }))                                         return Id::highgain_riff;
    if (anyOf (words, { "lead", "shred", "solo" }))                                  return Id::lead_lick;
    if (words.contains ("jazz"))                                                     return Id::jazz_comp;
    if (anyOf (words, { "funk", "wah", "reggae" }))                                  return Id::funk_chop;
    if (anyOf (words, { "country", "twang", "rockabilly", "surf" }))                 return Id::twang_lick;
    if (c.slide || words.contains ("slide"))                                         return Id::slide_lick;
    if (anyOf (words, { "ambient", "swell", "shoegaze" }))                           return Id::ambient_swell;

    // 3. By features.
    if (c.drive > 0.6)   return Id::highgain_riff;
    if (c.drive > 0.3)   return Id::crunch_riff;
    if (c.nylon)         return Id::nylon_comp;
    if (c.acousticBody)  return Id::acoustic_strum;

    return Id::clean_arp;
}

PreviewPhrase::Built PreviewPhrase::build (Id id, const Context& c)
{
    Built out;
    out.id = id;

    const bool bassPhrase = id == Id::bass_groove || id == Id::bass_slap || id == Id::bass_walk;
    const bool rhythm = c.rhythmEngineOn;

    // The rhythm engine's pattern plays the held chords at 100 BPM (3.1).
    out.bpm = rhythm ? 100.0 : getBpm (id);

    auto hits = hitsFor (id, rhythm);

    // Pitch follows the guitar: the reference root lands on the lowest open string.
    int shift = c.lowestOpenMidi - (bassPhrase ? kBassReferenceRoot : kGuitarReferenceRoot);

    if (id == Id::lead_lick || id == Id::twang_lick)
    {
        // Lead and twang are raised whole octaves until they fit (3.1).
        int lowest = 127;

        for (const auto& h : hits)
            for (int n : h.notes)
                lowest = juce::jmin (lowest, n);

        shift = 0;

        while (lowest + shift < c.lowestOpenMidi + 5)
            shift += 12;

        int highest = 0;

        for (const auto& h : hits)
            for (int n : h.notes)
                highest = juce::jmax (highest, n);

        while (highest + shift > c.highestMidi && lowest + shift - 12 >= c.lowestOpenMidi)
            shift -= 12;
    }

    const double secondsPerBeat = 60.0 / out.bpm;
    constexpr int channel = 1;

    for (const auto& h : hits)
    {
        const double start = h.beat * secondsPerBeat;
        const double end = juce::jmin (kMaxNoteSeconds, (h.beat + h.lengthBeats) * secondsPerBeat);

        if (start >= end)
            continue;

        const int count = (int) h.notes.size();

        for (int i = 0; i < count; ++i)
        {
            // A down-stroke plays low to high, an up-stroke high to low.
            const int order = h.strumMs >= 0.0 ? i : count - 1 - i;
            const double offset = std::abs (h.strumMs) * 0.001 * order;
            const int note = juce::jlimit (0, 127, h.notes[(size_t) i] + shift);
            const double on = juce::jmin (start + offset, end - 0.01);

            out.sequence.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, h.velocity)), on);
            out.sequence.addEvent (juce::MidiMessage::noteOff (channel, note), end);
            out.lowestNote = juce::jmin (out.lowestNote, note);
        }

        // A bend rises over the note's first half and holds; the wheel centres
        // at the note-off. Cents over the engine's default two-semitone range.
        if (h.bendCents != 0)
        {
            constexpr int steps = 12;

            for (int s = 1; s <= steps; ++s)
            {
                const double t = start + (end - start) * 0.5 * s / steps;
                const double amount = (double) h.bendCents / 200.0 * s / steps;
                out.sequence.addEvent (juce::MidiMessage::pitchWheel (channel, juce::jlimit (0, 16383, 8192 + (int) (amount * 8191.0))), t);
            }

            out.sequence.addEvent (juce::MidiMessage::pitchWheel (channel, 8192), end);
        }

        if (h.vibrato)
        {
            // Five cycles a second, a quarter-tone wide, over the second half.
            const double from = start + (end - start) * 0.4;

            for (double t = from; t < end; t += 0.02)
            {
                const double v = 0.25 * std::sin (juce::MathConstants<double>::twoPi * 5.0 * (t - from));
                out.sequence.addEvent (juce::MidiMessage::pitchWheel (channel, juce::jlimit (0, 16383, 8192 + (int) (v / 2.0 * 8191.0))), t);
            }

            out.sequence.addEvent (juce::MidiMessage::pitchWheel (channel, 8192), end);
        }

        out.noteEndSeconds = juce::jmax (out.noteEndSeconds, end);
    }

    out.sequence.sort();
    out.sequence.updateMatchedPairs();

    if (out.lowestNote == 127)
        out.lowestNote = c.lowestOpenMidi;

    return out;
}

} // namespace luthier
