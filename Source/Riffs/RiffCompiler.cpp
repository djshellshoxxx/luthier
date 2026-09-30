#include "RiffCompiler.h"

#include "../Model/Playing/TechniqueEngine.h"
#include "../Rhythm/BassStepGrid.h"
#include "../DSP/String/Harmonics.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

//==============================================================================
bool RiffEvent::operator== (const RiffEvent& o) const noexcept
{
    return juce::exactlyEqual (beat, o.beat) && juce::exactlyEqual (delaySeconds, o.delaySeconds)
        && kind == o.kind && stringIndex == o.stringIndex && juce::exactlyEqual (fret, o.fret)
        && midiNote == o.midiNote && juce::exactlyEqual (velocity, o.velocity) && technique == o.technique
        && harmonicPartial == o.harmonicPartial && juce::exactlyEqual (touchFret, o.touchFret)
        && juce::exactlyEqual (slideFromFret, o.slideFromFret)
        && juce::exactlyEqual (slideBeats, o.slideBeats) && juce::exactlyEqual (palmMuteDepth, o.palmMuteDepth)
        && bassTechnique == o.bassTechnique && letRing == o.letRing && bendSegment == o.bendSegment
        && noteIndex == o.noteIndex;
}

bool RiffBendSegment::operator== (const RiffBendSegment& o) const noexcept
{
    return stringIndex == o.stringIndex && juce::exactlyEqual (startBeat, o.startBeat)
        && juce::exactlyEqual (endBeat, o.endBeat) && firstPoint == o.firstPoint && numPoints == o.numPoints
        && juce::exactlyEqual (vibratoRateHz, o.vibratoRateHz)
        && juce::exactlyEqual (vibratoDepthCents, o.vibratoDepthCents)
        && juce::exactlyEqual (vibratoDelayBeats, o.vibratoDelayBeats);
}

namespace
{
    bool sameNotes (const std::vector<ScoreNote>& a, const std::vector<ScoreNote>& b)
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
        {
            const auto& x = a[i];
            const auto& y = b[i];

            if (! juce::exactlyEqual (x.startBeat, y.startBeat) || ! juce::exactlyEqual (x.durationBeats, y.durationBeats)
                  || x.stringIndex != y.stringIndex || x.fret != y.fret || x.midiNote != y.midiNote
                  || ! juce::exactlyEqual (x.velocity, y.velocity) || x.techniques.size() != y.techniques.size())
                return false;

            for (size_t k = 0; k < x.techniques.size(); ++k)
            {
                const auto& s = x.techniques[k];
                const auto& t = y.techniques[k];

                if (s.type != t.type || ! juce::exactlyEqual (s.value, t.value)
                      || ! juce::exactlyEqual (s.secondValue, t.secondValue) || s.curve != t.curve)
                    return false;
            }
        }

        return true;
    }
}

bool CompiledRiff::operator== (const CompiledRiff& o) const noexcept
{
    return id == o.id && name == o.name && events == o.events && bends == o.bends && bendPoints == o.bendPoints
        && juce::exactlyEqual (lengthBeats, o.lengthBeats) && juce::exactlyEqual (loopBeats, o.loopBeats)
        && juce::exactlyEqual (beatsPerBar, o.beatsPerBar) && juce::exactlyEqual (tempoBpm, o.tempoBpm)
        && numStrings == o.numStrings && sameNotes (placement.notes, o.placement.notes)
        && placement.sourceIndex == o.placement.sourceIndex && placement.shift == o.placement.shift
        && placement.dropped == o.placement.dropped && guitar == o.guitar && settings == o.settings
        && notices == o.notices;
}

Riff CompiledRiff::toPlacedRiff (const Riff& source) const
{
    Riff r = source;
    r.tuning.clear();

    for (int s = 0; s < guitar.numStrings; ++s)
        r.tuning.push_back (guitar.tuning[(size_t) s]);

    r.capo = guitar.capo;
    r.keyRoot = RiffVocabulary::rootName (placement.targetRoot);
    r.scale = placement.targetScale;
    r.notes = placement.notes;

    // Strum masks and bass techniques follow their notes to the strings they
    // were placed on.
    r.strums.clear();

    for (auto strum : source.strums)
    {
        int mask = 0;

        for (size_t i = 0; i < placement.notes.size(); ++i)
        {
            const auto& from = source.notes[(size_t) placement.sourceIndex[i]];

            if (std::abs (from.startBeat - strum.beat) < 1.0e-6 && (strum.mask & (1 << from.stringIndex)) != 0)
                mask |= 1 << placement.notes[i].stringIndex;
        }

        if (mask != 0)
        {
            strum.mask = mask;
            r.strums.push_back (strum);
        }
    }

    r.bassTech.clear();

    for (auto b : source.bassTech)
    {
        for (size_t i = 0; i < placement.notes.size(); ++i)
        {
            const auto& from = source.notes[(size_t) placement.sourceIndex[i]];

            if (std::abs (from.startBeat - b.beat) < 1.0e-6 && from.stringIndex == b.str)
            {
                b.str = placement.notes[i].stringIndex;
                r.bassTech.push_back (b);
                break;
            }
        }
    }

    r.techniques = r.computeTechniques();
    return r;
}

PerformanceScore CompiledRiff::toScore (const Riff& source) const
{
    return toPlacedRiff (source).toScore();
}

//==============================================================================
namespace RiffCompiler
{
    int bassTechniqueIndex (const juce::String& token)
    {
        if (token == "slap" || token == "thump") return (int) BassStepType::thumb;
        if (token == "pop")                      return (int) BassStepType::pop;
        if (token == "lhslap")                   return (int) BassStepType::dead;
        return -1;
    }

    int harmonicPartialFor (double fretOrOffset)
    {
        return TechniqueEngine::harmonicPartialForFret (fretOrOffset);
    }

    namespace
    {
        using T = ScoreTechnique::Type;

        double curveAt (const std::vector<std::pair<double, double>>& curve, double position)
        {
            if (curve.empty())
                return 0.0;

            if (position <= curve.front().first)
                return curve.front().second;

            for (size_t k = 1; k < curve.size(); ++k)
            {
                const auto& a = curve[k - 1];
                const auto& b = curve[k];

                if (position <= b.first)
                {
                    const double span = b.first - a.first;
                    return span > 0.0 ? a.second + (b.second - a.second) * (position - a.first) / span : b.second;
                }
            }

            return curve.back().second;
        }

        /** The semitone curve of a pitch technique; the value alone when no curve was written. */
        std::vector<std::pair<double, double>> curveOf (const ScoreTechnique& t)
        {
            if (! t.curve.empty())
                return t.curve;

            switch (t.type)
            {
                case T::bend:        return { { 0.0, 0.0 }, { 0.25, t.value }, { 1.0, t.value } };
                case T::bendRelease: return { { 0.0, 0.0 }, { 0.25, t.value }, { 0.6, t.value }, { 1.0, t.secondValue } };
                case T::preBend:     return { { 0.0, t.value }, { 1.0, t.secondValue } };
                case T::whammy:      return { { 0.0, 0.0 }, { 0.3, t.value }, { 0.6, t.value }, { 1.0, 0.0 } };
                default:             return {};
            }
        }

        bool isPitchCurve (T type)
        {
            return type == T::bend || type == T::bendRelease || type == T::preBend || type == T::whammy;
        }
    }

    std::shared_ptr<const CompiledRiff> compile (const Riff& riff, const RiffPlaySettings& settings,
                                                 const GuitarSpecSummary& guitar)
    {
        auto out = std::make_shared<CompiledRiff>();
        auto& c = *out;

        c.id = riff.meta.id;
        c.name = riff.meta.name;
        c.guitar = guitar;
        c.settings = settings;
        c.tempoBpm = riff.tempoBpm;
        c.beatsPerBar = riff.getBeatsPerBar();
        c.lengthBeats = riff.lengthBeats;
        c.loopBeats = riff.getNumBars() * c.beatsPerBar;
        c.numStrings = guitar.numStrings;
        c.placement = RiffTransposer::place (riff, settings, guitar);
        c.notices = c.placement.notices;

        const double bpm = settings.nominalBpm > 0.0 ? settings.nominalBpm : riff.tempoBpm;
        const double beatsPerSecond = bpm / 60.0;
        const double level = juce::Decibels::decibelsToGain (juce::jlimit (-24.0, 0.0, settings.levelDb));

        const auto& notes = c.placement.notes;
        bool whammyAsBend = false;

        // The index of the next note on the same string, for legato arrivals
        // and for leaving out note-offs a following note replaces.
        std::vector<int> nextOnString (notes.size(), -1);

        for (size_t i = 0; i < notes.size(); ++i)
            for (size_t j = i + 1; j < notes.size(); ++j)
                if (notes[j].stringIndex == notes[i].stringIndex)
                {
                    nextOnString[i] = (int) j;
                    break;
                }

        // A legato slide's arrival: the next note on the string, starting as
        // this one ends, at the slide's fret.
        std::vector<int> arrivalFrom (notes.size(), -1);
        std::vector<bool> arrivalPicked (notes.size(), false);

        for (size_t i = 0; i < notes.size(); ++i)
        {
            const auto* slide = notes[i].findTechnique (T::slideLegato);
            const bool shift = slide == nullptr && notes[i].findTechnique (T::slideShift) != nullptr;

            if (slide == nullptr)
                slide = notes[i].findTechnique (T::slideShift);

            if (slide == nullptr)
                continue;

            const int j = nextOnString[i];

            if (j >= 0 && std::abs (notes[(size_t) j].startBeat - (notes[i].startBeat + notes[i].durationBeats)) < 1.0e-6
                  && notes[(size_t) j].fret == juce::roundToInt (slide->value))
            {
                arrivalFrom[(size_t) j] = (int) i;
                arrivalPicked[(size_t) j] = shift;
            }
        }

        auto push = [&c] (const RiffEvent& e) { c.events.push_back (e); };

        for (size_t i = 0; i < notes.size(); ++i)
        {
            const auto& n = notes[i];
            const auto& source = riff.notes[(size_t) c.placement.sourceIndex[i]];

            double duration = n.durationBeats * (n.hasTechnique (T::staccato) ? 0.5 : 1.0);
            double velocity = n.velocity * level;

            if (n.hasTechnique (T::ghostNote)) velocity *= 0.45;
            if (n.hasTechnique (T::accent))    velocity *= 1.2;

            velocity = juce::jlimit (0.01, 1.0, velocity);

            RiffEvent on;
            on.beat = n.startBeat;
            on.kind = RiffEvent::Kind::noteOn;
            on.stringIndex = n.stringIndex;
            on.fret = (double) n.fret;
            on.midiNote = n.midiNote;
            on.velocity = velocity;
            on.noteIndex = (int) i;

            // ---- technique -------------------------------------------------------
            if (const auto* pm = n.findTechnique (T::palmMute))
                on.palmMuteDepth = pm->value > 0.0 ? juce::jlimit (0.0, 1.0, pm->value) : -1.0;

            const auto* natural = n.findTechnique (T::naturalHarmonic);
            const auto* artificial = n.findTechnique (T::artificialHarmonic);
            const auto* tapped = n.findTechnique (T::tapHarmonic);

            if (n.hasTechnique (T::deadNote))
            {
                on.technique = Technique::MutedPick;
            }
            else if (natural != nullptr)
            {
                // harmonic-realism 4.1: the string stops open; the touch at the
                // node picks the partial.
                on.touchFret = harmonics::tabTouchFret ((double) n.fret);
                on.harmonicPartial = harmonics::partialForFret (on.touchFret);
                on.fret = 0.0;
                on.technique = on.harmonicPartial > 0 ? Technique::NaturalHarmonic : Technique::Pluck;

                if (on.harmonicPartial <= 0)
                {
                    on.fret = (double) n.fret;
                    on.touchFret = -1.0;
                }
            }
            else if (artificial != nullptr || tapped != nullptr)
            {
                // Stopped at the fret, touched (or tapped) `value` frets above it.
                const double offset = (artificial != nullptr ? artificial->value : tapped->value) > 0.0
                                        ? (artificial != nullptr ? artificial->value : tapped->value) : 12.0;
                on.touchFret = (double) n.fret + offset;
                on.harmonicPartial = harmonics::findNode (harmonics::touchFractionFromBridge (on.touchFret, (double) n.fret),
                                                          648.0, 2.5).partial;
                on.technique = artificial != nullptr ? Technique::ArtificialHarmonic : Technique::Tap;
            }
            else if (n.hasTechnique (T::tap))
            {
                on.technique = Technique::Tap;
            }
            else if (n.hasTechnique (T::hammerOn))
            {
                on.technique = Technique::HammerOn;
            }
            else if (n.hasTechnique (T::pullOff))
            {
                on.technique = Technique::PullOff;
            }
            else if (n.hasTechnique (T::pinchHarmonic))
            {
                on.technique = Technique::PinchHarmonic;
                on.harmonicPartial = 2 + (int) (velocity * 3.0);
            }
            else if (arrivalFrom[i] >= 0)
            {
                const auto& from = notes[(size_t) arrivalFrom[i]];
                on.technique = arrivalPicked[i] ? Technique::Pluck : Technique::Slide;
                on.slideFromFret = (double) from.fret;
                on.slideBeats = juce::jmin (0.08 * beatsPerSecond, 0.5 * n.durationBeats);
            }
            else if (const auto* in = n.findTechnique (T::slideIn))
            {
                on.technique = Technique::Pluck;   // picked, then the glide
                on.slideFromFret = juce::jmax (0.0, in->value > 0.0 ? in->value : n.fret - 3.0);
                on.slideBeats = juce::jmin (0.06 * beatsPerSecond, 0.5 * n.durationBeats);
            }
            else if (const auto* down = n.findTechnique (T::slideDown))
            {
                on.technique = Technique::Pluck;
                on.slideFromFret = juce::jlimit (0.0, (double) guitar.maxFret, down->value > 0.0 ? down->value : n.fret + 3.0);
                on.slideBeats = juce::jmin (0.06 * beatsPerSecond, 0.5 * n.durationBeats);
            }
            else if (n.hasTechnique (T::palmMute))
            {
                on.technique = Technique::PalmMute;
            }

            // BASS_TECH on this note.
            for (const auto& b : riff.bassTech)
                if (std::abs (b.beat - source.startBeat) < 1.0e-6 && b.str == source.stringIndex)
                    on.bassTechnique = bassTechniqueIndex (b.tech);

            // Strum stagger (strum-dynamics 2): 1/cv seconds a string, in stroke order.
            for (const auto& strum : riff.strums)
            {
                if (std::abs (strum.beat - source.startBeat) > 1.0e-6 || (strum.mask & (1 << source.stringIndex)) == 0)
                    continue;

                int rank = 0;

                for (int s = 0; s < kMaxStrings; ++s)
                    if ((strum.mask & (1 << s)) != 0 && (strum.down ? s > source.stringIndex : s < source.stringIndex))
                        ++rank;

                on.delaySeconds = rank / juce::jmax (1.0, strum.cv);
                break;
            }

            // ---- pitch curve -----------------------------------------------------
            std::vector<const ScoreTechnique*> curves;

            for (const auto& t : n.techniques)
                if (isPitchCurve (t.type))
                    curves.push_back (&t);

            const auto* vibrato = n.findTechnique (T::vibrato);

            if (n.hasTechnique (T::whammy) && ! guitar.hasWhammy)
                whammyAsBend = true;

            if (! curves.empty() || vibrato != nullptr)
            {
                RiffBendSegment seg;
                seg.stringIndex = n.stringIndex;
                seg.startBeat = n.startBeat;
                seg.endBeat = n.startBeat + duration;
                seg.firstPoint = (int) c.bendPoints.size();

                std::vector<double> positions;

                for (const auto* t : curves)
                    for (const auto& p : curveOf (*t))
                        positions.push_back (juce::jlimit (0.0, 1.0, p.first));

                std::sort (positions.begin(), positions.end());
                positions.erase (std::unique (positions.begin(), positions.end()), positions.end());

                for (const auto p : positions)
                {
                    double semitones = 0.0;

                    for (const auto* t : curves)
                        semitones += curveAt (curveOf (*t), p);

                    c.bendPoints.emplace_back (n.startBeat + p * duration, semitones * 100.0);
                }

                seg.numPoints = (int) c.bendPoints.size() - seg.firstPoint;

                if (vibrato != nullptr)
                {
                    seg.vibratoRateHz = vibrato->value > 0.0 ? vibrato->value : 5.5;
                    seg.vibratoDepthCents = vibrato->secondValue > 0.0 ? vibrato->secondValue : 30.0;
                    seg.vibratoDelayBeats = juce::jmin (0.25, 0.25 * duration);
                }

                on.bendSegment = (int) c.bends.size();
                c.bends.push_back (seg);
            }

            push (on);

            // ---- extra note-ons inside the note ----------------------------------
            auto glideOff = [&] (double target, double atFraction)
            {
                RiffEvent g = on;
                g.beat = n.startBeat + atFraction * duration;
                g.technique = Technique::Slide;
                g.slideFromFret = (double) n.fret;
                g.fret = juce::jlimit (0.0, (double) guitar.maxFret, target);
                g.midiNote = juce::jlimit (0, 127, n.midiNote + juce::roundToInt (g.fret) - n.fret);
                g.slideBeats = (1.0 - atFraction) * duration;
                g.bendSegment = -1;
                g.bassTechnique = -1;
                g.harmonicPartial = 0;
                push (g);
            };

            const bool hasArrival = nextOnString[i] >= 0 && arrivalFrom[(size_t) nextOnString[i]] == (int) i;

            if (! hasArrival)
            {
                if (const auto* s = n.findTechnique (T::slideLegato))
                    glideOff (s->value, 0.5);
                else if (const auto* sh = n.findTechnique (T::slideShift))
                    glideOff (sh->value, 0.5);
            }

            if (const auto* o = n.findTechnique (T::slideOut))
                glideOff (o->value > 0.0 ? o->value : juce::jmax (1.0, n.fret - 5.0), 0.75);
            else if (const auto* u = n.findTechnique (T::slideUp))
                glideOff (u->value > 0.0 ? u->value : n.fret + 5.0, 0.75);

            if (const auto* trill = n.findTechnique (T::trill))
            {
                const double step = beatsPerSecond / 12.0;   // 12 Hz
                const int other = juce::roundToInt (trill->value);
                bool up = other > n.fret;
                int count = 0;

                for (double b = n.startBeat + step; b < n.startBeat + duration - 0.5 * step && count < 256; b += step, ++count)
                {
                    const bool toOther = (count % 2) == 0;
                    const int fret = toOther ? other : n.fret;

                    RiffEvent t = on;
                    t.beat = b;
                    t.fret = (double) fret;
                    t.midiNote = juce::jlimit (0, 127, n.midiNote + fret - n.fret);
                    t.technique = (toOther == up) ? Technique::HammerOn : Technique::PullOff;
                    t.bendSegment = -1;
                    t.bassTechnique = -1;
                    t.slideFromFret = -1.0;
                    t.slideBeats = 0.0;
                    push (t);
                }

                juce::ignoreUnused (up);
            }

            // ---- the note-off ----------------------------------------------------
            // Left out when the next note on the string starts by then: that
            // note takes the string over, and an off landing after it (a strum's
            // stagger) would silence it.
            const double endBeat = n.startBeat + duration;
            const int next = nextOnString[i];
            const bool replaced = next >= 0 && notes[(size_t) next].startBeat <= endBeat + 1.0e-6;

            if (! replaced)
            {
                RiffEvent off;
                off.beat = endBeat;
                off.delaySeconds = on.delaySeconds;
                off.kind = RiffEvent::Kind::noteOff;
                off.stringIndex = n.stringIndex;
                off.fret = (double) n.fret;
                off.midiNote = n.midiNote;
                off.letRing = n.hasTechnique (T::letRing);
                off.noteIndex = (int) i;
                push (off);
            }
        }

        std::stable_sort (c.events.begin(), c.events.end(), [] (const RiffEvent& a, const RiffEvent& b)
        {
            if (! juce::exactlyEqual (a.beat, b.beat))            return a.beat < b.beat;
            if (a.kind != b.kind)                                  return a.kind == RiffEvent::Kind::noteOff;
            if (! juce::exactlyEqual (a.delaySeconds, b.delaySeconds)) return a.delaySeconds < b.delaySeconds;
            return a.stringIndex < b.stringIndex;
        });

        if (whammyAsBend)
            c.notices.add ("played as bends: no whammy fitted");

        return out;
    }
}

} // namespace luthier
