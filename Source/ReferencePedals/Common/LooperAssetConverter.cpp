// OWNER: WP15/WP16
#include "LooperAssetConverter.h"
#include "../Services/AssetStore.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
namespace luthier::reference::common
{
namespace
{
using Core=PagedLooperCore;
constexpr double pi=3.1415926535897932384626433832795;
struct Kernel
{
    uint64_t numerator=1,phases=1;
    int half=0,taps=1,cachePages=1;
    double cutoff=.5;
};
Kernel kernel(const Core::Config& from,const Core::Config& to,uint64_t frames)
{
    Kernel result;
    const auto divisor=std::gcd(uint64_t(from.sampleRate),uint64_t(to.sampleRate));
    result.numerator=uint64_t(from.sampleRate)/divisor;result.phases=uint64_t(to.sampleRate)/divisor;
    if(from.sampleRate!=to.sampleRate)
    {
        const double minimum=std::min(from.sampleRate,to.sampleRate);
        const double pass=minimum>=44100?20000.:.45*minimum,stop=.5*minimum;
        // Kaiser design estimate for180dB, with25% length margin. These are
        // engineering design inputs; the original T8 limits are measured.
        const double length=1.25*(180.-8.)/(2.285*2*pi*(stop-pass)/from.sampleRate);
        result.half=int(std::ceil(length*.5));result.taps=2*result.half+1;
        result.cutoff=.5*(pass+stop)/from.sampleRate;
    }
    const auto pages=(frames+uint64_t(from.pageFrames)-1)/uint64_t(from.pageFrames);
    result.cachePages=int(std::min(pages,uint64_t((result.taps+from.pageFrames-1)/from.pageFrames+2)));
    return result;
}
double besselZero(double x)
{
    const double square=x*x*.25;double sum=1,term=1;
    for(int k=1;k<=64;++k){term*=square/double(k*k);sum+=term;if(term<sum*1e-17)break;}
    return sum;
}
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
    struct Cache {std::vector<float> pcm;uint64_t index=UINT64_MAX,used=0;};
    IAssetStore& store;const Core::Config& from;const Core::Config& to;uint64_t frames,outputFrames,serial=0;
    Kernel design;
    std::vector<uint8_t> manifest,output,encoded,input;
    std::vector<Cache> caches;std::vector<double> coefficients;
    StreamConverter(IAssetStore& s,const Core::Config& a,const Core::Config& b,uint64_t n,uint64_t m)
        :store(s),from(a),to(b),frames(n),outputFrames(m),design(kernel(a,b,n))
    {
        manifest.reserve(32+size_t((frames+uint64_t(from.pageFrames)-1)/uint64_t(from.pageFrames))*32);
        output.reserve(32+size_t((outputFrames+uint64_t(to.pageFrames)-1)/uint64_t(to.pageFrames))*32);
        encoded.reserve(20+size_t(to.pageFrames)*size_t(to.channels)*4);
        input.reserve(20+size_t(from.pageFrames)*size_t(from.channels)*4);
        caches.resize(size_t(design.cachePages));
        for(auto& cache:caches)cache.pcm.resize(size_t(from.pageFrames)*size_t(from.channels));
        coefficients.resize(size_t(design.phases)*size_t(design.taps));
        const double divisor=besselZero(19.);
        for(uint64_t phase=0;phase<design.phases;++phase)
        {
            double* row=coefficients.data()+size_t(phase)*size_t(design.taps);double total=0;
            for(int k=0;k<design.taps;++k)
            {
                if(!design.half){row[k]=1;total=1;continue;}
                const double distance=double(k-design.half)-double(phase)/double(design.phases);
                if(std::abs(distance)>double(design.half)){row[k]=0;continue;}
                const double radius=distance/double(design.half),argument=2*design.cutoff*distance;
                const double window=besselZero(19.*std::sqrt(std::max(0.,1.-radius*radius)))/divisor;
                row[k]=2*design.cutoff*(std::abs(argument)<1e-15?1.:std::sin(pi*argument)/(pi*argument))*window;total+=row[k];
            }
            if(!std::isfinite(total) || std::abs(total)<1e-12)throw std::runtime_error("Invalid looper conversion kernel");
            for(int k=0;k<design.taps;++k)row[k]/=total;
        }
    }
    const float* samples(int64_t position,uint64_t& available)
    {
        position%=int64_t(frames);if(position<0)position+=int64_t(frames);
        const uint64_t page=uint64_t(position)/uint64_t(from.pageFrames),offset=uint64_t(position)%uint64_t(from.pageFrames);
        Cache* selected=nullptr;
        for(auto& cache:caches)if(cache.index==page){selected=&cache;break;}
        if(!selected)
        {
            selected=&*std::min_element(caches.begin(),caches.end(),[](const Cache& a,const Cache& b){return a.used<b.used;});
            AssetHash hash;std::memcpy(hash.bytes.data(),manifest.data()+32+size_t(page)*32,32);
            selected->index=UINT64_MAX;
            const auto count=std::min(uint64_t(from.pageFrames),frames-page*uint64_t(from.pageFrames));
            if(hash.isNull())std::fill(selected->pcm.begin(),selected->pcm.end(),0.f);
            else
            {
                if(!readVerified(store,hash,20+size_t(from.pageFrames)*size_t(from.channels)*4,input)
                   || input.size()!=20+size_t(count)*size_t(from.channels)*4 || get(input,0,4)!=0x3150434c
                   || get(input,4,4)!=uint64_t(from.channels) || get(input,8,4)!=uint64_t(from.sampleRate) || get(input,12,8)!=count)return nullptr;
                for(size_t i=0;i<size_t(count)*size_t(from.channels);++i)
                {
                    const uint32_t word=uint32_t(get(input,20+i*4,4));float x;std::memcpy(&x,&word,4);
                    if(!std::isfinite(x))return nullptr;
                    selected->pcm[i]=x;
                }
            }
            selected->index=page;
        }
        selected->used=++serial;available=std::min(uint64_t(from.pageFrames)-offset,frames-uint64_t(position));
        return selected->pcm.data()+size_t(offset)*size_t(from.channels);
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
            uint64_t available=0;for(uint64_t page=0;page<count;++page)if(!samples(int64_t(page*uint64_t(from.pageFrames)),available))return false;
            result=source;return true;
        }
        output.clear();put(output,0x314d434c,4);put(output,1,4);put(output,uint64_t(to.channels),4);put(output,uint64_t(to.sampleRate),4);
        put(output,uint64_t(to.pageFrames),4);put(output,outputFrames,8);put(output,(outputFrames+uint64_t(to.pageFrames)-1)/uint64_t(to.pageFrames),4);
        for(uint64_t start=0;start<outputFrames;start+=uint64_t(to.pageFrames))
        {
            const auto n=std::min(uint64_t(to.pageFrames),outputFrames-start);encoded.clear();
            put(encoded,0x3150434c,4);put(encoded,uint64_t(to.channels),4);put(encoded,uint64_t(to.sampleRate),4);put(encoded,n,8);
            for(uint64_t j=0;j<n;++j)
            {
                // Integral rate ratios avoid accumulating fractional phase error
                // during long recordings; the product is bounded by validation.
                const uint64_t numerator=(start+j)*design.numerator,phase=numerator%design.phases;
                const auto first=int64_t(numerator/design.phases)-design.half;
                const double* weights=coefficients.data()+size_t(phase)*size_t(design.taps);
                double sums[2][4]{};
                for(int k=0;k<design.taps;)
                {
                    uint64_t available=0;const auto* pcm=samples(first+k,available);if(!pcm)return false;
                    const auto chunk=std::min(uint64_t(design.taps-k),available);size_t i=0;
                    for(;i+4<=chunk;i+=4)
                        for(int channel=0;channel<to.channels;++channel)
                            for(size_t lane=0;lane<4;++lane)
                                sums[channel][lane]+=weights[size_t(k)+i+lane]*pcm[(i+lane)*size_t(to.channels)+size_t(channel)];
                    for(;i<chunk;++i)for(int channel=0;channel<to.channels;++channel)sums[channel][0]+=weights[size_t(k)+i]*pcm[i*size_t(to.channels)+size_t(channel)];
                    k+=int(chunk);
                }
                for(int channel=0;channel<to.channels;++channel)
                {
                    const float value=float((sums[channel][0]+sums[channel][1])+(sums[channel][2]+sums[channel][3]));
                    if(!std::isfinite(value))return false;
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
    const auto design=kernel(source,target,graph.frames);
    const uint64_t bytes=sizeof(StreamConverter)+65536+64+(sourcePages+targetPages)*32
        +uint64_t(design.cachePages)*(sizeof(StreamConverter::Cache)+uint64_t(source.pageFrames)*uint64_t(source.channels)*4)
        +20+uint64_t(source.pageFrames)*uint64_t(source.channels)*4
        +20+uint64_t(target.pageFrames)*uint64_t(target.channels)*4
        +design.phases*uint64_t(design.taps)*sizeof(double);
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
