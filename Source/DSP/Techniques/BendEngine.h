#pragma once

/*  Microtonal bends (microtonal-bends.md).

    Pitch is continuous per string (0.1). Each string's bend is a stack of
    sources that add (1): the global bend, the string's own (MPE per note by
    default), a pre-bend releasing toward nominal, then vibrato on top. The
    held pitch can be pulled toward a scale grid (3). The slide bar's
    contribution stays in SlideEngine and adds to this in the engine.

    The spec asks for no new module (4): this is the bend source stack the
    string's existing pitch-offset input is fed from, one call per string per
    block - "setBendSourceStack" and "setBendQuantise" are its settings. It
    reads its controllers from TechniqueControls and its pre-bend trigger from
    TechniqueTriggers (keyswitch 20, or a CC).

    Disarmed, the engine uses the interpreter's own bend exactly as before.
    Audio thread apart from the scale and curve setters (double-buffered /
    atomic). Nothing allocates.
*/

#include "TechniqueControls.h"
#include "MicrotonalScale.h"
#include "../../Model/Playing/TechniqueTriggers.h"
#include <array>
#include <atomic>

namespace luthier
{

/** 2, "Bend quantise". Choice index: append only. */
enum class BendQuantise { none = 0, quarterTone, semitone, edo24, edo22, edo31, edo53, custom, numModes };

/** 2, "Bend curve" / "Release curve". */
enum class BendCurve { linear = 0, exponential, drawn, numCurves };

/** 2: the per-string source. */
enum class StringBendSource { none = 0, mpePitchBend, mpeY, customCc, numSources };

/** 2: the vibrato source. */
enum class VibratoSource { off = 0, lfo, aftertouch, mpeZ, numSources };

struct BendSettings
{
    bool armed = false;

    ControlSource globalSource = ControlSource::pitchBend;   ///< pitch bend, expression or a CC
    int globalCc = 20;
    double globalRangeCents = 200.0;

    StringBendSource stringSource = StringBendSource::mpeY;
    int stringCcBase = 21;                                    ///< custom CC: string n uses base + n
    std::array<double, kMaxStrings> stringRangeCents {};      ///< filled with 200 by the constructor

    VibratoSource vibratoSource = VibratoSource::lfo;
    double vibratoRateHz = 6.0;
    double vibratoDepthCents = 20.0;
    double vibratoOnsetMs = 200.0;

    BendQuantise quantise = BendQuantise::none;
    double snap = 1.0;                                        ///< 1 pure snap, 0 none (see coverage decisions)

    double preBendCents = -200.0;
    bool preBendOnCc = false;                                 ///< false: keyswitch 20
    int preBendCc = 22;
    double preBendReleaseMs = 300.0;

    BendCurve bendCurve = BendCurve::linear;
    BendCurve releaseCurve = BendCurve::linear;

    BendSettings() { stringRangeCents.fill (200.0); }

    TechniqueTriggerConfig triggerConfig() const noexcept;
};

//==============================================================================
class BendEngine
{
public:
    static constexpr double kVibratoRampSeconds = 0.030;

    BendEngine();

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setSettings (const BendSettings& s) noexcept { settings = s; }
    const BendSettings& getSettings() const noexcept { return settings; }
    bool isArmed() const noexcept { return settings.armed; }

    //==========================================================================
    /** 3: the custom scale. Message thread. */
    void setCustomScale (const MicrotonalScale& scale) noexcept;
    const MicrotonalScale& getCustomScale() const noexcept { return scales[(size_t) liveScale.load()]; }

    /** 2: the drawn curve, five points over the travel (0, .25, .5, .75, 1). Any thread. */
    void setDrawnCurvePoint (int index, double value) noexcept;
    double getDrawnCurvePoint (int index) const noexcept;
    static constexpr int kCurvePoints = 5;

    //==========================================================================
    /** A note starts on `s` from `channel` (the MPE per-string source's). */
    void noteOn (int s, int channel) noexcept;
    void noteOff (int s) noexcept;

    /** One block: the pre-bend trigger, and time for vibrato and pre-bend releases. */
    void processBlock (int numSamples, const TechniqueTriggers& triggers) noexcept;

    /*  String `s`'s bend in cents for this block. `baseMidiCents` is the
        unbent note (MIDI note x 100), for quantise. Call after processBlock. */
    double centsFor (int s, double baseMidiCents, const TechniqueControls& controls) noexcept;

    /** The shaping curves (for the UI's drawing and the tests). */
    double shapeBend (double x) const noexcept;        ///< -1..1 source -> -1..1
    double shapeRelease (double t) const noexcept;     ///< 0..1 progress -> 1..0 remaining

    /** The grid a quantise mode uses, for the tests: the nearest point to `midiCents`. */
    double quantiseTarget (double midiCents) const noexcept;

    /** Live, for the fretboard's bend arc badge. Any thread. */
    double getLiveCents (int s) const noexcept { return liveCents[(size_t) juce::jlimit (0, kMaxStrings - 1, s)].load (std::memory_order_relaxed); }
    bool isPreBendArmed() const noexcept { return preBendArmed; }

    /** The on-screen pre-bend button. Any thread. */
    void requestPreBend() noexcept { preBendRequested.store (true); }

private:
    double curveValue (BendCurve curve, double x) const noexcept;

    BendSettings settings;
    double sr = 48000.0;

    std::array<MicrotonalScale, 2> scales;
    std::atomic<int> liveScale { 0 };

    std::array<std::atomic<double>, kCurvePoints> curvePoints {};

    struct StringState
    {
        bool sounding = false;
        int channel = 1;
        double globalRange = 200.0, stringRange = 200.0;     // latched at note-on (4)
        double sinceOn = 0.0;
        double preBend = 0.0, preBendElapsed = 0.0;
        double vibPhase = 0.0;
    };

    std::array<StringState, kMaxStrings> strings {};
    std::array<std::atomic<double>, kMaxStrings> liveCents {};
    bool preBendArmed = false;
    std::atomic<bool> preBendRequested { false };
};

} // namespace luthier
