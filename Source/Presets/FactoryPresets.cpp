#include "FactoryPresets.h"
#include "PresetManager.h"
#include "../Parameters.h"

namespace luthier
{

const juce::AudioProcessor* FactoryPresets::rangeSource = nullptr;

void FactoryPresets::setProcessorForRanges (const juce::AudioProcessor* processor) noexcept
{
    rangeSource = processor;
}

namespace
{
    using E = FactoryPresets::Entry;

    namespace P = ParamIDs;

    // Choice indices, named so the tables below read as English rather than digits.
    enum Guitar { Strat = 0, Tele, LesPaul, SG, ES335, Jazzmaster, Explorer, RG,
                  SevenStr, EightStr, BaritoneE, Dread, Auditorium, Jumbo, Parlor,
                  Classical, Flamenco, TwelveStr, Resonator, PBass, JBass, Rick,
                  FiveBass, FretlessB, CustomGtr };

    enum Tune { StdTune = 0, DropDTune, DropCTune, DropBTune, DadgadTune, OpenGTune,
                OpenDTune, OpenETune, OpenCTune, HalfDown, FullDown, NashvilleTune,
                SevenTune, EightTune, BaritoneTune, BassTune, Bass5Tune };

    enum Amp { Twin = 0, Tweed, DeluxeAmp, Champ, Plexi, JCM800, AC30,
               Recto, Bogner, Diezel, Orange, SVT, AcousticDI };

    enum Pdl { NoPedal = 0, Comp, Gate, WahP, EnvF, Oct, PitchP, OD, Dist, Fuzz,
               BoostP, VolP, ChorusP, PhaserP, FlangerP, TremP, RotaryP, DelayP,
               ReverbP, SpringP, GEQ, PEQ };

    enum Cab { C1x12Open = 0, C1x12Closed, C2x12Open, C2x12Closed, C4x12,
               C4x12Vintage, C1x15, C4x10, C8x10, CabDI };

    enum Spk { Greenback = 0, V30, G12H, G12T75, Jensen, AlnicoBlue, EVM12L, BassSpk };
    enum Mic { SM57 = 0, SM7B, MD421, U87, R121, C414, D112 };
    enum MicPos { AxisCentre = 0, CapEdge, Off45, ConeEdge, RearPos };
    enum MicDist { CloseMic = 0, MediumMic, FarMic };
    enum Room { IsoBooth = 0, SmallBooth, SmallStudio, LargeStudio, LiveRoom, Hall, Cathedral };
    enum RoomMat { DryRoom = 0, WoodRoom, TileRoom, StoneRoom };
    enum Sel { BridgePu = 0, BridgeMid, MidPu, MidNeck, NeckPu, AllPu, BridgeNeck };
    enum Mat { NPS = 0, PureNickel, Stainless, Cobalt, PhosBronze, Bronze8020,
               SilkSteel, NylonStr, Fluoro, Flatwound, Halfwound, Coated };
    enum Gauge { XLight = 0, LightG, RegularG, MediumG, HeavyG, AcLight, AcMedium,
                 ClassNormal, ClassHard, BassStdG, BassHeavyG };
    enum Age { Fresh = 0, BrokenIn, OldStrings };
    enum Pick { NylonPick = 0, Celluloid, Delrin, MetalPick, WoodPick, FeltPick,
                Nail, Flesh, ThumbPick, Thumbpick2, BrushPick, SlidePick };
    enum Mode { MonoMode = 0, PolyMode, ControllerMode };
    enum Bridge { FixedBr = 0, VintageTrem, FloydRose, TransTrem, Bigsby };
    enum BodyM { Convolve = 0, ModalM, HybridM, NoBody };

    //==========================================================================
    // Each preset is written as a short recipe. The helpers below keep the common
    // groups (amp controls, macros, mic setup) to one line each, so what is left
    // on the page for any given preset is only what makes it distinctive.
    //==========================================================================
    struct PresetRecipe
    {
        juce::String name;
        juce::String category;
        juce::String description;
        juce::String tags;
        std::vector<std::pair<juce::String, double>> values;
    };

    void addCommon (PresetRecipe& r, int guitar, int tuning, int amp, int cab, int speaker,
                    int mic, int micPos, int micDist)
    {
        r.values.emplace_back (P::guitarType, guitar);
        r.values.emplace_back (P::tuningPreset, tuning);
        r.values.emplace_back (P::ampModel, amp);
        r.values.emplace_back (P::cabType, cab);
        r.values.emplace_back (P::cabSpeaker, speaker);
        r.values.emplace_back (P::micType, mic);
        r.values.emplace_back (P::micPosition, micPos);
        r.values.emplace_back (P::micDistance, micDist);
    }

    void addAmp (PresetRecipe& r, double gain, double bass, double mid, double treble,
                 double presence, double master)
    {
        r.values.emplace_back (P::ampGain, gain);
        r.values.emplace_back (P::ampBass, bass);
        r.values.emplace_back (P::ampMid, mid);
        r.values.emplace_back (P::ampTreble, treble);
        r.values.emplace_back (P::ampPresence, presence);
        r.values.emplace_back (P::ampMaster, master);
    }

    void addMacros (PresetRecipe& r, double attack, double body, double drive,
                    double tone, double space, double humanize)
    {
        r.values.emplace_back (P::macroAttack, attack);
        r.values.emplace_back (P::macroBody, body);
        r.values.emplace_back (P::macroDrive, drive);
        r.values.emplace_back (P::macroTone, tone);
        r.values.emplace_back (P::macroSpace, space);
        r.values.emplace_back (P::macroHumanize, humanize);
    }

    void addRoom (PresetRecipe& r, int size, int material, double blend)
    {
        r.values.emplace_back (P::roomSize, size);
        r.values.emplace_back (P::roomMaterial, material);
        r.values.emplace_back (P::roomBlend, blend);
    }

    void addPedal (PresetRecipe& r, bool post, int slot, int type,
                   std::initializer_list<double> params = {}, double mix = 1.0)
    {
        r.values.emplace_back (ParamIDs::slotType (post, slot), type);
        r.values.emplace_back (ParamIDs::slotMix (post, slot), mix);

        int index = 0;

        for (double v : params)
        {
            if (index >= Pedal::kMaxParams)
                break;

            r.values.emplace_back (ParamIDs::slotParam (post, slot, index), v);
            ++index;
        }
    }

    void addStrings (PresetRecipe& r, int material, int gauge, int age, double pluck, int pickMat)
    {
        r.values.emplace_back (P::stringMaterial, material);
        r.values.emplace_back (P::stringGauge, gauge);
        r.values.emplace_back (P::stringAge, age);
        r.values.emplace_back (P::pluckPosition, pluck);
        r.values.emplace_back (P::pickMaterial, pickMat);
    }

    //==========================================================================
    const std::vector<PresetRecipe>& buildBank()
    {
        static const std::vector<PresetRecipe> bank = []
        {
            std::vector<PresetRecipe> b;

            auto make = [&b] (const char* name, const char* category,
                              const char* description, const char* tags) -> PresetRecipe&
            {
                b.push_back ({ name, category, description, tags, {} });
                return b.back();
            };

            //==================================================================
            // ELECTRIC
            //==================================================================
            {
                auto& r = make ("Clean Double-Cut Funk", "Electric",
                                "Position 4 double-cut into a blackface combo. Tight, glassy, "
                                "and picked close to the bridge for attack.", "clean,funk,strat");
                addCommon (r, Strat, StdTune, Twin, C2x12Open, Jensen, SM57, CapEdge, CloseMic);
                addAmp (r, 0.16, 0.45, 0.40, 0.70, 0.55, 0.72);
                addMacros (r, 0.74, 0.28, 0.08, 0.64, 0.20, 0.38);
                addStrings (r, NPS, RegularG, Fresh, 0.11, Celluloid);
                addRoom (r, SmallStudio, WoodRoom, 0.16);
                r.values.emplace_back (P::pickupSelector, MidNeck);
                r.values.emplace_back (P::ampBright, 1.0);
                addPedal (r, false, 0, Comp, { 0.55, 0.25, 0.20, 0.35, 0.25, 0.0 });
            }

            {
                auto& r = make ("T-Style Country Twang", "Electric",
                                "Bridge single-coil, tweed breakup, slapback delay. Hybrid picked "
                                "near the bridge.", "country,tele,twang");
                addCommon (r, Tele, StdTune, Tweed, C1x12Open, Jensen, SM57, AxisCentre, CloseMic);
                addAmp (r, 0.34, 0.42, 0.55, 0.72, 0.62, 0.66);
                addMacros (r, 0.82, 0.24, 0.22, 0.70, 0.24, 0.42);
                addStrings (r, NPS, LightG, Fresh, 0.08, Nail);
                addRoom (r, SmallStudio, WoodRoom, 0.18);
                r.values.emplace_back (P::pickupSelector, BridgePu);
                r.values.emplace_back (P::useFingers, 1.0);
                addPedal (r, false, 0, Comp, { 0.60, 0.30, 0.15, 0.30, 0.30, 1.0 });
                addPedal (r, true, 0, DelayP, { 0.14, 0.22, 0.20, 0.55, 0.0, 0.0, 0.0 }, 0.8);
            }

            {
                auto& r = make ("Single-Cut Crunch", "Electric",
                                "Bridge humbucker into a cranked Plexi. The sound of a "
                                "4x12 in a room with the volume up.", "rock,crunch,marshall");
                addCommon (r, LesPaul, StdTune, Plexi, C4x12, Greenback, SM57, CapEdge, CloseMic);
                addAmp (r, 0.62, 0.52, 0.60, 0.56, 0.58, 0.70);
                addMacros (r, 0.55, 0.30, 0.46, 0.52, 0.26, 0.45);
                addStrings (r, NPS, RegularG, BrokenIn, 0.17, Celluloid);
                addRoom (r, LiveRoom, WoodRoom, 0.22);
                r.values.emplace_back (P::pickupSelector, BridgePu);
            }

            {
                auto& r = make ("Modern Metal Chug", "Electric",
                                "7-string into a German high-gain head, gated and tight. Palm-mute CC 67 "
                                "for the chug.", "metal,high gain,djent");
                addCommon (r, SevenStr, SevenTune, Diezel, C4x12, V30, SM57, CapEdge, CloseMic);
                addAmp (r, 0.82, 0.48, 0.34, 0.68, 0.66, 0.62);
                addMacros (r, 0.40, 0.14, 0.72, 0.56, 0.12, 0.22);
                addStrings (r, Stainless, HeavyG, Fresh, 0.07, Delrin);
                addRoom (r, IsoBooth, DryRoom, 0.08);
                r.values.emplace_back (P::pickupSelector, BridgePu);
                r.values.emplace_back (P::fretBuzz, 0.02);
                addPedal (r, false, 0, Gate, { 0.42, 0.10, 0.12, 0.18 });
                addPedal (r, false, 1, OD, { 0.22, 0.60, 0.70, 0.55, 0.0 });
                addPedal (r, true, 0, ReverbP, { 0.30, 0.18, 0.60, 0.05, 2.0, 0.10 });
            }

            {
                auto& r = make ("Jazz Hollowbody", "Electric",
                                "Semi-hollow neck pickup, flatwounds, tone rolled back. Thumb "
                                "over the fingerboard.", "jazz,warm,hollow");
                addCommon (r, ES335, StdTune, Twin, C1x12Open, AlnicoBlue, R121, Off45, MediumMic);
                addAmp (r, 0.14, 0.58, 0.60, 0.34, 0.25, 0.70);
                addMacros (r, 0.24, 0.55, 0.06, 0.30, 0.30, 0.50);
                addStrings (r, Flatwound, MediumG, BrokenIn, 0.30, Flesh);
                addRoom (r, SmallStudio, WoodRoom, 0.26);
                r.values.emplace_back (P::pickupSelector, NeckPu);
                r.values.emplace_back (P::guitarTone, 0.40);
                r.values.emplace_back (P::useFingers, 1.0);
                r.values.emplace_back (P::nailVsFlesh, 0.15);
            }

            {
                auto& r = make ("Blues Slide", "Electric",
                                "Open G resonator through a small tweed. Bottleneck mode "
                                "on: every note slides.", "blues,slide,resonator");
                addCommon (r, Resonator, OpenGTune, Champ, C1x12Open, Jensen, SM57, AxisCentre, CloseMic);
                addAmp (r, 0.58, 0.48, 0.62, 0.55, 0.40, 0.58);
                addMacros (r, 0.60, 0.80, 0.42, 0.52, 0.34, 0.62);
                addStrings (r, Bronze8020, AcMedium, BrokenIn, 0.22, SlidePick);
                addRoom (r, SmallStudio, WoodRoom, 0.30);
                r.values.emplace_back (P::slideGuitar, 1.0);
                r.values.emplace_back (P::slideNoise, 0.55);
                r.values.emplace_back (P::vibratoDepth, 32.0);
            }

            {
                auto& r = make ("Shred Lead", "Electric",
                                "RG into a Rectifier with a Floyd. Wide bend range and a "
                                "long delay for sustain.", "lead,shred,high gain");
                addCommon (r, RG, StdTune, Recto, C4x12, V30, MD421, CapEdge, CloseMic);
                addAmp (r, 0.78, 0.45, 0.48, 0.62, 0.70, 0.64);
                addMacros (r, 0.48, 0.16, 0.66, 0.58, 0.40, 0.30);
                addStrings (r, Cobalt, LightG, Fresh, 0.13, Delrin);
                addRoom (r, LargeStudio, WoodRoom, 0.20);
                r.values.emplace_back (P::playingMode, MonoMode);
                r.values.emplace_back (P::bridgeType, FloydRose);
                r.values.emplace_back (P::whammyDown, 24.0);
                r.values.emplace_back (P::feedbackOn, 1.0);
                r.values.emplace_back (P::feedbackThres, 0.55);
                addPedal (r, false, 0, OD, { 0.30, 0.55, 0.65, 0.50, 0.0 });
                addPedal (r, true, 0, DelayP, { 0.26, 0.38, 0.28, 0.50, 0.0, 5.0, 0.0 });
                addPedal (r, true, 1, ReverbP, { 0.55, 0.30, 0.45, 0.10, 1.0, 0.18 });
            }

            {
                auto& r = make ("Fuzz Face Lead", "Electric",
                                "Double-cut neck pickup into a germanium fuzz and a plexi head. "
                                "Roll the guitar volume back and it cleans up.", "fuzz,vintage,lead");
                addCommon (r, Strat, StdTune, Plexi, C4x12Vintage, Greenback, SM57, Off45, CloseMic);
                addAmp (r, 0.48, 0.55, 0.58, 0.52, 0.48, 0.66);
                addMacros (r, 0.50, 0.26, 0.40, 0.48, 0.34, 0.55);
                addStrings (r, PureNickel, RegularG, BrokenIn, 0.24, Celluloid);
                addRoom (r, LiveRoom, WoodRoom, 0.26);
                r.values.emplace_back (P::pickupSelector, NeckPu);
                addPedal (r, false, 0, Fuzz, { 0.72, 0.48, 0.40, 0.52, 0.0 });
            }

            {
                auto& r = make ("Surf Reverb", "Electric",
                                "Offset guitar, spring tank drowned, tremolo on. Pick hard "
                                "and let the trem do the rest.", "surf,vintage,spring");
                addCommon (r, Jazzmaster, StdTune, DeluxeAmp, C1x12Open, Jensen, SM57, AxisCentre, CloseMic);
                addAmp (r, 0.26, 0.50, 0.48, 0.70, 0.60, 0.68);
                addMacros (r, 0.80, 0.30, 0.16, 0.66, 0.55, 0.45);
                addStrings (r, PureNickel, MediumG, Fresh, 0.10, Celluloid);
                addRoom (r, SmallStudio, WoodRoom, 0.18);
                r.values.emplace_back (P::bridgeType, VintageTrem);
                addPedal (r, true, 0, SpringP, { 0.68, 0.45, 0.70, 0.45, 0.48 });
                addPedal (r, true, 1, TremP, { 0.30, 0.55, 1.0, 0.0, 0.0 });
            }

            {
                auto& r = make ("Semi-Hollow Chime", "Electric",
                                "Semi-hollow through a top-boost combo with both pickups on. All the "
                                "top-end sparkle the box has.", "clean,chime,vox");
                addCommon (r, ES335, StdTune, AC30, C2x12Open, AlnicoBlue, C414, CapEdge, MediumMic);
                addAmp (r, 0.34, 0.42, 0.50, 0.72, 0.30, 0.70);
                addMacros (r, 0.66, 0.50, 0.20, 0.68, 0.32, 0.48);
                addStrings (r, NPS, RegularG, Fresh, 0.16, Celluloid);
                addRoom (r, SmallStudio, WoodRoom, 0.24);
                r.values.emplace_back (P::pickupSelector, AllPu);
                addPedal (r, true, 0, ChorusP, { 0.22, 0.30, 0.45, 1.0, 0.30 });
            }

            {
                auto& r = make ("Drop C Riff", "Electric",
                                "Baritone in drop C, boutique head, tight low end. Built for "
                                "single-note riffing under a vocal.", "metal,drop,riff");
                addCommon (r, BaritoneE, DropCTune, Bogner, C4x12, V30, SM57, CapEdge, CloseMic);
                addAmp (r, 0.70, 0.42, 0.46, 0.62, 0.62, 0.64);
                addMacros (r, 0.45, 0.16, 0.60, 0.54, 0.14, 0.30);
                addStrings (r, Stainless, HeavyG, Fresh, 0.09, Delrin);
                addRoom (r, IsoBooth, DryRoom, 0.10);
                addPedal (r, false, 0, Gate, { 0.40, 0.10, 0.10, 0.16 });
                addPedal (r, false, 1, OD, { 0.20, 0.58, 0.68, 0.52, 0.0 });
            }

            {
                auto& r = make ("Wah Funk Rhythm", "Electric",
                                "Double-cut into an auto-wah and a compressor. Sixteenth-note "
                                "chord work.", "funk,wah,rhythm");
                addCommon (r, Strat, StdTune, Twin, C2x12Open, Jensen, SM57, CapEdge, CloseMic);
                addAmp (r, 0.22, 0.48, 0.45, 0.65, 0.52, 0.70);
                addMacros (r, 0.78, 0.26, 0.12, 0.60, 0.20, 0.52);
                addStrings (r, NPS, LightG, Fresh, 0.10, Celluloid);
                addRoom (r, SmallStudio, WoodRoom, 0.14);
                r.values.emplace_back (P::pickupSelector, BridgeMid);
                r.values.emplace_back (P::strumSpeed, 5.0);
                addPedal (r, false, 0, Comp, { 0.50, 0.35, 0.10, 0.25, 0.35, 1.0 });
                addPedal (r, false, 1, EnvF, { 0.60, 320.0, 3.0, 4.0, 8.0, 0.0 });
            }

            {
                auto& r = make ("Octave Fuzz Stoner", "Electric",
                                "Devil double-cut into a fuzz and a British crunch head, tuned down a whole step. "
                                "Thick and slow.", "stoner,fuzz,doom");
                addCommon (r, SG, FullDown, Orange, C4x12Vintage, G12H, MD421, AxisCentre, CloseMic);
                addAmp (r, 0.64, 0.66, 0.44, 0.50, 0.42, 0.62);
                addMacros (r, 0.38, 0.32, 0.58, 0.40, 0.42, 0.40);
                addStrings (r, NPS, HeavyG, OldStrings, 0.20, Celluloid);
                addRoom (r, LiveRoom, StoneRoom, 0.30);
                addPedal (r, false, 0, Fuzz, { 0.80, 0.40, 0.38, 0.50, 1.0 });
                addPedal (r, false, 1, Oct, { 0.45, 0.20, 0.85, 0.45 });
            }

            {
                auto& r = make ("Ambient Swell", "Electric",
                                "Volume-pedal swells into a long reverb and delay. "
                                "Freeze is armed for drones.", "ambient,pad,swell");
                addCommon (r, Strat, StdTune, Twin, C2x12Open, AlnicoBlue, C414, Off45, FarMic);
                addAmp (r, 0.12, 0.50, 0.42, 0.60, 0.40, 0.72);
                addMacros (r, 0.30, 0.42, 0.06, 0.55, 0.85, 0.35);
                addStrings (r, NPS, RegularG, BrokenIn, 0.30, Flesh);
                addRoom (r, Cathedral, StoneRoom, 0.45);
                r.values.emplace_back (P::useFingers, 1.0);
                addPedal (r, false, 0, VolP, { 0.0, 1.0, 0.0 });
                addPedal (r, true, 0, DelayP, { 0.60, 0.55, 0.42, 0.42, 2.0, 5.0, 1.0 });
                addPedal (r, true, 1, ReverbP, { 0.85, 0.70, 0.30, 0.30, 1.0, 0.48 });
            }

            {
                auto& r = make ("8-String Djent", "Electric",
                                "8-string, low F#, gated and scooped. Use palm mute "
                                "heavily.", "djent,metal,8-string");
                addCommon (r, EightStr, EightTune, Diezel, C4x12, V30, SM57, CapEdge, CloseMic);
                addAmp (r, 0.85, 0.44, 0.30, 0.70, 0.70, 0.60);
                addMacros (r, 0.42, 0.12, 0.78, 0.58, 0.10, 0.20);
                addStrings (r, Stainless, HeavyG, Fresh, 0.06, MetalPick);
                addRoom (r, IsoBooth, DryRoom, 0.06);
                addPedal (r, false, 0, Gate, { 0.36, 0.08, 0.08, 0.14 });
                addPedal (r, false, 1, OD, { 0.18, 0.62, 0.72, 0.58, 0.0 });
                addPedal (r, true, 0, GEQ, { 2.0, -4.0, -6.0, 2.0, 4.0, 2.0, 0.0 });
            }

            {
                auto& r = make ("Rockabilly Slap", "Electric",
                                "T-style bridge, tape slapback, fresh strings and a hard "
                                "pick attack.", "rockabilly,vintage,slapback");
                addCommon (r, Tele, StdTune, Tweed, C1x12Open, Jensen, SM57, AxisCentre, CloseMic);
                addAmp (r, 0.42, 0.40, 0.58, 0.72, 0.62, 0.64);
                addMacros (r, 0.88, 0.26, 0.28, 0.72, 0.22, 0.50);
                addStrings (r, PureNickel, LightG, Fresh, 0.07, Celluloid);
                addRoom (r, SmallBooth, WoodRoom, 0.14);
                addPedal (r, true, 0, DelayP, { 0.11, 0.18, 0.26, 0.42, 2.0, 0.0, 0.0 });
            }

            {
                auto& r = make ("Tapping Etude", "Electric",
                                "Mono lead voice set up for two-handed tapping: light "
                                "legato, long sustain.", "lead,tapping,technique");
                addCommon (r, RG, StdTune, JCM800, C4x12, V30, SM57, CapEdge, CloseMic);
                addAmp (r, 0.68, 0.46, 0.52, 0.60, 0.60, 0.66);
                addMacros (r, 0.42, 0.18, 0.55, 0.56, 0.32, 0.26);
                addStrings (r, Cobalt, XLight, Fresh, 0.15, Delrin);
                addRoom (r, SmallStudio, WoodRoom, 0.16);
                r.values.emplace_back (P::playingMode, MonoMode);
                r.values.emplace_back (P::legatoWindow, 90.0);
                r.values.emplace_back (P::fretAction, 1.1);
                r.values.emplace_back (P::sustainScale, 1.5);
            }

            //==================================================================
            // ACOUSTIC
            //==================================================================
            {
                auto& r = make ("Fingerstyle Folk", "Acoustic",
                                "Dreadnought, phosphor bronze, picked with the flesh of "
                                "the fingers over the soundhole.", "acoustic,fingerstyle,folk");
                addCommon (r, Dread, StdTune, AcousticDI, CabDI, Greenback, U87, AxisCentre, MediumMic);
                addAmp (r, 0.05, 0.50, 0.50, 0.52, 0.30, 0.75);
                addMacros (r, 0.42, 0.95, 0.0, 0.52, 0.32, 0.55);
                addStrings (r, PhosBronze, AcLight, BrokenIn, 0.26, Flesh);
                addRoom (r, SmallStudio, WoodRoom, 0.30);
                r.values.emplace_back (P::useFingers, 1.0);
                r.values.emplace_back (P::nailVsFlesh, 0.35);
                r.values.emplace_back (P::piezoMicBlend, 0.85);
                r.values.emplace_back (P::slideNoise, 0.45);
            }

            {
                auto& r = make ("Strummed Dreadnought", "Acoustic",
                                "Big-bodied strummer with a medium pick and a slow, "
                                "human strum.", "acoustic,strum,rhythm");
                addCommon (r, Dread, StdTune, AcousticDI, CabDI, Greenback, U87, AxisCentre, MediumMic);
                addAmp (r, 0.05, 0.52, 0.48, 0.56, 0.30, 0.75);
                addMacros (r, 0.60, 0.95, 0.0, 0.56, 0.30, 0.65);
                addStrings (r, PhosBronze, AcMedium, Fresh, 0.24, Celluloid);
                addRoom (r, SmallStudio, WoodRoom, 0.28);
                r.values.emplace_back (P::strumSpeed, 14.0);
                r.values.emplace_back (P::strumDir, 2.0);
                r.values.emplace_back (P::piezoMicBlend, 0.80);
            }

            {
                auto& r = make ("Parlor Blues", "Acoustic",
                                "Small ladder-braced parlour with old silk-and-steel "
                                "strings. Dry and woody.", "acoustic,blues,parlor");
                addCommon (r, Parlor, OpenDTune, AcousticDI, CabDI, Greenback, C414, Off45, CloseMic);
                addAmp (r, 0.05, 0.48, 0.58, 0.48, 0.25, 0.75);
                addMacros (r, 0.50, 0.92, 0.0, 0.44, 0.24, 0.70);
                addStrings (r, SilkSteel, AcLight, OldStrings, 0.28, Flesh);
                addRoom (r, SmallBooth, WoodRoom, 0.22);
                r.values.emplace_back (P::useFingers, 1.0);
                r.values.emplace_back (P::realismDetune, 8.0);
            }

            {
                auto& r = make ("12-String Jangle", "Acoustic",
                                "Six octave-paired courses, strummed bright. The chorus "
                                "is the instrument, not an effect.", "acoustic,12-string,jangle");
                addCommon (r, TwelveStr, StdTune, AcousticDI, CabDI, Greenback, U87, AxisCentre, MediumMic);
                addAmp (r, 0.05, 0.48, 0.46, 0.62, 0.30, 0.75);
                addMacros (r, 0.68, 0.95, 0.0, 0.64, 0.34, 0.60);
                addStrings (r, PhosBronze, AcLight, Fresh, 0.22, Celluloid);
                addRoom (r, SmallStudio, WoodRoom, 0.30);
                r.values.emplace_back (P::strumSpeed, 12.0);
                r.values.emplace_back (P::realismDetune, 6.0);
            }

            {
                auto& r = make ("Nylon Classical", "Classical",
                                "Cedar-topped classical, fan braced, played with the "
                                "nail. Wide dynamic range.", "classical,nylon,fingerstyle");
                addCommon (r, Classical, StdTune, AcousticDI, CabDI, Greenback, C414, AxisCentre, MediumMic);
                addAmp (r, 0.05, 0.50, 0.52, 0.48, 0.25, 0.75);
                addMacros (r, 0.40, 0.95, 0.0, 0.46, 0.34, 0.55);
                addStrings (r, NylonStr, ClassNormal, BrokenIn, 0.30, Nail);
                addRoom (r, LargeStudio, WoodRoom, 0.34);
                r.values.emplace_back (P::useFingers, 1.0);
                r.values.emplace_back (P::nailVsFlesh, 0.85);
                r.values.emplace_back (P::slideNoise, 0.12);
                r.values.emplace_back (P::piezoMicBlend, 1.0);
            }

            {
                auto& r = make ("Flamenco Rasgueado", "Classical",
                                "Thin cypress body, hard-tension fluorocarbon, fast "
                                "attack and plenty of nail.", "classical,flamenco,percussive");
                addCommon (r, Flamenco, StdTune, AcousticDI, CabDI, Greenback, C414, AxisCentre, CloseMic);
                addAmp (r, 0.05, 0.46, 0.56, 0.58, 0.30, 0.75);
                addMacros (r, 0.78, 0.95, 0.0, 0.58, 0.26, 0.72);
                addStrings (r, Fluoro, ClassHard, Fresh, 0.20, Nail);
                addRoom (r, SmallStudio, TileRoom, 0.26);
                r.values.emplace_back (P::useFingers, 1.0);
                r.values.emplace_back (P::nailVsFlesh, 1.0);
                r.values.emplace_back (P::bodyKnock, 0.35);
                r.values.emplace_back (P::strumSpeed, 4.0);
            }

            {
                auto& r = make ("Nashville High-Strung", "Acoustic",
                                "Nashville tuning: the lower four courses an octave up. "
                                "Layer it under a standard acoustic.", "acoustic,nashville,layer");
                addCommon (r, Auditorium, NashvilleTune, AcousticDI, CabDI, Greenback, C414, AxisCentre, MediumMic);
                addAmp (r, 0.05, 0.42, 0.46, 0.66, 0.30, 0.75);
                addMacros (r, 0.70, 0.92, 0.0, 0.68, 0.30, 0.55);
                addStrings (r, Bronze8020, AcLight, Fresh, 0.20, Celluloid);
                addRoom (r, SmallStudio, WoodRoom, 0.26);
            }

            {
                auto& r = make ("DADGAD Drone", "Acoustic",
                                "Modal DADGAD with the sympathetic ring pushed up. Let "
                                "the open strings do the work.", "acoustic,dadgad,modal");
                addCommon (r, Dread, DadgadTune, AcousticDI, CabDI, Greenback, U87, AxisCentre, MediumMic);
                addAmp (r, 0.05, 0.52, 0.48, 0.54, 0.30, 0.75);
                addMacros (r, 0.45, 0.95, 0.0, 0.52, 0.42, 0.60);
                addStrings (r, PhosBronze, AcLight, BrokenIn, 0.28, Flesh);
                addRoom (r, LargeStudio, WoodRoom, 0.36);
                r.values.emplace_back (P::useFingers, 1.0);
                r.values.emplace_back (P::couplingAmount, 1.0);
            }

            {
                auto& r = make ("Jumbo Bluegrass", "Acoustic",
                                "Big maple jumbo, 80/20 bronze, flatpicked hard right by "
                                "the bridge.", "acoustic,bluegrass,flatpick");
                addCommon (r, Jumbo, StdTune, AcousticDI, CabDI, Greenback, SM57, AxisCentre, CloseMic);
                addAmp (r, 0.05, 0.46, 0.54, 0.62, 0.30, 0.75);
                addMacros (r, 0.86, 0.95, 0.0, 0.62, 0.22, 0.58);
                addStrings (r, Bronze8020, AcMedium, Fresh, 0.12, Delrin);
                addRoom (r, SmallStudio, WoodRoom, 0.22);
            }

            //==================================================================
            // BASS
            //==================================================================
            {
                auto& r = make ("P-Bass Flatwound", "Bass",
                                "P-style bass with flats and the tone rolled off. Motown "
                                "in one preset.", "bass,motown,flats");
                addCommon (r, PBass, BassTune, SVT, C8x10, BassSpk, D112, AxisCentre, CloseMic);
                addAmp (r, 0.25, 0.60, 0.50, 0.35, 0.25, 0.72);
                addMacros (r, 0.34, 0.20, 0.14, 0.32, 0.12, 0.45);
                addStrings (r, Flatwound, BassStdG, OldStrings, 0.22, Flesh);
                addRoom (r, IsoBooth, DryRoom, 0.08);
                r.values.emplace_back (P::useFingers, 1.0);
                r.values.emplace_back (P::guitarTone, 0.35);
                addPedal (r, false, 0, Comp, { 0.50, 0.30, 0.15, 0.30, 0.30, 0.0 });
            }

            {
                auto& r = make ("J-Style Fingerstyle", "Bass",
                                "Both pickups, roundwounds, played over the neck pickup. "
                                "Growly and articulate.", "bass,jazz,fingerstyle");
                addCommon (r, JBass, BassTune, SVT, C4x10, BassSpk, D112, AxisCentre, CloseMic);
                addAmp (r, 0.32, 0.54, 0.48, 0.55, 0.40, 0.70);
                addMacros (r, 0.48, 0.18, 0.20, 0.50, 0.12, 0.45);
                addStrings (r, NPS, BassStdG, BrokenIn, 0.18, Flesh);
                addRoom (r, IsoBooth, DryRoom, 0.08);
                r.values.emplace_back (P::useFingers, 1.0);
                r.values.emplace_back (P::pickupSelector, AllPu);
            }

            {
                auto& r = make ("Fretless Mwah", "Bass",
                                "Flatwounds on an ebony board. Slide into everything; "
                                "the pitch is continuous.", "bass,fretless,jazz");
                addCommon (r, FretlessB, BassTune, SVT, C1x15, BassSpk, D112, AxisCentre, MediumMic);
                addAmp (r, 0.28, 0.58, 0.55, 0.42, 0.30, 0.70);
                addMacros (r, 0.34, 0.22, 0.16, 0.42, 0.18, 0.55);
                addStrings (r, Flatwound, BassStdG, BrokenIn, 0.20, Flesh);
                addRoom (r, SmallStudio, WoodRoom, 0.14);
                r.values.emplace_back (P::fretless, 1.0);
                r.values.emplace_back (P::useFingers, 1.0);
                r.values.emplace_back (P::legatoWindow, 120.0);
                r.values.emplace_back (P::vibratoDepth, 30.0);
            }

            {
                auto& r = make ("Violin Bass Grind", "Bass",
                                "Bright maple, picked hard, a little grit. Cuts through "
                                "a loud band.", "bass,rock,pick");
                addCommon (r, Rick, BassTune, SVT, C8x10, BassSpk, D112, CapEdge, CloseMic);
                addAmp (r, 0.48, 0.50, 0.46, 0.66, 0.55, 0.66);
                addMacros (r, 0.76, 0.28, 0.35, 0.62, 0.10, 0.40);
                addStrings (r, Stainless, BassStdG, Fresh, 0.10, Celluloid);
                addRoom (r, IsoBooth, DryRoom, 0.06);
                r.values.emplace_back (P::pickupSelector, AllPu);
                addPedal (r, false, 0, OD, { 0.22, 0.50, 0.55, 0.50, 2.0 }, 0.45);
            }

            {
                auto& r = make ("5-String Low B", "Bass",
                                "Extended range, fresh stainless, modern and tight. "
                                "Built for the bottom.", "bass,modern,5-string");
                addCommon (r, FiveBass, Bass5Tune, SVT, C8x10, BassSpk, D112, AxisCentre, CloseMic);
                addAmp (r, 0.36, 0.56, 0.42, 0.62, 0.50, 0.68);
                addMacros (r, 0.60, 0.16, 0.22, 0.56, 0.08, 0.35);
                addStrings (r, Stainless, BassHeavyG, Fresh, 0.14, Flesh);
                addRoom (r, IsoBooth, DryRoom, 0.05);
                r.values.emplace_back (P::useFingers, 1.0);
                addPedal (r, false, 0, Comp, { 0.45, 0.40, 0.10, 0.25, 0.30, 1.0 });
                addPedal (r, true, 0, PEQ, { 80.0, 2.0, 700.0, -2.5, 1.2, 3500.0, 3.0, 30.0, 12000.0 });
            }

            //==================================================================
            // SHOWCASE / UTILITY
            //==================================================================
            {
                auto& r = make ("Init", "Utility",
                                "Everything at its default. The starting point for "
                                "building your own.", "init,default");
                // Deliberately empty: this preset is the defaults.
                r.values.emplace_back (P::guitarType, Strat);
            }

            {
                auto& r = make ("Dry Instrument", "Utility",
                                "No amp, no cabinet, no room. The raw instrument, for "
                                "reamping or for using your own rig.", "dry,di,utility");
                addCommon (r, Strat, StdTune, AcousticDI, CabDI, Greenback, U87, AxisCentre, CloseMic);
                addAmp (r, 0.05, 0.5, 0.5, 0.5, 0.0, 0.8);
                addMacros (r, 0.5, 0.25, 0.0, 0.5, 0.0, 0.35);
                r.values.emplace_back (P::cabOn, 0.0);
                r.values.emplace_back (P::roomOn, 0.0);
                r.values.emplace_back (P::cableOn, 0.0);
            }

            {
                auto& r = make ("Physics Showcase", "Utility",
                                "Every physical behaviour turned up: sympathetic ring, "
                                "string noise, buzz, drift and inharmonicity.", "demo,physics");
                addCommon (r, Dread, StdTune, AcousticDI, CabDI, Greenback, U87, AxisCentre, MediumMic);
                addAmp (r, 0.05, 0.5, 0.5, 0.52, 0.3, 0.75);
                addMacros (r, 0.55, 1.0, 0.0, 0.52, 0.35, 1.0);
                addStrings (r, PhosBronze, AcMedium, OldStrings, 0.26, Flesh);
                addRoom (r, LiveRoom, WoodRoom, 0.32);
                r.values.emplace_back (P::couplingAmount, 1.0);
                r.values.emplace_back (P::slideNoise, 0.8);
                r.values.emplace_back (P::fretNoise, 0.7);
                r.values.emplace_back (P::releaseNoise, 0.6);
                r.values.emplace_back (P::bodyKnock, 0.4);
                r.values.emplace_back (P::fretBuzz, 0.30);
                r.values.emplace_back (P::fretAction, 1.0);
                r.values.emplace_back (P::realismDetune, 12.0);
                r.values.emplace_back (P::tuningDrift, 1.0);
                r.values.emplace_back (P::useFingers, 1.0);
            }

            {
                auto& r = make ("Transposing Trem Chords", "Utility",
                                "Steinberger-style bridge: bend the bar and the chord "
                                "stays in tune. Try it with the whammy parameter.", "demo,transtrem");
                addCommon (r, CustomGtr, StdTune, Twin, C2x12Open, Jensen, SM57, CapEdge, CloseMic);
                addAmp (r, 0.20, 0.5, 0.5, 0.6, 0.5, 0.7);
                addMacros (r, 0.6, 0.25, 0.12, 0.55, 0.25, 0.30);
                r.values.emplace_back (P::bridgeType, TransTrem);
                r.values.emplace_back (P::whammyDown, 12.0);
                r.values.emplace_back (P::whammyUp, 5.0);
            }

            {
                auto& r = make ("Microtonal Just", "Utility",
                                "Just intonation on an acoustic. Open chords ring "
                                "perfectly still; move key and they will not.", "demo,temperament,microtonal");
                addCommon (r, Auditorium, StdTune, AcousticDI, CabDI, Greenback, C414, AxisCentre, MediumMic);
                addAmp (r, 0.05, 0.5, 0.5, 0.52, 0.3, 0.75);
                addMacros (r, 0.45, 0.95, 0.0, 0.5, 0.3, 0.25);
                addStrings (r, PhosBronze, AcLight, Fresh, 0.26, Flesh);
                r.values.emplace_back (P::temperament, 1.0);   // Just
                r.values.emplace_back (P::realismDetune, 0.0);
                r.values.emplace_back (P::useFingers, 1.0);
            }

            return b;
        }();

        return bank;
    }
}

//==============================================================================
int FactoryPresets::getNumPresets() noexcept
{
    return (int) buildBank().size();
}

const FactoryPresets::Definition& FactoryPresets::getPreset (int index) noexcept
{
    // The Definition view is rebuilt on demand from the recipe table.
    static thread_local Definition def {};
    static thread_local std::vector<Entry> entries;
    static thread_local juce::String name, category, description, tags;

    const auto& bank = buildBank();
    const auto& recipe = bank[(size_t) juce::jlimit (0, (int) bank.size() - 1, index)];

    name = recipe.name;
    category = recipe.category;
    description = recipe.description;
    tags = recipe.tags;

    entries.clear();
    entries.reserve (recipe.values.size());

    for (const auto& v : recipe.values)
        entries.push_back ({ v.first.toRawUTF8(), v.second });

    def.name = name.toRawUTF8();
    def.category = category.toRawUTF8();
    def.description = description.toRawUTF8();
    def.tags = tags.toRawUTF8();
    def.entries = entries.data();
    def.numEntries = (int) entries.size();

    return def;
}

//==============================================================================
juce::var FactoryPresets::toVar (const Definition& def, const juce::AudioProcessor& processor)
{
    auto* root = new juce::DynamicObject();

    // file-formats 1: the canonical marker, the same one PresetManager writes.
    root->setProperty ("magic", PresetManager::kMagic);
    root->setProperty ("schemaVersion", PresetManager::kSchemaVersion);
    root->setProperty ("pluginVersion", JucePlugin_VersionString);
    root->setProperty ("name", def.name);
    root->setProperty ("category", def.category);
    root->setProperty ("author", "Luthier Audio");
    root->setProperty ("description", def.description);

    juce::Array<juce::var> tagArray;

    for (const auto& t : juce::StringArray::fromTokens (juce::String (def.tags), ",", ""))
        if (t.trim().isNotEmpty())
            tagArray.add (t.trim());

    root->setProperty ("tags", tagArray);

    // Start from every parameter's default, then apply the recipe's overrides.
    auto* params = new juce::DynamicObject();

    for (auto* p : processor.getParameters())
    {
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            params->setProperty (withId->paramID, (double) p->getDefaultValue());
    }

    for (int i = 0; i < def.numEntries; ++i)
    {
        const juce::String id (def.entries[i].paramId);

        for (auto* p : processor.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);

            if (ranged == nullptr || ranged->paramID != id)
                continue;

            const float normalised = ranged->getNormalisableRange()
                                            .convertTo0to1 ((float) def.entries[i].plainValue);

            params->setProperty (id, (double) juce::jlimit (0.0f, 1.0f, normalised));
            break;
        }
    }

    root->setProperty ("parameters", juce::var (params));

    return juce::var (root);
}

//==============================================================================
void FactoryPresets::writeAll (const juce::File& folder)
{
    if (rangeSource == nullptr)
        return;

    folder.createDirectory();

    /*  Factory presets renamed since an earlier install (the trademark sweep,
        DECISIONS 2g). Their old files are moved aside rather than deleted, so a
        user who edited one can still find it; the folder is not scanned. */
    static const char* const retired[][2] = {
        { "Electric", "Clean Strat Funk" }, { "Electric", "Tele Country Twang" },
        { "Electric", "Les Paul Crunch" },  { "Bass", "Jazz Bass Fingerstyle" },
        { "Bass", "Rickenbacker Grind" },   { "Electric", "TransTrem Chords" },
    };

    for (const auto& r : retired)
    {
        for (const auto& category : { juce::String (r[0]), juce::String ("Bass"), juce::String ("Electric") })
        {
            const auto old = folder.getChildFile (category).getChildFile (juce::String (r[1]) + ".luthierpreset");

            if (old.existsAsFile())
            {
                const auto aside = folder.getParentDirectory().getChildFile ("Retired Factory Presets");
                aside.createDirectory();
                old.moveFileTo (aside.getNonexistentChildFile (old.getFileNameWithoutExtension(), ".luthierpreset", false));
            }
        }
    }

    for (int i = 0; i < getNumPresets(); ++i)
    {
        const auto& def = getPreset (i);

        auto file = folder.getChildFile (juce::File::createLegalFileName (def.category))
                          .getChildFile (juce::File::createLegalFileName (def.name) + ".luthierpreset");

        // Never overwrite: a user who edited a factory preset keeps their edit.
        if (file.existsAsFile())
            continue;

        file.getParentDirectory().createDirectory();

        const auto data = toVar (def, *rangeSource);
        file.replaceWithText (juce::JSON::toString (data, false));
    }
}

} // namespace luthier
