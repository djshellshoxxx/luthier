/*  Host-integration regressions found by the CLAP validator (host-integration.md 3,
    state-model.md): what a host restores must survive what a host does next.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../Presets/ExactRestore.h"
#include "../Support/ErrorLog.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct ScopedHostDiagnostics
    {
        ScopedHostDiagnostics()
        {
            folder.createDirectory();
            ErrorLog::setFolderForTesting (folder);
        }
        ~ScopedHostDiagnostics()
        {
            ErrorLog::setFolderForTesting ({});
            folder.deleteRecursively();
        }
        juce::File folder { juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("luthier-host-state-" + juce::Uuid().toString()) };
    };

    juce::MemoryBlock saveState (LuthierAudioProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }

    juce::String asText (const juce::MemoryBlock& block)
    {
        return juce::String::fromUTF8 (static_cast<const char*> (block.getData()), (int) block.getSize());
    }

    // Failure diagnostics only: retain numeric lexemes instead of reparsing them
    // as doubles, and never print strings or values from opaque state sections.
    struct StateJsonNode
    {
        char kind=0;
        size_t begin=0,end=0;
        std::vector<StateJsonNode> children;
        std::vector<juce::String> keys;
    };

    class StateJsonReader
    {
    public:
        explicit StateJsonReader (const juce::String& text) : input (text.toStdString()) {}
        bool parse (StateJsonNode& node)
        { return input.size() <= 16*1024*1024 && value (node,0) && (space(), position==input.size()); }
        std::string token (const StateJsonNode& node) const { return input.substr (node.begin,node.end-node.begin); }
    private:
        std::string input;
        size_t position=0,nodes=0;
        void space() { while (position<input.size() && (input[position]==' ' || input[position]=='\n' || input[position]=='\r' || input[position]=='\t')) ++position; }
        bool take (char c) { space(); if (position==input.size() || input[position]!=c) return false; ++position; return true; }
        bool digits() { const auto start=position; while(position<input.size() && input[position]>='0' && input[position]<='9') ++position; return position>start; }
        bool quoted (juce::String* decoded=nullptr)
        {
            const auto start=position;
            if(position==input.size() || input[position++]!='"') return false;
            while(position<input.size())
            {
                const auto c=input[position++];
                if(c=='\\') { if(position==input.size()) return false; ++position; }
                else if(c=='"')
                {
                    const auto parsed=juce::JSON::fromString(juce::String::fromUTF8(input.data()+start,int(position-start)));
                    if(!parsed.isString()) return false;
                    if(decoded) *decoded=parsed.toString();
                    return true;
                }
            }
            return false;
        }
        bool value (StateJsonNode& node,int depth)
        {
            space(); if(depth>64 || ++nodes>65536 || position==input.size()) return false;
            node.begin=position; const auto c=input[position]; node.kind=c;
            if(c=='{' || c=='[')
            {
                ++position; const char close=c=='{'?'}':']';
                if(take(close)) { node.end=position; return true; }
                do
                {
                    if(c=='{') { juce::String key; space(); if(!quoted(&key) || !take(':')) return false; node.keys.push_back(key); }
                    StateJsonNode child; if(!value(child,depth+1)) return false; node.children.push_back(std::move(child));
                    if(take(close)) { node.end=position; return true; }
                } while(take(','));
                return false;
            }
            if(c=='"') { if(!quoted()) return false; }
            else if(c=='t' || c=='f' || c=='n')
            {
                const std::string word=c=='t'?"true":c=='f'?"false":"null";
                if(input.compare(position,word.size(),word)!=0) return false;
                position+=word.size();
            }
            else
            {
                node.kind='#'; if(c=='-') ++position;
                if(position==input.size()) return false;
                if(input[position]=='0') ++position; else if(!digits()) return false;
                if(position<input.size() && input[position]=='.') { ++position; if(!digits()) return false; }
                if(position<input.size() && (input[position]=='e' || input[position]=='E'))
                { ++position; if(position<input.size() && (input[position]=='+' || input[position]=='-')) ++position; if(!digits()) return false; }
            }
            node.end=position; return true;
        }
    };

    const char* stateJsonType (char kind)
    {
        switch(kind) { case '{':return "object"; case '[':return "array"; case '"':return "string";
            case '#':return "number"; case 'n':return "null"; case 't':case 'f':return "boolean"; default:return "unknown"; }
    }

    bool stateJsonKnownKey (const juce::String& key,bool root)
    {
        // Unknown names may themselves contain private data. Dynamic parameter,
        // asset and extension maps are deliberately opaque below their container.
        static const juce::StringArray roots {"preset","referencePedals","ui","normalization","modulation","metronome","stability","setlist","routing","midiLearn","tune","lockedParameters","slotBActive","presetMorphPosition","formatVersion","savedBy","jamPlayValue","jamFillNowValue","clickToMain","liveMode"};
        static const juce::StringArray fields {"schema","schemaVersion","parameters","params","modulation","lfos","envelopes","sequencers","followers","points","steps","v","g","s","p","rate","sync","division","phase","depth","symmetry","shape","retrigger","smoothing","bipolar","delay","attack","hold","decay","sustain","release","loop","curves","source","string","length","direction","swing","instances","order","enabled","mix","mixLaw","limiterOn","reportLatency","cpuCapPct","tempoBpm","midiChan","tempoSource","abSelect","abOut","mode","boardStyle","window","parallelNorm","numLanes","lanes","gainDb","phaseInvert","loops","used","kind","blend","sendDb","returnDb","first","last","feedback","on","src","dst","gain","kill","lpHz","runawayOk","extras","id","rev","placement","lane","slot","inDb","outDb","x","mounting","name"};
        return (root?roots:fields).contains(key);
    }

    juce::String firstStateJsonDifference (const StateJsonReader& a,const StateJsonNode& an,
                                          const StateJsonReader& b,const StateJsonNode& bn,
                                          const juce::String& path,bool opaque=false)
    {
        const auto types=juce::String(" types=")+stateJsonType(an.kind)+"/"+stateJsonType(bn.kind);
        if(juce::String(stateJsonType(an.kind))!=stateJsonType(bn.kind)) return "path="+path+" reason=type"+types;
        if(an.kind=='{' || an.kind=='[')
        {
            const auto common=std::min(an.children.size(),bn.children.size());
            for(size_t i=0;i<common;++i)
            {
                auto childPath=path+"["+juce::String(int(i))+"]"; bool childOpaque=opaque;
                if(an.kind=='{')
                {
                    const auto& ak=an.keys[i]; const auto& bk=bn.keys[i];
                    if(ak!=bk)
                    {
                        auto sortedA=an.keys,sortedB=bn.keys;
                        std::sort(sortedA.begin(),sortedA.end()); std::sort(sortedB.begin(),sortedB.end());
                        const bool sameKeys=sortedA==sortedB;
                        return "path="+path+"[property#"+juce::String(int(i))+"] reason="+(sameKeys?"key-order":"key-membership")+types
                            +" lengths="+juce::String(int(an.children.size()))+"/"+juce::String(int(bn.children.size()));
                    }
                    const bool known=!opaque && stateJsonKnownKey(ak,path=="$");
                    childPath=known?path+"."+ak:path+"[property#"+juce::String(int(i))+"]";
                    childOpaque=opaque || !known || ak=="extras" || ak=="parameters" || ak=="params" || ak=="name";
                }
                const auto difference=firstStateJsonDifference(a,an.children[i],b,bn.children[i],childPath,childOpaque);
                if(difference.isNotEmpty()) return difference;
            }
            if(an.children.size()!=bn.children.size()) return "path="+path+" reason=length"+types+" lengths="+juce::String(int(an.children.size()))+"/"+juce::String(int(bn.children.size()));
            return {};
        }
        const auto at=a.token(an),bt=b.token(bn);
        if(at==bt) return {};
        auto result="path="+path+" reason="+(an.kind=='#'?"numeric-representation":an.kind=='"'?"string-representation":"value")+types;
        if(an.kind=='#' && !opaque && at.size()<=48 && bt.size()<=48) result+=" tokens="+juce::String(at)+"/"+juce::String(bt);
        else result+=" values=<redacted> tokenLengths="+juce::String(int(at.size()))+"/"+juce::String(int(bt.size()));
        return result;
    }

    juce::String stateJsonDiagnostic (const juce::String& a,const juce::String& b)
    {
        const auto aa=a.toStdString(),bb=b.toStdString(); size_t first=0;
        while(first<std::min(aa.size(),bb.size()) && aa[first]==bb[first]) ++first;
        auto summary=juce::String("bytes=")+juce::String(juce::int64(aa.size()))+"/"+juce::String(juce::int64(bb.size()))+" firstByte="+juce::String(juce::int64(first))+" ";
        StateJsonReader ar(a),br(b); StateJsonNode an,bn;
        if(!ar.parse(an) || !br.parse(bn)) return summary+"reason=diagnostic-parse-unavailable values=<redacted>";
        const auto difference=firstStateJsonDifference(ar,an,br,bn,"$");
        return summary+(difference.isEmpty()?juce::String("path=$ reason=whitespace-or-encoding"):difference);
    }

    /** Equal states; when not, both are left in the temp folder to diff. */
    bool sameState (const juce::String& a, const juce::String& b, const char* name)
    {
        if (a == b)
            return true;

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);
        dir.getChildFile (juce::String (name) + "-a.json").replaceWithText (a);
        dir.getChildFile (juce::String (name) + "-b.json").replaceWithText (b);
        try { std::cout << "HostState exact JSON mismatch: " << stateJsonDiagnostic (a,b) << std::endl; }
        catch (...) { std::cout << "HostState exact JSON mismatch: diagnostic unavailable; values=<redacted>" << std::endl; }
        return false;
    }
}

LUTHIER_TEST (HostState, aSessionSurvivesThePrepareThatFollowsIt)
{
    // Hosts restore a session and then prepare (and prepare again on a sample
    // rate change). prepare() used to reset the LFOs' custom shapes, the
    // envelopes' curves, the sequencers' steps and the followers' sources.
    auto source = std::make_unique<LuthierAudioProcessor>();
    source->prepareToPlay (48000.0, 256);

    auto& lfo = source->getModMatrix().getLfo (0);
    for (int i = 0; i < ModLfo::kNumBreakpoints; ++i)
        lfo.setBreakpoint (i, i % 2 == 0 ? 0.9 : -0.3);

    auto step = source->getModMatrix().getSequencer (0).getStep (3);
    step.value = 0.123;
    source->getModMatrix().getSequencer (0).setStep (3, step);

    const auto saved = saveState (*source);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->setStateInformation (saved.getData(), (int) saved.getSize());
    restored->prepareToPlay (44100.0, 512);

    for (int i = 0; i < ModLfo::kNumBreakpoints; ++i)
        CHECK_NEAR (restored->getModMatrix().getLfo (0).getBreakpoint (i), i % 2 == 0 ? 0.9 : -0.3, 1.0e-9);

    CHECK_NEAR (restored->getModMatrix().getSequencer (0).getStep (3).value, 0.123, 1.0e-9);
    CHECK (sameState (asText (saveState (*restored)), asText (saved), "luthier-restored"));
}

LUTHIER_TEST (HostState, anUnpreparedInstanceSavesTheSameStateAsAPreparedOne)
{
    // A host may save before it ever prepares (clap-validator
    // state-reproducibility-flush). The defaults must not depend on prepare().
    auto prepared = std::make_unique<LuthierAudioProcessor>();
    prepared->prepareToPlay (48000.0, 256);

    auto unprepared = std::make_unique<LuthierAudioProcessor>();

    CHECK (sameState (asText (saveState (*prepared)), asText (saveState (*unprepared)), "luthier-unprepared"));
}

LUTHIER_TEST (HostState, failureDiagnosticsPreserveNumericTokensAndRedactPrivateState)
{
    const auto diagnostic=[](const char* a,const char* b){return stateJsonDiagnostic(juce::String(a),juce::String(b));};
    const auto signedZero=diagnostic(R"({"referencePedals":{"mix":-0.0}})",R"({"referencePedals":{"mix":0.0}})");
    CHECK(signedZero.contains("path=$.referencePedals.mix")); CHECK(signedZero.contains("tokens=-0.0/0.0"));
    CHECK(signedZero.contains("types=number/number")); CHECK(signedZero.contains("bytes=")); CHECK(signedZero.contains("firstByte="));
    CHECK(diagnostic(R"({"referencePedals":{"mix":1}})",R"({"referencePedals":{"mix":1.0}})").contains("tokens=1/1.0"));
    CHECK(diagnostic(R"({"referencePedals":{"mix":1e-200}})",R"({"referencePedals":{"mix":1.0000000000000001e-200}})").contains("tokens=1e-200/1.0000000000000001e-200"));
    const auto order=diagnostic(R"({"referencePedals":{"enabled":false,"mix":1.0}})",R"({"referencePedals":{"mix":1.0,"enabled":false}})");
    CHECK(order.contains("reason=key-order")); CHECK(order.contains("path=$.referencePedals[property#0]")); CHECK(order.contains("lengths=2/2"));
    CHECK(diagnostic(R"({"referencePedals":{"mix":1}})",R"({"referencePedals":{"enabled":true}})").contains("reason=key-membership"));
    const auto type=diagnostic(R"({"referencePedals":{"mix":1}})",R"({"referencePedals":{"mix":"SECRET_STRING"}})");
    CHECK(type.contains("reason=type")); CHECK(type.contains("types=number/string")); CHECK(!type.contains("SECRET_STRING"));
    CHECK(diagnostic(R"({"preset":{"modulation":{"lfos":[{"points":[0,1]}]}}})",R"({"preset":{"modulation":{"lfos":[{"points":[0]}]}}})").contains("path=$.preset.modulation.lfos[0].points reason=length"));
    const auto string=diagnostic(R"({"preset":{"name":"SECRET_A"}})",R"({"preset":{"name":"SECRET_B_LONG"}})");
    CHECK(string.contains("reason=string-representation")); CHECK(string.contains("values=<redacted>")); CHECK(!string.contains("SECRET_")); CHECK(string.contains("tokenLengths=10/15"));
    const auto privateKey=diagnostic(R"({"PRIVATE_LICENSE_KEY":{"mix":1122334455}})",R"({"PRIVATE_LICENSE_KEY":{"mix":9988776655}})");
    CHECK(privateKey.contains("path=$[property#0][property#0]")); CHECK(privateKey.contains("values=<redacted>")); CHECK(!privateKey.contains("PRIVATE_LICENSE_KEY")); CHECK(!privateKey.contains("1122334455")); CHECK(!privateKey.contains("9988776655"));
    const auto opaque=diagnostic(R"({"referencePedals":{"extras":{"mix":1122334455}}})",R"({"referencePedals":{"extras":{"mix":9988776655}}})");
    CHECK(opaque.contains("values=<redacted>")); CHECK(!opaque.contains("1122334455")); CHECK(!opaque.contains("9988776655"));
    const auto parameters=diagnostic(R"({"preset":{"parameters":{"PRIVATE_PARAM":1122334455}}})",R"({"preset":{"parameters":{"PRIVATE_PARAM":9988776655}}})");
    CHECK(parameters.contains("path=$.preset.parameters[property#0]")); CHECK(!parameters.contains("PRIVATE_PARAM")); CHECK(!parameters.contains("1122334455"));
    CHECK(diagnostic(R"({"referencePedals":{"mix":1}})",R"({ "referencePedals" : { "mix" : 1 } })").contains("reason=whitespace-or-encoding"));
    CHECK(diagnostic(R"({"referencePedals":{"mix":1}})",R"({"referencePedals":{"mix":PRIVATE_INVALID}})").contains("reason=diagnostic-parse-unavailable"));
    CHECK(diagnostic(R"({"reference\u0050edals":{"mix":-0.0}})",R"({"referencePedals":{"mix":0.0}})").contains("path=$.referencePedals.mix"));
    CHECK(diagnostic(R"({"referencePedals":{"mix":0}})",R"({"referencePedals":{"mix":0,}})").contains("reason=diagnostic-parse-unavailable"));
    CHECK(diagnostic(R"({"referencePedals":{"mix":0}})",R"({"referencePedals":{"mix":01}})").contains("reason=diagnostic-parse-unavailable"));
    const auto deep=juce::String::repeatedString("[",66)+"0"+juce::String::repeatedString("]",66);
    CHECK(stateJsonDiagnostic(deep,deep+" ").contains("reason=diagnostic-parse-unavailable"));
    const auto escaped=diagnostic(R"({"preset":{"name":"SECRET_\\\"VALUE_A"}})",R"({"preset":{"name":"SECRET_\\\"VALUE_B"}})");
    CHECK(escaped.contains("values=<redacted>")); CHECK(!escaped.contains("SECRET_"));
    CHECK(diagnostic(R"({"preset":{"name":0,"name":1}})",R"({"preset":{"name":0,"mix":1}})").contains("reason=key-membership"));
    const auto unknownOrder=diagnostic(R"({"PRIVATE_KEY_A":123})",R"({"PRIVATE_KEY_B":456})");
    CHECK(unknownOrder.contains("reason=key-membership")); CHECK(!unknownOrder.contains("PRIVATE_KEY"));


}

//==============================================================================
/*  ExactRestore.h, for every ranged parameter, skewed ranges included:

      - A value that arrived through a set (a host's plain write, or a raw
        normalised one as a hand-edited preset holds) is stored exactly as
        getValue() reports it - what clap-validator compares after a reload -
        and a restore reads it back to the bit. The direct setValueNotifyingHost
        path drifts by a float ULP on skewed ranges, which made save -> restore
        -> save differ.

      - A constructor default never went through the round trip and can be
        unreachable; it is stored where the parameter reads it back, and that
        restores exactly too.

    Either way the plain value moves by at most a few plain-range ULPs (three
    on tap_duration's 10-2000 ms range: 0.0002 ms), so nothing audible or
    displayed changes. */
LUTHIER_TEST (HostState, aRestoredNormalisedValueReadsBackExactly)
{
    auto ulpsApart = [] (float a, float b)
    {
        long n = 0;
        for (float x = a; x != b && n < 64; x = std::nextafter (x, b))
            ++n;
        return n;
    };

    LuthierAudioProcessor p;
    juce::Random rng (0x5EED);
    juce::StringArray inexact, moved, rewritten, defaultsMoved;
    int checked = 0;

    for (auto* param : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);

        if (ranged == nullptr || ParamIDs::isJamTransient (ranged->getParameterID()))
            continue;

        // A bool keeps the normalised float it is given, so a raw draw reads
        // back as itself; the helper leaves it alone. Floats are the subject.
        const bool isFloat = dynamic_cast<juce::AudioParameterFloat*> (param) != nullptr;
        long worstUlps = 0, worstPlainUlps = 0, worstDefaultUlps = 0;

        for (int k = -1; k < 256; ++k)
        {
            // k = -1: the constructor default, untouched. Even draws: what a
            // host wrote as a plain value. Odd draws: a raw normalised value.
            if (k >= 0)
                ranged->setValueNotifyingHost (k % 2 == 0 || ! isFloat
                                                   ? ranged->convertTo0to1 (ranged->convertFrom0to1 (rng.nextFloat()))
                                                   : rng.nextFloat());

            const float live = ranged->getValue();
            const double stored = ExactRestore::storable (*ranged, live);   // what toVar writes

            if (k >= 0 && (float) stored != live)
                rewritten.add (ranged->getParameterID());
            else if (k < 0)
                worstDefaultUlps = ulpsApart ((float) stored, live);

            ranged->setValueNotifyingHost (rng.nextFloat());   // scramble
            ExactRestore::applyNormalised (*ranged, stored);
            ++checked;

            worstUlps = std::max (worstUlps, ulpsApart (ranged->getValue(), (float) stored));
            worstPlainUlps = std::max (worstPlainUlps,
                                       ulpsApart (ranged->convertFrom0to1 (ranged->getValue()),
                                                  ranged->convertFrom0to1 (live)));
        }

        if (worstUlps > 0)
            inexact.add (ranged->getParameterID() + " (" + juce::String (worstUlps) + " ulp)");

        if (worstPlainUlps > 4)
            moved.add (ranged->getParameterID() + " (" + juce::String (worstPlainUlps) + " plain ulp)");

        if (worstDefaultUlps > 4)
            defaultsMoved.add (ranged->getParameterID() + " (" + juce::String (worstDefaultUlps) + " ulp)");
    }

    rewritten.removeDuplicates (false);

    CHECK (checked > 0);
    CHECK_MSG (rewritten.isEmpty(), "a set value was not stored as getValue() reports it: " + rewritten.joinIntoString (", "));
    CHECK_MSG (inexact.isEmpty(), "restored values not read back exactly: " + inexact.joinIntoString (", "));
    CHECK_MSG (moved.isEmpty(), "restore moved the plain value by more than 4 ulps: " + moved.joinIntoString (", "));
    CHECK_MSG (defaultsMoved.isEmpty(), "a default was stored more than 4 ulps away: " + defaultsMoved.joinIntoString (", "));
}

LUTHIER_TEST (HostState, theMorphSliderAndTheCharacterAmountAreSaved)
{
    auto source = std::make_unique<LuthierAudioProcessor>();

    auto set = [] (LuthierAudioProcessor& p, const char* id, float normalised)
    {
        if (auto* param = p.getState().getParameter (id))
            param->setValueNotifyingHost (normalised);
    };

    set (*source, ParamIDs::presetMorphPosition, 0.3f);
    set (*source, ParamIDs::macroCharacter, 0.8f);

    const auto saved = saveState (*source);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->setStateInformation (saved.getData(), (int) saved.getSize());

    CHECK_NEAR (restored->getState().getParameter (ParamIDs::presetMorphPosition)->getValue(), 0.3f, 1.0e-4);
    CHECK_NEAR (restored->getState().getParameter (ParamIDs::macroCharacter)->getValue(), 0.8f, 1.0e-4);
}

LUTHIER_TEST (HostState, aHostWritingAGuitarTypeWithItsPartsKeepsTheParts)
{
    // A session or automation lands the guitar type and the body together, in
    // any order: the body the host wrote is kept (guitar-workshop 0.6's
    // shortcut writes only what the host did not).
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (48000.0, 256);

    auto* body = p->getState().getParameter (ParamIDs::bodyDepth);
    auto* type = p->getState().getParameter (ParamIDs::guitarType);
    CHECK (body != nullptr && type != nullptr);

    if (body == nullptr || type == nullptr)
        return;

    body->setValueNotifyingHost (0.013f);
    type->setValueNotifyingHost (type->convertTo0to1 (type->convertFrom0to1 (type->getValue()) + 3.0f));
    p->getParameterBridge().applyAllNow();

    CHECK_NEAR (body->getValue(), 0.013f, 1.0e-4);

    // A player's pick in the header (a gesture) loads the new guitar's parts.
    type->beginChangeGesture();
    type->setValueNotifyingHost (type->convertTo0to1 (type->convertFrom0to1 (type->getValue()) - 2.0f));
    type->endChangeGesture();
    p->getParameterBridge().applyAllNow();

    CHECK_MSG (std::abs (body->getValue() - 0.013f) > 1.0e-3, "the picked guitar did not bring its own body");
}

LUTHIER_TEST (HostState, parameterTextRoundTripsStably)
{
    // host-integration 3 (clap-validator param-conversions): value -> text ->
    // value -> text must not drift.
    LuthierAudioProcessor p;
    juce::Random rng (1234);

    for (auto* param : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);

        if (ranged == nullptr)
            continue;

        for (int k = 0; k < 8; ++k)
        {
            const float v = rng.nextFloat();
            const auto text = ranged->getText (v, 64);
            const float back = ranged->getValueForText (text);
            CHECK_MSG (ranged->getText (back, 64) == text,
                       ranged->getName (64) + ": '" + text + "' -> '" + ranged->getText (back, 64) + "'");
        }
    }
}



LUTHIER_TEST (HostState, processingDoesNotMoveParameters)
{
    // clap-validator param-set-wrong-namespace: a host that sends nothing
    // must see every parameter where it left it, before and after processing.
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->setRateAndBufferSizeDetails (48000.0, 512);

    std::vector<float> before;
    for (auto* param : p->getParameters())
        before.push_back (param->getValue());

    p->prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (p->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;

    for (int b = 0; b < 20; ++b)
    {
        buffer.clear();
        p->processBlock (buffer, midi);
    }

    juce::StringArray moved;
    for (int i = 0; i < p->getParameters().size(); ++i)
        if (std::abs (p->getParameters()[i]->getValue() - before[(size_t) i]) > 1.0e-6f)
            moved.add (p->getParameters()[i]->getName (64) + " " + juce::String (before[(size_t) i]) + " -> "
                       + juce::String (p->getParameters()[i]->getValue()));

    CHECK_MSG (moved.isEmpty(), "parameters moved by themselves: " + moved.joinIntoString (", "));
}

LUTHIER_TEST (HostState, theSameParametersGiveTheSameStateHoweverTheyArrived)
{
    // clap-validator state-reproducibility-flush: one instance gets random
    // values while processing, another gets them without ever processing.
    // Their saved states must match.
    juce::Random rng (777);
    std::vector<float> values;

    auto first = std::make_unique<LuthierAudioProcessor>();
    for (int i = 0; i < first->getParameters().size(); ++i)
        values.push_back (rng.nextFloat());

    first->prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (first->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;

    for (int i = 0; i < first->getParameters().size(); ++i)
        first->getParameters()[i]->setValueNotifyingHost (values[(size_t) i]);

    for (int b = 0; b < 5; ++b)
    {
        buffer.clear();
        first->processBlock (buffer, midi);
    }

    auto second = std::make_unique<LuthierAudioProcessor>();

    for (int i = 0; i < second->getParameters().size(); ++i)
        second->getParameters()[i]->setValueNotifyingHost (values[(size_t) i]);

    CHECK (sameState (asText (saveState (*first)), asText (saveState (*second)), "luthier-arrival"));
}

//==============================================================================
// HI-8: the version string must name the exact build, not just "1.0.0", so a
// support thread can tell two builds with the same marketing version apart.
LUTHIER_TEST (HostState, versionCarriesABuildString)
{
    const auto full = getFullVersionString();

    CHECK (full.startsWith (JucePlugin_VersionString));
    CHECK (full.contains ("+"));
    CHECK (full.fromLastOccurrenceOf ("+", false, false).isNotEmpty());
}

//==============================================================================
// HI-20: the root JSON now carries a format version, so a later build can tell
// an old blob apart from one of its own.
LUTHIER_TEST (HostState, theStateCarriesAFormatVersion)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    const auto saved = saveState (p);
    const auto parsed = juce::JSON::parse (asText (saved));
    auto* root = parsed.getDynamicObject();

    CHECK (root != nullptr);
    CHECK (root != nullptr && root->hasProperty ("formatVersion"));
    CHECK (root != nullptr
           && (int) root->getProperty ("formatVersion") == LuthierAudioProcessor::kCurrentStateFormatVersion);
}

//==============================================================================
// HI-24: a root-level key this build does not recognise (a section a newer
// build added) used to vanish silently on the next save. It now travels
// through unchanged.
LUTHIER_TEST (HostState, unknownSectionsSurviveWriteBack)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    auto saved = juce::JSON::parse (asText (saveState (p)));
    auto* root = saved.getDynamicObject();
    CHECK (root != nullptr);

    if (root == nullptr)
        return;

    // Simulate a newer build's blob: a section this one has never heard of,
    // plus a format version ahead of what it understands.
    root->setProperty ("futureFeature", juce::var (juce::String ("keep-me")));
    root->setProperty ("formatVersion", LuthierAudioProcessor::kCurrentStateFormatVersion + 1);

    const auto withExtra = juce::JSON::toString (saved, false);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 256);
    restored.setStateInformation (withExtra.toRawUTF8(), (int) withExtra.getNumBytesAsUTF8());

    CHECK (restored.getUnknownHostSections().contains ("futureFeature"));
    CHECK (restored.getUnknownHostSections()["futureFeature"].toString() == "keep-me");

    // A newer-than-understood format version raises one banner.
    const auto notices = restored.takeGuitarNotices();
    CHECK (! notices.isEmpty());

    const auto reSaved = juce::JSON::parse (asText (saveState (restored)));
    auto* reRoot = reSaved.getDynamicObject();

    CHECK (reRoot != nullptr && reRoot->hasProperty ("futureFeature"));
    CHECK (reRoot != nullptr && reRoot->getProperty ("futureFeature").toString() == "keep-me");
}

//==============================================================================
// HI-25: loading a blob older than the current format used to migrate it in
// place with nothing kept of the original.
LUTHIER_TEST (HostState, anOldBlobIsBackedUpBeforeMigration)
{
    ScopedHostDiagnostics diagnostics;
    CHECK (diagnostics.folder.isDirectory());
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    auto saved = juce::JSON::parse (asText (saveState (p)));
    auto* root = saved.getDynamicObject();
    CHECK (root != nullptr);

    if (root == nullptr)
        return;

    root->removeProperty ("formatVersion");   // a pre-versioning blob

    const auto old = juce::JSON::toString (saved, false);

    auto before = LuthierAudioProcessor::getStateBackupFolder().findChildFiles (
        juce::File::findFiles, false, "state-backup-*.json");

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 256);
    restored.setStateInformation (old.toRawUTF8(), (int) old.getNumBytesAsUTF8());

    auto after = LuthierAudioProcessor::getStateBackupFolder().findChildFiles (
        juce::File::findFiles, false, "state-backup-*.json");

    // A new file, not a larger count: the folder keeps only the newest 20
    // backups, so at the cap a fresh backup prunes the oldest and the count
    // stays put.
    int created = 0;

    for (auto& f : after)
        if (! before.contains (f))
        {
            ++created;
            CHECK (f.loadFileAsString() == old);
            f.deleteFile();
        }

    CHECK_MSG (created == 1, "expected one new state backup, saw " + juce::String (created));
}

LUTHIER_TEST (HostState, backupTestOverridePreservesDefaultPathExactBytesAndTwentyFileLimit)
{
    const auto realFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                .getChildFile ("Luthier").getChildFile ("Diagnostics");
    CHECK (LuthierAudioProcessor::getStateBackupFolder() == realFolder);
    {
        ScopedHostDiagnostics diagnostics;
        CHECK (diagnostics.folder.isDirectory());
        CHECK (LuthierAudioProcessor::getStateBackupFolder() == diagnostics.folder);
        LuthierAudioProcessor p;
        auto state = juce::JSON::parse (asText (saveState (p)));
        state.getDynamicObject()->removeProperty ("formatVersion");
        const auto original = juce::JSON::toString (state, false);
        for (int i = 0; i < 23; ++i)
        {
            p.setStateInformation (original.toRawUTF8(), (int) original.getNumBytesAsUTF8());
            const auto backups = diagnostics.folder.findChildFiles (juce::File::findFiles, false, "state-backup-*.json");
            CHECK (backups.size() == juce::jmin (i + 1, 20));
        }
        const auto retained = diagnostics.folder.findChildFiles (juce::File::findFiles, false, "state-backup-*.json");
        for (const auto& file : retained)
            CHECK (file.loadFileAsString() == original);
    }
    CHECK (LuthierAudioProcessor::getStateBackupFolder() == realFolder);
}

//==============================================================================
// QA-48: bypassed output must be bit-identical to no plugin at all. Luthier is
// an instrument with no main input, so that means silence - not whatever the
// buffer already held (the JUCE default merely passes the buffer through).
LUTHIER_TEST (HostState, bypassOutputsSilence)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.getWritePointer (ch)[0] = 0.5f;   // garbage the JUCE default would pass through

    p.processBlockBypassed (buffer, midi);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        CHECK_NEAR (buffer.getMagnitude (ch, 0, buffer.getNumSamples()), 0.0f, 1.0e-9f);
}

//==============================================================================
// HI-45: getNumPrograms/getProgramName/setCurrentProgram enumerate and load
// the preset bank by index (host-integration.md section 12), untested before.
LUTHIER_TEST (HostState, programsEnumerateFactoryPresetsAndLoadByIndex)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    const int factoryCount = p.getPresetManager().getNumPresets();
    CHECK_MSG (factoryCount > 1, "no factory presets to enumerate, so this proves nothing");

    if (factoryCount <= 1)
        return;

    CHECK (p.getNumPrograms() == factoryCount);

    for (int i = 0; i < factoryCount; ++i)
    {
        const auto* info = p.getPresetManager().getPreset (i);
        CHECK (info != nullptr);

        if (info == nullptr)
            continue;

        CHECK (p.getProgramName (i) == info->name);
    }

    p.setCurrentProgram (factoryCount - 1);
    CHECK (p.getCurrentProgram() == factoryCount - 1);
    CHECK (p.getPresetManager().getCurrentPresetIndex() == factoryCount - 1);

    p.setCurrentProgram (0);
    CHECK (p.getCurrentProgram() == 0);
}
