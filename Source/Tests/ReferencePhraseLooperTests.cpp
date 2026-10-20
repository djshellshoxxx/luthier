// Standalone looper-state regression. No hardware gesture/fidelity claims.
#include "TestFramework.h"
#include "../ReferencePedals/PhraseLooper.h"
#include <cmath>
using namespace luthier::reference;

LUTHIER_TEST (ReferencePhraseLoop, recordThenPlayAndDryThrough)
{
    PhraseLooper l;
    l.prepare (10000.0, 1.0);
    double left[4] {0.1,0.2,0.3,0.4}, right[4] {0.5,0.6,0.7,0.8};
    l.request (PhraseLooper::Command::record);
    l.processStereo (left, right, 4);
    CHECK (l.getState() == PhraseLooper::State::recording);
    CHECK (l.getRecordedSamples() == 4);
    CHECK (std::abs (left[0] - 0.1) < 1e-9);
    double silentL[4] {}, silentR[4] {};
    l.request (PhraseLooper::Command::play);
    l.processStereo (silentL, silentR, 4);
    for (int i = 0; i < 4; ++i)
    {
        CHECK (std::abs (silentL[i] - (0.1 + i * 0.1)) < 1e-6);
        CHECK (std::abs (silentR[i] - (0.5 + i * 0.1)) < 1e-6);
    }
}

LUTHIER_TEST (ReferencePhraseLoop, stopResumeAndInstantClear)
{
    PhraseLooper l;
    l.prepare (48000.0, 0.02);
    double input[2] {0.25,0.5}, rhs[2] {0.75,1.0};
    l.request (PhraseLooper::Command::record);
    l.processStereo (input, rhs, 2);
    l.request (PhraseLooper::Command::stop);
    double silent[2] {}, silentR[2] {};
    l.processStereo (silent, silentR, 2);
    CHECK (l.getState() == PhraseLooper::State::stopped);
    CHECK (l.getRecordedSamples() == 2);
    l.request (PhraseLooper::Command::play);
    l.processStereo (silent, silentR, 2);
    CHECK (std::abs (silent[0] - 0.25) < 1e-6);
    l.request (PhraseLooper::Command::clear);
    l.processStereo (silent, silentR, 2);
    CHECK (l.getState() == PhraseLooper::State::empty);
    CHECK (l.getRecordedSamples() == 0);
}

LUTHIER_TEST (ReferencePhraseLoop, overdubChangesPlaybackWithoutChangingLength)
{
    PhraseLooper l;
    l.prepare (48000.0, 0.03);
    double in[2] {0.1,0.2}, right[2] {0.1,0.2};
    l.request (PhraseLooper::Command::record);
    l.processStereo (in, right, 2);
    l.request (PhraseLooper::Command::play);
    double quiet[2] {}, quietR[2] {};
    l.processStereo (quiet, quietR, 2);
    l.request (PhraseLooper::Command::overdub);
    double add[2] {0.1,0.1}, addR[2] {0.1,0.1};
    l.processStereo (add, addR, 2);
    l.request (PhraseLooper::Command::play);
    double next[2] {}, nextR[2] {};
    l.processStereo (next, nextR, 2);
    CHECK (l.getRecordedSamples() == 2);
    CHECK (std::abs (next[0] - 0.2) < 1e-6);
    CHECK (std::abs (next[1] - 0.3) < 1e-6);
}

LUTHIER_TEST (ReferencePhraseLoop, zeroOrTinyBuffersDoNotOverrun)
{
    PhraseLooper l;
    l.prepare (48000.0, 0.01);
    l.request (PhraseLooper::Command::record);
    double data[4] {0.1,0.2,0.3,0.4};
    double other[4] {};
    l.processStereo (data, other, 0);
    for(int n : {1,2,4}) l.processStereo (data, other, n);
    CHECK (l.getRecordedSamples() == 7);
    for (double v : data) CHECK (std::isfinite (v));
}
