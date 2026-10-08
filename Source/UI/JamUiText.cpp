#include "JamUiText.h"
#include "../Jam/JamStyle.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace luthier
{

const char* const JamUiText::kArmedNoChord     = "Play a chord - the band follows. Or press START for drums first.";
const char* const JamUiText::kNoTunePlaying    = "No tune is playing - following what you play.";
const char* const JamUiText::kSeparateNeedsAux = "Separate outputs need the multi-out layout (B or D). The band is on the main output.";
const char* const JamUiText::kNoPlayHead       = "This host sends no transport; the band keeps its own time.";
const char* const JamUiText::kBassistResting   = "You're the bassist - Jam bass is resting";
const char* const JamUiText::kBackingTrackToo  = "A backing track is also playing";

namespace
{
    const char* const kLaneNames[] = { "Kick", "Snare", "Hats", "Ride", "Crash", "Toms", "Percussion", "Bass" };

    juce::String chordName (const ChordSymbol& chord)
    {
        return chord.isKnown() ? chord.toString() : juce::String ("-");
    }

    juce::String listWithAnd (const juce::StringArray& items)
    {
        if (items.size() <= 1)
            return items.joinIntoString ("");

        juce::StringArray head (items);
        const auto last = head[head.size() - 1];
        head.remove (head.size() - 1);
        return head.joinIntoString (", ") + " and " + last;
    }
}

const char* JamUiText::laneName (int lane) noexcept
{
    return kLaneNames[juce::jlimit (0, 7, lane)];
}

juce::String JamUiText::pillText (JamState state, bool enabled)
{
    if (! enabled)
        return "JAM";

    switch (state)
    {
        case JamState::counting: return "COUNT";
        case JamState::playing:  return "PLAYING";
        case JamState::ending:   return "ENDING";
        case JamState::off:
        case JamState::armed:    break;
    }

    return "ARMED";
}

juce::String JamUiText::styleName (int style)
{
    return JamStyleLibrary::getStyleChoiceNames()[juce::jlimit (0, 10, style)];
}

juce::String JamUiText::statusLine (const JamUiFacts& facts)
{
    if (! facts.haveStatus)
        return "-";

    const auto& s = facts.status;
    juce::String line = pillText (s.state, facts.enabled);
    line << " . " << styleName (s.style) << (s.variation == 0 ? " A" : " B") << " . " << s.baseIntensity;

    if (s.effectiveIntensity != s.baseIntensity)
        line << "(" << (s.effectiveIntensity > s.baseIntensity ? "+" : "") << (s.effectiveIntensity - s.baseIntensity) << ")";

    if (s.fillActive)
        line << " . FILL";

    return line;
}

juce::String JamUiText::chordLine (const JamUiFacts& facts)
{
    if (! facts.haveStatus)
        return "-";

    const auto& s = facts.status;
    juce::String line = chordName (s.currentChord);

    if (s.nextChord.isKnown() && s.nextSource != JamStatus::NextSource::none)
        line << " -> " << chordName (s.nextChord)
             << (s.nextSource == JamStatus::NextSource::predicted ? " (predicted)" : " (tune)");

    line << " . " << juce::String (s.bpm, 0) << " bpm "
         << (s.clock == JamStatus::Clock::host ? "host" : s.clock == JamStatus::Clock::tune ? "tune" : "own");

    if (s.state == JamState::playing || s.state == JamState::ending)
        line << " . bar " << (s.bar + 1) << "." << (s.beat + 1);

    return line;
}

juce::String JamUiText::genericGroove (const JamStatus& status)
{
    return "Generic groove - " + styleName (status.style) + " has no "
           + juce::String (status.meterNumerator) + "/" + juce::String (status.meterDenominator) + ".";
}

juce::StringArray JamUiText::messages (const JamUiFacts& facts)
{
    juce::StringArray out;

    if (facts.styleWarning.isNotEmpty())
        out.add (facts.styleWarning);

    if (! facts.enabled)
        return out;

    if (facts.separateFallback)
        out.add (kSeparateNeedsAux);

    if (facts.chordSource == 2 && ! facts.tunePlaying)
        out.add (kNoTunePlaying);

    if (facts.haveStatus)
    {
        const auto& s = facts.status;

        if (s.state == JamState::armed && s.waitingForChord)
            out.add (kArmedNoChord);

        if (s.noPlayHead)
            out.add (kNoPlayHead);

        if (s.genericGroove)
            out.add (genericGroove (s));

        if (s.bassResting)
            out.add (kBassistResting);
    }
    else
    {
        out.add (kArmedNoChord);
    }

    if (facts.backingTrackPlaying)
        out.add (kBackingTrackToo);

    return out;
}

juce::String JamUiText::stateAnnouncement (const JamStatus& s)
{
    switch (s.state)
    {
        case JamState::playing:  return "Band playing, " + styleName (s.style) + ", intensity " + juce::String (s.effectiveIntensity);
        case JamState::counting: return "Band counting in";
        case JamState::ending:   return "Band ending";
        case JamState::armed:    return "Band armed";
        case JamState::off:      return "Band off";
    }

    return {};
}

juce::String JamUiText::describeHits (uint32_t hits, int stepsInBar, int stepsPerBeat)
{
    stepsInBar = juce::jlimit (1, JamStatus::kLaneSteps, stepsInBar);
    stepsPerBeat = juce::jmax (1, stepsPerBeat);

    juce::Array<int> steps;

    for (int i = 0; i < stepsInBar; ++i)
        if ((hits >> i) & 1u)
            steps.add (i);

    if (steps.isEmpty())
        return {};

    // Every step at one spacing: quarters, 8ths or 16ths.
    for (const auto& [spacing, name] : { std::pair<int, const char*> { 1, "16ths" }, { 2, "8ths" }, { 4, "quarters" } })
    {
        const int step = spacing * stepsPerBeat / 4;

        if (step < 1 || stepsInBar % step != 0 || steps.size() != stepsInBar / step || steps.size() < 3)
            continue;

        bool regular = true;

        for (int i = 0; i < steps.size() && regular; ++i)
            regular = steps[i] == i * step;

        if (regular)
            return name;
    }

    // All on beats: "1 and 3".
    bool onBeats = true;
    juce::StringArray beats;

    for (int s : steps)
    {
        onBeats = onBeats && s % stepsPerBeat == 0;
        beats.add (juce::String (s / stepsPerBeat + 1));
    }

    if (onBeats)
        return listWithAnd (beats);

    return juce::String (steps.size()) + (steps.size() == 1 ? " hit" : " hits");
}

juce::String JamUiText::laneDescription (const JamStatus& s)
{
    juce::StringArray drums;

    for (int lane = 0; lane < 7; ++lane)
    {
        const auto hits = describeHits (s.laneHits[(size_t) lane], s.stepsInBar, s.stepsPerBeat);

        if (hits.isNotEmpty())
            drums.add ((drums.isEmpty() ? juce::String (kLaneNames[lane]) : juce::String (kLaneNames[lane]).toLowerCase())
                       + " " + hits);
    }

    juce::String text = drums.isEmpty() ? juce::String ("No drums") : drums.joinIntoString (", ");

    if (s.bassResting)
        return text + "; bass resting";

    juce::StringArray notes;

    for (int i = 0; i < juce::jmin (s.numBassNotes, (int) s.bassNotes.size()); ++i)
        notes.add (juce::MidiMessage::getMidiNoteName (s.bassNotes[(size_t) i], true, false, 3));

    return text + "; bass " + (notes.isEmpty() ? juce::String ("resting") : notes.joinIntoString (" "));
}

} // namespace luthier
