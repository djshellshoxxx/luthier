// OWNER: WP15/WP16
#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperAssetConverter.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
using namespace luthier::reference;
using namespace luthier::reference::common;
namespace
{
void integer(std::vector<uint8_t>& bytes,uint64_t value,int n){for(int i=0;i<n;++i)bytes.push_back(uint8_t(value>>(i*8)));}
uint64_t integer(const std::vector<uint8_t>& bytes,size_t at,int n){uint64_t value=0;for(int i=0;i<n;++i)value|=uint64_t(bytes[at+size_t(i)])<<(i*8);return value;}
struct ConversionFixture final : IAssetStore
{
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("looper-rate-"+juce::Uuid().toString());AssetStore real{path};bool failWrites=false;int writes=0;
    ~ConversionFixture(){path.deleteRecursively();}
    bool putBlob(const AssetHash& h,const uint8_t* data,size_t n) override{++writes;return !failWrites && real.putBlob(h,data,n);}
    bool hasBlob(const AssetHash& h) const override{return real.hasBlob(h);}
    juce::File getBlobPath(const AssetHash& h) const override{return real.getBlobPath(h);}
    void putBlobAsync(const AssetHash& h,AssetBlob b,std::function<void(bool)> done) override{real.putBlobAsync(h,std::move(b),std::move(done));}
    void getBlobAsync(const AssetHash& h,std::function<void(AssetBlob)> done) override{real.getBlobAsync(h,std::move(done));}
    std::vector<AssetHash> findMissing(const std::vector<AssetHash>& h) const override{return real.findMissing(h);}
    AssetHash blob(const std::vector<uint8_t>& bytes){const auto hash=AssetStore::hashBlob(bytes.data(),bytes.size());if(!putBlob(hash,bytes.data(),bytes.size()))throw std::runtime_error("fixture write");return hash;}
    std::vector<uint8_t> bytes(const AssetHash& h){juce::MemoryBlock b;real.getBlobPath(h).loadFileAsData(b);const auto* data=static_cast<const uint8_t*>(b.getData());return {data,data+b.getSize()};}
    AssetHash audio(const PagedLooperCore::Config& c,const std::vector<float>& pcm,bool sparse=false)
    {
        const auto frames=pcm.size()/size_t(c.channels);std::vector<uint8_t> manifest;integer(manifest,0x314d434c,4);integer(manifest,1,4);integer(manifest,uint64_t(c.channels),4);integer(manifest,uint64_t(c.sampleRate),4);integer(manifest,uint64_t(c.pageFrames),4);integer(manifest,frames,8);integer(manifest,(frames+size_t(c.pageFrames)-1)/size_t(c.pageFrames),4);
        for(size_t start=0;start<frames;start+=size_t(c.pageFrames))
        {
            const auto n=std::min(size_t(c.pageFrames),frames-start);std::vector<uint8_t> page;integer(page,0x3150434c,4);integer(page,uint64_t(c.channels),4);integer(page,uint64_t(c.sampleRate),4);integer(page,n,8);
            bool zero=true;for(size_t i=start*size_t(c.channels);i<(start+n)*size_t(c.channels);++i){uint32_t word;std::memcpy(&word,&pcm[i],4);integer(page,word,4);zero=zero && pcm[i]==0;}
            const auto hash=sparse && zero?AssetHash{}:blob(page);manifest.insert(manifest.end(),hash.bytes.begin(),hash.bytes.end());
        }
        return blob(manifest);
    }
    std::vector<float> read(const AssetHash& hash)
    {
        const auto manifest=bytes(hash);std::vector<float> result;
        for(size_t i=32;i<manifest.size();i+=32){AssetHash h;std::memcpy(h.bytes.data(),manifest.data()+i,32);const auto page=bytes(h);for(size_t j=20;j<page.size();j+=4){const uint32_t word=uint32_t(integer(page,j,4));float x;std::memcpy(&x,&word,4);result.push_back(x);}}
        return result;
    }
};
PagedLooperCore::Config config(double rate,int channels=1){PagedLooperCore::Config c;c.sampleRate=rate;c.channels=channels;c.pageFrames=64;c.seconds=300;return c;}
PagedLooperCore::Graph graph(AssetHash hash,uint64_t frames){PagedLooperCore::Graph g;g.base=hash;g.frames=frames;g.playhead=frames/3;g.generation=123;g.status=PagedLooperCore::CaptureStatus::Ready;g.transport=PagedLooperCore::State::Playing;return g;}
double rms(const std::vector<float>& pcm){double sum=0;for(auto x:pcm)sum+=double(x)*x;return std::sqrt(sum/double(pcm.size()));}
}
LUTHIER_TEST(RefPedalLooperConversion, durationPhaseStereoHistoryAndDcSurviveThreeRates)
{
    ConversionFixture f;auto from=config(44100,2);auto g=graph(f.audio(from,std::vector<float>(4410*2,.125f)),4410);
    g.layerCount=1;g.layers[0]=f.audio(from,std::vector<float>(4410*2,.03125f));g.redoCount=1;g.redo[0]=f.audio(from,std::vector<float>(4410*2,-.015625f));g.retiredCount=1;g.retired[0]=f.audio(from,std::vector<float>(4410*2,0.f),true);
    for(double rate:{48000.,96000.,44100.})
    {
        auto to=config(rate,2);PagedLooperCore::Graph next;CHECK(LooperAssetConverter::convert(f,from,to,g,next));
        CHECK(next.frames==uint64_t(std::llround(double(g.frames)*rate/from.sampleRate)));CHECK(next.playhead==uint64_t(std::llround(double(g.playhead)*rate/from.sampleRate))%next.frames);CHECK(next.generation==123);CHECK(next.transport==g.transport);CHECK(next.layerCount==1 && next.redoCount==1 && next.retiredCount==1);
        const AssetHash hashes[]{next.base,next.layers[0],next.redo[0],next.retired[0]};const float values[]{.125f,.03125f,-.015625f,0};
        for(int i=0;i<4;++i){const auto audio=f.read(hashes[i]);CHECK(audio.size()==next.frames*2);for(auto x:audio)CHECK(x==values[i]);}
        g=next;from=to;
    }
}
LUTHIER_TEST(RefPedalLooperConversion, lowToneRetainsAmplitudeAndDownsamplingRejectsAlias)
{
    ConversionFixture f;const auto source=config(96000),target=config(44100);std::vector<float> low(9600),high(9600);
    for(size_t i=0;i<low.size();++i){low[i]=float(.25*std::sin(2*3.14159265358979323846*1000*double(i)/96000));high[i]=float(.25*std::sin(2*3.14159265358979323846*30000*double(i)/96000));}
    auto g=graph(f.audio(source,low),low.size());PagedLooperCore::Graph out;CHECK(LooperAssetConverter::convert(f,source,target,g,out));const auto audio=f.read(out.base);CHECK(audio.size()==4410);CHECK_NEAR(rms(audio),.25/std::sqrt(2.),1e-4);
    for(size_t i=0;i<audio.size();++i)CHECK_NEAR(audio[i],.25*std::sin(2*3.14159265358979323846*1000*double(i)/44100),2e-4);
    g=graph(f.audio(source,high),high.size());CHECK(LooperAssetConverter::convert(f,source,target,g,out));CHECK(rms(f.read(out.base))<.000025);
}
LUTHIER_TEST(RefPedalLooperConversion, exactBudgetsMissingCorruptAndNonfiniteAssetsFailAtomically)
{
    ConversionFixture f;auto source=config(44100),target=config(48000);auto g=graph(f.audio(source,std::vector<float>(441,.125f)),441);size_t ram=0;uint64_t disk=0;CHECK(LooperAssetConverter::reservation(source,target,g,ram,disk));
    target.ramBudget=ram;target.storageBudget=disk;PagedLooperCore::Graph out;CHECK(LooperAssetConverter::convert(f,source,target,g,out));const auto previous=out;const auto checkPrevious=[&]{CHECK(out.base==previous.base);CHECK(out.frames==previous.frames);};
    const auto writes=f.writes;--target.ramBudget;CHECK(!LooperAssetConverter::convert(f,source,target,g,out));checkPrevious();CHECK(f.writes==writes);++target.ramBudget;--target.storageBudget;CHECK(!LooperAssetConverter::convert(f,source,target,g,out));checkPrevious();CHECK(f.writes==writes);++target.storageBudget;
    f.failWrites=true;CHECK(!LooperAssetConverter::convert(f,source,target,g,out));checkPrevious();f.failWrites=false;
    auto bad=g;bad.base=AssetHash::fromHex(juce::String::repeatedString("1",64));CHECK(!LooperAssetConverter::convert(f,source,target,bad,out));checkPrevious();
    auto manifest=f.bytes(g.base);manifest[12]=0;bad.base=f.blob(manifest);CHECK(!LooperAssetConverter::convert(f,source,target,bad,out));checkPrevious();
    const auto original=f.bytes(g.base);const auto originalPath=f.real.getBlobPath(g.base);CHECK(originalPath.replaceWithData(manifest.data(),manifest.size()));CHECK(!LooperAssetConverter::convert(f,source,target,g,out));checkPrevious();CHECK(originalPath.replaceWithData(original.data(),original.size()));
    auto pcm=std::vector<float>(441,.125f);pcm[30]=std::numeric_limits<float>::infinity();bad=graph(f.audio(source,pcm),441);CHECK(!LooperAssetConverter::convert(f,source,target,bad,out));checkPrevious();
    bad=g;bad.layerCount=std::numeric_limits<int>::max();bad.redoCount=std::numeric_limits<int>::max();CHECK(!LooperAssetConverter::convert(f,source,target,bad,out));checkPrevious();
    target.sampleRate=std::numeric_limits<double>::quiet_NaN();CHECK(!LooperAssetConverter::convert(f,source,target,g,out));checkPrevious();
}

LUTHIER_TEST(RefPedalLooperConversion, sameRateAssetsRemainExactAndUnsupportedRatioDoesNotWrite)
{
    ConversionFixture f;auto source=config(48000);std::vector<float> pcm(103);for(size_t i=0;i<pcm.size();++i)pcm[i]=float(std::sin(double(i))*.125);
    auto g=graph(f.audio(source,pcm),pcm.size());g.transport=PagedLooperCore::State::Stopped;PagedLooperCore::Graph out;const auto writes=f.writes;
    CHECK(LooperAssetConverter::convert(f,source,source,g,out));CHECK(out.base==g.base);CHECK(out.transport==g.transport);CHECK(f.writes==writes);
    auto target=source;target.sampleRate=1000;CHECK(!LooperAssetConverter::convert(f,source,target,g,out));CHECK(out.base==g.base);CHECK(f.writes==writes);
}

LUTHIER_TEST(RefPedalLooperConversion, allSupportedHostRatePairsPreserveStereoToneDurationAndPhase)
{
    ConversionFixture f;
    for(double sourceRate:{44100.,48000.,88200.,96000.,192000.})
    {
        auto source=config(sourceRate,2);const auto n=size_t(sourceRate/100);std::vector<float> pcm(n*2);
        for(size_t i=0;i<n;++i){pcm[i*2]=float(.125*std::sin(2*3.14159265358979323846*1000*double(i)/sourceRate));pcm[i*2+1]=-.25f;}
        const auto g=graph(f.audio(source,pcm),n);
        for(double targetRate:{44100.,48000.,88200.,96000.,192000.})
        {
            const auto target=config(targetRate,2);PagedLooperCore::Graph out;CHECK(LooperAssetConverter::convert(f,source,target,g,out));CHECK(out.frames==uint64_t(targetRate/100));CHECK(out.playhead==uint64_t(std::llround(double(g.playhead)*targetRate/sourceRate))%out.frames);
            const auto audio=f.read(out.base);CHECK(audio.size()==size_t(out.frames)*2);
            for(size_t i=0;i<size_t(out.frames);++i){CHECK_NEAR(audio[i*2],.125*std::sin(2*3.14159265358979323846*1000*double(i)/targetRate),2e-4);CHECK(audio[i*2+1]==-.25f);}
        }
    }
}
