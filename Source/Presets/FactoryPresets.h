#pragma once

/*  The factory bank.

    Each preset is a list of parameter overrides on top of the defaults, written
    in real units (a fret position in frets, a gain in dB, a choice by its index)
    and converted to normalised values through each parameter's own range at write
    time. That keeps the table readable and stops the bank from silently rotting
    if a parameter's range is ever adjusted.
*/

#include <juce_audio_processors/juce_audio_processors.h>

namespace luthier
{

class FactoryPresets
{
public:
    struct Entry
    {
        const char* paramId;
        double plainValue;
    };

    struct Definition
    {
        const char* name;
        const char* category;
        const char* description;
        const char* tags;          ///< Comma separated.
        const Entry* entries;
        int numEntries;
    };

    static int getNumPresets() noexcept;
    static const Definition& getPreset (int index) noexcept;

    /** Writes any missing factory presets into `folder`, organised by category.
        Existing files are left alone so a user who edited one keeps their edit. */
    static void writeAll (const juce::File& folder);

    /** Builds one preset's JSON using a live processor for the parameter ranges. */
    static juce::var toVar (const Definition& def, const juce::AudioProcessor& processor);

    /** Called once at startup by the processor so writeAll has ranges to work with. */
    static void setProcessorForRanges (const juce::AudioProcessor* processor) noexcept;

private:
    static const juce::AudioProcessor* rangeSource;
};

} // namespace luthier
