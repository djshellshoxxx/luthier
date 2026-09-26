#include "JamStyle.h"
#include "../Support/IrLibrary.h"

namespace luthier
{

namespace jam
{
    const char* getLaneName (int lane) noexcept
    {
        static const char* const names[] = { "kick", "snare", "hat", "ride", "crash", "tom", "perc" };
        return juce::isPositiveAndBelow (lane, (int) numDrumLanes) ? names[lane] : "";
    }
}

namespace
{
    const char* const kKitNames[]    = { "Studio", "Vintage", "Arena", "Jazz", "Machine" };
    const char* const kVoiceNames[]  = { "Finger", "Pick", "Muted Pick", "Upright" };
    const char* const kFollowNames[] = { "Tight", "Natural", "Relaxed", "Bar" };

    int indexOf (const char* const* names, int count, const juce::String& name, int fallback)
    {
        for (int i = 0; i < count; ++i)
            if (name.equalsIgnoreCase (names[i]))
                return i;

        return fallback;
    }

    bool isDrumToken (char c) noexcept
    {
        return juce::String (".gxX?oOpbr123HMFsS").containsChar (c);
    }

    bool isBassToken (char c) noexcept
    {
        return juce::String ("R3578AWm-.").containsChar (c);
    }
}

//==============================================================================
void JamPattern::clear (int numSteps) noexcept
{
    steps = juce::jlimit (0, jam::kMaxSteps, numSteps);

    for (auto& lane : drums)
        lane.fill ('.');

    bass.fill ('-');
    hasBass = false;
}

bool JamPattern::setLane (int lane, const juce::String& text)
{
    if (! juce::isPositiveAndBelow (lane, (int) jam::numDrumLanes) || text.length() != steps)
        return false;

    for (int i = 0; i < steps; ++i)
    {
        const auto c = (char) text[i];

        if (! isDrumToken (c))
            return false;

        drums[(size_t) lane][(size_t) i] = c;
    }

    return true;
}

bool JamPattern::setBass (const juce::String& text)
{
    if (text.length() != steps)
        return false;

    for (int i = 0; i < steps; ++i)
    {
        const auto c = (char) text[i];

        if (! isBassToken (c))
            return false;

        bass[(size_t) i] = c;
    }

    hasBass = true;
    return true;
}

juce::String JamPattern::laneString (int lane) const
{
    juce::String s;

    for (int i = 0; i < steps; ++i)
        s += juce::String::charToString ((juce::juce_wchar) drums[(size_t) lane][(size_t) i]);

    return s;
}

juce::String JamPattern::bassString() const
{
    juce::String s;

    for (int i = 0; i < steps; ++i)
        s += juce::String::charToString ((juce::juce_wchar) bass[(size_t) i]);

    return s;
}

//==============================================================================
const JamMeterSet* JamStyle::findMeter (int numerator, int denominator) const noexcept
{
    for (const auto& m : meters)
        if (m.present && m.numerator == numerator && m.denominator == denominator)
            return &m;

    return nullptr;
}

bool JamStyle::isComplete (juce::String* whyNot) const
{
    auto fail = [whyNot] (const juce::String& reason)
    {
        if (whyNot != nullptr)
            *whyNot = reason;

        return false;
    };

    if (grid != 16 && grid != 12)
        return fail ("grid must be 16 or 12");

    if (findMeter (4, 4) == nullptr)
        return fail ("no 4/4");

    for (const auto& m : meters)
    {
        if (! m.present)
            continue;

        const int bar = m.barSteps (stepsPerQuarter());
        const auto meterName = juce::String (m.numerator) + "/" + juce::String (m.denominator);

        for (int v = 0; v < 2; ++v)
            for (int i = 0; i < jam::kNumIntensities; ++i)
                if (m.grooves[(size_t) v][(size_t) i].steps != bar || ! m.grooves[(size_t) v][(size_t) i].hasBass)
                    return fail (meterName + " groove " + juce::String (v == 0 ? "A" : "B") + juce::String (i + 1) + " is not a whole bar");

        if (m.numFills < 4)
            return fail (meterName + " has fewer than 4 fills");

        bool oneBeat = false, twoBeats = false, bar1 = false, barBig = false;
        const int beatsInBar = bar / stepsPerQuarter();

        for (int f = 0; f < m.numFills; ++f)
        {
            const auto& fill = m.fills[(size_t) f];

            if (fill.pattern.steps != fill.beats * stepsPerQuarter())
                return fail (meterName + " fill " + juce::String (f) + " length does not match its beats");

            oneBeat  |= fill.beats == 1;
            twoBeats |= fill.beats == 2;
            bar1     |= fill.beats == beatsInBar && ! fill.big;
            barBig   |= fill.beats == beatsInBar && fill.big;
        }

        if (! (oneBeat && twoBeats && bar1 && barBig))
            return fail (meterName + " lacks a fill size (1 beat, 2 beats, 1 bar, 1 bar big)");

        for (const auto* p : { &m.ending, &m.doubleTime, &m.halfTime })
            if (p->steps != bar)
                return fail (meterName + " ending / double-time / half-time is not a whole bar");
    }

    return true;
}

//==============================================================================
namespace
{
    juce::var patternToVar (const JamPattern& p)
    {
        auto* o = new juce::DynamicObject();

        for (int lane = 0; lane < jam::numDrumLanes; ++lane)
            o->setProperty (jam::getLaneName (lane), p.laneString (lane));

        if (p.hasBass)
            o->setProperty ("bass", p.bassString());

        return juce::var (o);
    }

    bool patternFromVar (const juce::var& v, int steps, JamPattern& p, juce::String& error, const juce::String& where)
    {
        p.clear (steps);

        if (! v.isObject())
        {
            error = where + " is missing";
            return false;
        }

        for (int lane = 0; lane < jam::numDrumLanes; ++lane)
        {
            const auto text = v.getProperty (jam::getLaneName (lane), {});

            if (text.isVoid())
                continue;

            if (! p.setLane (lane, text.toString()))
            {
                error = where + "." + jam::getLaneName (lane) + " is not " + juce::String (steps) + " valid steps";
                return false;
            }
        }

        const auto bass = v.getProperty ("bass", {});

        if (! bass.isVoid() && ! p.setBass (bass.toString()))
        {
            error = where + ".bass is not " + juce::String (steps) + " valid steps";
            return false;
        }

        return true;
    }

    juce::var meterBodyToVar (const JamMeterSet& m)
    {
        auto* body = new juce::DynamicObject();
        auto* grooves = new juce::DynamicObject();

        for (int v = 0; v < 2; ++v)
        {
            auto* byIntensity = new juce::DynamicObject();

            for (int i = 0; i < jam::kNumIntensities; ++i)
                byIntensity->setProperty (juce::String (i + 1), patternToVar (m.grooves[(size_t) v][(size_t) i]));

            grooves->setProperty (v == 0 ? "A" : "B", juce::var (byIntensity));
        }

        body->setProperty ("grooves", juce::var (grooves));

        juce::Array<juce::var> fills;

        for (int f = 0; f < m.numFills; ++f)
        {
            auto fv = patternToVar (m.fills[(size_t) f].pattern);
            fv.getDynamicObject()->setProperty ("beats", m.fills[(size_t) f].beats);
            fv.getDynamicObject()->setProperty ("big", m.fills[(size_t) f].big);
            fills.add (fv);
        }

        body->setProperty ("fills", fills);
        body->setProperty ("ending", patternToVar (m.ending));
        body->setProperty ("double_time", patternToVar (m.doubleTime));
        body->setProperty ("half_time", patternToVar (m.halfTime));
        return juce::var (body);
    }

    bool meterBodyFromVar (const juce::var& body, int spq, JamMeterSet& m, juce::String& error)
    {
        const int bar = m.barSteps (spq);
        const auto meterName = juce::String (m.numerator) + "/" + juce::String (m.denominator);
        const auto grooves = body.getProperty ("grooves", {});

        for (int v = 0; v < 2; ++v)
        {
            const auto byIntensity = grooves.getProperty (v == 0 ? "A" : "B", {});

            for (int i = 0; i < jam::kNumIntensities; ++i)
                if (! patternFromVar (byIntensity.getProperty (juce::String (i + 1), {}), bar,
                                      m.grooves[(size_t) v][(size_t) i], error,
                                      meterName + " grooves." + (v == 0 ? "A." : "B.") + juce::String (i + 1)))
                    return false;
        }

        m.numFills = 0;

        if (const auto* fills = body.getProperty ("fills", {}).getArray())
        {
            for (const auto& fv : *fills)
            {
                if (m.numFills >= jam::kMaxFills)
                    break;

                auto& fill = m.fills[(size_t) m.numFills];
                fill.beats = juce::jlimit (1, 8, (int) fv.getProperty ("beats", 1));
                fill.big = (bool) fv.getProperty ("big", false);

                if (! patternFromVar (fv, fill.beats * spq, fill.pattern, error, meterName + " fills[" + juce::String (m.numFills) + "]"))
                    return false;

                ++m.numFills;
            }
        }

        return patternFromVar (body.getProperty ("ending", {}), bar, m.ending, error, meterName + " ending")
            && patternFromVar (body.getProperty ("double_time", {}), bar, m.doubleTime, error, meterName + " double_time")
            && patternFromVar (body.getProperty ("half_time", {}), bar, m.halfTime, error, meterName + " half_time");
    }
}

juce::var JamStyle::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("magic", kMagic);
    root->setProperty ("schema", kSchema);

    auto* meta = new juce::DynamicObject();
    meta->setProperty ("name", name);
    meta->setProperty ("style", fallbackStyle.isNotEmpty() ? fallbackStyle : name);
    root->setProperty ("meta", juce::var (meta));

    juce::Array<juce::var> meterNames;

    for (const auto& m : meters)
        if (m.present)
            meterNames.add (juce::String (m.numerator) + "/" + juce::String (m.denominator));

    root->setProperty ("meters", meterNames);
    root->setProperty ("grid", grid);
    root->setProperty ("swing", juce::roundToInt (swing * 100.0));
    root->setProperty ("kit", kKitNames[juce::jlimit (0, 4, kit)]);
    root->setProperty ("bass_voice", kVoiceNames[juce::jlimit (0, 3, bassVoice)]);
    root->setProperty ("rhythm_kit", rhythmKit);
    root->setProperty ("follow", kFollowNames[juce::jlimit (0, 3, follow)]);

    // The first meter's patterns sit at the top level (12's field list); any
    // further meter's under by_meter.
    bool first = true;
    auto* byMeter = new juce::DynamicObject();

    for (const auto& m : meters)
    {
        if (! m.present)
            continue;

        const auto body = meterBodyToVar (m);

        if (first)
        {
            for (auto& prop : body.getDynamicObject()->getProperties())
                root->setProperty (prop.name, prop.value);

            first = false;
        }
        else
        {
            byMeter->setProperty (juce::String (m.numerator) + "/" + juce::String (m.denominator), body);
        }
    }

    root->setProperty ("by_meter", juce::var (byMeter));
    return juce::var (root);
}

bool JamStyle::fromVar (const juce::var& v, JamStyle& out, juce::String& error)
{
    out = JamStyle();

    if (! v.isObject())
    {
        error = "not a JSON object";
        return false;
    }

    const auto meta = v.getProperty ("meta", {});
    out.name = meta.getProperty ("name", "").toString();
    out.fallbackStyle = meta.getProperty ("style", v.getProperty ("style", "")).toString();

    if (v.getProperty ("magic", "").toString() != kMagic)
    {
        error = "not a .luthierjam file (magic)";
        return false;
    }

    if ((int) v.getProperty ("schema", 0) > kSchema)
    {
        error = "schema " + v.getProperty ("schema", 0).toString() + " is newer than this build reads";
        return false;
    }

    out.grid = (int) v.getProperty ("grid", 16);

    if (out.grid != 16 && out.grid != 12)
    {
        error = "grid must be 16 or 12";
        return false;
    }

    out.swing = juce::jlimit (0.0, 0.5, (double) v.getProperty ("swing", 0) / 100.0);
    out.kit = indexOf (kKitNames, 5, v.getProperty ("kit", "Studio").toString(), 0);
    out.bassVoice = indexOf (kVoiceNames, 4, v.getProperty ("bass_voice", "Finger").toString(), 0);
    out.follow = indexOf (kFollowNames, 4, v.getProperty ("follow", "Natural").toString(), 1);
    out.rhythmKit = v.getProperty ("rhythm_kit", "").toString();

    juce::StringArray meterNames;

    if (const auto* list = v.getProperty ("meters", {}).getArray())
        for (const auto& m : *list)
            meterNames.add (m.toString());

    if (meterNames.isEmpty())
        meterNames.add ("4/4");

    if (meterNames.size() > (int) out.meters.size())
    {
        error = "too many meters";
        return false;
    }

    for (int i = 0; i < meterNames.size(); ++i)
    {
        auto& m = out.meters[(size_t) i];
        m.numerator = meterNames[i].upToFirstOccurrenceOf ("/", false, false).getIntValue();
        m.denominator = meterNames[i].fromFirstOccurrenceOf ("/", false, false).getIntValue();

        if (m.numerator <= 0 || m.denominator <= 0 || m.barSteps (out.stepsPerQuarter()) > jam::kMaxSteps)
        {
            error = "unsupported meter " + meterNames[i];
            return false;
        }

        m.present = true;
        const auto body = (i == 0) ? v : v.getProperty ("by_meter", {}).getProperty (meterNames[i], {});

        if (! meterBodyFromVar (body, out.stepsPerQuarter(), m, error))
            return false;
    }

    return true;
}

//==============================================================================
// The factory styles (jam-mode 4.1), built in code.
namespace
{
    struct Groove
    {
        const char* kick = nullptr;
        const char* snare = nullptr;
        const char* hat = nullptr;
        const char* ride = nullptr;
        const char* crash = nullptr;
        const char* tom = nullptr;
        const char* perc = nullptr;
        const char* bass = nullptr;
    };

    struct Traits
    {
        bool walking = false;      ///< Jazz: W lines
        bool brushesLow = false;   ///< Jazz, Ballad: brushes at intensity <= 2
        bool rideLed = false;      ///< Jazz: the ride keeps time
        bool doubleKick = false;   ///< Metal
    };

    JamPattern make (int steps, const Groove& g)
    {
        JamPattern p;
        p.clear (steps);
        const char* lanes[] = { g.kick, g.snare, g.hat, g.ride, g.crash, g.tom, g.perc };

        for (int lane = 0; lane < jam::numDrumLanes; ++lane)
            if (lanes[lane] != nullptr)
            {
                if (! p.setLane (lane, lanes[lane]))
                    p.steps = 0;   // a typo in a factory string fails JM-01
            }

        if (g.bass != nullptr)
        {
            if (! p.setBass (g.bass))
                p.steps = 0;
        }
        else
        {
            juce::String ties ("R");
            ties += juce::String::repeatedString ("-", steps - 1);
            p.setBass (ties);
        }

        return p;
    }

    /** Positions of the 8ths in a beat: 0 and half a beat, or the triplet's
        first and third on a 12 grid (the swung 8th). */
    int offbeat (int spq) noexcept { return spq == 3 ? 2 : spq / 2; }

    void setStep (JamPattern& p, int lane, int step, char c)
    {
        if (step >= 0 && step < p.steps)
            p.drums[(size_t) lane][(size_t) step] = c;
    }

    void setBassStep (JamPattern& p, int step, char c)
    {
        if (step >= 0 && step < p.steps)
            p.bass[(size_t) step] = c;
    }

    int backbeat (int beats, int beat) noexcept
    {
        // 2 and 4 in 4/4, the last beat otherwise; every other beat in longer bars.
        if (beats == 4) return (beat == 1 || beat == 3) ? 1 : 0;
        if (beats == 3) return beat == 2 ? 1 : 0;
        return (beat % 2 == 1) ? 1 : 0;
    }

    /** jam-mode 4.2's five intensities from the style's own groove (3). */
    JamPattern deriveIntensity (const JamPattern& g3, int intensity, int spq, const Traits& t)
    {
        if (intensity == 3)
            return g3;

        const int steps = g3.steps;
        const int beats = steps / spq;
        JamPattern p;
        p.clear (steps);
        p.hasBass = true;

        if (intensity == 1)
        {
            // Rim or brushes with hat or ride; bass in whole or half notes.
            for (int b = 0; b < beats; ++b)
            {
                const int s = b * spq;

                if (t.rideLed)
                {
                    setStep (p, jam::ride, s, 'x');
                    if (backbeat (beats, b)) setStep (p, jam::hat, s, 'p');
                }
                else
                {
                    setStep (p, jam::hat, s, 'x');
                }

                if (t.brushesLow)
                    setStep (p, jam::snare, s, 'b');
                else if (backbeat (beats, b))
                    setStep (p, jam::perc, s, 'r');
            }

            setStep (p, jam::kick, 0, 'g');

            for (int i = 0; i < steps; ++i)
                p.bass[(size_t) i] = '-';

            p.bass[0] = 'R';

            if (t.walking && beats >= 4)
                p.bass[(size_t) (2 * spq)] = 'W';
            else if (beats >= 4)
                p.bass[(size_t) (2 * spq)] = '5';

            return p;
        }

        if (intensity == 2)
        {
            // Kick on 1 and 3, backbeat snare; bass in quarters.
            for (int b = 0; b < beats; ++b)
            {
                const int s = b * spq;

                if (t.rideLed)
                {
                    setStep (p, jam::ride, s, 'x');
                    if (backbeat (beats, b)) setStep (p, jam::hat, s, 'p');
                }
                else
                {
                    setStep (p, jam::hat, s, 'x');
                    setStep (p, jam::hat, s + offbeat (spq), 'x');
                }

                if (backbeat (beats, b))
                    setStep (p, jam::snare, s, t.brushesLow ? 'b' : 'x');

                p.bass[(size_t) s] = t.walking ? (b == 0 ? 'R' : 'W') : (b == 2 ? '5' : 'R');

                for (int k = 1; k < spq; ++k)
                    p.bass[(size_t) (s + k)] = '-';
            }

            setStep (p, jam::kick, 0, 'x');

            if (beats >= 4)
                setStep (p, jam::kick, 2 * spq, 'x');

            return p;
        }

        // 4 and 5 build on the style's own groove.
        p = g3;

        if (intensity >= 4)
        {
            // Open hats and a busier kick.
            for (int b = 0; b < beats; ++b)
                if (backbeat (beats, b) && ! t.rideLed)
                    setStep (p, jam::hat, b * spq + offbeat (spq), 'o');

            if (beats >= 3)
                setStep (p, jam::kick, 2 * spq - (spq == 3 ? 1 : spq / 2), 'x');

            if (t.doubleKick)
                for (int i = 0; i < steps; ++i)
                    setStep (p, jam::kick, i, 'x');

            if (! t.walking)
            {
                // Bass in 8ths using 5 and 8.
                const char cycle[] = { 'R', '8', 'R', '5' };
                int e = 0;

                for (int b = 0; b < beats; ++b)
                    for (int half = 0; half < 2; ++half)
                    {
                        const int s = b * spq + (half == 0 ? 0 : offbeat (spq));
                        const bool last = (b == beats - 1 && half == 1);
                        setBassStep (p, s, last ? 'A' : cycle[e++ % 4]);

                        const int end = half == 0 ? b * spq + offbeat (spq) : (b + 1) * spq;
                        for (int k = s + 1; k < end; ++k)
                            setBassStep (p, k, '-');
                    }
            }
        }

        if (intensity == 5)
        {
            // Ride (or crash) quarters; the crash every 4 bars is the
            // conductor's; the busiest bass.
            for (int i = 0; i < steps; ++i)
                setStep (p, jam::hat, i, '.');

            for (int b = 0; b < beats; ++b)
            {
                setStep (p, jam::ride, b * spq, 'X');
                setStep (p, jam::ride, b * spq + offbeat (spq), 'x');

                if (backbeat (beats, b))
                    setStep (p, jam::hat, b * spq, 'p');
            }

            if (! t.walking)
            {
                for (int b = 0; b < beats; ++b)
                {
                    const bool last = b == beats - 1;
                    const char* cell = spq == 3 ? (last ? "5.A" : (b % 2 == 0 ? "R.R" : "5.8"))
                                                : (last ? "5.8A" : (b % 2 == 0 ? "R.R8" : "5.R7"));

                    for (int k = 0; k < spq; ++k)
                        setBassStep (p, b * spq + k, cell[k]);
                }
            }
            else
            {
                // A walking line with a skip note before each beat.
                for (int b = 1; b < beats; ++b)
                    setBassStep (p, b * spq - 1, 'm');
            }
        }

        return p;
    }

    JamFill makeFill (int beats, bool big, int spq, int variant, int barBeats)
    {
        JamFill f;
        f.beats = beats;
        f.big = big;
        auto& p = f.pattern;
        p.clear (beats * spq);
        p.hasBass = false;
        const int n = p.steps;

        if (beats == 1)
        {
            for (int i = 0; i < n; ++i)
                setStep (p, variant == 0 ? jam::snare : jam::tom, i, variant == 0 ? (i == n - 1 ? 'X' : 'x') : (i < n / 2 ? '1' : '2'));
        }
        else if (beats == 2 && barBeats != 2)
        {
            for (int i = 0; i < n; ++i)
            {
                if (variant == 0)
                    setStep (p, i < spq ? jam::snare : jam::tom, i, i < spq ? 'x' : (i < spq + spq / 2 ? '1' : '2'));
                else
                    setStep (p, jam::tom, i, i < spq ? '1' : (i < n - 1 ? '2' : '3'));
            }

            setStep (p, jam::kick, 0, 'x');
        }
        else
        {
            // A bar: 8ths on the snare, then down the toms; big is all 16ths
            // with accents and the kick on every beat.
            for (int i = 0; i < n; ++i)
            {
                const bool firstHalf = i < n / 2;
                const bool onEighth = (i % spq == 0) || (i % spq == offbeat (spq));

                if (! big && ! onEighth)
                    continue;

                if (firstHalf)
                    setStep (p, jam::snare, i, big && (i % spq == 0) ? 'X' : 'x');
                else
                {
                    const int third = (i - n / 2) * 3 / juce::jmax (1, n - n / 2);
                    const char low[] = { '1', '2', '3' }, high[] = { 'H', 'M', 'F' };
                    setStep (p, jam::tom, i, big && (i % spq == 0) ? high[juce::jlimit (0, 2, third)] : low[juce::jlimit (0, 2, third)]);
                }
            }

            for (int b = 0; b < beats; ++b)
                if (big || b == 0)
                    setStep (p, jam::kick, b * spq, 'x');
        }

        return f;
    }

    void buildMeter (JamMeterSet& m, int num, int den, int spq, const Groove& a3, const Groove& b3, const Traits& t)
    {
        m.numerator = num;
        m.denominator = den;
        m.present = true;

        const int bar = m.barSteps (spq);
        const int beats = bar / spq;
        const JamPattern base[2] = { make (bar, a3), make (bar, b3) };

        for (int v = 0; v < 2; ++v)
            for (int i = 0; i < jam::kNumIntensities; ++i)
                m.grooves[(size_t) v][(size_t) i] = deriveIntensity (base[v], i + 1, spq, t);

        m.numFills = 0;
        m.fills[(size_t) m.numFills++] = makeFill (1, false, spq, 0, beats);
        m.fills[(size_t) m.numFills++] = makeFill (1, false, spq, 1, beats);
        m.fills[(size_t) m.numFills++] = makeFill (2, false, spq, 0, beats);
        m.fills[(size_t) m.numFills++] = makeFill (2, false, spq, 1, beats);
        m.fills[(size_t) m.numFills++] = makeFill (beats, false, spq, 0, beats);
        m.fills[(size_t) m.numFills++] = makeFill (beats, true, spq, 0, beats);

        // The ending: crash and kick on the downbeat; the bass holds the root
        // for a beat and then damps (2.2).
        m.ending.clear (bar);
        m.ending.hasBass = true;
        m.ending.drums[jam::kick][0] = 'X';
        m.ending.drums[jam::crash][0] = 'X';
        m.ending.bass[0] = 'R';

        for (int i = 1; i < bar; ++i)
            m.ending.bass[(size_t) i] = i < spq ? '-' : (i == spq ? '.' : '-');

        // Double time: the groove's 8ths twice as fast; half time: half as fast.
        const auto& src = base[0];
        m.doubleTime.clear (bar);
        m.halfTime.clear (bar);
        m.doubleTime.hasBass = m.halfTime.hasBass = true;

        for (int i = 0; i < bar; ++i)
        {
            for (int lane = 0; lane < jam::numDrumLanes; ++lane)
            {
                m.doubleTime.drums[(size_t) lane][(size_t) i] = src.drum (lane, (2 * i) % bar);
                m.halfTime.drums[(size_t) lane][(size_t) i] = (i % 2 == 0) ? src.drum (lane, i / 2) : '.';
            }

            m.doubleTime.bass[(size_t) i] = src.bassAt ((2 * i) % bar);
            m.halfTime.bass[(size_t) i] = (i % 2 == 0) ? src.bassAt (i / 2) : '-';
        }

        m.doubleTime.bass[0] = m.doubleTime.bass[0] == '-' ? 'R' : m.doubleTime.bass[0];
        m.halfTime.bass[0] = m.halfTime.bass[0] == '-' ? 'R' : m.halfTime.bass[0];
    }

    std::unique_ptr<JamStyle> style (const char* name, int grid, double swingPercent, int kit, int voice,
                                     jam::Follow follow, const char* rhythmKit)
    {
        auto s = std::make_unique<JamStyle>();
        s->name = name;
        s->fallbackStyle = name;
        s->grid = grid;
        s->swing = swingPercent / 100.0;
        s->kit = kit;
        s->bassVoice = voice;
        s->follow = (int) follow;
        s->rhythmKit = rhythmKit;
        return s;
    }
}

const char* JamStyleLibrary::getFactoryStyleName (int index) noexcept
{
    static const char* const names[] = { "Rock", "Pop", "Funk", "Blues Shuffle", "Country",
                                         "Metal", "Reggae", "Jazz Swing", "Ballad", "EDM" };
    return names[juce::jlimit (0, jam::kNumFactoryStyles - 1, index)];
}

juce::StringArray JamStyleLibrary::getStyleChoiceNames()
{
    juce::StringArray names;

    for (int i = 0; i < jam::kNumFactoryStyles; ++i)
        names.add (getFactoryStyleName (i));

    names.add ("User");
    return names;
}

std::vector<std::unique_ptr<JamStyle>> JamStyleLibrary::buildFactoryStyles()
{
    using F = jam::Follow;
    enum { studio = 0, vintage, arena, jazzKit, machine };
    enum { finger = 0, pick, mutedPick, upright };

    std::vector<std::unique_ptr<JamStyle>> out;
    Traits plain;

    {   // 0 Rock: 16, Studio, Pick, Natural
        auto s = style ("Rock", 16, 0, studio, pick, F::natural, "Rock");
        Groove a { "x.....x.x.......", "....X.......X...", "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, "R-R-R-R-R-R-R-A-" };
        Groove b { "x.....x...x.x...", "....X.......X..g", "x.x.x.x.x.x.x.xo", nullptr, nullptr, nullptr, nullptr, "R-R-R-5-R-R-8-A-" };
        buildMeter (s->meters[0], 4, 4, 4, a, b, plain);
        out.push_back (std::move (s));
    }

    {   // 1 Pop: 16, Studio, Finger, Natural
        auto s = style ("Pop", 16, 0, studio, finger, F::natural, "Pop");
        Groove a { "x.......x.x.....", "....X.......X...", "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, "..s...s...s...s.", "R---R-R-5---R-A-" };
        Groove b { "x...x...x...x...", "....X.......X...", "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, "s.s.s.s.s.s.s.s.", "R-R-R-R-5-5-R-A-" };
        buildMeter (s->meters[0], 4, 4, 4, a, b, plain);
        out.push_back (std::move (s));
    }

    {   // 2 Funk: 16, swing 8 %, Studio, Finger + ghosts, Tight
        auto s = style ("Funk", 16, 8, studio, finger, F::tight, "Funk 16th");
        Groove a { "x..x..x...x..x..", "....X..g.g..X..g", "xxxxxxxxxxxxxxxx", nullptr, nullptr, nullptr, nullptr, "R..8.Rm.5..R8.A." };
        Groove b { "x.x...x..x.x....", "....X..g.g.gX.g.", "x.xxx.xxx.xxx.xo", nullptr, nullptr, nullptr, nullptr, "R.mR..8.5.m7..A." };
        buildMeter (s->meters[0], 4, 4, 4, a, b, plain);
        out.push_back (std::move (s));
    }

    {   // 3 Blues Shuffle: 12 (triplet), Vintage, Finger, Natural
        auto s = style ("Blues Shuffle", 12, 0, vintage, finger, F::natural, "Blues Shuffle");
        Groove a { "x.....x.....", "...X.....X..", "x.xx.xx.xx.x", nullptr, nullptr, nullptr, nullptr, "R.R3.35.57.5" };
        Groove b { "x..x..x..x..", "...X..g..X..", "...p.....p..", "x.xx.xx.xx.x", nullptr, nullptr, nullptr, "R.R3.35.58.7" };
        buildMeter (s->meters[0], 4, 4, 3, a, b, plain);
        out.push_back (std::move (s));
    }

    {   // 4 Country: 16, + 3/4, Vintage, Pick, Natural
        auto s = style ("Country", 16, 0, vintage, pick, F::natural, "Country");
        Groove a { "x.......x.......", "....x.g.....x.g.", "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, "R-------5-------" };
        Groove b { "x.....x.x.......", "..g.x.g...g.x.g.", "x.xxx.xxx.xxx.xx", nullptr, nullptr, nullptr, nullptr, "R---3---5---3-A-" };
        buildMeter (s->meters[0], 4, 4, 4, a, b, plain);

        Groove a3 { "x...........", "....x...x...", "x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, "R-------5---" };
        Groove b3 { "x.......x...", "....x...x..g", "x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, "R---5---8---" };
        buildMeter (s->meters[1], 3, 4, 4, a3, b3, plain);
        out.push_back (std::move (s));
    }

    {   // 5 Metal: 16, Arena, Muted Pick, Tight
        auto s = style ("Metal", 16, 0, arena, mutedPick, F::tight, "Metal");
        Traits metal;
        metal.doubleKick = true;
        Groove a { "x.xxx.xxx.xxx.xx", "....X.......X...", "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, "R.RRR.RRR.RRR.RR" };
        Groove b { "xxxxxxxxxxxxxxxx", "....X.......X...", nullptr, "x...x...x...x...", nullptr, nullptr, nullptr, "RRRRRRRRRRRRRRRR" };
        buildMeter (s->meters[0], 4, 4, 4, a, b, metal);
        out.push_back (std::move (s));
    }

    {   // 6 Reggae: 16, swing 12 %, Vintage, Finger, Relaxed
        auto s = style ("Reggae", 16, 12, vintage, finger, F::relaxed, "Reggae");
        Groove a { "........X.......", nullptr, "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, "........r.......", "R--R..5-8--.5-A." };
        Groove b { "x...x...x...x...", "....g.......g...", "..x...x...x...x.", nullptr, nullptr, nullptr, "........r.......", "R---R.5.R---8.A." };
        buildMeter (s->meters[0], 4, 4, 4, a, b, plain);
        out.push_back (std::move (s));
    }

    {   // 7 Jazz Swing: 12 (triplet), Jazz, Upright walking, Bar
        auto s = style ("Jazz Swing", 12, 0, jazzKit, upright, F::bar, "Jazz Comp");
        Traits jazz;
        jazz.walking = jazz.brushesLow = jazz.rideLed = true;
        Groove a { "g..g..g..g..", "........g...", "...p.....p..", "x..x.xx..x.x", nullptr, nullptr, nullptr, "R--W--W--W--" };
        Groove b { "g.....g.....", ".....g.....g", "...p.....p..", "x..x.xx..x.x", nullptr, nullptr, nullptr, "R--W--W--W--" };
        buildMeter (s->meters[0], 4, 4, 3, a, b, jazz);
        out.push_back (std::move (s));
    }

    {   // 8 Ballad: 16, + 3/4, 6/8, Vintage, Finger, Relaxed
        auto s = style ("Ballad", 16, 0, vintage, finger, F::relaxed, "Folk Fingerstyle");
        Traits ballad;
        ballad.brushesLow = true;
        Groove a { "x.......x.x.....", nullptr, "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, "....r.......r...", "R-------5---R-A-" };
        Groove b { "x.....x.x.......", "....x.......x...", "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, "R-------R---5-A-" };
        buildMeter (s->meters[0], 4, 4, 4, a, b, ballad);

        Groove a3 { "x...........", nullptr, "x.x.x.x.x.x.", nullptr, nullptr, nullptr, "....r...r...", "R-------5---" };
        Groove b3 { "x.......x...", "....x...x...", "x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, "R---5---8---" };
        buildMeter (s->meters[1], 3, 4, 4, a3, b3, ballad);

        Groove a68 { "x...........", nullptr, "x.x.x.x.x.x.", nullptr, nullptr, nullptr, "......r.....", "R-----5-----" };
        Groove b68 { "x.....x.....", "......x.....", "x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, "R---8-5---A-" };
        buildMeter (s->meters[2], 6, 8, 4, a68, b68, ballad);
        out.push_back (std::move (s));
    }

    {   // 9 EDM: 16, Machine, Muted Pick, Tight
        auto s = style ("EDM", 16, 0, machine, mutedPick, F::tight, "Pop");
        Groove a { "x...x...x...x...", "....X.......X...", "..o...o...o...o.", nullptr, nullptr, nullptr, "s.s.s.s.s.s.s.s.", "..R...R...R...R." };
        Groove b { "x...x...x...x...", "....X.......X...", "xxoxxxoxxxoxxxox", nullptr, nullptr, nullptr, nullptr, "R.R.8.R.R.R.8.5." };
        buildMeter (s->meters[0], 4, 4, 4, a, b, plain);
        out.push_back (std::move (s));
    }

    return out;
}

//==============================================================================
JamStyleLibrary::JamStyleLibrary()
    : factory (buildFactoryStyles())
{
}

const JamStyle* JamStyleLibrary::getFactoryStyle (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, (int) factory.size()) ? factory[(size_t) index].get() : nullptr;
}

int JamStyleLibrary::findFactoryStyle (const juce::String& name) const noexcept
{
    for (int i = 0; i < (int) factory.size(); ++i)
        if (factory[(size_t) i]->name.equalsIgnoreCase (name.trim()))
            return i;

    return -1;
}

int JamStyleLibrary::loadOverrides (const juce::File& folder)
{
    int count = 0;

    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + JamStyle::kFileExtension))
    {
        JamStyle parsed;
        juce::String error;

        if (! JamStyle::fromVar (juce::JSON::parse (f), parsed, error) || ! parsed.isComplete())
            continue;

        const int index = findFactoryStyle (parsed.name);

        if (index < 0)
            continue;

        // Kept alive, like a user style: the old factory style may be playing.
        userStyles.push_back (std::move (factory[(size_t) index]));
        factory[(size_t) index] = std::make_unique<JamStyle> (parsed);
        ++count;
    }

    return count;
}

const JamStyle* JamStyleLibrary::loadUserStyle (const juce::File& file, juce::String& warning)
{
    JamStyle parsed;
    juce::String error;
    bool ok = file.existsAsFile();

    if (ok)
        ok = JamStyle::fromVar (juce::JSON::parse (file), parsed, error) && parsed.isComplete (&error);
    else
        error = "file not found";

    if (ok)
    {
        if (parsed.name.isEmpty())
            parsed.name = file.getFileNameWithoutExtension();

        userStyles.push_back (std::make_unique<JamStyle> (parsed));
        return userStyles.back().get();
    }

    // 13: the factory style its `style` field names, else Rock.
    int fallback = findFactoryStyle (parsed.fallbackStyle);

    if (fallback < 0)
        fallback = 0;

    warning = "The jam style \"" + file.getFileName() + "\" could not be loaded (" + error + "). Playing "
              + getFactoryStyleName (fallback) + " instead.";
    return getFactoryStyle (fallback);
}

bool JamStyleLibrary::saveStyle (const JamStyle& s, const juce::File& file, juce::String& error)
{
    file.getParentDirectory().createDirectory();
    juce::TemporaryFile temp (file);

    if (! temp.getFile().replaceWithText (juce::JSON::toString (s.toVar(), false)))
    {
        error = "could not write " + temp.getFile().getFullPathName();
        return false;
    }

    if (! temp.overwriteTargetFileWithTemporary())
    {
        error = "could not replace " + file.getFullPathName();
        return false;
    }

    return true;
}

juce::File JamStyleLibrary::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier").getChildFile ("Jam");
}

juce::File JamStyleLibrary::getFactoryOverrideFolder()
{
    return IrLibrary::getResourcesFolder().getChildFile ("Jam");
}

} // namespace luthier
