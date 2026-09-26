#include "ComboHarness.h"
using namespace luthier; using namespace luthier::tests; using namespace luthier::combo;
LUTHIER_TEST (Zz, scratch)
{
    Rig rig; auto& presets = rig.p().getPresetManager();
    int a = presets.indexOfPreset ("J-Style Fingerstyle"), b = presets.indexOfPreset ("P-Bass Flatwound");
    for (int pair = 0; pair < 3; ++pair) {
      int from = pair == 0 ? a : pair == 1 ? b : a, to = pair == 0 ? b : pair == 1 ? a : a;
      rig.p().resetEverything(); presets.loadPreset (from); rig.apply(); rig.processSilence (4);
      std::vector<TimedMidi> ev { {0, juce::MidiMessage::noteOn (1, 52, (juce::uint8)100)}, {(int)(1.0*kSr), juce::MidiMessage::noteOff (1,52)} };
      const int sw = (int)(0.5*kSr);
      auto s = rig.renderEvents (ev, (int)(1.0*kSr), 0.2, [&](int){ presets.loadPreset (to); rig.apply(); }, sw);
      int at = ((sw + kBlock - 1)/kBlock)*kBlock;
      double mx=0; int where=0; for (int i = at-2000; i < at+2000; ++i) { double d = std::abs (s.mono[i]-s.mono[i-1]); if (d>mx){mx=d;where=i-at;} }
      std::cout << "pair " << pair << " maxstep " << mx << " at offset " << where << " level before " << Rig::windowRms (s.mono, at-4800, 4800) << " after " << Rig::windowRms (s.mono, at, 4800) << "\n";
      for (int i = at+66; i < at+80; ++i) std::cout << s.mono[i] << " "; std::cout << "\n";
    }
    ctx.checks++;
}
