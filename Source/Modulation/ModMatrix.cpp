#include "ModMatrix.h"

namespace luthier
{

//==============================================================================
namespace
{
    /** Parses the trailing number of an id like "lfo3", returning the
        zero-based index, or -1 if the prefix does not match or the number is
        out of range. */
    int indexedSlot (const juce::String& id, const juce::String& prefix,
                     int base, int count) noexcept
    {
        if (! id.startsWith (prefix))
            return -1;

        const auto tail = id.substring (prefix.length());

        if (tail.isEmpty() || ! tail.containsOnly ("0123456789"))
            return -1;

        const int oneBased = tail.getIntValue();

        if (oneBased < 1 || oneBased > count)
            return -1;

        return base + oneBased - 1;
    }
}

int modSourceSlotForId (const juce::String& sourceId) noexcept
{
    const auto id = sourceId.trim().toLowerCase();

    if (id.isEmpty())
        return -1;

    if (auto s = indexedSlot (id, "lfo", ModSourceSlots::lfoBase, ModSourceSlots::numLfos); s >= 0)
        return s;

    if (auto s = indexedSlot (id, "env", ModSourceSlots::envBase, ModSourceSlots::numEnvelopes); s >= 0)
        return s;

    if (auto s = indexedSlot (id, "seq", ModSourceSlots::seqBase, ModSourceSlots::numSequencers); s >= 0)
        return s;

    if (auto s = indexedSlot (id, "follow", ModSourceSlots::followerBase, ModSourceSlots::numFollowers); s >= 0)
        return s;

    if (auto s = indexedSlot (id, "macro", ModSourceSlots::macroBase, ModSourceSlots::numMacros); s >= 0)
        return s;

    // The 14-bit form is checked before the plain one, because "cc14b0" also
    // starts with "cc".
    if (auto s = indexedSlot (id, "cc14b", ModSourceSlots::cc14Base + 1, ModSourceSlots::numCc14 - 1); s >= 0)
        return s;

    if (id == "cc14b0")
        return ModSourceSlots::cc14Base;

    if (id.startsWith ("cc"))
    {
        const auto tail = id.substring (2);

        if (tail.isNotEmpty() && tail.containsOnly ("0123456789"))
        {
            const int cc = tail.getIntValue();

            if (juce::isPositiveAndBelow (cc, ModSourceSlots::numCcs))
                return ModSourceSlots::ccBase + cc;
        }

        return -1;
    }

    if (id == "notepitch")      return ModSourceSlots::notePitch;
    if (id == "notevelocity")   return ModSourceSlots::noteVelocity;
    if (id == "notetrigger")    return ModSourceSlots::noteTrigger;
    if (id == "notesheld")      return ModSourceSlots::notesHeld;
    if (id == "aftertouch")     return ModSourceSlots::aftertouch;
    if (id == "polyaftertouch") return ModSourceSlots::polyAftertouch;
    if (id == "randnote")       return ModSourceSlots::randomPerNote;
    if (id == "randbar")        return ModSourceSlots::randomPerBar;
    if (id == "randsmooth")     return ModSourceSlots::randomSmooth;
    if (id == "pitchbend")      return ModSourceSlots::pitchBend;
    if (id == "modwheel")       return ModSourceSlots::modWheel;
    if (id == "pressure")       return ModSourceSlots::channelPressure;

    return -1;
}

juce::String modSourceIdForSlot (int slot)
{
    using namespace ModSourceSlots;

    if (slot < 0 || slot >= count)
        return {};

    if (slot < envBase)      return "lfo" + juce::String (slot - lfoBase + 1);
    if (slot < seqBase)      return "env" + juce::String (slot - envBase + 1);
    if (slot < followerBase) return "seq" + juce::String (slot - seqBase + 1);
    if (slot < notePitch)    return "follow" + juce::String (slot - followerBase + 1);

    switch (slot)
    {
        case notePitch:       return "notePitch";
        case noteVelocity:    return "noteVelocity";
        case noteTrigger:     return "noteTrigger";
        case notesHeld:       return "notesHeld";
        case aftertouch:      return "aftertouch";
        case polyAftertouch:  return "polyAftertouch";
        case randomPerNote:   return "randNote";
        case randomPerBar:    return "randBar";
        case randomSmooth:    return "randSmooth";
        case pitchBend:       return "pitchBend";
        case modWheel:        return "modWheel";
        case channelPressure: return "pressure";
        default:              break;
    }

    if (slot >= macroBase && slot < macroBase + numMacros)
        return "macro" + juce::String (slot - macroBase + 1);

    if (slot >= ccBase && slot < ccBase + numCcs)
        return "cc" + juce::String (slot - ccBase);

    if (slot >= cc14Base && slot < cc14Base + numCc14)
        return "cc14b" + juce::String (slot - cc14Base);

    return {};
}

juce::String modSourceDisplayName (int slot)
{
    using namespace ModSourceSlots;

    if (slot < 0 || slot >= count)
        return {};

    if (slot < envBase)      return "LFO " + juce::String (slot - lfoBase + 1);
    if (slot < seqBase)      return "Env " + juce::String (slot - envBase + 1);
    if (slot < followerBase) return "Seq " + juce::String (slot - seqBase + 1);
    if (slot < notePitch)    return "Follower " + juce::String (slot - followerBase + 1);

    switch (slot)
    {
        case notePitch:       return "Note Pitch";
        case noteVelocity:    return "Velocity";
        case noteTrigger:     return "Note Trigger";
        case notesHeld:       return "Notes Held";
        case aftertouch:      return "Aftertouch";
        case polyAftertouch:  return "Poly Aftertouch";
        case randomPerNote:   return "Random (note)";
        case randomPerBar:    return "Random (bar)";
        case randomSmooth:    return "Random (smooth)";
        case pitchBend:       return "Pitch Bend";
        case modWheel:        return "Mod Wheel";
        case channelPressure: return "Pressure";
        default:              break;
    }

    if (slot >= macroBase && slot < macroBase + numMacros)
        return "Macro " + juce::String (slot - macroBase + 1);

    if (slot >= ccBase && slot < ccBase + numCcs)
        return "CC " + juce::String (slot - ccBase);

    if (slot >= cc14Base && slot < cc14Base + numCc14)
        return "CC " + juce::String (slot - cc14Base) + " (14-bit)";

    return {};
}

//==============================================================================
ModMatrix::ModMatrix()
{
    for (auto& v : sourceValues)
        v.store (0.0f);

    for (auto& v : ccValues)
        v.store (0.0f);

    for (auto& v : macroValues)
        v.store (0.0f);

    // The two followers default to watching different things, because two
    // followers on the same signal is never what anyone wanted. Set here, not
    // in prepare(), which a host calls after restoring a session.
    followers[0].setSource (ModEnvelopeFollower::Source::mainOutput);

    if (followers.size() > 1)
        followers[1].setSource (ModEnvelopeFollower::Source::sidechain);
}

ModMatrix::~ModMatrix() = default;

//==============================================================================
void ModMatrix::prepare (double newSampleRate, int newBlockSize,
                         juce::AudioProcessorValueTreeState& state)
{
    apvts = &state;
    sampleRate = newSampleRate;
    blockSizeSamples = juce::jmax (1, newBlockSize);

    // modulation-matrix 0.1: a thirty-second of the block, floored at 128.
    controlRateSamples = juce::jmax (128, blockSizeSamples / 32);
    controlRateHz = sampleRate / (double) controlRateSamples;

    // --- sources ---------------------------------------------------------------
    // Each source gets its own seed, derived from its slot, so two LFOs set to
    // "random" are not the same random.
    for (size_t i = 0; i < lfos.size(); ++i)
        lfos[i].prepare (controlRateHz, 0x51A5E1D0ull + i * 0x9E3779B9ull);

    for (auto& e : envelopes)
        e.prepare (controlRateHz);

    for (size_t i = 0; i < sequencers.size(); ++i)
        sequencers[i].prepare (controlRateHz, 0x5EE9D000ull + i * 0x9E3779B9ull);

    for (auto& f : followers)
        f.prepare (controlRateHz);

    randomSource.prepare (controlRateHz, 0xD1CED1CEull);

    // --- destinations ------------------------------------------------------------
    const auto& parameters = state.processor.getParameters();
    const int numParameters = parameters.size();

    destinations.assign ((size_t) numParameters, {});
    currentOffsets = std::vector<std::atomic<float>> ((size_t) numParameters);
    targetOffsets = std::vector<std::atomic<float>> ((size_t) numParameters);
    destinationModulated = std::vector<std::atomic<bool>> ((size_t) numParameters);

    parameterIndexById.clear();

    for (int i = 0; i < numParameters; ++i)
    {
        currentOffsets[(size_t) i].store (0.0f);
        targetOffsets[(size_t) i].store (0.0f);
        destinationModulated[(size_t) i].store (false);

        auto& info = destinations[(size_t) i];

        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
        {
            parameterIndexById.set (withId->paramID, i);

            const auto range = state.getParameterRange (withId->paramID);
            info.minimum = range.start;
            info.maximum = range.end;
            info.range = juce::jmax (1.0e-9f, range.end - range.start);

            // Choice and bool parameters are the discrete destinations of
            // modulation-matrix 4; their step count is what the threshold rule
            // divides the modulation position by.
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameters[i]))
            {
                info.discrete = true;
                info.numSteps = choice->choices.size();
            }
            else if (dynamic_cast<juce::AudioParameterBool*> (parameters[i]) != nullptr)
            {
                info.discrete = true;
                info.numSteps = 2;
            }
        }
    }

    // Both tables start empty, and any routes already loaded are recompiled
    // against the parameter list we have just learned.
    rebuildTable();
    reset();
}

void ModMatrix::resetEnvelopes() noexcept
{
    for (auto& e : envelopes)
        e.reset();
}

void ModMatrix::reset() noexcept
{
    for (auto& l : lfos)       l.reset();
    for (auto& e : envelopes)  e.reset();
    for (auto& s : sequencers) s.reset();
    for (auto& f : followers)  f.reset();

    randomSource.reset();

    for (auto& v : sourceValues)
        v.store (0.0f);

    for (size_t i = 0; i < currentOffsets.size(); ++i)
    {
        currentOffsets[i].store (0.0f);
        targetOffsets[i].store (0.0f);
    }

    lastNotePitch = 0.5;
    lastNoteVelocity = 0.0;
    notesHeldCount = 0;
    noteTriggerTicks = 0;
    aftertouchValue = 0.0;
    polyAftertouchValue = 0.0;
    pitchBendValue = 0.0;
    lastBarPosition = -1.0;

    samplesUntilTick = 0;
}

void ModMatrix::releaseResources()
{
    destinations.clear();
    currentOffsets.clear();
    targetOffsets.clear();
    destinationModulated.clear();
    parameterIndexById.clear();

    tables[0] = Table {};
    tables[1] = Table {};
    liveTable.store (0);
    anyRoutes.store (false);
}

//==============================================================================
int ModMatrix::destinationIndexFor (const juce::String& parameterId) const
{
    return parameterIndexById.contains (parameterId) ? parameterIndexById[parameterId] : -1;
}

void ModMatrix::rebuildTable()
{
    // Build into whichever table is not live, then flip. The audio thread keeps
    // reading the old one until the store below, and reads the new one after.
    const int building = 1 - liveTable.load (std::memory_order_relaxed);
    auto& table = tables[(size_t) building];

    table.routes.clear();
    table.touchedDestinations.clear();

    table.routes.reserve ((size_t) kMaxRoutes);
    table.touchedDestinations.reserve (destinations.size());

    std::vector<bool> touched (destinations.size(), false);

    for (const auto& route : routes)
    {
        if (! route.enabled)
            continue;

        const int slot = modSourceSlotForId (route.sourceId);
        const int destination = destinationIndexFor (route.destinationId);

        if (slot < 0 || destination < 0)
            continue;

        CompiledRoute compiled;
        compiled.sourceSlot = slot;
        compiled.destinationIndex = destination;
        compiled.depth = juce::jlimit (-1.0f, 1.0f, route.depth);
        compiled.offset = juce::jlimit (-1.0f, 1.0f, route.offset);
        compiled.curve = route.curve;

        table.routes.push_back (compiled);

        if (! touched[(size_t) destination])
        {
            touched[(size_t) destination] = true;
            table.touchedDestinations.push_back (destination);
        }
    }

    // A destination that has just lost its last route must go back to zero, or
    // it would keep the offset it happened to have when the route was removed.
    for (size_t i = 0; i < destinationModulated.size(); ++i)
    {
        const bool nowModulated = touched[i];

        if (destinationModulated[i].load (std::memory_order_relaxed) && ! nowModulated)
        {
            targetOffsets[i].store (0.0f, std::memory_order_relaxed);
            currentOffsets[i].store (0.0f, std::memory_order_relaxed);
        }

        destinationModulated[i].store (nowModulated, std::memory_order_relaxed);
    }

    liveTable.store (building, std::memory_order_release);
    anyRoutes.store (! table.routes.empty(), std::memory_order_relaxed);
}

//==============================================================================
bool ModMatrix::addRoute (const ModRoute& route)
{
    const juce::ScopedLock sl (routeLock);

    if ((int) routes.size() >= kMaxRoutes)
        return false;

    if (modSourceSlotForId (route.sourceId) < 0)
        return false;

    if (destinationIndexFor (route.destinationId) < 0)
        return false;

    int existing = 0;

    for (const auto& r : routes)
        if (r.destinationId == route.destinationId)
            ++existing;

    if (existing >= kMaxRoutesPerDestination)
        return false;

    routes.push_back (route);
    rebuildTable();
    return true;
}

void ModMatrix::removeRoute (int index)
{
    const juce::ScopedLock sl (routeLock);

    if (! juce::isPositiveAndBelow (index, (int) routes.size()))
        return;

    routes.erase (routes.begin() + index);
    rebuildTable();
}

void ModMatrix::clearRoutes()
{
    const juce::ScopedLock sl (routeLock);
    routes.clear();
    unknownDestinations.clear();
    droppedOnLoad = 0;
    rebuildTable();
}

void ModMatrix::setRouteEnabled (int index, bool enabled)
{
    const juce::ScopedLock sl (routeLock);

    if (juce::isPositiveAndBelow (index, (int) routes.size()))
    {
        routes[(size_t) index].enabled = enabled;
        rebuildTable();
    }
}

void ModMatrix::setRouteDepth (int index, float depth)
{
    const juce::ScopedLock sl (routeLock);

    if (juce::isPositiveAndBelow (index, (int) routes.size()))
    {
        routes[(size_t) index].depth = juce::jlimit (-1.0f, 1.0f, depth);
        rebuildTable();
    }
}

void ModMatrix::setRouteOffset (int index, float offset)
{
    const juce::ScopedLock sl (routeLock);

    if (juce::isPositiveAndBelow (index, (int) routes.size()))
    {
        routes[(size_t) index].offset = juce::jlimit (-1.0f, 1.0f, offset);
        rebuildTable();
    }
}

void ModMatrix::setRouteCurve (int index, ModCurve curve)
{
    const juce::ScopedLock sl (routeLock);

    if (juce::isPositiveAndBelow (index, (int) routes.size()))
    {
        routes[(size_t) index].curve = curve;
        rebuildTable();
    }
}

int ModMatrix::getNumRoutes() const
{
    const juce::ScopedLock sl (routeLock);
    return (int) routes.size();
}

ModRoute ModMatrix::getRoute (int index) const
{
    const juce::ScopedLock sl (routeLock);

    return juce::isPositiveAndBelow (index, (int) routes.size()) ? routes[(size_t) index] : ModRoute {};
}

std::vector<ModRoute> ModMatrix::getRoutes() const
{
    const juce::ScopedLock sl (routeLock);
    return routes;
}

void ModMatrix::setRoutes (const std::vector<ModRoute>& newRoutes)
{
    const juce::ScopedLock sl (routeLock);

    routes.clear();
    unknownDestinations.clear();
    droppedOnLoad = 0;

    juce::HashMap<juce::String, int> perDestination;

    for (const auto& route : newRoutes)
    {
        if ((int) routes.size() >= kMaxRoutes)
        {
            ++droppedOnLoad;
            continue;
        }

        if (modSourceSlotForId (route.sourceId) < 0)
        {
            ++droppedOnLoad;
            continue;
        }

        // modulation-matrix 6: a preset made by a later version may name a
        // parameter this build does not have. That is worth telling the user
        // about, not worth refusing the preset over.
        if (destinationIndexFor (route.destinationId) < 0)
        {
            ++droppedOnLoad;
            unknownDestinations.addIfNotAlreadyThere (route.destinationId);
            continue;
        }

        const int already = perDestination.contains (route.destinationId)
                              ? perDestination[route.destinationId] : 0;

        if (already >= kMaxRoutesPerDestination)
        {
            ++droppedOnLoad;
            continue;
        }

        perDestination.set (route.destinationId, already + 1);
        routes.push_back (route);
    }

    rebuildTable();
}

juce::StringArray ModMatrix::getUnknownDestinations() const
{
    const juce::ScopedLock sl (routeLock);
    return unknownDestinations;
}

int ModMatrix::getRouteCountForDestination (const juce::String& destinationId) const
{
    const juce::ScopedLock sl (routeLock);

    int count = 0;

    for (const auto& r : routes)
        if (r.destinationId == destinationId)
            ++count;

    return count;
}

//==============================================================================
void ModMatrix::noteOn (int midiNote, double velocity) noexcept
{
    lastNotePitch = juce::jlimit (0.0, 1.0, (double) midiNote / 127.0);
    lastNoteVelocity = juce::jlimit (0.0, 1.0, velocity);
    ++notesHeldCount;

    // The trigger is an impulse: one control tick wide, which is exactly what
    // an envelope needs to see and no wider.
    noteTriggerTicks = 1;

    for (auto& l : lfos)
        l.noteOn();

    for (auto& e : envelopes)
        e.noteOn();

    randomSource.noteOn();
}

void ModMatrix::noteOff() noexcept
{
    notesHeldCount = juce::jmax (0, notesHeldCount - 1);

    if (notesHeldCount == 0)
        for (auto& e : envelopes)
            e.noteOff();
}

void ModMatrix::allNotesOff() noexcept
{
    notesHeldCount = 0;

    for (auto& e : envelopes)
        e.noteOff();
}

void ModMatrix::setAftertouch (double value) noexcept
{
    aftertouchValue = juce::jlimit (0.0, 1.0, value);
}

void ModMatrix::setPolyAftertouch (double value) noexcept
{
    polyAftertouchValue = juce::jlimit (0.0, 1.0, value);
}

void ModMatrix::setPitchBend (double bipolar) noexcept
{
    pitchBendValue = juce::jlimit (-1.0, 1.0, bipolar);
}

void ModMatrix::setControllerValue (int ccNumber, double value) noexcept
{
    if (juce::isPositiveAndBelow (ccNumber, 128))
        ccValues[(size_t) ccNumber].store ((float) juce::jlimit (0.0, 1.0, value),
                                           std::memory_order_relaxed);
}

void ModMatrix::setMacroValue (int macroIndex, double value) noexcept
{
    if (juce::isPositiveAndBelow (macroIndex, ModSourceSlots::numMacros))
        macroValues[(size_t) macroIndex].store ((float) juce::jlimit (0.0, 1.0, value),
                                                std::memory_order_relaxed);
}

//==============================================================================
ModLfo& ModMatrix::getLfo (int index) noexcept
{
    return lfos[(size_t) juce::jlimit (0, ModSourceSlots::numLfos - 1, index)];
}

ModEnvelope& ModMatrix::getEnvelope (int index) noexcept
{
    return envelopes[(size_t) juce::jlimit (0, ModSourceSlots::numEnvelopes - 1, index)];
}

ModStepSequencer& ModMatrix::getSequencer (int index) noexcept
{
    return sequencers[(size_t) juce::jlimit (0, ModSourceSlots::numSequencers - 1, index)];
}

ModEnvelopeFollower& ModMatrix::getFollower (int index) noexcept
{
    return followers[(size_t) juce::jlimit (0, ModSourceSlots::numFollowers - 1, index)];
}

float ModMatrix::getSourceValue (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, ModSourceSlots::count)
             ? sourceValues[(size_t) slot].load (std::memory_order_relaxed)
             : 0.0f;
}

//==============================================================================
void ModMatrix::updateSources (const ModBlockContext& context) noexcept
{
    using namespace ModSourceSlots;

    const double beatsPerTick = (context.bpm / 60.0) * ((double) controlRateSamples / sampleRate);

    if (context.transportJustStarted)
    {
        for (auto& l : lfos)
            l.transportStarted();

        for (auto& s : sequencers)
            s.transportStarted();
    }

    // A new bar redraws the per-bar random value.
    if (context.positionBeats >= 0.0)
    {
        const double bar = std::floor (context.positionBeats / 4.0);

        if (bar != lastBarPosition)
        {
            lastBarPosition = bar;
            randomSource.barStarted();
        }
    }

    for (int i = 0; i < numLfos; ++i)
        sourceValues[(size_t) (lfoBase + i)].store (
            (float) lfos[(size_t) i].tick (beatsPerTick, context.positionBeats),
            std::memory_order_relaxed);

    for (int i = 0; i < numEnvelopes; ++i)
        sourceValues[(size_t) (envBase + i)].store ((float) envelopes[(size_t) i].tick(),
                                                    std::memory_order_relaxed);

    for (int i = 0; i < numSequencers; ++i)
        sourceValues[(size_t) (seqBase + i)].store ((float) sequencers[(size_t) i].tick (beatsPerTick),
                                                    std::memory_order_relaxed);

    for (int i = 0; i < numFollowers; ++i)
    {
        auto& follower = followers[(size_t) i];

        double peak = 0.0, meanSquare = 0.0;

        switch (follower.getSource())
        {
            case ModEnvelopeFollower::Source::sidechain:
                peak = context.sidechainPeak;
                meanSquare = context.sidechainMeanSquare;
                break;

            case ModEnvelopeFollower::Source::pickup:
                peak = context.pickupPeak;
                meanSquare = context.pickupMeanSquare;
                break;

            case ModEnvelopeFollower::Source::perString:
            {
                const int s = juce::jlimit (0, kMaxStrings - 1, follower.getStringIndex());
                peak = context.perStringPeak[(size_t) s];

                // Only a peak is carried per string; squaring it is a fair
                // stand-in for the mean square of a decaying string.
                meanSquare = peak * peak;
                break;
            }

            case ModEnvelopeFollower::Source::mainOutput:
            default:
                peak = context.mainOutputPeak;
                meanSquare = context.mainOutputMeanSquare;
                break;
        }

        sourceValues[(size_t) (followerBase + i)].store ((float) follower.tick (peak, meanSquare),
                                                         std::memory_order_relaxed);
    }

    randomSource.tick();

    sourceValues[(size_t) randomPerNote].store ((float) randomSource.getPerNote(), std::memory_order_relaxed);
    sourceValues[(size_t) randomPerBar].store ((float) randomSource.getPerBar(), std::memory_order_relaxed);
    sourceValues[(size_t) randomSmooth].store ((float) randomSource.getSmooth(), std::memory_order_relaxed);

    sourceValues[(size_t) notePitch].store ((float) lastNotePitch, std::memory_order_relaxed);
    sourceValues[(size_t) noteVelocity].store ((float) lastNoteVelocity, std::memory_order_relaxed);
    sourceValues[(size_t) notesHeld].store (
        (float) juce::jlimit (0.0, 1.0, (double) notesHeldCount / (double) kMaxStrings),
        std::memory_order_relaxed);

    sourceValues[(size_t) noteTrigger].store (noteTriggerTicks > 0 ? 1.0f : 0.0f,
                                              std::memory_order_relaxed);
    noteTriggerTicks = juce::jmax (0, noteTriggerTicks - 1);

    sourceValues[(size_t) aftertouch].store ((float) aftertouchValue, std::memory_order_relaxed);
    sourceValues[(size_t) polyAftertouch].store ((float) polyAftertouchValue, std::memory_order_relaxed);
    sourceValues[(size_t) pitchBend].store ((float) pitchBendValue, std::memory_order_relaxed);

    for (int i = 0; i < numMacros; ++i)
        sourceValues[(size_t) (macroBase + i)].store (macroValues[(size_t) i].load (std::memory_order_relaxed),
                                                      std::memory_order_relaxed);

    for (int cc = 0; cc < numCcs; ++cc)
        sourceValues[(size_t) (ccBase + cc)].store (ccValues[(size_t) cc].load (std::memory_order_relaxed),
                                                    std::memory_order_relaxed);

    sourceValues[(size_t) modWheel].store (ccValues[1].load (std::memory_order_relaxed),
                                           std::memory_order_relaxed);
    sourceValues[(size_t) channelPressure].store ((float) aftertouchValue, std::memory_order_relaxed);

    // 14-bit pairs: controller n is the MSB, controller n + 32 the LSB, which is
    // the arrangement the MIDI specification defines.
    for (int i = 0; i < numCc14; ++i)
    {
        const float msb = ccValues[(size_t) i].load (std::memory_order_relaxed);
        const float lsb = ccValues[(size_t) (i + 32)].load (std::memory_order_relaxed);

        sourceValues[(size_t) (cc14Base + i)].store (
            juce::jlimit (0.0f, 1.0f, msb + lsb * (1.0f / 128.0f)), std::memory_order_relaxed);
    }
}

void ModMatrix::processBlock (int numSamples, const ModBlockContext& context) noexcept
{
    if (destinations.empty())
        return;

    // Control rate is per modulation-matrix 0.1 and does not follow the block
    // size: a host that alternates 64 and 448 sample blocks must not make the
    // LFOs speed up and slow down.
    int remaining = numSamples;

    while (remaining > 0)
    {
        if (samplesUntilTick <= 0)
        {
            updateSources (context);

            const int live = liveTable.load (std::memory_order_acquire);
            const auto& table = tables[(size_t) live];

            // Clear only the destinations this table touches. Zeroing all three
            // hundred parameters every tick would cost more than the routing.
            for (int destination : table.touchedDestinations)
                targetOffsets[(size_t) destination].store (0.0f, std::memory_order_relaxed);

            for (const auto& route : table.routes)
            {
                const float raw = sourceValues[(size_t) route.sourceSlot].load (std::memory_order_relaxed);
                const double shaped = applyModCurve ((double) raw, route.curve);

                const auto& info = destinations[(size_t) route.destinationIndex];

                // modulation-matrix 3: depth scales the source, offset is added
                // after, and the sum is in parameter units.
                const double contribution = (shaped * (double) route.depth + (double) route.offset)
                                              * (double) info.range;

                const auto previous = targetOffsets[(size_t) route.destinationIndex]
                                        .load (std::memory_order_relaxed);

                // In parameter units, so the guard is the destination's own span
                // (eight routes at full depth plus offset), not the audio
                // guard's +-4, which would pin a Hz or ms destination.
                const double sum = (double) previous + contribution;
                const double limit = 16.0 * (double) info.range;

                targetOffsets[(size_t) route.destinationIndex]
                    .store ((float) (std::isfinite (sum) ? juce::jlimit (-limit, limit, sum) : 0.0),
                            std::memory_order_relaxed);
            }

            // modulation-matrix 0.2: destinations step toward the new target
            // rather than jumping, which is what keeps a slow LFO on a filter
            // cutoff free of zipper noise.
            for (int destination : table.touchedDestinations)
            {
                const auto target = targetOffsets[(size_t) destination].load (std::memory_order_relaxed);
                const auto current = currentOffsets[(size_t) destination].load (std::memory_order_relaxed);

                currentOffsets[(size_t) destination].store (current + (target - current) * 0.5f,
                                                            std::memory_order_relaxed);
            }

            samplesUntilTick = controlRateSamples;
        }

        const int step = juce::jmin (remaining, samplesUntilTick);
        samplesUntilTick -= step;
        remaining -= step;
    }
}

//==============================================================================
bool ModMatrix::isDestinationModulated (int parameterIndex) const noexcept
{
    return juce::isPositiveAndBelow (parameterIndex, (int) destinationModulated.size())
             && destinationModulated[(size_t) parameterIndex].load (std::memory_order_relaxed);
}

float ModMatrix::getOffsetFor (int parameterIndex) const noexcept
{
    return juce::isPositiveAndBelow (parameterIndex, (int) currentOffsets.size())
             ? currentOffsets[(size_t) parameterIndex].load (std::memory_order_relaxed)
             : 0.0f;
}

float ModMatrix::apply (int parameterIndex, float baseValue) const noexcept
{
    if (! juce::isPositiveAndBelow (parameterIndex, (int) destinations.size()))
        return baseValue;

    if (! destinationModulated[(size_t) parameterIndex].load (std::memory_order_relaxed))
        return baseValue;

    const auto& info = destinations[(size_t) parameterIndex];
    const float offset = currentOffsets[(size_t) parameterIndex].load (std::memory_order_relaxed);

    float value = baseValue + offset;

    if (info.discrete && info.numSteps > 1)
    {
        // modulation-matrix 4: a discrete destination only changes when the
        // modulated position crosses an integer boundary.
        const float normalised = (value - info.minimum) / info.range;
        // Rounded to the nearest of the numSteps positions 0 .. numSteps - 1.
        const int index = juce::jlimit (0, info.numSteps - 1,
                                        juce::roundToInt (normalised * (float) (info.numSteps - 1)));

        value = info.minimum + (float) index * (info.range / (float) juce::jmax (1, info.numSteps - 1));
    }

    // Not sanitise(): its +-4 is an audio-sample guard, and this is a value in
    // the parameter's own units (Hz, ms, a choice index).
    if (! std::isfinite (value))
        value = baseValue;

    return juce::jlimit (info.minimum, info.maximum, value);
}

//==============================================================================
juce::var ModMatrix::toVar() const
{
    auto* root = new juce::DynamicObject();

    // --- routes ------------------------------------------------------------------
    // modulation-matrix 0.6: a compact array of route records, not an N x M grid.
    juce::Array<juce::var> routeArray;

    {
        const juce::ScopedLock sl (routeLock);

        for (const auto& r : routes)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("src", r.sourceId);
            o->setProperty ("ch", r.sourceChannel);
            o->setProperty ("dst", r.destinationId);
            o->setProperty ("depth", (double) r.depth);
            o->setProperty ("offset", (double) r.offset);
            o->setProperty ("curve", (int) r.curve);
            o->setProperty ("on", r.enabled);
            routeArray.add (juce::var (o));
        }
    }

    root->setProperty ("routes", routeArray);

    // --- LFOs ---------------------------------------------------------------------
    juce::Array<juce::var> lfoArray;

    for (const auto& l : lfos)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("shape", (int) l.getShape());
        o->setProperty ("rate", l.getRateHz());
        o->setProperty ("sync", l.isSynced());
        o->setProperty ("division", (int) l.getSyncDivision());
        o->setProperty ("phase", l.getPhaseOffsetDegrees());
        o->setProperty ("depth", l.getDepth());
        o->setProperty ("symmetry", l.getSymmetry());
        o->setProperty ("retrigger", (int) l.getRetrigger());
        o->setProperty ("smoothing", l.getSmoothingMs());
        o->setProperty ("bipolar", l.isBipolar());

        juce::Array<juce::var> points;

        for (int i = 0; i < ModLfo::kNumBreakpoints; ++i)
            points.add (l.getBreakpoint (i));

        o->setProperty ("points", points);
        lfoArray.add (juce::var (o));
    }

    root->setProperty ("lfos", lfoArray);

    // --- envelopes --------------------------------------------------------------
    juce::Array<juce::var> envArray;

    for (const auto& e : envelopes)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("delay", e.getDelaySeconds());
        o->setProperty ("attack", e.getAttackSeconds());
        o->setProperty ("hold", e.getHoldSeconds());
        o->setProperty ("decay", e.getDecaySeconds());
        o->setProperty ("sustain", e.getSustainLevel());
        o->setProperty ("release", e.getReleaseSeconds());
        o->setProperty ("retrigger", (int) e.getRetrigger());
        o->setProperty ("loop", (int) e.getLoopMode());

        juce::Array<juce::var> stageCurves;

        for (int s = 0; s < (int) ModEnvelope::Stage::numStages; ++s)
            stageCurves.add ((int) e.getStageCurve ((ModEnvelope::Stage) s));

        o->setProperty ("curves", stageCurves);
        envArray.add (juce::var (o));
    }

    root->setProperty ("envelopes", envArray);

    // --- sequencers ---------------------------------------------------------------
    juce::Array<juce::var> seqArray;

    for (const auto& s : sequencers)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("length", s.getLength());
        o->setProperty ("division", (int) s.getDivision());
        o->setProperty ("direction", (int) s.getDirection());
        o->setProperty ("swing", s.getSwing());
        o->setProperty ("sync", s.isSynced());

        juce::Array<juce::var> stepArray;

        for (int i = 0; i < s.getLength(); ++i)
        {
            const auto step = s.getStep (i);
            auto* so = new juce::DynamicObject();
            so->setProperty ("v", step.value);
            so->setProperty ("g", step.gate);
            so->setProperty ("s", step.slide);
            so->setProperty ("p", step.probability);
            stepArray.add (juce::var (so));
        }

        o->setProperty ("steps", stepArray);
        seqArray.add (juce::var (o));
    }

    root->setProperty ("sequencers", seqArray);

    // --- followers -----------------------------------------------------------------
    juce::Array<juce::var> followerArray;

    for (const auto& f : followers)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("source", (int) f.getSource());
        o->setProperty ("string", f.getStringIndex());
        o->setProperty ("attack", f.getAttackMs());
        o->setProperty ("release", f.getReleaseMs());
        o->setProperty ("detection", (int) f.getDetection());
        o->setProperty ("threshold", f.getThreshold());
        o->setProperty ("log", f.isLogarithmic());
        followerArray.add (juce::var (o));
    }

    root->setProperty ("followers", followerArray);

    return juce::var (root);
}

void ModMatrix::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    // --- LFOs ----------------------------------------------------------------------
    if (auto* lfoArray = root->getProperty ("lfos").getArray())
    {
        for (int i = 0; i < juce::jmin ((int) lfos.size(), lfoArray->size()); ++i)
        {
            if (auto* o = lfoArray->getReference (i).getDynamicObject())
            {
                auto& l = lfos[(size_t) i];
                l.setShape ((ModLfo::Shape) juce::jlimit (0, (int) ModLfo::Shape::numShapes - 1,
                                                          (int) o->getProperty ("shape")));
                l.setRateHz ((double) o->getProperty ("rate"));
                l.setSynced ((bool) o->getProperty ("sync"));
                l.setSyncDivision ((ModSyncDivision) juce::jlimit (
                    0, (int) ModSyncDivision::numDivisions - 1, (int) o->getProperty ("division")));
                l.setPhaseOffsetDegrees ((double) o->getProperty ("phase"));
                l.setDepth ((double) o->getProperty ("depth"));
                l.setSymmetry ((double) o->getProperty ("symmetry"));
                l.setRetrigger ((ModLfo::Retrigger) juce::jlimit (0, 3, (int) o->getProperty ("retrigger")));
                l.setSmoothingMs ((double) o->getProperty ("smoothing"));
                l.setBipolar ((bool) o->getProperty ("bipolar"));

                if (auto* points = o->getProperty ("points").getArray())
                    for (int p = 0; p < juce::jmin (ModLfo::kNumBreakpoints, points->size()); ++p)
                        l.setBreakpoint (p, (double) points->getReference (p));
            }
        }
    }

    // --- envelopes -------------------------------------------------------------------
    if (auto* envArray = root->getProperty ("envelopes").getArray())
    {
        for (int i = 0; i < juce::jmin ((int) envelopes.size(), envArray->size()); ++i)
        {
            if (auto* o = envArray->getReference (i).getDynamicObject())
            {
                auto& e = envelopes[(size_t) i];
                e.setDelaySeconds ((double) o->getProperty ("delay"));
                e.setAttackSeconds ((double) o->getProperty ("attack"));
                e.setHoldSeconds ((double) o->getProperty ("hold"));
                e.setDecaySeconds ((double) o->getProperty ("decay"));
                e.setSustainLevel ((double) o->getProperty ("sustain"));
                e.setReleaseSeconds ((double) o->getProperty ("release"));
                e.setRetrigger ((ModEnvelope::Retrigger) juce::jlimit (0, 2, (int) o->getProperty ("retrigger")));
                e.setLoopMode ((ModEnvelope::LoopMode) juce::jlimit (0, 2, (int) o->getProperty ("loop")));

                if (auto* stageCurves = o->getProperty ("curves").getArray())
                    for (int s = 0; s < juce::jmin ((int) ModEnvelope::Stage::numStages,
                                                    stageCurves->size()); ++s)
                        e.setStageCurve ((ModEnvelope::Stage) s,
                                         (ModCurve) juce::jlimit (0, (int) ModCurve::numCurves - 1,
                                                                  (int) stageCurves->getReference (s)));
            }
        }
    }

    // --- sequencers --------------------------------------------------------------------
    if (auto* seqArray = root->getProperty ("sequencers").getArray())
    {
        for (int i = 0; i < juce::jmin ((int) sequencers.size(), seqArray->size()); ++i)
        {
            if (auto* o = seqArray->getReference (i).getDynamicObject())
            {
                auto& s = sequencers[(size_t) i];
                s.setLength ((int) o->getProperty ("length"));
                s.setDivision ((ModSyncDivision) juce::jlimit (
                    0, (int) ModSyncDivision::numDivisions - 1, (int) o->getProperty ("division")));
                s.setDirection ((ModStepSequencer::Direction) juce::jlimit (0, 4, (int) o->getProperty ("direction")));
                s.setSwing ((double) o->getProperty ("swing"));
                s.setSynced ((bool) o->getProperty ("sync"));

                if (auto* stepArray = o->getProperty ("steps").getArray())
                {
                    for (int st = 0; st < juce::jmin (ModStepSequencer::kMaxSteps, stepArray->size()); ++st)
                    {
                        if (auto* so = stepArray->getReference (st).getDynamicObject())
                        {
                            ModStepSequencer::Step step;
                            step.value = (double) so->getProperty ("v");
                            step.gate = (bool) so->getProperty ("g");
                            step.slide = (bool) so->getProperty ("s");
                            step.probability = (double) so->getProperty ("p");
                            s.setStep (st, step);
                        }
                    }
                }
            }
        }
    }

    // --- followers ----------------------------------------------------------------------
    if (auto* followerArray = root->getProperty ("followers").getArray())
    {
        for (int i = 0; i < juce::jmin ((int) followers.size(), followerArray->size()); ++i)
        {
            if (auto* o = followerArray->getReference (i).getDynamicObject())
            {
                auto& f = followers[(size_t) i];
                f.setSource ((ModEnvelopeFollower::Source) juce::jlimit (0, 3, (int) o->getProperty ("source")));
                f.setStringIndex ((int) o->getProperty ("string"));
                f.setAttackMs ((double) o->getProperty ("attack"));
                f.setReleaseMs ((double) o->getProperty ("release"));
                f.setDetection ((ModEnvelopeFollower::Detection) juce::jlimit (0, 2, (int) o->getProperty ("detection")));
                f.setThreshold ((double) o->getProperty ("threshold"));
                f.setLogarithmic ((bool) o->getProperty ("log"));
            }
        }
    }

    // --- routes, last, so they compile against sources that are already set up ----------
    std::vector<ModRoute> loaded;

    if (auto* routeArray = root->getProperty ("routes").getArray())
    {
        for (const auto& entry : *routeArray)
        {
            if (auto* o = entry.getDynamicObject())
            {
                ModRoute r;
                r.sourceId = o->getProperty ("src").toString();
                r.sourceChannel = (int) o->getProperty ("ch");
                r.destinationId = o->getProperty ("dst").toString();
                r.depth = (float) (double) o->getProperty ("depth");
                r.offset = (float) (double) o->getProperty ("offset");
                r.curve = (ModCurve) juce::jlimit (0, (int) ModCurve::numCurves - 1,
                                                   (int) o->getProperty ("curve"));
                r.enabled = o->hasProperty ("on") ? (bool) o->getProperty ("on") : true;
                loaded.push_back (r);
            }
        }
    }

    setRoutes (loaded);

    // modulation-matrix 0.5: sources are stateful and reset on preset load.
    reset();
}

} // namespace luthier
