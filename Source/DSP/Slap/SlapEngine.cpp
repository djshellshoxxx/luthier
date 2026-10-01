#include "SlapEngine.h"
#include "../Noise/PlayingNoise.h"

namespace luthier
{

//==============================================================================
namespace
{
    int lowStrings (int count, int numStrings) noexcept
    {
        // String 0 is the high E, so the low strings are the last ones.
        int mask = 0;

        for (int s = juce::jmax (0, numStrings - count); s < numStrings; ++s)
            mask |= 1 << s;

        return mask;
    }

    int allStrings (int numStrings) noexcept
    {
        return (1 << juce::jlimit (1, kMaxStrings, numStrings)) - 1;
    }

    /*  How the note's own dynamics carry into a strike made with a set force:
        the force is the hand, the velocity how hard this one was meant. */
    double dynamicsScale (double velocity) noexcept
    {
        return 0.7 + 0.3 * juce::jlimit (0.0, 1.0, velocity);
    }

    /** The body tap's knock: three resonances per part and their weights. */
    struct BodyKnock
    {
        double hz[3];
        double q[3];
        double weight[3];
    };

    const BodyKnock& knockFor (BodyPart part) noexcept
    {
        // The top is the soundboard: its main air-coupled mode and a bright
        // knuckle click. The sides are stiff and mid-heavy; the back is the
        // dull, low one.
        static const BodyKnock knocks[] =
        {
            { { 190.0, 420.0, 2200.0 }, { 8.0, 6.0, 1.5 }, { 1.00, 0.60, 0.35 } },   // top
            { { 320.0, 650.0, 1500.0 }, { 7.0, 5.0, 1.5 }, { 0.50, 1.00, 0.30 } },   // side
            { { 110.0, 240.0,  900.0 }, { 8.0, 6.0, 2.0 }, { 1.00, 0.50, 0.15 } }    // back
        };

        return knocks[(size_t) juce::jlimit (0, 2, (int) part)];
    }
}

//==============================================================================
SlapSettings SlapSettings::fromPreset (SlapPreset preset, int numStrings, bool bassFamily) noexcept
{
    juce::ignoreUnused (bassFamily);

    SlapSettings s;
    s.armed = true;
    numStrings = juce::jlimit (1, kMaxStrings, numStrings);

    switch (preset)
    {
        case SlapPreset::bassStandard:
            // Thumb on the low E and A, pops on the rest; hard notes slap.
            s.type = SlapType::thumb;
            s.trigger = TriggerSource::velocityZone;
            s.velocityZone = 90;
            s.stringMask = lowStrings (2, numStrings);
            break;

        case SlapPreset::bassAggressive:
            s.type = SlapType::thumb;
            s.trigger = TriggerSource::velocityZone;
            s.velocityZone = 90;
            s.stringMask = lowStrings (2, numStrings);
            s.slapStrength = 0.95;
            s.popStrength = 0.95;
            s.thumbHardness = 0.8;
            s.fretContact = 1.0;
            s.snapBack = 0.9;
            break;

        case SlapPreset::funkGuitarPalm:
            // The chuka rhythm itself is muting-rhythm.md's grid; this is the hand.
            s.type = SlapType::palm;
            s.trigger = TriggerSource::keyswitch;
            s.stringMask = 0;   // every string
            break;

        case SlapPreset::acousticBodyTap:
            s.type = SlapType::bodyTap;
            s.trigger = TriggerSource::keyswitch;
            s.bodyPart = BodyPart::top;
            break;

        case SlapPreset::percussiveFingerstyle:
            // Thumb slaps on the bass strings from hard notes; the body taps
            // between them are keyswitch 17, which listens whatever the type.
            s.type = SlapType::thumb;
            s.trigger = TriggerSource::velocityZone;
            s.velocityZone = 100;
            s.stringMask = lowStrings (3, numStrings);
            s.bodyPart = BodyPart::top;
            break;

        case SlapPreset::numPresets:
        default:
            break;
    }

    return s;
}

const char* SlapSettings::getPresetName (SlapPreset preset) noexcept
{
    switch (preset)
    {
        case SlapPreset::bassStandard:          return "Bass Slap Standard";
        case SlapPreset::bassAggressive:        return "Bass Slap Aggressive";
        case SlapPreset::funkGuitarPalm:        return "Funk Guitar Palm Slap";
        case SlapPreset::acousticBodyTap:       return "Acoustic Body Tap";
        case SlapPreset::percussiveFingerstyle: return "Percussive Fingerstyle";
        case SlapPreset::numPresets:
        default:                                return "";
    }
}

TechniqueTriggerConfig SlapSettings::triggerConfig() const noexcept
{
    TechniqueTriggerConfig c;
    c.armed = armed;
    c.source = trigger;
    c.keyswitches = { TechniqueKeyswitch::slap, TechniqueKeyswitch::ghost,
                      TechniqueKeyswitch::bodyTap, TechniqueKeyswitch::palmSlap };
    c.triggerCc = triggerCc;
    c.auxCc = ghostCc;
    c.zoneChannel = zoneChannel;

    // A slapped note on the zone still sounds; a palm slap or a body tap has no
    // pitch, so its zone notes are the hand's and go no further.
    c.consumeZoneNotes = type == SlapType::palm || type == SlapType::bodyTap;
    return c;
}

//==============================================================================
void SlapEngine::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (8000.0, sampleRate);
    reset();
}

void SlapEngine::reset() noexcept
{
    numActions = 0;
    earliestDue = std::numeric_limits<juce::int64>::max();
    modifierHeld = ghostHeld = false;

    for (auto& m : bodyModes)
        m.reset();

    bodyGains.fill (0.0);
    bodyKick = 0.0;
    bodySamplesLeft = 0;
}

void SlapEngine::setSettings (const SlapSettings& s) noexcept
{
    settings = s;
    settings.velocityZone = juce::jlimit (1, 127, s.velocityZone);
    settings.triggerCc = juce::jlimit (0, 127, s.triggerCc);
    settings.ghostCc = juce::jlimit (0, 127, s.ghostCc);
    settings.zoneChannel = juce::jlimit (1, 16, s.zoneChannel);
    settings.slapStrength = juce::jlimit (0.0, 1.0, s.slapStrength);
    settings.slapPositionMm = juce::jlimit (5.0, 400.0, s.slapPositionMm);
    settings.thumbHardness = juce::jlimit (0.0, 1.0, s.thumbHardness);
    settings.fretContact = juce::jlimit (0.0, 1.0, s.fretContact);
    settings.popStrength = juce::jlimit (0.0, 1.0, s.popStrength);
    settings.popPositionMm = juce::jlimit (5.0, 400.0, s.popPositionMm);
    settings.upRatio = juce::jlimit (0.0, 1.0, s.upRatio);
    settings.ghostLevel = juce::jlimit (0.0, 1.0, s.ghostLevel);
    settings.ghostDamping = juce::jlimit (0.0, 1.0, s.ghostDamping);
    settings.ghostVelocityThreshold = juce::jlimit (1, 127, s.ghostVelocityThreshold);
    settings.force = juce::jlimit (0.0, 1.0, s.force);
    settings.palmPositionMm = juce::jlimit (5.0, 400.0, s.palmPositionMm);
    settings.stringMask = s.stringMask & ((1 << kMaxStrings) - 1);
    settings.reboundGapMs = juce::jlimit (1.0, 500.0, s.reboundGapMs);
    settings.snapBack = juce::jlimit (0.0, 1.0, s.snapBack);

    // Disarmed, nothing is held: a modifier let go behind a switch that is off
    // must not come back held when it is switched on.
    if (! settings.armed)
        modifierHeld = ghostHeld = false;
}

void SlapEngine::setInstrument (int n, double scaleMm, int frets, bool bassFamily) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
    scaleLengthMm = juce::jmax (100.0, scaleMm);
    numFrets = juce::jlimit (1, 36, frets);
    bass = bassFamily;
}

int SlapEngine::palmMask() const noexcept
{
    return allStrings (numStrings);
}

int SlapEngine::effectiveMask() const noexcept
{
    if (settings.stringMask != 0)
        return settings.stringMask & allStrings (numStrings);

    // 1: "Bass thumb defaults to low E/A. Palm slap defaults to all strings."
    // On a guitar the thumb takes the wound strings of a standard set.
    switch (settings.type)
    {
        case SlapType::thumb:   return lowStrings (bass ? 2 : 3, numStrings);
        case SlapType::pop:
        case SlapType::palm:    return allStrings (numStrings);
        case SlapType::bodyTap:
        case SlapType::numTypes:
        default:                return 0;
    }
}

double SlapEngine::positionFraction (double mmFromLastFret, double fret) const noexcept
{
    // bass-techniques 2.2 measures the contact from the last fret, which is
    // where a slap happens: over the end of the fretboard.
    const double lastFretFromBridge = scaleLengthMm * std::pow (2.0, -numFrets / 12.0);
    const double fromBridge = juce::jlimit (5.0, lastFretFromBridge, lastFretFromBridge - mmFromLastFret);
    const double vibrating = scaleLengthMm * std::pow (2.0, -juce::jlimit (0.0, 36.0, fret) / 12.0);

    return juce::jlimit (0.02, 0.5, fromBridge / juce::jmax (1.0, vibrating));
}

double SlapEngine::velocityForLevelRatio (double velocity, double ratio) noexcept
{
    // Excitation 8: a strike's peak is kindGain x (0.10 + 0.90 v^1.45).
    const double level = 0.10 + 0.90 * std::pow (juce::jlimit (0.0, 1.0, velocity), 1.45);
    const double wanted = juce::jlimit (0.0, 1.0, ratio) * level;
    const double inner = juce::jmax (0.0, (wanted - 0.10) / 0.90);

    return juce::jlimit (0.0, 1.0, std::pow (inner, 1.0 / 1.45));
}

//==============================================================================
SlapStrike SlapEngine::classify (const NoteOnEvent& e, bool underBar) const noexcept
{
    SlapStrike st;
    st.stringIndex = juce::jlimit (0, numStrings - 1, e.stringIndex);
    st.velocity = e.velocity;

    // technique-cascade.md 3.4: the bar holds the string and the thumb cannot get at it.
    if (underBar)
        return st;

    /*  bass-techniques 9 (MODEL-GAPS): a step of the bass grid names its
        technique outright - thumb, pop or ghost here; fingerstyle and dead
        notes are played as they came. Only on a bass (0.1). Values are
        BassStepType's: 1 thumb, 2 pop, 3 ghost. */
    if (e.bassTechnique >= 0)
    {
        if (! bass)
            return st;

        if (e.bassTechnique == 3)
        {
            st.ghost = true;
            st.velocity = e.velocity * settings.ghostLevel;
            return st;
        }

        if (e.bassTechnique == 1 || e.bassTechnique == 2)
        {
            st.strike = true;
            st.type = e.bassTechnique == 1 ? SlapType::thumb : SlapType::pop;
            st.force = (e.bassTechnique == 1 ? settings.slapStrength : settings.popStrength) * dynamicsScale (e.velocity);
            st.contactMm = e.bassTechnique == 1 ? settings.slapPositionMm : settings.popPositionMm;
            st.velocity = st.force;
            return st;
        }

        return st;
    }

    // Only a fresh attack can be slapped or ghosted: legato moves, harmonics
    // and taps keep their own excitation.
    switch (e.technique)
    {
        case Technique::Pluck:
        case Technique::PalmMute:
        case Technique::MutedPick:
        case Technique::Strum:
            break;

        case Technique::HammerOn:
        case Technique::PullOff:
        case Technique::Slide:
        case Technique::Bend:
        case Technique::Vibrato:
        case Technique::NaturalHarmonic:
        case Technique::PinchHarmonic:
        case Technique::ArtificialHarmonic:
        case Technique::Tap:
        case Technique::SlideGuitar:
        case Technique::NumTechniques:
        default:
            return st;
    }

    const int velocity127 = juce::roundToInt (juce::jlimit (0.0, 1.0, e.velocity) * 127.0);

    // bass-techniques 5: on a bass, a quiet note is a ghost, technique armed or not.
    const bool autoGhost = bass && settings.ghostAuto && velocity127 < settings.ghostVelocityThreshold;
    const bool ghost = autoGhost || (settings.armed && (settings.ghostMode || ghostHeld));

    bool slapped = false;

    if (settings.armed)
    {
        switch (settings.trigger)
        {
            case TriggerSource::velocityZone: slapped = velocity127 >= settings.velocityZone; break;
            case TriggerSource::keyswitch:
            case TriggerSource::controller:   slapped = modifierHeld; break;
            case TriggerSource::mpeZone:      slapped = e.midiChannel == settings.zoneChannel; break;
            case TriggerSource::buttonOnly:
            case TriggerSource::numSources:
            default:                          break;
        }
    }

    const bool inMask = (effectiveMask() & (1 << st.stringIndex)) != 0;

    if (slapped && settings.type == SlapType::pop && ! inMask)
        slapped = false;

    if (! slapped)
    {
        st.ghost = ghost;

        if (ghost)
            st.velocity = e.velocity * settings.ghostLevel;

        return st;
    }

    st.strike = true;
    st.ghost = ghost;

    switch (settings.type)
    {
        case SlapType::thumb:   st.type = inMask ? SlapType::thumb : SlapType::pop; break;
        case SlapType::pop:     st.type = SlapType::pop; break;
        case SlapType::palm:    st.type = SlapType::palm; break;
        case SlapType::bodyTap: st.type = SlapType::bodyTap; break;
        case SlapType::numTypes:
        default:                st.type = SlapType::thumb; break;
    }

    switch (st.type)
    {
        case SlapType::thumb: st.force = settings.slapStrength; st.contactMm = settings.slapPositionMm; break;
        case SlapType::pop:   st.force = settings.popStrength;  st.contactMm = settings.popPositionMm;  break;
        case SlapType::palm:
        case SlapType::bodyTap:
        case SlapType::numTypes:
        default:              st.force = settings.force;        st.contactMm = settings.palmPositionMm; break;
    }

    st.force *= dynamicsScale (e.velocity);
    st.velocity = st.force * (ghost ? settings.ghostLevel : 1.0);
    return st;
}

SlapStrike SlapEngine::strikeFor (int stringIndex, double dynamics) const noexcept
{
    SlapStrike st;
    st.stringIndex = juce::jlimit (0, numStrings - 1, stringIndex);
    st.strike = true;
    st.ghost = settings.ghostMode || ghostHeld;

    const bool inMask = (effectiveMask() & (1 << st.stringIndex)) != 0;
    st.type = (settings.type == SlapType::pop || (settings.type == SlapType::thumb && ! inMask))
                ? SlapType::pop : SlapType::thumb;

    st.force = (st.type == SlapType::thumb ? settings.slapStrength : settings.popStrength) * dynamicsScale (dynamics);
    st.contactMm = st.type == SlapType::thumb ? settings.slapPositionMm : settings.popPositionMm;
    st.velocity = st.force * (st.ghost ? settings.ghostLevel : 1.0);
    return st;
}

//==============================================================================
void SlapEngine::recomputeEarliest() noexcept
{
    earliestDue = std::numeric_limits<juce::int64>::max();

    for (int i = 0; i < numActions; ++i)
        earliestDue = juce::jmin (earliestDue, actions[(size_t) i].due);
}

void SlapEngine::queue (const SlapAction& a) noexcept
{
    if (numActions >= kMaxActions)
        return;   // a hand cannot do more than this at once; the newest is the one lost

    actions[(size_t) numActions++] = a;
    earliestDue = juce::jmin (earliestDue, a.due);
}

bool SlapEngine::popDue (juce::int64 now, SlapAction& out) noexcept
{
    if (earliestDue > now)
        return false;

    int best = -1;

    for (int i = 0; i < numActions; ++i)
        if (actions[(size_t) i].due <= now && (best < 0 || actions[(size_t) i].due < actions[(size_t) best].due))
            best = i;

    if (best < 0)
    {
        recomputeEarliest();
        return false;
    }

    out = actions[(size_t) best];
    actions[(size_t) best] = actions[(size_t) (--numActions)];
    recomputeEarliest();
    return true;
}

void SlapEngine::trigger (const SlapStrike& strike, juce::int64 atSample) noexcept
{
    SlapAction a;
    a.due = atSample;
    a.force = strike.force;

    if (strike.type == SlapType::palm)
    {
        a.kind = SlapAction::Kind::palmSlap;
        a.mask = settings.type == SlapType::palm ? effectiveMask() : palmMask();
    }
    else if (strike.type == SlapType::bodyTap)
    {
        a.kind = SlapAction::Kind::bodyTap;
    }
    else
    {
        a.kind = SlapAction::Kind::strike;
        a.strike = strike;
    }

    queue (a);

    if (a.kind != SlapAction::Kind::strike)
        firedCount.fetch_add (1, std::memory_order_relaxed);
}

void SlapEngine::queuePercussive (SlapType type, double dynamics, juce::int64 at) noexcept
{
    SlapStrike st;
    st.strike = true;
    st.type = type;
    st.force = settings.force * dynamicsScale (dynamics);
    trigger (st, at);
}

void SlapEngine::noteStruck (const SlapStrike& strike, juce::int64 atSample) noexcept
{
    if (! strike.strike || strike.isPercussive())
        return;

    if (strike.rebound)
        return;

    // A new down-stroke replaces the up-stroke the last one on this string still owed.
    preempt (strike.stringIndex);
    firedCount.fetch_add (1, std::memory_order_relaxed);

    // bass-techniques 4 / string-slap 1: the thumb comes back up through the
    // string, quieter by the up-ratio and brighter (the nail side), after the
    // rebound gap.
    if (strike.type == SlapType::thumb && settings.doubleThump)
    {
        SlapAction a;
        a.kind = SlapAction::Kind::strike;
        a.strike = strike;
        a.strike.rebound = true;
        a.strike.velocity = velocityForLevelRatio (strike.velocity, settings.upRatio);
        a.due = atSample + (juce::int64) std::llround (settings.reboundGapMs * 0.001 * sr);
        queue (a);
    }
}

void SlapEngine::preempt (int stringIndex) noexcept
{
    for (int i = 0; i < numActions;)
    {
        const auto& a = actions[(size_t) i];

        if (a.kind == SlapAction::Kind::strike && a.strike.stringIndex == stringIndex)
            actions[(size_t) i] = actions[(size_t) (--numActions)];
        else
            ++i;
    }

    recomputeEarliest();
}

//==============================================================================
void SlapEngine::processBlock (int numSamples, juce::int64 blockStartSample, const TechniqueTriggers& triggers) noexcept
{
    juce::ignoreUnused (numSamples);

    for (int i = 0; i < triggers.getNumEvents(); ++i)
    {
        const auto& e = triggers.getEvent (i);

        if (e.technique != TechniqueId::slap)
            continue;

        const juce::int64 at = blockStartSample + e.offset;
        const bool percussiveType = settings.type == SlapType::palm || settings.type == SlapType::bodyTap;

        switch (e.role)
        {
            case 0:
                if (e.number < 0)
                {
                    // The button: a strike now, on the type's strings.
                    if (! e.on)
                        break;

                    if (percussiveType)
                    {
                        queuePercussive (settings.type, 1.0, at);
                    }
                    else
                    {
                        const int mask = effectiveMask();

                        for (int s = 0; s < numStrings; ++s)
                            if ((mask & (1 << s)) != 0)
                                trigger (strikeFor (s, 1.0), at);
                    }
                }
                else if (settings.trigger == TriggerSource::mpeZone)
                {
                    // A slapped zone note is classified as it plays; a palm slap
                    // or a body tap is the zone note itself.
                    if (percussiveType && e.on)
                        queuePercussive (settings.type, e.value, at);
                }
                else if (percussiveType)
                {
                    if (e.on)
                        queuePercussive (settings.type, e.value, at);
                }
                else
                {
                    // The keyswitch or CC held: the notes under it are slapped.
                    modifierHeld = e.on;
                }
                break;

            case 1: ghostHeld = e.on; break;   // ghost mode's keyswitch or CC

            case 2:
                if (e.on)
                    queuePercussive (SlapType::bodyTap, e.number >= 0 ? e.value : 1.0, at);
                break;

            case 3:
                if (e.on)
                    queuePercussive (SlapType::palm, e.number >= 0 ? e.value : 1.0, at);
                break;

            default:
                break;
        }
    }
}

//==============================================================================
void SlapEngine::shapeExcitation (const SlapStrike& strike, double fret, Excitation::Params& p) const noexcept
{
    p.kind = Excitation::Kind::Slap;
    p.pluckPosition = positionFraction (strike.contactMm, fret);
    p.velocity = juce::jlimit (0.0, 1.0, strike.velocity);
    p.pickAngle = 0.0;

    if (strike.type == SlapType::pop)
    {
        // 3: the finger hooks the string and lets it snap back - a nail's
        // bright, short contact.
        p.material = Excitation::Material::Fingernail;
        p.nailVsFlesh = 0.85;
        p.brightness = 0.95;
        p.pickThickness = 0.0;
        p.noiseAmount = 0.12;
        return;
    }

    // 2.1: the thumb's pad, stiffer with hardness; 4: the up-stroke meets the
    // string with the nail side, so it is brighter.
    const double hardness = settings.thumbHardness;
    p.material = strike.rebound ? Excitation::Material::Fingernail : Excitation::Material::Thumb;
    p.nailVsFlesh = strike.rebound ? 0.7 : 0.2;
    p.brightness = juce::jlimit (0.0, 1.0, 0.3 + 0.6 * hardness + (strike.rebound ? 0.2 : 0.0));
    p.pickThickness = 1.0 - hardness;
    p.noiseAmount = 0.08 + 0.15 * hardness;
}

void SlapEngine::applyGhostDamping (StringEngine& string, double damping) noexcept
{
    // bass-techniques 5: "the same mechanism as strum-dynamics.md 6.1's chuck
    // and shares its code" - the fretting hand across the string, by amount.
    damping = juce::jlimit (0.0, 1.0, damping);

    if (damping > 0.0)
        string.setDamping (StringEngine::Damping::Chuck, damping);
}

NoiseEvent SlapEngine::makeContactBuzz (const SlapStrike& strike, bool wound, double fundamentalHz,
                                        const FretBuzz& buzz) const noexcept
{
    NoiseEvent e;
    e.noiseClass = NoiseClass::fretBuzz;
    e.stringIndex = strike.stringIndex;

    if (! strike.strike || strike.isPercussive())
        return e;

    const bool thumb = strike.type == SlapType::thumb;

    // 2.2 slap_fret_contact: how far the thumb drives the string into the
    // frets. A pop's collision is the snap-back; a bass control (1), fixed on
    // a guitar.
    const double contact = thumb ? settings.fretContact : (bass ? settings.snapBack : kGuitarSnapBack);

    if (contact <= 0.0)
        return e;   // 2.2: "at 0 gives a thumb-thump with no clack"

    // The excess over the fret the contact reaches, in fret-buzz.md's mm: the
    // harder the hit the further in, and a plain string, with no winding to
    // ride over the fret crowns, rattles less (7).
    const double reference = thumb ? 0.70 : 0.75;
    const double excess = 0.6 * contact * juce::jmin (2.0, strike.force / reference)
                          * (wound ? 1.0 : kPlainStringContact);

    e.level = buzz.levelFor (excess) * dbToGain (kClackGainDb)
              * (strike.rebound ? settings.upRatio : 1.0)
              * (strike.ghost ? settings.ghostLevel : 1.0);

    const double hz = (thumb ? 2200.0 + 2600.0 * settings.thumbHardness : 3200.0 + 1200.0 * contact)
                      * (strike.rebound ? 1.15 : 1.0);

    // A collision rings down onto its resonance as the string leaves the frets.
    e.startHz = hz * 1.1;
    e.endHz = hz;
    e.q = 1.8;
    e.brightness = juce::jlimit (0.2, 0.9, 0.55 + 0.3 * (thumb ? settings.thumbHardness : contact));
    e.texture = NoiseTexture::metallic;
    e.attackMs = 0.3;
    e.holdMs = 0.0;
    e.decayMs = thumb ? 24.0 - 12.0 * settings.thumbHardness : 10.0;

    // One contact a cycle, as fret-buzz.md's generator does.
    e.burstHz = juce::jmax (20.0, fundamentalHz);
    return e;
}

void SlapEngine::makePalmEvents (int stringIndex, bool wound, double fundamentalHz, double force,
                                 const FretBuzz& buzz, NoiseEvent& clack, NoiseEvent& thump) const noexcept
{
    juce::ignoreUnused (fundamentalHz);

    const double drive = juce::jmin (2.0, juce::jmax (0.0, force) / 0.6);

    // The strings slapped down onto the frets and held there: one short clack
    // each, not a buzz - the hand stays on the string, so nothing cycles.
    clack = NoiseEvent();
    clack.noiseClass = NoiseClass::fretBuzz;
    clack.stringIndex = stringIndex;
    clack.level = drive > 0.0 ? buzz.levelFor (0.5 * drive * (wound ? 1.0 : kPlainStringContact)) * dbToGain (4.0) : 0.0;
    clack.startHz = 1800.0;
    clack.endHz = 1500.0;
    clack.q = 1.2;
    clack.brightness = 0.4;
    clack.texture = NoiseTexture::metallic;
    clack.attackMs = 0.5;
    clack.decayMs = 14.0;

    // The hand itself: a broad, low thump.
    thump = NoiseEvent();
    thump.noiseClass = NoiseClass::fretBuzz;
    thump.stringIndex = stringIndex;
    thump.level = PlayingNoise::kNoteReference * dbToGain (-14.0) * drive;
    thump.startHz = 180.0;
    thump.endHz = 120.0;
    thump.q = 0.7;
    thump.brightness = 0.05;
    thump.texture = NoiseTexture::smooth;
    thump.attackMs = 0.8;
    thump.decayMs = 30.0;
}

//==============================================================================
void SlapEngine::startBodyTap (double force, BodyPart part) noexcept
{
    const auto& knock = knockFor (part);
    const double sum = knock.weight[0] + knock.weight[1] + knock.weight[2];

    for (size_t m = 0; m < bodyModes.size(); ++m)
    {
        const double hz = juce::jlimit (20.0, sr * 0.45, knock.hz[m]);
        bodyModes[m].setBandpass (sr, hz, knock.q[m]);

        // A constant-peak band-pass rings at about 2 alpha per unit impulse;
        // dividing that out puts each resonance's peak at its weight.
        const double alpha = std::sin (constants::kTwoPi * hz / sr) / (2.0 * knock.q[m]);
        bodyGains[m] = kBodyTapReference * juce::jlimit (0.0, 1.0, force) * knock.weight[m]
                       / (sum * juce::jmax (1.0e-6, 2.0 * alpha));
    }

    bodyKick += 1.0;
    bodySamplesLeft = (int) (kBodyTapSeconds * sr);
}

double SlapEngine::nextBodyDrive() noexcept
{
    if (bodySamplesLeft <= 0)
        return 0.0;

    const double x = bodyKick;
    bodyKick = 0.0;

    double y = 0.0;

    for (size_t m = 0; m < bodyModes.size(); ++m)
        y += bodyGains[m] * bodyModes[m].process (x);

    if (--bodySamplesLeft <= 0)
        for (auto& mode : bodyModes)
            mode.reset();

    return sanitise (y);
}

} // namespace luthier
