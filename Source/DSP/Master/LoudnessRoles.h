#pragma once

/*  Every APVTS parameter's loudness role (output-normalization.md 3.1).

      Config       rendered at its current (quantised) value; a change requests
                   a calibration 250 ms after it rests
      Performance  rendered at its default; never requests a calibration
      Mix          forced neutral in the reference render; never requests one

    HOW A PARAMETER DECLARES ITS ROLE
    ---------------------------------
    Config is the default. Every parameter this file does not name is Config,
    which is right for anything that shapes the sound: an instrument, string,
    body, pickup, circuit, amp, cab, mic, room, setup, noise, effect or macro
    control. So a workstream that appends a sound-shaping parameter needs to
    do nothing here.

    A parameter that is a *playing gesture* (the player moves it while playing,
    and it should still change the level with normalization on: a volume
    knob, a whammy bar, a technique trigger or its strength) must be added to
    kPerformanceIds or kPerformancePrefixes in LoudnessRoles.cpp. A parameter
    that is a *mix or output trim* (master gain, a limiter switch, a tap
    routing that is not on the main output, anything mixed after the master
    bus such as jam-mode's band) must be added to kMixIds or kMixPrefixes.

    ON-26 checks that no id matches both lists, that every id the spec names
    resolves to the role the spec gives it, and that the hash obeys the roles.

    Effect-slot parameters: output-normalization 3.1 makes a slot parameter
    Performance when EffectsChain marks it a performance control (wah or
    volume-pedal position). In this build those positions are the MIDI
    expression input (Pedal::setExpression), not parameters, so the reference
    render already leaves them at their default and every slot parameter is
    Config. isPerformanceSlotParameter is the hook for a future pedal whose
    position is a parameter.
*/

#include <juce_core/juce_core.h>
#include <vector>

namespace luthier
{

enum class LoudnessRole : std::uint8_t
{
    config = 0,
    performance = 1,
    mix = 2
};

namespace LoudnessRoles
{
    /** The role of a parameter id. Never fails: unlisted ids are Config. */
    LoudnessRole roleFor (const juce::String& paramId);

    /** True when the id is named explicitly (by id or prefix) in the
        Performance or Mix lists. */
    bool isListed (const juce::String& paramId);

    /** Ids that match more than one explicit list: must be empty (ON-26). */
    juce::StringArray findConflicts (const juce::StringArray& paramIds);

    /** The ids and prefixes the spec names, with their expected roles, for
        ON-26. */
    struct Expectation { const char* id; LoudnessRole role; };
    const std::vector<Expectation>& specExpectations();

    /** 3.1's slot rule; false for every pedal in this build. */
    bool isPerformanceSlotParameter (int pedalType, int paramIndex) noexcept;

    const char* roleName (LoudnessRole role) noexcept;
}

} // namespace luthier
