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

    // --- routing -------------------------------------------------------------
    taps.prepare (maxBlock);
    stringActivity.clear();

    sidechainFollower.prepare (sr);
    // Fast enough to track a kick drum's attack, slow enough that the release
    // does not chatter: the same shape a hardware sidechain detector has.
    sidechainFollower.setTimes (0.003, 0.120);
    sidechainEnv.store (0.0);

    sidechainChannels = nullptr;
    sidechainNumChannels = 0;
    sidechainNumSamples = 0;
    sidechainReadOffset = 0;

    // --- model ---------------------------------------------------------------
    tuning.prepare (sr);
    technique.prepare (sr, numStrings);
    voicer.prepare (&tuning, numStrings);
    midi.prepare (sr, numStrings);
    midi.setEngines (&tuning, &technique, &voicer);

    rhythm.prepare (sr, maxBlock, &tuning, &voicer);
    rhythm.setNumStrings (numStrings);

    character.prepare (sr, numStrings);

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

    playingNoise.prepare (sr);
    slide.prepare (sr);
    noteSustainScale.fill (1.0);
    playingNoise.setSeed ((juce::uint32) (character.getSeed() ^ (character.getSeed() >> 32)));

    // --- signal chain ---------------------------------------------------------
    circuit.prepare (sr);
    preEffects.prepare (sr, maxBlock);
    preEffects.setPosition (EffectsChain::Position::PreAmp);
    amp.prepare (sr, maxBlock);
    postEffects.prepare (sr, maxBlock);
    postEffects.setPosition (EffectsChain::Position::PostAmp);
    cabinet.prepare (sr, maxBlock);
    room.prepare (sr, maxBlock);
    secret.prepare (sr);
    master.prepare (sr, maxBlock);
    freezeOverlay.prepare (sr, 2);

    // --- scratch --------------------------------------------------------------
    stringSumBuffer.assign ((size_t) maxBlock, 0.0);
    noiseBuffer.assign ((size_t) maxBlock, 0.0);
    magneticBuffer.assign ((size_t) maxBlock, 0.0);
    instrumentBuffer.assign ((size_t) maxBlock, 0.0);
    bodyBuffer.setSize (1, maxBlock, false, true, true);
    workBuffer.setSize (2, maxBlock, false, true, true);
    wetDryBuffer.setSize (2, maxBlock, false, true, true);

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
    stringActivity.clear();
    sidechainFollower.reset();
    sidechainEnv.store (0.0);

    for (auto& s : strings)
        s.reset();

    coupling.reset();
    body.reset();
    pickups.reset();
    whammy.reset();
    playingNoise.reset();
    fretBuzzModel.reset();
    slide.reset();
    noteSustainScale.fill (1.0);
    shiftCount = 0;
    circuit.reset();
    preEffects.reset();
    amp.reset();
    postEffects.reset();
    cabinet.reset();
    room.reset();
    secret.reset();
    master.reset();
    freezeOverlay.reset();

    technique.reset();
    voicer.reset();
    midi.reset();
    rhythm.reset();
    character.reset();
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
    wetDryBuffer.setSize (0, 0);

    taps.releaseResources();
    stringActivity.clear();

    sidechainChannels = nullptr;
    sidechainNumChannels = 0;
    sidechainNumSamples = 0;
    sidechainReadOffset = 0;
}

//==============================================================================
void LuthierEngine::setSidechainInput (const float* const* channels, int numChannels, int numSamples) noexcept
{
    sidechainChannels = channels;
    sidechainNumChannels = juce::jmax (0, numChannels);
    sidechainNumSamples = juce::jmax (0, numSamples);
    sidechainReadOffset = 0;
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
    rhythm.setNumStrings (numStrings);
    character.setNumStrings (numStrings);
}

//==============================================================================
void LuthierEngine::setGuitarType (GuitarType type)
{
    guitarType = type;
    spec = GuitarLibrary::get (type);

    // A compiled type: no parts, so every part-derived value is neutral. The
    // gauges are the strings part's only when leaving a parts guitar; a
    // preset's own per-string gauges survive a type change as they always did.
    if (hasPartsOverride)
        customGauges.fill (0.0);

    hasPartsOverride = false;
    partsSustain = fretBrightnessFactor = nutBrightnessFactor = magnetSustain = 1.0;
    magnetDetuneCents = 0.0;

    applySpec();
}

void LuthierEngine::applyWorkshopGuitar (const DerivedAcoustics& d)
{
    spec = d.spec;
    hasPartsOverride = true;

    for (int i = 0; i < kMaxStrings; ++i)
        customGauges[(size_t) i] = i < (int) d.gaugesIn.size() ? d.gaugesIn[(size_t) i] : 0.0;

    partsBody = d.body;

    for (int i = 0; i < 3; ++i)
    {
        partsPickups[(size_t) i] = d.pickups[(size_t) i].spec;
        partsPickups[(size_t) i].coverLossDbAt4k = d.pickups[(size_t) i].coverLossDbAt4k;
        partsPickups[(size_t) i].poleBrightness = d.pickups[(size_t) i].poleBrightness;
    }

    partsSustain = d.sustainScale;

    // part-acoustics.md 4, against the reference parts - nickel-silver frets,
    // a bone nut - so a guitar of reference parts sounds as a compiled one does.
    fretBrightnessFactor = d.fretBrightness / 0.70;
    nutBrightnessFactor = d.nutBrightness / 0.75;

    /*  6.2: magnet pull damps the string and pulls it flat, as the square of
        how close the pole pieces are. A ceramic pickup at 1.5 mm is the
        "Stratitis" case; alnico 3 at 3.5 mm barely touches the string. The
        constants put a strong close magnet at about a quarter off the sustain
        and a few cents flat, which is the order real ones measure at. */
    double pull = 0.0;

    for (int i = 0; i < d.numPickups; ++i)
    {
        const auto& p = d.pickups[(size_t) i];
        const double proximity = 2.5 / juce::jmax (0.5, p.heightMm);
        pull += p.magnetPull * (p.magnetDamping / 0.032) * proximity * proximity;
    }

    magnetSustain = 1.0 / (1.0 + 0.09 * pull);
    magnetDetuneCents = -0.9 * pull;

    applySpec();
}

void LuthierEngine::applySpec()
{
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
    auto cfg = hasPartsOverride ? partsBody : GuitarLibrary::makeBodyConfig (spec);
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
        pickups.setPickupSpec (i, hasPartsOverride && i < 3 ? partsPickups[(size_t) i]
                                                          : GuitarLibrary::makePickupSpec (spec, i));

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

    /*  Finger squeak is PlayingNoise's now (string-squeak.md): the string's
        own glide noise would squeak a second time on every legato slide. The
        Slide Noise control keeps its meaning for a bottleneck, which is set
        per note in triggerNote. */
    for (int i = 0; i < numStrings; ++i)
        strings[(size_t) i].setNoiseAmount (0.0, fretNoise);
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
    tempoBpm = bpm;
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

    /*  slide-guitar.md 1: in hybrid mode one string is under the bar and the
        fingers fret the rest, so a slide note that the bar is not on is played
        as the fretted note it is - a legato move if it came from somewhere,
        a pluck if not. */
    if (e.technique == Technique::SlideGuitar)
    {
        if (! slide.noteOn (s, numStrings))
        {
            auto fretted = e;
            fretted.technique = e.slideFromFret >= 0.0 ? Technique::Slide : Technique::Pluck;
            triggerNote (fretted);
            return;
        }

        // 5.2: the bar landing on the strings clanks.
        if (slide.isLanding())
            playingNoise.getPool().trigger (slide.makeClank (s, e.velocity));
    }

    // Validator check 2.
    bool accepted = true;
    const double fret = validator.checkFretRange (s, e.fretPosition,
                                                  tuning.getStringTuning (s).maxFrets,
                                                  samplePosition, accepted);

    if (! accepted)
        return;

    targetFret[(size_t) s] = fret;
    stringMidiNote[(size_t) s] = e.midiNote;

    // Routing-io 6: what is actually ringing, at the sample it started.
    stringActivity.push ({ activeSampleOffset, s, e.midiNote, (float) e.velocity, true });

    auto& str = strings[(size_t) s];

    // ---- pitch and glide ------------------------------------------------------
    if (e.slideFromFret >= 0.0 && e.slideSeconds > 0.0)
    {
        // Legato move: start from where the hand was and glide.
        str.snapToFrequency (tuning.computeFrequency (s, e.slideFromFret,
                                                      midi.getStringBendCents (s)));
        str.setSlideSpeed (std::abs (fret - e.slideFromFret) / juce::jmax (0.001, e.slideSeconds));

        // Under a bar the bar itself moves, block by block, and the string only
        // smooths between blocks; otherwise the string glides on its own.
        if (slide.isUnderBar (s))
        {
            slide.startMove (s, e.slideFromFret, fret, e.slideSeconds);
            str.setGlideTime (256.0 / sr);
        }
        else
        {
            str.setGlideTime (e.slideSeconds);
        }
    }
    else
    {
        str.setGlideTime (0.002);
        str.setSlideSpeed (0.0);
        str.snapToFrequency (e.pitchHz);
    }

    /*  character-wear 2 and 3: the instrument's own imperfections, applied here
        rather than per sample.

        A dead spot and a worn fret are both properties of *where* the note is
        being played, so this is the only place they can be applied: the position
        is known, the note has not started yet, and nothing has to be recomputed
        again until the next note. */
    {
        const double bodyResonance = body.getAirResonanceHz();

        const double deadSpot = character.getSustainMultiplier (s, fret, e.pitchHz, bodyResonance);
        const double fretWear = character.getFretSustainMultiplier (fret);

        // An open string is stopped by the nut rather than by a fret, so it takes
        // the nut's wear and the nut material's damping instead (character-wear 7).
        const double nut = (fret <= 0.0)
                             ? (1.0 - character.getNutDamping (s)) * character.getNutMaterialDamping()
                             : 1.0;

        // Under a slide the frets are not touched, so their wear is irrelevant
        // (slide-guitar.md 8); the bar's own damping takes its place.
        const bool underBar = slide.isUnderBar (s);

        noteSustainScale[(size_t) s] = juce::jlimit (0.05, 4.0, deadSpot * (underBar ? 1.0 : fretWear) * nut
                                                                * slide.sustainScale (s)
                                                                * partsSustain * magnetSustain);

        // part-acoustics.md 4: an open string rings off the nut, a fretted one
        // off a fret, and they are different materials.
        str.setTerminationBrightness (fret <= 0.0 ? nutBrightnessFactor : fretBrightnessFactor);
        str.setSustainScale (noteSustainScale[(size_t) s]);

        // A worn crown alters the effective string length by a few cents.
        const double detune = character.getFretDetuneCents (fret);

        if (detune != 0.0)
            str.setTargetFrequency (e.pitchHz * std::pow (2.0, detune / 1200.0));
        else
            str.setTargetFrequency (e.pitchHz);
    }

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

    // ---- playing noise (pick-noise.md, string-squeak.md) ------------------------
    // The string's own glide noise is a bottleneck's friction now; a finger's
    // squeak comes from PlayingNoise below.
    // slide-guitar.md 5.1: friction, proportional to amount x material
    // friction x bar speed. The string's glide noise is bar-speed driven.
    str.setNoiseAmount (e.technique == Technique::SlideGuitar
                          ? slide.getSettings().noiseAmount
                              * getSlideMaterial (slide.getBar().material).friction * 2.5
                              * stringSpecs[(size_t) s].squeak
                          : 0.0,
                        fretNoise);

    {
        const auto info = StringNoiseInfo::fromSpec (stringSpecs[(size_t) s], spec.stringMaterial, stringAge);

        if (e.technique == Technique::Slide && e.slideFromFret >= 0.0 && ! slide.isUnderBar (s))
        {
            // The finger stayed down and travelled: that is the squeak's trigger
            // (string-squeak.md 2). A pluck at a new position is not.
            playingNoise.onShift (s, info, spec.scaleLengthMm, e.slideFromFret, fret,
                                  e.slideSeconds > 0.0 ? e.slideSeconds : 0.12, shiftCount++);
        }
        else if (p.kind == Excitation::Kind::Pluck || p.kind == Excitation::Kind::PinchHarmonic)
        {
            auto pickNow = playingNoise.getPick();
            pickNow.material = pickMaterial;
            pickNow.fingers = usingFingers || ! PlayingNoise::getPickMaterial (pickMaterial).isPick;
            pickNow.pluckPosition = pluckPosition;
            playingNoise.setPick (pickNow);

            playingNoise.onPluck (s, info, e.velocity);
        }
    }

    // A finger landing on a fret clicks; a hammer-on clicks harder.
    if (! fretless && fretNoise > 0.001)
    {
        const double strength = (e.technique == Technique::HammerOn
                                 || e.technique == Technique::Tap) ? 0.9 : 0.45;
        str.triggerFretNoise (strength * e.velocity * fretNoise);
    }
}

//==============================================================================
void LuthierEngine::setSetupGeometry (const SetupGeometry& geometry) noexcept
{
    auto g = geometry;
    g.scaleLengthMm = spec.scaleLengthMm;
    g.numStrings = numStrings;
    fretBuzzModel.setGeometry (g);

    // The string's own contact clipper (the older, in-loop half of buzz) takes
    // its threshold from the same setup, so the two never disagree.
    fretActionMm = juce::jlimit (0.5, 4.0, 0.5 * (g.actionTreble + g.actionBass));

    for (int i = 0; i < numStrings; ++i)
        strings[(size_t) i].setFretBuzz (fretless ? 0.0 : fretBuzzAmount, fretActionMm);
}

void LuthierEngine::triggerPickScrape (double seconds, bool downward) noexcept
{
    std::array<bool, kMaxStrings> wound {};

    for (int s = 0; s < numStrings; ++s)
        wound[(size_t) s] = stringSpecs[(size_t) s].wound;

    playingNoise.startScrape (seconds, downward, wound.data(), numStrings);
}

void LuthierEngine::setPickMaterialAndFingers (Excitation::Material material, bool fingers) noexcept
{
    pickMaterial = material;
    setUseFingers (fingers);

    // setUseFingers(false) resets a finger material to a pick; a finger
    // material chosen on purpose keeps its name and plays as fingers.
    if (! fingers)
        pickMaterial = material;

    usingFingers = fingers || ! PlayingNoise::getPickMaterial (material).isPick;
}

//==============================================================================
void LuthierEngine::applyNoteOff (const NoteOffEvent& e) noexcept
{
    const int s = juce::jlimit (0, numStrings - 1, e.stringIndex);

    // Captured before it is cleared: the note-off has to name the note that was
    // sounding, not the -1 that replaces it.
    const int soundingNote = stringMidiNote[(size_t) s];

    if (soundingNote >= 0)
        stringActivity.push ({ activeSampleOffset, s, soundingNote, 0.0f, false });

    strings[(size_t) s].release (e.letRing || ebow);
    slide.noteOff (s);
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
        activeSampleOffset = (int) juce::jlimit ((int64_t) 0, (int64_t) taps.getMaxBlockSize(),
                                                 e.absoluteSample - blockStartSample);

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

        // Where in this block the event landed, for MIDI out's string-activity
        // stream. An event scheduled in an earlier block is already overdue and
        // fires on this block's first sample, which is what the clamp expresses.
        activeSampleOffset = (int) juce::jlimit ((int64_t) 0, (int64_t) taps.getMaxBlockSize(),
                                                 e.absoluteSample - blockStartSample);

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

        double hz;

        if (slide.isUnderBar (s))
        {
            /*  slide-guitar.md 3 and 4: the string is stopped where the bar
                touches it, slant included; vibrato moves the bar (depth read
                as tenths of a millimetre of travel, 3.2); and the intonation
                assist pulls the result toward equal temperament. */
            const double barFret = slide.advanceBar (s, currentFret[(size_t) s], numSamples);
            const double contact = slide.contactFret (s, barFret, numStrings, spec.scaleLengthMm);
            const double slideVibrato = SlideEngine::vibratoCents (vib / 10.0, contact, spec.scaleLengthMm);
            const double raw = contact + (bend + whammyCents + slideVibrato) / 100.0;

            hz = tuning.computeFrequency (s, slide.assist (s, raw, numSamples), 0.0);
        }
        else
        {
            hz = tuning.computeFrequency (s, currentFret[(size_t) s], bend + whammyCents + vib + magnetDetuneCents);
        }

        strings[(size_t) s].setTargetFrequency (hz);
        coupling.setStringFrequency (s, hz);

        // Freeze: drive the string toward a target level at its own resonance,
        // the way an E-Bow does. The loop gain never reaches unity, so this can
        // sustain forever without any possibility of runaway.
        if (ebow && strings[(size_t) s].hasSounded())
            strings[(size_t) s].setSustainScale (12.0);
        else
            strings[(size_t) s].setSustainScale (noteSustainScale[(size_t) s] * (fretless ? 0.82 : 1.0));
    }

    // The fretboard overlay draws the bar where the first string under it is.
    double overlay = -1.0;

    for (int s = 0; s < numStrings && overlay < 0.0; ++s)
        if (slide.isUnderBar (s))
            overlay = currentFret[(size_t) s];

    slide.setOverlayFret (overlay);

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
CircuitComponents LuthierEngine::getLiveCircuitComponents() const noexcept
{
    auto parts = circuitControls;

    // An acoustic's transducers are a piezo and a mic behind a preamp: there
    // is no coil for the pots and cable to load, whatever the pickup slots say.
    const auto coil = pickups.getSelectedCoil();
    const bool acoustic = (spec.category == GuitarCategory::Acoustic);

    parts.hasCoil = coil.hasCoil && ! acoustic;

    if (parts.hasCoil)
    {
        parts.coilInductance = coil.inductance;
        parts.coilResistance = coil.resistance;
        parts.coilCapacitance = coil.capacitance;
    }

    return parts;
}

//==============================================================================
void LuthierEngine::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) noexcept
{
    const int numSamples = buffer.getNumSamples();

    // The routing taps and the string-activity stream span the whole host block,
    // however many sub-blocks it takes to render it.
    taps.beginBlock (numSamples);
    stringActivity.clear();
    sidechainReadOffset = 0;

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

    juce::MidiBuffer sliceMidi;
    sliceMidi.ensureSize (2048);

    for (int offset = 0; offset < numSamples;)
    {
        const int count = juce::jmin (maxBlock, numSamples - offset);

        // A view onto the caller's memory: this constructor does not allocate.
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(),
                                        numChannels, offset, count);

        // Sized once, outside the loop, so splitting never allocates per slice.
        sliceMidi.clear();

        for (const auto metadata : midiMessages)
        {
            const int position = metadata.samplePosition;

            if (position >= offset && position < offset + count)
                sliceMidi.addEvent (metadata.getMessage(), position - offset);
        }

        // Each slice's taps land after the previous slice's, so the aux buses
        // come out contiguous rather than overwritten.
        taps.setWriteOffset (offset);
        sidechainReadOffset = offset;

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

    blockStartSample = samplePosition;
    lastSubBlockNumSamples = numSamples;

    buffer.clear();

    /*  character-wear 4 and 9: the instrument drifts out of tune as it is played.

        Advanced once per block, not per sample. The drift moves over tens of
        seconds, so a block's worth of resolution is thousands of times finer
        than it needs, and it keeps the whole of character out of the inner loop
        as rule 2 of that spec's section 0 requires. */
    character.advance ((double) numSamples / juce::jmax (1.0, sr));

    if (character.isEnabled())
    {
        for (int s = 0; s < numStrings; ++s)
        {
            const double drift = character.getTunerDriftCents (s);

            if (drift != lastAppliedDrift[(size_t) s])
            {
                lastAppliedDrift[(size_t) s] = drift;
                tuning.setCharacterDriftCents (s, drift);
            }
        }
    }

    // ---- 0. sidechain --------------------------------------------------------
    // The envelope runs whether or not anything is listening: it is a modulation
    // source, and a source that only updates when someone looks at it would lag.
    for (int i = 0; i < numSamples; ++i)
        sidechainFollower.process (readSidechain (i));

    sidechainEnv.store (sanitise (sidechainFollower.current()), std::memory_order_relaxed);

    // ---- 1. MIDI -------------------------------------------------------------
    // The rhythm engine sees the raw MIDI first, so it can track what is held
    // even while it is switched off and be ready the moment it is switched on.
    rhythm.handleMidi (midiMessages, samplePosition);

    midi.processBlock (midiMessages, numSamples, samplePosition, events);

    // Events go onto the schedule rather than being applied here, so a strum
    // that runs past the end of this block still sounds.
    if (rhythm.isEnabled())
    {
        rhythmEvents.clear();

        RhythmTransport transport;
        transport.bpm = tempoBpm;
        transport.ppqPosition = hostPpq;
        transport.isPlaying = hostPlaying;

        rhythm.processBlock (numSamples, transport, rhythmEvents);

        // When the rhythm engine is driving, its stream replaces the
        // interpreter's note events; the interpreter's bends and controllers
        // still apply, because those are the player's hands, not the pattern's.
        scheduleEvents (rhythm.isDriving() ? rhythmEvents : events, numSamples);
    }
    else
    {
        scheduleEvents (events, numSamples);
    }

    updatePerBlockModulation (numSamples);

    // ---- 2. strings, coupling and the magnetic pickup ------------------------
    const bool anyPickupActive = ! pickups.isSilent();
    const bool perStringTaps = taps.isPerStringWanted() && taps.getRoomAtOffset() >= numSamples;

    playingNoise.getPool().setSamplePosition (samplePosition);

    // fret-buzz.md 2: sensed at block rate, not per sample - the envelope is
    // slow and a handful of sine evaluations per string is the whole budget.
    // A fretless neck has nothing to buzz against.
    if (! fretless)
    {
        std::array<double, kMaxStrings> levels {}, fundamentals {};

        for (int s = 0; s < numStrings; ++s)
        {
            levels[(size_t) s] = strings[(size_t) s].getLevel();
            fundamentals[(size_t) s] = strings[(size_t) s].getCurrentFrequency();
        }

        fretBuzzModel.process (playingNoise.getPool(), levels.data(), currentFret.data(),
                               fundamentals.data(), numStrings, pluckPosition);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        // Events land on their exact sample, whichever block they arrived in.
        fireScheduledEvents (samplePosition + i);

        coupling.process (bridgeOutputs.data(), couplingInputs.data());

        noiseBuffer[(size_t) i] = playingNoise.processSample (excitationNoise.data(), surfaceNoise.data(),
                                                              numStrings);

        double sum = 0.0;

        for (int s = 0; s < numStrings; ++s)
        {
            // The click is part of the excitation: it goes into the string.
            double couplingIn = couplingInputs[(size_t) s] + excitationNoise[(size_t) s];

            // Acoustic feedback re-excites the string at a harmonic.
            if (feedbackAmount > 1.0e-4 && s == feedbackString)
                couplingIn += feedbackFilter.process (stringOutputs[(size_t) s])
                              * feedbackAmount * 0.02;

            // Freeze drives the string up to a target level and no further.
            if (ebow && strings[(size_t) s].hasSounded()
                && strings[(size_t) s].getLevel() < ebowTargetLevel)
            {
                couplingIn += stringOutputs[(size_t) s] * 0.004;
            }

            // Everything else is surface noise, on the string's output before
            // the body and the pickups, so the instrument colours it.
            const double out = strings[(size_t) s].processSample (couplingIn) + surfaceNoise[(size_t) s];

            stringOutputs[(size_t) s] = out;
            bridgeOutputs[(size_t) s] = strings[(size_t) s].getBridgeOutput();
            stringDelays[(size_t) s] = strings[(size_t) s].getCurrentDelaySamples();

            sum += out;
        }

        // Per-string outputs (routing-io 3). Taken here, before the body, which
        // is where section 10's test defines them: the body is convolved over
        // the summed signal, so a post-body per-string tap would mean twelve
        // body convolutions for a signal nobody asked to be coloured that way.
        if (perStringTaps)
            for (int s = 0; s < numStrings; ++s)
                taps.stringWrite (s)[i] = (float) sanitise (stringOutputs[(size_t) s]);

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
    // The circuit is rebuilt only when something in it changed; a knob move is
    // a small matrix inverse at block rate, and the network's history carries
    // across so the move does not click.
    circuit.setComponents (getLiveCircuitComponents());

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

        instrument = circuit.process (instrument);
        instrument = sanitise (instrument);

        // Internal re-amp (routing-io 5B). The sidechain replaces the string
        // engine's contribution entirely rather than mixing with it - a DI clip
        // being re-amped should hear the amp, not the amp plus a ghost guitar.
        if (sidechainToAmp)
            instrument = sanitise (readSidechain (i));

        instrumentBuffer[(size_t) i] = instrument;
        blockPeak = juce::jmax (blockPeak, std::abs (instrument));
    }

    // Aux 1: the DI, which is exactly what is about to enter the amp.
    taps.writeAuxMono (AuxBus::di, instrumentBuffer.data(), numSamples);

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

        // Aux 2: the amp before the cabinet. Taken here, after the amp and
        // before the post-amp effects, which is the point a real amp's DI or
        // slave output sits at.
        taps.writeAuxMono (AuxBus::ampPreCab, dl.data(), numSamples);

        // ---- 7. post-amp effects ----------------------------------------------
        // Aux 6 is the tails alone, so the dry signal has to be kept to subtract.
        const bool wantWetTap = taps.isAuxWanted (AuxBus::wetFx);

        if (wantWetTap)
        {
            auto* dryL = wetDryBuffer.getWritePointer (0);
            auto* dryR = wetDryBuffer.getWritePointer (1);

            for (int i = 0; i < numSamples; ++i)
            {
                dryL[i] = (float) dl[(size_t) i];
                dryR[i] = (float) dr[(size_t) i];
            }
        }

        postEffects.processStereo (dl.data(), dr.data(), numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            wl[i] = (float) dl[(size_t) i];
            wr[i] = (float) dr[(size_t) i];
        }

        if (wantWetTap)
            taps.writeAuxDifference (AuxBus::wetFx, wl, wr,
                                     wetDryBuffer.getReadPointer (0),
                                     wetDryBuffer.getReadPointer (1),
                                     numSamples);
    }

    // ---- 8. cabinet and room --------------------------------------------------
    // The room only builds its tap when someone is listening to Aux 5.
    room.setRoomTapEnabled (taps.isAuxWanted (AuxBus::roomMic));

    cabinet.processBlock (workBuffer);

    // Aux 3 and Aux 4: each mic alone, before the blend threw the separation
    // away. Nothing extra was rendered to produce these.
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto bus = (slot == 0) ? AuxBus::cabMic1 : AuxBus::cabMic2;

        if (! taps.isAuxWanted (bus))
            continue;

        if (const auto* mic = cabinet.hasMicTap (slot) ? cabinet.getMicTap (slot) : nullptr)
            taps.writeAuxStereo (bus, mic, mic,
                                 juce::jmin (numSamples, cabinet.getMicTapNumSamples()));
    }

    room.processBlock (workBuffer);

    // Aux 5: the room alone.
    if (taps.isAuxWanted (AuxBus::roomMic) && room.hasRoomTap())
        if (const auto* rl = room.getRoomTap (0))
            if (const auto* rr = room.getRoomTap (1))
                taps.writeAuxStereo (AuxBus::roomMic, rl, rr,
                                     juce::jmin (numSamples, room.getRoomTapNumSamples()));

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

    /*  Freeze (ambiguity-resolutions 2.1) sits here, ahead of the master bus,
        for two reasons: it captures the instrument as the player hears it, after
        the amp and the room; and its layer then passes through the limiter like
        everything else, so holding a freeze cannot push the output past the
        ceiling. */
    freezeOverlay.process (buffer);

    master.processBlock (buffer);

    // Aux 7: the monitor bus. Until live-performance.md gives the monitor path a
    // mix of its own, it carries the post-master output, which is what a player
    // monitoring the plugin hears.
    if (taps.isAuxWanted (AuxBus::monitor))
    {
        const auto* ml = buffer.getReadPointer (0);
        const auto* mr = (numChannels > 1) ? buffer.getReadPointer (1) : ml;
        taps.writeAuxStereo (AuxBus::monitor, ml, mr, numSamples);
    }

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
/*  Per-output latency (routing-io 7). Each tap is only delayed by the modules
    that actually sit in front of it, so a DI printed from Aux 1 lines up with
    the take without the main output's convolution latency baked into it.

    Hosts that accept only one latency value get the main output's, which is the
    largest of these by construction - every aux tap is a prefix of the main
    chain, so none of them can be later than it.
*/
int LuthierEngine::getLatencySamples (AuxBus bus) const noexcept
{
    // The instrument's own event latency is in front of every output.
    const int engineLatency = midi.getLatencySamples();

    switch (bus)
    {
        case AuxBus::di:
            // Nothing but the oversampled front end and the body.
            return engineLatency + body.getLatencySamples();

        case AuxBus::ampPreCab:
            return engineLatency + body.getLatencySamples()
                     + preEffects.getLatencySamples() + amp.getLatencySamples();

        case AuxBus::cabMic1:
        case AuxBus::cabMic2:
            // Post-cabinet but before the post-amp effects, which the mic taps
            // are branched off ahead of.
            return engineLatency + body.getLatencySamples()
                     + preEffects.getLatencySamples() + amp.getLatencySamples()
                     + cabinet.getLatencySamples();

        case AuxBus::roomMic:
        case AuxBus::wetFx:
        case AuxBus::monitor:
        default:
            // These sit at or after the end of the chain.
            return getLatencySamples();
    }
}

int LuthierEngine::getPerStringLatencySamples() const noexcept
{
    // The per-string taps are pre-body and pre-everything-else: engine only.
    return midi.getLatencySamples();
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
