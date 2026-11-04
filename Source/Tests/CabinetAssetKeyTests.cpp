#include "../DSP/Amp/CabinetEngine.h"
#include "../Support/IrLibrary.h"
#include "TestFramework.h"
using namespace luthier;
namespace
{
const char *const cabinetKeys[]{"1x12_open",    "1x12_closed", "2x12_open", "2x12_closed", "4x12",
                                "4x12_vintage", "1x15_bass",   "4x10_bass", "8x10_bass",   "acoustic_di"};
const char *const speakerKeys[]{
    "celestion_greenback", "celestion_vintage_30", "celestion_g12h", "celestion_g12t_75",
    "jensen_c12",          "alnico_blue",          "evm_12l",        "bass_ceramic"};
const char *const micKeys[]{"shure_sm57",  "shure_sm7b", "sennheiser_md421", "neumann_u87",
                            "royer_r_121", "akg_c414",   "akg_d112"};
const char *const positionKeys[]{"on_axis_centre", "cap_edge", "off_axis_45", "cone_edge", "rear"};
const char *const distanceKeys[]{"close", "medium", "far"};
juce::String expected(const CabinetConfig &c)
{
    return juce::String(cabinetKeys[(int)c.cabinet]) + "/" + speakerKeys[(int)c.speaker] + "_" +
           micKeys[(int)c.mic] + "_" + positionKeys[(int)c.position] + "_" + distanceKeys[(int)c.distance] +
           ".wav";
}
} // namespace
LUTHIER_TEST(CabinetAssetKeys, resourceKeysDoNotDependOnDisplayLabels)
{
    CabinetConfig c;
    for (int cab = 0; cab < (int)CabinetType::NumCabinets; ++cab)
        for (int speaker = 0; speaker < (int)SpeakerType::NumSpeakers; ++speaker)
            for (int mic = 0; mic < (int)MicType::NumMics; ++mic)
            {
                c.cabinet = (CabinetType)cab;
                c.speaker = (SpeakerType)speaker;
                c.mic = (MicType)mic;
                CHECK(CabinetEngine::irFileNameFor(c) == expected(c));
            }
    for (int position = 0; position < (int)MicPosition::NumPositions; ++position)
        for (int distance = 0; distance < (int)MicDistance::NumDistances; ++distance)
        {
            c.position = (MicPosition)position;
            c.distance = (MicDistance)distance;
            CHECK(CabinetEngine::irFileNameFor(c) == expected(c));
        }
    CHECK(juce::String(CabinetEngine::getSpeakerName(SpeakerType::Vintage30)) == "British 60 W Modern");
    CHECK(juce::String(CabinetEngine::getMicName(MicType::SM57)) == "Classic Dynamic");
}
LUTHIER_TEST(CabinetAssetKeys, shippedCatalogResolvesExactSpeakerAndMic)
{
    const auto root = IrLibrary::getCabIrFolder();
    CHECK(root.isDirectory());
    if (!root.isDirectory())
        return;
    const int speakers[9][3] = {{4, 5, 0}, {1, 2, 6},   {0, 4, 5},   {1, 3, 2},  {0, 1, 3},
                                {0, 2, 5}, {7, -1, -1}, {7, -1, -1}, {7, -1, -1}};
    CabinetConfig c;
    for (int cab = 0; cab < 9; ++cab)
        for (int s = 0; s < 3; ++s)
        {
            if (speakers[cab][s] < 0)
                continue;
            c.cabinet = (CabinetType)cab;
            c.speaker = (SpeakerType)speakers[cab][s];
            for (int mic : {0, 2, 3, cab >= 6 ? 6 : 4})
                for (int pos : {0, 1, 2})
                    for (int dist : {0, 1})
                    {
                        c.mic = (MicType)mic;
                        c.position = (MicPosition)pos;
                        c.distance = (MicDistance)dist;
                        const auto file = root.getChildFile(expected(c));
                        CHECK(file.existsAsFile());
                        CHECK(IrLibrary::findCabIr(c) == file);
                    }
        }
}
LUTHIER_TEST(CabinetAssetKeys, unsupportedCatalogChoicesHaveStableFallback)
{
    const auto root = IrLibrary::getCabIrFolder();
    CHECK(root.isDirectory());
    if (!root.isDirectory())
        return;
    CabinetConfig c;
    c.speaker = SpeakerType::Vintage30;
    c.mic = MicType::C414;
    CHECK(IrLibrary::findCabIr(c) ==
          root.getChildFile("4x12/celestion_vintage_30_neumann_u87_cap_edge_close.wav"));
    c.cabinet = CabinetType::AcousticDI;
    CHECK(IrLibrary::findCabIr(c) ==
          root.getChildFile("4x12/celestion_vintage_30_neumann_u87_cap_edge_close.wav"));
    c.cabinet = CabinetType::Cab4x12;
    c.speaker = SpeakerType::EVM12L;
    CHECK(IrLibrary::findCabIr(c) ==
          root.getChildFile("4x12/celestion_g12t_75_neumann_u87_cap_edge_close.wav"));
    c.speaker = SpeakerType::Vintage30;
    c.mic = MicType::SM7B;
    CHECK(IrLibrary::findCabIr(c) ==
          root.getChildFile("4x12/celestion_vintage_30_shure_sm57_cap_edge_close.wav"));
}

LUTHIER_TEST(CabinetAssetKeys, invalidEnumsKeepExistingFallbackSemantics)
{
    CabinetConfig c;
    c.cabinet = static_cast<CabinetType>(-1);
    c.speaker = static_cast<SpeakerType>(-1);
    c.mic = static_cast<MicType>(-1);
    c.position = static_cast<MicPosition>(-1);
    c.distance = static_cast<MicDistance>(-1);
    CHECK(CabinetEngine::irFileNameFor(c) == "4x12/celestion_greenback_shure_sm57_cap_edge_close.wav");
    c.cabinet = CabinetType::NumCabinets;
    c.speaker = SpeakerType::NumSpeakers;
    c.mic = MicType::NumMics;
    c.position = MicPosition::NumPositions;
    c.distance = MicDistance::NumDistances;
    CHECK(CabinetEngine::irFileNameFor(c) == "4x12/bass_ceramic_akg_d112_cap_edge_close.wav");
}
