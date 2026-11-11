// OWNER: WP16
#include "TestFramework.h"
#include "../ReferencePedals/Models/rc5_storage.h"
#include "../ReferencePedals/Common/LooperStateCodec.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <chrono>
#include <limits>
#include <thread>
using namespace luthier::reference;
using namespace luthier::reference::common;
using luthier::reference::models::Rc5MemoryBank;
namespace
{
using V=juce::var;
V obj(){return V(new juce::DynamicObject());}
void put(V& v,const char* key,const V& child){v.getDynamicObject()->setProperty(key,child);}
V bankValue(){auto v=obj();put(v,"schema",1);V a=juce::Array<V>{};for(int i=0;i<99;++i)a.append(V());put(v,"entries",a);return v;}
AssetHash memoryHash(uint8_t id){AssetHash h;h.bytes[0]=id;return h;}
V entryValue(uint64_t frames=0,int rate=48000,int id=1)
{
    auto entry=obj(),settings=obj(),loop=obj();put(entry,"settings",settings);
    put(loop,"schema",2);put(loop,"status",frames?"ready":"empty");put(loop,"sampleRate",rate);
    put(loop,"channels",2);put(loop,"pageFrames",16);put(loop,"undoDepth",8);
    put(loop,"frames",juce::int64(frames));put(loop,"generation",juce::int64(9007199254740993ull));put(loop,"faults",0);
    put(loop,"base",frames?V("sha256:"+memoryHash(uint8_t(id)).toHex()):V());
    for(auto key:{"layers","retired","redo"})put(loop,key,juce::Array<V>{});
    put(entry,"looper",loop);return entry;
}
void entryAt(V& bank,int number,const V& entry){bank["entries"].getArray()->set(number-1,entry);}
std::string bytes(const Rc5MemoryBank& bank)
{V out;if(!bank.encode(out))return "ENCODE FAILURE";return juce::JSON::toString(out,true).toStdString();}
PagedLooperCore::Graph readyGraph(uint64_t frames=48000,uint8_t id=1)
{PagedLooperCore::Graph g;g.status=PagedLooperCore::CaptureStatus::Ready;g.frames=frames;g.base=memoryHash(id);return g;}
PagedLooperCore::Config memoryConfig(int rate=48000)
{PagedLooperCore::Config c;c.sampleRate=rate;c.undoDepth=8;return c;}
}
LUTHIER_TEST(RefPedalRc5MemoryBank, fixed99PositionsDefaultsAndPublicNumbers)
{
    Rc5MemoryBank bank;V encoded;CHECK(bank.encode(encoded));CHECK(encoded["schema"]==V(1));
    CHECK(encoded["entries"].size()==99);CHECK(encoded.getDynamicObject()->getProperties().size()==2);
    for(int n=1;n<=99;++n)
    {
        CHECK(encoded["entries"][n-1].isVoid());Rc5MemoryBank::Memory m;CHECK(bank.get(n,m));
        CHECK(m.settings.name.isEmpty());CHECK(m.settings.loopLevel==100);CHECK(m.settings.rhythmLevel==80);
        CHECK(m.settings.tempo==120);CHECK(m.settings.pattern==1);CHECK(m.settings.variation==0);
        CHECK(m.settings.kit==1);CHECK(!m.settings.reverse);CHECK(m.loop.graph.status==PagedLooperCore::CaptureStatus::Empty);
        m.settings.name="memory "+juce::String(n);m.settings.pattern=1+(n-1)%57;
        CHECK(bank.setSettings(n,m.settings));
    }
    Rc5MemoryBank restored;CHECK(bank.encode(encoded));CHECK(Rc5MemoryBank::decode(encoded,restored));
    for(int n=1;n<=99;++n){Rc5MemoryBank::Memory m;CHECK(restored.get(n,m));CHECK(m.settings.name=="memory "+juce::String(n));CHECK(m.settings.pattern==1+(n-1)%57);}
    const auto before=bytes(bank);Rc5MemoryBank::Memory previous;previous.settings.name="untouched";
    for(int invalid:{std::numeric_limits<int>::min(),-1,0,100,std::numeric_limits<int>::max()})
    {
        CHECK(!bank.get(invalid,previous));CHECK(previous.settings.name=="untouched");
        CHECK(!bank.setSettings(invalid,{}));CHECK(!bank.save(invalid,readyGraph(),memoryConfig()));CHECK(!bank.clear(invalid));
    }
    CHECK(bytes(bank)==before);
}
LUTHIER_TEST(RefPedalRc5MemoryBank, metadataBoundariesFullPrecisionAndInvalidTypedMutations)
{
    Rc5MemoryBank bank;Rc5MemoryBank::Settings s;s.name=juce::String::fromUTF8("音楽 🎸 mémoire");
    s.loopLevel=0;s.rhythmLevel=0;s.tempo=40;s.pattern=1;s.variation=0;s.kit=1;
    CHECK(bank.setSettings(1,s));s.loopLevel=200;s.rhythmLevel=100;s.tempo=300;s.pattern=57;s.variation=1;s.kit=7;s.reverse=true;
    CHECK(bank.setSettings(99,s));s.tempo=123.45678901234567;CHECK(bank.setSettings(1,s));
    Rc5MemoryBank::Memory m;CHECK(bank.get(1,m));CHECK(m.settings.name==s.name);CHECK(m.settings.tempo==s.tempo);CHECK(m.settings.reverse);
    const auto before=bytes(bank);
    const auto reject=[&](const Rc5MemoryBank::Settings& bad){CHECK(!bank.setSettings(1,bad));CHECK(bytes(bank)==before);};
    for(double bad:{-1.,201.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){auto x=s;x.loopLevel=bad;reject(x);}
    for(double bad:{-1.,101.,std::numeric_limits<double>::quiet_NaN(),-std::numeric_limits<double>::infinity()}){auto x=s;x.rhythmLevel=bad;reject(x);}
    for(double bad:{39.999,300.001,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){auto x=s;x.tempo=bad;reject(x);}
    for(int bad:{0,58}){auto x=s;x.pattern=bad;reject(x);}for(int bad:{-1,2}){auto x=s;x.variation=bad;reject(x);}for(int bad:{0,8}){auto x=s;x.kit=bad;reject(x);}
}
LUTHIER_TEST(RefPedalRc5MemoryBank, malformedSchemaSettingsAndNonfiniteUnknownFieldsAreAtomic)
{
    Rc5MemoryBank bank;Rc5MemoryBank::Settings kept;kept.name="retained";CHECK(bank.setSettings(1,kept));
    const auto before=bytes(bank);const auto reject=[&](const V& bad){juce::String error;CHECK(!Rc5MemoryBank::decode(bad,bank,&error));CHECK(error.isNotEmpty());CHECK(bytes(bank)==before);};
    reject(1);reject(juce::Array<V>{});
    for(auto schema:{V(true),V(0),V(-1),V(1.5),V("1"),V(std::numeric_limits<double>::infinity())}){auto x=bankValue();put(x,"schema",schema);reject(x);}
    for(int count:{0,98,100}){auto x=bankValue();V entries=juce::Array<V>{};for(int i=0;i<count;++i)entries.append(V());put(x,"entries",entries);reject(x);}
    {auto x=bankValue();entryAt(x,1,42);reject(x);}
    {auto x=bankValue();auto e=entryValue();put(e,"settings",true);entryAt(x,1,e);reject(x);}
    for(auto key:{"loopLevel","rhythmLevel","tempo","pattern","variation","kit"})
        for(auto value:{V(true),V("1"),V(std::numeric_limits<double>::quiet_NaN()),V(std::numeric_limits<double>::infinity())})
        {auto x=bankValue(),e=entryValue(),s=obj();put(s,key,value);put(e,"settings",s);entryAt(x,1,e);reject(x);}
    for(auto key:{"pattern","variation","kit"}){auto x=bankValue(),e=entryValue(),s=obj();put(s,key,1.25);put(e,"settings",s);entryAt(x,1,e);reject(x);}
    for(auto key:{"name","reverse"}){auto x=bankValue(),e=entryValue(),s=obj();put(s,key,1);put(e,"settings",s);entryAt(x,1,e);reject(x);}
    const std::array<const char*,6> keys{"loopLevel","rhythmLevel","tempo","pattern","variation","kit"};
    const std::array<double,6> low{0,0,40,1,0,1},high{200,100,300,57,1,7};
    for(size_t i=0;i<keys.size();++i)for(double bad:{low[i]-1,high[i]+1})
    {auto x=bankValue(),e=entryValue(),s=obj();put(s,keys[i],bad);put(e,"settings",s);entryAt(x,1,e);reject(x);}
    for(int endpoint=0;endpoint<2;++endpoint)
    {auto x=bankValue(),e=entryValue(),s=obj();for(size_t i=0;i<keys.size();++i)put(s,keys[i],endpoint?high[i]:low[i]);put(s,"reverse",endpoint!=0);put(s,"name",juce::String::fromUTF8("保存"));put(e,"settings",s);entryAt(x,1,e);Rc5MemoryBank edge;CHECK(Rc5MemoryBank::decode(x,edge));}
    for(auto where:{0,1,2,3})
    {auto x=bankValue(),e=entryValue();if(where==0)put(x,"unknown",std::numeric_limits<double>::quiet_NaN());if(where==1)put(e,"unknown",std::numeric_limits<double>::quiet_NaN());if(where==2){auto s=e["settings"];put(s,"unknown",std::numeric_limits<double>::quiet_NaN());}if(where==3){auto l=e["looper"];put(l,"unknown",std::numeric_limits<double>::quiet_NaN());}entryAt(x,1,e);reject(x);}
    CHECK(Rc5MemoryBank::decode({},bank));Rc5MemoryBank::Memory empty;CHECK(bank.get(1,empty));CHECK(empty.settings.name.isEmpty());
}
LUTHIER_TEST(RefPedalRc5MemoryBank, unknownTreesAndEveryCloneBoundaryAreIsolated)
{
    auto input=bankValue(),entry=entryValue(123),settings=entry["settings"],loop=entry["looper"];
    auto vendor=obj();put(vendor,"nested",juce::Array<V>{V("original"),V(.12345678901234567)});
    put(input,"vendor",vendor.clone());put(entry,"vendor",vendor.clone());put(settings,"vendor",vendor.clone());put(loop,"vendor",vendor.clone());
    for(auto key:{"rhythmOn","playmode","countin","precount"})put(settings,key,"opaque policy");
    entryAt(input,1,entry);Rc5MemoryBank bank;CHECK(Rc5MemoryBank::decode(input,bank));
    input["vendor"]["nested"].getArray()->set(0,"input changed");settings["vendor"]["nested"].getArray()->set(0,"settings changed");
    Rc5MemoryBank::Memory m;CHECK(bank.get(1,m));CHECK(m.source["vendor"]["nested"][0].toString()=="original");
    m.source["vendor"]["nested"].getArray()->set(0,"getter changed");m.loop.source["vendor"]["nested"].getArray()->set(0,"loop getter changed");
    m.settings.name="edited";CHECK(bank.setSettings(1,m.settings));CHECK(bank.save(1,readyGraph(456,9),memoryConfig()));
    V out;CHECK(bank.encode(out));CHECK(out["vendor"]["nested"][0].toString()=="original");
    auto e=out["entries"][0];for(auto part:{e,e["settings"],e["looper"]})CHECK(part["vendor"]["nested"][0].toString()=="original");
    for(auto key:{"rhythmOn","playmode","countin","precount"})CHECK(e["settings"][key].toString()=="opaque policy");
    out["entries"][0]["looper"]["vendor"]["nested"].getArray()->set(0,"output changed");
    CHECK(bank.get(1,m));CHECK(m.loop.source["vendor"]["nested"][0].toString()=="original");
    CHECK(bank.clear(1));CHECK(bank.encode(out));CHECK(out["entries"][0]["settings"]["name"].toString()=="edited");
    CHECK(out["entries"][0]["looper"]["vendor"]["nested"][0].toString()=="original");
}
LUTHIER_TEST(RefPedalRc5MemoryBank, futureIndexAndFutureLoopRemainOpaqueWithoutSuccessfulNoopSaves)
{
    auto future=obj();put(future,"schema",99);put(future,"invented",juce::Array<V>{V("kept"),V(.125)});Rc5MemoryBank bank;
    CHECK(Rc5MemoryBank::decode(future,bank));CHECK(bank.opaque());CHECK(!bank.capacityKnown());V out;CHECK(bank.encode(out));CHECK(juce::JSON::toString(out)==juce::JSON::toString(future));
    const auto original=bytes(bank);
    out["invented"].getArray()->set(0,"output mutation");future["invented"].getArray()->set(0,"input mutation");CHECK(bytes(bank)==original);
    Rc5MemoryBank::Memory m;m.settings.name="unchanged";
    CHECK(!bank.get(1,m));CHECK(m.settings.name=="unchanged");CHECK(!bank.setSettings(1,{}));CHECK(!bank.save(1,readyGraph(),memoryConfig()));CHECK(!bank.clear(1));CHECK(bytes(bank)==original);
    Rc5MemoryBank::References refs;CHECK(bank.references(refs));CHECK(!refs.complete);CHECK(refs.hashes.empty());
    auto mixed=bankValue(),e=entryValue();auto loop=obj();put(loop,"schema",3);put(loop,"unrecognisedAudio","future");put(e,"looper",loop);entryAt(mixed,2,e);entryAt(mixed,1,entryValue(48000));
    CHECK(Rc5MemoryBank::decode(mixed,bank));CHECK(!bank.opaque());CHECK(!bank.capacityKnown());CHECK(bank.get(2,m));CHECK(m.loop.forward);
    const auto before=bytes(bank);CHECK(!bank.save(2,readyGraph(),memoryConfig()));CHECK(!bank.clear(2));CHECK(!bank.save(3,readyGraph(),memoryConfig()));CHECK(bytes(bank)==before);
    m.settings.name="safe metadata";CHECK(bank.setSettings(2,m.settings));CHECK(bank.clear(1));CHECK(bank.encode(out));CHECK(out["entries"][1]["looper"]["unrecognisedAudio"].toString()=="future");
    CHECK(bank.references(refs));CHECK(!refs.complete);CHECK(refs.hashes.empty());
}
LUTHIER_TEST(RefPedalRc5MemoryBank, legacyLooperMigratesAndSchemaTwoHistoryFaultsRuntimeRemainCorrect)
{
    auto input=bankValue(),e=entryValue(500),loop=e["looper"];
    put(loop,"schema",1);loop.getDynamicObject()->removeProperty("redo");put(loop,"legacy","preserved");entryAt(input,1,e);
    Rc5MemoryBank bank;CHECK(Rc5MemoryBank::decode(input,bank));V out;CHECK(bank.encode(out));
    CHECK(out["entries"][0]["looper"]["schema"]==V(2));CHECK(out["entries"][0]["looper"]["redo"].size()==0);CHECK(out["entries"][0]["looper"]["legacy"].toString()=="preserved");
    auto g=readyGraph(789);g.layerCount=2;g.layers[0]=memoryHash(2);g.layers[1]=memoryHash(3);g.retiredCount=2;g.retired[0]=memoryHash(4);g.retired[1]=memoryHash(5);g.redoCount=2;g.redo[0]=memoryHash(6);g.redo[1]=memoryHash(7);g.faults=LooperStorage::readUnderrun;
    g.runtime.available=true;g.runtime.state=PagedLooperCore::State::Playing;g.runtime.head=88;g.runtime.gain=.5;g.runtime.sample=1234;g.runtime.direction=PagedLooperCore::Direction::Reverse;
    CHECK(bank.save(1,g,memoryConfig()));Rc5MemoryBank::Memory m;CHECK(bank.get(1,m));CHECK(m.loop.graph.layerCount==2);CHECK(m.loop.graph.retiredCount==2);CHECK(m.loop.graph.redoCount==2);CHECK(m.loop.graph.redo[0]==memoryHash(6));CHECK(m.loop.graph.redo[1]==memoryHash(7));CHECK(m.loop.graph.faults==LooperStorage::readUnderrun);CHECK(!m.loop.graph.runtime.available);
    CHECK(bank.encode(out));CHECK(!out["entries"][0]["looper"].hasProperty("runtime"));
    CHECK(bank.clear(1));CHECK(bank.get(1,m));CHECK(m.loop.graph.frames==0);CHECK(m.loop.graph.base.isNull());CHECK(m.loop.graph.layerCount==0);CHECK(m.loop.graph.redoCount==0);CHECK(m.loop.source["legacy"].toString()=="preserved");
}
LUTHIER_TEST(RefPedalRc5MemoryBank, incompleteCapturesAndInvalidCodecFieldsCannotReplaceSavedAudio)
{
    Rc5MemoryBank bank;CHECK(bank.save(1,readyGraph(),memoryConfig()));const auto before=bytes(bank);
    for(auto status:{PagedLooperCore::CaptureStatus::Empty,PagedLooperCore::CaptureStatus::Recording,PagedLooperCore::CaptureStatus::Pending,PagedLooperCore::CaptureStatus::Failed})
    {auto g=readyGraph();g.status=status;CHECK(!bank.save(1,g,memoryConfig()));CHECK(bytes(bank)==before);}
    for(double rate:{0.,.5,192001.,48000.5,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
    {auto c=memoryConfig();c.sampleRate=rate;CHECK(!bank.save(1,readyGraph(),c));CHECK(bytes(bank)==before);}
    auto g=readyGraph();g.base={};CHECK(!bank.save(1,g,memoryConfig()));g=readyGraph();g.layerCount=9;CHECK(!bank.save(1,g,memoryConfig()));g=readyGraph();g.redoCount=1;CHECK(!bank.save(1,g,memoryConfig()));CHECK(bytes(bank)==before);
    for(auto status:{"pending","recording","failed"})
    {auto input=bankValue(),e=entryValue();auto loop=e["looper"];put(loop,"status",status);entryAt(input,1,e);CHECK(!Rc5MemoryBank::decode(input,bank));CHECK(bytes(bank)==before);}
    V out="unchanged";CHECK(bank.encode(out));CHECK(out["entries"][0]["looper"]["base"].toString()=="sha256:"+memoryHash(1).toHex());
}
LUTHIER_TEST(RefPedalRc5MemoryBank, exactPerMemoryDurationRetainsAllSupportedIntegralRates)
{
    for(int rate:{1,3,44100,48000,88200,96000,191999,192000})
    {
        Rc5MemoryBank bank;const auto frames=uint64_t(rate)*5400;CHECK(bank.save(99,readyGraph(frames),memoryConfig(rate)));const auto before=bytes(bank);
        CHECK(!bank.save(99,readyGraph(frames+1),memoryConfig(rate)));CHECK(bytes(bank)==before);
        auto input=bankValue();entryAt(input,1,entryValue(frames+1,rate));CHECK(!Rc5MemoryBank::decode(input,bank));CHECK(bytes(bank)==before);
    }
}
LUTHIER_TEST(RefPedalRc5MemoryBank, exactMixedRateTotalAndReplacementCountLogicalMemories)
{
    Rc5MemoryBank bank;
    for(int n=1;n<=8;++n)CHECK(bank.save(n,readyGraph(5400*44100ull),memoryConfig(44100)));
    CHECK(bank.save(9,readyGraph(3599),memoryConfig(1)));CHECK(bank.save(10,readyGraph(1),memoryConfig(3)));CHECK(bank.save(11,readyGraph(4),memoryConfig(6)));
    const auto atCap=bytes(bank);CHECK(!bank.save(12,readyGraph(1),memoryConfig(192000)));CHECK(bytes(bank)==atCap);
    CHECK(bank.save(1,readyGraph(5400*192000ull),memoryConfig(192000))); // replace, not add
    CHECK(bank.clear(1));CHECK(bank.save(12,readyGraph(5400*48000ull),memoryConfig(48000)));
    auto g=readyGraph(5400*48000ull);g.layerCount=1;g.layers[0]=memoryHash(2);g.retiredCount=1;g.retired[0]=memoryHash(3);g.redoCount=1;g.redo[0]=memoryHash(4);
    CHECK(bank.save(12,g,memoryConfig())); // history does not multiply duration
    auto input=bankValue();for(int n=1;n<=9;++n)entryAt(input,n,entryValue(5400,1));const auto before=bytes(bank);CHECK(!Rc5MemoryBank::decode(input,bank));CHECK(bytes(bank)==before);
    auto opaque=entryValue();auto f=obj();put(f,"schema",3);put(opaque,"looper",f);entryAt(input,99,opaque);CHECK(!Rc5MemoryBank::decode(input,bank));CHECK(bytes(bank)==before);
}
LUTHIER_TEST(RefPedalRc5MemoryBank, coprimeFractionalDurationsHaveExactBoundaryWithoutRounding)
{
    // For each prime p, 1/p + (p-1)/p = 1 second exactly. A floating sum
    // can drift on this boundary; no fixed microsecond/frame rounding is valid.
    const std::array<int,45> primes{2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197};
    auto input=bankValue();for(int i=0;i<8;++i)entryAt(input,i+1,entryValue(5400,1));entryAt(input,9,entryValue(3555,1));
    for(int i=0;i<45;++i){entryAt(input,10+2*i,entryValue(1,primes[size_t(i)]));entryAt(input,11+2*i,entryValue(uint64_t(primes[size_t(i)]-1),primes[size_t(i)]));}
    Rc5MemoryBank bank;CHECK(Rc5MemoryBank::decode(input,bank));CHECK(bank.capacityKnown());
    const auto before=bytes(bank);CHECK(!bank.save(10,readyGraph(2),memoryConfig(2)));CHECK(bytes(bank)==before);
    CHECK(bank.save(10,readyGraph(1),memoryConfig(2))); // same exact contribution at cap
}
LUTHIER_TEST(RefPedalRc5MemoryBank, knownRootOrderCompletenessAndOptionalDedupPreserveAllHistory)
{
    Rc5MemoryBank bank;auto g=readyGraph(48000,1);g.layerCount=2;g.layers[0]=memoryHash(2);g.layers[1]=memoryHash(1);g.retiredCount=1;g.retired[0]=memoryHash(3);g.redoCount=2;g.redo[0]=memoryHash(4);g.redo[1]=memoryHash(2);
    CHECK(bank.save(2,g,memoryConfig()));CHECK(bank.save(99,readyGraph(48000,5),memoryConfig()));Rc5MemoryBank::References refs;
    CHECK(bank.references(refs));CHECK(refs.complete);const std::vector<AssetHash> expected{memoryHash(1),memoryHash(2),memoryHash(1),memoryHash(3),memoryHash(4),memoryHash(2),memoryHash(5)};CHECK(refs.hashes==expected);
    CHECK(bank.references(refs,true));const std::vector<AssetHash> unique{memoryHash(1),memoryHash(2),memoryHash(3),memoryHash(4),memoryHash(5)};CHECK(refs.hashes==unique);CHECK(refs.complete);
    V input;CHECK(bank.encode(input));auto opaque=entryValue();auto loop=obj();put(loop,"schema",3);put(opaque,"looper",loop);entryAt(input,1,opaque);CHECK(Rc5MemoryBank::decode(input,bank));CHECK(bank.references(refs));CHECK(!refs.complete);CHECK(refs.hashes==expected);
}
LUTHIER_TEST(RefPedalRc5MemoryBank, realReadyCaptureSavesAndReturnsRuntimeFreeGraphForFutureWrapper)
{
    const auto path=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rc5-memory-"+juce::Uuid().toString());
    {
        AssetStore store(path);PagedLooperCore core;auto c=memoryConfig();c.seconds=.1;c.pageFrames=16;c.writePages=16;c.readPages=8;c.readAhead=3;
        CHECK(core.prepare(store,c));core.setSeamMilliseconds(0);core.apply(LooperAction::Record);
        std::array<double,64> l{},r{};l.fill(.125);r.fill(-.25);core.process(l.data(),r.data(),64);core.apply(LooperAction::Play);
        PagedLooperCore::Graph graph;bool ready=false;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        do{core.process(nullptr,nullptr,0);if(core.capture(graph) && graph.status==PagedLooperCore::CaptureStatus::Ready){ready=true;break;}std::this_thread::yield();}while(std::chrono::steady_clock::now()<until);
        CHECK(ready);if(ready)
        {
            CHECK(graph.runtime.available);Rc5MemoryBank bank;CHECK(bank.save(1,graph,c));Rc5MemoryBank::Memory m;CHECK(bank.get(1,m));
            CHECK(m.loop.graph.base==graph.base);CHECK(m.loop.graph.frames==64);CHECK(!m.loop.graph.runtime.available);CHECK(m.loop.config.sampleRate==48000);CHECK(core.length()==64);CHECK(core.state()==PagedLooperCore::State::Playing);
            Rc5MemoryBank::References refs;CHECK(bank.references(refs));CHECK(refs.complete);CHECK(refs.hashes.size()==1);CHECK(store.hasBlob(refs.hashes[0]));
        }
    }
    CHECK(path.deleteRecursively());
}

// Coverage-only: unknown source data named runtime belongs to its original
// owner. It must survive while typed Graph.runtime stays deliberately absent.
LUTHIER_TEST(RefPedalRc5MemoryBank, unknownRuntimeObjectSurvivesRealReadySaveAndClearWithoutResumeAuthority)
{
    const auto literal=juce::JSON::parse(R"json({
        "available":true,"state":"playing","head":17,"gain":0.25,
        "sample":777,"direction":"reverse",
        "futureOwner":{"version":7,"values":["keep",false,0.12345678901234566]}
    })json");
    const auto expected=literal.clone();
    auto input=bankValue(),entry=entryValue(),loop=entry["looper"];
    put(loop,"runtime",literal.clone());entryAt(input,1,entry);
    Rc5MemoryBank bank;CHECK(Rc5MemoryBank::decode(input,bank));
    const auto checkOpaqueRuntime=[&]
    {
        Rc5MemoryBank::Memory memory;CHECK(bank.get(1,memory));
        CHECK(!memory.loop.graph.runtime.available);
        CHECK(memory.loop.graph.runtime.head==0);CHECK(memory.loop.graph.runtime.sample==0);
        CHECK(memory.loop.graph.runtime.gain==0);
        CHECK(memory.loop.graph.runtime.state==PagedLooperCore::State::Stopped);
        CHECK(memory.loop.graph.runtime.direction==PagedLooperCore::Direction::Forward);
        CHECK(juce::JSON::toString(memory.loop.source["runtime"],true)==juce::JSON::toString(expected,true));
        V encoded;CHECK(bank.encode(encoded));
        CHECK(juce::JSON::toString(encoded["entries"][0]["looper"]["runtime"],true)==juce::JSON::toString(expected,true));
    };
    checkOpaqueRuntime();
    // Mutating the incoming object's nested data cannot change retained data.
    input["entries"][0]["looper"]["runtime"]["futureOwner"]["values"].getArray()->set(0,"input changed");
    checkOpaqueRuntime();
    const auto path=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rc5-memory-runtime-"+juce::Uuid().toString());
    {
        AssetStore store(path);PagedLooperCore recorded,candidate;auto config=memoryConfig();
        config.seconds=.1;config.pageFrames=16;config.writePages=16;config.readPages=8;config.readAhead=3;
        CHECK(recorded.prepare(store,config));CHECK(candidate.prepare(store,config));
        recorded.setSeamMilliseconds(0);recorded.apply(LooperAction::Record);
        std::array<double,64> left{},right{};left.fill(.125);right.fill(-.25);
        recorded.process(left.data(),right.data(),64);recorded.apply(LooperAction::Play);
        PagedLooperCore::Graph capture;bool ready=false;
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        do{recorded.process(nullptr,nullptr,0);if(recorded.capture(capture) && capture.status==PagedLooperCore::CaptureStatus::Ready){ready=true;break;}std::this_thread::yield();}while(std::chrono::steady_clock::now()<until);
        CHECK(ready);
        if(ready)
        {
            CHECK(capture.runtime.available);CHECK(capture.runtime.sample==64);
            CHECK(bank.save(1,capture,config));checkOpaqueRuntime();
            Rc5MemoryBank::Memory saved;CHECK(bank.get(1,saved));
            CHECK(saved.loop.graph.base==capture.base);CHECK(saved.loop.graph.frames==64);
            CHECK(!candidate.restore(saved.loop.graph,PagedLooperCore::RestorePolicy::ResumeCaptured));
            CHECK(candidate.state()==PagedLooperCore::State::Empty);CHECK(candidate.length()==0);CHECK(candidate.samplePosition()==0);
            CHECK(bank.clear(1));checkOpaqueRuntime();CHECK(bank.get(1,saved));
            CHECK(saved.loop.graph.status==PagedLooperCore::CaptureStatus::Empty);CHECK(saved.loop.graph.frames==0);
            CHECK(!candidate.restore(saved.loop.graph,PagedLooperCore::RestorePolicy::ResumeCaptured));
            CHECK(candidate.state()==PagedLooperCore::State::Empty);CHECK(candidate.samplePosition()==0);
        }
    }
    CHECK(path.deleteRecursively());
}
