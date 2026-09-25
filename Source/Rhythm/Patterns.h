#pragma once

/*  Strum and fingerpick patterns, and the library that holds them
    (rhythm-engine.md sections 4, 5 and 6).

    A pattern is data, not behaviour: a fixed-size array of steps, each either a
    strum event or a rest. The schedulers in RhythmEngine read them; nothing here
    touches the audio thread except by being read from it, and a pattern is never
    resized once it is in the library.
*/

#include "../DSP/Common/DspCommon.h"
#include "Muting.h"

#include <array>
#include <vector>

namespace luthier
{

//==============================================================================
/** What one step of a strum pattern does (rhythm-engine 4). */
enum class StrumType
{
    rest = 0,
    down,
    up,
    downMute,
    upMute,
    rake,
    rasgueado,
    chuck,      ///< strum-dynamics 6.1: a strum with the fretting hand flat on the strings. Appended.
    numTypes
};

const char* getStrumTypeName (StrumType type) noexcept;

/** True for the types that set the palm-mute technique flag. */
inline bool isMutedStrum (StrumType type) noexcept
{
    return type == StrumType::downMute || type == StrumType::upMute
             || type == StrumType::rake;
}

/** True for the types that travel from the low strings upward. */
inline bool isDownStroke (StrumType type) noexcept
{
    return type == StrumType::down || type == StrumType::downMute
             || type == StrumType::rake || type == StrumType::chuck;
}

//==============================================================================
/** Which finger plays a string in a fingerpick pattern. */
enum class Finger { thumb = 0, index, middle, ring, little, numFingers };

const char* getFingerName (Finger finger) noexcept;
Finger fingerFromLetter (juce::juce_wchar letter) noexcept;

//==============================================================================
/** The grid a pattern's steps sit on. */
enum class Subdivision
{
    eighth = 0, eighthTriplet, sixteenth, sixteenthTriplet, thirtySecond,
    numSubdivisions
};

const char* getSubdivisionName (Subdivision s) noexcept;

/** How many of these fit in one beat (quarter note). */
double subdivisionsPerBeat (Subdivision s) noexcept;

//==============================================================================
struct StrumStep
{
    StrumType type = StrumType::rest;
    double dynamic = 1.0;          ///< 0..1, scales velocity.
    uint16_t stringMask = 0x0FFF;  ///< Bit n set means string n takes part.
    double crossingSps = 0.0;      ///< ambiguity-resolutions 6: a step's own override; 0 follows the pattern.

    bool isRest() const noexcept { return type == StrumType::rest; }
};

struct FingerpickStep
{
    bool active = false;
    Finger finger = Finger::thumb;
    double dynamic = 1.0;
};

//==============================================================================
/** A strum or fingerpick pattern. Both kinds share a length, a subdivision and
    a swing amount; only the step payload differs. */
class RhythmPattern
{
public:
    static constexpr int kMaxSteps = 32;

    enum class Kind { strum = 0, fingerpick };

    RhythmPattern();

    void clear() noexcept;

    const juce::String& getName() const noexcept { return name; }
    void setName (const juce::String& n) { name = n; }

    Kind getKind() const noexcept { return kind; }
    void setKind (Kind k) noexcept { kind = k; }

    int getLength() const noexcept { return length; }
    void setLength (int steps) noexcept { length = juce::jlimit (1, kMaxSteps, steps); }

    Subdivision getSubdivision() const noexcept { return subdivision; }
    void setSubdivision (Subdivision s) noexcept { subdivision = s; }

    /** 0.5 is straight; above that the offbeats are pushed later. */
    double getSwing() const noexcept { return swing; }
    void setSwing (double s) noexcept { swing = juce::jlimit (0.5, 0.75, s); }

    /** ambiguity-resolutions 6: the pattern's crossing velocity in strings per
        second, or 0 when it names none (the kit's, then the global, apply). */
    double getCrossingSps() const noexcept { return crossingSps; }
    void setCrossingSps (double sps) noexcept { crossingSps = sps > 0.0 ? juce::jlimit (20.0, 800.0, sps) : 0.0; }

    const juce::StringArray& getTags() const noexcept { return tags; }
    void setTags (const juce::StringArray& t) { tags = t; }

    StrumStep getStrumStep (int index) const noexcept;
    void setStrumStep (int index, const StrumStep& step) noexcept;

    FingerpickStep getFingerpickStep (int index) const noexcept;
    void setFingerpickStep (int index, const FingerpickStep& step) noexcept;

    /** muting-rhythm.md 2: each step's mute, whatever the pattern's kind. Open by default. */
    MuteStep getMuteStep (int index) const noexcept;
    void setMuteStep (int index, const MuteStep& step) noexcept;

    /** Which string each finger plays. Only meaningful for fingerpick patterns. */
    int getStringForFinger (Finger finger) const noexcept;
    void setStringForFinger (Finger finger, int stringIndex) noexcept;

    bool isEmpty() const noexcept;

    //==========================================================================
    juce::var toVar() const;
    static RhythmPattern fromVar (const juce::var& state);

    /** Parses a `.luthierpattern` file. Returns false and leaves the pattern
        untouched if the file is missing or malformed. */
    bool loadFrom (const juce::File& file);
    bool saveTo (const juce::File& file) const;

private:
    juce::String name { "Untitled" };
    Kind kind = Kind::strum;
    int length = 16;
    Subdivision subdivision = Subdivision::sixteenth;
    double swing = 0.5;
    double crossingSps = 0.0;
    juce::StringArray tags;

    std::array<StrumStep, kMaxSteps> strumSteps {};
    std::array<FingerpickStep, kMaxSteps> fingerpickSteps {};
    std::array<MuteStep, kMaxSteps> muteSteps {};

    /** Default assignment is the classical one: thumb on the bass strings,
        i/m/a on the top three. */
    std::array<int, (size_t) Finger::numFingers> fingerStrings { { 5, 2, 1, 0, 0 } };
};

//==============================================================================
/** Holds the factory patterns and any the user has saved
    (rhythm-engine 5 and 6). */
class PatternLibrary
{
public:
    PatternLibrary();

    /** Builds the factory patterns and scans both pattern directories. Message
        thread only. */
    void refresh();

    int getNumPatterns() const noexcept { return (int) patterns.size(); }
    const RhythmPattern& getPattern (int index) const noexcept;

    /** Index of the first pattern with this name, or -1. */
    int indexOf (const juce::String& patternName) const;

    /** Indices of every pattern carrying this tag. */
    juce::Array<int> findByTag (const juce::String& tag) const;

    juce::Array<int> findByKind (RhythmPattern::Kind kind) const;

    /** Adds or replaces a pattern by name, and writes it to the user directory. */
    bool save (const RhythmPattern& pattern);

    static juce::File getFactoryDirectory();
    static juce::File getUserDirectory();

private:
    void addFactoryPatterns();
    void scanDirectory (const juce::File& directory);

    std::vector<RhythmPattern> patterns;
};

} // namespace luthier
