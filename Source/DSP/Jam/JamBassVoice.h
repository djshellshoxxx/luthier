#pragma once

/*  The Jam bass (jam-mode.md 6): a dedicated voice, not a second
    LuthierEngine (6.1 records why - about a tenth of the cost).

    - Two StringEngine waveguides, ping-ponged: a new note plucks the idle
      one, the previous one gets Damping::Released (the finger lifting), so a
      change never clicks and never cuts a tail short.
    - Four strings, E1 A1 D2 G2, 864 mm scale, roundwound .105-.045 through
      StringMaterials. The note goes on the string giving the lowest fret <= 7
      nearest the previous position, and the idle waveguide takes that string's
      physics (setPhysical) before the pluck.
    - Finger, pick or palm-muted pick (Damping::PalmMuteBass); Upright is a
      flesh pluck at 0.18 of the scale with more loop damping.
    - JamBassTone: the pickup's position comb at 0.21 of the scale, a 2-pole
      pickup resonance at 4.5 kHz (Q 1.2), a 3-band tone and tube saturation
      at 2x oversampling. Upright swaps the pickup for two body modes
      (95 and 180 Hz).

    Audio thread; no allocation after prepare.
*/

#include "../String/StringEngine.h"
#include "../Common/Oversampler.h"
#include <vector>

namespace luthier
{

enum class JamBassVoiceKind { finger = 0, pick, mutedPick, upright, numKinds };
const char* getJamBassVoiceName (int kind) noexcept;

//==============================================================================
class JamBassTone
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    void setUpright (bool isUpright) noexcept;
    void setTone (double tone01) noexcept;

    /** The pickup sits 0.21 of the scale from the bridge; on a note of this
        period and fret, its comb delay is 0.21 x 2^(fret/12) periods. */
    void setNote (double hz, int fret) noexcept;

    double process (double x) noexcept;

private:
    double sr = 48000.0;
    bool upright = false;
    double tone = 0.5;

    std::vector<double> combLine;
    int combWrite = 0;
    double combDelay = 100.0;

    Biquad pickupResonance, lowBand, midBand, highBand, bodyA, bodyB;
    HalfbandStage upStage, downStage;
    DCBlocker dc;
};

//==============================================================================
class JamBassVoice
{
public:
    static constexpr int kNumStrings = 4;
    static constexpr int kOpenNotes[kNumStrings] = { 28, 33, 38, 43 };   ///< E1 A1 D2 G2
    static constexpr double kScaleMm = 864.0;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setVoice (JamBassVoiceKind kind) noexcept;
    JamBassVoiceKind getVoice() const noexcept { return voice; }
    void setTone (double tone01) noexcept { tone.setTone (tone01); }

    /** Plucks `midiNote` now; `ghost` is the `m` token's muted ghost. */
    void noteOn (int midiNote, double velocity, bool ghost) noexcept;

    /** The finger lifts: the sounding note is damped. */
    void noteOff() noexcept;

    /** Fades everything out over `seconds` (panic, a cut). */
    void choke (double seconds) noexcept;

    void render (double* out, int n) noexcept;

    bool isSounding() const noexcept;
    int getCurrentNote() const noexcept { return currentNote; }
    int getLastString() const noexcept { return lastString; }
    int getLastFret() const noexcept { return lastFret; }

    /** Which of the two waveguides the last note went to (JM-30). */
    int getActiveInstance() const noexcept { return active; }

    double getLastPeak() const noexcept { return lastPeak; }

    /** 6.1: the string and fret a bassist would choose for `note`, given the
        previous fret. Public for the tests. */
    static void chooseString (int note, int previousFret, int& string, int& fret) noexcept;

    const StringEngine& getString (int i) const noexcept { return strings[(size_t) juce::jlimit (0, 1, i)]; }

private:
    double sr = 48000.0;
    JamBassVoiceKind voice = JamBassVoiceKind::finger;
    std::array<StringEngine, 2> strings;
    std::array<StringEngine::Physical, kNumStrings> physics {};
    JamBassTone tone;

    int active = 0;
    int currentNote = -1, lastString = 1, lastFret = 0;
    bool sounding = false;

    double fadeGain = 1.0, fadeStep = 0.0;
    bool fading = false;
    double lastPeak = 0.0;
};

} // namespace luthier
