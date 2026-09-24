#pragma once

/*  preset-browser-previews.md 6.1: the parameter features of a preset, read
    straight from its JSON without loading it.

    Used by the index (search, filters, similarity), by the descriptor rules and
    by the preview renderer's phrase choice, so all three agree on what "drive"
    or "bass family" means for a given file.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>

namespace luthier
{

struct PresetFeatures
{
    enum Family { electric = 0, acoustic, classical, bass, numFamilies };
    enum Pickup { singleCoil = 0, humbucker, piezo, numPickups };

    /** gui-techniques-updates.md 2's six pills, in pill order. */
    enum Technique { scrape = 0, slide, slap, mute, tap, bend, numTechniques };

    double drive = 0.0;         ///< 0-1: amp gain by the model's gain class, plus drive pedals
    double reverb = 0.0;        ///< room blend x decay, plus reverb pedal mix
    double delay = 0.0;         ///< delay pedal mix
    double compression = 0.0;   ///< compressor pedal amount
    double roomBlend = 0.0;

    int family = electric;
    int bodyShape = 0;          ///< BodyShape
    int pickup = singleCoil;
    bool neckHumbucker = false;
    bool hollowOrArchtop = false;
    bool acousticBody = false;

    bool nylon = false, flat = false, steel = true;

    int numStrings = 6;
    int lowestOpenMidi = 40;
    int highestMidi = 88;

    bool slideOn = false;
    bool fuzzPedal = false;
    bool rhythmEngineOn = false;
    std::array<bool, numTechniques> techniques {};

    int guitarType = 0;
    int ampModel = 0;
    juce::String guitarName, ampName;

    bool usesAnyTechnique() const noexcept
    {
        for (bool b : techniques)
            if (b)
                return true;

        return false;
    }

    static const char* getFamilyName (int family) noexcept;
    static const char* getTechniqueName (int technique) noexcept;

    /** The parameter id each technique's arm flag lives in, or nullptr for a
        technique that has no arm parameter in this build. */
    static const char* getTechniqueParamId (int technique) noexcept;
};

//==============================================================================
/** Normalised-to-plain conversion through a processor's own parameter ranges,
    looked up once. Construct on the message thread; read from any thread. */
class PresetFeatureReader
{
public:
    explicit PresetFeatureReader (const juce::AudioProcessor& rangeSource);

    PresetFeatures read (const juce::var& presetJson) const;

    /** The plain value of one parameter as the preset stores it, or the
        parameter's default when the preset does not mention it. */
    double plain (const juce::var& presetJson, const juce::String& paramId) const;

    bool hasParameter (const juce::String& paramId) const
    {
        return ranges.find (paramId.toStdString()) != ranges.end();
    }

private:
    struct Range
    {
        juce::NormalisableRange<float> range;
        float defaultNormalised = 0.0f;
    };

    std::unordered_map<std::string, Range> ranges;
};

} // namespace luthier
