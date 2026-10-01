/*  LuthierEngine's technique-layer glue (engine-technique-layer.md 2-3,
    TECHNIQUES workstream): where the mute, the tap, the bend stack, the slide
    controls and the cascade resolver sit in the block, and what each does to
    the strings. Kept out of LuthierEngine.cpp so that file only carries the
    call sites, each marked with its spec.

    Pipeline (engine-technique-layer 2):
      1   MIDI, TechniqueTriggers                      (LuthierEngine.cpp)
      2b  cascade: activity in, masks out              techniqueBeginBlock
      2c  controls, slide controls, bend, tap, mute    techniqueBeginBlock / techniqueStampEvents
      3   rhythm engine                                (LuthierEngine.cpp; its notes are stamped here)
      5   strings: tap pitch and bend offsets          techniqueFret / techniqueBendCents
*/

#include "LuthierEngine.h"

namespace luthier
{

//==============================================================================
void LuthierEngine::setTapSettings (const TapSettings& s) noexcept
{
    techniqueLayer.tap.setSettings (s);
    techniqueTriggers.configure (TechniqueId::tap, s.triggerConfig());
}

void LuthierEngine::setBendSettings (const BendSettings& s) noexcept
{
    techniqueLayer.bend.setSettings (s);
    techniqueTriggers.configure (TechniqueId::bend, s.triggerConfig());
}

void LuthierEngine::setSlideControls (const SlideControlSettings& c) noexcept
{
    slide.setControls (c);
    techniqueTriggers.configure (TechniqueId::slide, c.triggerConfig (slide.isEnabled()));
}

//==============================================================================
void LuthierEngine::techniqueBeginBlock (int numSamples, const juce::MidiBuffer& played) noexcept
{
    auto& layer = techniqueLayer;
    layer.stages.clear();
    layer.stages.add (TechniqueLayer::stageTriggers);

    // ---- 2b: what each string is doing, into the resolver --------------------------
    for (int s = 0; s < numStrings; ++s)
    {
        layer.cascade.setHeld (CascadeTechnique::slide, s, slide.isUnderBar (s), samplePosition);
        layer.cascade.setHeld (CascadeTechnique::scrape, s, scrape.isStringActive (s), samplePosition);
        layer.cascade.setHeld (CascadeTechnique::tap, s, layer.tap.isTapping (s), samplePosition);
        layer.cascade.setHeld (CascadeTechnique::bend, s, std::abs (layer.bend.getLiveCents (s)) > 1.0, samplePosition);
    }

    layer.stages.add (TechniqueLayer::stageCascade);

    // ---- 2c: the controllers, then the modules ---------------------------------------
    layer.controls.processMidi (played);
    slide.advanceControls (numSamples, layer.controls, techniqueTriggers);
    layer.bend.processBlock (numSamples, techniqueTriggers);

    // muting-rhythm.md 1: a fret mute's finger lets go, block-accurately.
    for (int s = 0; s < numStrings; ++s)
    {
        double t60 = 0.0;

        if (layer.mute.takeDueRelease (s, samplePosition + numSamples, t60))
            strings[(size_t) s].setMutedDamping (t60, 800.0);
    }

    // two-hand-tapping.md 4: the taps.
    {
        std::array<double, kMaxStrings> lhFrets {}, openNotes {};
        std::array<bool, kMaxStrings> lhHeld {}, blocked {};

        for (int s = 0; s < numStrings; ++s)
        {
            lhFrets[(size_t) s] = currentFret[(size_t) s];
            lhHeld[(size_t) s] = stringMidiNote[(size_t) s] >= 0;
            blocked[(size_t) s] = slide.isUnderBar (s);

            const double openHz = tuning.computeFrequency (s, 0.0, 0.0);
            openNotes[(size_t) s] = 69.0 + 12.0 * std::log2 (juce::jmax (1.0, openHz) / 440.0);
        }

        layer.tap.setInstrument (numStrings, openNotes.data(), tuning.getStringTuning (0).maxFrets);
        layer.tap.processBlock (numSamples, techniqueTriggers, lhFrets.data(), lhHeld.data(), blocked.data());

        for (int i = 0; i < layer.tap.getNumEvents(); ++i)
            playTapEvent (layer.tap.getEvent (i));
    }

    layer.cascade.publish (numStrings, samplePosition);
    layer.stages.add (TechniqueLayer::stageTechniques);
}

void LuthierEngine::techniqueStampEvents (PlayEventQueue& queue, bool fromRhythm) noexcept
{
    // muting-rhythm.md 4: every note-on leaves here with its final mute.
    techniqueLayer.mute.apply (queue, sr, tempoBpm, hostPpq, hostPlaying, fromRhythm);

    if (fromRhythm)
        techniqueLayer.stages.add (TechniqueLayer::stageRhythm);
}

//==============================================================================
void LuthierEngine::playTapEvent (const TapEvent& e) noexcept
{
    const int s = juce::jlimit (0, numStrings - 1, e.stringIndex);
    auto& str = strings[(size_t) s];
    auto& layer = techniqueLayer;
    const auto at = samplePosition + e.offset;

    if (e.kind == TapEvent::Kind::tapOn)
    {
        // technique-cascade.md 2-3: tap x scrape conflict, tap x slap alternate.
        const auto outcome = layer.cascade.request (CascadeTechnique::tap, s, at, true);

        if (! outcome.accepted)
            return;

        scrape.preempt (s);
        slap.preempt (s);

        // Nothing fretted under the tap: the fretting hand is at the nut.
        if (stringMidiNote[(size_t) s] < 0 && layer.tap.soundingFret (s, 0.0) >= e.fret - 1.0e-9)
            currentFret[(size_t) s] = 0.0;

        // 2: tap-on. The string now sounds from the tap fret to the bridge,
        // struck by the finger's own momentum.
        str.setDamping (StringEngine::Damping::Open, 1.0);
        str.setHarmonicRestriction (0);
        str.setGlideTime (0.002);
        str.snapToFrequency (tuning.computeFrequency (s, e.fret, midi.getStringBendCents (s)));

        Excitation::Params p;
        p.material = pickMaterial;
        p.kind = Excitation::Kind::Tap;
        p.pluckPosition = pluckPosition;
        p.velocity = juce::jlimit (0.02, 1.0, e.strength);
        p.brightness = attackBrightness;
        p.nailVsFlesh = nailVsFlesh;
        str.excite (p);

        if (! fretless && fretNoise > 0.001)
            str.triggerFretNoise (0.9 * e.strength * fretNoise);

        return;
    }

    // ---- tap-off --------------------------------------------------------------------
    if (e.revealFret < 0.0)
    {
        // Nothing fretted and no flick: the finger lifts and the string stops.
        str.release (false);
        return;
    }

    if (! layer.tap.isTapping (s))
        currentFret[(size_t) s] = e.revealFret;

    str.setGlideTime (0.002);
    str.setTargetFrequency (tuning.computeFrequency (s, layer.tap.soundingFret (s, e.revealFret),
                                                     midi.getStringBendCents (s)));

    // 2: a pull-off flicks the string as the finger leaves it.
    if (e.strength > 0.0)
    {
        Excitation::Params p;
        p.material = pickMaterial;
        p.kind = Excitation::Kind::PullOff;
        p.pluckPosition = pluckPosition;
        p.velocity = juce::jlimit (0.02, 1.0, e.strength);
        p.brightness = attackBrightness;
        p.nailVsFlesh = nailVsFlesh;
        str.excite (p);
    }
}

//==============================================================================
void LuthierEngine::techniqueStrike (const NoteOnEvent& e, int s, bool slapStruck) noexcept
{
    auto& layer = techniqueLayer;
    const auto at = blockStartSample + activeSampleOffset;

    lastStrikeSample[(size_t) s] = at;

    // microtonal-bends.md 4: ranges latch, a pre-bend starts here.
    layer.bend.noteOn (s, e.midiChannel);

    if (slapStruck)
        layer.cascade.request (CascadeTechnique::slap, s, at, true);

    // muting-rhythm.md 4: the strike's mute damps its string.
    const auto damping = layer.mute.dampingFor (e);
    layer.mute.noteStruck (s, damping, at, sr);

    if (e.muteType == 0)
        return;

    layer.cascade.request (CascadeTechnique::mute, s, at, true);

    if (damping.dampsAtStrike())
        strings[(size_t) s].setMutedDamping (damping.t60Seconds, damping.cutoffHz);

    // 3: rock spread - the fretting hand's spare fingers deaden the strings
    // this strike does not play (a string struck in the same strum is not).
    if (layer.mute.deadensOtherStrings (e))
    {
        const auto window = (juce::int64) (0.060 * sr);

        for (int o = 0; o < numStrings; ++o)
        {
            if (o == s || at - lastStrikeSample[(size_t) o] < window)
                continue;

            bool strikeComing = false;

            for (int i = 0; i < numScheduled && ! strikeComing; ++i)
                strikeComing = scheduled[(size_t) i].isNoteOn && scheduled[(size_t) i].noteOn.stringIndex == o
                                 && scheduled[(size_t) i].absoluteSample - at < window;

            if (! strikeComing)
                strings[(size_t) o].setMutedDamping (0.030, 500.0);
        }
    }
}

bool LuthierEngine::techniqueNoteOff (int s) noexcept
{
    techniqueLayer.bend.noteOff (s);
    techniqueLayer.mute.noteReleased (s);

    // two-hand-tapping.md 0.2: the fretting hand lifting under a held tap does
    // not stop the tapped note.
    if (techniqueLayer.tap.isTapping (s))
    {
        techniqueLayer.tap.fretHandReleased (s);
        return true;
    }

    return false;
}

double LuthierEngine::techniqueFret (int s) const noexcept
{
    return techniqueLayer.tap.soundingFret (s, currentFret[(size_t) s]);
}

double LuthierEngine::techniqueBendCents (int s, double interpreterBend) noexcept
{
    // microtonal-bends.md: disarmed, the interpreter's bend exactly as before.
    if (! techniqueLayer.bend.isArmed())
        return interpreterBend;

    const double hz = tuning.computeFrequency (s, techniqueFret (s), 0.0);
    const double base = 6900.0 + 1200.0 * std::log2 (juce::jmax (1.0, hz) / 440.0);
    return techniqueLayer.bend.centsFor (s, base, techniqueLayer.controls);
}

} // namespace luthier
