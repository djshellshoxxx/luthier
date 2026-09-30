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

        /** TECHNIQUES: the preset's techniques block as JSON (engine-technique-layer.md 7), or empty. */
        const char* techniques = "";
    };

    /** Bumped when generated factory files change meaning; older generated files
        (never user edits) are rewritten by writeAll. 2: the guitar block. 3: the fuzz lead's hum. */
    static constexpr int kFactoryRevision = 3;

    static int getNumPresets() noexcept;
    static const Definition& getPreset (int index) noexcept;

    /** Writes any missing factory presets into `folder`, organised by category.
        Existing files are left alone so a user who edited one keeps their edit. */
    static void writeAll (const juce::File& folder);

    /*  SPEC-SWEEP: FC-1. A factory preset's current name for a name it shipped
        under before the trademark sweep, or the name unchanged. Lets a setlist
        or a lookup by name that still says the old one find the preset. */
    static juce::String renamedPreset (const juce::String& name);

    /** Builds one preset's JSON using a live processor for the parameter ranges. */
    static juce::var toVar (const Definition& def, const juce::AudioProcessor& processor);

    /** Called once at startup by the processor so writeAll has ranges to work with. */
    static void setProcessorForRanges (const juce::AudioProcessor* processor) noexcept;

    /** SPEC-SWEEP: a processor going away stops being the range source, so the
        next one's PresetManager does not read a dangling pointer. */
    static void forgetProcessorForRanges (const juce::AudioProcessor* processor) noexcept
    {
        if (rangeSource == processor)
            rangeSource = nullptr;
    }

private:
    static const juce::AudioProcessor* rangeSource;
};

} // namespace luthier
