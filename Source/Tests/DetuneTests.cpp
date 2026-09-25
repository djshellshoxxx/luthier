/*  String Detune (spec/DECISIONS.md, "String Detune").

    The feature's promise is narrow and every part of it is checked here: each
    string's rendered pitch moves by exactly the cents set, no other string
    moves, the offset rides the string up the neck and under a capo, nothing
    gets further than 25 cents out, Randomise stays inside its range and is
    reproducible, and presets - new and old - come back as they were saved.

    Pitch is measured on rendered audio, relative to the same note rendered in
    tune: the string model's own absolute tuning error (and the estimator's)
    cancels, and what is left is the offset the feature applied.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../Character/StringDetune.h"
#include "../UI/CharacterPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    // Standard tuning's open notes, engine order (string 0 = high E).
    constexpr int kOpenMidi[6] = { 64, 59, 55, 50, 45, 40 };

    void setPlain (LuthierAudioProcessor& processor, const juce::String& id, double plain)
    {
        if (auto* p = processor.getState().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) plain));
    }

    double getPlain (LuthierAudioProcessor& processor, const juce::String& id)
    {
        if (auto* p = processor.getState().getParameter (id))
            return (double) p->convertFrom0to1 (p->getValue());

        return 0.0;
    }

    /** A rig that measures the instrument: one string per MIDI channel, no
        humanising, no random detune or drift, no character, no cab or room. */
    void quietRig (LuthierAudioProcessor& processor)
    {
        setPlain (processor, ParamIDs::playingMode, (double) (int) PlayingMode::GuitarController);
        setPlain (processor, ParamIDs::realismDetune, 0.0);
        setPlain (processor, ParamIDs::macroHumanize, 0.0);
        setPlain (processor, ParamIDs::macroCharacter, 0.0);
        setPlain (processor, ParamIDs::tuningDrift, 0.0);
        setPlain (processor, ParamIDs::cabOn, 0.0);
        setPlain (processor, ParamIDs::roomOn, 0.0);

        /*  Sympathetic coupling as low as it goes: fret 5 is the next string's
            open note, and an open string ringing in sympathy, in tune, beside a
            fretted note 15 cents out is a second peak a couple of hertz away -
            real, and exactly what String Detune makes audible, but not what
            this measures. */
        setPlain (processor, ParamIDs::couplingAmount, 0.05);

        processor.getEngine().getCharacterEngine().setEnabled (false);
    }

    /*  Hann-windowed DTFT magnitude at one frequency. Evaluated directly, so the
        peak can be found to a small fraction of a bin: an FFT's bin spacing at
        this length is about a cent at 110 Hz, and the test wants better than
        that. */
    double dtftMagnitude (const std::vector<double>& x, double hz)
    {
        const int n = (int) x.size();
        const double w = 2.0 * juce::MathConstants<double>::pi * hz / kSr;

        double re = 0.0, im = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double hann = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            const double v = x[(size_t) i] * hann;
            re += v * std::cos (w * i);
            im -= v * std::sin (w * i);
        }

        return std::sqrt (re * re + im * im);
    }

    /** The fundamental near `expectedHz`: a coarse FFT peak, then a golden-
        section search on the DTFT around it. */
    double estimateF0 (const std::vector<double>& x, double expectedHz)
    {
        const double coarse = findPeakFrequency (x.data(), (int) x.size(), kSr,
                                                 expectedHz * 0.9, expectedHz * 1.1);

        if (coarse <= 0.0)
            return 0.0;

        const double span = 2.0 * kSr / (double) x.size();   // two bins either side
        double a = coarse - span, b = coarse + span;
        const double g = 0.5 * (std::sqrt (5.0) - 1.0);

        double c = b - g * (b - a), d = a + g * (b - a);
        double fc = dtftMagnitude (x, c), fd = dtftMagnitude (x, d);

        for (int i = 0; i < 40 && (b - a) > 1.0e-5; ++i)
        {
            if (fc > fd) { b = d; d = c; fd = fc; c = b - g * (b - a); fc = dtftMagnitude (x, c); }
            else         { a = c; c = d; fc = fd; d = a + g * (b - a); fd = dtftMagnitude (x, d); }
        }

        return 0.5 * (a + b);
    }

    /** Renders one note on one string and measures its fundamental. */
    double renderPitch (int stringIndex, int fret, int capo,
                        const std::function<void (LuthierAudioProcessor&)>& setup)
    {
        LuthierAudioProcessor processor;
        quietRig (processor);
        setPlain (processor, ParamIDs::capoFret, (double) capo);

        if (setup)
            setup (processor);

        processor.prepareToPlay (kSr, kBlock);

        const int note = kOpenMidi[stringIndex] + capo + fret;
        const double expected = midiToHz ((double) note);

        const int total = (int) (kSr * 1.1);
        const int skip = (int) (kSr * 0.3);

        std::vector<double> mono;
        mono.reserve ((size_t) total);

        juce::AudioBuffer<float> block (2, kBlock);

        for (int position = 0; position < total; position += kBlock)
        {
            block.clear();
            juce::MidiBuffer midi;

            if (position == 0)
                midi.addEvent (juce::MidiMessage::noteOn (stringIndex + 1, note, 0.85f), 0);

            processor.processBlock (block, midi);

            for (int i = 0; i < kBlock; ++i)
                if (position + i >= skip)
                    mono.push_back (0.5 * ((double) block.getSample (0, i) + (double) block.getSample (1, i)));
        }

        processor.releaseResources();

        return estimateF0 (mono, expected);
    }

    double centsBetween (double hz, double referenceHz)
    {
        return (hz > 0.0 && referenceHz > 0.0) ? 1200.0 * std::log2 (hz / referenceHz) : 1.0e9;
    }

    std::function<void (LuthierAudioProcessor&)> detuneString (int stringIndex, double cents)
    {
        return [stringIndex, cents] (LuthierAudioProcessor& p)
        {
            setPlain (p, ParamIDs::stringDetune (stringIndex + 1), cents);
        };
    }
}

//==============================================================================
/*  Each string's rendered pitch moves by the cents set, to within a cent, in
    both directions; and when one string is detuned, the other five do not move. */
LUTHIER_TEST (Detune, eachStringMovesByTheCentsSetAndNoOtherStringMoves)
{
    double reference[6] {};

    for (int s = 0; s < 6; ++s)
    {
        reference[s] = renderPitch (s, 0, 0, {});
        CHECK_MSG (reference[s] > 0.0, "string " + juce::String (s + 1) + " was not measurable");
    }

    const double offsets[6] = { 12.0, -18.0, 25.0, -7.5, 20.0, -25.0 };

    for (int s = 0; s < 6; ++s)
    {
        const double measured = centsBetween (renderPitch (s, 0, 0, detuneString (s, offsets[s])), reference[s]);

        CHECK_MSG (std::abs (measured - offsets[s]) <= 1.0,
                   "string " + juce::String (s + 1) + " set to " + juce::String (offsets[s], 1)
                     + " cents moved " + juce::String (measured, 2));
    }

    // String 5 (the A) well out; every other string must sound as it did.
    const auto aOut = detuneString (4, 22.0);

    for (int s = 0; s < 6; ++s)
    {
        if (s == 4)
            continue;

        const double moved = centsBetween (renderPitch (s, 0, 0, aOut), reference[s]);

        CHECK_MSG (std::abs (moved) <= 0.25,
                   "detuning string 5 moved string " + juce::String (s + 1) + " by "
                     + juce::String (moved, 3) + " cents");
    }
}

//==============================================================================
/*  The offset rides the string: fretted at 5, and under a capo at 3. */
LUTHIER_TEST (Detune, theOffsetFollowsTheStringUpTheNeckAndUnderACapo)
{
    const int s = 3;   // the D
    const double cents = -15.0;

    {
        const double in = renderPitch (s, 5, 0, {});
        const double out = renderPitch (s, 5, 0, detuneString (s, cents));
        const double moved = centsBetween (out, in);

        CHECK_MSG (std::abs (moved - cents) <= 1.0,
                   "at fret 5 the string moved " + juce::String (moved, 2) + " cents, set "
                     + juce::String (cents, 1));
    }

    {
        const double in = renderPitch (s, 0, 3, {});
        const double out = renderPitch (s, 0, 3, detuneString (s, cents));
        const double moved = centsBetween (out, in);

        CHECK_MSG (std::abs (moved - cents) <= 1.0,
                   "under a capo at 3 the string moved " + juce::String (moved, 2) + " cents, set "
                     + juce::String (cents, 1));
    }

    {
        const double in = renderPitch (s, 5, 3, {});
        const double out = renderPitch (s, 5, 3, detuneString (s, cents));
        const double moved = centsBetween (out, in);

        CHECK_MSG (std::abs (moved - cents) <= 1.0,
                   "at fret 5 over a capo at 3 the string moved " + juce::String (moved, 2) + " cents");
    }
}

//==============================================================================
/*  The target frequency itself, which every path (frets, bends, slides, the
    capo) goes through: the ratio is the offset exactly, whatever else is
    happening, and only on the detuned string. */
LUTHIER_TEST (Detune, theTuningEngineAppliesTheOffsetOnEveryPathAndOnlyToItsString)
{
    TuningEngine plain, detuned;
    detuned.setStringDetuneCents (2, 17.0);

    for (int capo : { 0, 2, 7 })
    {
        plain.setCapoFret (capo);
        detuned.setCapoFret (capo);

        for (double fret : { 0.0, 1.0, 5.0, 7.5, 12.0 })
        {
            for (double bend : { 0.0, 100.0, 200.0, -50.0 })
            {
                for (int s = 0; s < 6; ++s)
                {
                    const double ratio = detuned.computeFrequency (s, fret, bend) / plain.computeFrequency (s, fret, bend);
                    const double want = (s == 2) ? 17.0 : 0.0;

                    CHECK_NEAR (1200.0 * std::log2 (ratio), want, 1.0e-6);
                }
            }
        }

        // A note is fretted where the in-tune guitar frets it, so the offset is
        // heard rather than fretted away (guitar-controller path, voicer slop).
        const double hz = plain.computeFrequency (2, 5.0);
        CHECK_NEAR (detuned.frequencyToFretPosition (2, hz), plain.frequencyToFretPosition (2, hz), 1.0e-9);
    }
}

//==============================================================================
/*  Never more than 25 cents from these controls: the parameter's range, the
    engine's own clamp, and the combination with drift and realism detune. */
LUTHIER_TEST (Detune, theRangeClampsAtTwentyFiveCents)
{
    LuthierAudioProcessor processor;

    for (int n = 1; n <= ParamIDs::kNumStringDetunes; ++n)
    {
        auto* p = processor.getState().getParameter (ParamIDs::stringDetune (n));
        CHECK (p != nullptr);

        if (p == nullptr)
            continue;

        CHECK_NEAR (p->convertFrom0to1 (0.0f), -25.0, 1.0e-4);
        CHECK_NEAR (p->convertFrom0to1 (1.0f),  25.0, 1.0e-4);
        CHECK_NEAR (p->convertFrom0to1 (p->getDefaultValue()), 0.0, 1.0e-4);
        CHECK (p->isAutomatable());
    }

    CHECK (ParamIDs::kNumStringDetunes == kMaxStrings);

    TuningEngine tuning;
    tuning.setStringDetuneCents (0, 40.0);
    CHECK_NEAR (tuning.getStringDetuneCents (0), 25.0, 1.0e-12);
    tuning.setStringDetuneCents (0, -400.0);
    CHECK_NEAR (tuning.getStringDetuneCents (0), -25.0, 1.0e-12);

    // With drift already pulling sharp, the control cannot add past 25.
    tuning.setStringDetuneCents (1, 25.0);
    tuning.setCharacterDriftCents (1, 10.0);
    CHECK_NEAR (tuning.getOutOfTuneCents (1), 25.0, 1.0e-12);

    // Pulling against it is fine: 10 - 25 = -15.
    tuning.setStringDetuneCents (1, -25.0);
    CHECK_NEAR (tuning.getOutOfTuneCents (1), -15.0, 1.0e-12);

    // Imperfections already past 25 are left as they were, never pushed further.
    CHECK_NEAR (TuningEngine::combineOutOfTune (-25.0, -30.0), -30.0, 1.0e-12);
    CHECK_NEAR (TuningEngine::combineOutOfTune (25.0, -30.0), -5.0, 1.0e-12);

    // No String Detune: the imperfections pass through untouched.
    CHECK_NEAR (TuningEngine::combineOutOfTune (0.0, 37.0), 37.0, 1.0e-12);

    // And the sounding pitch agrees.
    const double ratio = tuning.computeFrequency (0, 3.0) / TuningEngine().computeFrequency (0, 3.0);
    CHECK_NEAR (1200.0 * std::log2 (ratio), -25.0, 1.0e-6);
}

//==============================================================================
/*  Randomise stays within +/- amount * 25 and is deterministic for a seed;
    Reset zeroes; the Out of tune knob scales what is there. */
LUTHIER_TEST (Detune, randomiseStaysInRangeAndIsDeterministic)
{
    for (double amount : { 0.0, 0.1, 0.4, 1.0 })
    {
        for (uint64_t seed : { 1ull, 0xABCDEFull, 0x123456789ull })
        {
            const auto a = StringDetune::randomOffsets (amount, seed);
            const auto b = StringDetune::randomOffsets (amount, seed);

            for (size_t s = 0; s < a.size(); ++s)
            {
                CHECK (a[s] == b[s]);
                CHECK_MSG (std::abs (a[s]) <= amount * 25.0 + 1.0e-12,
                           "amount " + juce::String (amount) + " gave " + juce::String (a[s], 3) + " cents");
            }
        }
    }

    // Different seeds are different guitars.
    const auto one = StringDetune::randomOffsets (1.0, 7);
    const auto two = StringDetune::randomOffsets (1.0, 8);
    CHECK (one != two);

    // Through the parameters.
    LuthierAudioProcessor p1, p2;

    for (auto* p : { &p1, &p2 })
    {
        setPlain (*p, ParamIDs::outOfTune, 0.4);
        StringDetune::randomise (p->getState(), 0xFEEDull);
    }

    const auto written = StringDetune::read (p1.getState());
    const auto again = StringDetune::read (p2.getState());
    bool anyMoved = false;

    for (size_t s = 0; s < written.size(); ++s)
    {
        CHECK_NEAR (written[s], again[s], 1.0e-9);
        CHECK_MSG (std::abs (written[s]) <= 10.0 + 1.0e-3,
                   "string " + juce::String ((int) s + 1) + " randomised to " + juce::String (written[s], 3));
        anyMoved = anyMoved || std::abs (written[s]) > 0.5;
    }

    CHECK_MSG (anyMoved, "Randomise at 40% left every string in tune");

    // The Out of tune knob, turned by hand to half: every string halves.
    StringDetune::AmountDrag drag;
    drag.begin (p1.getState(), 0x1ull);
    setPlain (p1, ParamIDs::outOfTune, 0.2);
    drag.update (p1.getState(), 0.2);
    drag.end();

    const auto halved = StringDetune::read (p1.getState());

    for (size_t s = 0; s < halved.size(); ++s)
        CHECK_NEAR (halved[s], written[s] * 0.5, 1.0e-3);

    // Reset zeroes every string, and leaves the amount.
    StringDetune::reset (p1.getState());

    for (auto v : StringDetune::read (p1.getState()))
        CHECK_NEAR (v, 0.0, 1.0e-4);

    CHECK_NEAR (StringDetune::getAmount (p1.getState()), 0.2, 1.0e-4);

    // From in tune, the knob lays down the seeded pattern at its amount.
    drag.begin (p1.getState(), 0x5EEDull);
    drag.update (p1.getState(), 0.6);
    drag.end();

    const auto laid = StringDetune::read (p1.getState());
    const auto wanted = StringDetune::randomOffsets (0.6, 0x5EEDull);

    for (size_t s = 0; s < laid.size(); ++s)
        CHECK_NEAR (laid[s], wanted[s], 1.0e-3);
}

//==============================================================================
/*  Presets carry the offsets; a preset from before the feature loads in tune. */
LUTHIER_TEST (Detune, presetsRoundTripAndOldPresetsLoadInTune)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    StringDetune::Offsets saved {};

    for (int s = 0; s < kMaxStrings; ++s)
        saved[(size_t) s] = -24.0 + 4.0 * s;

    StringDetune::write (processor.getState(), saved);
    setPlain (processor, ParamIDs::outOfTune, 0.65);

    const auto data = presets.toVar ("Out of tune test");

    StringDetune::reset (processor.getState());
    setPlain (processor, ParamIDs::outOfTune, 0.0);

    CHECK (presets.fromVar (data));

    const auto loaded = StringDetune::read (processor.getState());

    for (int s = 0; s < kMaxStrings; ++s)
        CHECK_NEAR (loaded[(size_t) s], juce::jlimit (-25.0, 25.0, saved[(size_t) s]), 1.0e-3);

    CHECK_NEAR (StringDetune::getAmount (processor.getState()), 0.65, 1.0e-4);

    // An old preset: the same file without any of the new keys.
    auto old = juce::JSON::parse (juce::JSON::toString (data));

    if (auto* params = old.getProperty ("parameters", {}).getDynamicObject())
    {
        for (int n = 1; n <= ParamIDs::kNumStringDetunes; ++n)
            params->removeProperty (ParamIDs::stringDetune (n));

        params->removeProperty (ParamIDs::outOfTune);
    }
    else
    {
        CHECK_MSG (false, "the preset had no parameters block");
    }

    // The strings are out now; the old preset must put them back in tune.
    StringDetune::write (processor.getState(), saved);

    CHECK (presets.fromVar (old));

    for (auto v : StringDetune::read (processor.getState()))
        CHECK_NEAR (v, 0.0, 1.0e-4);

    CHECK_NEAR (StringDetune::getAmount (processor.getState()), 0.0, 1.0e-4);

    // And a patch randomiser does not detune the guitar.
    CHECK (! PresetManager::isRandomisable (ParamIDs::stringDetune (1)));
    CHECK (! PresetManager::isRandomisable (ParamIDs::outOfTune));
}

//==============================================================================
/*  The CHARACTER panel's group: one knob per string, labelled with the note,
    low string first; double-click returns to zero; Reset works. */
LUTHIER_TEST (Detune, theCharacterPanelShowsOneKnobPerStringLabelledByNote)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    StringDetuneGroup group (processor);
    group.setSize (360, group.preferredHeight());

    CHECK (group.getShownStringCount() == processor.getEngine().getNumStrings());

    const char* names[6] = { "E", "B", "G", "D", "A", "E" };

    for (int s = 0; s < 6; ++s)
    {
        CHECK_MSG (StringDetune::noteNameFor (processor.getEngine().getTuningEngine(), s) == names[s],
                   "string " + juce::String (s + 1) + " is labelled "
                     + StringDetune::noteNameFor (processor.getEngine().getTuningEngine(), s));

        auto* knob = group.getStringKnob (s);
        CHECK (knob != nullptr && knob->isVisible());

        if (knob == nullptr)
            continue;

        CHECK (knob->getSlider().isDoubleClickReturnEnabled());
        CHECK_NEAR (knob->getSlider().getDoubleClickReturnValue(), 0.0, 1.0e-9);
        CHECK (knob->getParameterId() == ParamIDs::stringDetune (s + 1));
    }

    // Low E on the left, high E on the right.
    CHECK (group.getStringKnob (5)->getX() < group.getStringKnob (0)->getX());

    for (int s = 6; s < kMaxStrings; ++s)
        CHECK (! group.getStringKnob (s)->isVisible());

    setPlain (processor, ParamIDs::stringDetune (3), 11.0);
    group.getResetButton().onClick();

    CHECK_NEAR (getPlain (processor, ParamIDs::stringDetune (3)), 0.0, 1.0e-4);
}
