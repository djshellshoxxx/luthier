#include "TestFramework.h"
#include "../ReferencePedals/Common/GestureDecoder.h"
#include "../ReferencePedals/FootController.h"
#include "../ReferencePedals/Midi/ExprEngine.h"
using namespace luthier::reference;
using namespace luthier::reference::common;
LUTHIER_TEST (RefPedalMidiGesture, thresholdsAndDouble)
{
    GestureDecoder d; d.prepare (48000); d.setDoubleTapEnabled (false);
    Gesture g; d.down (0); d.up (100); CHECK (d.pop (g)); CHECK (g.kind == GestureKind::Tap);
    d.down (1000); d.advance (48999); CHECK (! d.pop (g));
    d.advance (49000); CHECK (d.pop (g)); CHECK (g.kind == GestureKind::Hold); CHECK (g.sample == 49000);
    d.reset(); d.setDoubleTapEnabled (true); d.down (0); d.up (100); d.down (1000);
    CHECK (d.pop (g)); CHECK (g.kind == GestureKind::DoubleTap);
    d.up (1100); d.advance (48000); CHECK (! d.pop (g));
}
LUTHIER_TEST (RefPedalMidiGesture, momentaryAutoDebounce)
{
    FootController f; f.prepare (48000); CHECK (f.press (0)); CHECK (f.release (10));
    CHECK (f.press (100)); // rejected contact bounce, remains engaged
    f.setMode (FootMode::Momentary); f.press (1000); CHECK (! f.release (2000));
    f.setMode (FootMode::Auto); CHECK (f.press (3000)); CHECK (! f.release (30000));
}
LUTHIER_TEST (RefPedalMidiGesture, expressionStepResponse)
{
    midi::ExprEngine e; e.prepare (48000); e.setTarget (0, 10); e.setTarget (1, 10);
    float v = 0; for (int i = 0; i < 480; ++i) v = e.next();
    CHECK (std::abs (v - .63212f) < .001f);
    CHECK (midi::ExprEngine::pitchBend (8192, true) == 0.f);
    CHECK (midi::ExprEngine::pitchBend (16383, true) == 1.f);
}
