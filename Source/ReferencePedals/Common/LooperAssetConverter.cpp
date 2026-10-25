// OWNER: WP15/WP16
#include "LooperAssetConverter.h"
#include "../Services/AssetStore.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace luthier::reference::common
{
namespace
{
using Core=PagedLooperCore;
constexpr double pi=3.1415926535897932384626433832795;
constexpr int taps=64,cachePages=5;
void put(std::vector<uint8_t>& bytes,uint64_t value,int count)
{for(int i=0;i<count;++i)bytes.push_back(uint8_t(value>>(8*i)));}
uint64_t get(const std::vector<uint8_t>& bytes,size_t at,int count)
{uint64_t value=0;for(int i=0;i<count;++i)value|=uint64_t(bytes[at+size_t(i)])<<(8*i);return value;}
bool configValid(const Core::Config& c)
{return std::isfinite(c.sampleRate) && c.sampleRate>=1 && c.sampleRate<=192000 && std::floor(c.sampleRate)==c.sampleRate && c.channels>=1 && c.channels<=2 && c.pageFrames>=16 && c.pageFrames<=65536;}
uint64_t scaled(uint64_t frames,double from,double to)
{return uint64_t(std::llround(double(frames)*to/from));}
bool dimensions(const Core::Config& from,const Core::Config& to,const Core::Graph& graph,uint64_t& frames)
{
    if(!configValid(from) || !configValid(to) || from.channels!=to.channels || graph.status!=Core::CaptureStatus::Ready
       || !graph.frames || graph.frames>uint64_t(from.sampleRate*5400) || graph.playhead>=graph.frames || graph.base.isNull()
       || graph.layerCount<0 || graph.layerCount>8 || graph.redoCount<0 || graph.redoCount>8 || graph.retiredCount<0 || graph.layerCount+graph.redoCount>8
       || graph.retiredCount>10 || graph.layerCount+graph.redoCount+graph.retiredCount>10
       || (graph.transport!=Core::State::Playing && graph.transport!=Core::State::Stopped)
       || !std::isfinite(to.seconds) || to.seconds<=0 || to.seconds>5400
       || to.sampleRate/from.sampleRate<.125 || to.sampleRate/from.sampleRate>8)return false;
    for(int i=0;i<graph.layerCount;++i)if(graph.layers[size_t(i)].isNull())return false;
    for(int i=0;i<graph.redoCount;++i)if(graph.redo[size_t(i)].isNull())return false;
    for(int i=0;i<graph.retiredCount;++i)if(graph.retired[size_t(i)].isNull())return false;
    frames=std::max(uint64_t(1),scaled(graph.frames,from.sampleRate,to.sampleRate));
    return frames<=uint64_t(to.sampleRate*to.seconds);
}
bool readVerified(IAssetStore& store,const AssetHash& hash,size_t maximum,std::vector<uint8_t>& bytes)
{
    const auto file=store.getBlobPath(hash);if(file==juce::File())return false;
    juce::FileInputStream input(file);const auto length=input.getTotalLength();
    if(!input.openedOk() || length<0 || uint64_t(length)>maximum)return false;
    bytes.resize(size_t(length));
    for(size_t at=0;at<bytes.size();)
    {
        const int n=int(std::min(size_t(65536),bytes.size()-at));
        if(input.read(bytes.data()+at,n)!=n)return false;
        at+=size_t(n);
    }
    return input.getStatus().wasOk() && AssetStore::hashBlob(bytes.data(),bytes.size())==hash;
}
struct StreamConverter
{
    struct Cache {std::vector<uint8_t> bytes;uint64_t index=UINT64_MAX,used=0;bool silent=false;};
    IAssetStore& store;const Core::Config& from;const Core::Config& to;uint64_t frames,outputFrames,serial=0;
    std::vector<uint8_t> manifest,output,encoded;std::array<Cache,cachePages> caches;
    StreamConverter(IAssetStore& s,const Core::Config& a,const Core::Config& b,uint64_t n,uint64_t m)
        :store(s),from(a),to(b),frames(n),outputFrames(m)
    {
        manifest.reserve(32+size_t((frames+uint64_t(from.pageFrames)-1)/uint64_t(from.pageFrames))*32);
        output.reserve(32+size_t((outputFrames+uint64_t(to.pageFrames)-1)/uint64_t(to.pageFrames))*32);
        encoded.reserve(20+size_t(to.pageFrames)*size_t(to.channels)*4);
        for(auto& cache:caches)cache.bytes.reserve(20+size_t(from.pageFrames)*size_t(from.channels)*4);
    }
    bool sample(int64_t position,int channel,double& value)
    {
        position%=int64_t(frames);if(position<0)position+=int64_t(frames);
        const uint64_t page=uint64_t(position)/uint64_t(from.pageFrames);Cache* selected=nullptr;
        for(auto& cache:caches)if(cache.index==page){selected=&cache;break;}
        if(!selected)
        {
            selected=&*std::min_element(caches.begin(),caches.end(),[](const Cache& a,const Cache& b){return a.used<b.used;});
            AssetHash hash;std::memcpy(hash.bytes.data(),manifest.data()+32+size_t(page)*32,32);
            selected->index=UINT64_MAX;selected->silent=hash.isNull();
            if(!selected->silent)
            {
                auto& bytes=selected->bytes;const auto count=std::min(uint64_t(from.pageFrames),frames-page*uint64_t(from.pageFrames));
                if(!readVerified(store,hash,20+size_t(from.pageFrames)*size_t(from.channels)*4,bytes)
                   || bytes.size()!=20+size_t(count)*size_t(from.channels)*4 || get(bytes,0,4)!=0x3150434c
                   || get(bytes,4,4)!=uint64_t(from.channels) || get(bytes,8,4)!=uint64_t(from.sampleRate) || get(bytes,12,8)!=count)return false;
                for(size_t i=20;i<bytes.size();i+=4){const uint32_t word=uint32_t(get(bytes,i,4));float x;std::memcpy(&x,&word,4);if(!std::isfinite(x))return false;}
            }
            selected->index=page;
        }
        selected->used=++serial;value=0;
        if(!selected->silent)
        {
            const size_t at=20+(size_t(uint64_t(position)%uint64_t(from.pageFrames))*size_t(from.channels)+size_t(channel))*4;
            const uint32_t word=uint32_t(get(selected->bytes,at,4));float x;std::memcpy(&x,&word,4);value=x;
        }
        return true;
    }
    bool run(const AssetHash& source,AssetHash& result)
    {
        const auto count=(frames+uint64_t(from.pageFrames)-1)/uint64_t(from.pageFrames);
        if(!readVerified(store,source,32+size_t(count)*32,manifest) || manifest.size()!=32+size_t(count)*32
           || get(manifest,0,4)!=0x314d434c || get(manifest,4,4)!=1 || get(manifest,8,4)!=uint64_t(from.channels)
           || get(manifest,12,4)!=uint64_t(from.sampleRate) || get(manifest,16,4)!=uint64_t(from.pageFrames)
           || get(manifest,20,8)!=frames || get(manifest,28,4)!=count)return false;
        for(auto& cache:caches){cache.index=UINT64_MAX;cache.used=0;}serial=0;
        if(from.sampleRate==to.sampleRate && from.pageFrames==to.pageFrames)
        {
            double value=0;for(uint64_t page=0;page<count;++page)if(!sample(int64_t(page*uint64_t(from.pageFrames)),0,value))return false;
            result=source;return true;
        }
        output.clear();put(output,0x314d434c,4);put(output,1,4);put(output,uint64_t(to.channels),4);put(output,uint64_t(to.sampleRate),4);
        put(output,uint64_t(to.pageFrames),4);put(output,outputFrames,8);put(output,(outputFrames+uint64_t(to.pageFrames)-1)/uint64_t(to.pageFrames),4);
        const double cutoff=.45*std::min(1.,to.sampleRate/from.sampleRate);
        for(uint64_t start=0;start<outputFrames;start+=uint64_t(to.pageFrames))
        {
            const auto n=std::min(uint64_t(to.pageFrames),outputFrames-start);encoded.clear();
            put(encoded,0x3150434c,4);put(encoded,uint64_t(to.channels),4);put(encoded,uint64_t(to.sampleRate),4);put(encoded,n,8);
            for(uint64_t j=0;j<n;++j)
            {
                const double position=double(start+j)*from.sampleRate/to.sampleRate;const auto first=int64_t(std::floor(position))-31;
                std::array<double,taps> weights{};double total=0;
                for(int k=0;k<taps;++k)
                {
                    const double distance=double(first+k)-position,argument=2*cutoff*distance;
                    const double window=.42+.5*std::cos(pi*distance/32.)+.08*std::cos(2*pi*distance/32.);
                    weights[size_t(k)]=2*cutoff*(std::abs(argument)<1e-15?1.:std::sin(pi*argument)/(pi*argument))*window;total+=weights[size_t(k)];
                }
                if(!std::isfinite(total) || std::abs(total)<1e-12)return false;
                for(int channel=0;channel<to.channels;++channel)
                {
                    double sum=0,x=0;for(int k=0;k<taps;++k){if(!sample(first+k,channel,x))return false;sum+=weights[size_t(k)]*x;}
                    const float value=float(sum/total);if(!std::isfinite(value))return false;
                    uint32_t word;std::memcpy(&word,&value,4);put(encoded,word,4);
                }
            }
            const auto hash=AssetStore::hashBlob(encoded.data(),encoded.size());if(!store.putBlob(hash,encoded.data(),encoded.size()))return false;
            output.insert(output.end(),hash.bytes.begin(),hash.bytes.end());
        }
        const auto hash=AssetStore::hashBlob(output.data(),output.size());if(!store.putBlob(hash,output.data(),output.size()))return false;
        result=hash;return true;
    }
};
}
bool LooperAssetConverter::reservation(const Core::Config& source,const Core::Config& target,const Core::Graph& graph,size_t& ram,uint64_t& storage) noexcept
{
    uint64_t frames=0;if(!dimensions(source,target,graph,frames))return false;
    const auto sourcePages=(graph.frames+uint64_t(source.pageFrames)-1)/uint64_t(source.pageFrames);
    const auto targetPages=(frames+uint64_t(target.pageFrames)-1)/uint64_t(target.pageFrames);
    const uint64_t bytes=sizeof(StreamConverter)+65536+64+(sourcePages+targetPages)*32
        +uint64_t(cachePages+1)*(20+uint64_t(source.pageFrames)*uint64_t(source.channels)*4)
        +20+uint64_t(target.pageFrames)*uint64_t(target.channels)*4;
    const uint64_t disk=(frames*uint64_t(target.channels)*4+targetPages*52+32)*uint64_t(1+graph.layerCount+graph.redoCount+graph.retiredCount);
    if(bytes>std::numeric_limits<size_t>::max() || bytes>target.ramBudget || disk>target.storageBudget)return false;
    ram=size_t(bytes);storage=disk;return true;
}
bool LooperAssetConverter::convert(IAssetStore& store,const Core::Config& source,const Core::Config& target,const Core::Graph& graph,Core::Graph& out,juce::String* error)
{
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    size_t ram=0;uint64_t storage=0;if(!reservation(source,target,graph,ram,storage))return fail("Looper conversion format, duration or aggregate budget rejected");
    try
    {
        Core::Graph next=graph;next.frames=std::max(uint64_t(1),scaled(graph.frames,source.sampleRate,target.sampleRate));next.playhead=scaled(graph.playhead,source.sampleRate,target.sampleRate)%next.frames;
        StreamConverter converter(store,source,target,graph.frames,next.frames);
        if(!converter.run(graph.base,next.base))return fail("Looper conversion asset read or write failed");
        for(int i=0;i<graph.layerCount;++i)if(!converter.run(graph.layers[size_t(i)],next.layers[size_t(i)]))return fail("Looper active-layer conversion failed");
        for(int i=0;i<graph.redoCount;++i)if(!converter.run(graph.redo[size_t(i)],next.redo[size_t(i)]))return fail("Looper redo-layer conversion failed");
        for(int i=0;i<graph.retiredCount;++i)if(!converter.run(graph.retired[size_t(i)],next.retired[size_t(i)]))return fail("Looper retired-layer conversion failed");
        out=next;return true;
    }
    catch(...){return fail("Looper conversion resource failure");}
}
}
