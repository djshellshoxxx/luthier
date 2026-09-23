#pragma once

/*  The Workshop bench's model (workshop-ui.md): what the bench does to the
    guitar, apart from how it is drawn.

    Every committed change goes through here so that each is exactly one undo
    entry described in real units (section 8): a part fitted, a pickup moved,
    a height, a saddle, a nut slot, a bench slot recalled. A drag is a gesture:
    begin opens the entry, the edits in between move the sound live, end
    writes the description with the before and after values.

    Audition (3.2) plays a candidate on a shadow guitar and never touches the
    committed guitar or the undo stack. The A/B slots (7) are workspace, kept
    in the processor's uiState rather than the preset.

    Message thread only.
*/

#include "../Model/Workshop/PartLibrary.h"

#include <array>
#include <optional>

namespace luthier
{

class LuthierAudioProcessor;

class WorkshopBench
{
public:
    explicit WorkshopBench (LuthierAudioProcessor& processor);

    /** What the bench shows: the committed guitar, or its live copy mid-gesture. */
    const WorkshopGuitar& current() const;

    /** The guitar differs from the file it was loaded from. */
    bool isModified() const;

    //==========================================================================
    // Commits (section 8): one undo entry each.

    /** Fits a part in a slot (nullptr removes an optional part). False if nothing changed. */
    bool fit (GuitarSlot slot, const PartPtr& part);

    /** Puts a slot back to what the guitar file had ("Revert", section 5). */
    bool revert (GuitarSlot slot);

    //==========================================================================
    // Gestures: a drag, or a keyboard nudge, is one entry opened here and
    // closed by endGesture(). Edits made outside a gesture open and close their own.

    void beginGesture();
    void endGesture();
    bool isInGesture() const noexcept { return gesture.has_value(); }

    struct Travel
    {
        double min = 0.0, max = 0.0;
        juce::String belowMin, aboveMax;   ///< why it stops there (section 4)
    };

    /** Where pickup `index` (0 neck, 1 middle, 2 bridge) can go, mm from the saddle. */
    Travel getPickupTravel (int index) const;

    /** Moves a pickup; returns where it ended up, and the reason if it was stopped. */
    double movePickup (int index, double positionMm, juce::String* stoppedBecause = nullptr);

    /** Sets a pickup's heights (0.8 - 6 mm, guitar-illustration.md 19). */
    void setPickupHeights (int index, double trebleMm, double bassMm);

    /** Saddle intonation for a string (engine index), +-6 mm. */
    void setIntonation (int stringIndex, double mm);

    /** Nut slot depth for a string (engine index), 0 - 1.2 mm. */
    void setNutSlotDepth (int stringIndex, double mm);

    /*  Section 4's snap: 1 mm by default, 0.1 mm with Shift (fine), none with
        Alt (free). `unit` is the coarse snap (1 mm, 0.1 mm for heights). */
    static double snap (double value, bool fine, bool free, double unit = 1.0)
    {
        if (free)
            return value;

        const double step = fine ? unit * 0.1 : unit;
        return std::round (value / step) * step;
    }

    static constexpr double kMinPickupHeight = 0.8, kMaxPickupHeight = 6.0;
    static constexpr double kMaxIntonation = 6.0, kMaxNutSlot = 1.2;

    //==========================================================================
    // A/B slots (section 7).
    static constexpr int kNumSlots = 8;
    static juce::String slotName (int index) { return juce::String::charToString ((juce::juce_wchar) ('A' + index)); }

    bool hasSlot (int index) const;
    void storeSlot (int index);
    bool recallSlot (int index);
    void clearSlot (int index);

    //==========================================================================
    // Audition (section 3.2): a shadow guitar with a candidate part in place.
    void beginAudition (GuitarSlot slot, const PartPtr& candidate);
    void endAudition();
    bool isAuditioning() const noexcept { return audition.has_value(); }
    const WorkshopGuitar* getAuditionGuitar() const noexcept { return audition ? &*audition : nullptr; }

    /** The guitar with `candidate` in `slot`, placed sensibly if the slot was empty. */
    WorkshopGuitar withPart (GuitarSlot slot, const PartPtr& candidate) const;

    //==========================================================================
    /** "neck pickup", "bridge", "strings" - for undo sentences and the inspector. */
    static juce::String describeSlot (GuitarSlot slot);

    /** A pickup's depth along the strings, mm, from its family (guitar-illustration.md 8). */
    static double pickupDepthMm (const Part* pickup, bool bass);

private:
    void applyLive();
    void commit (const WorkshopGuitar& edited, const juce::String& description);

    LuthierAudioProcessor& processor;

    struct Gesture
    {
        WorkshopGuitar before, live;
        juce::StringArray changes;   ///< what the gesture did, in order, for the sentence
        bool pushed = false;
    };

    std::optional<Gesture> gesture;
    std::optional<WorkshopGuitar> audition;
};

} // namespace luthier
