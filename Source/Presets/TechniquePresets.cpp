#include "TechniquePresets.h"
#include "../Parameters.h"

namespace luthier
{

namespace
{
    namespace P = ParamIDs;

    // Choice indices, as FactoryPresets.cpp names them.
    enum { Strat = 0, LesPaul = 2, Dread = 11, Classical = 15, Resonator = 18, PBass = 19 };
    enum { StdTune = 0, OpenGTune = 5, BassTune = 15 };
    enum { Twin = 0, Plexi = 4, JCM800 = 5, SVT = 11, AcousticDI = 12 };
    enum { C2x12Open = 2, C4x12 = 4, C8x10 = 8, CabDI = 9 };

    // mute_master_mode: 0 Off, then MuteType + 1.
    constexpr double masterFor (MuteType t) { return (double) ((int) t + 1); }

    juce::String gridBlock (const char* cells)
    {
        juce::String ids;

        for (int i = 0; i < kLiveMuteSteps; ++i)
            ids << (i > 0 ? "," : "") << getMuteTypeId (muteTypeFromLetter (cells[i]));

        return "{\"mute_grid\":\"" + ids + "\"}";
    }

    juce::String gridPresetBlock (const char* presetName)
    {
        for (int i = 0; i < getNumMuteGridPresets(); ++i)
            if (juce::String (getMuteGridPreset (i).name) == presetName)
                return gridBlock (getMuteGridPreset (i).cells);

        return {};
    }
}

juce::StringArray getTechniqueArmParameterIds()
{
    return { P::scrapeArmed, P::slapArmed, P::muteArmed, P::tapArmed, P::bendArmed, P::slideGuitar };
}

const std::vector<TechniquePresetRecipe>& getTechniquePresetRecipes()
{
    static const std::vector<TechniquePresetRecipe> bank = []
    {
        std::vector<TechniquePresetRecipe> b;

        auto make = [&b] (const char* name, const char* description, const char* tags,
                          int guitar, int tuning, int amp, int cab, double gain) -> TechniquePresetRecipe&
        {
            b.push_back ({ name, "Techniques", description, tags, {}, {} });
            auto& r = b.back();
            r.values = { { P::guitarType, guitar }, { P::tuningPreset, tuning }, { P::ampModel, amp },
                         { P::cabType, cab }, { P::ampGain, gain } };
            return r;
        };

        auto set = [] (TechniquePresetRecipe& r, std::initializer_list<std::pair<const char*, double>> values)
        {
            for (auto& v : values)
                r.values.emplace_back (v.first, v.second);
        };

        //======================================================================
        // technique-cascade.md 5: combined presets
        //======================================================================
        {
            auto& r = make ("Metal Lead Combo", "Tap arpeggios over a palm-muted gallop, bends on the melody. "
                            "Right-hand taps on MIDI channel 2.", "techniques,cascade,tap,bend,mute,metal",
                            LesPaul, StdTune, JCM800, C4x12, 0.78);
            set (r, { { P::tapArmed, 1 }, { P::tapSource, 0 }, { P::tapAutoPullOff, 1 },
                      { P::bendArmed, 1 }, { P::bendVibratoSource, 1 },
                      { P::muteArmed, 1 }, { P::mutePalmPressure, 0.6 } });
            r.techniques = gridPresetBlock ("Metal Gallop");
        }
        {
            auto& r = make ("Funk Slap Groove", "Thumb slaps and pops with a chuka mute grid and the odd bend.",
                            "techniques,cascade,slap,mute,bend,funk,bass", PBass, BassTune, SVT, C8x10, 0.35);
            set (r, { { P::slapArmed, 1 }, { P::slapType, 0 }, { P::muteArmed, 1 },
                      { P::bendArmed, 1 }, { P::bendVibratoSource, 0 } });
            r.techniques = gridPresetBlock ("Funk Chuka");
        }
        {
            auto& r = make ("Slide Blues", "Bottleneck in open G, finger vibrato on the bar, a light palm behind it.",
                            "techniques,cascade,slide,bend,mute,blues", Strat, OpenGTune, Twin, C2x12Open, 0.3);
            set (r, { { P::slideGuitar, 1 }, { P::slideMode, 0 }, { P::bendArmed, 1 }, { P::bendVibratoSource, 1 },
                      { P::bendVibratoDepth, 15 }, { P::muteArmed, 1 } });
            r.techniques = gridBlock ("llllllllllllllll");
        }
        {
            auto& r = make ("Percussive Tap", "Full-strength taps on extreme-muted strings, body taps for the beat.",
                            "techniques,cascade,tap,mute,slap,percussive,acoustic", Dread, StdTune, AcousticDI, CabDI, 0.05);
            set (r, { { P::tapArmed, 1 }, { P::tapStrengthCurve, 1.0 },
                      { P::muteArmed, 1 }, { P::muteMasterMode, masterFor (MuteType::palmExtreme) },
                      { P::slapArmed, 1 }, { P::slapType, 3 }, { P::useFingers, 1 } });
        }
        {
            auto& r = make ("Scrape Intro", "A pick scrape into a palm mute that eases off as the part builds.",
                            "techniques,cascade,scrape,mute,rock", LesPaul, StdTune, Plexi, C4x12, 0.62);
            set (r, { { P::scrapeArmed, 1 }, { P::muteArmed, 1 }, { P::mutePalmPressure, 0.8 },
                      { P::muteHumanise, 0.35 } });
            r.techniques = gridPresetBlock ("Metal Chug 16ths");
        }
        {
            auto& r = make ("Full Cascade Demo", "Every technique armed at once: slide on the bass three, "
                            "taps and bends above, scrape, slap and mute over it all.",
                            "techniques,cascade,demo", Strat, StdTune, Twin, C2x12Open, 0.3);
            set (r, { { P::scrapeArmed, 1 }, { P::slapArmed, 1 }, { P::muteArmed, 1 }, { P::tapArmed, 1 },
                      { P::bendArmed, 1 }, { P::slideGuitar, 1 }, { P::slideContact, 1 } });
            r.techniques = gridPresetBlock ("Country Boom-Chick");
        }

        //======================================================================
        // muting-rhythm.md 6
        //======================================================================
        struct MutePreset { const char* name; const char* description; bool grid; MuteType master; int guitar; int amp; int cab; double gain; };
        const MutePreset mutes[] =
        {
            { "Metal Chug 16ths",   "Palm mute heavy on every step, full dynamics.",              false, MuteType::palmHeavy, LesPaul,   JCM800,     C4x12,     0.8 },
            { "Funk Chuka",         "Chuka on the off-beats, open on the beats, ghosts on the ands.", true, MuteType::open,  Strat,     Twin,       C2x12Open, 0.15 },
            { "Reggae Skank",       "Open on 2 and 4, a light palm everywhere else.",             true,  MuteType::open,      Strat,     Twin,       C2x12Open, 0.2 },
            { "Country Boom-Chick", "Palm-muted boom on the bass, chuka chick on the treble.",    true,  MuteType::open,      Strat,     Twin,       C2x12Open, 0.25 },
            { "Metal Gallop",       "Palm mute heavy in the gallop: dotted eighth, sixteenth, eighth.", true, MuteType::open, LesPaul, JCM800,     C4x12,     0.8 },
            { "Classical Staccato", "A fret mute on every note, no palm.",                        false, MuteType::fretMute,  Classical, AcousticDI, CabDI,     0.05 },
        };

        for (const auto& m : mutes)
        {
            auto& r = make (m.name, m.description, "techniques,mute", m.guitar, StdTune, m.amp, m.cab, m.gain);
            set (r, { { P::muteArmed, 1 } });

            if (m.grid)
                r.techniques = gridPresetBlock (m.name);
            else
                set (r, { { P::muteMasterMode, masterFor (m.master) } });

            if (m.guitar == Classical)
                set (r, { { P::useFingers, 1 } });
        }

        //======================================================================
        // two-hand-tapping.md 8
        //======================================================================
        {
            auto& r = make ("Standard Two-Hand Tap", "Right-hand taps on MIDI channel 2, auto pull-off, flick 0.5, fret snap.",
                            "techniques,tap", Strat, StdTune, Plexi, C4x12, 0.55);
            set (r, { { P::tapArmed, 1 }, { P::tapSource, 0 }, { P::tapChannel, 2 }, { P::tapAutoPullOff, 1 },
                      { P::tapFlick, 0.5 }, { P::tapFretSnap, 1 } });
        }
        {
            auto& r = make ("Legato Runs", "One channel: soft notes close together are hammer-ons and pull-offs.",
                            "techniques,tap,legato", Strat, StdTune, Plexi, C4x12, 0.55);
            set (r, { { P::tapArmed, 1 }, { P::tapSource, 1 }, { P::tapHammerThreshold, 60 }, { P::tapFlick, 0.3 } });
        }
        {
            auto& r = make ("Eight-Finger Tap", "Many taps per string, each hand on its own MPE channel. "
                            "Four per string in stock range; unlock Pick for eight.",
                            "techniques,tap,mpe", Strat, StdTune, Twin, C2x12Open, 0.2);
            set (r, { { P::tapArmed, 1 }, { P::tapSource, 0 }, { P::tapMaxConcurrent, 4 }, { P::tapFlick, 0.4 } });
        }
        {
            auto& r = make ("Microtonal Tap", "Fret snap off: fretboard taps land between the frets.",
                            "techniques,tap,microtonal", Strat, StdTune, Twin, C2x12Open, 0.2);
            set (r, { { P::tapArmed, 1 }, { P::tapSource, 2 }, { P::tapFretSnap, 0 } });
        }
        {
            auto& r = make ("Percussive Tap Mute", "Taps at full strength on extreme-muted strings.",
                            "techniques,tap,mute,percussive", Strat, StdTune, Twin, C2x12Open, 0.2);
            set (r, { { P::tapArmed, 1 }, { P::tapStrengthCurve, 1.0 }, { P::muteArmed, 1 },
                      { P::muteMasterMode, masterFor (MuteType::palmExtreme) } });
        }

        //======================================================================
        // microtonal-bends.md 7
        //======================================================================
        {
            auto& r = make ("Standard Whole-Tone Bend", "Global and per-string ranges a whole tone, no quantise.",
                            "techniques,bend", Strat, StdTune, Plexi, C4x12, 0.5);
            set (r, { { P::bendArmed, 1 }, { P::bendGlobalRange, 200 }, { P::bendVibratoSource, 0 } });
        }
        {
            auto& r = make ("Quarter-Tone Blues", "Bends drawn lightly to the quarter-tone grid.",
                            "techniques,bend,microtonal,blues", Strat, StdTune, Twin, C2x12Open, 0.3);
            set (r, { { P::bendArmed, 1 }, { P::bendQuantise, 1 }, { P::bendSnap, 0.3 } });
        }
        {
            auto& r = make ("Middle Eastern Maqam", "24-EDO quantise with a strong snap.",
                            "techniques,bend,microtonal,maqam", Dread, StdTune, AcousticDI, CabDI, 0.05);
            set (r, { { P::bendArmed, 1 }, { P::bendQuantise, 3 }, { P::bendSnap, 0.8 }, { P::useFingers, 1 } });
        }
        {
            auto& r = make ("Wide Vibrato", "Vibrato 40 cents deep at 5 Hz, in after 100 ms.",
                            "techniques,bend,vibrato", LesPaul, StdTune, Plexi, C4x12, 0.6);
            set (r, { { P::bendArmed, 1 }, { P::bendVibratoSource, 1 }, { P::bendVibratoDepth, 40 },
                      { P::bendVibratoRate, 5 }, { P::bendVibratoOnset, 100 } });
        }
        {
            auto& r = make ("Whammy-Style Two-Octave", "The expression pedal bends everything two octaves.",
                            "techniques,bend,whammy", Strat, StdTune, JCM800, C4x12, 0.7);
            set (r, { { P::bendArmed, 1 }, { P::bendGlobalSource, 1 }, { P::bendGlobalRange, 2400 },
                      { P::bendVibratoSource, 0 } });
        }

        //======================================================================
        // slide-technique-controls.md 6
        //======================================================================
        {
            auto& r = make ("Standard Slide (Mod Wheel)", "The mod wheel moves the bar; a standard glass bar at pressure 0.7.",
                            "techniques,slide", Strat, OpenGTune, Twin, C2x12Open, 0.3);
            set (r, { { P::slideGuitar, 1 }, { P::slideMode, 0 }, { P::slidePosSource, 1 }, { P::slidePressure, 0.7 } });
        }
        {
            auto& r = make ("Pitch-Bend Slide", "Pitch bend moves the bar two octaves either way, for keyboard players.",
                            "techniques,slide", Strat, OpenGTune, Twin, C2x12Open, 0.3);
            set (r, { { P::slideGuitar, 1 }, { P::slideMode, 0 }, { P::slidePosSource, 2 }, { P::slidePosMode, 1 },
                      { P::slidePosRange, 24 } });
        }
        {
            auto& r = make ("Lap Steel Full Control", "MPE Y moves the bar, MPE Z presses it, aftertouch slants it.",
                            "techniques,slide,mpe,lap steel", Resonator, OpenGTune, AcousticDI, CabDI, 0.05);
            set (r, { { P::slideGuitar, 1 }, { P::slideMode, 1 }, { P::slidePosSource, 3 },
                      { P::slidePressureSource, 8 }, { P::slideSlantSource, 7 } });
        }
        {
            auto& r = make ("Auto-Vibrato Hold", "Hold the bar still and it starts to shake: 5 Hz, 10 cents.",
                            "techniques,slide,vibrato", Strat, OpenGTune, Twin, C2x12Open, 0.3);
            set (r, { { P::slideGuitar, 1 }, { P::slideMode, 0 }, { P::slidePosSource, 1 }, { P::slidePressure, 0.7 },
                      { P::slideAutoVibrato, 1 }, { P::slideAutoVibRate, 5 }, { P::slideAutoVibDepth, 10 } });
        }

        return b;
    }();

    return bank;
}

} // namespace luthier
