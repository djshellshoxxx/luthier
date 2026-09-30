#include "TechniqueOverlay.h"
#include "../FretboardComponent.h"
#include "../UiPreferences.h"
#include "../../PluginProcessor.h"
#include "../../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    // The fretboard tap the mouse is holding, released on mouse-up.
    int heldTapString = -1;
    double heldTapFret = 0.0;
    bool dragSlide = false, dragBend = false;

    bool reducedMotion() { return AccessibilitySettings::get().isReducedMotion(); }

    float fadeFor (juce::uint32 since, juce::uint32 lifeMs)
    {
        if (reducedMotion())
            return since < lifeMs ? 1.0f : 0.0f;

        return juce::jlimit (0.0f, 1.0f, 1.0f - (float) since / (float) lifeMs);
    }
}

//==============================================================================
const char* TechniqueOverlay::getLayerName (Layer layer) noexcept
{
    switch (layer)
    {
        case muteZone:    return "Mute zone";
        case scrapeTrail: return "Scrape trail";
        case tapMarkers:  return "Tap markers";
        case bendArc:     return "Bend arc";
        case slapFlash:   return "Slap impact";
        case numLayers:
        default:          return "";
    }
}

juce::String TechniqueOverlay::getPreferenceKey (Layer layer)
{
    return "techniques.overlay." + juce::String (getLayerName (layer)).removeCharacters (" ").toLowerCase();
}

bool TechniqueOverlay::isLayerEnabled (Layer layer)
{
    return UiPreferences::get().getBool (getPreferenceKey (layer), true);
}

void TechniqueOverlay::setLayerEnabled (Layer layer, bool enabled)
{
    UiPreferences::get().setBool (getPreferenceKey (layer), enabled);
}

//==============================================================================
TechniqueOverlay::TechniqueOverlay (LuthierAudioProcessor& p, FretboardComponent& b)
    : processor (p), board (b)
{
    setInterceptsMouseClicks (false, false);
    setAccessible (false);
    startTimerHz (30);
}

TechniqueOverlay::~TechniqueOverlay()
{
    stopTimer();
}

void TechniqueOverlay::refresh()
{
    auto& engine = processor.getEngine();
    const auto now = juce::Time::getMillisecondCounter();
    const double scale = juce::jmax (100.0, engine.getGuitarSpec().scaleLengthMm);

    bool live = false;

    // The scrape's pick, as a fret position along each string.
    for (int s = 0; s < juce::jmin (12, engine.getNumStrings()); ++s)
    {
        const float mm = engine.getScrapeEngine().getUiPositionMm (s);

        if (mm >= 0.0f)
        {
            const float fret = (float) (12.0 * std::log2 (scale / juce::jmax (1.0, (double) mm)));
            auto& head = trailHead[(size_t) s];
            head = (head + 1) % kTrail;
            trails[(size_t) s][(size_t) head] = { fret, now };
            live = true;
        }
    }

    const auto slaps = engine.getSlapEngine().getFiredCount();

    if (slaps != lastSlapCount)
    {
        lastSlapCount = slaps;
        slapFlashAt = now;
    }

    live = live || now - slapFlashAt < 200 || engine.getTechniqueLayer().anyArmed();

    for (int m = 0; m < TapEngine::kMaxMarkers && ! live; ++m)
        live = engine.getTechniqueLayer().tap.getMarker (m).fret >= 0.0f;

    // Repaint only while something is (or was just) on screen: an idle
    // fretboard costs one test per tick.
    if (live || anythingLive)
        repaint();

    anythingLive = live;
}

void TechniqueOverlay::paint (juce::Graphics& g)
{
    const auto start = juce::Time::getHighResolutionTicks();
    drawn.fill (0);

    auto& engine = processor.getEngine();
    const auto& layer = engine.getTechniqueLayer();
    const auto now = juce::Time::getMillisecondCounter();
    const auto area = board.boardArea.toFloat();
    const int strings = juce::jmin (board.numStrings, 12);
    const float laneH = area.getHeight() / (float) juce::jmax (1, strings);

    // ---- 33: mute-zone shading ---------------------------------------------------
    if (isLayerEnabled (muteZone) && layer.mute.getSettings().armed)
    {
        const auto& settings = layer.mute.getSettings();
        const bool palm = (settings.masterMode >= 0 && isPalmMute ((MuteType) settings.masterMode))
                            || isPalmMute ((MuteType) layer.mute.getLastMuteType());

        if (palm)
        {
            // Behind the bridge: the board's end, as wide as the palm reaches (5-100 mm of ~650).
            const float width = area.getWidth() * (float) (settings.palmPositionMm / juce::jmax (100.0, engine.getGuitarSpec().scaleLengthMm)) * 1.5f;
            g.setColour (Palette::secondary.withAlpha (0.18f + 0.2f * (float) settings.palmPressure));
            g.fillRect (area.withLeft (area.getRight() - juce::jmax (6.0f, width)));
            ++drawn[muteZone];
        }
    }

    // ---- 34: scrape trail --------------------------------------------------------
    if (isLayerEnabled (scrapeTrail))
    {
        for (int s = 0; s < strings; ++s)
        {
            const float y = board.stringY (s);
            juce::Point<float> previous;
            bool have = false;

            for (int i = 0; i < kTrail; ++i)
            {
                const auto& p = trails[(size_t) s][(size_t) ((trailHead[(size_t) s] + 1 + i) % kTrail)];
                const float alpha = p.fret >= 0.0f ? fadeFor (now - p.at, 400) : 0.0f;

                if (alpha <= 0.0f)
                {
                    have = false;
                    continue;
                }

                const juce::Point<float> here (board.fretX (juce::jmin ((double) p.fret, (double) board.numFrets)), y);
                g.setColour (Palette::accentBright.withAlpha (alpha));

                if (have)
                    g.drawLine ({ previous, here }, 2.0f);

                // A tick per catch.
                g.drawVerticalLine ((int) here.x, y - 3.0f, y + 3.0f);
                previous = here;
                have = true;
                ++drawn[scrapeTrail];
            }
        }
    }

    // ---- 35: tap markers ---------------------------------------------------------
    if (isLayerEnabled (tapMarkers))
    {
        for (int m = 0; m < TapEngine::kMaxMarkers; ++m)
        {
            const auto marker = layer.tap.getMarker (m);

            if (marker.fret < 0.0f || ! juce::isPositiveAndBelow (marker.string, strings))
                continue;

            const float alpha = marker.held ? 1.0f : fadeFor (now - marker.releasedMs, 100);

            if (alpha <= 0.0f)
                continue;

            const float x = board.fretX ((double) marker.fret - 0.5);
            const float size = juce::jmin (12.0f, laneH * 0.7f);
            const auto square = juce::Rectangle<float> (size, size).withCentre ({ x, board.stringY (marker.string) });

            g.setColour (Palette::warning.withAlpha (alpha));
            g.fillRect (square);
            g.setColour (Palette::backgroundDeep.withAlpha (alpha));
            g.drawRect (square, 1.0f);
            ++drawn[tapMarkers];
        }
    }

    // ---- 36: bend arc with its cents badge ---------------------------------------
    if (isLayerEnabled (bendArc))
    {
        for (int s = 0; s < strings; ++s)
        {
            const double cents = layer.bend.getLiveCents (s);

            if (std::abs (cents) < 3.0 || board.liveLevel[(size_t) s] < 0.002)
                continue;

            const double fret = board.liveFret[(size_t) s];
            const float x0 = board.fretX (juce::jmax (0.0, fret - 0.5));
            const float x1 = board.fretX (juce::jlimit (0.0, (double) board.numFrets, fret - 0.5 + cents / 100.0));
            const float y = board.stringY (s);
            const float lift = juce::jlimit (4.0f, laneH * 0.45f, std::abs (x1 - x0) * 0.4f);

            juce::Path arc;
            arc.startNewSubPath (x0, y);
            arc.quadraticTo ((x0 + x1) * 0.5f, y - lift, x1, y);
            g.setColour (Palette::accentBright);
            g.strokePath (arc, juce::PathStrokeType (1.5f));

            const juce::String badge = (cents > 0 ? "+" : "") + juce::String (juce::roundToInt (cents)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xa2"));
            g.setFont (Fonts::ui (9.0f, true));
            const auto box = juce::Rectangle<float> (30.0f, 11.0f).withCentre ({ x1, y - lift - 4.0f });
            g.setColour (Palette::backgroundDeep.withAlpha (0.8f));
            g.fillRoundedRectangle (box, 2.0f);
            g.setColour (Palette::accentBright);
            g.drawText (badge, box, juce::Justification::centred, false);
            ++drawn[bendArc];
        }
    }

    // ---- 37: slap impact ----------------------------------------------------------
    if (isLayerEnabled (slapFlash) && slapFlashAt != 0)
    {
        const float alpha = fadeFor (now - slapFlashAt, 150);

        if (alpha > 0.0f)
        {
            g.setColour (Palette::accentBright.withAlpha (0.5f * alpha));
            g.fillRect (area.withLeft (area.getRight() - 10.0f));
            ++drawn[slapFlash];
        }
    }

    lastPaintMs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) * 1000.0;
}

//==============================================================================
bool TechniqueOverlay::handleMouseDown (LuthierAudioProcessor& p, int string, double continuousFret, int numFrets)
{
    auto& engine = p.getEngine();
    auto& layer = engine.getTechniqueLayer();

    dragSlide = engine.getSlideEngine().getControls().positionSource == ControlSource::fretboard
                  && engine.getSlideEngine().isEnabled();
    dragBend = layer.bend.isArmed();

    if (dragSlide)
        layer.controls.setFretboardSlidePosition (juce::jlimit (0.0, 1.0, continuousFret / SlideControlSettings::kAbsoluteFrets));

    // two-hand-tapping.md 3: the fretboard tap layer.
    if (layer.tap.acceptsFretboardTaps())
    {
        TapGesture g;
        g.stringIndex = string;
        g.fret = juce::jlimit (1.0, (double) numFrets, continuousFret);
        g.strength = 0.8;
        g.durationMs = 0.0;   // held while the mouse is
        layer.tap.requestGesture (g);

        heldTapString = string;
        heldTapFret = layer.tap.getSettings().fretSnap ? std::round (g.fret) : g.fret;
        return true;
    }

    return false;
}

void TechniqueOverlay::handleMouseDrag (LuthierAudioProcessor& p, double continuousFret, float dragPixelsUp, int numFrets)
{
    auto& layer = p.getEngine().getTechniqueLayer();
    juce::ignoreUnused (numFrets);

    if (dragSlide)
        layer.controls.setFretboardSlidePosition (juce::jlimit (0.0, 1.0, continuousFret / SlideControlSettings::kAbsoluteFrets));

    // A push across the strings bends: 40 pixels is a full throw.
    if (dragBend)
        layer.controls.setFretboardBend (juce::jlimit (-1.0, 1.0, (double) dragPixelsUp / 40.0));
}

void TechniqueOverlay::handleMouseUp (LuthierAudioProcessor& p)
{
    auto& layer = p.getEngine().getTechniqueLayer();

    if (heldTapString >= 0)
        layer.tap.requestRelease (heldTapString, heldTapFret);

    heldTapString = -1;

    if (dragSlide)
        layer.controls.setFretboardSlidePosition (-1.0);

    if (dragBend)
        layer.controls.setFretboardBend (0.0);

    dragSlide = dragBend = false;
}

} // namespace luthier
