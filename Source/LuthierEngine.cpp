#include "LuthierEngine.h"

namespace luthier
{

LuthierEngine::LuthierEngine()
{
    spec = GuitarLibrary::get (guitarType);

    for (int i = 0; i < kMaxStrings; ++i)
    {
        strings[(size_t) i].setIndex (i);
        currentFret[(size_t) i] = 0.0;
        targetFret[(size_t) i] = 0.0;
        stringMidiNote[(size_t) i] = -1;
        vibratoAmount[(size_t) i] = 0.0;
    }
}

LuthierEngine::~LuthierEngine() = default;

//==============================================================================
void LuthierEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    // --- model ---------------------------------------------------------------
    tuning.prepare (sr);
    technique.prepare (sr, numStrings);
    voicer.prepare (&tuning, numStrings);
    midi.prepare (sr, numStrings);
    midi.setEngines (&tuning, &technique, &voicer);

    // --- instrument ----------------------------------------------------------
    for (int i = 0; i < kMaxStrings; ++i)
    {
        strings[(size_t) i].prepare (sr, maxBlock);
        vibratoLfo[(size_t) i].prepare (sr);
        vibratoLfo[(size_t) i].setShape (Lfo::Shape::FingerVibrato);
        vibratoLfo[(size_t) i].setRate (vibratoRate);

        // Offsetting the phases stops six strings from wobbling in lockstep,
        // which no two fingers ever do.
        vibratoLfo[(size_t) i].setPhase ((double) i * 0.137);
    }

    coupling.prepare (sr, numStrings);
    body.prepare (sr, maxBlock);
    pickups.prepare (sr, numStrings);
    whammy.prepare (sr, numStrings);

    // --- signal chain ---------------------------------------------------------
    cable.prepare (sr);
    preEffects.prepare (sr, maxBlock);
    preEffects.setPosition (EffectsChain::Position::PreAmp);
    amp.prepare (sr, maxBlock);
    postEffects.prepare (sr, maxBlock);
    postEffects.setPosition (EffectsChain::Position::PostAmp);
    cabinet.prepare (sr, maxBlock);
    room.prepare (sr, maxBlock);
    secret.prepare (sr);
    master.prepare (sr, maxBlock);

    // --- scratch --------------------------------------------------------------
    stringSumBuffer.assign ((size_t) maxBlock, 0.0);
    magneticBuffer.assign ((size_t) maxBlock, 0.0);
    instrumentBuffer.assign ((size_t) maxBlock, 0.0);
    bodyBuffer.setSize (1, maxBlock, false, true, true);
    workBuffer.setSize (2, maxBlock, false, true, true);

    // Doubler: up to 40 ms of delay for the second voice.
    doublerSize = juce::nextPowerOfTwo ((int) (sr * 0.05) + 8);
    doublerMask = doublerSize - 1;
    doublerBuffer.assign ((size_t) doublerSize, 0.0);
    doublerIndex = 0;

    doublerLfo.prepare (sr);
    doublerLfo.setShape (Lfo::Shape::RandomSmooth);
    doublerLfo.setRate (0.31);

    feedbackFilter.setBandpass (sr, 440.0, 8.0);

    setGuitarType (guitarType);
    reset();
}

void LuthierEngine::reset() noexcept
{
    for (auto& s : strings)
        s.reset();

    coupling.reset();
    body.reset();
    pickups.reset();
    whammy.reset();
    cable.reset();
    preEffects.reset();
    amp.reset();
    postEffects.reset();
    cabinet.reset();
    room.reset();
    secret.reset();
    master.reset();

    technique.reset();
    voicer.reset();
    midi.reset();
    tuning.reset();
    validator.reset();

    bridgeOutputs.fill (0.0);
    couplingInputs.fill (0.0);
    stringOutputs.fill (0.0);
    stringDelays.fill (100.0);
    vibratoAmount.fill (0.0);

    // Articulation state has to go back to its initial value too. Without this a
    // reset leaves the last note's fret position and string assignment behind, so
    // the next note starts from a slide rather than from the nut and the first
    // render after a prepare does not match any later one - which would make
    // offline rendering and regression comparison meaningless.
    for (int i = 0; i < kMaxStrings; ++i)
    {
        currentFret[(size_t) i] = 0.0;
        targetFret[(size_t) i] = 0.0;
        stringMidiNote[(size_t) i] = -1;

        vibratoLfo[(size_t) i].reset();
        vibratoLfo[(size_t) i].setPhase ((double) i * 0.137);
    }

    std::fill (doublerBuffer.begin(), doublerBuffer.end(), 0.0);
    doublerIndex = 0;
    doublerLfo.reset();

    feedbackAmount = 0.0;
    feedbackString = -1;
    feedbackFilter.reset();

    numScheduled = 0;
    samplePosition = 0;
}

void LuthierEngine::releaseResources()
{
    stringSumBuffer.clear();
    magneticBuffer.clear();
    instrumentBuffer.clear();
    doublerBuffer.clear();
    bodyBuffer.setSize (0, 0);
    workBuffer.setSize (0, 0);
}

//==============================================================================
void LuthierEngine::setNumStrings (int n)
{
    numStrings = juce::jlimit (1, kMaxStrings, n);

    tuning.setNumStrings (numStrings);
    technique.setNumStrings (numStrings);
    voicer.setNumStrings (numStrings);
    midi.setNumStrings (numStrings);
    coupling.setNumStrings (numStrings);
    pickups.setNumStrings (numStrings);
    whammy.setNumStrings (numStrings);
}

//==============================================================================
void LuthierEngine::setGuitarType (GuitarType type)
{
    guitarType = type;
    spec = GuitarLibrary::get (type);

    setNumStrings (spec.numStrings);

    // --- tuning ---------------------------------------------------------------
    tuning.setTuningPreset (spec.tuning);
    tuning.setNumStrings (spec.numStrings);

    for (int i = 0; i < numStrings; ++i)
        tuning.setMaxFrets (i, spec.maxFrets);

    // A 12-string's second string in each of the lower four courses is an octave up.
    if (spec.twelveString)
    {
        double base[6];
        TuningEngine::getPresetFrequencies (spec.tuning, base, 6);

        for (int i = 0; i < numStrings; ++i)
        {
            const int course = GuitarLibrary::courseForString (i);
            const double offset = GuitarLibrary::twelveStringOctaveOffset (i);
            tuning.setOpenFrequency (i, base[juce::jlimit (0, 5, course)] * semitonesToRatio (offset));
            tuning.setMaxFrets (i, spec.maxFrets);
        }
    }

    voicer.setMaxFret (spec.maxFrets);
    voicer.setNumStrings (spec.numStrings);

    // --- physical ---------------------------------------------------------------
    fretless = spec.fretless;
    technique.setFretlessMode (fretless);
    fretActionMm = spec.fretActionMm;
    pluckPosition = spec.defaultPluckPosition;
    bodyAmount = spec.bodyAmount;

    coupling.setAmount (spec.couplingAmount);
    coupling.buildDefault (0.020);

    refreshStringPhysics();
    rebuildBodyFromSpec();
    rebuildPickupsFromSpec();

    // --- hardware and rig --------------------------------------------------------
    whammy.setBridgeType (spec.bridge);

    amp.setModel (spec.defaultAmp);

    CabinetConfig cab;
    cab.cabinet = spec.defaultCabinet;
    cab.speaker = spec.defaultSpeaker;
    cab.mic = spec.defaultMic;
    cab.position = MicPosition::OnAxisCapEdge;
    cab.distance = MicDistance::Close;
    cabinet.setConfigA (cab);

    CabinetConfig cabB = cab;
    cabB.mic = MicType::RibbonR121;
    cabB.position = MicPosition::OffAxis45;
    cabB.distance = MicDistance::Medium;
    cabinet.setConfigB (cabB);

    reloadCabinetIrs();

    // Acoustic and classical instruments use fingers by default.
    setUseFingers (spec.category == GuitarCategory::Acoustic);
}

//==============================================================================
void LuthierEngine::rebuildBodyFromSpec()
{
    auto cfg = GuitarLibrary::makeBodyConfig (spec);
    body.setBodyConfig (cfg);

    // Solid-body electrics have no cavity to convolve, so modal synthesis is the
    // honest choice there; acoustics get convolution when an IR is available and
    // fall back to modal when it is not.
    const bool acoustic = (spec.category == GuitarCategory::Acoustic);
    body.setMode (acoustic ? BodyEngine::Mode::Convolution : BodyEngine::Mode::Modal);
    body.setAmount (bodyAmount);

    reloadBodyIr();
}

//==============================================================================
void LuthierEngine::reloadBodyIr()
{
    const auto file = IrLibrary::findBodyIr (body.getBodyConfig());

    if (file.existsAsFile())
    {
        body.loadImpulseResponse (file);
        return;
    }

    // No IR for this body. Rather than let the convolution path pass the strings
    // through untouched - which would silently drop the body and break identity
    // rule 3 - switch to modal synthesis, which needs no files at all.
    if (body.getMode() == BodyEngine::Mode::Convolution
        || body.getMode() == BodyEngine::Mode::Hybrid)
    {
        body.setMode (BodyEngine::Mode::Modal);
    }
}

void LuthierEngine::reloadCabinetIrs()
{
    const auto fileA = IrLibrary::findCabIr (cabinet.getConfigA());

    if (fileA.existsAsFile())
        cabinet.loadImpulseResponse (0, fileA);

    const auto fileB = IrLibrary::findCabIr (cabinet.getConfigB());

    if (fileB.existsAsFile())
        cabinet.loadImpulseResponse (1, fileB);

    // When no IR is found the cabinet engine keeps its procedural speaker model,
    // which is why it exists: the amp is never heard without a speaker.
}

void LuthierEngine::rebuildPickupsFromSpec()
{
    pickups.setNumPickups (juce::jmax (1, spec.numPickups));

    for (int i = 0; i < PickupEngine::kMaxPickups; ++i)
        pickups.setPickupSpec (i, GuitarLibrary::makePickupSpec (spec, i));

    pickups.setSelector (spec.defaultSelector);
    pickups.setPiezoMicBlend (spec.hasInternalMic && ! spec.hasPiezo ? 1.0 : 0.35);
}

//==============================================================================
void LuthierEngine::setTuningPreset (TuningPreset preset)
{
    tuning.setTuningPreset (preset);
    tuning.setNumStrings (juce::jmax (1, TuningEngine::getPresetStringCount (preset)));

    setNumStrings (tuning.getNumStrings());

    for (int i = 0; i < numStrings; ++i)
        tuning.setMaxFrets (i, spec.maxFrets);

    refreshStringPhysics();
}

void LuthierEngine::setStringMaterial (StringMaterial m)
{
    spec.stringMaterial = m;
    refreshStringPhysics();
}

void LuthierEngine::setStringGauge (StringGauge g)
{
    spec.stringGauge = g;
    refreshStringPhysics();
}

void LuthierEngine::setStringAge (StringAge a)
{
    stringAge = a;
    refreshStringPhysics();
}

void LuthierEngine::setCustomStringGauge (int stringIndex, double inches)
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
    {
        customGauges[(size_t) stringIndex] = juce::jlimit (0.0, 0.2, inches);
        refreshStringPhysics();
    }
}

//==============================================================================
void LuthierEngine::refreshStringPhysics()
{
    for (int i = 0; i < numStrings; ++i)
    {
        const double openHz = tuning.getEffectiveOpenFrequency (i);

        auto s = StringMaterials::computeSpec (spec.stringMaterial,
                                               spec.stringGauge,
                                               stringAge,
                                               i,
                                               openHz,
                                               spec.scaleLengthMm,
                                               customGauges[(size_t) i]);

        // Validator check 1: a tuning that would need an impossible tension is
        // corrected, and the correction is logged.
        bool accepted = true;
        s.tensionNewtons = validator.checkTension (i, s.tensionNewtons, spec.scaleLengthMm,
                                                  samplePosition, accepted);

        // Validator check 3: the decay envelope has to be plausible for the pitch.
        s.sustainSeconds = validator.checkDamping (i, s.sustainSeconds, openHz, samplePosition);

        stringSpecs[(size_t) i] = s;

        auto physical = StringMaterials::toPhysical (s, spec.scaleLengthMm);
        strings[(size_t) i].setPhysical (physical);
        strings[(size_t) i].setNoiseAmount (slideNoise * s.squeak, fretNoise);
        strings[(size_t) i].setFretBuzz (fretless ? 0.0 : fretBuzzAmount, fretActionMm);
        strings[(size_t) i].snapToFrequency (openHz);

        coupling.setStringFrequency (i, openHz);

        // Old strings do not hold their tuning.
        if (s.ageDetuneCents > 0.0)
        {
            RtRandom r { 0xA6E0000ull + (uint64_t) i };
            tuning.setFineTuneCents (i, r.nextBipolar() * s.ageDetuneCents);
        }
    }
}

//==============================================================================
void LuthierEngine::setBodyAmount (double amount) noexcept
{
    bodyAmount = juce::jlimit (0.0, 1.0, amount);
    body.setAmount (bodyAmount);
}

void LuthierEngine::setPluckPosition (double position) noexcept
{
    pluckPosition = juce::jlimit (0.02, 0.5, position);
}

void LuthierEngine::setUseFingers (bool useFingers) noexcept
{
    usingFingers = useFingers;

    if (useFingers)
        pickMaterial = (nailVsFlesh > 0.5) ? Excitation::Material::Fingernail
                                           : Excitation::Material::Fingertip;
    else if (pickMaterial == Excitation::Material::Fingertip
             || pickMaterial == Excitation::Material::Fingernail
             || pickMaterial == Excitation::Material::Thumb)
        pickMaterial = Excitation::Material::PickCelluloid;
}

void LuthierEngine::setFretless (bool f) noexcept
{
    fretless = f;
    technique.setFretlessMode (f);

    for (int i = 0; i < numStrings; ++i)
    {
        // No frets means no fret buzz, and a finger on a wound string damps it
        // differently from a steel fret: slightly shorter sustain, darker attack.
        strings[(size_t) i].setFretBuzz (f ? 0.0 : fretBuzzAmount, fretActionMm);
        strings[(size_t) i].setSustainScale (f ? 0.82 : 1.0);
    }
}

void LuthierEngine::setFretAction (double mm) noexcept
{
    fretActionMm = juce::jlimit (0.5, 4.0, mm);

    for (int i = 0; i < numStrings; ++i)
        strings[(size_t) i].setFretBuzz (fretless ? 0.0 : fretBuzzAmount, fretActionMm);
}

void LuthierEngine::setFretBuzzAmount (double amount) noexcept
{
    fretBuzzAmount = juce::jlimit (0.0, 1.0, amount);

    for (int i = 0; i < numStrings; ++i)
        strings[(size_t) i].setFretBuzz (fretless ? 0.0 : fretBuzzAmount, fretActionMm);
}

void LuthierEngine::setNoiseAmounts (double slide, double fret, double release,
                                     double knock, double pickAttack) noexcept
{
    slideNoise = juce::jlimit (0.0, 1.0, slide);
    fretNoise = juce::jlimit (0.0, 1.0, fret);
    releaseNoise = juce::jlimit (0.0, 1.0, release);
    bodyKnockAmount = juce::jlimit (0.0, 1.0, knock);
    pickAttackNoise = juce::jlimit (0.0, 1.0, pickAttack);

    for (int i = 0; i < numStrings; ++i)
        strings[(size_t) i].setNoiseAmount (slideNoise * stringSpecs[(size_t) i].squeak, fretNoise);
}

void LuthierEngine::setAmpBuzzAmount (double amount) noexcept
{
    pickups.setHumAmount (amount);
}

void LuthierEngine::setVibratoShape (Lfo::Shape s) noexcept
{
    for (int i = 0; i < kMaxStrings; ++i)
        vibratoLfo[(size_t) i].setShape (s);
}

void LuthierEngine::setOversamplingFactor (int factor) noexcept
{
    oversamplingFactor = juce::jlimit (1, 8, factor);
    amp.setOversamplingFactor (oversamplingFactor);
    preEffects.setOversamplingFactor (oversamplingFactor);
    postEffects.setOversamplingFactor (oversamplingFactor);
}

void LuthierEngine::setTempoBpm (double bpm) noexcept
{
    preEffects.setTempoBpm (bpm);
    postEffects.setTempoBpm (bpm);
}

//==============================================================================
void LuthierEngine::panic() noexcept
{
    PlayEventQueue q;
    midi.allNotesOff (q);

    for (int i = 0; i < numStrings; ++i)
    {
        strings[(size_t) i].setDamping (StringEngine::Damping::Choked, 1.0);
        strings[(size_t) i].reset();
        vibratoAmount[(size_t) i] = 0.0;
        stringMidiNote[(size_t) i] = -1;
    }

    feedbackAmount = 0.0;
    feedbackString = -1;
    numScheduled = 0;
}

//==============================================================================
void LuthierEngine::triggerNote (const NoteOnEvent& e) noexcept
{
    const int s = juce::jlimit (0, numStrings - 1, e.stringIndex);

    // Validator check 2.
    bool accepted = true;
    const double fret = validator.checkFretRange (s, e.fretPosition,
                                                  tuning.getStringTuning (s).maxFrets,
                                                  samplePosition, accepted);

    if (! accepted)
        return;

    targetFret[(size_t) s] = fret;
    stringMidiNote[(size_t) s] = e.midiNote;

    auto& str = strings[(size_t) s];

    // ---- pitch and glide ------------------------------------------------------
    if (e.slideFromFret >= 0.0 && e.slideSeconds > 0.0)
    {
        // Legato move: start from where the hand was and glide.
        str.snapToFrequency (tuning.computeFrequency (s, e.slideFromFret,
                                                      midi.getStringBendCents (s)));
        str.setGlideTime (e.slideSeconds);
        str.setSlideSpeed (std::abs (fret - e.slideFromFret) / juce::jmax (0.001, e.slideSeconds));
    }
    else
    {
        str.setGlideTime (0.002);
        str.setSlideSpeed (0.0);
        str.snapToFrequency (e.pitchHz);
    }

    str.setTargetFrequency (e.pitchHz);
    currentFret[(size_t) s] = fret;

    // ---- damping from the technique --------------------------------------------
    switch (e.technique)
    {
        case Technique::PalmMute:
            str.setDamping (StringEngine::Damping::PalmMute, technique.getPalmMuteAmount());
            break;

        case Technique::MutedPick:
            str.setDamping (StringEngine::Damping::LightTouch, 0.8);
            break;

        default:
            str.setDamping (StringEngine::Damping::Open, 1.0);
            break;
    }

    str.setHarmonicRestriction (e.harmonicPartial);

    // ---- build the excitation ---------------------------------------------------
    Excitation::Params p;
    p.material = pickMaterial;
    p.pluckPosition = pluckPosition;
    p.velocity = e.velocity;
    p.pickThickness = pickThickness;
    p.pickAngle = pickAngle;
    p.brightness = attackBrightness;
    p.nailVsFlesh = nailVsFlesh;
    p.harmonicNumber = e.harmonicPartial;
    p.noiseAmount = pickAttackNoise * 0.4;

    switch (e.technique)
    {
        case Technique::HammerOn:           p.kind = Excitation::Kind::HammerOn; break;
        case Technique::PullOff:            p.kind = Excitation::Kind::PullOff; break;
        case Technique::Tap:                p.kind = Excitation::Kind::Tap; break;
        case Technique::NaturalHarmonic:    p.kind = Excitation::Kind::Harmonic; break;
        case Technique::ArtificialHarmonic: p.kind = Excitation::Kind::Harmonic; break;
        case Technique::PinchHarmonic:      p.kind = Excitation::Kind::PinchHarmonic; break;

        case Technique::SlideGuitar:
            p.kind = Excitation::Kind::Pluck;
            p.material = Excitation::Material::Slide;
            break;

        case Technique::Slide:
            // A legato slide is not re-picked: the string keeps its energy and only
            // the pitch moves. Injecting a fresh pluck here is the classic mistake
            // that makes a "slide" sound like two separate notes.
            p.kind = Excitation::Kind::HammerOn;
            p.velocity *= 0.35;
            break;

        default:
            p.kind = Excitation::Kind::Pluck;
            break;
    }

    str.excite (p);

    // A finger landing on a fret clicks; a hammer-on clicks harder.
    if (! fretless && fretNoise > 0.001)
    {
        const double strength = (e.technique == Technique::HammerOn
                                 || e.technique == Technique::Tap) ? 0.9 : 0.45;
        str.triggerFretNoise (strength * e.velocity * fretNoise);
    }
}

//==============================================================================
void LuthierEngine::applyNoteOff (const NoteOffEvent& e) noexcept
{
    const int s = juce::jlimit (0, numStrings - 1, e.stringIndex);

    strings[(size_t) s].release (e.letRing || freeze);
    stringMidiNote[(size_t) s] = -1;

    // Lifting a finger makes a soft thump as the string is stopped.
    if (! e.letRing && releaseNoise > 0.001)
        strings[(size_t) s].triggerFretNoise (0.25 * releaseNoise);
}

//==============================================================================
void LuthierEngine::scheduleEvents (const PlayEventQueue& queue, int numSamples) noexcept
{
    juce::ignoreUnused (numSamples);

    auto push = [this] (const ScheduledEvent& e)
    {
        if (numScheduled < kMaxScheduledEvents)
        {
            scheduled[(size_t) numScheduled++] = e;
            return;
        }

        // The schedule is full, which takes a genuinely pathological amount of
        // simultaneous activity. Firing immediately is wrong-but-audible; dropping
        // would be silent, and a missing note is the worse failure.
        if (e.isNoteOn)
            triggerNote (e.noteOn);
        else
            applyNoteOff (e.noteOff);
    };

    for (int i = 0; i < queue.getNumNoteOns(); ++i)
    {
        ScheduledEvent e;
        e.isNoteOn = true;
        e.noteOn = queue.getNoteOn (i);
        e.absoluteSample = samplePosition + e.noteOn.sampleOffset;
        push (e);
    }

    for (int i = 0; i < queue.getNumNoteOffs(); ++i)
    {
        ScheduledEvent e;
        e.isNoteOn = false;
        e.noteOff = queue.getNoteOff (i);
        e.absoluteSample = samplePosition + e.noteOff.sampleOffset;
        push (e);
    }
}

void LuthierEngine::fireScheduledEvents (int64_t absoluteSample) noexcept
{
    for (int i = 0; i < numScheduled;)
    {
        auto& e = scheduled[(size_t) i];

        if (e.absoluteSample > absoluteSample)
        {
            ++i;
            continue;
        }

        if (e.isNoteOn)
            triggerNote (e.noteOn);
        else
            applyNoteOff (e.noteOff);

        // Swap-remove: order within a single sample does not matter, and this
        // keeps the cost at O(1) per event.
        scheduled[(size_t) i] = scheduled[(size_t) (--numScheduled)];
    }
}

//==============================================================================
void LuthierEngine::updatePerBlockModulation (int numSamples) noexcept
{
    whammy.setPosition (midi.getWhammyPosition());
    whammy.updateBlock (numSamples);

    const double vibratoDepthFromCc = midi.getVibratoDepth();

    for (int s = 0; s < numStrings; ++s)
    {
        vibratoLfo[(size_t) s].setRate (vibratoRate);

        // Vibrato depth comes from the mod wheel or aftertouch, and it ramps in
        // rather than appearing instantly: a player's hand takes a moment to start
        // shaking the string.
        const double target = vibratoDepthFromCc * vibratoDepthCents;
        vibratoAmount[(size_t) s] += (target - vibratoAmount[(size_t) s]) * 0.02;

        double vib = 0.0;

        if (vibratoAmount[(size_t) s] > 0.01)
        {
            // Advance the LFO by a whole block, reading the value at the end.
            for (int i = 0; i < numSamples; ++i)
                vib = vibratoLfo[(size_t) s].next();

            vib *= vibratoAmount[(size_t) s];
        }

        const double bend = midi.getStringBendCents (s);
        const double whammyCents = whammy.getCentOffset (s);

        const double hz = tuning.computeFrequency (s, currentFret[(size_t) s],
                                                   bend + whammyCents + vib);

        strings[(size_t) s].setTargetFrequency (hz);
        coupling.setStringFrequency (s, hz);

        // Freeze: drive the string toward a target level at its own resonance,
        // the way an E-Bow does. The loop gain never reaches unity, so this can
        // sustain forever without any possibility of runaway.
        if (freeze && strings[(size_t) s].hasSounded())
            strings[(size_t) s].setSustainScale (12.0);
        else
            strings[(size_t) s].setSustainScale (fretless ? 0.82 : 1.0);
    }

    tuning.advanceDrift (numSamples);
}

//==============================================================================
void LuthierEngine::processFeedback (double outputLevel) noexcept
{
    if (! feedbackEnabled)
    {
        feedbackAmount *= 0.995;
        return;
    }

    // Feedback needs a loud amp and a note that is already ringing. It builds
    // gradually and then takes over, which is the behaviour that makes it musical
    // rather than a squeal that arrives all at once.
    if (outputLevel > feedbackThreshold)
    {
        const double rate = 0.00002 + feedbackSpeed * 0.00035;
        feedbackAmount = juce::jmin (1.0, feedbackAmount + rate);
    }
    else
    {
        feedbackAmount *= 0.9995;
    }

    if (feedbackAmount < 1.0e-4)
    {
        feedbackString = -1;
        return;
    }

    // Pick the loudest ringing string to feed back.
    if (feedbackString < 0 || strings[(size_t) feedbackString].getLevel() < 1.0e-4)
    {
        double best = 1.0e-4;
        feedbackString = -1;

        for (int s = 0; s < numStrings; ++s)
        {
            const double level = strings[(size_t) s].getLevel();

            if (level > best)
            {
                best = level;
                feedbackString = s;
            }
        }

        // As the feedback grows it climbs to a higher harmonic, which is what a
        // guitar in front of a loud amp actually does.
        feedbackPartial = 2 + (int) (feedbackAmount * 3.0);
    }

    if (feedbackString >= 0)
    {
        const double f0 = strings[(size_t) feedbackString].getCurrentFrequency();
        const double target = juce::jlimit (60.0, sr * 0.45, f0 * (double) feedbackPartial);
        feedbackFilter.setBandpass (sr, target, 12.0);
    }
}

//==============================================================================
void LuthierEngine::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) noexcept
{
    const int numSamples = buffer.getNumSamples();

    if (numSamples <= maxBlock)
    {
        processSubBlock (buffer, midiMessages);
        return;
    }

    // The host has handed us a bigger block than it promised in prepareToPlay.
    // Every scratch buffer here is sized from that promise, so writing the whole
    // thing would run off the end. Splitting is the only correct response:
    // allocating would be worse, and truncating would drop audio.
    const int numChannels = buffer.getNumChannels();

    for (int offset = 0; offset < numSamples;)
    {
        const int count = juce::jmin (maxBlock, numSamples - offset);

        // A view onto the caller's memory: this constructor does not allocate.
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(),
                                        numChannels, offset, count);

        juce::MidiBuffer sliceMidi;

        for (const auto metadata : midiMessages)
        {
            const int position = metadata.samplePosition;

            if (position >= offset && position < offset + count)
                sliceMidi.addEvent (metadata.getMessage(), position - offset);
        }

        processSubBlock (slice, sliceMidi);
        offset += count;
    }
}

void LuthierEngine::processSubBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto startTicks = juce::Time::getHighResolutionTicks();

    buffer.clear();

    // ---- 1. MIDI -------------------------------------------------------------
    midi.processBlock (midiMessages, numSamples, samplePosition, events);

    // Events go onto the schedule rather than being applied here, so a strum
    // that runs past the end of this block still sounds.
    scheduleEvents (events, numSamples);

    updatePerBlockModulation (numSamples);

    // ---- 2. strings, coupling and the magnetic pickup ------------------------
    const bool anyPickupActive = ! pickups.isSilent();

    for (int i = 0; i < numSamples; ++i)
    {
        // Events land on their exact sample, whichever block they arrived in.
        fireScheduledEvents (samplePosition + i);

        coupling.process (bridgeOutputs.data(), couplingInputs.data());

        double sum = 0.0;

        for (int s = 0; s < numStrings; ++s)
        {
            double couplingIn = couplingInputs[(size_t) s];

            // Acoustic feedback re-excites the string at a harmonic.
            if (feedbackAmount > 1.0e-4 && s == feedbackString)
                couplingIn += feedbackFilter.process (stringOutputs[(size_t) s])
                              * feedbackAmount * 0.02;

            // Freeze drives the string up to a target level and no further.
            if (freeze && strings[(size_t) s].hasSounded()
                && strings[(size_t) s].getLevel() < freezeTargetLevel)
            {
                couplingIn += stringOutputs[(size_t) s] * 0.004;
            }

            const double out = strings[(size_t) s].processSample (couplingIn);

            stringOutputs[(size_t) s] = out;
            bridgeOutputs[(size_t) s] = strings[(size_t) s].getBridgeOutput();
            stringDelays[(size_t) s] = strings[(size_t) s].getCurrentDelaySamples();

            sum += out;
        }

        // Normalise by string count so a 12-string is not twice as loud as a 6.
        sum *= 1.0 / std::sqrt ((double) juce::jmax (1, numStrings));

        // Whammy spring noise rides on the instrument bus, not the strings.
        sum += whammy.processSpringNoise();

        stringSumBuffer[(size_t) i] = sanitise (sum);
        magneticBuffer[(size_t) i] = anyPickupActive
                                       ? pickups.processStrings (stringOutputs.data(),
                                                                 stringDelays.data(),
                                                                 numStrings)
                                       : 0.0;
    }

    validator.reportCouplingLimiting (coupling.getLastLimiting(), samplePosition);

    // ---- 3. body -------------------------------------------------------------
    bodyBuffer.setSize (1, numSamples, false, false, true);
    auto* bodyData = bodyBuffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
        bodyData[i] = (float) stringSumBuffer[(size_t) i];

    const bool bodyActive = (body.getMode() != BodyEngine::Mode::Bypassed);
    validator.checkBodyCoupling (bodyActive, samplePosition);

    {
        juce::dsp::AudioBlock<float> block (bodyBuffer);
        auto sub = block.getSubBlock (0, (size_t) numSamples);
        body.processBlock (sub);
    }

    // ---- 4. combine the transducer paths ------------------------------------
    const bool acoustic = (spec.category == GuitarCategory::Acoustic);
    const double micBlend = pickups.getPiezoMicBlend();

    double blockPeak = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        double instrument = 0.0;

        if (acoustic)
        {
            // Piezo senses the bridge (the raw string sum); the internal mic hears
            // the body. The blend between them is the acoustic-electric sound.
            const double piezo = pickups.processPiezo (stringSumBuffer[(size_t) i]);
            const double mic = pickups.processInternalMic ((double) bodyData[i]);
            instrument = piezo * (1.0 - micBlend) + mic * micBlend;
        }
        else
        {
            // Electric: the magnetic pickup carries the signal, and the body's
            // resonance colours it. Identity rule 3 - even a solid body is in there.
            instrument = magneticBuffer[(size_t) i] * (1.0 - bodyAmount * 0.55)
                         + (double) bodyData[i] * bodyAmount * 0.55;
        }

        instrument = cable.process (instrument);
        instrument = sanitise (instrument);

        instrumentBuffer[(size_t) i] = instrument;
        blockPeak = juce::jmax (blockPeak, std::abs (instrument));
    }

    validator.checkPickupOutput (anyPickupActive || acoustic, blockPeak, samplePosition);

    // ---- 5. pre-amp effects ---------------------------------------------------
    workBuffer.setSize (2, numSamples, false, false, true);
    auto* wl = workBuffer.getWritePointer (0);
    auto* wr = workBuffer.getWritePointer (1);

    preEffects.setExpression (midi.getExpression());
    postEffects.setExpression (midi.getExpression());

    {
        // The pedalboard is stereo-capable but the guitar is mono up to here.
        static thread_local std::vector<double> dl, dr;

        if ((int) dl.size() < numSamples) { dl.resize ((size_t) numSamples); dr.resize ((size_t) numSamples); }

        for (int i = 0; i < numSamples; ++i)
        {
            dl[(size_t) i] = instrumentBuffer[(size_t) i];
            dr[(size_t) i] = instrumentBuffer[(size_t) i];
        }

        preEffects.processStereo (dl.data(), dr.data(), numSamples);

        // ---- 6. amp (mono) ----------------------------------------------------
        for (int i = 0; i < numSamples; ++i)
        {
            const double mono = (dl[(size_t) i] + dr[(size_t) i]) * 0.5;
            const double amped = amp.processSample (mono);
            dl[(size_t) i] = amped;
            dr[(size_t) i] = amped;
        }

        // ---- 7. post-amp effects ----------------------------------------------
        postEffects.processStereo (dl.data(), dr.data(), numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            wl[i] = (float) dl[(size_t) i];
            wr[i] = (float) dr[(size_t) i];
        }
    }

    // ---- 8. cabinet and room --------------------------------------------------
    cabinet.processBlock (workBuffer);
    room.processBlock (workBuffer);

    // ---- 8b. the hidden effect --------------------------------------------------
    if (secret.isEnabled())
    {
        static thread_local std::vector<double> sl, sr2;

        if ((int) sl.size() < numSamples) { sl.resize ((size_t) numSamples); sr2.resize ((size_t) numSamples); }

        for (int i = 0; i < numSamples; ++i)
        {
            sl[(size_t) i] = (double) wl[i];
            sr2[(size_t) i] = (double) wr[i];
        }

        secret.process (sl.data(), sr2.data(), numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            wl[i] = (float) sl[(size_t) i];
            wr[i] = (float) sr2[(size_t) i];
        }
    }

    // ---- 9. doubler -----------------------------------------------------------
    if (doublerEnabled && doublerAmount > 1.0e-4 && doublerSize > 0)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double mono = 0.5 * ((double) wl[i] + (double) wr[i]);

            doublerBuffer[(size_t) doublerIndex] = flushDenormal (mono);

            // A real double track is a second performance: slightly late, slightly
            // out of tune, and never identical. A fixed delay would just comb.
            const double wobble = doublerLfo.next();
            const double delaySamples = juce::jlimit (2.0, (double) (doublerSize - 4),
                                                      sr * (0.018 + wobble * 0.004));

            const int i0 = (int) delaySamples;
            const double frac = delaySamples - (double) i0;

            const double a = doublerBuffer[(size_t) ((doublerIndex - i0) & doublerMask)];
            const double b = doublerBuffer[(size_t) ((doublerIndex - i0 - 1) & doublerMask)];
            const double doubled = a * (1.0 - frac) + b * frac;

            doublerIndex = (doublerIndex + 1) & doublerMask;

            // The double is panned opposite the original, which is what makes the
            // classic wide guitar sound.
            wl[i] = (float) sanitise ((double) wl[i] * (1.0 - doublerAmount * 0.3)
                                      + doubled * doublerAmount * 0.0);
            wr[i] = (float) sanitise ((double) wr[i] * (1.0 - doublerAmount * 0.3)
                                      + doubled * doublerAmount * 0.85);
        }
    }

    // ---- 10. master ------------------------------------------------------------
    for (int ch = 0; ch < juce::jmin (numChannels, 2); ++ch)
        buffer.copyFrom (ch, 0, workBuffer, ch, 0, numSamples);

    if (numChannels == 1)
        buffer.addFrom (0, 0, workBuffer, 1, 0, numSamples, 0.5f);

    master.processBlock (buffer);

    processFeedback (juce::jmax (master.getPeakLeft(), master.getPeakRight()));

    samplePosition += numSamples;

    // ---- CPU estimate ----------------------------------------------------------
    const auto elapsed = juce::Time::highResolutionTicksToSeconds (
        juce::Time::getHighResolutionTicks() - startTicks);
    const double budget = (double) numSamples / sr;
    const double instant = (budget > 0.0) ? (elapsed / budget) * 100.0 : 0.0;

    cpuEstimate.store (cpuEstimate.load (std::memory_order_relaxed) * 0.9 + instant * 0.1,
                       std::memory_order_relaxed);
}

//==============================================================================
int LuthierEngine::getLatencySamples() const noexcept
{
    int latency = 0;

    latency += body.getLatencySamples();
    latency += cabinet.getLatencySamples();
    latency += preEffects.getLatencySamples();
    latency += postEffects.getLatencySamples();
    latency += amp.getLatencySamples();
    latency += midi.getLatencySamples();

    return latency;
}

//==============================================================================
double LuthierEngine::getStringLevel (int i) const noexcept
{
    if (! juce::isPositiveAndBelow (i, kMaxStrings))
        return 0.0;

    return strings[(size_t) i].getLevel();
}

double LuthierEngine::getStringFrequency (int i) const noexcept
{
    if (! juce::isPositiveAndBelow (i, kMaxStrings))
        return 0.0;

    return strings[(size_t) i].getCurrentFrequency();
}

int LuthierEngine::getStringMidiNote (int i) const noexcept
{
    if (! juce::isPositiveAndBelow (i, kMaxStrings))
        return -1;

    return stringMidiNote[(size_t) i];
}

double LuthierEngine::getStringFret (int i) const noexcept
{
    if (! juce::isPositiveAndBelow (i, kMaxStrings))
        return 0.0;

    return currentFret[(size_t) i];
}

double LuthierEngine::getStringTensionNewtons (int i) const noexcept
{
    if (! juce::isPositiveAndBelow (i, kMaxStrings))
        return 0.0;

    return stringSpecs[(size_t) i].tensionNewtons;
}

const StringSpec& LuthierEngine::getStringSpec (int i) const noexcept
{
    return stringSpecs[(size_t) juce::jlimit (0, kMaxStrings - 1, i)];
}

} // namespace luthier
