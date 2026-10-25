// Explicit long-running native acceptance fixture; not part of the default
// unit-test registry. The caller supplies one newly created owned directory.
#pragma once
#include "../../../ReferencePedals/Models/tc_ditto.h"
#include "../../../ReferencePedals/Common/LooperStateCodec.h"
#include "../../../ReferencePedals/Common/LooperAssetConverter.h"
#include "../../../ReferencePedals/Services/AssetStore.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#if JUCE_LINUX
#include <sys/stat.h>
#endif
namespace luthier::reference::acceptance
{
using Clock=std::chrono::steady_clock;
inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
inline double elapsed(Clock::time_point start){return std::chrono::duration<double>(Clock::now()-start).count();}
inline void integer(std::vector<uint8_t>& out,uint64_t value,int n){for(int i=0;i<n;++i)out.push_back(uint8_t(value>>(8*i)));}
inline uint64_t integer(const std::vector<uint8_t>& in,size_t at,int n){uint64_t value=0;for(int i=0;i<n;++i)value|=uint64_t(in[at+size_t(i)])<<(8*i);return value;}
class ObservedStore final : public IAssetStore
{
public:
    explicit ObservedStore(const juce::File& directory):real(directory),directory(directory){}
    bool putBlob(const AssetHash& hash,const uint8_t* data,size_t size) override
    {
        const auto attempt=++attempts;if(failAt.load()>0 && attempt>=failAt.load())return false;
        if(!real.putBlob(hash,data,size))return false;
        {const std::lock_guard<std::mutex> guard(mutex);if(unique.insert(hash.toHex().toStdString()).second){physicalBytes+=size;maxBlob=std::max(maxBlob,size);}if(size>=4 && data[0]=='L' && data[1]=='C' && data[2]=='P' && data[3]=='1')++pageWrites;}
        completed.notify_all();return true;
    }
    bool hasBlob(const AssetHash& h) const override{return real.hasBlob(h);}
    juce::File getBlobPath(const AssetHash& h) const override{return real.getBlobPath(h);}
    void putBlobAsync(const AssetHash& h,AssetBlob b,std::function<void(bool)> done) override{real.putBlobAsync(h,std::move(b),std::move(done));}
    void getBlobAsync(const AssetHash& h,std::function<void(AssetBlob)> done) override{real.getBlobAsync(h,std::move(done));}
    std::vector<AssetHash> findMissing(const std::vector<AssetHash>& h) const override{return real.findMissing(h);}
    bool waitPages(uint64_t count){std::unique_lock<std::mutex> guard(mutex);return completed.wait_for(guard,std::chrono::seconds(10),[&]{return pageWrites>=count;});}
    uint64_t bytes() const{const std::lock_guard<std::mutex> guard(mutex);return physicalBytes;}
    uint64_t maximumTemporaryBlob() const{const std::lock_guard<std::mutex> guard(mutex);return maxBlob;}
    void verifyDisk() const
    {
        uint64_t payload=0,allocated=0,files=0;
        for(const auto& entry:juce::RangedDirectoryIterator(directory,false,"*",juce::File::findFiles))
        {
            const auto file=entry.getFile();payload+=uint64_t(file.getSize());++files;
#if JUCE_LINUX
            struct stat st{};require(::stat(file.getFullPathName().toRawUTF8(),&st)==0,"cannot stat owned test asset");allocated+=uint64_t(st.st_blocks)*512;
#endif
        }
        require(payload==bytes(),"tracked file payload differs from real storage");
        std::cout<<"real storage files="<<files<<" payloadBytes="<<payload<<" allocatedBytesLinux="<<allocated<<std::endl;
    }
    AssetStore real;std::atomic<int64_t> attempts{0},failAt{-1};
private:
    juce::File directory;mutable std::mutex mutex;std::condition_variable completed;uint64_t pageWrites=0,physicalBytes=0;size_t maxBlob=0;std::unordered_set<std::string> unique;
};
inline float signal(uint64_t frame,uint64_t rate,bool layer=false)
{
    uint64_t bits=frame+(layer?0x12345678ull:0x87654321ull);bits=(bits^(bits>>30))*0xbf58476d1ce4e5b9ull;bits=(bits^(bits>>27))*0x94d049bb133111ebull;bits^=bits>>31;
    const double noise=(double(bits&0xffff)/65535.-.5)*.00001;
    return float((layer?.03125:.125)*std::sin(6.283185307179586476925286766559*(layer?4000.:1000.)*double(frame%rate)/double(rate))+noise);
}
inline std::unique_ptr<IPedal> pedal(ObservedStore& store,double rate)
{
    auto result=models::makeTcDitto();PedalServices services;services.assets=&store;services.sampleRate=rate;services.maxBlock=512;result->setServices(services);result->prepare(rate,512,0);return result;
}
inline bool save(IPedal& p,juce::var& out)
{
    const auto start=Clock::now();do{p.process(nullptr,nullptr,0);p.saveExtra(out);if(out["looper"]["status"].toString()=="ready")return true;std::this_thread::yield();}while(elapsed(start)<10);return false;
}
inline juce::var capacity(IPedal& p,ObservedStore& store,uint64_t rate)
{
    const auto start=Clock::now();const uint64_t limit=rate*300;std::array<double,512> left{},right{};FootEvent foot;foot.press=true;p.onFoot(foot);bool released=false;uint64_t frame=0,nextReport=rate*60;
    while(frame<limit)
    {
        if(!released && frame>=rate/50){foot.press=false;p.onFoot(foot);released=true;}
        uint64_t n=std::min({uint64_t(left.size()),limit-frame,uint64_t(4096)-frame%4096});if(frame<limit-1)n=std::min(n,limit-1-frame);
        for(uint64_t i=0;i<n;++i)left[size_t(i)]=right[size_t(i)]=signal(frame+i,rate);
        p.process(left.data(),right.data(),int(n));frame+=n;
        if(frame%4096==0 || frame==limit)require(store.waitPages((frame+4095)/4096),"capacity worker did not finish a queued PCM page");
        if(frame==limit-1){require(p.readUiMeters().transport==1,"capacity closed before the final sample");require(p.readUiMeters().loopLength==limit-1,"pre-capacity frame count differs");}
        if(frame>=nextReport){std::cout<<"T1 progress rate="<<rate<<" frame="<<frame<<" elapsed="<<elapsed(start)<<std::endl;nextReport+=rate*60;}
        require(elapsed(start)<180,"capacity fixture exceeded its finite180s offline deadline");
    }
    const auto meters=p.readUiMeters();require(meters.transport==2,"capacity did not auto-close to PLAYING");require(meters.loopLength==limit,"capacity length differs from300*sampleRate");require(meters.faults==0,"recording reached capacity with an IO/queue fault");
    juce::var state;require(save(p,state),"capacity recording did not publish complete assets");require(juce::int64(state["looper"]["frames"])==juce::int64(limit),"saved capacity length differs");
    // A fully restored copy primes the real read-ahead window off audio.
    auto copy=pedal(store,double(rate));copy->loadExtra(state);double l=0,r=0;copy->process(&l,&r,1);const double gain=std::pow(10.,(-60+66*double(models::tcDittoDescription().params[0].defaultNormalised()))/20.);
    require(std::abs(l-double(signal(0,rate))*gain)<1e-12 && r==l,"full-capacity asset does not play its first sample");
    l=r=0;p.process(&l,&r,1);require(p.readUiMeters().loopLength==limit && p.readUiMeters().transport==2,"300s+1sample extended recording instead of playing");
    store.verifyDisk();std::cout<<"T1 PASS rate="<<rate<<" frames="<<limit<<" elapsed="<<elapsed(start)<<" actualStoreBytes="<<store.bytes()<<" base="<<state["looper"]["base"].toString()<<std::endl;return state;
}
inline AssetHash layer(ObservedStore& store,uint64_t rate,uint64_t frames)
{
    std::vector<uint8_t> manifest;integer(manifest,0x314d434c,4);integer(manifest,1,4);integer(manifest,1,4);integer(manifest,rate,4);integer(manifest,4096,4);integer(manifest,frames,8);integer(manifest,(frames+4095)/4096,4);
    for(uint64_t first=0;first<frames;first+=4096)
    {
        const auto n=std::min(uint64_t(4096),frames-first);std::vector<uint8_t> page;page.reserve(20+size_t(n)*4);integer(page,0x3150434c,4);integer(page,1,4);integer(page,rate,4);integer(page,n,8);
        for(uint64_t i=0;i<n;++i){const float sample=signal(first+i,rate,true);uint32_t word;std::memcpy(&word,&sample,4);integer(page,word,4);}const auto hash=AssetStore::hashBlob(page.data(),page.size());require(store.putBlob(hash,page.data(),page.size()),"full undo layer write failed");manifest.insert(manifest.end(),hash.bytes.begin(),hash.bytes.end());
    }
    const auto hash=AssetStore::hashBlob(manifest.data(),manifest.size());require(store.putBlob(hash,manifest.data(),manifest.size()),"full undo manifest write failed");return hash;
}
inline std::vector<uint8_t> blob(ObservedStore& store,const AssetHash& hash)
{
    juce::MemoryBlock data;require(store.getBlobPath(hash).loadFileAsData(data),"converted asset missing");const auto* first=static_cast<const uint8_t*>(data.getData());return {first,first+data.getSize()};
}
inline float value(ObservedStore& store,const AssetHash& manifestHash,uint64_t frame)
{
    const auto manifest=blob(store,manifestHash);AssetHash page;std::memcpy(page.bytes.data(),manifest.data()+32+size_t(frame/4096)*32,32);const auto pcm=blob(store,page);const uint32_t word=uint32_t(integer(pcm,20+size_t(frame%4096)*4,4));float result;std::memcpy(&result,&word,4);return result;
}
inline void fullConversion(IPedal& source,ObservedStore& store,juce::var original)
{
    using Codec=common::LooperStateCodec;using Converter=common::LooperAssetConverter;
    Codec::Stored decoded;require(Codec::decode(original["looper"],decoded),"full source state decode failed");decoded.graph.layerCount=1;decoded.graph.layers[0]=layer(store,44100,decoded.graph.frames);
    juce::var looper;require(Codec::encode(decoded,decoded.graph,decoded.config,looper),"full source undo encode failed");original.getDynamicObject()->setProperty("looper",looper);
    auto target=pedal(store,192000);const auto start=Clock::now();std::cout<<"full conversion START actualStoreBytes="<<store.bytes()<<std::endl;target->loadExtra(original);juce::var converted;require(save(*target,converted),"full conversion did not yield Ready state");
    Codec::Stored result;require(Codec::decode(converted["looper"],result),"converted state decode failed");require(result.graph.frames==57600000 && result.graph.layerCount==1 && result.config.sampleRate==192000,"full conversion duration/history differs");require(target->readUiMeters().transport==2,"full conversion lost PLAYING state");
    const auto before=result.graph;const auto attempts=store.attempts.load();store.failAt=attempts+3;bool rejected=false;try{target->loadExtra(original);}catch(const std::runtime_error&){rejected=true;}store.failAt=-1;require(rejected,"full-state store failure was accepted");require(save(*target,converted),"old full loop was not retained after store failure");require(Codec::decode(converted["looper"],result),"rollback state decode failed");require(result.graph.base==before.base && result.graph.layers[0]==before.layers[0] && result.graph.frames==before.frames && result.graph.playhead==before.playhead && result.graph.transport==before.transport && result.graph.layerCount==before.layerCount && result.graph.redoCount==before.redoCount && result.graph.retiredCount==before.retiredCount,"failed full conversion changed previous asset graph or transport");
    common::PagedLooperCore::Config targetConfig;targetConfig.sampleRate=192000;targetConfig.channels=1;size_t ram=0;uint64_t disk=0;require(Converter::reservation(decoded.config,targetConfig,decoded.graph,ram,disk),"full conversion reservation failed");
    store.verifyDisk();std::cout<<"full conversion PASS elapsed="<<elapsed(start)<<" converterScratch="<<ram<<" convertedReservation="<<disk<<" actualCommittedStoreBytes="<<store.bytes()<<" plusAtMostOneTemporaryBlob="<<store.maximumTemporaryBlob()<<" base="<<before.base.toHex()<<" undo="<<before.layers[0].toHex()<<std::endl;
    for(auto frame:{uint64_t(0),uint64_t(1),uint64_t(4095),uint64_t(4096),uint64_t(777777),uint64_t(28800017),uint64_t(57599998),uint64_t(57599999)})
        std::cout<<"long sample frame="<<frame<<" base="<<std::setprecision(17)<<value(store,before.base,frame)<<" undo="<<value(store,before.layers[0],frame)<<std::endl;
    require(source.readUiMeters().loopLength==13230000,"source loop changed during conversion");
}
inline int runDittoLongAcceptance(const juce::File& ownedDirectory,bool convert)
{
    std::cout.setf(std::ios::unitbuf);require(!ownedDirectory.exists(),"acceptance directory must be newly owned");require(ownedDirectory.createDirectory().wasOk(),"cannot create acceptance directory");
    for(uint64_t rate:{44100ull,48000ull,88200ull,96000ull,192000ull})
    {
        const auto path=ownedDirectory.getChildFile(juce::String(juce::int64(rate)));
        {ObservedStore store(path);auto p=pedal(store,double(rate));const auto state=capacity(*p,store,rate);if(convert && rate==44100)fullConversion(*p,store,state);}
        require(path.deleteRecursively(),"acceptance temp asset cleanup failed");
    }
    require(ownedDirectory.deleteRecursively(),"acceptance directory cleanup failed");return 0;
}
}
