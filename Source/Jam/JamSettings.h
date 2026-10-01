#pragma once

/*  The 34 Jam parameters (jam-mode.md 10) as plain values: what the
    ParameterBridge reads from the APVTS each block and hands to JamEngine. */

namespace luthier
{

struct JamSettings
{
    bool enabled = false, play = false, fillNow = false;
    int style = 0, variation = 0, intensity = 3, fillEvery = 3;
    int follow = 1;
    bool predict = true;
    int chordSource = 0, startMode = 0, countInBars = 1;
    bool stopOnSilence = true;
    int silenceBars = 2;
    bool ending = true, dynamicsFollow = true;
    double swing = 0.0, humanise = 50.0;
    int kit = 0;
    bool kitAuto = true;
    double kitTuning = 0.0, kitDamping = 40.0, kitRoom = 25.0, kitWidth = 70.0;
    int perspective = 0, bassVoice = 0;
    double bassTone = 0.5, volumeDb = -6.0, balance = 0.0, drumsPan = 0.0, bassPan = 0.0;
    bool drumsMute = false, bassMute = false;
    int output = 0;
};

} // namespace luthier
