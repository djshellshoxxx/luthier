#pragma once

/*  Slide Mode (slide-guitar.md).

    A slide is a hard tube pressed against the strings instead of a fingertip:
    pitch is continuous, the string is damped on both sides of the contact,
    and the bar rings and clanks. This decides, per string, whether a note is
    under the bar and what that does to it - its contact position with the
    bar's slant, its sustain, the intonation assist, and the clank when the
    bar lands. The string model and the noise pool do the sounding.

    Four modes: bottleneck, lap steel and dobro put every note under the bar;
    hybrid (the electric default, 0.4) puts one string under it and frets the
    others, which is how most electric slide is actually played.
*/

#include "../Noise/NoiseEngine.h"
#include "../Techniques/TechniqueControls.h"
#include "../../Model/Playing/TechniqueTriggers.h"
#include <array>

namespace luthier
{

//==============================================================================
enum class SlideMode { bottleneck = 0, lapSteel, dobro, hybrid, numModes };

/** slide-guitar.md 2.1, in its order. */
enum class SlideMaterial
{
    glass = 0, glassThick, brass, steel, ceramic, bone, dobroBar, numMaterials
};

struct SlideMaterialProperties
{
    const char* name;
    double damping, brightness, friction;
    double clankHz;           ///< clank centre for a 65 g bar
    juce::uint32 colour;      ///< the fretboard overlay's colour (gui-integration 21)
};

const SlideMaterialProperties& getSlideMaterial (SlideMaterial m) noexcept;

/*  The bar: a Workshop part (guitar-workshop.md), mirrored read-only into the
    CHARACTER tab. Until the Workshop exists the engine owns a default one. */
struct SlideBar
{
    SlideMaterial material = SlideMaterial::glass;
    double massGrams = 65.0;
    double lengthMm = 70.0;
    double diameterMm = 22.0;
};

/** The slide parameters (slide-guitar.md 7). */
struct SlideSettings
{
    bool enabled = false;
    SlideMode mode = SlideMode::hybrid;
    double pressure = 0.55;
    double slantDegrees = 0.0;
    double dampingBehind = 0.55;
    double noiseAmount = 0.4;
    double clankAmount = 0.45;
    double intonationAssist = 0.15;
};

//==============================================================================
/*  slide-technique-controls.md 2 (TECHNIQUES): a scripted slide, from A to B
    over T with a curve, the bar's slant and pressure moving with it. */
enum class SlideCurve { linear = 0, easeIn, easeOut, easeInOut, numCurves };

struct SlideGesture
{
    double fromFret = 0.0;
    double toFret = 12.0;
    double durationMs = 500.0;
    SlideCurve curve = SlideCurve::easeInOut;
    double slantStartDegrees = 0.0;
    double slantEndDegrees = 0.0;
    double pressure = 0.7;
};

/** slide-technique-controls.md 1: the user controls. Every field defaults to "as before". */
struct SlideControlSettings
{
    ControlSource positionSource = ControlSource::none;
    int positionCc = 16;
    bool relative = false;               ///< Position mode: absolute 0-1 -> fret 0-24, or relative to the fretted note
    double relativeRangeFrets = 12.0;    ///< how far a full relative throw moves the bar

    ControlSource slantSource = ControlSource::none;
    int slantCc = 17;
    ControlSource pressureSource = ControlSource::none;
    int pressureCc = 18;

    int contactMask = 0;                 ///< strings the bar touches; 0 = all
    double speedLimitCentsPerSecond = 4800.0;

    bool autoVibrato = false;
    double autoVibratoDepthCents = 10.0;
    double autoVibratoRateHz = 5.0;

    bool gestureOnCc = false;            ///< false: keyswitch 21
    int gestureCc = 23;
    SlideGesture gesture;

    TechniqueTriggerConfig triggerConfig (bool slideModeOn) const noexcept;

    static constexpr double kAbsoluteFrets = 24.0;       ///< 1: "position 0-1 maps to fret 0-24"
    static constexpr double kAutoVibratoHoldSeconds = 0.3;
    static constexpr double kSourceCrossfadeSeconds = 0.010;
};

/** The contact-mask presets the SLIDE sub-tab offers (1: all, bass 3, treble 3). */
int slideContactMaskFor (int choice, int numStrings) noexcept;

//==============================================================================
class SlideEngine
{
public:
    static constexpr int kMaxStrings = 12;

    /** String spacing at the bar, mm - the slant offset's lever arm (3.1). */
    static constexpr double kStringSpacingMm = 10.5;

    /** The intonation assist's time constant (4). */
    static constexpr double kAssistSeconds = 0.120;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setSettings (const SlideSettings& s) noexcept { settings = s; }
    const SlideSettings& getSettings() const noexcept { return settings; }
    void setBar (const SlideBar& b) noexcept { bar = b; }
    const SlideBar& getBar() const noexcept { return bar; }

    bool isEnabled() const noexcept { return settings.enabled; }

    //==========================================================================
    /*  A note is starting on string `s`. Decides whether it is under the bar
        (hybrid puts only one string there) and remembers it. Returns true if
        it is. Audio thread. */
    bool noteOn (int s, int numStrings) noexcept;
    void noteOff (int s) noexcept;

    bool isUnderBar (int s) const noexcept;

    /** True if no string was under the bar before this note: the bar is landing. */
    bool isLanding() const noexcept { return landing; }

    //==========================================================================
    /*  The fret the string is actually stopped at: the bar's fret plus the
        slant's offset for this string (3.1). Continuous. */
    double contactFret (int s, double barFret, int numStrings, double scaleLengthMm) const noexcept;

    /*  3.2: slide vibrato moves the bar. `depthMm` of travel at the bar's
        position, as cents - more cents higher up the neck, because the same
        movement is a larger fraction of a shorter string. */
    static double vibratoCents (double depthMm, double barFret, double scaleLengthMm) noexcept;

    /*  4: pulls a string's sounding fret toward the nearest equal-tempered
        fret with a 120 ms time constant, by `intonationAssist`. Called once a
        block per string with the raw (bar) fret; returns the fret to sound. */
    double assist (int s, double rawFret, int numSamples) noexcept;

    /*  3: the sustain multiplier for a string under the bar - the damped
        segment behind the contact and the bar's own absorption, which heavier
        bars have less of. 1 for a string not under it. */
    double sustainScale (int s) const noexcept;

    /** 5.2: the clank for a bar landing at `velocity` on string `s`. */
    NoiseEvent makeClank (int s, double velocity) const noexcept;

    /** 6: a setup under 2.2 mm bass action was not built for slide. */
    static constexpr double kMinimumBassActionMm = 2.2;

    /** gui-integration 14's exact words for that. */
    static const char* const kLowActionMessage;

    /*  A move of the bar along string `s` from one fret to another over
        `seconds`. The bar moves at a steady speed and arrives when the move
        ends - it is a hand, not a filter - so the string is driven from here
        rather than left to its own glide. */
    void startMove (int s, double fromFret, double toFret, double seconds) noexcept;

    /** Where the bar is on string `s` after advancing `numSamples`; `heldFret`
        when no move is in progress. Once a block per string. */
    double advanceBar (int s, double heldFret, int numSamples) noexcept;

    //==========================================================================
    // slide-technique-controls.md 3 (TECHNIQUES): control inputs.

    void setControls (const SlideControlSettings& c) noexcept;
    const SlideControlSettings& getControls() const noexcept { return controls; }

    void setPositionSource (ControlSource source, int cc = 16) noexcept;
    void setSpeedLimit (double centsPerSecond) noexcept { controls.speedLimitCentsPerSecond = juce::jmax (1.0, centsPerSecond); }

    /** 2: starts a scripted slide. Audio thread; requestGesture from any other. */
    void triggerGesture (const SlideGesture& g) noexcept;
    void requestGesture() noexcept { gestureRequested.store (true); }
    bool isGestureRunning() const noexcept { return gestureActive; }

    /** 1: whether the bar touches string `s` (the contact mask). */
    bool contactsString (int s) const noexcept;

    /*  Once a block, before the strings: reads the sources, runs the gesture
        and the speed limit, and applies slant and pressure sources to the
        settings in force. */
    void advanceControls (int numSamples, const TechniqueControls& sources, const TechniqueTriggers& triggers) noexcept;

    /** The bar fret for string `s`, given where the notes put it: the source's or the gesture's when one drives. */
    double controlledBarFret (int s, double noteBarFret) noexcept;

    /** Auto-vibrato on hold, in cents, for this block. */
    double getControlVibratoCents() const noexcept { return controlVibratoCents; }

    /** For the tests: where the controlled bar is now (-1 when no control drives it). */
    double getControlledFret() const noexcept { return controlledFret; }

    /** The current bar fret for the fretboard overlay; -1 when nothing is under it. */
    double getOverlayFret() const noexcept { return overlayFret; }
    void setOverlayFret (double fret) noexcept { overlayFret = fret; }

private:
    SlideSettings settings;
    SlideBar bar;
    double sr = 48000.0;

    std::array<bool, kMaxStrings> underBar {};
    std::array<double, kMaxStrings> assistedFret {};
    std::array<bool, kMaxStrings> assistPrimed {};

    struct Move { double from = 0.0, to = 0.0, elapsed = 0.0, duration = 0.0; bool active = false; };
    std::array<Move, kMaxStrings> moves {};
    int barString = -1;
    bool landing = false;
    double overlayFret = -1.0;

    // --- TECHNIQUES: slide-technique-controls.md -----------------------------
    double advanceTowards (double current, double target, double seconds) const noexcept;

    SlideControlSettings controls;
    ControlSource lastPositionSource = ControlSource::none;
    double controlledFret = -1.0;        ///< absolute mode / gesture: the bar
    double relativeOffset = 0.0;         ///< relative mode: frets added to the note's position
    double smoothedTarget = -1.0;
    bool relativeActive = false;
    bool gestureActive = false;
    SlideGesture runningGesture;
    double gestureElapsed = 0.0;
    std::atomic<bool> gestureRequested { false };
    double lastControlledFret = -1.0, heldSeconds = 0.0, vibratoPhase = 0.0, vibratoRamp = 0.0;
    double controlVibratoCents = 0.0;
    double blockSeconds = 0.0;
};

} // namespace luthier
