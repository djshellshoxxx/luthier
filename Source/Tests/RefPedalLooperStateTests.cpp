// OWNER: WP15/WP16
#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperStateCodec.h"
#include <limits>
using namespace luthier::reference;
using namespace luthier::reference::common;
namespace
{
juce::var object(){return juce::var(new juce::DynamicObject());}
void put(juce::var& value,const char* name,const juce::var& child){value.getDynamicObject()->setProperty(name,child);}
AssetHash asset(uint8_t marker){AssetHash h;h.bytes[0]=marker;return h;}
PagedLooperCore::Graph complete()
{
    PagedLooperCore::Graph g;g.status=PagedLooperCore::CaptureStatus::Ready;g.frames=12345;g.generation=9007199254740993ull;g.base=asset(1);g.layerCount=2;g.layers[0]=g.layers[1]=asset(2);g.retiredCount=1;g.retired[0]=asset(3);return g;
}
PagedLooperCore::Config config(){PagedLooperCore::Config c;c.undoDepth=2;return c;}
juce::var valid(){juce::var value;LooperStateCodec::Stored stored;LooperStateCodec::encode(stored,complete(),config(),value);return value;}
}
LUTHIER_TEST(RefPedalLooperState, exactAssetGraphAndOpaqueNestedFieldsSurvive)
{
    LooperStateCodec::Stored stored;stored.source=object();auto vendor=object();put(vendor,"tiny",1e-200);put(vendor,"precision",.12345678901234567);put(vendor,"nested",juce::Array<juce::var>{"kept",false,42});put(stored.source,"vendor",vendor);put(stored.source,"base","/private/tmp/audio.wav");
    juce::var encoded;CHECK(LooperStateCodec::encode(stored,complete(),config(),encoded));CHECK(encoded["base"].toString().startsWith("sha256:"));CHECK(!encoded["base"].toString().contains("/"));
    LooperStateCodec::Stored restored;CHECK(LooperStateCodec::decode(encoded,restored));CHECK(restored.graph.frames==12345);CHECK(restored.graph.generation==9007199254740993ull);CHECK(restored.graph.layers[0]==restored.graph.layers[1]);CHECK(restored.graph.retired[0]==asset(3));
    CHECK(double(restored.source["vendor"]["tiny"])==1e-200);CHECK(double(restored.source["vendor"]["precision"])==.12345678901234567);CHECK(restored.source["vendor"]["nested"].size()==3);
    auto clone=encoded["vendor"];put(clone,"tiny",.5);CHECK(double(stored.source["vendor"]["tiny"])==1e-200);
    juce::var again;CHECK(LooperStateCodec::encode(restored,restored.graph,restored.config,again));CHECK(again["layers"].size()==2);CHECK(again["retired"].size()==1);
}
LUTHIER_TEST(RefPedalLooperState, invalidKnownFieldsFailAtomically)
{
    const auto good=valid();LooperStateCodec::Stored previous;CHECK(LooperStateCodec::decode(good,previous));
    auto reject=[&](const char* key,const juce::var& value)
    {
        auto bad=good.clone();put(bad,key,value);CHECK(!LooperStateCodec::decode(bad,previous));CHECK(previous.graph.base==asset(1));CHECK(previous.graph.generation==9007199254740993ull);
    };
    reject("schema",true);reject("schema",0);reject("schema",1.5);reject("sampleRate",0);reject("sampleRate",48000.5);reject("sampleRate",std::numeric_limits<double>::infinity());
    reject("channels",3);reject("channels",false);reject("pageFrames",15);reject("pageFrames",65537);reject("frames",-1);reject("frames",double(192000)*5400+1);
    reject("generation",9223372036854775808.);reject("generation",true);reject("faults",4294967296.);reject("faults",-.5);reject("undoDepth",1);reject("status","invented");
    reject("base","/tmp/audio.wav");reject("base","sha256:"+juce::String::repeatedString("0",64));reject("base","sha256:"+juce::String::repeatedString("g",64));
    reject("layers",object());reject("layers",juce::Array<juce::var>{"file:///tmp/a.wav"});reject("retired",juce::Array<juce::var>{"x","x","x"});
    reject("status","pending");reject("vendor",std::numeric_limits<double>::quiet_NaN());
    CHECK(!LooperStateCodec::decode(juce::Array<juce::var>{},previous));CHECK(previous.graph.base==asset(1));
}
LUTHIER_TEST(RefPedalLooperState, pendingRecordingAndFailureNeverClaimAnOlderHash)
{
    LooperStateCodec::Stored stored;stored.source=valid();
    for(auto status:{PagedLooperCore::CaptureStatus::Recording,PagedLooperCore::CaptureStatus::Pending,PagedLooperCore::CaptureStatus::Failed,PagedLooperCore::CaptureStatus::Empty})
    {
        PagedLooperCore::Graph graph;graph.status=status;graph.frames=status==PagedLooperCore::CaptureStatus::Empty?0:123;juce::var encoded;
        CHECK(LooperStateCodec::encode(stored,graph,config(),encoded));CHECK(encoded["base"].isVoid());CHECK(encoded["layers"].size()==0);CHECK(encoded["retired"].size()==0);
        LooperStateCodec::Stored restored;CHECK(LooperStateCodec::decode(encoded,restored));CHECK(restored.graph.status==status);CHECK(restored.graph.base.isNull());
    }
    auto stale=complete();stale.status=PagedLooperCore::CaptureStatus::Pending;juce::var out="previous";CHECK(!LooperStateCodec::encode(stored,stale,config(),out));CHECK(out.toString()=="previous");
    auto bad=config();bad.sampleRate=std::numeric_limits<double>::quiet_NaN();CHECK(!LooperStateCodec::encode(stored,complete(),bad,out));CHECK(out.toString()=="previous");
}
LUTHIER_TEST(RefPedalLooperState, futureSchemaRemainsOpaqueAndMissingStateIsEmpty)
{
    auto future=valid();put(future,"schema",3);put(future,"base",object());auto opaque=object();put(opaque,"curve",juce::Array<juce::var>{1e-200,.12345678901234567});put(future,"future",opaque);
    LooperStateCodec::Stored stored;CHECK(LooperStateCodec::decode(future,stored));CHECK(stored.forward);juce::var out;auto invalid=config();invalid.sampleRate=0;
    CHECK(LooperStateCodec::encode(stored,PagedLooperCore::Graph{},invalid,out));CHECK(out["schema"]==juce::var(3));CHECK(out["base"].getDynamicObject()!=nullptr);CHECK(double(out["future"]["curve"][0])==1e-200);CHECK(double(out["future"]["curve"][1])==.12345678901234567);
    CHECK(LooperStateCodec::decode({},stored));CHECK(!stored.forward);CHECK(stored.graph.status==PagedLooperCore::CaptureStatus::Empty);CHECK(stored.source.isVoid());
}

LUTHIER_TEST(RefPedalLooperState, legacySchemaOneMigratesAndMalformedSchemaTwoFailsAtomically)
{
    // Literal legacy bytes, independent of the current encoder.
    const auto legacy=juce::JSON::parse(R"legacy({
  "schema": 1,
  "status": "ready",
  "sampleRate": 48000,
  "channels": 2,
  "pageFrames": 4096,
  "undoDepth": 2,
  "frames": 12345,
  "generation": 9007199254740993,
  "faults": 0,
  "base": "sha256:0100000000000000000000000000000000000000000000000000000000000000",
  "layers": [
    "sha256:0200000000000000000000000000000000000000000000000000000000000000",
    "sha256:0200000000000000000000000000000000000000000000000000000000000000"
  ],
  "retired": [
    "sha256:0300000000000000000000000000000000000000000000000000000000000000"
  ],
  "legacyVendor": [
    "kept",
    0.12345678901234566
  ]
})legacy");
    LooperStateCodec::Stored old;CHECK(LooperStateCodec::decode(legacy,old));CHECK(!old.forward);
    CHECK(old.graph.redoCount==0);CHECK(old.graph.layerCount==2);CHECK(old.graph.base==asset(1));
    juce::var migrated;CHECK(LooperStateCodec::encode(old,old.graph,old.config,migrated));
    CHECK(migrated["schema"]==juce::var(2));CHECK(migrated["redo"].isArray());CHECK(migrated["redo"].size()==0);
    CHECK(migrated["legacyVendor"][0].toString()=="kept");
    CHECK(double(migrated["legacyVendor"][1])==.12345678901234567);
    const auto good=valid();LooperStateCodec::Stored previous;CHECK(LooperStateCodec::decode(good,previous));
    auto reject=[&](const juce::var& bad)
    {
        CHECK(!LooperStateCodec::decode(bad,previous));CHECK(previous.graph.base==asset(1));
        CHECK(previous.graph.generation==9007199254740993ull);CHECK(previous.graph.redoCount==0);
    };
    auto missing=good.clone();missing.getDynamicObject()->removeProperty("redo");reject(missing);
    for(const auto& value:juce::Array<juce::var>{juce::var(),true,object(),"not an array",
            juce::Array<juce::var>{"sha256:"+asset(4).toHex()},
            juce::Array<juce::var>{"sha256:"+juce::String::repeatedString("0",64)}})
    {auto bad=good.clone();put(bad,"redo",value);reject(bad);}
    auto tooMany=juce::var(juce::Array<juce::var>{});
    for(int i=0;i<9;++i)tooMany.append("sha256:"+asset(4).toHex());
    {auto bad=good.clone();put(bad,"redo",tooMany);reject(bad);}
    LooperStateCodec::Stored stored;juce::var out="unchanged";
    auto rejectGraph=[&](const auto& graph)
    {CHECK(!LooperStateCodec::encode(stored,graph,config(),out));CHECK(out.toString()=="unchanged");};
    for(int count:{-1,9,std::numeric_limits<int>::max()}){auto bad=complete();bad.redoCount=count;rejectGraph(bad);}
    {auto bad=complete();bad.redoCount=1;bad.redo[0]=asset(4);rejectGraph(bad);} // active+redo > depth
    {auto bad=complete();bad.layerCount=1;bad.redoCount=1;rejectGraph(bad);} // missing redo hash
    for(auto status:{PagedLooperCore::CaptureStatus::Empty,PagedLooperCore::CaptureStatus::Recording,
            PagedLooperCore::CaptureStatus::Pending,PagedLooperCore::CaptureStatus::Failed})
    {
        PagedLooperCore::Graph bad;bad.status=status;bad.redoCount=1;bad.redo[0]=asset(4);rejectGraph(bad);
    }
}

LUTHIER_TEST(RefPedalLooperState, redoHistorySurvivesJsonWithExplicitVersionAndExactOrder)
{
    for(int redo=0;redo<=8;++redo)
    {
        auto graph=complete();graph.layerCount=8-redo;graph.redoCount=redo;
        for(int i=0;i<graph.layerCount;++i)graph.layers[size_t(i)]=asset(uint8_t(10+i));
        for(int i=0;i<redo;++i)graph.redo[size_t(i)]=asset(uint8_t(30+i));
        auto c=config();c.undoDepth=8;
        LooperStateCodec::Stored stored;stored.source=object();put(stored.source,"vendor","preserved");
        juce::var encoded;CHECK(LooperStateCodec::encode(stored,graph,c,encoded));
        CHECK(encoded["schema"]==juce::var(2));CHECK(encoded["redo"].isArray());CHECK(encoded["redo"].size()==redo);
        const auto json=juce::JSON::toString(encoded,true,17);
        LooperStateCodec::Stored restored;CHECK(LooperStateCodec::decode(juce::JSON::parse(json),restored));
        CHECK(!restored.forward);CHECK(restored.graph.redoCount==redo);CHECK(restored.graph.layerCount==graph.layerCount);
        CHECK(restored.graph.generation==graph.generation);CHECK(restored.source["vendor"].toString()=="preserved");
        for(int i=0;i<redo;++i)CHECK(restored.graph.redo[size_t(i)]==graph.redo[size_t(i)]);
    }
}
