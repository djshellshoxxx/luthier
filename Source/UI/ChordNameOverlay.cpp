#include "ChordNameOverlay.h"
#include "Theme.h"
#include "VisualAids.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "AnimationPolicy.h"

namespace luthier
{

ChordNameOverlay::ChordNameOverlay (LuthierAudioProcessor& p) : processor (p)
{
    lastNotes.fill (-1);
    lastStarts.fill (-1);
}

ChordNaming::Spelling ChordNameOverlay::spelling() const
{
    // A loaded tune's key signature decides; otherwise sharps, but Bb and Eb.
    if (processor.getTunePlayer().hasTimeline())
        return processor.getTuneSession().getTune().preferFlats() ? ChordNaming::Spelling::flats
                                                                  : ChordNaming::Spelling::sharps;

    return ChordNaming::Spelling::sharpsExceptBbEb;
}

void ChordNameOverlay::tick (double nowMs)
{
    fader.setReducedMotion (! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition));   // cpu-quality-modes 6

    if (! VisualAids::showChordNames())
    {
        fader.clear();
        lastNotes.fill (-1);
        lastName.clear();
        return;
    }

    SoundingNotes::Frame frame;

    if (! processor.getSoundingNotes().read (frame))
        return;   // a publish landed mid-copy: the next tick reads it

    // Stale (transport stopped, bypassed): nothing published for 250 ms is silence.
    if (frame.sequence != lastSequence)
    {
        lastSequence = frame.sequence;
        lastPublishMs = nowMs;
    }
    else if (nowMs - lastPublishMs > 250.0)
    {
        frame = SoundingNotes::Frame {};
    }

    const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    observe (nowMs, frame, sr);
}

void ChordNameOverlay::observe (double nowMs, const SoundingNotes::Frame& frame, double sampleRate)
{
    if (! VisualAids::showChordNames())
        return;

    fader.setReducedMotion (! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition));   // cpu-quality-modes 6

    // What changed since the last look: onsets (a start sample moved, or a
    // string began) and the set of notes.
    bool changed = false;
    std::int64_t firstNewOnset = -1, lastNewOnset = -1;
    std::array<int, SoundingNotes::kMaxStrings> notes;
    int count = 0;

    for (int s = 0; s < SoundingNotes::kMaxStrings; ++s)
    {
        const auto& str = frame.strings[(size_t) s];
        const int note = s < frame.numStrings ? str.note : -1;

        if (note >= 0)
        {
            notes[(size_t) count++] = note;

            if (str.startSample != lastStarts[(size_t) s] || lastNotes[(size_t) s] < 0)
            {
                // A new pluck (not a bend or a slide, which keep their start).
                if (str.startSample != lastStarts[(size_t) s])
                {
                    firstNewOnset = firstNewOnset < 0 ? str.startSample : juce::jmin (firstNewOnset, str.startSample);
                    lastNewOnset = juce::jmax (lastNewOnset, str.startSample);
                }
            }

            lastStarts[(size_t) s] = str.startSample;
        }

        if (note != lastNotes[(size_t) s])
            changed = true;

        lastNotes[(size_t) s] = note;
    }

    const bool sounding = count > 0;
    auto onset = ChordNameFader::Onset::none;

    if (firstNewOnset >= 0)
    {
        const auto burst = (std::int64_t) (ChordNameFader::kBurstMs * 0.001 * sampleRate);

        if (chordFirstOnset >= 0 && firstNewOnset - chordFirstOnset <= burst && firstNewOnset >= chordFirstOnset)
        {
            onset = ChordNameFader::Onset::sameStrum;
        }
        else
        {
            onset = ChordNameFader::Onset::newChord;
            chordFirstOnset = firstNewOnset;
        }
    }

    if (sounding && (changed || lastName.isEmpty()))
    {
        lastName = ChordNaming::nameFor (notes.data(), count, spelling());
        ++detections;
    }

    const auto before = fader.getText();
    fader.update (nowMs, sounding, onset, lastName);

    // accessibility: a polite announcement of a new name, rate-limited.
    if (sounding && VisualAids::announceChordNames() && fader.getText() != before
        && fader.getText().isNotEmpty() && nowMs - lastAnnounceMs >= kAnnounceIntervalMs)
    {
        lastAnnouncement = fader.getText();
        lastAnnounceMs = nowMs;
        juce::AccessibilityHandler::postAnnouncement (lastAnnouncement,
                                                      juce::AccessibilityHandler::AnnouncementPriority::low);
    }
}

float ChordNameOverlay::fontHeightFor (float illustrationHeight) noexcept
{
    return juce::jlimit (28.0f, 96.0f, illustrationHeight * 0.12f);
}

juce::Rectangle<float> ChordNameOverlay::lowerBout (juce::Rectangle<float> body, juce::Point<float> nut)
{
    if (body.isEmpty())
        return body;

    // The long axis is whichever the body is longer along; the tail is the
    // end away from the nut.
    if (body.getWidth() >= body.getHeight())
    {
        const float quarter = body.getWidth() * 0.45f;
        return nut.x > body.getCentreX() ? body.withWidth (quarter)
                                          : body.withLeft (body.getRight() - quarter);
    }

    const float quarter = body.getHeight() * 0.45f;
    return nut.y > body.getCentreY() ? body.withHeight (quarter)
                                      : body.withTop (body.getBottom() - quarter);
}

void ChordNameOverlay::paint (juce::Graphics& g, juce::Rectangle<float> boutArea, float illustrationHeight, double nowMs) const
{
    if (boutArea.isEmpty())
        return;

    const auto font = Fonts::display (fontHeightFor (illustrationHeight));

    auto draw = [&] (const juce::String& text, float alpha)
    {
        if (text.isEmpty() || alpha <= 0.0f)
            return;

        g.setColour (Palette::textPrimary.withAlpha (alpha));
        g.setFont (font);
        g.drawFittedText (text, boutArea.toNearestInt(), juce::Justification::centred, 1, 0.6f);
    };

    draw (fader.getOutgoingText (nowMs), fader.getOutgoingOpacity (nowMs));
    draw (fader.getText(), fader.getOpacity (nowMs));
}

} // namespace luthier
