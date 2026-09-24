#pragma once

/*  The technique layer's MIDI front (engine-technique-layer.md 3.1 and 3.6).

    Every phase-5b technique is fired the same few ways: a keyswitch from a
    reserved range, a CC held past half way, notes on an MPE zone's channel,
    a button in the Techniques tab or the Playing strip. This reads a block's
    MIDI once for all of them, takes out what belongs to a technique (so a
    keyswitch never sounds as a note, input-routing.md 5 before 6) and turns
    the rest of their triggers into GestureEvents at their samples. Each
    technique engine then interprets its own events - a scrape starts on one,
    a slap modifier is held by one - so the rules for what a trigger means
    stay with the technique, and the rules for how it arrives are written once.

    The keyswitch table below is the one place a technique's note is chosen,
    so two techniques cannot claim the same key.

    Audio thread, apart from request(). Nothing allocates: `filtered` is sized
    by the caller in prepare.
*/

#include "PlayingEvents.h"
#include <array>
#include <atomic>

namespace luthier
{

//==============================================================================
/** The technique engines this front serves. Append only. */
enum class TechniqueId
{
    scrape = 0,
    slap,
    numTechniques
};

/*  engine-technique-layer.md 3.6: keyswitches from a range nothing plays. A
    five-string bass's low B is MIDI 23 and a drop-A bass 21; these sit under
    that, from C0 (12) up. */
namespace TechniqueKeyswitch
{
    inline constexpr int scrape    = 12;   ///< string-scraping.md 2
    inline constexpr int rakeDown  = 13;   ///< pick-noise.md 5
    inline constexpr int rakeUp    = 14;
    inline constexpr int slap      = 15;   ///< string-slap-technique.md 1: the slap modifier / strike
    inline constexpr int ghost     = 16;   ///< 1: ghost mode's modifier keyswitch
    inline constexpr int bodyTap   = 17;   ///< a body tap, whatever the slap type
    inline constexpr int palmSlap  = 18;   ///< a palm slap, whatever the slap type
}

/** How a technique is fired (string-scraping.md 2, string-slap-technique.md 1). */
enum class TriggerSource
{
    keyswitch = 0,
    controller,
    mpeZone,
    buttonOnly,
    velocityZone,   ///< slap only: notes at or above a velocity; nothing here to consume
    numSources
};

//==============================================================================
/** One trigger, at its sample. */
struct GestureEvent
{
    TechniqueId technique = TechniqueId::scrape;

    /*  Which of the technique's inputs fired: 0 is its main trigger (the main
        keyswitch, the trigger CC, a zone note, the button); 1-3 its extra
        keyswitches, in its config's order; kContinuousRole a watched
        controller's value. */
    int role = 0;
    bool on = true;
    int offset = 0;

    /** Velocity for notes, 0-1 value for controllers, 1 for the button. */
    double value = 1.0;

    /** The note or controller that fired it, -1 for the button. */
    int number = -1;
};

/** A technique's triggers. Keyswitch roles 1-3 listen whenever it is armed; role 0 only with TriggerSource::keyswitch. */
struct TechniqueTriggerConfig
{
    bool armed = false;
    TriggerSource source = TriggerSource::keyswitch;

    std::array<int, 4> keyswitches { -1, -1, -1, -1 };   ///< per role; -1 unused
    int triggerCc = -1;                                  ///< with TriggerSource::controller
    int auxCc = -1;                                      ///< role 1 as a CC, whenever armed (slap's ghost CC)
    int zoneChannel = 16;

    /*  With TriggerSource::mpeZone: true, the zone's notes are the technique's
        and are taken out (a scrape has no pitch); false, they stay notes and
        the technique only hears about them (a slapped note still sounds). */
    bool consumeZoneNotes = true;

    int continuousCc = -1;           ///< a controller whose every value is reported
    bool continuousAftertouch = false;
};

//==============================================================================
class TechniqueTriggers
{
public:
    static constexpr int kContinuousRole = 15;
    static constexpr int kMaxEvents = 128;
    static constexpr int kMaxRoles = 8;

    void configure (TechniqueId technique, const TechniqueTriggerConfig& config) noexcept;
    const TechniqueTriggerConfig& getConfig (TechniqueId technique) const noexcept;

    void reset() noexcept;

    /*  The UI's buttons: a role pressed or let go. Any thread; delivered at
        offset 0 of the next process(). Ignored while the technique is disarmed. */
    void request (TechniqueId technique, int role, bool on) noexcept;

    /*  Reads one block. Returns `in` untouched if nothing was taken, otherwise
        `filtered`, holding everything else at its own sample. With nothing
        armed and nothing requested, it does not look at the MIDI. */
    const juce::MidiBuffer& process (const juce::MidiBuffer& in, juce::MidiBuffer& filtered) noexcept;

    /** The last process()'s events, in time order. */
    int getNumEvents() const noexcept { return numEvents; }
    const GestureEvent& getEvent (int i) const noexcept
    {
        return events[(size_t) juce::jlimit (0, kMaxEvents - 1, i)];
    }

    /** True if a role of a technique was held (on and not yet off) at the end of the last block. */
    bool isHeld (TechniqueId technique, int role) const noexcept;

private:
    bool takes (const juce::uint8* d, int n) const noexcept;
    void push (const GestureEvent& e) noexcept;

    static constexpr int kTechniques = (int) TechniqueId::numTechniques;

    std::array<TechniqueTriggerConfig, (size_t) kTechniques> configs {};
    std::array<std::array<bool, kMaxRoles>, (size_t) kTechniques> held {};
    std::array<bool, (size_t) kTechniques> triggerCcDown {}, auxCcDown {};

    /** Per technique: bit r = role r pressed, bit 8 + r = role r let go. */
    std::array<std::atomic<juce::uint32>, (size_t) kTechniques> requests {};

    std::array<GestureEvent, kMaxEvents> events {};
    int numEvents = 0;
};

} // namespace luthier
