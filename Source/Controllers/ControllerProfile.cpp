#include "ControllerProfile.h"

#include <algorithm>

namespace luthier
{

//==============================================================================
const char* getControllerModeName (ControllerMode mode) noexcept
{
    switch (mode)
    {
        case ControllerMode::standard:   return "standard";
        case ControllerMode::mpe:        return "mpe";
        case ControllerMode::perChannel: return "per_channel";
        case ControllerMode::numModes:
        default:                         return "standard";
    }
}

ControllerMode controllerModeFromName (const juce::String& name) noexcept
{
    for (int i = 0; i < (int) ControllerMode::numModes; ++i)
        if (name.equalsIgnoreCase (getControllerModeName ((ControllerMode) i)))
            return (ControllerMode) i;

    return ControllerMode::standard;
}

namespace
{
    /** Resolves a cc_map value, which is written in the profile as the name of a
        target rather than as a number, so the file stays readable and cannot be
        silently repointed by an insertion into the enum. */
    MidiTarget targetFromName (const juce::String& name) noexcept
    {
        for (int i = 0; i < (int) MidiTarget::NumTargets; ++i)
        {
            const juce::String targetName (getMidiTargetName ((MidiTarget) i));

            // Profiles are written in the spec's snake_case ("whammy_amount"),
            // while the enum's names are human-facing ("Whammy Bar"). Comparing
            // with the separators stripped lets the file use either.
            if (name.equalsIgnoreCase (targetName)
                  || name.removeCharacters ("_- ").equalsIgnoreCase (
                         targetName.removeCharacters ("_- ")))
                return (MidiTarget) i;
        }

        // The spec's example profile uses names that read naturally rather than
        // matching the enum, so the common ones are spelled out.
        if (name.equalsIgnoreCase ("whammy_amount"))  return MidiTarget::WhammyBar;
        if (name.equalsIgnoreCase ("vibrato_depth"))  return MidiTarget::VibratoDepth;
        if (name.equalsIgnoreCase ("vibrato_rate"))   return MidiTarget::VibratoRate;
        if (name.equalsIgnoreCase ("timbre"))         return MidiTarget::Tone;
        if (name.equalsIgnoreCase ("expression"))     return MidiTarget::Expression;

        return MidiTarget::None;
    }

    double getDouble (const juce::DynamicObject& object, const char* key, double fallback)
    {
        return object.hasProperty (key) ? (double) object.getProperty (key) : fallback;
    }
}

//==============================================================================
bool ControllerProfile::hasPerStringRouting() const noexcept
{
    for (const auto& routing : perString)
        if (routing.channel > 0)
            return true;

    return false;
}

int ControllerProfile::stringForChannel (int channel) const noexcept
{
    for (int s = 0; s < kMaxStrings; ++s)
        if (perString[(size_t) s].channel == channel)
            return s;

    return -1;
}

double ControllerProfile::applyPitchCurve (double normalised) const noexcept
{
    const double clamped = juce::jlimit (-1.0, 1.0, normalised);

    if (pitchCurve.size() < 2)
        return clamped;

    // The curve describes the positive half; the negative half is its mirror,
    // because no controller bends differently in one direction by design.
    const double magnitude = std::abs (clamped);
    const double sign = (clamped < 0.0) ? -1.0 : 1.0;

    const double position = magnitude * (double) (pitchCurve.size() - 1);
    const int lower = juce::jlimit (0, (int) pitchCurve.size() - 1, (int) position);
    const int upper = juce::jmin ((int) pitchCurve.size() - 1, lower + 1);

    const double fraction = position - (double) lower;

    const double value = pitchCurve[(size_t) lower]
                           + (pitchCurve[(size_t) upper] - pitchCurve[(size_t) lower]) * fraction;

    return sign * juce::jlimit (0.0, 1.0, value);
}

//==============================================================================
juce::var ControllerProfile::toVar() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("id", id);
    root->setProperty ("display_name", displayName);
    root->setProperty ("mode", getControllerModeName (mode));

    if (hasPerStringRouting())
    {
        auto* strings = new juce::DynamicObject();

        for (int s = 0; s < kMaxStrings; ++s)
        {
            const auto& routing = perString[(size_t) s];

            if (routing.channel <= 0)
                continue;

            auto* entry = new juce::DynamicObject();
            entry->setProperty ("channel", routing.channel);
            entry->setProperty ("pitch_bend_semis", routing.pitchBendSemis);

            // Strings are numbered from one in the file, the way a guitarist
            // numbers them, and from zero in the code.
            strings->setProperty (juce::String (s + 1), juce::var (entry));
        }

        root->setProperty ("per_string", juce::var (strings));
    }

    root->setProperty ("mpe_master_channel", mpeMasterChannel);
    root->setProperty ("mpe_first_member_channel", mpeFirstMemberChannel);
    root->setProperty ("mpe_last_member_channel", mpeLastMemberChannel);
    root->setProperty ("pitch_bend_semis", pitchBendSemis);
    root->setProperty ("member_pitch_bend_semis", memberPitchBendSemis);

    auto* cc = new juce::DynamicObject();

    for (int number = 0; number < 128; ++number)
        if (ccMap[(size_t) number] != MidiTarget::None)
            cc->setProperty (juce::String (number),
                             juce::String (getMidiTargetName (ccMap[(size_t) number])));

    root->setProperty ("cc_map", juce::var (cc));

    root->setProperty ("latency_ms_default", latencyMsDefault);

    if (latencyMsMeasured >= 0.0)
        root->setProperty ("latency_ms_measured", latencyMsMeasured);

    root->setProperty ("pitch_dead_zone_cents", pitchDeadZoneCents);
    root->setProperty ("minimum_note_ms", minimumNoteDurationMs);
    root->setProperty ("rows_as_strings", rowsAsStrings);
    root->setProperty ("notes", notes);

    if (! pitchCurve.empty())
    {
        juce::Array<juce::var> curve;

        for (double value : pitchCurve)
            curve.add (value);

        root->setProperty ("pitch_curve", curve);
    }

    return { root };
}

ControllerProfile ControllerProfile::fromVar (const juce::var& state)
{
    ControllerProfile profile;

    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return profile;

    profile.id = root->getProperty ("id").toString();
    profile.displayName = root->getProperty ("display_name").toString();

    if (profile.displayName.isEmpty())
        profile.displayName = profile.id;

    profile.mode = controllerModeFromName (root->getProperty ("mode").toString());

    if (auto* strings = root->getProperty ("per_string").getDynamicObject())
    {
        for (const auto& property : strings->getProperties())
        {
            const int stringNumber = property.name.toString().getIntValue();
            const int index = stringNumber - 1;

            if (! juce::isPositiveAndBelow (index, kMaxStrings))
                continue;

            if (auto* entry = property.value.getDynamicObject())
            {
                auto& routing = profile.perString[(size_t) index];

                routing.channel = juce::jlimit (0, 16, (int) entry->getProperty ("channel"));
                routing.pitchBendSemis = juce::jlimit (
                    0.0, 96.0, getDouble (*entry, "pitch_bend_semis", 2.0));
            }
        }
    }

    profile.mpeMasterChannel      = juce::jlimit (1, 16, (int) getDouble (*root, "mpe_master_channel", 1.0));
    profile.mpeFirstMemberChannel = juce::jlimit (1, 16, (int) getDouble (*root, "mpe_first_member_channel", 2.0));
    profile.mpeLastMemberChannel  = juce::jlimit (1, 16, (int) getDouble (*root, "mpe_last_member_channel", 16.0));

    profile.pitchBendSemis       = juce::jlimit (0.0, 96.0, getDouble (*root, "pitch_bend_semis", 2.0));
    profile.memberPitchBendSemis = juce::jlimit (0.0, 96.0, getDouble (*root, "member_pitch_bend_semis", 48.0));

    profile.ccMap.fill (MidiTarget::None);

    if (auto* cc = root->getProperty ("cc_map").getDynamicObject())
    {
        for (const auto& property : cc->getProperties())
        {
            const int number = property.name.toString().getIntValue();

            if (juce::isPositiveAndBelow (number, 128))
                profile.ccMap[(size_t) number] = targetFromName (property.value.toString());
        }
    }

    profile.latencyMsDefault  = juce::jlimit (0.0, 200.0, getDouble (*root, "latency_ms_default", 0.0));
    profile.latencyMsMeasured = root->hasProperty ("latency_ms_measured")
                                  ? juce::jlimit (0.0, 200.0, getDouble (*root, "latency_ms_measured", 0.0))
                                  : -1.0;

    profile.pitchDeadZoneCents    = juce::jlimit (0.0, 100.0, getDouble (*root, "pitch_dead_zone_cents", 5.0));
    profile.minimumNoteDurationMs = juce::jlimit (0.0, 1000.0, getDouble (*root, "minimum_note_ms", 0.0));
    profile.rowsAsStrings         = (bool) root->getProperty ("rows_as_strings");
    profile.notes                 = root->getProperty ("notes").toString();

    if (const auto* curve = root->getProperty ("pitch_curve").getArray())
        for (const auto& value : *curve)
            profile.pitchCurve.push_back (juce::jlimit (0.0, 1.0, (double) value));

    return profile;
}

bool ControllerProfile::loadFrom (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.getDynamicObject() == nullptr)
        return false;

    auto loaded = fromVar (parsed);

    if (! loaded.isValid())
        return false;

    *this = std::move (loaded);
    return true;
}

bool ControllerProfile::saveTo (const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (toVar(), false));
}

//==============================================================================
namespace
{
    /** Builds a hex-pickup profile: six strings on consecutive channels starting
        at `firstChannel`, with the high E first, which is how every hex pickup
        in the ship list is laid out. */
    ControllerProfile makeHexProfile (const char* id, const char* displayName,
                                      int firstChannel, double bendSemis,
                                      double latencyMs, const char* notes)
    {
        ControllerProfile profile;

        profile.id = id;
        profile.displayName = displayName;
        profile.mode = ControllerMode::perChannel;
        profile.pitchBendSemis = bendSemis;
        profile.latencyMsDefault = latencyMs;
        profile.notes = notes;

        for (int s = 0; s < 6; ++s)
        {
            profile.perString[(size_t) s].channel = firstChannel + s;
            profile.perString[(size_t) s].pitchBendSemis = bendSemis;
        }

        profile.ccMap.fill (MidiTarget::None);

        return profile;
    }

    ControllerProfile makeMpeProfile (const char* id, const char* displayName,
                                      double memberBendSemis, double latencyMs,
                                      const char* notes)
    {
        ControllerProfile profile;

        profile.id = id;
        profile.displayName = displayName;
        profile.mode = ControllerMode::mpe;
        profile.mpeMasterChannel = 1;
        profile.mpeFirstMemberChannel = 2;
        profile.mpeLastMemberChannel = 16;
        profile.pitchBendSemis = 2.0;
        profile.memberPitchBendSemis = memberBendSemis;
        profile.latencyMsDefault = latencyMs;
        profile.notes = notes;

        profile.ccMap.fill (MidiTarget::None);

        // CC 74 is MPE's third dimension, and on a guitar the natural home for it
        // is the tone control rather than a pitch effect.
        profile.ccMap[74] = MidiTarget::Tone;

        return profile;
    }
}

//==============================================================================
ControllerProfileLibrary::ControllerProfileLibrary()
{
    addFactoryProfiles();
}

void ControllerProfileLibrary::addFactoryProfiles()
{
    profiles.clear();

    // ---- generic ---------------------------------------------------------------------
    {
        ControllerProfile profile;
        profile.id = "generic-midi";
        profile.displayName = "Generic MIDI";
        profile.mode = ControllerMode::standard;
        profile.pitchBendSemis = 2.0;
        profile.pitchDeadZoneCents = 0.0;
        profile.notes = "Channel 1, ordinary MIDI, no per-string routing.";
        profile.ccMap.fill (MidiTarget::None);
        profile.ccMap[1] = MidiTarget::VibratoDepth;
        profile.ccMap[11] = MidiTarget::Expression;
        profiles.push_back (profile);
    }

    {
        auto profile = makeMpeProfile ("generic-mpe", "Generic MPE (Lower Zone)", 48.0, 0.0,
                                       "Master channel 1, members 2-16.");
        profiles.push_back (profile);
    }

    // ---- MPE hardware ----------------------------------------------------------------
    {
        auto profile = makeMpeProfile ("roli-seaboard", "ROLI Seaboard", 48.0, 0.0,
                                       "CC 74 drives the whammy by default; "
                                       "aftertouch drives vibrato depth.");
        // controllers.md 1: the Seaboard's Y axis goes to the whammy, not the tone.
        profile.ccMap[74] = MidiTarget::WhammyBar;
        profiles.push_back (profile);
    }

    {
        auto profile = makeMpeProfile ("linnstrument", "LinnStrument", 48.0, 0.0,
                                       "Enable guitar mode in Options to map rows "
                                       "to strings.");
        profile.rowsAsStrings = false;
        profiles.push_back (profile);
    }

    {
        auto profile = makeMpeProfile ("osmose", "Expressive E Osmose", 48.0, 0.0,
                                       "Continuous pitch with a non-linear response; "
                                       "the profile carries its curve.");

        /*  The Osmose's key travel is not proportional to the pitch it sends: the
            first part of the press moves very little and the rest accelerates.
            Sampling that shape here lets the interpreter undo it, so a given
            physical displacement bends by the amount the player expects. */
        profile.pitchCurve = { 0.0, 0.035, 0.09, 0.17, 0.27, 0.39,
                               0.52, 0.65, 0.77, 0.89, 1.0 };

        profiles.push_back (profile);
    }

    // ---- hex pickups -----------------------------------------------------------------
    {
        // controllers.md 1: GK puts the high E on channel 11.
        auto profile = makeHexProfile ("roland-gk", "Roland GK Series", 11, 24.0, 3.0,
                                       "GK pickup latency typical for GR-55; "
                                       "calibrate for GR-D.");
        profile.ccMap[74] = MidiTarget::WhammyBar;
        profile.ccMap[1] = MidiTarget::VibratoDepth;
        profiles.push_back (profile);
    }

    {
        auto profile = makeHexProfile ("fishman-tripleplay", "Fishman TriplePlay", 1, 24.0, 5.0,
                                       "Per-string on channels 1-6, configurable "
                                       "in the TriplePlay software.");
        profiles.push_back (profile);
    }

    {
        auto profile = makeHexProfile ("jamstik", "Jamstik", 1, 12.0, 8.0,
                                       "Covers Jamstik, Jamstik+ and Jamstik Studio.");
        profiles.push_back (profile);
    }

    // ---- guitar-shaped controllers ------------------------------------------------------
    {
        ControllerProfile profile;
        profile.id = "yamaha-ez-eg";
        profile.displayName = "Guitar-shaped MIDI (Yamaha EZ-EG and similar)";
        profile.mode = ControllerMode::standard;
        profile.pitchBendSemis = 2.0;
        profile.latencyMsDefault = 6.0;
        profile.minimumNoteDurationMs = 40.0;
        profile.notes = "Standard MIDI on channel 1, no per-string. The CC map "
                        "varies between units; these are sensible defaults.";
        profile.ccMap.fill (MidiTarget::None);
        profile.ccMap[1] = MidiTarget::VibratoDepth;
        profile.ccMap[11] = MidiTarget::Expression;
        profiles.push_back (profile);
    }
}

//==============================================================================
juce::File ControllerProfileLibrary::getFactoryDirectory()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
             .getParentDirectory()
             .getChildFile ("Resources")
             .getChildFile ("Controllers");
}

juce::File ControllerProfileLibrary::getUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Controllers");
}

void ControllerProfileLibrary::refresh()
{
    addFactoryProfiles();

    scanDirectory (getFactoryDirectory());

    // controllers.md 0.4: a user override of the same id wins, so the user
    // directory is scanned last.
    scanDirectory (getUserDirectory());
}

void ControllerProfileLibrary::scanDirectory (const juce::File& directory)
{
    if (! directory.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.json"))
    {
        ControllerProfile profile;

        if (! profile.loadFrom (entry.getFile()))
            continue;

        const int existing = indexOf (profile.id);

        if (existing >= 0)
            profiles[(size_t) existing] = profile;
        else
            profiles.push_back (profile);
    }
}

const ControllerProfile& ControllerProfileLibrary::getProfile (int index) const noexcept
{
    static const ControllerProfile empty;

    return juce::isPositiveAndBelow (index, (int) profiles.size())
             ? profiles[(size_t) index] : empty;
}

int ControllerProfileLibrary::indexOf (const juce::String& id) const
{
    for (size_t i = 0; i < profiles.size(); ++i)
        if (profiles[i].id == id)
            return (int) i;

    return -1;
}

juce::StringArray ControllerProfileLibrary::getDisplayNames() const
{
    juce::StringArray names;

    for (const auto& profile : profiles)
        names.add (profile.displayName);

    return names;
}

bool ControllerProfileLibrary::save (const ControllerProfile& profile)
{
    if (! profile.isValid())
        return false;

    const int existing = indexOf (profile.id);

    if (existing >= 0)
        profiles[(size_t) existing] = profile;
    else
        profiles.push_back (profile);

    return profile.saveTo (getUserDirectory().getChildFile (
        juce::File::createLegalFileName (profile.id) + ".json"));
}

//==============================================================================
void ControllerProfileLibrary::apply (const ControllerProfile& profile,
                                      MidiInterpreter& interpreter)
{
    // CT-9: "Guitar mode" turns a normally-MPE controller (LinnStrument) into a
    // per-channel one for the duration of the toggle, regardless of its own
    // mode field. LinnStrument's own default Guitar-mode channel assignment
    // puts the lowest row on channel 2; Luthier only needs six of its rows, so
    // rows 1-6 map to channels 2-7, high string to low, at MPE's 48-semitone
    // bend range (the controller does not change its own bend behaviour when
    // Guitar mode is enabled).
    if (profile.rowsAsStrings)
    {
        interpreter.setMpeEnabled (false);
        interpreter.setPlayingMode (PlayingMode::GuitarController);
        interpreter.setPitchBendRange (48.0);

        constexpr int kGuitarModeFirstChannel = 2;
        constexpr int kGuitarModeStrings = 6;

        for (int s = 0; s < kMaxStrings; ++s)
        {
            const int channel = (s < kGuitarModeStrings) ? (kGuitarModeFirstChannel + s) : 0;
            interpreter.setChannelForString (s, channel);

            if (channel > 0)
                interpreter.setStringBendRange (s, 48.0);
        }

        return;
    }

    switch (profile.mode)
    {
        case ControllerMode::mpe:
            interpreter.setMpeEnabled (true);
            interpreter.setPlayingMode (PlayingMode::GuitarController);
            interpreter.setPitchBendRange (profile.memberPitchBendSemis);
            interpreter.setMpeMasterChannel (profile.mpeMasterChannel);   // CT-17
            interpreter.resetChannelMap();
            break;

        case ControllerMode::perChannel:
            interpreter.setMpeEnabled (false);
            interpreter.setPlayingMode (PlayingMode::GuitarController);
            interpreter.setPitchBendRange (profile.pitchBendSemis);

            for (int s = 0; s < kMaxStrings; ++s)
            {
                const auto& routing = profile.perString[(size_t) s];

                interpreter.setChannelForString (s, routing.channel);

                if (routing.channel > 0)
                    interpreter.setStringBendRange (s, routing.pitchBendSemis);
            }
            break;

        case ControllerMode::standard:
        case ControllerMode::numModes:
        default:
            interpreter.setMpeEnabled (false);
            interpreter.setPitchBendRange (profile.pitchBendSemis);
            interpreter.resetChannelMap();

            // A standard controller says nothing about how the player wants to
            // play, so the playing mode is deliberately left alone here.
            break;
    }

    // The CC map is replaced wholesale rather than merged, so that switching
    // profiles cannot leave a mapping behind from the previous one.
    interpreter.resetCcMapToDefaults();

    for (int number = 0; number < 128; ++number)
        if (profile.ccMap[(size_t) number] != MidiTarget::None)
            interpreter.setCcTarget (number, profile.ccMap[(size_t) number]);

    interpreter.setPitchDeadZoneCents (profile.pitchDeadZoneCents);
    interpreter.setMinimumNoteDurationMs (profile.minimumNoteDurationMs);
}

//==============================================================================
void LatencyWizard::begin (int runs) noexcept
{
    runsWanted = juce::jlimit (1, kMaxRuns, runs);
    numMeasurements = 0;
    running = true;

    measurements.fill (0.0);
}

bool LatencyWizard::addMeasurement (double millisecondsAfterClick) noexcept
{
    if (! running)
        return false;

    // A gap outside this range is not a measurement of anything: the player
    // missed the click entirely, or hit the one before.
    if (millisecondsAfterClick < -200.0 || millisecondsAfterClick > 500.0)
        return false;

    if (numMeasurements < kMaxRuns)
        measurements[(size_t) numMeasurements++] = millisecondsAfterClick;

    if (numMeasurements >= runsWanted)
    {
        running = false;
        return true;
    }

    return false;
}

double LatencyWizard::getMeasuredLatencyMs() const noexcept
{
    if (numMeasurements <= 0)
        return 0.0;

    // The median, not the mean: a player who fluffs one run should not shift the
    // answer, and the whole point of taking ten runs is to be able to ignore the
    // bad ones.
    std::array<double, kMaxRuns> sorted = measurements;
    std::sort (sorted.begin(), sorted.begin() + numMeasurements);

    return (numMeasurements % 2 == 1)
             ? sorted[(size_t) (numMeasurements / 2)]
             : 0.5 * (sorted[(size_t) (numMeasurements / 2 - 1)]
                        + sorted[(size_t) (numMeasurements / 2)]);
}

double LatencyWizard::getSigmaMs() const noexcept
{
    if (numMeasurements < 2)
        return 0.0;

    double sum = 0.0;

    for (int i = 0; i < numMeasurements; ++i)
        sum += measurements[(size_t) i];

    const double mean = sum / (double) numMeasurements;

    double variance = 0.0;

    for (int i = 0; i < numMeasurements; ++i)
    {
        const double d = measurements[(size_t) i] - mean;
        variance += d * d;
    }

    return std::sqrt (variance / (double) (numMeasurements - 1));
}

//==============================================================================
void ControllerMerge::reset() noexcept
{
    owner.fill (-1);
    claimedAt.fill (0);
    sounding.fill (false);

    contention = 0;
}

bool ControllerMerge::claim (int stringIndex, int source, int64_t timestamp) noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return false;

    const int current = owner[(size_t) stringIndex];

    // controllers.md 6: the most recent event wins, per string. A string held by
    // another source is taken, but taking one that is still sounding is the
    // contention the spec wants reported.
    if (current >= 0 && current != source && sounding[(size_t) stringIndex])
        ++contention;

    owner[(size_t) stringIndex] = source;
    claimedAt[(size_t) stringIndex] = timestamp;
    sounding[(size_t) stringIndex] = true;

    return true;
}

void ControllerMerge::release (int stringIndex, int source) noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    // Only the source that owns the string may end its note. A stale note-off
    // from the source that was displaced must not cut short the note that
    // replaced it.
    if (owner[(size_t) stringIndex] != source)
        return;

    sounding[(size_t) stringIndex] = false;
    owner[(size_t) stringIndex] = -1;
}

int ControllerMerge::getOwner (int stringIndex) const noexcept
{
    return juce::isPositiveAndBelow (stringIndex, kMaxStrings)
             ? owner[(size_t) stringIndex] : -1;
}

} // namespace luthier
