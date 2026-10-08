#include "GuitarProLegacyReader.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace luthier
{
namespace
{
    constexpr int kMaxFretLocal = 36;

    //==========================================================================
    /** A bounded little-endian reader. Overruns set `bad` and return zero, so a
        truncated or hostile file ends the parse rather than the process. */
    struct Bytes
    {
        const juce::uint8* data = nullptr;
        size_t size = 0;
        size_t pos = 0;
        bool bad = false;

        size_t remaining() const noexcept { return pos <= size ? size - pos : 0; }

        bool need (size_t n) noexcept
        {
            if (bad || remaining() < n)
            {
                bad = true;
                return false;
            }
            return true;
        }

        void skip (size_t n) noexcept { if (need (n)) pos += n; }

        int u8() noexcept { return need (1) ? data[pos++] : 0; }
        int i8() noexcept { return (int) (juce::int8) u8(); }
        bool boolean() noexcept { return u8() != 0; }

        int i16() noexcept
        {
            if (! need (2)) return 0;
            const int v = data[pos] | (data[pos + 1] << 8);
            pos += 2;
            return (juce::int16) v;
        }

        int i32() noexcept
        {
            if (! need (4)) return 0;
            const juce::uint32 v = (juce::uint32) data[pos] | ((juce::uint32) data[pos + 1] << 8)
                                 | ((juce::uint32) data[pos + 2] << 16) | ((juce::uint32) data[pos + 3] << 24);
            pos += 4;
            return (int) v;
        }

        juce::String latin1 (size_t n)
        {
            if (! need (n)) return {};
            juce::String s;
            for (size_t i = 0; i < n; ++i)
                s += (juce::juce_wchar) data[pos + i];
            pos += n;
            return s;
        }

        /** Length byte, then `field` bytes of which the first `length` count. */
        juce::String byteSizeString (int field)
        {
            const int length = u8();
            if (field < 0 || field > 255 || ! need ((size_t) field))
            {
                bad = true;
                return {};
            }
            auto s = latin1 ((size_t) field);
            return s.substring (0, juce::jmin (length, field));
        }

        juce::String intSizeString()
        {
            const int count = i32();
            if (count < 0 || (size_t) count > remaining())
            {
                bad = true;
                return {};
            }
            return latin1 ((size_t) count);
        }

        juce::String intByteSizeString()
        {
            const int count = i32();
            if (count <= 0)
                return {};
            return byteSizeString (count - 1);
        }
    };

    //==========================================================================
    struct GBend
    {
        std::vector<std::pair<int, int>> points;   // (position 0..12, value in 1/25 semitone)
    };

    struct GNote
    {
        int string = 1;            // 1 is the highest
        int fret = 0;
        int type = 1;              // 1 normal, 2 tie, 3 dead
        int velocity = 95;
        bool ghost = false, accent = false, vibrato = false, palmMute = false,
             letRing = false, staccato = false, hammer = false, trill = false;
        int harmonic = 0;          // 1 natural, 2 artificial, 3 tapped, 4 pinch
        int slide = 0;             // bit0 shift, 1 legato, 2 out down, 3 out up, 4 in below, 5 in above
        int slapKind = 0;          // 1 slap, 2 pop, 3 tap
        GBend bend;
        bool hasBend = false;
    };

    struct GBeat
    {
        bool empty = false;
        double length = 1.0;
        juce::String chord;
        std::vector<GNote> notes;
    };

    struct GHeader
    {
        int numerator = 4, denominator = 4;
        bool repeatOpen = false;
        int repeatClose = 0;
        int alternative = 0;
        double beats() const noexcept { return numerator * 4.0 / juce::jmax (1, denominator); }
    };

    struct GTrack
    {
        juce::String name;
        int numStrings = 6;
        int tuning[7] = {};
        int capo = 0;
        bool percussion = false;
        int channel = 0;
        std::vector<std::vector<std::vector<GBeat>>> measures;   // [measure][voice][beat]
        int totalNotes = 0;
    };

    struct Parser
    {
        Bytes in;
        int major = 3, minor = 0;
        bool v50 = false;       // exactly 5.0.0 (differs from 5.10)
        std::vector<GHeader> headers;
        std::vector<GTrack> tracks;
        std::vector<std::vector<int>> lastFret;   // per track, per string
        juce::String title, artist;
        int tempo = 120;
        juce::StringArray warnings;

        bool gp3() const noexcept { return major == 3; }
        bool gp5() const noexcept { return major >= 5; }

        //----------------------------------------------------------------------
        bool parseVersion()
        {
            const auto text = in.byteSizeString (30);
            if (in.bad || ! text.startsWith ("FICHIER GUITAR PRO"))
                return false;
            const auto v = text.fromFirstOccurrenceOf ("v", false, false);
            major = v.upToFirstOccurrenceOf (".", false, false).getIntValue();
            minor = v.fromFirstOccurrenceOf (".", false, false).getIntValue();
            v50 = (major == 5 && minor == 0);
            return major >= 3 && major <= 5;
        }

        void readInfo()
        {
            title = in.intByteSizeString();
            in.intByteSizeString();                 // subtitle
            artist = in.intByteSizeString();
            in.intByteSizeString();                 // album
            in.intByteSizeString();                 // words
            if (gp5()) in.intByteSizeString();      // music
            in.intByteSizeString();                 // copyright
            in.intByteSizeString();                 // tabbed by
            in.intByteSizeString();                 // instructions
            const int notices = in.i32();
            if (notices < 0 || (size_t) notices > in.remaining()) { in.bad = true; return; }
            for (int i = 0; i < notices && ! in.bad; ++i)
                in.intByteSizeString();
        }

        void readLyrics()
        {
            in.i32();
            for (int i = 0; i < 5 && ! in.bad; ++i)
            {
                in.i32();
                in.intSizeString();
            }
        }

        void readPageSetup()
        {
            in.skip (4 * 2 + 4 * 4 + 4 + 2);
            for (int i = 0; i < 10 && ! in.bad; ++i)
                in.intByteSizeString();
        }

        void readMidiChannels() { in.skip (64 * 12); }

        //----------------------------------------------------------------------
        void readMeasureHeaders (int count)
        {
            for (int n = 0; n < count && ! in.bad; ++n)
            {
                GHeader h;
                if (! headers.empty())
                    h = headers.back(), h.repeatOpen = false, h.repeatClose = 0, h.alternative = 0;

                if (gp5() && n > 0)
                    in.skip (1);

                const int flags = in.u8();
                if (flags & 0x01) h.numerator = in.i8();
                if (flags & 0x02) h.denominator = in.i8();
                h.repeatOpen = (flags & 0x04) != 0;

                if (flags & 0x08)
                {
                    h.repeatClose = in.i8();
                    if (gp5()) --h.repeatClose;
                }

                auto readAlternative = [&]
                {
                    const int value = juce::jlimit (0, 30, in.u8());
                    int existing = 0;
                    for (auto it = headers.rbegin(); it != headers.rend(); ++it)
                    {
                        if (it->repeatOpen) break;
                        existing |= it->alternative;
                    }
                    h.alternative = (((1 << value) - 1) ^ existing);
                };

                if (! gp5() && (flags & 0x10)) readAlternative();
                if (flags & 0x20) { in.intByteSizeString(); in.skip (4); }   // marker + colour
                if (flags & 0x40) in.skip (2);                               // key change
                if (gp5())
                {
                    if (flags & 0x10) readAlternative();
                    if (flags & 0x03) in.skip (4);                           // beam groups
                    if ((flags & 0x10) == 0) in.skip (1);
                    in.skip (1);                                             // triplet feel
                }

                if (h.numerator < 1 || h.numerator > 64 || h.denominator < 1 || h.denominator > 64)
                {
                    h.numerator = 4;
                    h.denominator = 4;
                }

                headers.push_back (h);
            }
        }

        //----------------------------------------------------------------------
        void readTracks (int count)
        {
            for (int t = 0; t < count && ! in.bad; ++t)
            {
                GTrack track;
                if (gp5() && (t == 0 || v50))
                    in.skip (1);

                const int flags = in.u8();
                track.percussion = (flags & 1) != 0;
                track.name = in.byteSizeString (40);
                const int strings = in.i32();
                for (int i = 0; i < 7; ++i)
                    track.tuning[i] = in.i32();

                if (strings < 1 || strings > 7)
                {
                    warnings.add ("Track " + juce::String (t + 1) + " has an impossible string count ("
                                  + juce::String (strings) + ").");
                    in.bad = true;
                    return;
                }
                track.numStrings = strings;

                in.i32();                        // port
                const int channel = in.i32();
                in.i32();                        // effect channel
                track.channel = channel - 1;
                if (track.channel % 16 == 9)
                    track.percussion = true;
                in.i32();                        // fret count
                track.capo = juce::jlimit (0, 24, in.i32());
                in.skip (4);                     // colour

                if (gp5())
                {
                    in.i16();                    // settings flags
                    in.skip (3);                 // accentuation, bank, humanize
                    in.skip (12);                // clef transpose x3
                    in.skip (12);
                    in.i32(); in.i32(); in.i32();  // RSE instrument
                    if (v50) { in.i16(); in.skip (1); } else in.i32();
                    if (! v50)
                    {
                        in.skip (4);             // equalizer
                        in.intByteSizeString();
                        in.intByteSizeString();
                    }
                }

                tracks.push_back (std::move (track));
            }

            if (gp5() && ! in.bad)
                in.skip (v50 ? 2 : 1);
        }

        //----------------------------------------------------------------------
        void readBend (GNote& note)
        {
            in.i8();                // type
            in.i32();               // overall value
            const int count = in.i32();
            if (count < 0 || (size_t) count * 9 > in.remaining()) { in.bad = true; return; }
            GBend b;
            for (int i = 0; i < count && ! in.bad; ++i)
            {
                const int position = in.i32();
                const int value = in.i32();
                in.boolean();
                b.points.emplace_back (juce::jlimit (0, 12, position), value);
            }
            if (count > 0)
            {
                note.bend = std::move (b);
                note.hasBend = true;
            }
        }

        void readGrace()
        {
            in.skip (gp5() ? 6 : 5);
        }

        void readNoteEffects (GNote& note)
        {
            if (gp3())
            {
                const int flags = in.u8();
                note.hammer = (flags & 2) != 0;
                note.letRing = (flags & 8) != 0;
                if (flags & 1) readBend (note);
                if (flags & 16) readGrace();
                if (flags & 4) note.slide |= 1;
                return;
            }

            const int flags1 = in.u8();
            const int flags2 = in.u8();
            note.hammer = (flags1 & 2) != 0;
            note.letRing = (flags1 & 8) != 0;
            note.staccato = (flags2 & 1) != 0;
            note.palmMute = (flags2 & 2) != 0;
            note.vibrato = note.vibrato || (flags2 & 64) != 0;

            if (flags1 & 1) readBend (note);
            if (flags1 & 16) readGrace();
            if (flags2 & 4) in.i8();                       // tremolo picking
            if (flags2 & 8)
            {
                if (gp5())
                {
                    const int s = in.u8();
                    if (s & 1) note.slide |= 1;
                    if (s & 2) note.slide |= 2;
                    if (s & 4) note.slide |= 4;
                    if (s & 8) note.slide |= 8;
                    if (s & 16) note.slide |= 16;
                    if (s & 32) note.slide |= 32;
                }
                else
                {
                    const int s = in.i8();
                    if (s == 1) note.slide |= 1;
                    else if (s == 2) note.slide |= 2;
                    else if (s == 3) note.slide |= 4;
                    else if (s == 4) note.slide |= 8;
                    else if (s == -1) note.slide |= 16;
                    else if (s == -2) note.slide |= 32;
                }
            }
            if (flags2 & 16)
            {
                const int type = in.i8();
                if (type == 1) note.harmonic = 1;
                else if (type == 3) note.harmonic = 3;
                else if (type == 4) note.harmonic = 4;
                else if (type != 0) note.harmonic = 2;

                if (gp5())
                {
                    if (type == 2) in.skip (3);
                    else if (type == 3) in.skip (1);
                }
            }
            if (flags2 & 32)
            {
                note.trill = true;
                in.skip (2);
            }
        }

        void readNotes (int trackIndex, GBeat& beat, const GNote& beatTemplate)
        {
            auto& track = tracks[(size_t) trackIndex];
            const int stringFlags = in.u8();

            for (int s = 1; s <= track.numStrings && ! in.bad; ++s)
            {
                if ((stringFlags & (1 << (7 - s))) == 0)
                    continue;

                GNote note = beatTemplate;
                note.string = s;
                const int flags = in.u8();
                note.ghost = (flags & 4) != 0;
                note.accent = (flags & 64) != 0 || (gp5() && (flags & 2) != 0);

                if (flags & 32)
                    note.type = in.u8();
                if (gp3() && (flags & 1)) in.skip (2);
                if (flags & 16)
                    note.velocity = 15 + 16 * in.i8() - 16;

                int fret = 0;
                if (flags & 32)
                    fret = in.i8();

                if (note.type == 2)
                    fret = lastFret[(size_t) trackIndex][(size_t) s];   // a tie sounds the fret before it
                note.fret = juce::jlimit (0, 99, fret);
                lastFret[(size_t) trackIndex][(size_t) s] = note.fret;

                if (flags & 128) in.skip (2);                // fingering
                if (gp5())
                {
                    if (flags & 1) in.skip (8);              // duration percent
                    in.u8();                                 // swap accidentals
                }
                else if (! gp3() && (flags & 1))
                    in.skip (2);

                if (flags & 8)
                    readNoteEffects (note);

                if (! in.bad)
                    beat.notes.push_back (std::move (note));
            }
        }

        void readChord (GBeat& beat, int stringCount)
        {
            const bool newFormat = in.boolean();
            if (! newFormat)
            {
                beat.chord = in.intByteSizeString();
                if (in.i32() != 0)
                    in.skip (6 * 4);
                return;
            }

            in.u8();                                         // sharp
            in.skip (3);
            if (gp3())
                in.skip (4 * 3);                             // root, type, extension
            else
                in.skip (3);
            in.skip (4);                                     // bass
            in.skip (4);                                     // tonality
            in.u8();                                         // add
            beat.chord = in.byteSizeString (22);
            in.skip (gp3() ? 12 : 3);                        // 5th, 9th, 11th
            in.skip (4);                                     // first fret
            in.skip ((size_t) (gp3() ? 6 : 7) * 4);
            if (gp3())
                in.skip (4 + 6 * 4);
            else
                in.skip (1 + 15);
            in.skip (7);                                     // omissions
            in.skip (1);
            if (! gp3())
                in.skip (7 + 1);                             // fingerings, show
            juce::ignoreUnused (stringCount);
        }

        void readMixTableChange()
        {
            if (gp5())
            {
                in.i8();                                     // instrument
                in.i32(); in.i32(); in.i32();
                if (v50) { in.i16(); in.skip (1); in.skip (1); } else in.i32();
            }
            else
                in.i8();

            int values[6];
            for (auto& v : values)
                v = in.i8();
            if (gp5()) in.intByteSizeString();
            const int tempoValue = in.i32();

            for (const int v : values)
                if (v >= 0) in.i8();
            if (tempoValue >= 0)
            {
                in.i8();
                if (gp5() && ! v50) in.u8();
            }

            if (! gp3()) in.i8();                            // all-tracks flags
            if (gp5())
            {
                in.i8();                                     // wah
                if (! v50) { in.intByteSizeString(); in.intByteSizeString(); }
            }
        }

        void readBeatEffects (GNote& tmpl)
        {
            if (gp3())
            {
                const int f1 = in.u8();
                tmpl.vibrato = tmpl.vibrato || (f1 & 1) != 0;
                if (f1 & 4) tmpl.harmonic = 1;
                if (f1 & 8) tmpl.harmonic = 2;
                if (f1 & 32)
                {
                    const int slap = in.u8();
                    tmpl.slapKind = slap;                   // 1 slap, 2 pop, 3 tap
                    in.i32();
                }
                if (f1 & 64) in.skip (2);
                return;
            }

            const int f1 = in.u8();
            const int f2 = in.u8();
            if (f1 & 32) tmpl.slapKind = in.i8();
            if (f2 & 4)
            {
                GNote scratch;
                readBend (scratch);
            }
            if (f1 & 64) in.skip (2);
            if (f2 & 2) in.i8();
        }

        double readDuration (int flags)
        {
            const int code = in.i8();
            const int clamped = juce::jlimit (-2, 4, code);
            double beats = 4.0 / (double) (1 << (clamped + 2));
            if (flags & 1) beats *= 1.5;
            if (flags & 32)
            {
                const int tuplet = in.i32();
                int times = 0;
                switch (tuplet)
                {
                    case 3: times = 2; break;
                    case 5: case 6: case 7: times = 4; break;
                    case 9: case 10: case 11: case 12: case 13: times = 8; break;
                    default: break;
                }
                if (times > 0)
                    beats *= (double) times / (double) tuplet;
            }
            return beats;
        }

        void readBeat (int trackIndex, int stringCount, std::vector<GBeat>& voice)
        {
            GBeat beat;
            const int flags = in.u8();
            int status = 1;
            if (flags & 0x40) status = in.u8();
            beat.empty = (status == 0);
            beat.length = readDuration (flags);

            GNote tmpl;
            if (flags & 0x02) readChord (beat, stringCount);
            if (flags & 0x04) in.intByteSizeString();
            if (flags & 0x08) readBeatEffects (tmpl);
            if (flags & 0x10) readMixTableChange();
            readNotes (trackIndex, beat, tmpl);

            if (gp5() && ! in.bad)
            {
                const int flags2 = in.i16();
                if (flags2 & 2048) in.u8();
            }

            if (status == 2)           // rest
                beat.notes.clear();

            voice.push_back (std::move (beat));
        }

        void readMeasures()
        {
            for (size_t m = 0; m < headers.size() && ! in.bad; ++m)
            {
                for (size_t t = 0; t < tracks.size() && ! in.bad; ++t)
                {
                    auto& track = tracks[t];
                    track.measures.emplace_back();
                    auto& voices = track.measures.back();
                    const int numVoices = gp5() ? 2 : 1;

                    for (int v = 0; v < numVoices && ! in.bad; ++v)
                    {
                        voices.emplace_back();
                        const int beats = in.i32();
                        if (beats < 0 || (size_t) beats > in.remaining())
                        {
                            in.bad = true;
                            break;
                        }

                        for (int b = 0; b < beats && ! in.bad; ++b)
                            readBeat ((int) t, track.numStrings, voices.back());
                    }

                    if (gp5() && ! in.bad && in.remaining() > 0)
                        in.u8();                            // line break
                    else if (gp5() && in.remaining() == 0)
                        {}                                    // tolerated at EOF
                }
            }
        }

        //----------------------------------------------------------------------
        bool parse()
        {
            if (! parseVersion())
                return false;

            readInfo();

            if (! gp5())
                in.boolean();                               // triplet feel
            if (! gp3())
                readLyrics();

            if (gp5())
            {
                if (! v50) { in.skip (8 + 11); }            // RSE master effect
                readPageSetup();
                in.intByteSizeString();                     // tempo name
            }

            tempo = in.i32();
            if (gp5())
            {
                if (! v50) in.boolean();                    // hide tempo
                in.i8();                                    // key
                in.i32();                                   // octave
            }
            else
            {
                in.i32();                                   // key
                if (! gp3()) in.i8();                       // octave
            }

            readMidiChannels();
            if (gp5())
            {
                in.skip (19 * 2);                           // directions
                in.i32();                                   // master reverb
            }

            const int measureCount = in.i32();
            const int trackCount = in.i32();
            if (in.bad)
                return true;

            if (measureCount < 0 || measureCount > GuitarProLegacyReader::kMaxMeasures
                || trackCount < 1 || trackCount > GuitarProLegacyReader::kMaxTracks)
            {
                warnings.add ("The measure or track count is implausible ("
                              + juce::String (measureCount) + " / " + juce::String (trackCount) + ").");
                in.bad = true;
                return true;
            }

            readMeasureHeaders (measureCount);
            readTracks (trackCount);

            lastFret.assign (tracks.size(), std::vector<int> (8, 0));
            if (! in.bad)
                readMeasures();

            return true;
        }
    };

    //==========================================================================
    std::vector<int> unrollRepeats (const std::vector<GHeader>& headers, int limit, bool& truncated)
    {
        std::vector<int> order;
        const int n = (int) headers.size();
        std::vector<int> remaining (headers.size());
        for (int i = 0; i < n; ++i)
            remaining[(size_t) i] = headers[(size_t) i].repeatClose;

        int i = 0, open = 0, pass = 1;
        bool jumped = false;
        int guard = 0;

        while (i < n && (int) order.size() < limit && ++guard < limit * 8)
        {
            const auto& h = headers[(size_t) i];

            if (h.repeatOpen && ! jumped)
            {
                open = i;
                pass = 1;
            }
            jumped = false;

            if (h.alternative != 0 && pass >= 1 && pass <= 30 && (h.alternative & (1 << (pass - 1))) == 0)
            {
                ++i;
                continue;
            }

            order.push_back (i);

            if (h.repeatClose > 0)
            {
                if (remaining[(size_t) i] > 0)
                {
                    --remaining[(size_t) i];
                    ++pass;
                    i = open;
                    jumped = true;
                    continue;
                }
                remaining[(size_t) i] = h.repeatClose;   // a later outer pass may replay it
                pass = 1;
            }
            ++i;
        }

        truncated = i < n;
        return order;
    }

    struct OutNote
    {
        double start = 0.0, duration = 1.0;
        int stringIndex = 0, fret = 0, midi = 60;
        double velocity = 0.75;
        const GNote* src = nullptr;
        bool legatoFrom = false, hopoUp = false;
        int hopoTarget = -1;
        int slideTarget = -1;
    };
}

//==============================================================================
bool GuitarProLegacyReader::looksLikeLegacyGuitarPro (const void* data, size_t numBytes) noexcept
{
    static const char magic[] = "FICHIER GUITAR PRO";
    if (data == nullptr || numBytes < 20)
        return false;
    const auto* p = static_cast<const juce::uint8*> (data);
    return p[0] >= 18 && p[0] <= 30 && std::memcmp (p + 1, magic, sizeof (magic) - 1) == 0;
}

bool GuitarProLegacyReader::looksLikeGpx (const void* data, size_t numBytes) noexcept
{
    return data != nullptr && numBytes >= 4
        && (std::memcmp (data, "BCFZ", 4) == 0 || std::memcmp (data, "BCFS", 4) == 0);
}

bool GuitarProLegacyReader::looksLikePowerTab (const void* data, size_t numBytes) noexcept
{
    return data != nullptr && numBytes >= 4 && std::memcmp (data, "ptab", 4) == 0;
}

bool GuitarProLegacyReader::read (const void* data, size_t numBytes, PerformanceScore& destination,
                                  TabImportDiagnostics* diagnostics, int preferredTrack)
{
    lastError.clear();
    trackNames.clear();
    if (diagnostics != nullptr)
        *diagnostics = {};

    if (! looksLikeLegacyGuitarPro (data, numBytes))
    {
        lastError = "That is not a Guitar Pro 3, 4 or 5 file.";
        return false;
    }

    if (numBytes > kMaxFileBytes)
    {
        lastError = "That Guitar Pro file is too large to open.";
        return false;
    }

    Parser parser;
    parser.in.data = static_cast<const juce::uint8*> (data);
    parser.in.size = numBytes;

    if (! parser.parse())
    {
        lastError = "That is not a Guitar Pro 3, 4 or 5 file.";
        return false;
    }

    for (const auto& t : parser.tracks)
        trackNames.add (t.name);

    // Pick the track.
    int chosen = -1;
    auto countNotes = [] (const GTrack& t)
    {
        int n = 0;
        for (const auto& m : t.measures)
            for (const auto& v : m)
                for (const auto& b : v)
                    if (! b.empty)
                        n += (int) b.notes.size();
        return n;
    };

    if (preferredTrack >= 0 && preferredTrack < (int) parser.tracks.size()
        && ! parser.tracks[(size_t) preferredTrack].percussion)
        chosen = preferredTrack;

    for (size_t i = 0; chosen < 0 && i < parser.tracks.size(); ++i)
        if (! parser.tracks[i].percussion && countNotes (parser.tracks[i]) > 0)
            chosen = (int) i;

    juce::StringArray warnings = parser.warnings;
    if (parser.in.bad)
        warnings.add ("The file ended or went out of step part-way through; the bars read so far were kept.");

    if (chosen < 0)
    {
        lastError = parser.in.bad ? "That Guitar Pro file is damaged and no notes could be read from it."
                                  : "That Guitar Pro file has no pitched notes Luthier can read.";
        if (diagnostics != nullptr)
            diagnostics->warnings = warnings;
        return false;
    }

    const auto& track = parser.tracks[(size_t) chosen];

    if (parser.tracks.size() > 1)
    {
        juce::StringArray names;
        for (size_t i = 0; i < parser.tracks.size(); ++i)
            names.add (juce::String ((int) i + 1) + " " + parser.tracks[i].name
                       + (parser.tracks[i].percussion ? " (drums)" : ""));
        warnings.add ("Read track " + juce::String (chosen + 1) + " of " + juce::String ((int) parser.tracks.size())
                      + " (" + names.joinIntoString (", ") + ").");
    }

    // Walk the bars in playing order.
    bool truncated = false;
    const auto order = unrollRepeats (parser.headers, kMaxUnrolledMeasures, truncated);
    if (truncated)
        warnings.add ("Repeats were unrolled up to a safety limit; the rest of the piece was not read.");

    std::vector<OutNote> out;
    double barStart = 0.0;
    std::vector<std::vector<GBeat>> emptyBar;
    int hammerFrom[8];
    std::fill (std::begin (hammerFrom), std::end (hammerFrom), -1);
    std::vector<std::pair<double, juce::String>> chords;
    int lastOut[8];
    std::fill (std::begin (lastOut), std::end (lastOut), -1);
    bool timeSigChanged = false;
    const auto& firstHeader = parser.headers.empty() ? GHeader() : parser.headers.front();

    for (const int m : order)
    {
        const auto& header = parser.headers[(size_t) m];
        if (header.numerator != firstHeader.numerator || header.denominator != firstHeader.denominator)
            timeSigChanged = true;

        if ((size_t) m < track.measures.size())
        {
            for (const auto& voice : track.measures[(size_t) m])
            {
                double pos = barStart;
                for (const auto& beat : voice)
                {
                    if (beat.empty)
                        continue;

                    if (beat.chord.isNotEmpty())
                        chords.emplace_back (pos, beat.chord);

                    for (const auto& n : beat.notes)
                    {
                        const int s = n.string;     // 1..7
                        if (n.type == 2)             // tie: lengthen the sounding note
                        {
                            const int idx = lastOut[s];
                            if (idx >= 0 && std::abs (out[(size_t) idx].start + out[(size_t) idx].duration - pos) < 1.0e-3)
                            {
                                out[(size_t) idx].duration += beat.length;
                                continue;
                            }
                        }

                        OutNote o;
                        o.start = pos;
                        o.duration = beat.length;
                        o.stringIndex = s - 1;
                        o.fret = n.fret;
                        o.midi = juce::jlimit (0, 127, track.tuning[s - 1] + track.capo + n.fret);
                        o.velocity = juce::jlimit (0.05, 1.0, n.velocity / 127.0);
                        o.src = &n;

                        if (hammerFrom[s] >= 0)
                        {
                            o.legatoFrom = true;
                            o.hopoUp = n.fret >= hammerFrom[s];
                            hammerFrom[s] = -1;
                        }
                        if (n.hammer)
                            hammerFrom[s] = n.fret;

                        lastOut[s] = (int) out.size();
                        out.push_back (o);
                    }

                    pos += beat.length;
                }
            }
        }

        barStart += header.beats();
    }

    if (timeSigChanged)
        warnings.add ("The time signature changes during the piece; bar lines use the first one.");

    if (out.empty())
    {
        lastError = "That Guitar Pro file has no pitched notes Luthier can read.";
        if (diagnostics != nullptr)
            diagnostics->warnings = warnings;
        return false;
    }

    std::stable_sort (out.begin(), out.end(), [] (const OutNote& a, const OutNote& b)
    {
        return a.start < b.start - 1.0e-9;
    });

    // Slide targets: the fret the next note on the string lands on.
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (out[i].src == nullptr || (out[i].src->slide & 3) == 0)
            continue;
        for (size_t j = i + 1; j < out.size(); ++j)
            if (out[j].stringIndex == out[i].stringIndex && out[j].start > out[i].start + 1.0e-9)
            {
                out[i].slideTarget = out[j].fret;
                break;
            }
    }

    // Build the score.
    destination.clear();
    destination.beginCapture (parser.tempo >= 20 && parser.tempo <= 400 ? (double) parser.tempo : 120.0,
                              firstHeader.numerator, firstHeader.denominator);
    destination.getMeta().title = parser.title.isNotEmpty() ? parser.title : juce::String ("Guitar Pro import");
    destination.getMeta().artist = parser.artist;

    auto& scoreTrack = destination.getTrack (0);
    scoreTrack.name = track.name.isNotEmpty() ? track.name : juce::String ("Guitar");
    scoreTrack.numStrings = juce::jlimit (1, kMaxStrings, track.numStrings);
    scoreTrack.capoFret = track.capo;
    for (int s = 0; s < scoreTrack.numStrings; ++s)
        scoreTrack.tuning[(size_t) s] = juce::jlimit (0, 127, track.tuning[s]);

    using Type = ScoreTechnique::Type;
    int openIndex[8];
    double openEnd[8];
    std::fill (std::begin (openIndex), std::end (openIndex), -1);
    std::fill (std::begin (openEnd), std::end (openEnd), 0.0);
    double finalBeat = barStart;

    auto closeString = [&] (int s, double at)
    {
        if (openIndex[s] >= 0)
        {
            destination.noteEnded (s, juce::jmin (openEnd[s], at));
            openIndex[s] = -1;
        }
    };

    for (const auto& o : out)
    {
        const int s = juce::jlimit (0, 7, o.stringIndex);
        closeString (s, o.start);

        destination.noteStarted (s, o.fret, o.midi, 440.0 * std::pow (2.0, (o.midi - 69) / 12.0),
                                 o.velocity, o.start);
        openIndex[s] = 1;
        openEnd[s] = o.start + o.duration;
        finalBeat = juce::jmax (finalBeat, openEnd[s]);

        const auto& n = *o.src;
        auto add = [&] (Type type, double value = 0.0)
        {
            ScoreTechnique t;
            t.type = type;
            t.value = value;
            destination.addTechnique (s, t);
        };

        if (o.legatoFrom)       add (o.hopoUp ? Type::hammerOn : Type::pullOff);
        if (n.ghost)            add (Type::ghostNote);
        if (n.accent)           add (Type::accent);
        if (n.type == 3)        add (Type::deadNote);
        if (n.vibrato)          add (Type::vibrato);
        if (n.palmMute)         add (Type::palmMute);
        if (n.letRing)          add (Type::letRing);
        if (n.staccato)         add (Type::staccato);
        if (n.trill)            add (Type::trill);
        if (n.slapKind == 1)    add (Type::slap);
        if (n.slapKind == 2)    add (Type::pop);
        if (n.slapKind == 3)    add (Type::tap);

        switch (n.harmonic)
        {
            case 1: add (Type::naturalHarmonic); break;
            case 2: add (Type::artificialHarmonic); break;
            case 3: add (Type::tapHarmonic); break;
            case 4: add (Type::pinchHarmonic); break;
            default: break;
        }

        if ((n.slide & 3) != 0)
            add ((n.slide & 2) != 0 ? Type::slideLegato : Type::slideShift,
                 o.slideTarget >= 0 ? (double) o.slideTarget : (double) o.fret);
        if ((n.slide & 4) != 0)  add (Type::slideOut, (double) juce::jmax (0, o.fret - 5));
        if ((n.slide & 8) != 0)  add (Type::slideOut, (double) juce::jmin (kMaxFretLocal, o.fret + 5));
        if ((n.slide & 16) != 0) add (Type::slideIn);
        if ((n.slide & 32) != 0) add (Type::slideIn, (double) juce::jmin (kMaxFretLocal, o.fret + 3));

        if (n.hasBend && ! n.bend.points.empty())
        {
            int peak = 0;
            for (const auto& p : n.bend.points)
                peak = juce::jmax (peak, p.second);

            const bool pre = n.bend.points.front().first == 0 && n.bend.points.front().second > 0;
            const bool releases = n.bend.points.back().second < peak;

            ScoreTechnique t;
            t.type = pre ? Type::preBend : Type::bend;
            t.value = peak / 25.0;
            for (const auto& p : n.bend.points)
                t.curve.emplace_back (p.first / 12.0, p.second / 25.0);
            destination.addTechnique (s, t);

            if (releases && peak > 0)
                add (Type::bendRelease, peak / 25.0);
        }
    }

    for (int s = 0; s < 8; ++s)
        closeString (s, 1.0e9);

    for (const auto& c : chords)
        destination.addChordSymbol (c.first, c.second);

    destination.endCapture (finalBeat);

    if (diagnostics != nullptr)
    {
        diagnostics->numStrings = scoreTrack.numStrings;
        diagnostics->measures = (int) order.size();
        diagnostics->notes = (int) out.size();
        diagnostics->repeatsUnrolled = juce::jmax (0, (int) order.size() - (int) parser.headers.size());
        diagnostics->tuningFromHeader = true;
        diagnostics->warnings = warnings;
    }

    return true;
}

} // namespace luthier
