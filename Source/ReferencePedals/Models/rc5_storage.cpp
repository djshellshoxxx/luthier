// OWNER: WP16
#include "rc5_storage.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace luthier::reference::models
{
namespace
{
using V=juce::var;
using Codec=common::LooperStateCodec;
using Graph=common::PagedLooperCore::Graph;
using Status=common::PagedLooperCore::CaptureStatus;
bool fail(juce::String* error,const char* text){if(error)*error=text;return false;}
bool numeric(const V& value){return value.isInt() || value.isInt64() || value.isDouble();}
bool finiteTree(const V& value,int depth=0)
{
    if(depth>64)return false;
    if(value.isDouble())return std::isfinite(double(value));
    if(const auto* array=value.getArray()){for(const auto& child:*array)if(!finiteTree(child,depth+1))return false;return true;}
    if(const auto* object=value.getDynamicObject()){for(const auto& child:object->getProperties())if(!finiteTree(child.value,depth+1))return false;return true;}
    return value.isVoid() || value.isBool() || value.isInt() || value.isInt64() || value.isString();
}
bool valid(const Rc5MemoryBank::Settings& s)
{
    return std::isfinite(s.loopLevel) && s.loopLevel>=0 && s.loopLevel<=200
        && std::isfinite(s.rhythmLevel) && s.rhythmLevel>=0 && s.rhythmLevel<=100
        && std::isfinite(s.tempo) && s.tempo>=40 && s.tempo<=300
        && s.pattern>=1 && s.pattern<=57 && s.variation>=0 && s.variation<=1 && s.kit>=1 && s.kit<=7;
}
bool isDefault(const Rc5MemoryBank::Settings& s)
{return s.name.isEmpty() && s.loopLevel==100 && s.rhythmLevel==80 && s.tempo==120 && s.pattern==1 && s.variation==0 && s.kit==1 && !s.reverse;}
bool readNumber(const V& root,const char* key,double& out,double low,double high,bool integral=false)
{
    if(!root.hasProperty(key))return true;
    const auto& value=root[key];
    if(!numeric(value))return false;
    const double n=double(value);
    if(!std::isfinite(n) || n<low || n>high || (integral && std::floor(n)!=n))return false;
    out=n;return true;
}
bool readSettings(const V& value,Rc5MemoryBank::Settings& s)
{
    if(!value.getDynamicObject())return false;
    if(value.hasProperty("name")){if(!value["name"].isString())return false;s.name=value["name"].toString();}
    if(value.hasProperty("reverse")){if(!value["reverse"].isBool())return false;s.reverse=bool(value["reverse"]);}
    double pattern=s.pattern,variation=s.variation,kit=s.kit;
    if(!readNumber(value,"loopLevel",s.loopLevel,0,200) || !readNumber(value,"rhythmLevel",s.rhythmLevel,0,100)
        || !readNumber(value,"tempo",s.tempo,40,300) || !readNumber(value,"pattern",pattern,1,57,true)
        || !readNumber(value,"variation",variation,0,1,true) || !readNumber(value,"kit",kit,1,7,true))return false;
    s.pattern=int(pattern);s.variation=int(variation);s.kit=int(kit);return true;
}
V writeSettings(const Rc5MemoryBank::Memory& memory)
{
    V next=memory.source["settings"].getDynamicObject()?memory.source["settings"].clone():V(new juce::DynamicObject());
    auto* object=next.getDynamicObject();const auto& s=memory.settings;
    object->setProperty("name",s.name);object->setProperty("loopLevel",s.loopLevel);object->setProperty("rhythmLevel",s.rhythmLevel);
    object->setProperty("tempo",s.tempo);object->setProperty("pattern",s.pattern);object->setProperty("variation",s.variation);
    object->setProperty("kit",s.kit);object->setProperty("reverse",s.reverse);return next;
}
}
bool Rc5MemoryBank::withinCapacity(const std::array<Memory,memoryCount>& memories,juce::String* error)
{
    // Exact rational accumulation. At most99 bounded terms; no per-memory
    // rounding and no restriction to the usual audio rates. Message thread only.
    juce::BigInteger numerator(0),denominator(1);
    for(const auto& memory:memories)
    {
        if(memory.loop.forward || memory.loop.graph.status==Status::Empty)continue;
        const auto& loop=memory.loop;
        const auto rate=uint64_t(loop.config.sampleRate);
        if(loop.graph.frames>5400*rate)return fail(error,"Memory exceeds90minutes");
        const juce::BigInteger divisor{juce::int64(rate)};
        const juce::BigInteger frames{juce::int64(loop.graph.frames)};
        numerator=numerator*divisor+frames*denominator;
        denominator*=divisor;
        if(numerator>denominator*juce::BigInteger(46800))return fail(error,"Known memory duration exceeds13hours");
    }
    return true;
}
bool Rc5MemoryBank::capacityKnown() const noexcept
{
    if(forward)return false;
    for(const auto& memory:entries)if(memory.loop.forward)return false;
    return true;
}
bool Rc5MemoryBank::get(int number,Memory& out,juce::String* error) const
{
    if(!numberValid(number) || forward)return fail(error,"Unavailable memory number or opaque index");
    Memory next=entries[size_t(number-1)];
    next.source=next.source.clone();next.loop.source=next.loop.source.clone();
    out=std::move(next);return true;
}
bool Rc5MemoryBank::setSettings(int number,const Settings& settings,juce::String* error)
{
    if(!numberValid(number) || forward)return fail(error,"Unavailable memory number or opaque index");
    if(!valid(settings))return fail(error,"Invalid memory settings");
    entries[size_t(number-1)].settings=settings;return true;
}
bool Rc5MemoryBank::save(int number,const Graph& graph,const common::PagedLooperCore::Config& config,juce::String* error)
{
    if(!numberValid(number) || forward)return fail(error,"Unavailable memory number or opaque index");
    if(entries[size_t(number-1)].loop.forward)return fail(error,"Cannot replace a future looper schema");
    if(graph.status!=Status::Ready)return fail(error,"Saving audio requires a Ready capture");
    if(!capacityKnown())return fail(error,"Future audio makes total duration indeterminate");
    V encoded;Codec::Stored stored;
    if(!Codec::encode(entries[size_t(number-1)].loop,graph,config,encoded,error) || !Codec::decode(encoded,stored,error))return false;
    auto candidate=entries;candidate[size_t(number-1)].loop=std::move(stored);
    if(!withinCapacity(candidate,error))return false;
    entries=std::move(candidate);return true;
}
bool Rc5MemoryBank::clear(int number,juce::String* error)
{
    if(!numberValid(number) || forward)return fail(error,"Unavailable memory number or opaque index");
    auto& memory=entries[size_t(number-1)];
    if(memory.loop.forward)return fail(error,"Cannot clear a future looper schema");
    if(memory.loop.source.isVoid())return true;
    V encoded;Codec::Stored next;
    if(!Codec::encode(memory.loop,Graph{},memory.loop.config,encoded,error) || !Codec::decode(encoded,next,error))return false;
    memory.loop=std::move(next);return true;
}
bool Rc5MemoryBank::references(References& out,bool deduplicate,juce::String*) const
{
    References next;next.complete=!forward;
    const auto add=[&](const AssetHash& hash)
    {if(!deduplicate || std::find(next.hashes.begin(),next.hashes.end(),hash)==next.hashes.end())next.hashes.push_back(hash);};
    if(!forward)for(const auto& memory:entries)
    {
        if(memory.loop.forward){next.complete=false;continue;}
        const auto& g=memory.loop.graph;if(g.status!=Status::Ready)continue;
        add(g.base);
        for(int i=0;i<g.layerCount;++i)add(g.layers[size_t(i)]);
        for(int i=0;i<g.retiredCount;++i)add(g.retired[size_t(i)]);
        for(int i=0;i<g.redoCount;++i)add(g.redo[size_t(i)]);
    }
    out=std::move(next);return true;
}
bool Rc5MemoryBank::decode(const V& value,Rc5MemoryBank& out,juce::String* error)
{
    Rc5MemoryBank next;
    if(value.isVoid()){out=std::move(next);return true;}
    if(!value.getDynamicObject() || !finiteTree(value))return fail(error,"Invalid memory index object");
    double schema=1;
    if(!value.hasProperty("schema") || !readNumber(value,"schema",schema,1,std::numeric_limits<int>::max(),true))return fail(error,"Invalid memory index schema");
    next.source=value.clone();
    if(schema>1){next.forward=true;out=std::move(next);return true;}
    const auto& array=value["entries"];
    if(!array.isArray() || array.size()!=memoryCount)return fail(error,"Memory index requires99entries");
    for(int i=0;i<memoryCount;++i)
    {
        const auto& entry=array[i];if(entry.isVoid())continue;
        if(!entry.getDynamicObject() || !entry.hasProperty("settings") || !entry.hasProperty("looper"))return fail(error,"Invalid memory entry");
        auto& memory=next.entries[size_t(i)];
        if(!readSettings(entry["settings"],memory.settings))return fail(error,"Invalid memory settings");
        if(!Codec::decode(entry["looper"],memory.loop,error))return false;
        if(!memory.loop.forward && memory.loop.graph.status!=Status::Ready && memory.loop.graph.status!=Status::Empty)return fail(error,"Persisted memory must be Ready or Empty");
        memory.source=entry.clone();
    }
    if(!withinCapacity(next.entries,error))return false;
    out=std::move(next);return true;
}
bool Rc5MemoryBank::encode(V& out,juce::String* error) const
{
    if(forward){out=source.clone();return true;}
    V next=source.getDynamicObject()?source.clone():V(new juce::DynamicObject());
    V array=juce::Array<V>{};
    for(const auto& memory:entries)
    {
        if(memory.source.isVoid() && memory.loop.source.isVoid() && isDefault(memory.settings)){array.append(V());continue;}
        V entry=memory.source.getDynamicObject()?memory.source.clone():V(new juce::DynamicObject());
        V loop;if(!Codec::encode(memory.loop,memory.loop.graph,memory.loop.config,loop,error))return false;
        entry.getDynamicObject()->setProperty("settings",writeSettings(memory));entry.getDynamicObject()->setProperty("looper",loop);
        array.append(entry);
    }
    next.getDynamicObject()->setProperty("schema",1);next.getDynamicObject()->setProperty("entries",array);
    if(!finiteTree(next))return fail(error,"Invalid opaque memory fields");
    out=std::move(next);return true;
}
}
