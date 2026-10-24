// OWNER: WP4
#include "BoardState.h"
#include "PedalRegistry.h"
#include "Midi/MidiState.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string_view>
#include <charconv>
#include <cerrno>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <locale.h>
#include <sstream>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

namespace luthier::reference
{
namespace
{
using V = juce::var;
V object() { return V (new juce::DynamicObject()); }
V copy (const V& v) { return v.clone(); }
V element (const V& v, int i) { if (const auto* a = v.getArray()) if (i >= 0 && i < a->size()) return a->getReference(i); return {}; }
void put (V& v, const char* key, const V& value) { v.getDynamicObject()->setProperty (key, value); }
bool has (const V& v, const char* key) { return v.hasProperty (key); }
V source (const V& envelope) { auto v = copy (envelope["source"]); return v.getDynamicObject() ? v : object(); }
bool fail (juce::String* error, const juce::String& text)
{ if (error) *error = text; return false; }
// Older Apple libc++ deployments do not provide floating charconv. Parse
// with an explicit, immutable C numeric locale, never the process locale.
// strtod_l correctly rounds representable subnormals (some libcs set ERANGE
// for these); only nonfinite overflow and nonzero values rounded to zero fail.
// StrictJson supplies a grammar-checked range in the NUL-terminated input.
bool parseDouble(const char* first,const char* last,double& out)
{
#if defined(_WIN32)
    static const auto numericLocale=_create_locale(LC_NUMERIC,"C");
#else
    static const auto numericLocale=newlocale(LC_NUMERIC_MASK,"C",nullptr);
#endif
    if(!numericLocale)return false;
    char* end=nullptr;const auto previousErrno=errno;errno=0;
#if defined(_WIN32)
    const auto value=_strtod_l(first,&end,numericLocale);
#else
    const auto value=strtod_l(first,&end,numericLocale);
#endif
    errno=previousErrno;
    if(end!=last || !std::isfinite(value))return false;
    if(value==0)
        for(auto p=first;p!=last && *p!='e' && *p!='E';++p)if(*p>='1' && *p<='9')return false;
    out=value;return true;
}
std::string doubleText(double value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    auto text=stream.str();
    // Canonical exponent spelling across standard-library implementations.
    const auto exponent=text.find_first_of("eE");
    if(exponent!=std::string::npos)
    {
        auto digit=exponent+1;bool negative=false;
        if(text[digit]=='+' || text[digit]=='-'){negative=text[digit]=='-';++digit;}
        while(digit+1<text.size() && text[digit]=='0')++digit;
        const auto tail=text.substr(digit);
        text.resize(exponent);text+='e';if(negative && tail!="0")text+='-';text+=tail;
    }
    else if(text.find('.')==std::string::npos)text+=".0";
    return text;
}
// Bound syntax/nesting before correctly rounded numeric conversion. JUCE
// remains the string escape decoder.
class StrictJson
{
public:
    explicit StrictJson(const juce::String& text) : data(text.toRawUTF8(),text.getNumBytesAsUTF8()) {}
    bool parse(V& out) { return value(0,out) && (space(), pos == data.size()); }
    std::string_view boardBlock() const { return block; }
private:
    std::string_view data, block;
    size_t pos = 0;
    void space() { while (pos < data.size() && (data[pos]==' ' || data[pos]=='\t' || data[pos]=='\n' || data[pos]=='\r')) ++pos; }
    bool take(char c) { space(); if (pos < data.size() && data[pos]==c) { ++pos; return true; } return false; }
    bool string(juce::String* decoded = nullptr)
    {
        space(); const auto start=pos;
        if (pos == data.size() || data[pos++]!='"') return false;
        while (pos < data.size())
        {
            const auto c=(unsigned char)data[pos++];
            if (c=='"')
            {
                if (decoded) *decoded=juce::JSON::fromString(juce::String::fromUTF8(data.data()+start,(int)(pos-start))).toString();
                return true;
            }
            if (c < 32) return false;
            if (c=='\\')
            {
                if (pos == data.size()) return false;
                const auto e=data[pos++];
                if (e=='u')
                {
                    for (int i=0;i<4;++i)
                    {
                        if (pos == data.size()) return false;
                        const auto h=data[pos++];
                        if (! ((h>='0' && h<='9') || (h>='a' && h<='f') || (h>='A' && h<='F'))) return false;
                    }
                }
                else if (std::string_view("\"\\/bfnrt").find(e)==std::string_view::npos) return false;
            }
        }
        return false;
    }
    bool value(int depth, V& out)
    {
        space(); if (depth > 64 || pos == data.size()) return false;
        if (data[pos]=='"') { juce::String text; if (!string(&text)) return false; out=text; return true; }
        if (take('{'))
        {
            std::set<juce::String> names; out=object();
            if (take('}')) return true;
            do
            {
                juce::String name; V child;
                if (!string(&name) || !names.insert(name).second || !take(':')) return false;
                space(); const auto start=pos;
                if (!value(depth+1,child)) return false;
                if (depth==0 && name=="referencePedals") block=data.substr(start,pos-start);
                out.getDynamicObject()->setProperty(name,std::move(child));
            } while (take(','));
            return take('}');
        }
        if (take('['))
        {
            out=juce::Array<V>{};
            if (take(']')) return true;
            do { V child; if (!value(depth+1,child)) return false; out.append(std::move(child)); } while (take(','));
            return take(']');
        }
        for (const auto token : { "true", "false", "null" })
        {
            const std::string_view t(token);
            if (data.substr(pos,t.size())==t) { pos+=t.size(); out=token==std::string_view("null") ? V() : V(token==std::string_view("true")); return true; }
        }
        const auto start=pos;
        if (data[pos]=='-') ++pos;
        if (pos == data.size() || data[pos]<'0' || data[pos]>'9') return false;
        if (data[pos]=='0') ++pos;
        else while (pos < data.size() && data[pos]>='0' && data[pos]<='9') ++pos;
        bool floating=false;
        if (pos < data.size() && data[pos]=='.')
        {
            floating=true; ++pos; const auto first=pos;
            while (pos < data.size() && data[pos]>='0' && data[pos]<='9') ++pos;
            if (first==pos) return false;
        }
        if (pos < data.size() && (data[pos]=='e' || data[pos]=='E'))
        {
            floating=true; ++pos;
            if (pos < data.size() && (data[pos]=='+' || data[pos]=='-')) ++pos;
            const auto first=pos;
            while (pos < data.size() && data[pos]>='0' && data[pos]<='9') ++pos;
            if (first==pos) return false;
        }
        if (floating)
        {
            double n=0;
            if (!parseDouble(data.data()+start,data.data()+pos,n)) return false;
            out=n;
        }
        else
        {
            int64_t n=0;
            const auto parsed=std::from_chars(data.data()+start,data.data()+pos,n);
            if (parsed.ec!=std::errc() || parsed.ptr!=data.data()+pos) return false;
            out=n>=std::numeric_limits<int>::min() && n<=std::numeric_limits<int>::max() ? V((int)n) : V((juce::int64)n);
        }
        return true;
    }
};
bool finiteTree (const V& v, int depth = 0)
{
    if (depth > 64) return false;
    if (v.isDouble()) return std::isfinite ((double) v);
    if (auto* a = v.getArray()) { for (const auto& x : *a) if (! finiteTree (x, depth + 1)) return false; }
    if (auto* o = v.getDynamicObject()) { for (const auto& p : o->getProperties()) if (! finiteTree (p.value, depth + 1)) return false; }
    return v.isVoid() || v.isBool() || v.isInt() || v.isInt64() || v.isDouble() || v.isString() || v.isArray() || v.getDynamicObject();
}
// Preserve every binary double, including unknown vendor payloads. JUCE's
// default decimal-place formatting can round them on repeated saves.
juce::String jsonText(const V& value)
{
    std::string out;
    std::function<void(const V&)> write = [&](const V& v)
    {
        if (v.isDouble())
        {
            out+=doubleText((double)v);
        }
        else if (const auto* a=v.getArray())
        {
            out+='['; bool first=true;
            for (const auto& x : *a) { if (!first) out+=','; first=false; write(x); }
            out+=']';
        }
        else if (const auto* o=v.getDynamicObject())
        {
            out+='{'; bool first=true;
            for (const auto& p : o->getProperties())
            {
                if (!first) out+=',';
                first=false;
                out+=juce::JSON::toString(V(p.name.toString()),true).toStdString(); out+=':'; write(p.value);
            }
            out+='}';
        }
        else out+=juce::JSON::toString(v,true).toStdString();
    };
    write(value);
    return juce::String::fromUTF8(out.data(),(int)out.size());
}
bool number (const V& v) { return v.isInt() || v.isInt64() || v.isDouble(); }
template <typename T> bool real (const V& o, const char* key, T& out, double lo, double hi, bool integer = false)
{
    if (! has (o, key)) return true;
    const auto& v = o[key];
    const double n = (double) v;
    if (! number (v) || ! std::isfinite (n) || n < lo || n > hi || (integer && std::floor (n) != n)) return false;
    out = static_cast<T> (n); return true;
}
bool flag (const V& o, const char* key, bool& out)
{
    if (! has (o, key)) return true;
    if (! o[key].isBool()) return false;
    out = (bool) o[key]; return true;
}
bool uuid (const V& v, InstanceId& out, bool nullable = false)
{
    if (! v.isString()) return false;
    const auto s = v.toString();
    if (s.length()!=32 && s.length()!=36) return false;
    if (s.length()==36)
        for (int i=0;i<s.length();++i)
            if ((s[i]=='-') != (i==8 || i==13 || i==18 || i==23)) return false;
    const auto hex = s.removeCharacters ("-");
    if (hex.length() != 32 || ! hex.containsOnly ("0123456789abcdefABCDEF")) return false;
    out = InstanceId (s); return nullable || ! out.isNull();
}
bool idField (const V& o, const char* key, InstanceId& out)
{ return ! has (o, key) || uuid (o[key], out, true); }
V instanceSource (const V& root, const InstanceId& id)
{
    if (auto* a = root["instances"].getArray())
        for (const auto& v : *a) { InstanceId parsed; if (uuid(v["id"],parsed) && parsed == id) return copy(v); }
    return object();
}
bool readSettings (const V& root, BoardSettings& s)
{
    if (! flag (root,"enabled",s.enabled) || ! flag(root,"reportLatency",s.reportLatency)
        || ! flag(root,"limiterOn",s.limiterOn) || ! flag(root,"trails",s.trails)
        || ! flag(root,"mixTail",s.mixTail) || ! flag(root,"reampEnable",s.reampEnable)
        || ! flag(root,"reampReplace",s.reampReplace)
        || ! real(root,"mix",s.mix,0,1) || ! real(root,"mixLaw",s.mixLaw,0,2,true)
        || ! real(root,"abSelect",s.abSelect,0,3,true) || ! real(root,"abOut",s.abOut,0,1,true)
        || ! real(root,"tempoSource",s.tempoSource,0,3,true) || ! real(root,"tempoBpm",s.tempoBpm,20,300)
        || ! real(root,"midiChan",s.midiChan,0,16,true) || ! real(root,"cpuCapPct",s.cpuCapPct,1,100,true)
        || ! real(root,"limiterCeilDb",s.limiterCeilDb,-6,6)
        || ! real(root,"reampGainDb",s.reampGainDb,-std::numeric_limits<float>::max(),12)) return false;
    if (has(root,"mode") && (! root["mode"].isString() || ! modeFromString(root["mode"].toString(),s.mode))) return false;
    if (has(root,"boardStyle")) { if (! root["boardStyle"].isString()) return false; boardStyleFromString(root["boardStyle"].toString(),s.style); }
    for (const auto* key : { "midi", "ui", "feedback" }) if (has(root,key) && ! root[key].getDynamicObject()) return false;
    s.midi = copy(root["midi"]); s.ui = copy(root["ui"]);
    if (has(root,"window"))
    {
        if (!root["window"].isString()) return false;
        if (!s.ui.getDynamicObject()) s.ui=object();
        put(s.ui,"window",root["window"]);
    }
    if (has(s.ui,"window") && !s.ui["window"].isString()) return false;
    if (!midi::MidiState::validate(s.midi)) return false;
    const auto& f = root["feedback"];
    bool heldKill=false; // Momentary runtime state never resumes from a preset.
    if (!flag(f,"kill",heldKill)) return false;
    if (! flag(f,"on",s.fb.on) || ! flag(f,"runawayOk",s.fb.runawayOk) || ! idField(f,"src",s.fb.src)
        || ! idField(f,"dst",s.fb.dst) || ! real(f,"gain",s.fb.gain,0,1.05f)
        || ! real(f,"lpHz",s.fb.lpHz,std::numeric_limits<float>::min(),std::numeric_limits<float>::max())) return false;
    if (has(root,"lanes"))
    {
        const auto& lanes = root["lanes"];
        if (! lanes.isArray() || lanes.size() > kMaxBranches) return false;
        for (int i=0;i<lanes.size();++i)
            if (! lanes[i].getDynamicObject() || ! real(lanes[i],"gainDb",s.lanes.gainDb[(size_t)i],-std::numeric_limits<float>::max(),6)
                || ! flag(lanes[i],"phaseInvert",s.lanes.phaseInvert[(size_t)i])) return false;
    }
    if (! real(root,"numLanes",s.lanes.numLanes,2,4,true) || ! real(root,"parallelNorm",s.lanes.norm,0,2,true)) return false;
    if (has(root,"loops"))
    {
        const auto& loops = root["loops"];
        if (! loops.isArray() || loops.size() > kMaxLoops) return false;
        for (int i=0;i<loops.size();++i)
        {
            const auto& l = loops[i]; auto& t = s.loops[(size_t)i];
            if (! l.getDynamicObject() || ! flag(l,"used",t.used) || ! flag(l,"enabled",t.enabled)
                || ! real(l,"kind",t.kind,0,2,true) || ! real(l,"blend",t.blend,0,1)
                || ! real(l,"sendDb",t.sendDb,-24,12) || ! real(l,"returnDb",t.returnDb,-24,12)
                || ! idField(l,"first",t.first) || ! idField(l,"last",t.last)) return false;
        }
    }
    if (has(root,"snapshots"))
    {
        const auto& bank=root["snapshots"];
        if (!bank.isArray() || bank.size()>8) return false;
        for (const auto& preset : *bank.getArray())
        {
            if (preset.isVoid()) continue;
            if (!preset.getDynamicObject() || has(preset,"snapshots") || (has(preset,"name") && !preset["name"].isString())) return false;
            std::vector<PedalInstance> pedals; BoardSettings state;
            if (!BoardState::decode(preset,pedals,state)) return false;
        }
    }
    return true;
}
V writeSettings (const BoardSettings& s)
{
    V r = source(s.unknown);
    put(r,"schema",kSchemaVersion);
#define WRITE(name) put(r,#name,s.name)
    WRITE(enabled); WRITE(mix); put(r,"mixLaw",(int)s.mixLaw); WRITE(reportLatency); WRITE(limiterOn);
    WRITE(limiterCeilDb); WRITE(trails); WRITE(mixTail); WRITE(reampEnable); WRITE(reampReplace); WRITE(reampGainDb);
    WRITE(cpuCapPct); WRITE(tempoBpm); WRITE(midiChan); put(r,"tempoSource",(int)s.tempoSource);
    put(r,"abSelect",(int)s.abSelect); put(r,"abOut",(int)s.abOut);
    put(r,"mode",toString(s.mode));
    BoardStyle original;
    // Unknown presentation choices remain usable with default visuals.
    if (! has(r,"boardStyle") || boardStyleFromString(r["boardStyle"].toString(),original) || s.style != BoardStyle::plywood)
        put(r,"boardStyle",toString(s.style));
    if (! s.midi.isVoid()) put(r,"midi",copy(s.midi));
    if (! s.ui.isVoid()) put(r,"ui",copy(s.ui));
    if (s.ui["window"].isString()) put(r,"window",s.ui["window"]);
    put(r,"parallelNorm",(int)s.lanes.norm); put(r,"numLanes",s.lanes.numLanes);
    V lanes(juce::Array<V>{}), loops(juce::Array<V>{});
    for (int i=0;i<kMaxBranches;++i)
    {
        auto l = element(r["lanes"],i).getDynamicObject() ? copy(element(r["lanes"],i)) : object();
        put(l,"gainDb",s.lanes.gainDb[(size_t)i]); put(l,"phaseInvert",s.lanes.phaseInvert[(size_t)i]); lanes.append(l);
    }
    put(r,"lanes",lanes);
    for (int i=0;i<kMaxLoops;++i)
    {
        const auto& t = s.loops[(size_t)i]; auto l = element(r["loops"],i).getDynamicObject() ? copy(element(r["loops"],i)) : object();
        put(l,"used",t.used); put(l,"enabled",t.enabled); put(l,"kind",(int)t.kind); put(l,"blend",t.blend);
        put(l,"sendDb",t.sendDb); put(l,"returnDb",t.returnDb); put(l,"first",t.first.toString()); put(l,"last",t.last.toString()); loops.append(l);
    }
    put(r,"loops",loops);
    auto f = r["feedback"].getDynamicObject() ? copy(r["feedback"]) : object();
    put(f,"on",s.fb.on); put(f,"src",s.fb.src.toString()); put(f,"dst",s.fb.dst.toString()); put(f,"gain",s.fb.gain);
    put(f,"kill",false); put(f,"lpHz",s.fb.lpHz); put(f,"runawayOk",s.fb.runawayOk); put(r,"feedback",f);
#undef WRITE
    return r;
}
}

bool BoardState::canonicalJson(const V& value,juce::String& output,juce::String* error)
{
    if(error)error->clear();
    if(!finiteTree(value))return fail(error,"Invalid or nonfinite JSON value");
    auto next=jsonText(value);
    if(next.getNumBytesAsUTF8()>maxBytes)return fail(error,"JSON exceeds 256 KB");
    output=std::move(next);return true;
}

bool BoardState::extractFromHostJson(const juce::String& host, juce::String& board, juce::String* error)
{
    if (error) error->clear();
    StrictJson parser(host); V root;
    if (!parser.parse(root) || !root.getDynamicObject()) return fail(error,"Malformed host JSON");
    if (!root.hasProperty("referencePedals")) { board.clear(); return true; }
    if (!root["referencePedals"].getDynamicObject()) return fail(error,"Board state must be an object");
    const auto block=parser.boardBlock();
    if (block.size()>maxBytes) return fail(error,"Board exceeds 256 KB");
    board=juce::String::fromUTF8(block.data(),(int)block.size());
    return true;
}

bool BoardState::decode (const V& json, std::vector<PedalInstance>& output, BoardSettings& settings, juce::String* error)
{
    if (error) error->clear();
    if (! json.getDynamicObject() || ! finiteTree(json) || jsonText(json).getNumBytesAsUTF8() > maxBytes)
        return fail(error,"Invalid or oversized board JSON");
    int schema = 0;
    if (! has(json,"schema") || ! real(json,"schema",schema,1,std::numeric_limits<int>::max(),true)) return fail(error,"Invalid board schema");
    BoardSettings s;
    s.unknown = object(); put(s.unknown,"source",copy(json));
    if (schema > kSchemaVersion)
    {
        std::vector<PedalInstance> empty; V baseline;
        BoardSettings clean;
        if (! encode(empty,clean,baseline,error)) return false;
        put(s.unknown,"baseline",jsonText(baseline));
        output.clear(); settings = std::move(s); return true;
    }
    if (! readSettings(json,s)) return fail(error,"Invalid board settings");
    const auto& instances = json["instances"]; const auto& order = json["order"];
    if ((has(json,"instances") && ! instances.isArray()) || (has(json,"order") && ! order.isArray())
        || instances.size() > kMaxSlots || order.size() != instances.size()) return fail(error,"Invalid board instance/order list");
    std::vector<PedalInstance> parsed;
    std::set<juce::String> ids;
    std::set<int> slots;
    for (int i=0;i<instances.size();++i)
    {
        const auto& v = instances[i]; PedalInstance p;
        if (! v.getDynamicObject() || ! uuid(v["id"],p.instanceId) || ! ids.insert(p.instanceId.toString()).second
            || ! v["model"].isString() || v["model"].toString().isEmpty()) return fail(error,"Invalid/duplicate pedal identity");
        p.modelId = v["model"].toString().toStdString();
        if (! real(v,"rev",p.modelRevision,1,65535,true) || ! flag(v,"enabled",p.enabled)
            || ! real(v,"placement",p.placement,0,2,true) || ! real(v,"lane",p.lane,0,kMaxBranches-1,true)
            || ! real(v,"slot",p.slot,-1,kMaxSlots-1,true) || ! real(v,"inDb",p.inDb,-12,12)
            || ! real(v,"outDb",p.outDb,-12,12) || ! real(v,"x",p.x,-std::numeric_limits<float>::max(),std::numeric_limits<float>::max())) return fail(error,"Invalid pedal controls");
        // Order wins a duplicate automation assignment; fill conflicts in order below.
        if (has(v,"mounting")) { if (! v["mounting"].isString()) return fail(error,"Invalid mounting"); mountingFromString(v["mounting"].toString(),p.mounting); }
        if (has(v,"extras") && ! v["extras"].getDynamicObject()) return fail(error,"Invalid pedal extras");
        p.extras = copy(v["extras"]);
        if(const auto* midi=p.extras["midi"].getDynamicObject();midi && midi->hasProperty("fsMode"))
        {
            const auto mode=midi->getProperty("fsMode");
            if((!mode.isInt() && !mode.isInt64()) || (juce::int64)mode<0 || (juce::int64)mode>2)
                return fail(error,"Invalid pedal MIDI footswitch mode");
        }
        const auto* desc = PedalRegistry::find(p.modelId.c_str());
        p.missing = desc == nullptr; if (p.missing) p.enabled = false;
        if (desc) for (int j=0;j<desc->numParams;++j) p.params[(size_t)j] = desc->params[j].defaultNormalised();
        if (has(v,"params") && ! v["params"].getDynamicObject()) return fail(error,"Invalid pedal params");
        if (auto* po = v["params"].getDynamicObject()) for (const auto& param : po->getProperties())
        {
            const auto n = (double)param.value;
            if (! number(param.value) || ! std::isfinite(n) || n < 0 || n > 1) return fail(error,"Invalid normalized pedal parameter");
            const auto index = desc ? PedalRegistry::findParam(*desc,param.name.toString().toRawUTF8()) : -1;
            if (index >= 0) p.params[(size_t)index] = (float)n;
        }
        parsed.push_back(std::move(p));
    }
    std::vector<PedalInstance> ordered;
    ids.clear();
    for (int i=0;i<order.size();++i)
    {
        InstanceId id;
        if (! uuid(order[i],id) || ! ids.insert(id.toString()).second) return fail(error,"Invalid/duplicate order UUID");
        const auto found = std::find_if(parsed.begin(),parsed.end(),[&](const auto& p){return p.instanceId == id;});
        if (found == parsed.end()) return fail(error,"Order references missing instance");
        auto p = *found;
        if (p.slot >= 0 && ! slots.insert(p.slot).second) p.slot = -1;
        ordered.push_back(std::move(p));
    }
    for (auto& p : ordered) if (p.slot < 0)
        for (int slot=0;slot<kMaxSlots;++slot) if (slots.insert(slot).second) { p.slot=slot; break; }
    const auto position=[&](const InstanceId& id)
    {
        for (size_t i=0;i<ordered.size();++i) if (ordered[i].instanceId==id) return (int)i;
        return -1;
    };
    if (s.fb.on && (position(s.fb.dst)<0 || position(s.fb.src)<position(s.fb.dst)
                   || (s.fb.gain>0.98f && !s.fb.runawayOk))) return fail(error,"Invalid feedback endpoints or runaway gain");
    for (const auto& loop : s.loops)
        if (loop.used && (position(loop.first)<0 || position(loop.last)<position(loop.first))) return fail(error,"Invalid loop endpoints");
    output = std::move(ordered); settings = std::move(s); return true;
}

bool BoardState::encode (const std::vector<PedalInstance>& instances, const BoardSettings& settings, V& output, juce::String* error)
{
    if (error) error->clear();
    // Validate typed input before enum-to-string lookups and serialisation.
    if (settings.mode > Mode::Loop || settings.style > BoardStyle::loose) return fail(error,"Invalid board enum");
    const auto baseline = settings.unknown["baseline"].toString();
    auto current = settings;
    if (baseline.isNotEmpty()) current.unknown = V();
    V root = writeSettings(current), list(juce::Array<V>{}), order(juce::Array<V>{});
    for (const auto& p : instances)
    {
        if (p.mounting > Mounting::none || p.slot < -1 || p.slot >= kMaxSlots
            || ! std::all_of(p.params.begin(),p.params.end(),[](float n){return std::isfinite(n) && n >= 0 && n <= 1;}))
            return fail(error,"Invalid pedal controls");
        V v = instanceSource(source(settings.unknown),p.instanceId);
        put(v,"id",p.instanceId.toString()); put(v,"model",p.modelId.c_str()); put(v,"rev",p.modelRevision);
        put(v,"enabled",p.enabled); put(v,"placement",(int)p.placement); put(v,"lane",p.lane); put(v,"slot",p.slot);
        put(v,"inDb",p.inDb); put(v,"outDb",p.outDb); put(v,"x",p.x);
        Mounting original;
        if (! has(v,"mounting") || mountingFromString(v["mounting"].toString(),original) || p.mounting != Mounting::automatic)
            put(v,"mounting",toString(p.mounting));
        V params = v["params"].getDynamicObject() ? copy(v["params"]) : object();
        if (const auto* desc = PedalRegistry::find(p.modelId.c_str()))
            for (int i=0;i<desc->numParams;++i) put(params,desc->params[i].id,p.params[(size_t)i]);
        put(v,"params",params); put(v,"extras",p.extras.isVoid() ? object() : copy(p.extras));
        list.append(v); order.append(p.instanceId.toString());
    }
    put(root,"instances",list); put(root,"order",order);
    if (! finiteTree(root)) return fail(error,"Nonfinite or invalid JSON value");
    std::vector<PedalInstance> checked; BoardSettings checkedSettings;
    // schema is current here; decode cannot take the forward recursion path.
    if (! decode(root,checked,checkedSettings,error)) return false;
    if (baseline.isNotEmpty())
    {
        if (baseline == jsonText(root)) root = source(settings.unknown);
        else put(root,"unknown",source(settings.unknown));
    }
    if (jsonText(root).getNumBytesAsUTF8() > maxBytes) return fail(error,"Board exceeds 256 KB");
    output = std::move(root); return true;
}
bool BoardState::save (const BoardModel& model, V& json, juce::String* error)
{
    std::vector<PedalInstance> instances; BoardSettings settings; model.snapshot(instances,settings);
    return encode(instances,settings,json,error);
}
bool BoardState::save (const BoardModel& model, juce::String& json, juce::String* error)
{
    std::vector<PedalInstance> instances; BoardSettings settings; model.snapshot(instances,settings);
    V v; if (! encode(instances,settings,v,error)) return false;
    auto text = jsonText(v);
    if ((int)v["schema"] > kSchemaVersion && settings.unknown["verbatim"].isString()) text = settings.unknown["verbatim"].toString();
    if (text.getNumBytesAsUTF8() > maxBytes) return fail(error,"Board exceeds 256 KB");
    json = std::move(text); return true;
}
bool BoardState::restore (BoardModel& model, const V& json, juce::String* error)
{
    std::vector<PedalInstance> instances; BoardSettings settings;
    return decode(json,instances,settings,error) && model.replaceAll(instances,settings,error);
}
bool BoardState::restore (BoardModel& model, const juce::String& text, juce::String* error)
{
    if (text.getNumBytesAsUTF8() > maxBytes) return fail(error,"Board exceeds 256 KB");
    V json;
    if (! StrictJson(text).parse(json)) return fail(error,"Malformed board JSON");
    std::vector<PedalInstance> instances; BoardSettings settings;
    if (! decode(json,instances,settings,error)) return false;
    if ((int)json["schema"] > kSchemaVersion) put(settings.unknown,"verbatim",text);
    return model.replaceAll(instances,settings,error);
}
} // namespace luthier::reference
