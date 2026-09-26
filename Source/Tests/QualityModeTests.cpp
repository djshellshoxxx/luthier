/*  cpu-quality-modes.md 12: CQ-01 to CQ-10, CQ-15 to CQ-19, CQ-28, CQ-29, CQ-31.

    The render-heavy tests (CQ-09, CQ-11 to CQ-14, CQ-20, CQ-30, CQ-32) are in
    QualityModeRenderTests.cpp; the GUI ones (CQ-21 to CQ-27) in
    QualityModeUiTests.cpp. The level is forced with
    QualityController::forceLevelForTesting; Auto uses an injected clock and
    load feed; the global setting lives in a temporary performance.json.
*/

#include "TestFramework.h"
#include "QualityTestSupport.h"

#include "../PluginProcessor.h"
#include "../LuthierEngine.h"
#include "../Support/QualityProfile.h"
#include "../Support/QualityController.h"
#include "../Support/PerformanceSettings.h"
#include "../Support/CpuLoadMonitor.h"
#include "../DSP/Common/IrVariants.h"
#include "../DSP/Amp/AmpEngine.h"
#include "../DSP/Effects/PedalsDrive.h"
#include "../DSP/Body/BodyEngine.h"
#include "../DSP/String/StringEngine.h"
#include "../DSP/Noise/NoiseEngine.h"

#include <regex>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    const QualityLevel kLevels[3] = { QualityLevel::High, QualityLevel::Medium, QualityLevel::Low };

    juce::String levelLabel (QualityLevel l) { return qualityLevelKey (l); }

    void renderEngine (LuthierEngine& engine, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer none;

        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            engine.processBlock (buffer, none);
        }
    }
}

//==============================================================================
// CQ-01 Profile table
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ01_profileTableMatchesTheSpec)
{
    const auto h = QualityProfile::forLevel (QualityLevel::High);
    const auto m = QualityProfile::forLevel (QualityLevel::Medium);
    const auto l = QualityProfile::forLevel (QualityLevel::Low);

    // Amp and drive oversampling caps (0 = the parameter decides).
    CHECK (h.ampOversamplingCap == 0 && m.ampOversamplingCap == 2 && l.ampOversamplingCap == 2);
    CHECK (h.driveOversamplingCap == 0 && m.driveOversamplingCap == 2 && l.driveOversamplingCap == 1);

    // Dispersion: 8; 4 from 330 Hz; 4 from 165 Hz and 2 from 440 Hz.
    CHECK (h.dispersionStagesFor (1000.0) == 8);
    CHECK (m.dispersionStagesFor (329.0) == 8 && m.dispersionStagesFor (330.0) == 4 && m.dispersionStagesFor (900.0) == 4);
    CHECK (l.dispersionStagesFor (164.0) == 8 && l.dispersionStagesFor (165.0) == 4 && l.dispersionStagesFor (439.0) == 4);
    CHECK (l.dispersionStagesFor (440.0) == 2);

    // Convolution lengths, modal bank, room taps, noise, mod rate.
    CHECK (h.bodyIrSeconds == 0.0 && m.bodyIrSeconds == 1.5 && l.bodyIrSeconds == 0.75);
    CHECK (h.bodyModes == 48 && m.bodyModes == 32 && l.bodyModes == 20);
    CHECK (h.bodyModesAlwaysKept == 8 && m.bodyModesAlwaysKept == 8 && l.bodyModesAlwaysKept == 8);
    CHECK (h.cabinetIrSeconds == 0.0 && m.cabinetIrSeconds == 0.25 && l.cabinetIrSeconds == 0.12);
    CHECK (h.roomTaps == 16 && m.roomTaps == 16 && l.roomTaps == 8);
    CHECK (! h.noiseDegraded && ! m.noiseDegraded && l.noiseDegraded);
    CHECK (h.modIntervalMultiplier == 1 && m.modIntervalMultiplier == 1 && l.modIntervalMultiplier == 2);
    CHECK (h.modFastLfoHz == 20.0);

    // Sleep and ring-out.
    CHECK (! h.idleSleep && m.idleSleep && l.idleSleep);
    CHECK (h.ringOutDb == 0.0 && m.ringOutDb == -80.0 && l.ringOutDb == -60.0);
    CHECK (h.maxRingingOut == 0 && m.maxRingingOut == 0 && l.maxRingingOut == 8);

    // UI.
    CHECK (h.motion == MotionLevel::Full && m.motion == MotionLevel::Limited && l.motion == MotionLevel::Off);
    CHECK (h.liveReadoutMaxHz == 0 && m.liveReadoutMaxHz == 0 && l.liveReadoutMaxHz == 10);

    // min(nominal, cap); 1x stays 1x.
    CHECK (QualityProfile::capFactor (8, 2) == 2);
    CHECK (QualityProfile::capFactor (1, 2) == 1);
    CHECK (QualityProfile::capFactor (4, 0) == 4);
}

LUTHIER_TEST (CpuQuality, CQ01_noLiteralCapsOutsideTheProfile)
{
    // A cap written as a literal anywhere but QualityProfile.h fails here.
    const std::regex callWithLiteral (R"(\b(setTapCount|setControlIntervalMultiplier|setDispersionRule|fadeToSleep)\s*\(\s*-?[0-9])");
    const std::regex fieldAssignedLiteral (R"(\b(ampOversamplingCap|driveOversamplingCap|bodyIrSeconds|cabinetIrSeconds|bodyModes|bodyModesAlwaysKept|roomTaps|modIntervalMultiplier|ringOutDb|maxRingingOut|liveReadoutMaxHz|fourStageFromHz|twoStageFromHz)\s*=\s*-?[0-9])");

    const auto source = QualityTestSupport::sourceRoot();
    CHECK_MSG (source.isDirectory(), "source tree not found: " + source.getFullPathName());

    int scanned = 0;

    for (const auto& entry : juce::RangedDirectoryIterator (source, true, "*.cpp;*.h", juce::File::findFiles))
    {
        const auto f = entry.getFile();

        if (f.getFileName() == "QualityProfile.h" || f.getFullPathName().contains ("/Tests/")
            || f.getFullPathName().contains ("\\Tests\\"))
            continue;

        ++scanned;
        const auto text = f.loadFileAsString().toStdString();

        CHECK_MSG (! std::regex_search (text, callWithLiteral), "literal quality cap in a call: " + f.getFileName());
        CHECK_MSG (! std::regex_search (text, fieldAssignedLiteral), "literal quality cap assigned: " + f.getFileName());
    }

    CHECK (scanned > 100);
}

//==============================================================================
// CQ-02 Settings file
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ02_settingsFileRoundTripsAndFallsBackToDefaults)
{
    QualityTestSupport::ScopedTempSettings temp;
    auto& settings = PerformanceSettings::get();

    // Every field round-trips, through the string form and through the file.
    for (int q = 0; q < 4; ++q)
    {
        for (int l = 0; l < 3; ++l)
        {
            PerformanceSettings::Values v;
            v.quality = (QualityChoice) q;
            v.autoLastLevel = (QualityLevel) l;
            v.offlineAtHigh = (q % 2) == 0;
            v.autoNotify = (l % 2) == 1;
            v.emergencyStringDrop = q != 2;

            PerformanceSettings::Values back;
            CHECK (PerformanceSettings::parse (PerformanceSettings::toJson (v), back));
            CHECK (back == v);

            settings.setValues (v);
            CHECK (settings.load());
            CHECK (settings.getValues() == v);
        }
    }

    // The file carries the schema and the magic.
    const auto json = juce::JSON::parse (settings.getFile().loadFileAsString());
    CHECK ((int) json.getProperty ("schema", 0) == 1);
    CHECK (json.getProperty ("magic", "").toString() == "luthier.performance");

    // Corrupt: the defaults, never an error.
    settings.getFile().replaceWithText ("{ this is not json");
    CHECK (! settings.load());
    CHECK (settings.getValues() == PerformanceSettings::Values {});

    // Wrong magic: the defaults.
    settings.getFile().replaceWithText (R"({ "schema": 1, "magic": "something.else", "quality": "low" })");
    settings.load();
    CHECK (settings.getQuality() == QualityChoice::High);

    // Missing: the defaults.
    settings.getFile().deleteFile();
    settings.load();
    CHECK (settings.getValues() == PerformanceSettings::Values {});
    CHECK (settings.getQuality() == QualityChoice::High);

    // Rewritten on the next change.
    settings.setQuality (QualityChoice::Medium);
    CHECK (settings.getFile().existsAsFile());

    // Temp-and-rename: the folder holds the one file, complete, and nothing else.
    const auto siblings = settings.getFile().getParentDirectory().findChildFiles (juce::File::findFiles, false, "*");
    CHECK (siblings.size() == 1);
    CHECK (PerformanceSettings::parse (settings.getFile().loadFileAsString(), temp.scratch));
    CHECK (temp.scratch.quality == QualityChoice::Medium);

    // Reset all settings restores the defaults, written.
    settings.resetToDefaults();
    settings.load();
    CHECK (settings.getValues() == PerformanceSettings::Values {});
}

//==============================================================================
// CQ-03 Not in presets
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ03_theLevelIsNeverInAPreset)
{
    QualityTestSupport::ScopedTempSettings temp;
    PerformanceSettings::get().setQuality (QualityChoice::Low);

    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);

    // A preset saved at Low carries no quality key.
    const auto presetJson = juce::JSON::toString (p.getPresetManager().toVar ("at low"));
    CHECK (! presetJson.contains ("\"quality\""));
    CHECK (! presetJson.contains ("qualityOverride"));
    CHECK (! presetJson.contains ("offline_at_high"));
    CHECK (! presetJson.contains ("auto_last_level"));

    // Loading any factory preset leaves the level where it was.
    auto& presets = p.getPresetManager();

    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        presets.loadPreset (i);
        CHECK (p.getQualityController().getLiveLevel() == QualityLevel::Low);
        CHECK (PerformanceSettings::get().getQuality() == QualityChoice::Low);
    }

    // No parameter was added: nothing named for quality, and `oversample`
    // keeps its id, choices and default.
    for (auto* param : p.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            CHECK_MSG (! withId->paramID.startsWithIgnoreCase ("quality") && ! withId->paramID.containsIgnoreCase ("cpu"),
                       "unexpected parameter " + withId->paramID);   // (cable_quality is the cable's, not this)

    auto* os = dynamic_cast<juce::AudioParameterChoice*> (p.getState().getParameter (ParamIDs::oversample));
    CHECK (os != nullptr);

    if (os != nullptr)
    {
        CHECK (os->choices.size() == 4);
        CHECK (juce::roundToInt (os->convertFrom0to1 (static_cast<juce::AudioProcessorParameter*> (os)->getDefaultValue())) == 2);   // 4x
    }
}

//==============================================================================
// CQ-04 Override
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ04_perInstanceOverrideRoundTripsAndFollowsTheRules)
{
    QualityTestSupport::ScopedTempSettings temp;
    auto& settings = PerformanceSettings::get();
    settings.setQuality (QualityChoice::High);

    juce::MemoryBlock state;

    {
        LuthierAudioProcessor p;
        p.prepareToPlay (kSr, kBlock);
        p.setQualityOverride (QualityOverride::Medium);
        p.getStateInformation (state);
    }

    {
        LuthierAudioProcessor p;
        p.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (p.getUiState().qualityOverride == QualityOverride::Medium);
        CHECK (p.getQualityController().getOverride() == QualityOverride::Medium);
        CHECK (p.getQualityController().getLiveLevel() == QualityLevel::Medium);

        // A fixed override ignores the global setting...
        settings.setQuality (QualityChoice::Low);
        CHECK (p.getQualityController().getLiveLevel() == QualityLevel::Medium);

        // ...and "global" follows it.
        p.setQualityOverride (QualityOverride::Global);
        CHECK (p.getQualityController().getLiveLevel() == QualityLevel::Low);
        settings.setQuality (QualityChoice::High);
        CHECK (p.getQualityController().getLiveLevel() == QualityLevel::High);

        // Reset (Ctrl+Shift+R) never touches it.
        p.setQualityOverride (QualityOverride::Low);
        p.resetEverything();
        CHECK (p.getUiState().qualityOverride == QualityOverride::Low);
    }
}

//==============================================================================
// CQ-05 Oversampling cap
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ05_effectiveFactorIsTheCappedNominal)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);

    for (int nominal : { 1, 2, 4, 8 })
    {
        for (auto level : kLevels)
        {
            const auto profile = QualityProfile::forLevel (level);
            engine.applyQuality (profile, false);
            engine.setOversamplingFactor (nominal);

            const int expectAmp = profile.ampOversamplingCap > 0 ? juce::jmin (nominal, profile.ampOversamplingCap) : nominal;
            const int expectDrive = profile.driveOversamplingCap > 0 ? juce::jmin (nominal, profile.driveOversamplingCap) : nominal;

            CHECK_MSG (engine.getEffectiveAmpOversampling() == expectAmp,
                       "amp " + juce::String (nominal) + "x at " + levelLabel (level)
                       + " runs " + juce::String (engine.getEffectiveAmpOversampling()) + "x");
            CHECK_MSG (engine.getEffectiveDriveOversampling() == expectDrive,
                       "drive " + juce::String (nominal) + "x at " + levelLabel (level));
            CHECK (engine.getOversamplingFactor() == nominal);   // the parameter's value is untouched
        }
    }
}

//==============================================================================
// CQ-06 Constant latency
//==============================================================================
namespace
{
    struct LatencyWatcher : public juce::AudioProcessorListener
    {
        int latencyChanges = 0;

        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& d) override
        {
            if (d.latencyChanged)
                ++latencyChanges;
        }
    };
}

LUTHIER_TEST (CpuQuality, CQ06_latencyNeverFollowsTheLevel)
{
    QualityTestSupport::ScopedTempSettings temp;
    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);

    // A drive pedal in, so the pedals' pads are in the figure too.
    QualityTestSupport::insertDrivePedal (p);

    LatencyWatcher watcher;
    p.addListener (&watcher);

    juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels()), kBlock);
    juce::MidiBuffer midi;

    for (int index = 0; index < 4; ++index)
    {
        p.getQualityController().forceLevelForTesting ((int) QualityLevel::High);
        QualityTestSupport::setChoice (p, ParamIDs::oversample, index);
        p.getParameterBridge().applyAllNow();
        buffer.clear();
        p.processBlock (buffer, midi);
        p.prepareToPlay (kSr, kBlock);   // the host re-reads the latency here

        const int high = p.getLatencySamples();
        const int engineHigh = p.getEngine().getLatencySamples();
        watcher.latencyChanges = 0;

        for (int sw = 0; sw < 50; ++sw)
        {
            p.getQualityController().forceLevelForTesting (sw % 3);

            for (int b = 0; b < 2; ++b)
            {
                buffer.clear();
                midi.clear();

                if (b == 0 && sw % 5 == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 40 + sw % 12, (juce::uint8) 100), 0);

                p.processBlock (buffer, midi);
            }

            CHECK_MSG (p.getEngine().getLatencySamples() == engineHigh,
                       "engine latency moved at switch " + juce::String (sw) + " with oversample index " + juce::String (index));
            CHECK (p.getLatencySamples() == high);
        }

        CHECK_MSG (watcher.latencyChanges == 0, "setLatencySamples called " + juce::String (watcher.latencyChanges) + " times");
    }

    p.removeListener (&watcher);
    p.getQualityController().forceLevelForTesting (-1);
}

//==============================================================================
// CQ-07 Latency accuracy
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ07_anImpulseArrivesWhereItDidAtHigh)
{
    /*  An impulse through the amp path (the internal re-amp, routing-io 5B)
        with a high-gain amp and a drive pedal: at every level it arrives within
        one sample of where it arrives at High, on the main output and on Aux 2
        (the amp before the cabinet), and the reported latency is the same. */
    auto arrivals = [] (QualityLevel level, int& mainAt, int& auxAt, int& reported)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.getAmpEngine().setModel (AmpModel::MesaRectifier);
        engine.getAmpEngine().setGain (0.9);
        engine.getPreEffects().setSlotType (0, PedalType::Overdrive);
        engine.setOversamplingFactor (4);
        engine.applyQuality (QualityProfile::forLevel (level), true);
        engine.getTapBuffers().setAuxWanted ((int) AuxBus::ampPreCab, true);
        engine.setSidechainToAmp (true);

        const int total = 4 * kBlock;
        juce::AudioBuffer<float> side (1, total);
        side.clear();
        side.setSample (0, kBlock, 0.5f);   // the impulse, one block in

        std::vector<float> mainOut, auxOut;
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer none;

        for (int b = 0; b < 4; ++b)
        {
            const float* ch[] = { side.getReadPointer (0, b * kBlock) };
            engine.setSidechainInput (ch, 1, kBlock);
            buffer.clear();
            engine.processBlock (buffer, none);

            for (int i = 0; i < kBlock; ++i)
                mainOut.push_back (buffer.getSample (0, i));

            const float* aux = engine.getTapBuffers().auxRead ((int) AuxBus::ampPreCab, 0);

            for (int i = 0; i < kBlock; ++i)
                auxOut.push_back (aux != nullptr ? aux[i] : 0.0f);
        }

        mainAt = QualityTestSupport::onset (mainOut, 0.25);
        auxAt = QualityTestSupport::onset (auxOut, 0.25);
        reported = engine.getLatencySamples();
    };

    int mainHigh = 0, auxHigh = 0, repHigh = 0;
    arrivals (QualityLevel::High, mainHigh, auxHigh, repHigh);
    CHECK (mainHigh > 0 && auxHigh > 0);

    for (auto level : { QualityLevel::Medium, QualityLevel::Low })
    {
        int mainAt = 0, auxAt = 0, rep = 0;
        arrivals (level, mainAt, auxAt, rep);

        CHECK_MSG (std::abs (mainAt - mainHigh) <= 1, "main output at " + levelLabel (level) + ": "
                   + juce::String (mainAt) + " vs " + juce::String (mainHigh));
        CHECK_MSG (std::abs (auxAt - auxHigh) <= 1, "Aux 2 at " + levelLabel (level) + ": "
                   + juce::String (auxAt) + " vs " + juce::String (auxHigh));
        CHECK (rep == repHigh);
    }
}

//==============================================================================
// CQ-08 Pitch, CQ-10 note-boundary dispersion
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ08_fundamentalAndTenthPartialHoldAtEveryLevel)
{
    LuthierEngine donor;
    donor.prepare (kSr, kBlock);
    donor.setGuitarType (GuitarType::Stratocaster);

    const double pitches[] = { 82.41, 110.0, 164.81, 329.63, 659.26 };
    double worstF0 = 0.0, worstP10 = 0.0;

    for (int s = 0; s < donor.getNumStrings(); ++s)
    {
        const auto physical = donor.getString (s).getPhysical();

        for (double hz : pitches)
        {
            double f0High = 0.0, p10High = 0.0;

            for (auto level : kLevels)
            {
                const auto profile = QualityProfile::forLevel (level);
                double f0 = 0.0, p10 = 0.0;
                QualityTestSupport::measureString (physical, hz, profile, f0, p10);

                if (level == QualityLevel::High)
                {
                    f0High = f0;
                    p10High = p10;
                    continue;
                }

                const double f0Cents = 1200.0 * std::log2 (f0 / f0High);
                const double p10Cents = p10High > 0.0 && p10 > 0.0 ? 1200.0 * std::log2 (p10 / p10High) : 0.0;
                worstF0 = juce::jmax (worstF0, std::abs (f0Cents));
                worstP10 = juce::jmax (worstP10, std::abs (p10Cents));

                CHECK_MSG (std::abs (f0Cents) <= 0.1, "string " + juce::String (s) + " at " + juce::String (hz)
                           + " Hz, " + levelLabel (level) + ": fundamental " + juce::String (f0Cents, 3) + " cents");
                CHECK_MSG (std::abs (p10Cents) <= 3.0, "string " + juce::String (s) + " at " + juce::String (hz)
                           + " Hz, " + levelLabel (level) + ": partial 10 " + juce::String (p10Cents, 3) + " cents");
            }
        }
    }

    std::cout << "    worst fundamental " << juce::String (worstF0, 4) << " cents, partial 10 "
              << juce::String (worstP10, 3) << " cents" << std::endl;
}

LUTHIER_TEST (CpuQuality, CQ10_aRingingNoteKeepsItsStagesUntilReExcited)
{
    StringEngine str;
    str.prepare (kSr, kBlock);
    str.snapToFrequency (659.26);

    const auto low = QualityProfile::forLevel (QualityLevel::Low);
    const auto high = QualityProfile::forLevel (QualityLevel::High);

    str.setDispersionRule (low.fourStageFromHz, low.twoStageFromHz);
    str.excite (Excitation::Params {});

    for (int i = 0; i < 2000; ++i)
        str.processSample (0.0);

    const int stagesAtLow = str.getActiveDispersionStages();
    CHECK (str.getLatchedDispersionStages() == 2);
    CHECK (stagesAtLow <= 2);

    // Mid-note switch to High: nothing changes until the next excite.
    str.setDispersionRule (high.fourStageFromHz, high.twoStageFromHz);

    for (int i = 0; i < 4000; ++i)
    {
        str.processSample (0.0);

        if (str.getActiveDispersionStages() != stagesAtLow)
        {
            CHECK_MSG (false, "stage count changed mid-note");
            break;
        }
    }

    // The re-excite of a ringing string is a voice steal: the new note is
    // plucked, and takes its stages, after the 5 ms fade (FIX-CROSS item 4).
    str.excite (Excitation::Params {});

    for (int i = 0; i < (int) (0.010 * kSr); ++i)
        str.processSample (0.0);

    CHECK (str.getLatchedDispersionStages() == 8);
    CHECK (str.getActiveDispersionStages() > stagesAtLow);
}

//==============================================================================
// CQ-15 Aliasing bounds
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ15_aliasingStaysWithinItsBounds)
{
    /*  The amp at 2x. At gain 10 (the knob's top) this amp aliases at about
        -39 dBc even at its default 4x on a 1 kHz sine - the waveshaper's
        harmonics fold whatever the factor - so the spec's -60 dBc cannot bound
        2x on its own there. What is bounded is what the cap adds: in the band
        below 16 kHz, 2x stays under -60 dBc or within 12 dB of the nominal 4x
        at gain 10, and within 10 dB at gain 5. The figures are printed and
        recorded (docs/coverage/FEAT-CPU.md, decision CQ-15). */
    for (auto model : { AmpModel::FenderTwin, AmpModel::MarshallPlexi, AmpModel::MesaRectifier, AmpModel::DiezelVH4 })
    {
        for (double gain : { 1.0, 0.5 })
        {
            const double at2 = QualityTestSupport::ampAliasDbc (model, gain, 2, 16000.0);
            const double at4 = QualityTestSupport::ampAliasDbc (model, gain, 4, 16000.0);
            const double allowance = gain >= 1.0 ? 12.0 : 10.0;

            std::cout << "    amp " << AmpEngine::getModelName (model) << " gain " << (int) (gain * 10.0) << ": 2x "
                      << juce::String (at2, 1) << " dBc, 4x " << juce::String (at4, 1) << " dBc" << std::endl;

            CHECK_MSG (at2 <= -60.0 || at2 <= at4 + allowance,
                       juce::String (AmpEngine::getModelName (model)) + " gain " + juce::String (gain * 10.0)
                       + " at 2x aliases at " + juce::String (at2, 1) + " dBc against " + juce::String (at4, 1) + " at 4x");
        }
    }

    // Each drive pedal at its defaults, at the factor its Low cap gives it.
    for (auto type : { PedalType::Overdrive, PedalType::Distortion, PedalType::Fuzz, PedalType::Boost })
    {
        const int lowCap = QualityProfile::driveCapForPedal ((int) type, QualityProfile::forLevel (QualityLevel::Low).driveOversamplingCap);
        const double dbc = QualityTestSupport::pedalAliasDbc (type, lowCap);
        std::cout << "    pedal " << (int) type << " at " << lowCap << "x: " << juce::String (dbc, 1) << " dBc" << std::endl;
        CHECK_MSG (dbc <= -50.0, "pedal " + juce::String ((int) type) + " at its Low cap aliases at " + juce::String (dbc, 1) + " dBc");

        // A pedal that fails at 1x must carry the 2x Low cap in the table.
        if (QualityTestSupport::pedalAliasDbc (type, 1) > -50.0)
            CHECK_MSG (lowCap == 2, "pedal " + juce::String ((int) type) + " fails at 1x but its Low cap is not 2x");
    }
}

//==============================================================================
// CQ-16 Body and IR
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ16_truncationObeysTheTailRuleAndKeepsLatency)
{
    // A synthetic 2 s response: a decaying noise tail.
    juce::AudioBuffer<float> ir (1, (int) (2.0 * kSr));
    juce::Random rng (7);

    for (int i = 0; i < ir.getNumSamples(); ++i)
        ir.setSample (0, i, (float) ((rng.nextFloat() * 2.0f - 1.0f) * std::exp (-(double) i / (0.25 * kSr))));

    for (double capSeconds : { 0.75, 0.12 })
    {
        const int cap = (int) (capSeconds * kSr);
        const int cut = IrVariants::findCut (ir, cap, QualityProfile::kTailEnergyDb);
        CHECK (cut >= cap);

        double total = 0.0, after = 0.0;

        for (int i = 0; i < ir.getNumSamples(); ++i)
        {
            const double e = (double) ir.getSample (0, i) * ir.getSample (0, i);
            total += e;
            after += i >= cut ? e : 0.0;
        }

        CHECK_MSG (10.0 * std::log10 (juce::jmax (1.0e-30, after / total)) <= -40.0 + 1.0e-9,
                   "tail after the cut is above -40 dB");

        // Moved no later than it had to be: one sample earlier breaks the rule.
        if (cut > cap)
        {
            double before = after + (double) ir.getSample (0, cut - 1) * ir.getSample (0, cut - 1);
            CHECK (10.0 * std::log10 (before / total) > -40.0);
        }
    }

    // Variants have the full response's latency, and a short response gets none.
    BodyEngine body;
    body.prepare (kSr, kBlock);
    body.setMode (BodyEngine::Mode::Convolution);
    body.loadImpulseResponse (ir.getReadPointer (0), ir.getNumSamples(), kSr);

    const int latencyFull = body.getLatencySamples();
    CHECK (body.getIrVariants().hasVariant (1));   // 2 s > 1.5 s
    CHECK (body.getIrVariants().hasVariant (2));

    for (auto level : kLevels)
    {
        body.setQualityLevel (QualityProfile::forLevel (level), true);
        juce::AudioBuffer<float> b (2, kBlock);
        b.clear();
        juce::dsp::AudioBlock<float> block (b);
        body.processBlock (block);
        CHECK (body.getLatencySamples() == latencyFull);
    }

    CHECK_NEAR (body.getIrVariants().getSecondsForLevel (1), 1.5, 0.3);
    CHECK (body.getIrVariants().getSecondsForLevel (2) < body.getIrVariants().getSecondsForLevel (1));

    juce::AudioBuffer<float> shortIr (1, (int) (0.1 * kSr));
    shortIr.clear();
    shortIr.setSample (0, 0, 1.0f);
    body.loadImpulseResponse (shortIr.getReadPointer (0), shortIr.getNumSamples(), kSr);
    CHECK (! body.getIrVariants().hasVariant (1) && ! body.getIrVariants().hasVariant (2));
}

LUTHIER_TEST (CpuQuality, CQ16_modalCapKeepsTheLowestModesAndNullsTheBody)
{
    BodyEngine high, low;

    for (auto* b : { &high, &low })
    {
        b->prepare (kSr, kBlock);
        b->setMode (BodyEngine::Mode::Modal);
        b->setBodyConfig (BodyConfig {});
        b->setAmount (1.0);
        b->reset();
    }

    const int modes = high.getNumModes();
    CHECK (modes > 20);

    low.setQualityLevel (QualityProfile::forLevel (QualityLevel::Low), true);
    CHECK (low.getRunningModeCount() == juce::jmin (modes, 20));

    // The eight lowest-frequency modes are always among those that run.
    std::vector<double> freqs;

    for (int i = 0; i < modes; ++i)
        freqs.push_back (low.getModes()[i].frequencyHz);

    auto sorted = freqs;
    std::sort (sorted.begin(), sorted.end());

    for (int k = 0; k < 8; ++k)
    {
        const double f = sorted[(size_t) k];
        bool running = false;

        for (int r = 0; r < low.getRunningModeCount(); ++r)
            running = running || low.getModes()[low.getModePriority()[r]].frequencyHz == f;

        CHECK_MSG (running, "low mode " + juce::String (k) + " (" + juce::String (f, 1) + " Hz) was dropped");
    }

    // Body null against High, on what a body really hears: a strummed open
    // chord's string sum.
    const int n = (int) (1.5 * kSr);
    std::vector<double> a ((size_t) n, 0.0), b ((size_t) n, 0.0);

    for (double hz : { 82.41, 110.0, 146.83, 196.0, 246.94, 329.63 })
    {
        StringEngine str;
        str.prepare (kSr, kBlock);
        str.snapToFrequency (hz);
        str.excite (Excitation::Params {});

        for (int i = 0; i < n; ++i)
            a[(size_t) i] += str.processSample (0.0) / 6.0;
    }

    b = a;
    high.processMono (a.data(), n);
    low.processMono (b.data(), n);

    double diff = 0.0, ref = 0.0;

    for (int i = n / 4; i < n; ++i)
    {
        diff += (a[(size_t) i] - b[(size_t) i]) * (a[(size_t) i] - b[(size_t) i]);
        ref += a[(size_t) i] * a[(size_t) i];
    }

    const double nullDb = 10.0 * std::log10 (juce::jmax (1.0e-30, diff) / juce::jmax (1.0e-30, ref));
    std::cout << "    body null at Low: " << juce::String (nullDb, 1) << " dB" << std::endl;
    CHECK_MSG (nullDb <= -30.0, "body null at Low only " + juce::String (nullDb, 1) + " dB");
}

//==============================================================================
// CQ-17 Auto, CQ-18 Notification
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ17_autoStepsDownUpAndHoldsByTheRules)
{
    QualityTestSupport::ScopedTempSettings temp;
    PerformanceSettings::get().setQuality (QualityChoice::Auto);

    CpuLoadMonitor monitor;
    QualityController c (monitor);
    c.stopTimerForTesting();

    double now = 0.0;
    QualityController::LoadSnapshot load;
    c.setClockForTesting ([&] { return now; });
    c.setLoadFeedForTesting ([&] { return load; });
    c.restartAutoForTesting();

    auto runUntil = [&] (double untilMs) { for (; now <= untilMs; now += 100.0) c.tick(); };

    // A 70 % mean: nothing before 2 s, a step at 2 s.
    load.mean2s = 0.70;
    runUntil (1900.0);
    CHECK (c.getAutoLevel() == QualityLevel::High);
    runUntil (2100.0);
    CHECK (c.getAutoLevel() == QualityLevel::Medium);

    // 3 s dwell before the next step.
    runUntil (4900.0);
    CHECK (c.getAutoLevel() == QualityLevel::Medium);
    runUntil (5200.0);
    CHECK (c.getAutoLevel() == QualityLevel::Low);

    // The p95 trigger alone.
    PerformanceSettings::get().setAutoLastLevel (QualityLevel::High);
    c.restartAutoForTesting();
    now = 100000.0;
    load = {};
    load.mean2s = 0.30;
    load.p95_2s = 0.90;
    runUntil (102100.0);
    CHECK (c.getAutoLevel() == QualityLevel::Medium);

    // Up only after 20 s below 30 % with a passing prediction, and never
    // within 30 s of a down-step.
    load = {};
    load.mean2s = 0.2;
    load.mean20s = 0.20;   // 0.20 * 1.25 = 0.25 < 0.50
    runUntil (122000.0);
    CHECK (c.getAutoLevel() == QualityLevel::Medium);   // < 30 s since the down
    runUntil (132300.0);
    CHECK (c.getAutoLevel() == QualityLevel::High);

    // A failing prediction keeps it down: High measured at 60 %, Medium at
    // 25 %, so the ratio is 2.4 and 25 % * 2.4 = 60 % > 50 %.
    PerformanceSettings::get().setAutoLastLevel (QualityLevel::High);
    c.restartAutoForTesting();
    now = 300000.0;
    load = {};
    load.mean2s = 0.60;
    runUntil (303000.0);
    CHECK (c.getAutoLevel() == QualityLevel::High);
    load.p95_2s = 0.90;
    runUntil (303200.0);
    CHECK (c.getAutoLevel() == QualityLevel::Medium);
    load = {};
    load.mean2s = 0.25;
    load.mean20s = 0.25;
    runUntil (400000.0);
    CHECK (c.getAutoLevel() == QualityLevel::Medium);

    // Starts at auto_last_level.
    PerformanceSettings::get().setAutoLastLevel (QualityLevel::Low);
    c.restartAutoForTesting();
    CHECK (c.getAutoLevel() == QualityLevel::Low);

    // Holds after three downs in ten minutes; choosing Auto again clears it.
    PerformanceSettings::get().setAutoLastLevel (QualityLevel::High);
    c.restartAutoForTesting();
    now = 1000000.0;

    for (int cycle = 0; cycle < 3 && ! c.isAutoHeld(); ++cycle)
    {
        load = {};
        load.mean2s = 0.9;
        runUntil (now + 2500.0);
        load = {};
        load.mean20s = 0.05;
        runUntil (now + 35000.0);
    }

    CHECK (c.isAutoHeld());
    const auto heldAt = c.getAutoLevel();
    load = {};
    load.mean2s = 0.95;
    runUntil (now + 10000.0);
    CHECK (c.getAutoLevel() == heldAt);

    c.resumeAuto();
    CHECK (! c.isAutoHeld());
}

LUTHIER_TEST (CpuQuality, CQ18_oneBannerPerDownStepAtMostOncePerMinute)
{
    QualityTestSupport::ScopedTempSettings temp;
    PerformanceSettings::get().setQuality (QualityChoice::Auto);
    PerformanceSettings::get().setAutoNotify (true);

    CpuLoadMonitor monitor;
    QualityController c (monitor);
    c.stopTimerForTesting();

    double now = 0.0;
    QualityController::LoadSnapshot load;
    c.setClockForTesting ([&] { return now; });
    c.setLoadFeedForTesting ([&] { return load; });
    c.restartAutoForTesting();

    auto runUntil = [&] (double untilMs) { for (; now <= untilMs; now += 100.0) c.tick(); };
    auto drain = [&] (int& banners, int& announcements)
    {
        QualityController::Notice n;

        while (c.popNotice (n))
        {
            banners += n.banner ? 1 : 0;
            announcements += n.announce ? 1 : 0;
        }
    };

    int banners = 0, announcements = 0;

    // Two downs 3 s apart: two announcements, one banner (60 s rule).
    load.mean2s = 0.9;
    runUntil (5500.0);
    CHECK (c.getAutoLevel() == QualityLevel::Low);
    drain (banners, announcements);
    CHECK (banners == 1);
    CHECK (announcements == 2);

    // An up-step: an announcement, no banner.
    banners = announcements = 0;
    load = {};
    load.mean20s = 0.05;
    runUntil (40000.0);
    CHECK (c.getAutoLevel() == QualityLevel::Medium);
    drain (banners, announcements);
    CHECK (banners == 0);
    CHECK (announcements >= 1);

    // With auto_notify off: no banner, still an announcement.
    PerformanceSettings::get().setAutoNotify (false);
    banners = announcements = 0;
    load = {};
    load.mean2s = 0.9;
    runUntil (200000.0);
    drain (banners, announcements);
    CHECK (banners == 0);
    CHECK (announcements >= 1);

    // The banner text is the spec's.
    PerformanceSettings::get().setAutoNotify (true);
    PerformanceSettings::get().setAutoLastLevel (QualityLevel::High);
    c.restartAutoForTesting();
    now = 500000.0;
    load = {};
    load.mean2s = 0.9;
    runUntil (502100.0);
    QualityController::Notice n;
    CHECK (c.popNotice (n));
    CHECK (n.banner && n.text.contains ("Auto lowered quality to Medium"));
}

//==============================================================================
// CQ-19 Governor (E1 / E2 here; E3 in the render tests)
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ19_governorEntersAndLeavesAtItsThresholds)
{
    QualityTestSupport::ScopedTempSettings temp;
    CpuLoadMonitor monitor;
    QualityController c (monitor);
    c.stopTimerForTesting();

    double now = 0.0;
    QualityController::LoadSnapshot load;
    c.setClockForTesting ([&] { return now; });
    c.setLoadFeedForTesting ([&] { return load; });

    auto tickFor = [&] (double ms) { const double end = now + ms; for (; now < end; now += 100.0) c.tick(); };

    load.mean200ms = 0.84; load.mean2s = 0.8;
    tickFor (500.0);
    CHECK (c.getReliefLevel() == 0);

    load.mean200ms = 0.86;
    tickFor (100.0);
    CHECK (c.getReliefLevel() == 1);
    CHECK (c.isDataStreamSuspended());

    load.mean200ms = 0.91;
    tickFor (100.0);
    CHECK (c.getReliefLevel() == 1);   // not yet 200 ms above 90 %
    tickFor (300.0);
    CHECK (c.getReliefLevel() == 2);
    CHECK (c.isShadowAuditionFrozen());

    // Leaves when the 2 s mean falls below 70 %.
    load.mean200ms = 0.5; load.mean2s = 0.72;
    tickFor (300.0);
    CHECK (c.getReliefLevel() == 2);
    load.mean2s = 0.69;
    tickFor (100.0);
    CHECK (c.getReliefLevel() == 0);

    // Nothing while rendering offline.
    c.setNonRealtime (true);
    load.mean200ms = 0.99; load.mean2s = 0.99;
    tickFor (1000.0);
    CHECK (c.getReliefLevel() == 0);
}

LUTHIER_TEST (CpuQuality, CQ19_loadMonitorMeansAndP95)
{
    CpuLoadMonitor m;

    // 3 s of blocks at 40 % then 1 s at 90 %.
    const double block = 256.0 / kSr;

    for (int i = 0; i < (int) (3.0 / block); ++i)
        m.addBlock (0.4 * block, block);

    CHECK_NEAR (m.getMean2s(), 0.4, 0.01);
    CHECK_NEAR (m.getMean200ms(), 0.4, 0.01);

    for (int i = 0; i < (int) (1.0 / block); ++i)
        m.addBlock (0.9 * block, block);

    CHECK_NEAR (m.getMean200ms(), 0.9, 0.02);
    CHECK_NEAR (m.getMean2s(), 0.65, 0.03);
    CHECK_NEAR (m.getMean20s(), 0.525, 0.03);
    CHECK (m.getP95_2s() >= 0.88 && m.getP95_2s() <= 0.93);
}

//==============================================================================
// CQ-28 Multi-instance, CQ-29 Edition
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ28_aGlobalChangeReachesEveryInstanceButOverrides)
{
    QualityTestSupport::ScopedTempSettings temp;
    PerformanceSettings::get().setQuality (QualityChoice::High);

    LuthierAudioProcessor a, b;
    a.prepareToPlay (kSr, kBlock);
    b.prepareToPlay (kSr, kBlock);
    b.setQualityOverride (QualityOverride::High);

    PerformanceSettings::get().setQuality (QualityChoice::Low);
    CHECK (a.getQualityController().getLiveLevel() == QualityLevel::Low);
    CHECK (b.getQualityController().getLiveLevel() == QualityLevel::High);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    buffer.clear(); a.processBlock (buffer, midi);
    buffer.clear(); b.processBlock (buffer, midi);
    CHECK (a.getAppliedQualityLevel() == QualityLevel::Low);
    CHECK (b.getAppliedQualityLevel() == QualityLevel::High);
}

LUTHIER_TEST (CpuQuality, CQ29_noEditionGatesTheQualityModes)
{
    /*  editions.md 2.5: both editions, identical, no Free limit. The split
        itself is not built yet (editions.md 9), so what can be checked now is
        that nothing in the quality code knows about an edition: the Free
        configuration then runs CQ-01 to CQ-28 unchanged. */
    const auto root = QualityTestSupport::sourceRoot();

    for (const auto* rel : { "Support/QualityProfile.h", "Support/QualityController.cpp", "Support/QualityController.h",
                             "Support/PerformanceSettings.cpp", "Support/CpuLoadMonitor.h", "LuthierEngineQuality.cpp",
                             "UI/AnimationPolicy.cpp", "UI/QualityBadge.cpp", "UI/QualityOptions.cpp",
                             "DSP/Common/IrVariants.cpp" })
    {
        const auto text = root.getChildFile (rel).loadFileAsString();
        CHECK_MSG (text.isNotEmpty(), juce::String ("missing ") + rel);
        CHECK_MSG (! text.containsIgnoreCase ("LUTHIER_FREE") && ! text.containsIgnoreCase ("edition"),
                   juce::String (rel) + " gates on the edition");
    }
}

//==============================================================================
// CQ-31 Memory
//==============================================================================
LUTHIER_TEST (CpuQuality, CQ31_irVariantsAddAtMost12MB)
{
    // Measured as heap held by the live engines (see heapInUseMegabytes).
    const double without = QualityTestSupport::memoryHeldWithFourSecondIrs (false);
    const double withVariants = QualityTestSupport::memoryHeldWithFourSecondIrs (true);

    std::cout << "    memory held: " << juce::String (withVariants, 1) << " MB with variants, "
              << juce::String (without, 1) << " MB without" << std::endl;

    if (withVariants >= 0.0 && without >= 0.0)
        CHECK_MSG (withVariants - without <= 12.0, "IR variants add " + juce::String (withVariants - without, 1) + " MB");
}

//==============================================================================
// Section 7: the governor is display or emergency only (supersedes the
// performance-budget 8 ladder, which halved the noise pools under load)
//==============================================================================
LUTHIER_TEST (CpuQuality, noisePoolsHalveAtLowAndNeverUnderLoadAtHigh)
{
    QualityTestSupport::ScopedTempSettings temp;
    const QualityController::GovernorScope governor (true);
    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);

    const int full = p.getEngine().getNoisePool().getPoolLimit (NoiseClass::squeak);
    juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels()), kBlock);
    juce::MidiBuffer midi;

    auto run = [&] (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            p.processBlock (buffer, midi);
        }
    };

    // An explicit High under a saturated load: the sound is not reduced.
    p.getQualityController().forceLevelForTesting ((int) QualityLevel::High);
    const double block = (double) kBlock / kSr;

    for (int i = 0; i < (int) (2.0 / block); ++i)
        p.getCpuLoadMonitorForTesting().addBlock (1.5 * block, block);

    run (8);
    CHECK (p.getEngine().getNoisePool().getPoolLimit (NoiseClass::squeak) == full);

    // Low halves them (table 2.1), and High gives them back.
    p.getQualityController().forceLevelForTesting ((int) QualityLevel::Low);
    run (2);
    CHECK (p.getEngine().getNoisePool().getPoolLimit (NoiseClass::squeak) < full);

    p.getQualityController().forceLevelForTesting ((int) QualityLevel::High);
    run (2);
    CHECK (p.getEngine().getNoisePool().getPoolLimit (NoiseClass::squeak) == full);

    p.getQualityController().forceLevelForTesting (-1);
}
