#include "StageHarness.h"
#include "../Support/IrLibrary.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace luthier;
using namespace luthier::stagesurvey;
LUTHIER_TEST(ReleaseStages, unchangedReleaseCriterion)
{
 const auto out=juce::SystemStats::getEnvironmentVariable("LUTHIER_RELEASE_SURVEY_OUT",{});
 if(out.isEmpty())throw std::runtime_error("explicit output folder required");
 std::ofstream csv(out.toStdString()+"/metrics.csv");csv<<std::setprecision(17);
 csv<<"rate,type,name,effectiveType,strings,workshop,note,actualOn,actualOff,onNote,offNote,scheduledOff,hostLatency,engineLatency,midiLatency,ampAuxLatency,micAuxLatency,cabLatency,rawBefore,rawAfter,rawDrop,diBefore,diAfter,diDrop,ampBefore,ampAfter,ampDrop,micBefore,micAfter,micDrop,outputBefore,outputAfter,outputDrop,missingRaw,missingMic,nonfiniteRaw,nonfiniteAmp,nonfiniteMic,finiteOutput,activityDrops\n";
 for(double rate:{44100.,48000.,88200.,96000.,192000.})
 {
  kSr=rate;Rig rig;
  auto* types=dynamic_cast<juce::AudioParameterChoice*>(rig.param(ParamIDs::guitarType));
  for(int t:{0,19,20})
  {
   rig.p().resetEverything();rig.setIndex(ParamIDs::guitarType,t);
   rig.setNormalised(ParamIDs::couplingAmount,0);rig.setNormalised(ParamIDs::couplingAirAmount,0);
   rig.setNormalised(ParamIDs::bodyCouplingAmount,0);rig.setIndex(ParamIDs::bodyMode,3);
   rig.setIndex(ParamIDs::roomOn,0);rig.apply();rig.processSilence(4);
   const int note=rig.lowestPlayableNote()+2,releaseAt=(int)(.5*kSr),window=(int)(.03*kSr);
   std::vector<TimedMidi> events{{0,juce::MidiMessage::noteOn(1,note,(juce::uint8)100)},{releaseAt,juce::MidiMessage::noteOff(1,note)}};
   const auto s=rig.renderEvents(events,releaseAt,.4);auto& e=rig.p().getEngine();
   csv<<rate<<','<<t<<','<<types->choices[t]<<','<<(int)e.getGuitarType()<<','<<e.getNumStrings()<<','<<e.isWorkshopGuitar()<<','<<note<<','<<s.actualOn<<','<<s.actualOff<<','<<s.onNote<<','<<s.offNote<<','<<releaseAt<<','<<rig.p().getLatencySamples()<<','<<e.getLatencySamples()<<','<<e.getPerStringLatencySamples()<<','<<e.getLatencySamples(AuxBus::ampPreCab)<<','<<e.getLatencySamples(AuxBus::cabMic1)<<','<<e.getCabinetEngine().getLatencySamples();
   double rawDrop=0,outputDrop=0;
   for(const auto* stream:{&s.raw,&s.di,&s.preCab,&s.mic,&s.mono}){
    const double before=Rig::windowRms(*stream,releaseAt-window,window),after=Rig::windowRms(*stream,releaseAt+(int)(.25*kSr),window);
    const double drop=20*std::log10(std::max(before,1e-20)/std::max(after,1e-20));
    csv<<','<<before<<','<<after<<','<<drop;
    if(stream==&s.raw)rawDrop=drop;if(stream==&s.mono)outputDrop=drop;
   }
   csv<<','<<s.missingRaw<<','<<s.missingMic<<','<<s.nonfiniteRaw<<','<<s.nonfinitePreCab<<','<<s.nonfiniteMic<<','<<s.finite<<','<<s.activityDrops<<'\n';csv.flush();
   std::ofstream audio(out.toStdString()+"/r"+std::to_string((int)rate)+"-t"+std::to_string(t)+".f32",std::ios::binary);
   for(size_t j=0;j<s.mono.size();++j){const float frame[]{s.raw[j],s.di[j],s.preCab[j],s.mic[j],s.mono[j]};audio.write(reinterpret_cast<const char*>(frame),sizeof(frame));}
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
