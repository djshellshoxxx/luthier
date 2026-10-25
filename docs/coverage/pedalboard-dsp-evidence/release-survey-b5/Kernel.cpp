#include "StageHarness.h"
#include "../Support/IrLibrary.h"
#include <fstream>
#include <iostream>
using namespace luthier;
using namespace luthier::stagesurvey;
LUTHIER_TEST(ReleaseKernel, preparedCabinetTransfer)
{
 const auto out=juce::SystemStats::getEnvironmentVariable("LUTHIER_RELEASE_SURVEY_OUT",{});
 CHECK(out.isNotEmpty());if(out.isEmpty())return;
 std::ofstream meta(out.toStdString()+"/kernel-settings.txt");meta<<std::setprecision(17);
 for(double rate:{44100.,48000.,88200.,96000.,192000.})
 {
  kSr=rate;Rig rig;rig.p().resetEverything();rig.setIndex(ParamIDs::guitarType,19);
  rig.setNormalised(ParamIDs::couplingAmount,0);rig.setNormalised(ParamIDs::couplingAirAmount,0);rig.setNormalised(ParamIDs::bodyCouplingAmount,0);
  rig.setIndex(ParamIDs::bodyMode,3);rig.setIndex(ParamIDs::roomOn,0);rig.apply();rig.processSilence(4);
  auto& cab=rig.p().getEngine().getCabinetEngine();
  meta<<"rate="<<rate<<" latency="<<cab.getLatencySamples()<<" key="<<CabinetEngine::irFileNameFor(CabinetEngine::anchorConfig(cab.getConfigA()))<<" file="<<IrLibrary::findCabIr(CabinetEngine::anchorConfig(cab.getConfigA())).getFullPathName()<<'\n';
  for(const auto* id:{ParamIDs::ampModel,ParamIDs::ampGain,ParamIDs::ampMaster,ParamIDs::cabType,ParamIDs::cabSpeaker,ParamIDs::micType,ParamIDs::micPosition,ParamIDs::micDistance,ParamIDs::dualMic,ParamIDs::limiterOn,ParamIDs::inputGain,ParamIDs::masterGain})
   if(auto* p=rig.param(id))meta<<id<<'='<<p->convertFrom0to1(p->getValue())<<'\n';
  cab.reset();std::ofstream file(out.toStdString()+"/kernel-r"+std::to_string((int)rate)+".f32",std::ios::binary);
  for(int pos=0,total=(int)(rate*.2);pos<total;pos+=kBlock)
  {
   const int n=std::min(kBlock,total-pos);juce::AudioBuffer<float> b(2,n);b.clear();if(pos==0){b.setSample(0,0,1);b.setSample(1,0,1);}
   cab.processBlock(b);const auto* mic=cab.getMicTap(0);CHECK(mic!=nullptr);if(!mic)return;
   for(int i=0;i<n;++i){const float frame[]{mic[i],b.getSample(0,i)};file.write(reinterpret_cast<const char*>(frame),sizeof(frame));}
  }
 }
}
