#pragma once

/*  The Jam drum kit (jam-mode.md 5): the pieces, a fixed voice pool, the kit
    styles, panning by perspective and width, and the room send.

    Voice pool (fixed): kick 2, snare 2, each tom 2, hat 1, ride 1, crash 1,
    rim 1, shaker 1. A two-voice piece with both voices ringing steals the
    oldest: its state moves to a tail slot that fades out over 2 ms while the
    new hit starts on time in the voice it left, so a steal never delays a
    hit and never clicks.

    Everything is set on the audio thread by JamEngine (kit changes land at a
    bar line, tuning and damping at each piece's next hit). No allocation after
    prepare: the room's delay lines are sized there for the highest rate.
*/

#include "DrumPieces.h"
#include "KitRoom.h"

namespace luthier
{

/** What the kit can play. The MIDI note for each is in getGmNote. */
enum class DrumSound
{
    kick = 0, snare, snareGhost, snareRim, snareBrush,
    tomHigh, tomMid, tomFloor,
    hatClosed, hatOpen, hatPedal,
    ride, rideBell, crash,
    rim, sticks, shaker,
    numSounds
};

/** jam-mode 9: the GM drum map (kick 36, rim 37, snare 38, closed hat 42,
    pedal hat 44, open hat 46, toms 45 / 47 / 50, crash 49, ride 51, bell 53,
    shaker 82). */
int getGmNote (DrumSound sound) noexcept;

/** Plain name for the lane view and tests. */
const char* getDrumSoundName (DrumSound sound) noexcept;

enum class JamKitStyle { studio = 0, vintage, arena, jazz, machine, numKits };
const char* getJamKitName (int kit) noexcept;

class JamDrumKit
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setSeed (uint64_t seed) noexcept;

    /** Kit style (5's table). Audio thread, at a bar line. */
    void setKit (int kitIndex) noexcept;
    int getKit() const noexcept { return kit; }

    /** jam_kit_tuning (semitones) and jam_kit_damping (0..1): each piece's
        next hit. */
    void setTuning (double semitones, double damping01) noexcept;

    void setRoom (double amount01) noexcept    { roomSend = juce::jlimit (0.0, 1.0, amount01); }
    void setWidth (double width01) noexcept    { width = juce::jlimit (0.0, 1.0, width01); }
    void setPerspective (bool drummer) noexcept { drummerView = drummer; }

    /** CPU relief step between 4 and 5 (jam-mode 13). */
    void setReduced (bool reduced) noexcept;

    /** Strikes a sound now (the next rendered sample). */
    void trigger (DrumSound sound, double velocity) noexcept;

    /** A hand on every piece: fades the whole kit out over `seconds` and then
        clears it (panic 5 ms, a cut 20 ms). */
    void chokeAll (double seconds) noexcept;

    /** Chokes the crash and ride (the ending's "choked after ring time"). */
    void chokeCymbals (double seconds) noexcept;

    /** Renders `n` samples into L / R (overwrites). */
    void render (double* left, double* right, int n) noexcept;

    bool isSilent() const noexcept;

    /** Output peak of the last render, for the meters. */
    double getLastPeak() const noexcept { return lastPeak; }

    //==========================================================================
    // For the DSP tests (JM-24 to JM-28).
    MembranePiece& getKickVoice (int i) noexcept   { return kick[(size_t) juce::jlimit (0, 1, i)]; }
    SnarePiece&    getSnareVoice (int i) noexcept  { return snare[(size_t) juce::jlimit (0, 1, i)]; }
    CymbalPiece&   getHat() noexcept    { return hat; }
    CymbalPiece&   getRide() noexcept   { return ride; }
    CymbalPiece&   getCrash() noexcept  { return crash; }

    /** The kick's resting f0 for the current kit and tuning. */
    double getKickF0() const noexcept;

private:
    struct Pan { double left = 0.707, right = 0.707; };
    Pan panFor (DrumSound sound) const noexcept;
    void applyKit() noexcept;

    double sr = 48000.0;
    int kit = 0;
    uint64_t seed = 4849997;
    double tuningSemitones = 0.0, damping = 0.4;
    double roomSend = 0.25, width = 0.7;
    bool drummerView = false;
    int64_t hitCounter = 0;

    std::array<MembranePiece, 2> kick;
    std::array<int64_t, 2> kickAge {};
    MembranePiece kickTail;
    double kickTailGain = 0.0;

    std::array<SnarePiece, 2> snare;
    std::array<int64_t, 2> snareAge {};
    SnarePiece snareTail;
    double snareTailGain = 0.0;

    std::array<std::array<MembranePiece, 2>, 3> toms;
    std::array<std::array<int64_t, 2>, 3> tomAge {};
    std::array<MembranePiece, 3> tomTail;
    std::array<double, 3> tomTailGain {};

    double tailStep = 0.0;

    CymbalPiece hat, ride, crash;
    RimPiece rim;
    ShakerPiece shaker;

    KitRoom room;
    std::array<Pan, (size_t) DrumSound::numSounds> pans {};

    double fadeGain = 1.0, fadeStep = 0.0;
    bool fading = false;
    int housekeepCountdown = 0;
    double lastPeak = 0.0;
};

} // namespace luthier
