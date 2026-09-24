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

    parkedMidi.clear();
    parkedMidi.ensureSize ((size_t) kParkedMidiBytes);

    dryBuffer.assign ((size_t) maxBlock, 0.0);

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
    rhythmVoicer.prepare (&tuning, numStrings);
    midi.prepare (sr, numStrings);
    midi.setEngines (&tuning, &technique, &voicer);

    rhythm.prepare (sr, maxBlock, &tuning, &rhythmVoicer);
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
    scrape.prepare (sr, maxBlock);
    scrapeMidi.ensureSize (8192);
    noteSustainScale.fill (1.0);
    playingNoise.setSeed ((juce::uint32) (character.getSeed() ^ (character.getSeed() >> 32)));
    scrape.setSeed ((juce::uint32) (character.getSeed() ^ (character.getSeed() >> 32)) ^ 0x5c4a9e11u);

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

    feedbackLoop.prepare (sr, maxBlock);
    ebowDriver.prepare (sr);
    feedbackInjection.assign ((size_t) maxBlock, 0.0);

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
    scrape.reset();
    noteSustainScale.fill (1.0);
    shiftCount = 0;

    // Rebuild a changed circuit now rather than in the first block after.
    circuit.setComponents (getLiveCircuitComponents());
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
    rhythmVoicer.reset();
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

    // tuning.reset() zeroes each string's character drift; this cache of what
    // was last sent must agree, or an unchanged drift is never re-applied.
    lastAppliedDrift.fill (0.0);

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

        // Back to the open string, not wherever the last render's last note
        // left it: an unplayed string still rings sympathetically, and its
        // pitch would otherwise depend on the previous preset.
        strings[(size_t) i].snapToFrequency (tuning.computeFrequency (i, 0.0));
    }

    feedbackLoop.reset();

    numScheduled = 0;
    samplePosition = 0;

    // The tone strip's ramps land where they are heading, like every smoother.
    inputGainNow = inputGainTarget.load (std::memory_order_relaxed);
    outputMixNow = outputMixTarget.load (std::memory_order_relaxed);
    widthNow = widthTarget.load (std::memory_order_relaxed);

    // A structural change applied with no audio running leaves a fade-in
    // pending. Reset already starts from silence, so there is nothing to fade
    // from, and leaving it would make the first render differ from the second.
    if (swapState.load (std::memory_order_acquire) == swapIdle)
        swapPhase = 1.0;
}

void LuthierEngine::releaseResources()
{
    stringSumBuffer.clear();
    magneticBuffer.clear();
    instrumentBuffer.clear();
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
    rhythmVoicer.setNumStrings (numStrings);
    midi.setNumStrings (numStrings);
    coupling.setNumStrings (numStrings);
    pickups.setNumStrings (numStrings);
    whammy.setNumStrings (numStrings);
    rhythm.setNumStrings (numStrings);
    character.setNumStrings (numStrings);

    // The buzz geometry carries the guitar's scale and string count; a setup
    // applied before a guitar change would otherwise keep the old guitar's
    // (the first render after a preset load buzzed where the next did not).
    setSetupGeometry (requestedSetup);
}

//==============================================================================
void LuthierEngine::setGuitarType (GuitarType type)
{
    const ScopedStructuralChange change (*this);

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

void LuthierEngine::applyWorkshopGuitar (const DerivedAcoustics& d, GuitarType standsFor)
{
    const ScopedStructuralChange change (*this);

    guitarType = standsFor;
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

    if (spec.twelveString)
        applyTwelveStringTuning();

    voicer.setMaxFret (spec.maxFrets);
    voicer.setNumStrings (spec.numStrings);
    rhythmVoicer.setMaxFret (spec.maxFrets);
    rhythmVoicer.setNumStrings (spec.numStrings);

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
    // A 12-string's preset names its six courses; the pairs are rebuilt from it
    // rather than the guitar collapsing to the preset's six strings.
    if (spec.twelveString && TuningEngine::getPresetStringCount (preset) == 6)
    {
        spec.tuning = preset;
        applyTwelveStringTuning();
        refreshStringPhysics();
        return;
    }

    tuning.setTuningPreset (preset);
    tuning.setNumStrings (juce::jmax (1, TuningEngine::getPresetStringCount (preset)));

    setNumStrings (tuning.getNumStrings());

    for (int i = 0; i < numStrings; ++i)
        tuning.setMaxFrets (i, spec.maxFrets);

    refreshStringPhysics();
}

void LuthierEngine::applyTwelveStringTuning()
{
    tuning.setTuningPreset (spec.tuning);
    tuning.setNumStrings (12);
    setNumStrings (12);

    // A 12-string's second string in each of the lower four courses is an octave up.
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
    // 9.1: all notes off on every string, and nothing left waiting to sound.
    PlayEventQueue q;
    midi.allNotesOff (q);
    events.clear();
    rhythmEvents.clear();
    directEvents.clear();
    numScheduled = 0;

    for (int i = 0; i < numStrings; ++i)
    {
        strings[(size_t) i].setDamping (StringEngine::Damping::Choked, 1.0);
        strings[(size_t) i].reset();
        vibratoAmount[(size_t) i] = 0.0;
        stringMidiNote[(size_t) i] = -1;
    }

    // The rhythm engine's held chord and pending strums go too; whether it is
    // enabled is a setting, and 9.6 leaves settings alone.
    rhythm.reset();
    technique.reset();

    // 9.2 - 9.4: every tail - the body's and the cabinet's resonances, the
    // circuit, the effects, the amp's DC and feedback, the room - and the
    // coupling state. The instrument's noise (buzz, knocks, bursts) goes too.
    body.reset();
    pickups.reset();
    circuit.reset();
    secret.reset();
    playingNoise.reset();
    fretBuzzModel.reset();
    preEffects.reset();
    amp.reset();
    postEffects.reset();
    cabinet.reset();
    room.reset();
    master.reset();
    freezeOverlay.reset();
    coupling.reset();
    bridgeOutputs.fill (0.0);
    couplingInputs.fill (0.0);
    stringOutputs.fill (0.0);
    feedbackLoop.reset();
    ebowDriver.reset();
    scrape.stopAll();
}

bool LuthierEngine::isAudioThreadActive() const noexcept
{
    const auto last = lastProcessMs.load (std::memory_order_relaxed);

    if (last == 0)
        return false;

    const auto since = juce::Time::getMillisecondCounter() - last;

    return since < 200
        && audioThreadId.load (std::memory_order_relaxed) != juce::Thread::getCurrentThreadId();
}

//==============================================================================
void LuthierEngine::triggerNote (const NoteOnEvent& e) noexcept
{
    const int s = juce::jlimit (0, numStrings - 1, e.stringIndex);

    // technique-cascade.md 2 / string-scraping.md 5: a tap on a string being
    // scraped damps the scrape (a 10 ms fade, from the next block).
    if (e.technique == Technique::Tap)
        scrape.preempt (s);

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

    // strum-dynamics 6.1: a chuck is the fretting hand flat on the strings; it
    // is there before the pick, so it overrides the technique's damping. The
    // hand lies across every string, not only the ones this strum crosses: an
    // idle string left open would ring sympathetically off the chucked strikes
    // (the open G at 196 Hz, at -28 dB, was the "pitch" a chuck kept). A note-on
    // without a chuck is the hand moving to a chord, which lifts it off the
    // idle strings it was muting.
    if (e.chuck > 0.0)
        str.setDamping (StringEngine::Damping::Chuck, e.chuck);

    for (int o = 0; o < numStrings; ++o)
    {
        if (o == s)
            continue;

        auto& other = strings[(size_t) o];

        if (e.chuck > 0.0)
            other.setDamping (StringEngine::Damping::Chuck, e.chuck);
        else if (stringMidiNote[(size_t) o] < 0 && other.getDamping() == StringEngine::Damping::Chuck)
            other.setDamping (StringEngine::Damping::Open, 1.0);
    }

    str.setHarmonicRestriction (e.harmonicPartial);

    // ---- build the excitation ---------------------------------------------------
    // strum-dynamics 5: a strum's striker stands in for the pick on this
    // strike only; -1 is the player's own pick.
    const auto material = e.strikerMaterial >= 0
                            ? (Excitation::Material) juce::jlimit (0, (int) Excitation::Material::NumMaterials - 1,
                                                                   e.strikerMaterial)
                            : pickMaterial;

    Excitation::Params p;
    p.material = material;
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
            pickNow.material = material;
            pickNow.fingers = (e.strikerMaterial >= 0 ? false : usingFingers)
                                || ! PlayingNoise::getPickMaterial (material).isPick;
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
    requestedSetup = geometry;

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

    // An E-Bow on explicitly chosen strings keeps them going after the note is
    // released; on "held strings" (mask 0) releasing is exactly what lets go.
    const auto& ebow = ebowDriver.getSettings();
    const bool ebowHolds = ebow.enabled && (ebow.stringMask & (1 << s)) != 0;

    strings[(size_t) s].release (e.letRing || ebowHolds);
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

    // string-scraping.md 2: while the mod wheel or aftertouch sweeps a scrape,
    // it is the pick's, not the vibrato's.
    const double vibratoDepthFromCc = scrape.ownsVibratoControllers() ? 0.0 : midi.getVibratoDepth();

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
            hz = tuning.computeFrequency (s, currentFret[(size_t) s],
                                          bend + whammyCents + vib + magnetDetuneCents + scrape.getPitchOffsetCents (s));
        }

        strings[(size_t) s].setTargetFrequency (hz);
        coupling.setStringFrequency (s, hz);

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

    audioThreadId.store (juce::Thread::getCurrentThreadId(), std::memory_order_relaxed);
    lastProcessMs.store (juce::Time::getMillisecondCounter(), std::memory_order_relaxed);

    // The routing taps and the string-activity stream span the whole host block,
    // however many sub-blocks it takes to render it.
    taps.beginBlock (numSamples);
    stringActivity.clear();
    sidechainReadOffset = 0;
    playingNoise.getPool().beginBlockTriggers();

    // Parked behind a structural change: the message thread owns the engine.
    // Incoming MIDI is kept for the first block after (DECISIONS C-09).
    if (swapState.load (std::memory_order_acquire) == swapParked)
    {
        for (const auto metadata : midiMessages)
            if (parkedMidi.data.size() + metadata.numBytes + 8 <= kParkedMidiBytes)
                parkedMidi.addEvent (metadata.data, metadata.numBytes, 0);

        // Direct notes are not kept: the tune player restarts what is held
        // after a swap. The pointer must not outlive this call either.
        directMidi = nullptr;

        buffer.clear();
        return;
    }

    if (! parkedMidi.isEmpty())
    {
        midiMessages.addEvents (parkedMidi, 0, -1, 0);
        parkedMidi.clear();
    }

    if (numSamples <= maxBlock)
    {
        directForSubBlock = directMidi;
        processSubBlock (buffer, midiMessages);
        directMidi = nullptr;
        directForSubBlock = nullptr;
        applySwapFade (buffer);
        return;
    }

    // The host has handed us a bigger block than it promised in prepareToPlay.
    // Every scratch buffer here is sized from that promise, so writing the whole
    // thing would run off the end. Splitting is the only correct response:
    // allocating would be worse, and truncating would drop audio.
    const int numChannels = buffer.getNumChannels();

    juce::MidiBuffer sliceMidi;
    sliceMidi.ensureSize (2048);
    directSlice.ensureSize (2048);

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

        // The direct notes are sliced the same way.
        directSlice.clear();

        if (directMidi != nullptr)
            for (const auto metadata : *directMidi)
                if (metadata.samplePosition >= offset && metadata.samplePosition < offset + count)
                    directSlice.addEvent (metadata.getMessage(), metadata.samplePosition - offset);

        directForSubBlock = &directSlice;

        // Each slice's taps land after the previous slice's, so the aux buses
        // come out contiguous rather than overwritten.
        taps.setWriteOffset (offset);
        sidechainReadOffset = offset;

        processSubBlock (slice, sliceMidi);
        offset += count;
    }

    directMidi = nullptr;
    directForSubBlock = nullptr;

    applySwapFade (buffer);
}

void LuthierEngine::applySwapFade (juce::AudioBuffer<float>& buffer) noexcept
{
    const int state = swapState.load (std::memory_order_acquire);

    if (state == swapIdle && swapPhase >= 1.0)
        return;

    // Idle with a fade in progress only happens when a change applied without
    // parking (no audio running then); the fade-in finishes regardless.
    const bool out = state == swapFadingOut;
    const double step = 1.0 / juce::jmax (1.0, kSwapFadeSeconds * sr);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        swapPhase = out ? juce::jmax (0.0, swapPhase - step) : juce::jmin (1.0, swapPhase + step);

        // A raised cosine: no corner at either end of the fade.
        const auto gain = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * swapPhase));

        for (int c = 0; c < numChannels; ++c)
            buffer.getWritePointer (c)[i] *= gain;
    }

    if (out && swapPhase <= 0.0)
    {
        // Silent: hand the engine over. The message thread waits for this.
        int expected = swapFadingOut;
        swapState.compare_exchange_strong (expected, swapParked, std::memory_order_acq_rel);
    }
    else if (state == swapFadingIn && swapPhase >= 1.0)
    {
        int expected = swapFadingIn;
        swapState.compare_exchange_strong (expected, swapIdle, std::memory_order_acq_rel);
    }
}

void LuthierEngine::beginStructuralChange()
{
    if (structuralDepth++ > 0)
        return;

    structuralParked = false;

    /*  Only park an audio thread that is actually running, and never the
        caller's own thread: waiting for yourself to render is a deadlock. */
    if (! isAudioThreadActive())
        return;

    swapState.store (swapFadingOut, std::memory_order_release);
    structuralParked = true;

    // Bounded: a stalled host must not hang the UI. 250 ms covers the largest
    // buffer sizes; past it the change goes ahead as it always used to.
    const auto start = juce::Time::getMillisecondCounter();

    while (swapState.load (std::memory_order_acquire) != swapParked
           && juce::Time::getMillisecondCounter() - start < 250)
        juce::Thread::sleep (1);
}

void LuthierEngine::endStructuralChange()
{
    if (--structuralDepth > 0)
        return;

    structuralDepth = 0;

    if (structuralParked)
        swapState.store (swapFadingIn, std::memory_order_release);

    structuralParked = false;
}

void LuthierEngine::setPickupPlacementLive (int slot, double position, double heightMm) noexcept
{
    if (! juce::isPositiveAndBelow (slot, 3))
        return;

    livePickupPosition[(size_t) slot].store (juce::jlimit (0.02, 0.48, position), std::memory_order_relaxed);
    livePickupHeight[(size_t) slot].store (juce::jlimit (0.1, 10.0, heightMm), std::memory_order_relaxed);
    livePickupDirty.fetch_or (1 << slot, std::memory_order_release);

    // With no audio running (tests, the offline renderer) nothing else would apply it.
    const auto since = juce::Time::getMillisecondCounter() - lastProcessMs.load (std::memory_order_relaxed);

    if (lastProcessMs.load (std::memory_order_relaxed) == 0 || since > 200)
        applyLivePickupPlacements();
}

void LuthierEngine::applyLivePickupPlacements() noexcept
{
    const int dirty = livePickupDirty.exchange (0, std::memory_order_acquire);

    for (int slot = 0; slot < 3; ++slot)
    {
        if ((dirty & (1 << slot)) == 0)
            continue;

        partsPickups[(size_t) slot].position = livePickupPosition[(size_t) slot].load (std::memory_order_relaxed);
        partsPickups[(size_t) slot].heightMm = livePickupHeight[(size_t) slot].load (std::memory_order_relaxed);
        pickups.setPickupSpec (slot, partsPickups[(size_t) slot]);
    }
}

void LuthierEngine::processSubBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    if (livePickupDirty.load (std::memory_order_relaxed) != 0)
        applyLivePickupPlacements();

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
    // string-scraping.md 2: the scrape's keyswitches and zone notes are the
    // technique's, taken out before anything that would play them sees them
    // (input-routing.md 5 before 6). Disarmed, this is the same buffer.
    const juce::MidiBuffer& played = scrape.handleMidi (midiMessages, scrapeMidi);

    rhythm.handleMidi (played, samplePosition);

    midi.processBlock (played, numSamples, samplePosition, events);

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

    // Direct notes play as written whether or not the rhythm engine drives.
    if (directForSubBlock != nullptr && ! directForSubBlock->isEmpty())
    {
        midi.processBlock (*directForSubBlock, numSamples, samplePosition, directEvents);
        scheduleEvents (directEvents, numSamples);
    }

    // ---- 1b. string scraping (string-scraping.md 3) --------------------------
    // After the MIDI, before the strings: the block's catches are scheduled
    // here and added to each string's excitation input below. Idle, isBusy()
    // is the whole cost.
    if (scrape.isBusy())
    {
        scrape.setNumStrings (numStrings);
        scrape.setScaleLengthMm (spec.scaleLengthMm);
        scrape.setMuteAmount (technique.getPalmMuteAmount());

        for (int s = 0; s < numStrings; ++s)
        {
            scrape.setString (s, StringNoiseInfo::fromSpec (stringSpecs[(size_t) s], spec.stringMaterial, stringAge),
                              strings[(size_t) s].getCurrentFrequency(), currentFret[(size_t) s],
                              midi.getStringBendCents (s));

            // technique-cascade.md 3.4: the slide holds its strings.
            scrape.setStringBlocked (s, slide.isUnderBar (s));
        }
    }

    scrape.processBlock (numSamples);

    for (int r = 0; r < scrape.getNumRecords(); ++r)
    {
        const auto& rec = scrape.getRecord (r);
        playingNoise.getPool().recordExternalTrigger (NoiseClass::pickScrape, rec.stringIndex,
                                                      sidechainReadOffset + rec.offset,
                                                      samplePosition + rec.offset, rec.level, rec.durationMs);
    }

    // pick-noise.md 5: the rake, from its keyswitch or the Easy-mode gesture.
    {
        bool downward = true;
        double seconds = 0.0;

        if (scrape.takeRake (downward, seconds))
            triggerPickScrape (seconds, downward);
    }

    const bool scrapeOn = scrape.hasOutput();

    updatePerBlockModulation (numSamples);

    // ---- 2. strings, coupling and the magnetic pickup ------------------------
    const bool anyPickupActive = ! pickups.isSilent();

    // ambiguity-resolutions 1: which strings are ringing, and at what, for the
    // feedback loop's per-string peaks. Skipped entirely at amount 0 (1.2).
    const bool feedbackOn = feedbackLoop.isActive();

    if (feedbackOn)
    {
        std::array<double, kMaxStrings> hz {}, levels {};
        std::array<bool, kMaxStrings> wound {};

        for (int s = 0; s < numStrings; ++s)
        {
            hz[(size_t) s] = strings[(size_t) s].getCurrentFrequency();
            levels[(size_t) s] = strings[(size_t) s].getLevel();
            wound[(size_t) s] = stringSpecs[(size_t) s].wound;
        }

        feedbackLoop.beginBlock (hz.data(), levels.data(), wound.data(), numStrings);
    }

    // The E-Bow's strings for this block; the ones it has just let go of are
    // damped, so they stop within 200 ms (2.4) rather than ringing on.
    const bool ebowOn = ebowDriver.getSettings().enabled || ebowDriver.isDrivingAny();

    if (ebowOn)
    {
        std::array<double, kMaxStrings> hz {}, levels {};
        std::array<bool, kMaxStrings> held {}, letGo {};

        for (int s = 0; s < numStrings; ++s)
        {
            hz[(size_t) s] = strings[(size_t) s].getCurrentFrequency();
            levels[(size_t) s] = strings[(size_t) s].getLevel();
            held[(size_t) s] = stringMidiNote[(size_t) s] >= 0;
        }

        ebowDriver.beginBlock (hz.data(), levels.data(), held.data(), numStrings, letGo);

        for (int s = 0; s < numStrings; ++s)
            if (letGo[(size_t) s])
                strings[(size_t) s].setDamping (StringEngine::Damping::Silenced, 1.0);
    }
    const bool perStringTaps = taps.isPerStringWanted() && taps.getRoomAtOffset() >= numSamples;

    playingNoise.getPool().setSamplePosition (samplePosition);
    playingNoise.getPool().setTriggerOffset (sidechainReadOffset);

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
        playingNoise.getPool().setTriggerOffset (sidechainReadOffset + i);
        fireScheduledEvents (samplePosition + i);

        const double fbAmp = feedbackOn ? feedbackLoop.delayedAmp (i) : 0.0;
        double fbSum = 0.0;

        coupling.process (bridgeOutputs.data(), couplingInputs.data());

        noiseBuffer[(size_t) i] = playingNoise.processSample (excitationNoise.data(), surfaceNoise.data(),
                                                              numStrings);

        // pick-noise.md 1.3: Aux 8 carries every generator, the scrape's catches too.
        if (scrapeOn)
            noiseBuffer[(size_t) i] += scrape.getNoiseOutput()[i];

        double sum = 0.0;

        for (int s = 0; s < numStrings; ++s)
        {
            // The click is part of the excitation: it goes into the string.
            double couplingIn = couplingInputs[(size_t) s] + excitationNoise[(size_t) s];

            // string-scraping.md 1: each catch is an impulse into the string at the pick.
            if (scrapeOn)
                couplingIn += scrape.getExcitation (s)[i];

            // Acoustic feedback (ambiguity-resolutions 1): the amp's output,
            // through the air, at this string's own note.
            if (feedbackOn)
            {
                const double fb = feedbackLoop.process (s, fbAmp);
                couplingIn += fb;
                fbSum += fb;
            }

            // The E-Bow (ambiguity-resolutions 2.2): the string's own partial,
            // driven up to its intensity's level and held there.
            if (ebowOn)
                couplingIn += ebowDriver.process (s, stringOutputs[(size_t) s]);

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

        if (i < (int) feedbackInjection.size())
            feedbackInjection[(size_t) i] = fbSum;
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

        // Input gain (3.4): the trim into the rig, after the guitar's own circuit.
        inputGainNow += (inputGainTarget.load (std::memory_order_relaxed) - inputGainNow) * 0.002;
        instrument = sanitise (instrument * inputGainNow);

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

    // The dry side of the wet/dry control is this DI.
    for (int i = 0; i < juce::jmin (numSamples, (int) dryBuffer.size()); ++i)
        dryBuffer[(size_t) i] = instrumentBuffer[(size_t) i];

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

        // What the speaker puts into the room, for the feedback loop's next blocks.
        if (feedbackLoop.isActive())
            feedbackLoop.pushAmpOutput (dl.data(), numSamples);

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

    // ---- 9. (the doubler is a post-amp pedal now: ambiguity-resolutions 3) ------

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

    /*  The tone strip's width and wet/dry (gui-integration.md 3.4), ahead of the
        master bus so its limiter still guards the ceiling whatever the blend
        (DECISIONS). Width is mid/side; dry is the DI, centred. */
    {
        auto* ol = buffer.getWritePointer (0);
        auto* orr = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;
        const double mixTarget = outputMixTarget.load (std::memory_order_relaxed);
        const double wTarget = widthTarget.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            outputMixNow += (mixTarget - outputMixNow) * 0.002;
            widthNow += (wTarget - widthNow) * 0.002;

            double l = ol[i], r = orr != nullptr ? orr[i] : ol[i];
            const double mid = 0.5 * (l + r), side = 0.5 * (l - r) * widthNow;
            l = mid + side;
            r = mid - side;

            const double dry = i < (int) dryBuffer.size() ? dryBuffer[(size_t) i] : 0.0;
            l = l * outputMixNow + dry * (1.0 - outputMixNow);
            r = r * outputMixNow + dry * (1.0 - outputMixNow);

            ol[i] = (float) sanitise (l);
            if (orr != nullptr)
                orr[i] = (float) sanitise (r);
        }
    }

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
