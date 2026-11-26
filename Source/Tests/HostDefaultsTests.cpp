/*  Host defaults of the guitar-owned parameters (ParamIDs::isGuitarOwned).

    clap-validator 0.4.1 `param-default-values`: a fresh CLAP instance must sit
    on the defaults it reports. The constructor loads the startup guitar, whose
    parts move about a dozen guitar-owned parameters off their declared
    defaults, so a CLAP instance reports those startup values as its defaults
    (Parameters::adoptStartupGuitarDefaults). VST3, AU and Standalone keep the
    declared defaults, and everything that gives a default saved or hashed
    meaning - factory presets, a preset's absent keys, the loudness table,
    preset search - reads the declared one (Parameters::referenceDefault). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Edition.h"
#include "../Presets/FactoryPresets.h"
#include "../Presets/Search/PresetFeatures.h"
#include "../Support/NormalizationCalibrator.h"
#include "../Support/OutputNormalization.h"

#include <cstdio>
#include <cstring>
#include <map>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /*  The declared normalised defaults of 29 of the 33 guitar-owned parameters,
        as beta2 shipped them: printed with %a from the build before host
        defaults existed (3a365fb7, Pro), in parameter order. Each is a bool's
        0 or 1, or one division - a choice's index over its highest index, a
        linear range's (default - min) / (max - min) - so it rounds the same on
        every IEEE platform. A VST3, AU or Standalone instance keeps reporting
        exactly these. The other four are on a skewed range
        (kBeta2SkewedDeclarations). */
    const std::pair<const char*, float> kBeta2GuitarOwnedDefaults[] = {
        { "tuning_preset",        0x0p+0f },          // index 0
        { "string_material",      0x0p+0f },          // index 0
        { "fretless",             0x0p+0f },          // off
        { "setup_action_treble",  0x1.333334p-2f },   // 1.6 mm
        { "setup_action_bass",    0x1.642c84p-2f },   // 2.0 mm
        { "setup_relief",         0x1.d1745cp-2f },   // 0.2 mm
        { "setup_nut_depth_1",    0x1.7ffffep-2f },   // 0.45 mm
        { "setup_nut_depth_2",    0x1.7ffffep-2f },
        { "setup_nut_depth_3",    0x1.7ffffep-2f },
        { "setup_nut_depth_4",    0x1.7ffffep-2f },
        { "setup_nut_depth_5",    0x1.7ffffep-2f },
        { "setup_nut_depth_6",    0x1.7ffffep-2f },
        { "setup_fret_height",    0x1.999998p-2f },   // 1.0 mm
        { "body_width",           0x1p-1f },          // 1.0
        { "body_depth",           0x1.999998p-2f },   // 1.0
        { "body_bracing",         0x0p+0f },          // index 0
        { "body_top_wood",        0x0p+0f },          // index 0
        { "body_back_wood",       0x1.99999ap-2f },   // index 6
        { "body_age",             0x1.333334p-2f },   // 0.3
        { "pickup0_type",         0x0p+0f },          // index 0
        { "pickup0_magnet",       0x1.555556p-1f },   // index 2
        { "pickup1_type",         0x0p+0f },
        { "pickup1_magnet",       0x1.555556p-1f },
        { "pickup2_type",         0x0p+0f },
        { "pickup2_magnet",       0x1.555556p-1f },
        { "bridge_type",          0x1p-2f },          // index 1
        { "circuit_pot_taper",    0x0p+0f },          // index 0
        { "circuit_treble_bleed", 0x0p+0f },          // index 0
        { "circuit_active",       0x0p+0f },          // off
    };

    /*  The four guitar-owned parameters on a skewed range, as Parameters.cpp
        declares them and beta2 declared them: min, max, default, and the skew
        centre as a fraction of the span. Their normalised default goes through
        logf and powf (NormalisableRange::setSkewForCentre, convertTo0to1),
        which MSVC's UCRT and Apple's libm need not round as glibc does, so a
        number printed on Linux (beta2's were 0x1.13b13ep-1, 0x1.5950b2p-1
        twice and 0x1.80d80cp-2) is no reference on Windows or macOS. Each is
        compared instead with a stock AudioParameterFloat declared the same way
        on the machine that runs the test (declaredTwin). */
    struct SkewedDeclaration
    {
        const char* id;
        float min, max, def, skew;
    };

    const SkewedDeclaration kBeta2SkewedDeclarations[] = {
        { "body_top_thickness", 1.4f,     4.0f,   2.8f,     0.5f  },   // floatParam, 2.8 mm
        { "circuit_volume_pot", 100.0e3f, 1.0e6f, 500.0e3f, 0.24f },   // ohmParam, 500k
        { "circuit_tone_pot",   100.0e3f, 1.0e6f, 500.0e3f, 0.24f },   // ohmParam, 500k
        { "circuit_tone_cap",   10.0f,    100.0f, 22.0f,    0.24f },   // floatParam, 22 nF
    };

    /** Every guitar-owned ID: the printed table's 29 and the four skewed ones. */
    juce::StringArray beta2GuitarOwnedIds()
    {
        juce::StringArray ids;

        for (const auto& [id, value] : kBeta2GuitarOwnedDefaults)
            ids.add (id);

        for (const auto& declaration : kBeta2SkewedDeclarations)
            ids.add (declaration.id);

        return ids;
    }

    /** A stock juce::AudioParameterFloat declared as floatParam and ohmParam
        declare one (Parameters.cpp): the range (min, max), its skew set for the
        centre min + (max - min) * skew, and the default. Their
        RangeRegistry::noteDeclaration record is left out: it does not touch the
        parameter, and writing it from here would overwrite the layout's. The
        label and text functions do not reach the range or the default. Held
        by its base: AudioParameterFloat's own overrides are private. */
    std::unique_ptr<juce::RangedAudioParameter> declaredTwin (const SkewedDeclaration& d)
    {
        juce::NormalisableRange<float> range (d.min, d.max);

        if (d.skew != 1.0f)
            range.setSkewForCentre (d.min + (d.max - d.min) * d.skew);

        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, d.id, range, d.def);
    }

    bool sameBits (float a, float b) noexcept
    {
        juce::uint32 x = 0, y = 0;
        std::memcpy (&x, &a, sizeof (x));
        std::memcpy (&y, &b, sizeof (y));
        return x == y;
    }

    juce::String hex (float v)
    {
        char text[48] = {};
        std::snprintf (text, sizeof (text), "%a", (double) v);
        return text;
    }

    juce::String idOf (const juce::AudioProcessorParameter* p)
    {
        if (auto* withId = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p))
            return withId->paramID;

        return {};
    }

    /** As a host builds one: under the CLAP seam or not. Never prepared. */
    std::unique_ptr<LuthierAudioProcessor> makeProcessor (bool clapHostDefaults)
    {
        if (! clapHostDefaults)
            return std::make_unique<LuthierAudioProcessor>();

        const LuthierAudioProcessor::ScopedClapHostDefaults clap;
        return std::make_unique<LuthierAudioProcessor>();
    }

    /** The guitar type the constructor loads: the type parameter's own value, as Free plays it. */
    GuitarType startupGuitarType (LuthierAudioProcessor& processor)
    {
        auto* type = processor.getState().getParameter (ParamIDs::guitarType);
        const int index = juce::roundToInt (type->convertFrom0to1 (type->getValue()));
        return (GuitarType) edition::effectiveGuitarIndex (index);
    }

    /** A guitar-type change, applied the way the processor applies one. */
    void setGuitarType (LuthierAudioProcessor& processor, GuitarType type)
    {
        auto* p = processor.getState().getParameter (ParamIDs::guitarType);
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (int) type));
        processor.getParameterBridge().applyAllNow();
    }

    const FactoryPresets::Definition* findFactoryPreset (const juce::String& name)
    {
        for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
            if (name == FactoryPresets::getPreset (i).name)
                return &FactoryPresets::getPreset (i);

        return nullptr;
    }

    bool loadFactoryPreset (LuthierAudioProcessor& processor, const juce::String& name)
    {
        const auto* def = findFactoryPreset (name);

        if (def == nullptr || ! processor.getPresetManager().fromVar (FactoryPresets::toVar (*def, processor)))
            return false;

        processor.getParameterBridge().applyAllNow();
        return true;
    }

    /** Every guitar type that loads a factory guitar file as itself in this edition. */
    std::vector<GuitarType> typesWithFactoryGuitars()
    {
        std::vector<GuitarType> types;

        for (int t = 0; t < (int) GuitarType::NumTypes; ++t)
        {
            const auto path = LuthierAudioProcessor::getFactoryGuitarPath ((GuitarType) t);

            if (path.isNotEmpty() && edition::effectiveGuitarIndex (t) == t
                  && PartLibrary::getFactoryGuitarsFolder().getChildFile (path).existsAsFile())
                types.push_back ((GuitarType) t);
        }

        return types;
    }

    /** Records which parameters are written, in the order of first write. */
    struct WriteRecorder final : juce::AudioProcessorListener
    {
        explicit WriteRecorder (juce::AudioProcessor& p) : processor (p) { processor.addListener (this); }
        ~WriteRecorder() override { processor.removeListener (this); }

        void audioProcessorParameterChanged (juce::AudioProcessor*, int index, float) override
        {
            written.addIfNotAlreadyThere (idOf (processor.getParameters()[index]));
        }

        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}

        juce::AudioProcessor& processor;
        juce::StringArray written;
    };

    /** A host default, as plain and normalised, with the range it was read through. */
    struct DefaultReading
    {
        float normalised = 0.0f, plain = 0.0f;
        juce::NormalisableRange<float> range;
    };

    std::map<juce::String, DefaultReading> readDefaults (LuthierAudioProcessor& processor)
    {
        std::map<juce::String, DefaultReading> out;

        for (auto* p : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                DefaultReading r;
                r.normalised = ranged->getDefaultValue();
                r.plain = ranged->convertFrom0to1 (r.normalised);
                r.range = ranged->getNormalisableRange();
                out[ranged->paramID] = r;
            }

        return out;
    }

    bool sameRange (const juce::NormalisableRange<float>& a, const juce::NormalisableRange<float>& b) noexcept
    {
        return sameBits (a.start, b.start) && sameBits (a.end, b.end) && sameBits (a.skew, b.skew)
            && sameBits (a.interval, b.interval) && a.symmetricSkew == b.symmetricSkew;
    }

    /** What a parameter reads back after setValueNotifyingHost (normalised): a
        float or choice parameter keeps the plain value, so getValue() is
        convertTo0to1 (convertFrom0to1 (normalised)), which a skewed range's
        pow and log can leave an ulp off on one libm and not on another (for a
        bool, both are the 0 or 1 itself). */
    float readBackAfterWriting (const juce::RangedAudioParameter& p, float normalised)
    {
        return p.convertTo0to1 (p.convertFrom0to1 (normalised));
    }
}

//==============================================================================
/*  clap-validator 0.4.1 param-default-values: create, init, and every value is
    its reported default (stepped exactly, the rest within 1e-4 normalised).
    clap-juce-extensions reports only booleans as stepped and every value
    normalised, so a choice is held to the 1e-4 as well. */
LUTHIER_TEST (HostDefaults, aFreshClapInstanceSitsOnItsReportedDefaults)
{
    const auto processor = makeProcessor (true);

    // The factory startup guitar loaded (not the compiled fallback FACTORY_GUITAR_MISSING leaves).
    const auto startup = startupGuitarType (*processor);
    CHECK_MSG (processor->hasPartsGuitar(), "no parts guitar loaded at construction");
    CHECK_MSG (processor->getGuitarReference() == "Factory/" + LuthierAudioProcessor::getFactoryGuitarPath (startup),
               "startup guitar is '" + processor->getGuitarReference() + "'");

    juce::StringArray offenders;
    int movedByTheGuitar = 0;

    for (auto* p : processor->getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);

        if (ranged == nullptr)
            continue;

        const float value = ranged->getValue();
        const float reported = ranged->getDefaultValue();
        bool same = std::abs ((double) value - (double) reported) <= 1.0e-4;

        if (ranged->isBoolean() || ranged->isDiscrete())
            same = same && juce::roundToInt (ranged->convertFrom0to1 (value))
                             == juce::roundToInt (ranged->convertFrom0to1 (reported));

        if (! same)
            offenders.add (ranged->paramID + " (value " + juce::String (value, 6)
                           + ", reported default " + juce::String (reported, 6) + ")");

        if (ParamIDs::isGuitarOwned (ranged->paramID) && ! sameBits (value, Parameters::referenceDefault (*ranged)))
            ++movedByTheGuitar;
    }

    std::printf ("    %s: %d guitar-owned parameters start off their declared default; %d of %d off their reported one\n",
                 LuthierAudioProcessor::getFactoryGuitarPath (startup).toRawUTF8(), movedByTheGuitar,
                 offenders.size(), processor->getParameters().size());

    // Not vacuous: the startup guitar does move guitar-owned parameters off their declared defaults.
    CHECK_MSG (movedByTheGuitar > 0, "the startup guitar moved no guitar-owned parameter off its declared default");
    CHECK_MSG (offenders.isEmpty(), juce::String (offenders.size()) + " of "
                                      + juce::String (processor->getParameters().size())
                                      + " parameters are off their reported default: "
                                      + offenders.joinIntoString ("; "));
}

//==============================================================================
/*  Without the CLAP adoption (VST3, AU, Standalone) the guitar-owned parameters
    report beta2's defaults, bit for bit: the printed table for 29 of them, and
    for the four on a skewed range what beta2's declaration gives on this
    machine (declaredTwin). Every other parameter reports JUCE's own default;
    the evidence for that is the type check below (it is not declared as a
    GuitarOwnedParameter, so its getDefaultValue() is the stock one). Comparing
    its host default with Parameters::referenceDefault would prove nothing -
    for it, that is the same getDefaultValue() call, so it could not fail - and
    that comparison is made only on the guitar-owned parameters. */
LUTHIER_TEST (HostDefaults, nonClapInstancesKeepTheBeta2Defaults)
{
    const auto processor = makeProcessor (false);

    std::map<juce::String, float> beta2;

    for (const auto& [id, value] : kBeta2GuitarOwnedDefaults)
        beta2[id] = value;

    std::map<juce::String, const SkewedDeclaration*> skewed;

    for (const auto& declaration : kBeta2SkewedDeclarations)
        skewed[declaration.id] = &declaration;

    juce::StringArray problems;
    int guitarOwned = 0, comparedWithATwin = 0;

    for (auto* p : processor->getParameters())
    {
        const auto id = idOf (p);

        if (id.isEmpty())
            continue;

        const bool owned = ParamIDs::isGuitarOwned (id);
        const bool declaredOwned = dynamic_cast<const GuitarOwnedParameter*> (p) != nullptr;

        if (owned != declaredOwned)
            problems.add (id + (owned ? " is guitar-owned but not declared as one" : " is declared guitar-owned but is not"));

        if (! owned)
            continue;

        ++guitarOwned;
        const float host = p->getDefaultValue();
        const float reference = Parameters::referenceDefault (*p);

        // Not adopted: the host default is still the declared one.
        if (! sameBits (host, reference))
            problems.add (id + ": host default " + hex (host) + " != declared " + hex (reference));

        if (const auto it = beta2.find (id); it != beta2.end())
        {
            if (! sameBits (host, it->second) || ! sameBits (reference, it->second))
                problems.add (id + ": " + hex (host) + " / " + hex (reference) + ", beta2 " + hex (it->second));
        }
        else if (const auto declaration = skewed.find (id); declaration != skewed.end())
        {
            ++comparedWithATwin;
            const auto twin = declaredTwin (*declaration->second);
            const float expected = twin->getDefaultValue();
            const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (p);

            if (ranged == nullptr || ! sameRange (ranged->getNormalisableRange(), twin->getNormalisableRange()))
                problems.add (id + ": its range is not beta2's declaration");

            if (! sameBits (host, expected) || ! sameBits (reference, expected))
                problems.add (id + ": " + hex (host) + " / " + hex (reference) + ", beta2's declaration gives " + hex (expected));
        }
        else
        {
            problems.add (id + ": guitar-owned but in neither beta2 table");
        }
    }

    const int expected = beta2GuitarOwnedIds().size();
    CHECK_MSG (guitarOwned == expected,
               juce::String (guitarOwned) + " guitar-owned parameters, the beta2 tables have " + juce::String (expected));
    CHECK_MSG (comparedWithATwin == (int) std::size (kBeta2SkewedDeclarations),
               juce::String (comparedWithATwin) + " skewed parameters compared with their declaration");
    CHECK_MSG (problems.isEmpty(), problems.joinIntoString ("; "));
}

//==============================================================================
/*  A CLAP instance's defaults are fixed at construction: a prepare, a preset
    load, a guitar change and a state round trip leave them where they were (a
    host reset after a guitar change goes back to the startup guitar). */
LUTHIER_TEST (HostDefaults, clapDefaultsNeverMoveAfterConstruction)
{
    const auto processor = makeProcessor (true);
    const auto before = readDefaults (*processor);

    processor->prepareToPlay (kSr, kBlock);
    CHECK_MSG (loadFactoryPreset (*processor, "P-Bass Flatwound"), "factory preset 'P-Bass Flatwound' did not load");

    setGuitarType (*processor, GuitarType::Dreadnought);

    juce::MemoryBlock state;
    processor->getStateInformation (state);
    processor->setStateInformation (state.getData(), (int) state.getSize());
    processor->getParameterBridge().applyAllNow();

    const auto after = readDefaults (*processor);
    juce::StringArray moved;
    int guitarOwnedOffTheirDefault = 0;

    for (const auto& [id, was] : before)
    {
        const auto it = after.find (id);

        if (it == after.end())
        {
            moved.add (id + " disappeared");
            continue;
        }

        const auto& now = it->second;
        const bool same = sameRange (was.range, now.range)
                            ? sameBits (was.normalised, now.normalised)
                            : std::abs ((double) was.plain - (double) now.plain)
                                  <= 1.0e-5 * (double) (was.range.end - was.range.start);

        if (! same)
            moved.add (id + " " + juce::String (was.plain, 6) + " -> " + juce::String (now.plain, 6));

        if (ParamIDs::isGuitarOwned (id))
            if (auto* p = processor->getState().getParameter (id))
                if (! sameBits (p->getValue(), now.normalised))
                    ++guitarOwnedOffTheirDefault;
    }

    // Not vacuous: the guitar change did move guitar-owned values away from the defaults.
    CHECK_MSG (guitarOwnedOffTheirDefault > 0, "the guitar change left every guitar-owned value on its default");
    CHECK_MSG (moved.isEmpty(), juce::String (moved.size()) + " default(s) moved: " + moved.joinIntoString ("; "));
}

//==============================================================================
/*  ParamIDs::isGuitarOwned is writeGuitarParameters' list: a guitar load writes
    nothing else (the family retargets aside), so no write escapes the CLAP
    adoption - and, in Pro, every listed ID is written by some guitar. */
LUTHIER_TEST (HostDefaults, guitarOwnedCoversEveryGuitarLoadWrite)
{
    // Written by a guitar load that are not its parts: the type itself, and the
    // strum and bass-family retargets (retargetStrumDefaults, BassFamilyDefaults).
    juce::StringArray retargets { ParamIDs::guitarType,
                                  ParamIDs::strumCrossingSps, ParamIDs::strumAcceleration,
                                  ParamIDs::strumUpVelocityRatio, ParamIDs::strumTilt, ParamIDs::strumMissProbability,
                                  ParamIDs::squeakPressure, ParamIDs::pluckPosition, ParamIDs::pickThickness,
                                  ParamIDs::setupStyle };
    retargets.add (ParamIDs::slotType (false, 0));
    retargets.add (ParamIDs::slotBypass (false, 0));
    retargets.add (ParamIDs::slotMix (false, 0));

    for (int i = 0; i < Pedal::kMaxParams; ++i)
        retargets.add (ParamIDs::slotParam (false, 0, i));

    const auto types = typesWithFactoryGuitars();
    CHECK_MSG (! types.empty(), "no factory guitar files are installed");

    juce::StringArray escaped, everWritten;

    for (const auto target : types)
    {
        const auto processor = makeProcessor (false);
        processor->prepareToPlay (kSr, kBlock);

        // Somewhere else first, so the target is a new source and loads.
        const auto other = types.front() != target ? types.front() : types.back();
        setGuitarType (*processor, other);

        juce::StringArray written;

        {
            WriteRecorder recorder (*processor);
            setGuitarType (*processor, target);
            written = recorder.written;
        }

        const auto path = LuthierAudioProcessor::getFactoryGuitarPath (target);
        CHECK_MSG (processor->getGuitarReference() == "Factory/" + path,
                   path + " did not load (reference '" + processor->getGuitarReference() + "')");

        int guitarOwnedWritten = 0;

        for (const auto& id : written)
        {
            if (ParamIDs::isGuitarOwned (id))
            {
                ++guitarOwnedWritten;
                everWritten.addIfNotAlreadyThere (id);
            }
            else if (! retargets.contains (id))
            {
                escaped.addIfNotAlreadyThere (id + " (" + path + ")");
            }
        }

        CHECK_MSG (guitarOwnedWritten > 0, path + " wrote no guitar-owned parameter");
    }

    CHECK_MSG (escaped.isEmpty(), "a guitar load wrote parameters that are not guitar-owned: " + escaped.joinIntoString ("; "));

    if (edition::isPro)
    {
        juce::StringArray neverWritten;

        for (const auto& id : beta2GuitarOwnedIds())
            if (! everWritten.contains (id))
                neverWritten.add (id);

        CHECK_MSG (neverWritten.isEmpty(), "guitar-owned but no factory guitar writes: " + neverWritten.joinIntoString (", "));
    }
}

//==============================================================================
/*  What gives a default saved or hashed meaning reads the declared default, so
    an adopting (CLAP) instance and a VST3 one agree: factory preset JSON byte
    for byte, the loudness table's hash of one sound state, a preset's absent
    keys as a sound state, and preset search's reading of an absent key. */
LUTHIER_TEST (HostDefaults, hashesIgnoreHostDefaults)
{
    const auto clap = makeProcessor (true);
    const auto vst3 = makeProcessor (false);
    clap->prepareToPlay (kSr, kBlock);
    vst3->prepareToPlay (kSr, kBlock);

    const auto& clapParams = clap->getParameters();
    const auto& vst3Params = vst3->getParameters();
    CHECK (clapParams.size() == vst3Params.size());

    std::vector<int> guitarOwned;

    for (int i = 0; i < clapParams.size() && i < vst3Params.size(); ++i)
        if (ParamIDs::isGuitarOwned (idOf (clapParams[i])) && idOf (clapParams[i]) == idOf (vst3Params[i]))
            guitarOwned.push_back (i);

    CHECK ((int) guitarOwned.size() == beta2GuitarOwnedIds().size());

    // Not vacuous: the adopting instance's host defaults differ from the declared ones.
    int differing = 0;

    for (const int i : guitarOwned)
        differing += sameBits (clapParams[i]->getDefaultValue(), vst3Params[i]->getDefaultValue()) ? 0 : 1;

    CHECK_MSG (differing > 0, "the CLAP-seam instance adopted no startup default");

    auto sameOnGuitarOwned = [&guitarOwned] (const NormalizationSoundState& a, const NormalizationSoundState& b)
    {
        for (const int i : guitarOwned)
            if (i >= (int) a.values.size() || i >= (int) b.values.size() || ! sameBits (a.values[(size_t) i], b.values[(size_t) i]))
                return false;

        return true;
    };

    for (const char* name : { "Init", "Clean Double-Cut Funk", "P-Bass Flatwound", "Nylon Classical" })
    {
        const auto* def = findFactoryPreset (name);
        CHECK_MSG (def != nullptr, juce::String ("no factory preset '") + name + "'");

        if (def == nullptr)
            continue;

        // The factory file each would write.
        const auto clapJson = juce::JSON::toString (FactoryPresets::toVar (*def, *clap), false);
        const auto vst3Json = juce::JSON::toString (FactoryPresets::toVar (*def, *vst3), false);
        CHECK_MSG (clapJson == vst3Json, juce::String (name) + ": FactoryPresets::toVar differs between CLAP and VST3 instances");

        // The same sound loaded into both: the same values, and one hash whichever shape reads it.
        CHECK_MSG (loadFactoryPreset (*clap, name) && loadFactoryPreset (*vst3, name), juce::String (name) + " did not load");

        const auto clapState = clap->getOutputNormalization().captureSoundState();
        const auto vst3State = vst3->getOutputNormalization().captureSoundState();
        CHECK_MSG (sameOnGuitarOwned (clapState, vst3State), juce::String (name) + ": guitar-owned values differ after the load");

        CHECK_MSG (NormalizationCalibrator::hashSoundState (clapState, *clap) == NormalizationCalibrator::hashSoundState (clapState, *vst3),
                   juce::String (name) + ": the CLAP state hashes differently through the two shapes");
        CHECK_MSG (NormalizationCalibrator::hashSoundState (vst3State, *clap) == NormalizationCalibrator::hashSoundState (vst3State, *vst3),
                   juce::String (name) + ": the VST3 state hashes differently through the two shapes");
    }

    // A hand-written preset that leaves the guitar-owned keys out (PF-14: absent means the default).
    const auto sparse = juce::JSON::parse (R"({ "parameters": { "guitar_type": 0.0, "amp_gain": 0.5 } })");
    const auto clapSparse = clap->getOutputNormalization().soundStateFromPreset (sparse);
    const auto vst3Sparse = vst3->getOutputNormalization().soundStateFromPreset (sparse);
    CHECK_MSG (sameOnGuitarOwned (clapSparse, vst3Sparse), "soundStateFromPreset reads an absent guitar-owned key differently");
    CHECK (NormalizationCalibrator::hashSoundState (clapSparse, *clap) == NormalizationCalibrator::hashSoundState (vst3Sparse, *vst3));

    const PresetFeatureReader clapReader (*clap), vst3Reader (*vst3);
    juce::StringArray searchDiffers;

    for (const auto& id : beta2GuitarOwnedIds())
        if (clapReader.plain (sparse, id) != vst3Reader.plain (sparse, id))
            searchDiffers.add (id);

    CHECK_MSG (searchDiffers.isEmpty(), "preset search reads absent keys differently: " + searchDiffers.joinIntoString (", "));
}

//==============================================================================
/*  PF-14 through the real load (PresetManager::fromVar, then the bridge): a
    preset that leaves the guitar out - no guitar block, no guitar-owned key -
    puts every guitar-owned parameter on its declared default, the same bits in
    an adopting (CLAP) instance as in a VST3 one. Red with getDefaultValue() in
    the PF-14 branch: the CLAP instance kept its startup guitar's values, the
    defaults it adopted, and the two instances loaded different sounds. */
LUTHIER_TEST (HostDefaults, absentGuitarKeysLoadTheDeclaredDefaults)
{
    const auto clap = makeProcessor (true);
    const auto vst3 = makeProcessor (false);
    clap->prepareToPlay (kSr, kBlock);
    vst3->prepareToPlay (kSr, kBlock);

    const auto& clapParams = clap->getParameters();
    const auto& vst3Params = vst3->getParameters();
    CHECK (clapParams.size() == vst3Params.size());

    std::vector<int> guitarOwned;

    for (int i = 0; i < clapParams.size() && i < vst3Params.size(); ++i)
        if (ParamIDs::isGuitarOwned (idOf (clapParams[i])) && idOf (clapParams[i]) == idOf (vst3Params[i]))
            guitarOwned.push_back (i);

    CHECK ((int) guitarOwned.size() == beta2GuitarOwnedIds().size());

    // Not vacuous: before the load, the startup guitar holds guitar-owned values
    // off their declared defaults, and the CLAP instance reports exactly those as
    // its defaults - a load that wrote getDefaultValue() would leave them there.
    int startupOffDeclared = 0;

    for (const int i : guitarOwned)
    {
        const auto* p = clapParams[i];
        const float value = p->getValue();

        if (! sameBits (value, Parameters::referenceDefault (*p)) && sameBits (value, p->getDefaultValue()))
            ++startupOffDeclared;
    }

    CHECK_MSG (startupOffDeclared > 0, "the CLAP instance starts with no guitar-owned value off its declared default");

    // A minimal valid preset: the magic and schema PresetManager checks, two keys
    // that are not the guitar's, and nothing of the guitar.
    auto* parameters = new juce::DynamicObject();
    parameters->setProperty (ParamIDs::ampGain, 0.6);
    parameters->setProperty (ParamIDs::roomBlend, 0.3);

    auto* root = new juce::DynamicObject();
    root->setProperty ("magic", PresetManager::kMagic);
    root->setProperty ("schemaVersion", PresetManager::kSchemaVersion);
    root->setProperty ("parameters", juce::var (parameters));
    const juce::var preset (root);

    for (auto* processor : { clap.get(), vst3.get() })
    {
        CHECK_MSG (processor->getPresetManager().fromVar (preset), "the minimal preset did not load");
        processor->getParameterBridge().applyAllNow();

        // It loaded: the keys it has are applied.
        auto* gain = processor->getState().getParameter (ParamIDs::ampGain);
        auto* room = processor->getState().getParameter (ParamIDs::roomBlend);
        CHECK (gain != nullptr && std::abs (gain->getValue() - 0.6f) <= 1.0e-6f);
        CHECK (room != nullptr && std::abs (room->getValue() - 0.3f) <= 1.0e-6f);
    }

    juce::StringArray differ, offDeclared;
    int exactlyDeclared = 0;

    for (const int i : guitarOwned)
    {
        const auto id = idOf (clapParams[i]);
        const float clapValue = clapParams[i]->getValue();
        const float vst3Value = vst3Params[i]->getValue();

        if (! sameBits (clapValue, vst3Value))
            differ.add (id + " (CLAP " + hex (clapValue) + ", VST3 " + hex (vst3Value) + ")");

        for (const auto* p : { clapParams[i], vst3Params[i] })
        {
            const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (p);
            const float value = p->getValue();
            const float declared = Parameters::referenceDefault (*p);
            exactlyDeclared += sameBits (value, declared) ? 1 : 0;

            // The declared default, as the load's write of it reads back.
            if (ranged == nullptr || ! (sameBits (value, declared) || sameBits (value, readBackAfterWriting (*ranged, declared))))
                offDeclared.addIfNotAlreadyThere (id + " (" + (p == clapParams[i] ? "CLAP " : "VST3 ") + hex (value)
                                                  + ", declared " + hex (declared) + ")");
        }
    }

    std::printf ("    %d guitar-owned parameters start off their declared default; after the load %d differ between CLAP and VST3,"
                 " %d are off it (%d of %d values on it bit for bit)\n",
                 startupOffDeclared, differ.size(), offDeclared.size(), exactlyDeclared, 2 * (int) guitarOwned.size());

    CHECK_MSG (differ.isEmpty(), "absent guitar-owned keys load differently in CLAP and VST3: " + differ.joinIntoString ("; "));
    CHECK_MSG (offDeclared.isEmpty(), "absent guitar-owned keys did not load the declared default: " + offDeclared.joinIntoString ("; "));
}
