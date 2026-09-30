#pragma once

/*  Jam styles (jam-mode.md 4 and 12): the grooves a style plays, and the
    library that holds the factory styles and the user's.

    A pattern is one bar (or a fill's tail of a bar) as step strings, one per
    drum lane plus the bass lane, exactly as `.luthierjam` stores them:

      drums  `.` rest, `g` ghost (30), `x` hit (90), `X` accent (118),
             `?` a 50 % chance of 90 (drawn from the bar's seed),
             hat `o` / `O` open, `p` pedal chick; ride `b` bell;
             snare `r` rim shot, `b` brush sweep; toms `1` `2` `3` (high, mid,
             floor) and `H` `M` `F` accented; perc `r` cross-stick, `s` / `S`
             shaker.
      bass   `R 3 5 7 8` chord tones, `A` approach, `W` walking step,
             `m` muted ghost, `-` tie, `.` rest.

    A style has a set of patterns per meter it supports (every style 4/4;
    Country and Ballad 3/4; Ballad 6/8): variations A and B with a groove per
    intensity 1-5, at least four fills (1 beat, 2 beats, 1 bar, 1 bar big),
    an ending, and a double-time and a half-time bar.

    Styles are immutable once built. The audio thread reads them through
    pointers the library keeps alive (JamEngine::setStyleSlot); they are built,
    parsed and freed on the message thread only.
*/

#include <juce_core/juce_core.h>
#include <array>
#include <memory>
#include <vector>

namespace luthier
{

namespace jam
{
    enum Lane { kick = 0, snare, hat, ride, crash, tom, perc, numDrumLanes };
    const char* getLaneName (int lane) noexcept;

    /** Longest bar: 7/4 at a 16 grid, or a 12/8 at a 12 grid, with room. */
    constexpr int kMaxSteps = 32;
    constexpr int kMaxFills = 8;
    constexpr int kNumIntensities = 5;
    constexpr int kNumFactoryStyles = 10;
    constexpr int kUserStyleIndex = 10;
    constexpr int kNumStyleChoices = 11;

    enum class Follow { tight = 0, natural, relaxed, bar };
}

//==============================================================================
struct JamPattern
{
    int steps = 0;
    std::array<std::array<char, jam::kMaxSteps>, jam::numDrumLanes> drums {};
    std::array<char, jam::kMaxSteps> bass {};
    bool hasBass = false;   ///< a fill without bass keeps the groove's line

    char drum (int lane, int step) const noexcept
    {
        return (step >= 0 && step < steps) ? drums[(size_t) lane][(size_t) step] : '.';
    }

    char bassAt (int step) const noexcept
    {
        return (hasBass && step >= 0 && step < steps) ? bass[(size_t) step] : '-';
    }

    /** Sets a lane from its step string. Returns false if the length is not
        `steps` (13, a malformed file). */
    bool setLane (int lane, const juce::String& text);
    bool setBass (const juce::String& text);
    juce::String laneString (int lane) const;
    juce::String bassString() const;
    void clear (int numSteps) noexcept;
};

struct JamFill
{
    int beats = 1;
    bool big = false;
    JamPattern pattern;   ///< `beats` worth of steps, ending at the bar line
};

struct JamMeterSet
{
    int numerator = 4, denominator = 4;
    bool present = false;

    /** [variation A / B][intensity 1-5 as 0-4] */
    std::array<std::array<JamPattern, jam::kNumIntensities>, 2> grooves {};
    std::array<JamFill, jam::kMaxFills> fills {};
    int numFills = 0;
    JamPattern ending, doubleTime, halfTime;

    /** Steps in one bar of this meter at `stepsPerQuarter`. */
    int barSteps (int stepsPerQuarter) const noexcept
    {
        return (int) std::round (numerator * 4.0 / (double) denominator * stepsPerQuarter);
    }
};

//==============================================================================
struct JamStyle
{
    juce::String name;
    juce::String fallbackStyle;    ///< the factory style a user file names (13)
    int grid = 16;                 ///< 16 (straight 16ths) or 12 (triplet 8ths)
    double swing = 0.0;            ///< 0-0.5 of a step pair, for 16 grids
    int kit = 0;                   ///< JamKitStyle
    int bassVoice = 0;             ///< JamBassVoiceKind
    int follow = (int) jam::Follow::natural;
    juce::String rhythmKit;        ///< the RhythmEngine genre kit it suggests (4.1)

    std::array<JamMeterSet, 3> meters {};

    int stepsPerQuarter() const noexcept { return grid == 12 ? 3 : 4; }

    /** The meter set for a time signature, or nullptr (the generic bar). */
    const JamMeterSet* findMeter (int numerator, int denominator) const noexcept;

    /** JM-01: A/B x 1-5 grooves, >= 4 fills of the four sizes, an ending,
        double-time and half-time bars, every lane as long as its grid. */
    bool isComplete (juce::String* whyNot = nullptr) const;

    //==========================================================================
    // `.luthierjam` (jam-mode 12, file-formats 1).
    juce::var toVar() const;

    /** Parses a style. False, with `error` set, when it is malformed; the
        `style` field (meta.style) is still read into fallbackStyle if present. */
    static bool fromVar (const juce::var& v, JamStyle& out, juce::String& error);

    static constexpr const char* kMagic = "luthier.jam";
    static constexpr int kSchema = 1;
    static constexpr const char* kFileExtension = ".luthierjam";
};

//==============================================================================
/** The factory styles and the user's (jam-mode 12). Message thread. */
class JamStyleLibrary
{
public:
    JamStyleLibrary();

    static const char* getFactoryStyleName (int index) noexcept;
    static juce::StringArray getStyleChoiceNames();

    /** Builds the ten factory styles in code (JamStyleLibrary::addFactoryStyles). */
    static std::vector<std::unique_ptr<JamStyle>> buildFactoryStyles();

    const JamStyle* getFactoryStyle (int index) const noexcept;
    int findFactoryStyle (const juce::String& name) const noexcept;

    /** Overrides factory styles by name from a folder (Resources/Jam/). */
    int loadOverrides (const juce::File& folder);

    /*  Loads a user style file. A malformed or missing file falls back to the
        factory style its `style` field names, else Rock (13): the result is
        then that factory style, `warning` says so and the return is false.
        The loaded style stays owned by the library for its lifetime, so the
        audio thread can keep a pointer to it. */
    const JamStyle* loadUserStyle (const juce::File& file, juce::String& warning);

    /** Writes a style atomically (file-formats 13): a temp file, then a rename. */
    static bool saveStyle (const JamStyle& style, const juce::File& file, juce::String& error);

    /** ~/Documents/Luthier/Jam/ */
    static juce::File getUserFolder();

    /** Resources/Jam/ beside the binary, if it is there. */
    static juce::File getFactoryOverrideFolder();

private:
    std::vector<std::unique_ptr<JamStyle>> factory;
    std::vector<std::unique_ptr<JamStyle>> userStyles;   ///< never freed while the library lives
};

} // namespace luthier
