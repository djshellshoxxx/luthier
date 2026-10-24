// OWNER: WP4
#include "TestFramework.h"
#include "../ReferencePedals/BoardState.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../ReferencePedals/BoardSnapshots.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <atomic>
#include <random>
#include <thread>

using namespace luthier::reference;
namespace
{
const ParamDesc stateParams[] {
    {"state_drive","Drive","",ParamKind::Continuous,Taper::Linear,0,10,5,1,0,nullptr,PF_Automatable,0,-1},
    {"state_tone","Tone","Hz",ParamKind::Continuous,Taper::LogA,100,10000,1000,1,0,nullptr,PF_Automatable,0,-1}
};
const PedalDesc stateDesc {"test.state","state_",1,"State test","Test",nullptr,ChannelMode::Stereo,BypassMode::True,
                           2,stateParams,0,false,false,false,1};
class StatePedal final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return stateDesc; }
    void prepare(double,int,int) override {}
    void reset() noexcept override {}
    void process(double* l,double* r,int n) noexcept override
    { for (int i=0;i<n;++i) { l[i]*=values[0]; r[i]*=values[0]; } }
    void setParam(int i,float v) noexcept override { if(i>=0 && i<2) values[i]=v; }
    float getParam(int i) const noexcept override { return i>=0 && i<2 ? values[i] : 0; }
private:
    float values[2] {0.5f,0.5f};
};
std::unique_ptr<IPedal> makeStatePedal() { return std::make_unique<StatePedal>(); }
juce::String minimal() { return R"({"schema":1,"enabled":true,"order":[],"instances":[]})"; }
}
LUTHIER_TEST (RefPedalState, malformedRestoreIsAtomic)
{
    BoardModel model;
    CHECK (BoardState::restore (model, minimal()));
    juce::String before;
    CHECK (BoardState::save (model, before));
    for (const auto* text : { "null", "[]", "{}", "{\"schema\":1.5}", "{\"schema\":true}",
                             "{\"schema\":-1}", "{\"schema\":1,\"mix\":2}",
                             "{\"schema\":1,\"enabled\":1}", "{\"schema\":1,\"tempoBpm\":1e300}",
                             "{\"schema\":1,\"instances\":{}}", "{\"schema\":1,\"feedback\":[]}", "{\"schema\":1,\"ui\":[]}", "{\"schema\":1} trailing",
                             "{\"schema\":01}", "{\"schema\":1,\"mix\":.5}", "{\"schema\":1,\"mix\":1.}",
                             "{\"schema\":1,\"x\":1e999}", "{\"schema\":1,\"x\":9223372036854775808}",
                             "{\"schema\":1,\"schema\":1}", "{\"schema\":1,\"snapshots\":[3]}",
                             "{\"schema\":1,\"loops\":[{\"blend\":2}]}" })
    {
        juce::String error, after;
        CHECK (! BoardState::restore (model, juce::String (text), &error));
        CHECK (error.isNotEmpty());
        CHECK (BoardState::save (model, after));
        CHECK (after == before);
    }
}
LUTHIER_TEST (RefPedalState, futureSchemaVerbatimAndDisabled)
{
    const juce::String future = "{ \"schema\":99, \"enabled\":true, \"novel\": [1, {\"x\":2}] }";
    BoardModel model;
    CHECK (BoardState::restore (model, future));
    CHECK (! model.getBoardSettings().enabled);
    juce::String saved;
    CHECK (BoardState::save (model, saved));
    CHECK (saved == future);
    CHECK (model.apply (cmd::SetMix { 0.25f }));
    CHECK (BoardState::save (model, saved));
    CHECK ((int) juce::JSON::parse (saved)["schema"] == 1);
}
LUTHIER_TEST (RefPedalState, capRejectsAtomically)
{
    BoardModel model;
    CHECK (BoardState::restore (model, minimal()));
    CHECK (! BoardState::restore (model, juce::String::repeatedString (" ", (int) BoardState::maxBytes + 1)));
    CHECK (model.getBoardSettings().enabled);
}
LUTHIER_TEST (RefPedalState, unknownNestedAndMissingModel)
{
    const auto text = R"({"schema":1,"boardStyle":"future-glass","order":["12345678-1234-1234-1234-123456789abc"],"novel":{"a":[1,2]},"instances":[{"id":"12345678-1234-1234-1234-123456789abc","model":"unavailable.model","enabled":true,"params":{"unavailable_drive":0.7},"mounting":"magnets","novel":{"x":3},"extras":{"asset":"sha256:abcd"}}],"lanes":[{"future":17}],"loops":[{"future":18}],"feedback":{"future":19},"midi":{"future":{"x":1}},"ui":{"future":4}})";
    BoardModel model;
    CHECK (BoardState::restore(model,juce::String(text)));
    const auto p = model.getInstance(model.getOrder().front());
    CHECK (p.has_value()); CHECK(p->missing); CHECK(!p->enabled);
    juce::var saved; CHECK (BoardState::save(model,saved));
    CHECK ((int)saved["novel"]["a"][1] == 2);
    CHECK (saved["boardStyle"].toString() == "future-glass");
    CHECK (saved["instances"][0]["mounting"].toString() == "magnets");
    CHECK ((int)saved["instances"][0]["novel"]["x"] == 3);
    CHECK_NEAR ((double)saved["instances"][0]["params"]["unavailable_drive"],0.7,1e-7);
    CHECK ((int)saved["lanes"][0]["future"] == 17);
    CHECK ((int)saved["loops"][0]["future"] == 18);
    CHECK ((int)saved["feedback"]["future"] == 19);
    CHECK ((int)saved["midi"]["future"]["x"] == 1);
    CHECK (model.apply(cmd::SetMix{0.4f}));
    CHECK (BoardState::save(model,saved)); CHECK ((int)saved["ui"]["future"] == 4);
    BoardModel restored; CHECK(BoardState::restore(restored,saved));
    juce::var again; CHECK(BoardState::save(restored,again));
    CHECK(juce::JSON::toString(saved,true) == juce::JSON::toString(again,true));
}
LUTHIER_TEST (RefPedalState, identityAndOrderValidation)
{
    const juce::String id = "12345678123412341234123456789abc";
    auto make = [&] (const juce::String& order, const juce::String& instances)
    { return "{\"schema\":1,\"order\":" + order + ",\"instances\":" + instances + "}"; };
    auto pedal = [&] (const juce::String& uuid, int slot)
    { return "{\"id\":\"" + uuid + "\",\"model\":\"same.model\",\"slot\":" + juce::String(slot) + "}"; };
    BoardModel model;
    CHECK(!BoardState::restore(model,make("[\""+id+"\",\""+id+"\"]","["+pedal(id,0)+","+pedal(id,1)+"]")));
    CHECK(!BoardState::restore(model,make("[\"invalid\"]","["+pedal("invalid",0)+"]")));
    const juce::String id2 = "22345678123412341234123456789abc";
    CHECK(BoardState::restore(model,make("[\""+id2+"\",\""+id+"\"]","["+pedal(id,0)+","+pedal(id2,0)+"]")));
    CHECK(model.getOrder().front() == InstanceId(id2));
    CHECK(model.getInstance(InstanceId(id2))->slot == 0);
    CHECK(model.getInstance(InstanceId(id))->slot == 1);
}
LUTHIER_TEST (RefPedalState, eightSnapshotsSurvivePersistence)
{
    BoardModel model;
    for (int i=0;i<8;++i)
    {
        CHECK(model.apply(cmd::SetMix{(float)i/8}));
        CHECK(BoardSnapshots::capture(model,i,"Scene " + juce::String(i)));
    }
    juce::String text; CHECK(BoardState::save(model,text));
    BoardModel restored; CHECK(BoardState::restore(restored,text));
    for (int i=0;i<8;++i)
    {
        CHECK(BoardSnapshots::recall(restored,i));
        CHECK_NEAR(restored.getBoardSettings().mix,(float)i/8,1e-6);
        CHECK(BoardSnapshots::get(restored,i)["name"].toString() == "Scene " + juce::String(i));
    }
    CHECK(!BoardSnapshots::recall(restored,8));
    CHECK(!BoardSnapshots::capture(restored,-1));
}
LUTHIER_TEST (RefPedalState, seededThousandRoundTrips)
{
    PedalRegistry::TestScope scope;
    CHECK(PedalRegistry::add(stateDesc,makeStatePedal));
    std::mt19937 rng(0x547a7e);
    for (int seed=0;seed<1000;++seed)
    {
        BoardModel model;
        std::vector<PedalInstance> pedals;
        BoardSettings settings; settings.mix=(float)(rng()%1001)/1000; settings.enabled=(rng()%2)!=0;
        settings.mode=(Mode)(rng()%4); settings.style=(BoardStyle)(rng()%7); settings.tempoBpm=20+rng()%281;
        const int n=(int)(rng()%17);
        for (int i=0;i<n;++i)
        {
            PedalInstance p; p.modelId=(rng()%2 ? "fuzz.unavailable" : "test.state"); p.slot=i; p.lane=(int)(rng()%4);
            p.enabled=false; p.missing=p.modelId!="test.state";
            for (auto& value : p.params) value=(float)(rng()%1001)/1000; p.inDb=(float)(rng()%2401)/100-12;
            p.outDb=(float)(rng()%2401)/100-12; p.placement=(Placement)(rng()%3);
            p.mounting=(Mounting)(rng()%8); p.x=(float)(rng()%1000);
            pedals.push_back(p);
        }
        CHECK(model.replaceAll(pedals,settings));
        juce::String before,after; CHECK(BoardState::save(model,before));
        BoardModel restored; CHECK(BoardState::restore(restored,before)); CHECK(BoardState::save(restored,after));
        CHECK_MSG(before == after,"roundtrip seed " + juce::String(seed));
    }
}
LUTHIER_TEST (RefPedalState, concurrentSaveIsCompleteGeneration)
{
    BoardModel model;
    std::atomic<bool> start{false}; std::atomic<int> failures{0};
    std::thread reader([&]
    {
        while (!start.load()) std::this_thread::yield();
        for (int i=0;i<500;++i)
        {
            juce::String text;
            if (!BoardState::save(model,text)) { ++failures; continue; }
            const auto state=juce::JSON::parse(text);
            if ((double)state["mix"] != (double)state["tempoBpm"]/300.0) ++failures;
        }
    });
    for (int i=0;i<500;++i)
    {
        BoardSettings s; s.tempoBpm=(i%2 ? 75.0 : 150.0); s.mix=(float)(s.tempoBpm/300.0);
        if (!model.replaceAll({},s)) ++failures;
        start=true;
    }
    reader.join(); CHECK(failures.load()==0);
}
LUTHIER_TEST (RefPedalState, assetsVerifyAtomicFailureAndAsyncDrain)
{
    const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("refstate-"+juce::Uuid().toString());
    const std::vector<uint8_t> bytes{1,2,3,4,5};
    const auto hash=AssetStore::hashBlob(bytes.data(),bytes.size());
    std::atomic<int> completed{0};
    {
        AssetStore store(dir);
        CHECK(store.putBlob(hash,bytes.data(),bytes.size())); CHECK(store.hasBlob(hash));
        CHECK(!store.putBlob(hash,bytes.data(),bytes.size()-1)); CHECK(store.hasBlob(hash));
        CHECK(AssetStore::reference(hash)=="sha256:"+hash.toHex());
        CHECK(!AssetStore::reference(hash).contains(dir.getFullPathName()));
        auto other=hash; ++other.bytes[0];
        CHECK(store.findMissing({hash,other,other}).size()==1);
        store.putBlobAsync(hash,std::make_shared<const std::vector<uint8_t>>(bytes),[&](bool ok){if(ok) ++completed;});
        store.getBlobAsync(hash,[&](AssetBlob blob){if(blob && *blob==bytes) ++completed;});
    }
    CHECK(completed.load()==2);
    // Force replacement failure with a directory at the content address.
    AssetStore store(dir);
    const auto emptyHash=AssetStore::hashBlob(nullptr,0);
    const auto blocked=dir.getChildFile(emptyHash.toHex()+".blob");
    CHECK(emptyHash.toHex()=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(blocked.createDirectory().wasOk());
    CHECK(blocked.getChildFile("sentinel").replaceWithText("keep"));
    CHECK(!store.putBlob(emptyHash,nullptr,0));
    CHECK(blocked.getChildFile("sentinel").loadFileAsString()=="keep");
    CHECK(dir.deleteRecursively());
}
LUTHIER_TEST (RefPedalState, descriptorDefaultsAndDeterministicAudio)
{
    PedalRegistry::TestScope scope;
    CHECK(PedalRegistry::add(stateDesc,makeStatePedal));
    const juce::String text=R"({"schema":1,"order":["12345678123412341234123456789abc"],"instances":[{"id":"12345678123412341234123456789abc","model":"test.state","params":{"state_drive":0.75,"future_control":0.375}}]})";
    BoardModel model; CHECK(BoardState::restore(model,text));
    const auto p=model.getInstance(model.getOrder().front());
    CHECK(!p->missing); CHECK_NEAR(p->params[0],0.75,1e-7); CHECK_NEAR(p->params[1],0.5,1e-7);
    juce::String saved; CHECK(BoardState::save(model,saved));
    CHECK_NEAR((double)juce::JSON::parse(saved)["instances"][0]["params"]["future_control"],0.375,1e-7);
    BoardModel restored; CHECK(BoardState::restore(restored,saved));
    auto a=PedalRegistry::make("test.state"), b=PedalRegistry::make("test.state");
    const auto q=restored.getInstance(restored.getOrder().front());
    for (int i=0;i<2;++i) { a->setParam(i,p->params[(size_t)i]); b->setParam(i,q->params[(size_t)i]); }
    double la[64],ra[64],lb[64],rb[64];
    for (int i=0;i<64;++i) { la[i]=lb[i]=(double)i/64; ra[i]=rb[i]=-la[i]; }
    a->process(la,ra,64); b->process(lb,rb,64);
    CHECK(std::equal(std::begin(la),std::end(la),std::begin(lb)));
    CHECK(std::equal(std::begin(ra),std::end(ra),std::begin(rb)));
}
LUTHIER_TEST (RefPedalState, invalidTypedOutputKeepsPrevious)
{
    juce::var previous=juce::JSON::parse("{\"keep\":true}");
    BoardSettings settings; settings.mix=std::numeric_limits<float>::quiet_NaN();
    CHECK(!BoardState::encode({},settings,previous)); CHECK((bool)previous["keep"]);
    settings={};
    PedalInstance p; p.modelId="absent"; p.params[31]=2;
    CHECK(!BoardState::encode({p},settings,previous)); CHECK((bool)previous["keep"]);
    settings.ui=juce::JSON::parse("{\"huge\":\"x\"}");
    settings.ui.getDynamicObject()->setProperty("huge",juce::String::repeatedString("x",(int)BoardState::maxBytes));
    CHECK(!BoardState::encode({},settings,previous)); CHECK((bool)previous["keep"]);
    BoardModel model;
    juce::String deeply="{\"schema\":1,\"x\":"+juce::String::repeatedString("[",70)+"0"+juce::String::repeatedString("]",70)+"}";
    CHECK(!BoardState::restore(model,deeply));
}
LUTHIER_TEST (RefPedalState, savedSchemaFixtures)
{
    const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile(__FILE__).getParentDirectory().getChildFile("Fixtures/ReferencePedals");
    for (const auto* file : {"ref_board_v1.json","ref_board_future.json"})
    {
        const auto fixture=directory.getChildFile(file); CHECK(fixture.existsAsFile());
        BoardModel model; CHECK(BoardState::restore(model,fixture.loadFileAsString()));
        juce::String saved; CHECK(BoardState::save(model,saved));
        if (juce::String(file).contains("future")) { CHECK(!model.getBoardSettings().enabled); CHECK(saved==fixture.loadFileAsString()); }
    }
}
LUTHIER_TEST (RefPedalState, unknownNumbersDoNotLosePrecision)
{
    const juce::String text=R"({"schema":1,"vendor":{"a":1.2345678901234567,"b":0.00012345678901234567,"c":1.2345678901234567e-200,"d":9223372036854775807}})";
    BoardModel model; CHECK(BoardState::restore(model,text));
    const auto original=model.getBoardSettings().unknown["source"].clone();
    CHECK((double)original["vendor"]["a"]==1.2345678901234567);
    CHECK((double)original["vendor"]["b"]==0.00012345678901234567);
    juce::String saved; CHECK(BoardState::save(model,saved));
    BoardModel restored; CHECK(BoardState::restore(restored,saved));
    const auto result=restored.getBoardSettings().unknown["source"].clone();
    for (const auto* key : {"a","b","c"}) CHECK((double)original["vendor"][key]==(double)result["vendor"][key]);
    CHECK((juce::int64)original["vendor"]["d"]==(juce::int64)result["vendor"]["d"]);
}
LUTHIER_TEST (RefPedalState, invalidRoutingReferencesKeepPrevious)
{
    BoardModel model; CHECK(BoardState::restore(model,minimal()));
    for (const auto* text : {
        R"({"schema":1,"feedback":{"on":true}})",
        R"({"schema":1,"loops":[{"used":true}]})",
        R"({"schema":1,"order":["12345678123412341234123456789abc"],"instances":[{"id":"12345678123412341234123456789abc","model":"unknown"}],"feedback":{"on":true,"src":"12345678123412341234123456789abc","dst":"12345678123412341234123456789abc","gain":1.0}})"
    })
    {
        CHECK(!BoardState::restore(model,juce::String(text)));
        CHECK(model.getInstanceCount()==0); CHECK(model.getBoardSettings().enabled);
    }
}
LUTHIER_TEST (RefPedalState, rawHostExtractionPreservesOnlyTopLevelBlock)
{
    const juce::String board="{ \"schema\": 2, \"text\": \"}referencePedals{\\\"\", \"value\": 1.2345678901234567 }";
    const auto host="{\"nested\":{\"referencePedals\":{}},\"text\":\"referencePedals: {}\",\"referencePedals\": "+board+",\"after\":[1,2]}";
    juce::String extracted="keep";
    CHECK(BoardState::extractFromHostJson(host,extracted)); CHECK(extracted==board);
    BoardModel model; CHECK(BoardState::restore(model,extracted));
    juce::String saved; CHECK(BoardState::save(model,saved)); CHECK(saved==board);
    CHECK(BoardState::extractFromHostJson("{\"nested\":{\"referencePedals\":{}}}",extracted)); CHECK(extracted.isEmpty());
    for (const auto* malformed : {"[]","{\"referencePedals\":null}","{\"referencePedals\":{}} trailing", "{\"referencePedals\":{},\"referencePedals\":{}}"})
    {
        extracted="keep"; CHECK_MSG(!BoardState::extractFromHostJson(malformed,extracted),malformed); CHECK_MSG(extracted=="keep",malformed);
    }
    const auto tooLarge="{\"referencePedals\":{\"schema\":1,\"unknown\":\""+juce::String::repeatedString("x",(int)BoardState::maxBytes)+"\"}}";
    CHECK(!BoardState::extractFromHostJson(tooLarge,extracted)); CHECK(extracted=="keep");
    // Escaped root member names decode to the same key.
    CHECK(BoardState::extractFromHostJson("{\"reference\\u0050edals\":{\"schema\":1}}",extracted));
    CHECK(extracted=="{\"schema\":1}");
}
LUTHIER_TEST (RefPedalState, windowPreferenceMirrorsLegacyRoot)
{
    BoardModel model;
    CHECK(BoardState::restore(model,juce::String(R"({"schema":1,"window":"popout","ui":{"window":"drawer","vendor":{"preserve":true}}})")));
    std::vector<PedalInstance> pedals; BoardSettings settings; model.snapshot(pedals,settings);
    CHECK(settings.ui["window"].toString()=="popout");
    settings.ui.getDynamicObject()->setProperty("window","drawer");
    CHECK(model.replaceAll(pedals,settings));
    juce::var json; CHECK(BoardState::save(model,json));
    CHECK(json["window"].toString()=="drawer"); CHECK(json["ui"]["window"].toString()=="drawer");
    CHECK((bool)json["ui"]["vendor"]["preserve"]);
    CHECK(!BoardState::restore(model,juce::String(R"({"schema":1,"window":3})")));
    CHECK(!BoardState::restore(model,juce::String(R"({"schema":1,"ui":{"window":3}})")));
}

LUTHIER_TEST(RefPedalState, momentaryFeedbackKillNeverResumes)
{
    BoardModel model;juce::String saved,error;
    CHECK(BoardState::restore(model,juce::String(R"({"schema":1,"feedback":{"kill":true,"vendor":{"curve":[1,2,3]}}})"),&error));
    CHECK(BoardState::save(model,saved,&error));
    const auto state=juce::JSON::parse(saved);
    CHECK(state["feedback"]["kill"].isBool());CHECK(!(bool)state["feedback"]["kill"]);
    CHECK(state["feedback"]["vendor"]["curve"].size()==3);
    const auto before=saved;
    CHECK(!BoardState::restore(model,juce::String(R"({"schema":1,"feedback":{"kill":1}})"),&error));
    CHECK(BoardState::save(model,saved,&error));CHECK(saved==before);
}
