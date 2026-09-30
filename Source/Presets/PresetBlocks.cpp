/*  SPEC-SWEEP: SM-1, SM-11, SM-16, FF-24..29, RIO-30..32, LP-9, RE-40.

    The preset blocks the processor owns (state-model.md 1, file-formats.md 2):
    modulation, snapshots, MIDI Learn mappings, rhythm engine, routing,
    character and tone-match. Before this they were written only into the host
    session, so a `.luthierpreset` lost them and a setlist entry recalled its
    snapshot from whatever bank the previous preset had left.

    PresetManager calls writePresetBlocks from toVar and readPresetBlocks at the
    end of fromVar, so every path that goes through a preset - a file load, the
    host session, undo, A/B, preset morph and the setlist - carries them.

    A block the preset does not carry is set back to its default (the value
    each module had when this instance was built), never left as the previous
    preset's: a factory preset with no routing block unmutes the aux strips
    the last preset muted. */

#include "../PluginProcessor.h"

namespace luthier
{

namespace PresetBlockKeys
{
    // file-formats.md 2's spellings. The camelCase ones are what the preset
    // known-key list reserved before the blocks were written; read as aliases.
    static constexpr const char* modulation   = "modulation";
    static constexpr const char* snapshots    = "snapshots";
    static constexpr const char* midiMappings = "midi_mappings";
    static constexpr const char* rhythm       = "rhythm_engine";
    static constexpr const char* routing      = "routing";
    static constexpr const char* character    = "character";
    static constexpr const char* toneMatch    = "tone_match";

    static constexpr const char* legacyMidiMappings = "midiMappings";
    static constexpr const char* legacyRhythm       = "rhythmEngine";
    static constexpr const char* legacyToneMatch    = "toneMatch";
}

void LuthierAudioProcessor::writePresetBlocks (juce::DynamicObject& root) const
{
    root.setProperty (PresetBlockKeys::modulation, modMatrix.toVar());
    root.setProperty (PresetBlockKeys::snapshots, snapshots.toVar());

    /*  live-performance 11: MIDI Learn assignments are per-preset, but a
        controller setup must survive browsing presets (docs/PRESET_FORMAT.md).
        So the block is written only when there is something in it, and a
        preset without one leaves the current mappings alone (see below). */
    if (midiLearn.getNumMappings() > 0)
        root.setProperty (PresetBlockKeys::midiMappings, midiLearn.toVar());
    root.setProperty (PresetBlockKeys::rhythm, engine.getRhythmEngine().toVar());

    // routing-io 9: aux mute/solo/gain, per-string, MIDI-out and sidechain-to-amp.
    // The bus layout is the host's and is not in RoutingMatrix::toVar.
    root.setProperty (PresetBlockKeys::routing, routing.toVar());

    // character-wear 1: the seed and wear map are the instrument's identity. The
    // amount is the character macro, which the engine picks up on its next
    // block, so the parameter's value is the one written.
    {
        auto character = engine.getCharacterEngine().toVar();

        if (auto* o = character.getDynamicObject())
            if (auto* amount = apvts.getRawParameterValue (ParamIDs::macroCharacter))
                o->setProperty ("amount", (double) amount->load());

        // string-aging.md 8 / environment.md 6 (REALISM-A): the per-string
        // aging state and the environment's reference ride in the block.
        if (auto* o = character.getDynamicObject())
        {
            o->setProperty ("aging", engine.getStringAging().toVar());
            o->setProperty ("environment", engine.getEnvironment().toVar());
        }

        root.setProperty (PresetBlockKeys::character, character);
    }

    // tone-match 7: the IR slots, by path plus their settings.
    {
        auto* irs = new juce::DynamicObject();

        irs->setProperty ("body", bodyIr.toVar());
        irs->setProperty ("cab1", cabIr[0].toVar());
        irs->setProperty ("cab2", cabIr[1].toVar());
        irs->setProperty ("eq", eqMatchSlot.toVar());                       // SPEC-SWEEP TM-28
        irs->setProperty ("eqPosition", (int) getEqMatchPosition());

        root.setProperty (PresetBlockKeys::toneMatch, juce::var (irs));
    }
}

void LuthierAudioProcessor::readPresetBlocks (const juce::DynamicObject& root)
{
    auto defaults = defaultPresetBlocks.getDynamicObject();

    // The block from the file, the legacy spelling, or the default.
    auto pick = [&root, defaults] (const char* key, const char* legacyKey = nullptr) -> juce::var
    {
        if (root.hasProperty (key))
            return root.getProperty (key);

        if (legacyKey != nullptr && root.hasProperty (legacyKey))
            return root.getProperty (legacyKey);

        return defaults != nullptr ? defaults->getProperty (key) : juce::var();
    };

    modMatrix.fromVar (pick (PresetBlockKeys::modulation));

    // The one block whose absence keeps the current state (see the writer).
    if (root.hasProperty (PresetBlockKeys::midiMappings))
        midiLearn.fromVar (root.getProperty (PresetBlockKeys::midiMappings));
    else if (root.hasProperty (PresetBlockKeys::legacyMidiMappings))
        midiLearn.fromVar (root.getProperty (PresetBlockKeys::legacyMidiMappings));
    engine.getRhythmEngine().fromVar (pick (PresetBlockKeys::rhythm, PresetBlockKeys::legacyRhythm));
    routing.fromVar (pick (PresetBlockKeys::routing));
    {
        const auto character = pick (PresetBlockKeys::character);
        engine.getCharacterEngine().fromVar (character);
        applyRealismCharacterBlock (character);   // REALISM-A: aging, environment
    }

    if (auto* irs = pick (PresetBlockKeys::toneMatch, PresetBlockKeys::legacyToneMatch).getDynamicObject())
    {
        bodyIr.fromVar (irs->getProperty ("body"));
        cabIr[0].fromVar (irs->getProperty ("cab1"));
        cabIr[1].fromVar (irs->getProperty ("cab2"));

        // SPEC-SWEEP TM-28: absent in older presets - no filter, post-amp.
        if (irs->hasProperty ("eq"))
            eqMatchSlot.fromVar (irs->getProperty ("eq"));
        else
            eqMatchSlot.unload();

        setEqMatchPosition ((EqMatchPosition) juce::jlimit (0, 2, irs->hasProperty ("eqPosition")
                                                                     ? (int) irs->getProperty ("eqPosition") : 1));
    }

    /*  live-performance 1: a preset saved before snapshots existed has none,
        which the spec treats as one implicit snapshot equal to the preset
        (the bank's empty state). Last, so a snapshot's modules are the ones
        this preset just set. */
    const auto bank = pick (PresetBlockKeys::snapshots);

    if (bank.isObject())
        snapshots.fromVar (bank);
    else
        snapshots.clear();
}

void LuthierAudioProcessor::captureDefaultPresetBlocks()
{
    auto* defaults = new juce::DynamicObject();
    writePresetBlocks (*defaults);
    defaultPresetBlocks = juce::var (defaults);
}

//==============================================================================
void LuthierAudioProcessor::presetFileLoaded()
{
    /*  SPEC-SWEEP: SM-46, state-model.md 8.1: "A / B compare active - compare
        state clears; banner 'A/B cleared by preset load.'" The two slots hold
        whole states of the preset being compared; after another preset loads,
        recalling either would silently undo the load.

        (Section 2's "never touches ... A/B" list is the general rule; 8.1 is
        the specific intersection and wins - see sweep-notes/state.md.) */
    const bool hadCompare = slotBActive || slotA.getSize() > 0 || slotB.getSize() > 0;

    slotA.reset();
    slotB.reset();
    slotBActive = false;

    if (hadCompare)
        stateNotices.addIfNotAlreadyThere ("A/B cleared by preset load.");
}

juce::StringArray LuthierAudioProcessor::takeStateNotices()
{
    auto out = stateNotices;
    stateNotices.clear();
    return out;
}

juce::StringArray LuthierAudioProcessor::takeStateWarnings()
{
    auto out = stateWarnings;
    stateWarnings.clear();
    return out;
}

} // namespace luthier
