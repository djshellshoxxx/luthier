// OWNER: WP20 / WP21
#pragma once
#include "PedalFaceFactory.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeBossDs1Face();
std::unique_ptr<PedalFace> makeIbanezTs808Face();
std::unique_ptr<PedalFace> makeBossSd1Face();
std::unique_ptr<PedalFace> makeMxrDistortionPlusFace();
std::unique_ptr<PedalFace> makeProcoRatFace();
std::unique_ptr<PedalFace> makeMxrDynacompFace();
std::unique_ptr<PedalFace> makeEhxBigMuffNycFace();
std::unique_ptr<PedalFace> makeEhxMuffWickerFace();
std::unique_ptr<PedalFace> makeEhxGreenRussianFace();
std::unique_ptr<PedalFace> makeEhxDeluxeMuffFace();
std::unique_ptr<PedalFace> makeBossCe2Face();
std::unique_ptr<PedalFace> makeBossCe2wFace();
std::unique_ptr<PedalFace> makeMxrPhase90ScriptFace();
std::unique_ptr<PedalFace> makeBossDd3tFace();
std::unique_ptr<PedalFace> makeTcDittoFace();
std::unique_ptr<PedalFace> makeEhxNano360Face();
std::unique_ptr<PedalFace> makeBossRc1Face();
std::unique_ptr<PedalFace> makeBossRc5Face();
}
