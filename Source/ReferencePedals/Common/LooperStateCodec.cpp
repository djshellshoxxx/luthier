// OWNER: WP15/WP16
#include "LooperStateCodec.h"
#include <cmath>
#include <limits>
namespace luthier::reference::common
{
namespace
{
using V=juce::var;
using Status=PagedLooperCore::CaptureStatus;
bool fail(juce::String* error,const char* text){if(error)*error=text;return false;}
bool numeric(const V& v){return v.isInt() || v.isInt64() || v.isDouble();}
bool finiteTree(const V& v,int depth=0)
{
    if(depth>64)return false;
    if(v.isDouble())return std::isfinite(double(v));
    if(const auto* array=v.getArray()){for(const auto& child:*array)if(!finiteTree(child,depth+1))return false;return true;}
    if(const auto* object=v.getDynamicObject()){for(const auto& child:object->getProperties())if(!finiteTree(child.value,depth+1))return false;return true;}
    return v.isVoid() || v.isBool() || v.isInt() || v.isInt64() || v.isString();
}
bool integer(const V& root,const char* key,uint64_t& value,uint64_t low,uint64_t high,bool required=true)
{
    if(!root.hasProperty(key))return !required;
    const auto& v=root[key];const double n=double(v);
    if(!numeric(v) || !std::isfinite(n) || std::floor(n)!=n || n<double(low) || n>double(high))return false;
    // int64 avoids converting a rounded 2^63 double to a signed integer.
    if(v.isInt64()){const auto signedValue=juce::int64(v);if(signedValue<0 || uint64_t(signedValue)<low || uint64_t(signedValue)>high)return false;value=uint64_t(signedValue);return true;}
    if(n>=9223372036854775808.)return false;
    value=uint64_t(n);return value>=low && value<=high;
}
bool hash(const V& value,AssetHash& out)
{
    if(!value.isString())return false;
    const auto text=value.toString();if(!text.startsWith("sha256:") || text.length()!=71)return false;
    const auto hex=text.substring(7);
    for(auto c:hex)if(!((c>='0'&&c<='9') || (c>='a'&&c<='f') || (c>='A'&&c<='F')))return false;
    const auto next=AssetHash::fromHex(hex);if(next.isNull())return false;out=next;return true;
}
bool hashes(const V& root,const char* key,AssetHash* out,int& count,int max)
{
    const auto& value=root[key];if(!value.isArray() || value.size()>max)return false;
    count=value.size();for(int i=0;i<count;++i)if(!hash(value[i],out[size_t(i)]))return false;return true;
}
V encodedHashes(const AssetHash* hashes,int count)
{
    V out=juce::Array<V>{};for(int i=0;i<count;++i)out.append("sha256:"+hashes[size_t(i)].toHex());return out;
}
const char* statusText(Status status)
{
    switch(status){case Status::Empty:return "empty";case Status::Recording:return "recording";case Status::Pending:return "pending";case Status::Ready:return "ready";case Status::Failed:return "failed";}
    return nullptr;
}
bool status(const V& value,Status& out)
{
    if(!value.isString())return false;
    for(auto candidate:{Status::Empty,Status::Recording,Status::Pending,Status::Ready,Status::Failed})if(value.toString()==statusText(candidate)){out=candidate;return true;}
    return false;
}
}
bool LooperStateCodec::decode(const V& value,Stored& out,juce::String* error)
{
    if(value.isVoid()){out=Stored{};return true;}
    if(!value.getDynamicObject() || !finiteTree(value))return fail(error,"Invalid looper state object");
    Stored next;uint64_t schema=0;
    if(!integer(value,"schema",schema,1,uint64_t(std::numeric_limits<int>::max())))return fail(error,"Invalid looper schema");
    next.source=value.clone();
    if(schema>2){next.forward=true;out=std::move(next);return true;}
    uint64_t rate=0,channels=0,pageFrames=0,frames=0,generation=0,faults=0,depth=1;
    if(!integer(value,"sampleRate",rate,1,192000) || !integer(value,"channels",channels,1,2)
       || !integer(value,"pageFrames",pageFrames,16,65536) || !integer(value,"frames",frames,0,uint64_t(192000)*5400)
       || !integer(value,"generation",generation,0,uint64_t(std::numeric_limits<juce::int64>::max()))
       || !integer(value,"faults",faults,0,std::numeric_limits<uint32_t>::max()) || !integer(value,"undoDepth",depth,1,8,false)
       || !status(value["status"],next.graph.status))return fail(error,"Invalid looper fields");
    next.config.sampleRate=double(rate);next.config.channels=int(channels);next.config.pageFrames=int(pageFrames);next.config.undoDepth=int(depth);
    next.graph.frames=frames;next.graph.generation=generation;next.graph.faults=uint32_t(faults);
    if(!hashes(value,"layers",next.graph.layers.data(),next.graph.layerCount,int(next.graph.layers.size()))
       || !hashes(value,"retired",next.graph.retired.data(),next.graph.retiredCount,int(next.graph.retired.size()))
       || (schema==2 && !hashes(value,"redo",next.graph.redo.data(),next.graph.redoCount,int(next.graph.redo.size())))
       || next.graph.layerCount+next.graph.redoCount>int(depth))return fail(error,"Invalid looper layer assets");
    if(next.graph.status==Status::Ready)
    {
        if(!frames || !hash(value["base"],next.graph.base))return fail(error,"Ready looper requires complete assets");
    }
    else if(!value["base"].isVoid() || next.graph.layerCount || next.graph.redoCount || next.graph.retiredCount || (next.graph.status==Status::Empty && frames))
        return fail(error,"Unfinished looper cannot claim completed assets");
    out=std::move(next);return true;
}
bool LooperStateCodec::encode(const Stored& stored,const PagedLooperCore::Graph& graph,const PagedLooperCore::Config& config,V& out,juce::String* error)
{
    if(stored.forward)
    {
        if(!stored.source.getDynamicObject() || !finiteTree(stored.source))return fail(error,"Invalid forward looper state");
        out=stored.source.clone();return true;
    }
    if(!statusText(graph.status) || graph.frames>uint64_t(192000)*5400 || graph.generation>uint64_t(std::numeric_limits<juce::int64>::max())
       || !std::isfinite(config.sampleRate) || std::floor(config.sampleRate)!=config.sampleRate || config.sampleRate<1 || config.sampleRate>192000
       || config.channels<1 || config.channels>2 || config.pageFrames<16 || config.pageFrames>65536 || config.undoDepth<1 || config.undoDepth>8
       || graph.layerCount<0 || graph.layerCount>config.undoDepth || graph.redoCount<0 || graph.redoCount>8 || graph.layerCount+graph.redoCount>config.undoDepth || graph.retiredCount<0 || graph.retiredCount>2)return fail(error,"Invalid looper capture");
    if(graph.status==Status::Ready)
    {
        if(!graph.frames || graph.base.isNull())return fail(error,"Ready looper requires complete assets");
        for(int i=0;i<graph.layerCount;++i)if(graph.layers[size_t(i)].isNull())return fail(error,"Missing looper layer hash");
        for(int i=0;i<graph.retiredCount;++i)if(graph.retired[size_t(i)].isNull())return fail(error,"Missing retired looper hash");
        for(int i=0;i<graph.redoCount;++i)if(graph.redo[size_t(i)].isNull())return fail(error,"Missing redo looper hash");
    }
    else if(!graph.base.isNull() || graph.layerCount || graph.redoCount || graph.retiredCount || (graph.status==Status::Empty && graph.frames))return fail(error,"Unfinished looper cannot claim completed assets");
    V next=stored.source.getDynamicObject()?stored.source.clone():V(new juce::DynamicObject());
    auto* object=next.getDynamicObject();
    object->setProperty("schema",2);object->setProperty("status",statusText(graph.status));object->setProperty("sampleRate",config.sampleRate);
    object->setProperty("channels",config.channels);object->setProperty("pageFrames",config.pageFrames);object->setProperty("undoDepth",config.undoDepth);
    object->setProperty("frames",juce::int64(graph.frames));object->setProperty("generation",juce::int64(graph.generation));object->setProperty("faults",juce::int64(graph.faults));
    object->setProperty("base",graph.status==Status::Ready?V("sha256:"+graph.base.toHex()):V());
    object->setProperty("layers",encodedHashes(graph.layers.data(),graph.layerCount));object->setProperty("retired",encodedHashes(graph.retired.data(),graph.retiredCount));
    object->setProperty("redo",encodedHashes(graph.redo.data(),graph.redoCount));
    if(!finiteTree(next))return fail(error,"Invalid opaque looper fields");
    out=std::move(next);return true;
}
}
