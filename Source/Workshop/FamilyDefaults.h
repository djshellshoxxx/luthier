#pragma once

/*  guitar-illustration.md 12.3: "Amp defaults (bass presets recall a bass amp;
    electric recalls a guitar amp; acoustic recalls a clean DI + reverb)", with
    12.4's rule that amp settings survive a family switch unless the amp is
    family-inappropriate. Message thread; the caller owns the undo entry.
*/

#include <juce_audio_processors/juce_audio_processors.h>

namespace luthier::FamilyDefaults
{
    /** The amp model index a family calls for when the current one does not suit it, or -1 to keep it. */
    int ampModelFor (const juce::String& family, int currentModel) noexcept;

    /*  Applies 12.3's amp defaults to the parameters: the model if the current
        one does not suit the family, and for acoustic and classical a room with
        some reverb if it is off or dry. Returns a line for the family-change
        banner ("Amp: Bass 800 for the bass."), empty if nothing changed. */
    juce::String applyAmpDefaults (juce::AudioProcessorValueTreeState& state, const juce::String& family);
}
