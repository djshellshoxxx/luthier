#pragma once

/*  Preset-to-preset morphing (ambiguity-resolutions.md 5; not snapshot morph,
    which is live-performance.md 3 and SnapshotBank's).

    Two whole presets sit in slots A and B. The morph position, automatable as
    `preset_morph_position`, moves between them:

      - continuous parameters interpolate (5.1);
      - discrete ones - amp model, cab model, pickup selector, pedal types -
        hard-switch at 0.5, as SnapshotBank::isDiscrete decides;
      - structural state - the guitar reference, mod routes, patterns,
        snapshots, the ranges block - is the side the position is on: crossing
        0.5 loads that side's preset whole (with the standard crossfade the
        preset and guitar loaders already give), and the continuous values are
        then laid over it.

    So 0.0 is preset A exactly and 1.0 is preset B exactly (5.3), and a
    snapshot recall during a morph cancels the morph and keeps the recalled
    state (8). Message thread: it writes parameters and may load a guitar.
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace luthier
{

class LuthierAudioProcessor;

class PresetMorph
{
public:
    enum Slot { slotA = 0, slotB = 1 };

    explicit PresetMorph (LuthierAudioProcessor& processor);

    /** 5.2's Morph toggle. Switching it on puts the current sound in slot A
        (and B, if it is empty), so the morph starts where the player is. */
    void setEnabled (bool shouldMorph);
    bool isEnabled() const noexcept { return enabled; }

    /** Where "load a preset" goes while morphing (5.2). */
    void setCurrentSlot (Slot slot) noexcept { currentSlot = slot; }
    Slot getCurrentSlot() const noexcept { return currentSlot; }

    /** Puts a preset state (PresetManager::toVar's shape) in a slot, and
        re-applies the morph so the sound follows. */
    void setSlot (Slot slot, const juce::var& presetState, const juce::String& name);
    const juce::var& getSlotState (Slot slot) const noexcept { return slots[(size_t) slot]; }
    const juce::String& getSlotName (Slot slot) const noexcept { return names[(size_t) slot]; }
    bool hasBothSlots() const noexcept { return slots[0].isObject() && slots[1].isObject(); }

    /** Moves the morph to `position`. Does nothing when off or a slot is empty. */
    void apply (double position);
    double getAppliedPosition() const noexcept { return appliedPosition; }

    /** 8: a snapshot recall ends the morph and keeps the recalled state. */
    void cancel();

private:
    LuthierAudioProcessor& processor;

    bool enabled = false;
    Slot currentSlot = slotA;
    std::array<juce::var, 2> slots;
    std::array<juce::String, 2> names;

    double appliedPosition = -1.0;
    int loadedSide = -1;       ///< whose structure is loaded: 0 = A, 1 = B

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetMorph)
};

} // namespace luthier
