/*  Jam mode's side of LuthierAudioProcessor (jam-mode.md, FEAT-JAM).

    The band belongs to the processor (0.2). processBlock's hooks are in
    PluginProcessor.cpp, each marked FEAT-JAM; the message-thread work - START /
    STOP, the preset's jam block, style files, the tune's chord map, mirroring
    jam_play back to the band's state - lives here, in its own file, so the
    shared hub stays small. */

#include "../PluginProcessor.h"
#include "../Support/ErrorLog.h"

namespace luthier
{

namespace
{
    constexpr uint64_t kDefaultJamSeed = 4849997;

    bool parameterOn (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        if (auto* p = apvts.getParameter (id))
            return p->getValue() > 0.5f;

        return false;
    }

    void setJamParameter (juce::AudioProcessorValueTreeState& apvts, const char* id, float plain)
    {
        if (auto* p = apvts.getParameter (id))
        {
            const float normalised = p->convertTo0to1 (plain);

            if (std::abs (p->getValue() - normalised) > 1.0e-6f)
                p->setValueNotifyingHost (normalised);
        }
    }
}

//==============================================================================
void LuthierAudioProcessor::jamStartStop()
{
    // The first press arms (8.2): a band that is off is switched on and started.
    if (! parameterOn (apvts, ParamIDs::jamEnabled))
    {
        setJamParameter (apvts, ParamIDs::jamEnabled, 1.0f);
        jam.requestStart();
        return;
    }

    jam.requestToggle();
}

void LuthierAudioProcessor::jamFill()
{
    if (parameterOn (apvts, ParamIDs::jamEnabled))
        jam.requestFill();
}

void LuthierAudioProcessor::jamArmToggle()
{
    setJamParameter (apvts, ParamIDs::jamEnabled, parameterOn (apvts, ParamIDs::jamEnabled) ? 0.0f : 1.0f);
}

//==============================================================================
juce::var LuthierAudioProcessor::getJamBlock() const
{
    // 12: `"jam": {"style_ref": null | "User/My Shuffle.luthierjam",
    //             "link_rhythm_kit": false, "seed": 4849997}`
    auto* block = new juce::DynamicObject();
    block->setProperty ("style_ref", jamStyleRef.isNotEmpty() ? juce::var (jamStyleRef) : juce::var());
    block->setProperty ("link_rhythm_kit", jamLinkRhythmKit);
    block->setProperty ("seed", (juce::int64) jam.getSeed());
    return juce::var (block);
}

void LuthierAudioProcessor::setJamBlock (const juce::var& block)
{
    // A missing block means defaults (12).
    jamLinkRhythmKit = block.isObject() && (bool) block.getProperty ("link_rhythm_kit", false);
    jam.setSeed (block.isObject() && block.hasProperty ("seed") ? (uint64_t) (juce::int64) block.getProperty ("seed", 0)
                                                              : kDefaultJamSeed);

    const auto ref = block.isObject() ? block.getProperty ("style_ref", {}) : juce::var();
    jamStyleWarning.clear();

    if (ref.isString() && ref.toString().isNotEmpty())
    {
        jamStyleRef = ref.toString();

        // "User/<name>" is in the user folder; anything else is a full path.
        const auto file = jamStyleRef.startsWith ("User/")
                            ? JamStyleLibrary::getUserFolder().getChildFile (jamStyleRef.fromFirstOccurrenceOf ("User/", false, false))
                            : juce::File (jamStyleRef);

        const auto* style = jamStyles.loadUserStyle (file, jamStyleWarning);
        jam.setStyleSlot (jam::kUserStyleIndex, style);

        // 13: the banner, logged once; style_ref is kept for the next save.
        if (jamStyleWarning.isNotEmpty())
            ErrorLog::write (ErrorLog::Severity::warn, "Jam", "JAM_STYLE_UNREADABLE", jamStyleWarning);
    }
    else
    {
        jamStyleRef.clear();
        jam.setStyleSlot (jam::kUserStyleIndex, jamStyles.getFactoryStyle (0));
    }
}

juce::String LuthierAudioProcessor::loadJamStyleFile (const juce::File& file)
{
    // 12: picking a user style file is one undo entry.
    pushUndoState ("jam-style-file");

    const auto userFolder = JamStyleLibrary::getUserFolder();
    auto block = getJamBlock();

    if (auto* o = block.getDynamicObject())
        o->setProperty ("style_ref", file.isAChildOf (userFolder) ? "User/" + file.getRelativePathFrom (userFolder).replaceCharacter ('\\', '/')
                                                                    : file.getFullPathName());

    setJamBlock (block);
    setJamParameter (apvts, ParamIDs::jamStyle, (float) jam::kUserStyleIndex);
    return jamStyleWarning;
}

void LuthierAudioProcessor::setJamRhythmKitLinked (bool linked)
{
    jamLinkRhythmKit = linked;
    jamLinkedStyle = -1;   // applies at the next service
}

//==============================================================================
void LuthierAudioProcessor::onJamTimeline (const TuneTimeline& timeline)
{
    // 3.3: the tune's chord map, with each section's optional jam hint (11).
    std::vector<JamSectionHint> hints;
    const auto& tune = tuneSession.getTune();

    for (int i = 0; i < tune.getNumSections(); ++i)
    {
        JamSectionHint hint;

        if (const auto* section = tune.getSection (i))
        {
            const auto jamHint = section->extra["jam"];

            if (jamHint.isObject())
            {
                hint.intensity = juce::jlimit (0, 5, (int) jamHint.getProperty ("intensity", 0));
                hint.fillInto = (bool) jamHint.getProperty ("fill_into", true);
            }
        }

        hints.push_back (hint);
    }

    auto map = JamChordMap::fromTimeline (timeline, tunePlayer.isLooping(), hints);

    if (map->truncated)
        ErrorLog::write (ErrorLog::Severity::warn, "Jam", "JAM_CHORD_MAP_TRUNCATED",
                         "The tune has more than 1024 chord changes; the band anticipates the first 1024.");

    jam.setChordMap (std::move (map));
}

void LuthierAudioProcessor::serviceJam()
{
    jam.collectGarbage();

    // 10: jam_fill_now resets itself once it has risen.
    if (parameterOn (apvts, ParamIDs::jamFillNow))
        setJamParameter (apvts, ParamIDs::jamFillNow, 0.0f);

    // jam_play follows the band once its state has held for 200 ms, so a
    // toggling footswitch and the pill always agree with what is playing.
    const auto state = jam.getState();
    const bool running = state == JamState::counting || state == JamState::playing;
    const double now = juce::Time::getMillisecondCounterHiRes();

    if ((int) state != jamMirrorState)
    {
        jamMirrorState = (int) state;
        jamMirrorSince = now;
    }
    else if (now - jamMirrorSince > 200.0 && parameterOn (apvts, ParamIDs::jamEnabled)
               && parameterOn (apvts, ParamIDs::jamPlay) != running && state != JamState::ending)
    {
        setJamParameter (apvts, ParamIDs::jamPlay, running ? 1.0f : 0.0f);
    }

    // 4.1: the style's genre kit, when the preset links it. Never its rig preset.
    JamStatus status;

    if (jamLinkRhythmKit && jam.getStatusChannel().read (status) && status.style != jamLinkedStyle)
    {
        jamLinkedStyle = status.style;

        if (const auto* style = jam.getStyleSlot (status.style))
        {
            const int kit = genreKits.indexOf (style->rhythmKit);

            if (kit >= 0)
                applyGenreKit (kit);
        }
    }

    // 11: whether the playing section has a bass line.
    {
        bool hasBass = false;

        if (const auto* section = tuneSession.getTune().getSection (juce::jmax (0, tunePlayer.getPlayingSection())))
            hasBass = section->bass.mode != BassMode::off;

        jamTuneHasBass.store (hasBass, std::memory_order_relaxed);
    }
}

//==============================================================================
void LuthierAudioProcessor::mixJam (juce::AudioBuffer<float>& mainOut, int numSamples) noexcept
{
    // 7: Main, Separate (Aux 9 and 10), or both; Separate falls back to Main
    // when the host gave no aux (layouts A and C, 8.4).
    const int output = jamOutputRaw != nullptr ? juce::roundToInt (jamOutputRaw->load()) : 0;
    bool separateAvailable = false;

    if (RoutingMatrix::layoutHasAux (routing.getActiveLayout()))
        for (int bus = 1; bus < getBusCount (false) && ! separateAvailable; ++bus)
            if (const auto* declared = getBus (false, bus))
                separateAvailable = getChannelCountOfBus (false, bus) > 0
                                    && (declared->getName() == getAuxBusName (kJamDrumsAux)
                                        || declared->getName() == getAuxBusName (kJamBassAux));

    jamSeparateFallback.store (output != (int) JamEngine::Output::main && ! separateAvailable, std::memory_order_relaxed);

    const bool toMain = output != (int) JamEngine::Output::separate || ! separateAvailable;

    if (! toMain || numSamples > jamMain.getNumSamples())
        return;

    for (int ch = 0; ch < 2; ++ch)
    {
        jamMain.copyFrom (ch, 0, jam.getDrums (ch), numSamples);
        jamMain.addFrom (ch, 0, jam.getBass (ch), numSamples);
    }

    // The kill switch mutes the band with the ramp it gave the guitar.
    killSwitch.applyBlockRamp (jamMain, numSamples);

    for (int ch = 0; ch < juce::jmin (2, mainOut.getNumChannels()); ++ch)
        mainOut.addFrom (ch, 0, jamMain, mainOut.getNumChannels() == 1 ? 0 : ch, 0, numSamples);
}

} // namespace luthier
