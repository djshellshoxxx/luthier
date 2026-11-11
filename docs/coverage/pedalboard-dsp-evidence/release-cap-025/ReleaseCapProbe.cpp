#include "ReleaseCapHarness.h"
#include "../Support/IrLibrary.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace luthier;
using namespace luthier::releasecapprobe;
LUTHIER_TEST(ReleaseCapProbe, unchangedReleaseCriterion)
{
 const auto out=juce::SystemStats::getEnvironmentVariable("LUTHIER_RELEASE_SURVEY_OUT",{});
 if(out.isEmpty())throw std::runtime_error("explicit output folder required");
 std::ofstream csv(out.toStdString()+"/metrics.csv");csv<<std::setprecision(17);
 csv<<"rate,type,name,effectiveType,strings,workshop,note,actualOn,actualOff,onNote,offNote,scheduledOff,hostLatency,engineLatency,midiLatency,ampAuxLatency,micAuxLatency,cabLatency,rawBefore,rawAfter,rawDrop,ampBefore,ampAfter,ampDrop,micBefore,micAfter,micDrop,outputBefore,outputAfter,outputDrop,missingRaw,missingMic,nonfiniteRaw,nonfiniteAmp,nonfiniteMic,finiteOutput,activityDrops\n";
 for(double rate:{44100.,48000.,88200.,96000.,192000.})
 {
  const auto onlyRate=juce::SystemStats::getEnvironmentVariable("LUTHIER_RELEASE_ONLY_RATE",{}).getDoubleValue();
  if(onlyRate>0&&rate!=onlyRate)continue;
  kSr=rate;Rig rig;
  auto* types=dynamic_cast<juce::AudioParameterChoice*>(rig.param(ParamIDs::guitarType));
  for(int t=0;t<types->choices.size();++t)
  {
   rig.p().resetEverything();rig.setIndex(ParamIDs::guitarType,t);
   rig.setNormalised(ParamIDs::couplingAmount,0);rig.setNormalised(ParamIDs::couplingAirAmount,0);
   rig.setNormalised(ParamIDs::bodyCouplingAmount,0);rig.setIndex(ParamIDs::bodyMode,3);
   rig.setIndex(ParamIDs::roomOn,0);rig.apply();rig.processSilence(4);
   const int note=rig.lowestPlayableNote()+2,releaseAt=(int)(.5*kSr),window=(int)(.03*kSr);
   std::vector<TimedMidi> events{{0,juce::MidiMessage::noteOn(1,note,(juce::uint8)100)},{releaseAt,juce::MidiMessage::noteOff(1,note)}};
   const auto s=rig.renderEvents(events,releaseAt,.4);auto& e=rig.p().getEngine();
   if(juce::SystemStats::getEnvironmentVariable("LUTHIER_RELEASE_DUMP_STATE",{}).getIntValue()>0){std::ofstream state(out.toStdString()+"/state-r"+std::to_string((int)rate)+"-t"+std::to_string(t)+".xml");state<<rig.state().copyState().toXmlString();}

   csv<<rate<<','<<t<<','<<types->choices[t]<<','<<(int)e.getGuitarType()<<','<<e.getNumStrings()<<','<<e.isWorkshopGuitar()<<','<<note<<','<<s.actualOn<<','<<s.actualOff<<','<<s.onNote<<','<<s.offNote<<','<<releaseAt<<','<<rig.p().getLatencySamples()<<','<<e.getLatencySamples()<<','<<e.getPerStringLatencySamples()<<','<<e.getLatencySamples(AuxBus::ampPreCab)<<','<<e.getLatencySamples(AuxBus::cabMic1)<<','<<e.getCabinetEngine().getLatencySamples();
   double rawDrop=0,outputDrop=0;
   for(const auto* stream:{&s.raw,&s.preCab,&s.mic,&s.mono}){
    const double before=Rig::windowRms(*stream,releaseAt-window,window),after=Rig::windowRms(*stream,releaseAt+(int)(.25*kSr),window);
    const double drop=20*std::log10(std::max(before,1e-20)/std::max(after,1e-20));
    csv<<','<<before<<','<<after<<','<<drop;
    if(stream==&s.raw)rawDrop=drop;if(stream==&s.mono)outputDrop=drop;
   }
   csv<<','<<s.missingRaw<<','<<s.missingMic<<','<<s.nonfiniteRaw<<','<<s.nonfinitePreCab<<','<<s.nonfiniteMic<<','<<s.finite<<','<<s.activityDrops<<'\n';csv.flush();
   std::ofstream audio(out.toStdString()+"/r"+std::to_string((int)rate)+"-t"+std::to_string(t)+".f32",std::ios::binary);
   for(size_t j=0;j<s.mono.size();++j){const float frame[]{s.raw[j],s.preCab[j],s.mic[j],s.mono[j]};audio.write(reinterpret_cast<const char*>(frame),sizeof(frame));}
   CHECK(s.missingRaw==0);CHECK(s.missingMic==0);CHECK(s.nonfiniteRaw+s.nonfinitePreCab+s.nonfiniteMic==0);CHECK(s.finite);CHECK(s.activityDrops==0);
   CHECK(s.actualOff==releaseAt);CHECK(rawDrop>40.0);
   const double before=Rig::windowRms(s.mono,releaseAt-window,window),after=Rig::windowRms(s.mono,releaseAt+(int)(.25*kSr),window);
   const double dropDb=juce::Decibels::gainToDecibels(juce::jmax(after,s.idleRms,1e-7)/juce::jmax(1e-9,before));
   ++ctx.checks;if(before>1e-3&&dropDb>-35.&&after>2*s.idleRms)ctx.fail("original35dB: rate="+juce::String(rate)+" type="+types->choices[t]+" drop="+juce::String(outputDrop,8));
   rig.quiet();
  }
  std::cout<<"completed rate="<<rate<<std::endl;
 }
}

#include "../Model/Guitar/StringMaterials.h"
LUTHIER_TEST(ReleaseCapProbe, heldOpenAndLetRingIdentity)
{
 const auto out=juce::SystemStats::getEnvironmentVariable("LUTHIER_RELEASE_SURVEY_OUT",{});
 if(out.isEmpty())throw std::runtime_error("explicit output folder required");
 std::ofstream csv(out.toStdString()+"/identity.csv");csv<<std::setprecision(17);
 csv<<"rate,index,fret,scenario,preHash,fullHash,rawDrop,pitchRatio\n";
 for(double sr:{44100.,48000.,88200.,96000.,192000.})for(int index:{0,5})for(double fret:{0.,2.,12.})for(int scenario=0;scenario<4;++scenario)
 {
  StringEngine str;str.setIndex(index);str.prepare(sr,128);
  const double open=index==0?329.628:82.407;
  auto spec=StringMaterials::computeSpec(StringMaterial::NickelPlatedSteel,StringGauge::Regular,StringAge::BrokenIn,index,open,648.);
  auto physical=StringMaterials::toPhysical(spec,648.);physical.coreDiameterMm=spec.coreDiameterMm;physical.tensionNewtons=spec.tensionNewtons;physical.youngsModulus=StringMaterials::get(StringMaterial::NickelPlatedSteel).youngsModulusPa;
  str.setPhysical(physical);
  StringEngine::SustainShape shape;
  if(scenario==3){shape.releaseSeconds=.04;shape.releaseSagMm=5;}
  str.setSustainShape(shape);const double hz=open*std::pow(2.,fret/12.);str.snapToFrequency(hz);str.reset();str.snapToFrequency(hz);str.setTargetFrequency(hz);str.setStoppedFret(fret);str.setDamping(StringEngine::Damping::Open,1.);
  Excitation::Params p;p.velocity=.9;p.kind=Excitation::Kind::Pluck;str.excite(p);
  const int off=(int)(sr*.5),count=off+(int)(sr*.4),win=(int)(sr*.03);
  std::vector<double> audio(count);bool finite=true;
  for(int i=0;i<count;++i){if(i==off&&scenario!=0)str.release(scenario==1,fret);audio[i]=str.processSample(0.);finite&=std::isfinite(audio[i]);}
  auto rms=[&](int start){double x=0;for(int i=start;i<start+win;++i)x+=audio[i]*audio[i];return std::sqrt(x/win);};
  const double drop=20*std::log10(std::max(rms(off-win),1e-20)/std::max(rms(off+(int)(sr*.25)),1e-20));
  csv<<sr<<','<<index<<','<<fret<<','<<scenario<<','<<juce::SHA256(audio.data(),off*sizeof(double)).toHexString()<<','<<juce::SHA256(audio.data(),audio.size()*sizeof(double)).toHexString()<<','<<drop<<','<<str.getShapePitchRatio()<<'\n';
  CHECK(finite);if(scenario>=2)CHECK(drop>40.);if(fret==0||scenario!=3)CHECK(str.getShapePitchRatio()==1.);
 }
}

LUTHIER_TEST(ReleaseCapProbe, actualSustainAndSostenutoHoldIdentity)
{
 const auto out=juce::SystemStats::getEnvironmentVariable("LUTHIER_RELEASE_SURVEY_OUT",{});
 if(out.isEmpty())throw std::runtime_error("explicit output folder required");
 std::ofstream csv(out.toStdString()+"/pedal-identity.csv");csv<<std::setprecision(17);
 csv<<"rate,type,cc,pedalUp,actualKeyOff,heldOpenCount,releasedCount,rawHeldRms,rawPrefixHash,ampPrefixHash,micPrefixHash,outputPrefixHash\n";
 for(double rate:{44100.,48000.,88200.,96000.,192000.})
 {
  kSr=rate;Rig rig;
  for(int t:{0,18,19})for(int cc:{64,66})
  {
   rig.p().resetEverything();rig.setIndex(ParamIDs::guitarType,t);
   rig.setNormalised(ParamIDs::couplingAmount,0);rig.setNormalised(ParamIDs::couplingAirAmount,0);rig.setNormalised(ParamIDs::bodyCouplingAmount,0);rig.setIndex(ParamIDs::bodyMode,3);rig.setIndex(ParamIDs::roomOn,0);rig.apply();rig.processSilence(4);
   const int note=rig.lowestPlayableNote()+2,off=(int)(rate*.5),pedalUp=(int)(rate*.8);
   std::vector<TimedMidi> events{{0,juce::MidiMessage::noteOn(1,note,(juce::uint8)100)},{(int)(rate*.1),juce::MidiMessage::controllerEvent(1,cc,127)},{off,juce::MidiMessage::noteOff(1,note)},{pedalUp,juce::MidiMessage::controllerEvent(1,cc,0)}};
   int heldOpen=0,released=0;
   auto countState=[&](StringEngine::Damping damping){int count=0;auto& engine=rig.p().getEngine();for(int j=0;j<engine.getNumStrings();++j){auto& string=engine.getString(j);const double cents=1200*std::log2(string.getTargetFrequency()/juce::MidiMessage::getMidiNoteInHertz(note));if(std::abs(cents)<1&&string.getDamping()==damping)++count;}return count;};
   const auto s=rig.renderEvents(events,pedalUp,.4,[&](int){heldOpen=countState(StringEngine::Damping::Open);},(int)(rate*.7));
   released=countState(StringEngine::Damping::Released);
   const double held=Rig::windowRms(s.raw,(int)(rate*.7),(int)(rate*.03));
   csv<<rate<<','<<t<<','<<cc<<','<<pedalUp<<','<<s.actualOff<<','<<heldOpen<<','<<released<<','<<held;
   for(const auto* samples:{&s.raw,&s.preCab,&s.mic,&s.mono})csv<<','<<juce::SHA256(samples->data(),pedalUp*sizeof(float)).toHexString();
   csv<<'\n';csv.flush();
   // Activity records the physical key-up even while letRing keeps the string open.
   CHECK(s.actualOff==off);CHECK(heldOpen>0&&released>0);CHECK(held>1e-8);CHECK(s.finite);CHECK(s.nonfiniteRaw+s.nonfinitePreCab+s.nonfiniteMic==0);
   rig.quiet();
  }
 }
}
