#include "JamDrumKit.h"

namespace luthier
{

int getGmNote (DrumSound sound) noexcept
{
    switch (sound)
    {
        case DrumSound::kick:       return 36;
        case DrumSound::snare:
        case DrumSound::snareGhost:
        case DrumSound::snareBrush: return 38;
        case DrumSound::snareRim:   return 40;
        case DrumSound::tomHigh:    return 50;
        case DrumSound::tomMid:     return 47;
        case DrumSound::tomFloor:   return 45;
        case DrumSound::hatClosed:  return 42;
        case DrumSound::hatOpen:    return 46;
        case DrumSound::hatPedal:   return 44;
        case DrumSound::ride:       return 51;
        case DrumSound::rideBell:   return 53;
        case DrumSound::crash:      return 49;
        case DrumSound::rim:
        case DrumSound::sticks:     return 37;
        case DrumSound::shaker:     return 82;
        case DrumSound::numSounds:  break;
    }

    return 38;
}

const char* getDrumSoundName (DrumSound sound) noexcept
{
    static const char* const names[] = { "Kick", "Snare", "Snare ghost", "Rim shot", "Brush",
                                         "High tom", "Mid tom", "Floor tom",
                                         "Closed hat", "Open hat", "Pedal hat",
                                         "Ride", "Ride bell", "Crash",
                                         "Rim", "Sticks", "Shaker" };
    const int i = (int) sound;
    return juce::isPositiveAndBelow (i, (int) DrumSound::numSounds) ? names[i] : "";
}

const char* getJamKitName (int kitIndex) noexcept
{
    static const char* const names[] = { "Studio", "Vintage", "Arena", "Jazz", "Machine" };
    return names[juce::jlimit (0, 4, kitIndex)];
}

namespace
{
    /** jam-mode 5's kit table. */
    struct KitDesign
    {
        double kickF0, snareF0, floorTomF0;
        double kickT60, snareT60, tomT60;
        double kickSweep, kickDrop, kickPulse;
        double cymbalBrightness, cymbalDecay, hatHz;
        double roomRt;
    };

    const KitDesign& kitDesign (int kit) noexcept
    {
        static const KitDesign designs[] =
        {
            //  kick  snare floor | kickT snareT tomT | sweep drop  pulse   | bright decay hatHz | room
            {   55.0, 200.0, 82.0,  0.45, 0.32, 0.65,   0.040, 0.25, 0.0015,  1.00, 1.00, 330.0,  0.25 },   // Studio: tight, medium damping
            {   62.0, 185.0, 90.0,  0.60, 0.42, 0.85,   0.040, 0.22, 0.0015,  0.60, 1.10, 300.0,  0.30 },   // Vintage: looser heads, darker cymbals
            {   48.0, 175.0, 70.0,  0.55, 0.40, 1.10,   0.045, 0.25, 0.0012,  0.90, 1.20, 320.0,  0.60 },   // Arena: low, long toms, more room
            {   78.0, 240.0, 110.0, 0.70, 0.36, 0.70,   0.030, 0.12, 0.0015,  0.80, 1.15, 350.0,  0.35 },   // Jazz: 18" kick, ride-led
            {   50.0, 220.0, 85.0,  0.30, 0.18, 0.35,   0.120, 0.90, 0.0006,  1.10, 0.50, 380.0,  0.10 },   // Machine: short, strong sweeps
        };

        return designs[juce::jlimit (0, 4, kit)];
    }
}

//==============================================================================
void JamDrumKit::prepare (double sampleRate, int)
{
    sr = sampleRate;

    for (auto& v : kick)  v.prepare (sr);
    for (auto& v : snare) v.prepare (sr);
    kickTail.prepare (sr);
    snareTail.prepare (sr);

    for (int t = 0; t < 3; ++t)
    {
        for (auto& v : toms[(size_t) t])
            v.prepare (sr);

        tomTail[(size_t) t].prepare (sr);
    }

    hat.prepare (sr, CymbalPiece::Kind::hat);
    ride.prepare (sr, CymbalPiece::Kind::ride);
    crash.prepare (sr, CymbalPiece::Kind::crash);
    rim.prepare (sr);
    shaker.prepare (sr);
    room.prepare (sr);

    tailStep = 1.0 / juce::jmax (1.0, 0.002 * sr);   // 2 ms steal fade

    applyKit();
    reset();
}

void JamDrumKit::reset() noexcept
{
    for (auto& v : kick)  v.reset();
    for (auto& v : snare) v.reset();
    kickTail.reset();
    snareTail.reset();
    kickTailGain = snareTailGain = 0.0;

    for (int t = 0; t < 3; ++t)
    {
        for (auto& v : toms[(size_t) t])
            v.reset();

        tomTail[(size_t) t].reset();
        tomTailGain[(size_t) t] = 0.0;
    }

    hat.reset();
    ride.reset();
    crash.reset();
    rim.reset();
    shaker.reset();
    room.reset();

    fadeGain = 1.0;
    fading = false;
    lastPeak = 0.0;
}

void JamDrumKit::setSeed (uint64_t newSeed) noexcept
{
    seed = newSeed;
    snare[0].setSeed (seed * 31 + 1);
    snare[1].setSeed (seed * 31 + 2);
    shaker.setSeed (seed * 31 + 3);
    applyKit();
}

void JamDrumKit::setKit (int kitIndex) noexcept
{
    kitIndex = juce::jlimit (0, (int) JamKitStyle::numKits - 1, kitIndex);

    if (kitIndex == kit)
        return;

    kit = kitIndex;
    applyKit();
}

void JamDrumKit::setTuning (double semitones, double damping01) noexcept
{
    tuningSemitones = juce::jlimit (-12.0, 12.0, semitones);
    damping = juce::jlimit (0.0, 1.0, damping01);

    // A tension change moves every f0 and keeps the ratios; damping scales the
    // membranes' T60 (head muffling): 40 % is the kit as designed.
    const double ratio = semitonesToRatio (tuningSemitones);
    const double t60Scale = std::pow (2.0, (0.4 - damping) / 0.3);

    for (auto& v : kick)  v.setTuning (ratio, t60Scale);
    for (auto& v : snare) v.setTuning (ratio, t60Scale);

    for (auto& pair : toms)
        for (auto& v : pair)
            v.setTuning (ratio, t60Scale);

    rim.setBatterHz (kitDesign (kit).snareF0 * ratio);
}

double JamDrumKit::getKickF0() const noexcept
{
    return kitDesign (kit).kickF0 * semitonesToRatio (tuningSemitones);
}

void JamDrumKit::setReduced (bool reduced) noexcept
{
    hat.setReduced (reduced);
    ride.setReduced (reduced);
    crash.setReduced (reduced);
}

void JamDrumKit::applyKit() noexcept
{
    const auto& k = kitDesign (kit);

    MembranePiece::Design kickDesign;
    kickDesign.numModes = 6;
    kickDesign.f0 = k.kickF0;
    kickDesign.t60 = k.kickT60;
    kickDesign.pitchDropK = k.kickDrop;
    kickDesign.sweepSeconds = k.kickSweep;
    kickDesign.pulseSeconds = k.kickPulse;
    kickDesign.decays = { { 1.0, 0.55, 0.45, 0.40, 0.34, 0.30, 0.28, 0.26 } };
    kickDesign.gains  = { { 1.0, 0.45, 0.32, 0.22, 0.16, 0.12, 0.1, 0.1 } };
    kickDesign.resonantHead = true;
    kickDesign.level = 1.4;

    MembranePiece::Design snareDesign;
    snareDesign.numModes = 8;
    snareDesign.f0 = k.snareF0;
    snareDesign.t60 = k.snareT60;
    snareDesign.pitchDropK = 0.06;
    snareDesign.sweepSeconds = 0.02;
    snareDesign.pulseSeconds = 0.0006;
    snareDesign.level = 0.9;

    for (auto& v : kick)  v.setDesign (kickDesign);
    for (auto& v : snare) v.setDesign (snareDesign);

    // Toms a 4th and a 3rd apart: floor, mid = floor x 4/3, high = mid x 5/4.
    const double tomF0[3] = { k.floorTomF0 * (4.0 / 3.0) * 1.25, k.floorTomF0 * (4.0 / 3.0), k.floorTomF0 };

    for (int t = 0; t < 3; ++t)
    {
        MembranePiece::Design tom;
        tom.numModes = 6;
        tom.f0 = tomF0[t];
        tom.t60 = k.tomT60;
        tom.pitchDropK = 0.12;
        tom.sweepSeconds = 0.05;
        tom.pulseSeconds = 0.0008;
        tom.level = 0.8;

        for (auto& v : toms[(size_t) t])
            v.setDesign (tom);
    }

    CymbalPiece::Design hatDesign;
    hatDesign.baseHz = k.hatHz;
    hatDesign.brightness = k.cymbalBrightness;
    hatDesign.decayScale = k.cymbalDecay;
    hatDesign.level = 0.5;
    hat.setDesign (hatDesign, seed + 11);

    CymbalPiece::Design rideDesign;
    rideDesign.baseHz = 300.0;
    rideDesign.topHz = 14000.0;
    rideDesign.brightness = k.cymbalBrightness;
    rideDesign.decayScale = k.cymbalDecay;
    rideDesign.level = 0.45;
    ride.setDesign (rideDesign, seed + 12);

    CymbalPiece::Design crashDesign;
    crashDesign.baseHz = 350.0;
    crashDesign.topHz = 15000.0;
    crashDesign.brightness = k.cymbalBrightness;
    crashDesign.decayScale = k.cymbalDecay;
    crashDesign.level = 0.5;
    crash.setDesign (crashDesign, seed + 13);

    room.setDecay (k.roomRt);
    setTuning (tuningSemitones, damping);
}

JamDrumKit::Pan JamDrumKit::panFor (DrumSound sound) const noexcept
{
    // Audience perspective: the hat on the right (5). Drummer mirrors it.
    double position = 0.0;

    switch (sound)
    {
        case DrumSound::kick:       position = 0.0;   break;
        case DrumSound::snare:
        case DrumSound::snareGhost:
        case DrumSound::snareRim:
        case DrumSound::snareBrush:
        case DrumSound::rim:
        case DrumSound::sticks:     position = 0.12;  break;
        case DrumSound::tomHigh:    position = 0.22;  break;
        case DrumSound::tomMid:     position = -0.10; break;
        case DrumSound::tomFloor:   position = -0.40; break;
        case DrumSound::hatClosed:
        case DrumSound::hatOpen:
        case DrumSound::hatPedal:   position = 0.50;  break;
        case DrumSound::ride:
        case DrumSound::rideBell:   position = -0.45; break;
        case DrumSound::crash:      position = -0.28; break;
        case DrumSound::shaker:     position = 0.30;  break;
        case DrumSound::numSounds:  break;
    }

    position *= width * (drummerView ? -1.0 : 1.0);
    const double angle = (position + 1.0) * constants::kPi * 0.25;
    return { std::cos (angle), std::sin (angle) };
}

//==============================================================================
void JamDrumKit::trigger (DrumSound sound, double velocity) noexcept
{
    velocity = juce::jlimit (0.0, 1.0, velocity);
    ++hitCounter;

    // Two-voice pieces: an idle voice, else steal the oldest into the tail.
    auto strikeMembrane = [this, velocity] (std::array<MembranePiece, 2>& voices, std::array<int64_t, 2>& ages,
                                            MembranePiece& tail, double& tailGain)
    {
        int use = ! voices[0].isActive() ? 0 : ! voices[1].isActive() ? 1 : (ages[0] <= ages[1] ? 0 : 1);

        if (voices[(size_t) use].isActive())
        {
            tail = voices[(size_t) use];
            tailGain = 1.0;
            voices[(size_t) use].reset();
        }

        voices[(size_t) use].strike (velocity);
        ages[(size_t) use] = hitCounter;
    };

    switch (sound)
    {
        case DrumSound::kick:
            strikeMembrane (kick, kickAge, kickTail, kickTailGain);
            break;

        case DrumSound::snare:
        case DrumSound::snareGhost:
        case DrumSound::snareRim:
        case DrumSound::snareBrush:
        {
            int use = ! snare[0].isActive() ? 0 : ! snare[1].isActive() ? 1 : (snareAge[0] <= snareAge[1] ? 0 : 1);

            if (snare[(size_t) use].isActive())
            {
                snareTail = snare[(size_t) use];
                snareTailGain = 1.0;
                snare[(size_t) use].reset();
            }

            const auto stroke = sound == DrumSound::snareRim   ? SnarePiece::Stroke::rimshot
                              : sound == DrumSound::snareBrush ? SnarePiece::Stroke::brush
                              : sound == DrumSound::snareGhost ? SnarePiece::Stroke::ghost
                                                               : SnarePiece::Stroke::normal;
            snare[(size_t) use].strike (velocity, stroke);
            snareAge[(size_t) use] = hitCounter;
            break;
        }

        case DrumSound::tomHigh:
        case DrumSound::tomMid:
        case DrumSound::tomFloor:
        {
            const int t = sound == DrumSound::tomHigh ? 0 : sound == DrumSound::tomMid ? 1 : 2;
            strikeMembrane (toms[(size_t) t], tomAge[(size_t) t], tomTail[(size_t) t], tomTailGain[(size_t) t]);
            break;
        }

        case DrumSound::hatClosed: hat.strike (velocity, CymbalPiece::Hit::closed); break;
        case DrumSound::hatOpen:   hat.strike (velocity, CymbalPiece::Hit::open);   break;
        case DrumSound::hatPedal:  hat.strike (velocity, CymbalPiece::Hit::pedal);  break;
        case DrumSound::ride:      ride.strike (velocity, CymbalPiece::Hit::bow);   break;
        case DrumSound::rideBell:  ride.strike (velocity, CymbalPiece::Hit::bell);  break;
        case DrumSound::crash:     crash.strike (velocity, CymbalPiece::Hit::crash); break;
        case DrumSound::rim:       rim.strike (velocity, RimPiece::Hit::rim);       break;
        case DrumSound::sticks:    rim.strike (velocity, RimPiece::Hit::sticks);    break;
        case DrumSound::shaker:    shaker.strike (velocity);                        break;
        case DrumSound::numSounds: break;
    }
}

void JamDrumKit::chokeAll (double seconds) noexcept
{
    fadeStep = 1.0 / juce::jmax (1.0, seconds * sr);
    fading = true;
}

void JamDrumKit::chokeCymbals (double seconds) noexcept
{
    crash.choke (seconds, 0.08);
    ride.choke (seconds, 0.08);
    hat.choke (seconds, 0.05);
}

bool JamDrumKit::isSilent() const noexcept
{
    for (auto& v : kick)  if (v.isActive()) return false;
    for (auto& v : snare) if (v.isActive()) return false;

    for (auto& pair : toms)
        for (auto& v : pair)
            if (v.isActive())
                return false;

    return ! (hat.isActive() || ride.isActive() || crash.isActive() || rim.isActive() || shaker.isActive()
              || kickTailGain > 0.0 || snareTailGain > 0.0);
}

void JamDrumKit::render (double* left, double* right, int n) noexcept
{
    const auto pKick  = panFor (DrumSound::kick);
    const auto pSnare = panFor (DrumSound::snare);
    const auto pHat   = panFor (DrumSound::hatClosed);
    const auto pRide  = panFor (DrumSound::ride);
    const auto pCrash = panFor (DrumSound::crash);
    const auto pRim   = panFor (DrumSound::rim);
    const auto pShake = panFor (DrumSound::shaker);
    const Pan pTom[3] = { panFor (DrumSound::tomHigh), panFor (DrumSound::tomMid), panFor (DrumSound::tomFloor) };

    double peak = 0.0;

    for (int i = 0; i < n; ++i)
    {
        if (--housekeepCountdown <= 0)
        {
            housekeepCountdown = 64;

            for (auto& v : kick)  v.housekeep();
            for (auto& v : snare) v.housekeep();
            kickTail.housekeep();
            snareTail.housekeep();

            for (int t = 0; t < 3; ++t)
            {
                for (auto& v : toms[(size_t) t])
                    v.housekeep();

                tomTail[(size_t) t].housekeep();
            }

            hat.housekeep();
            ride.housekeep();
            crash.housekeep();
            rim.housekeep();
            shaker.housekeep();
        }

        double l = 0.0, r = 0.0;

        const double k = kick[0].process() + kick[1].process()
                       + (kickTailGain > 0.0 ? kickTail.process() * kickTailGain : 0.0);
        const double s = snare[0].process() + snare[1].process()
                       + (snareTailGain > 0.0 ? snareTail.process() * snareTailGain : 0.0);

        kickTailGain = juce::jmax (0.0, kickTailGain - tailStep);
        snareTailGain = juce::jmax (0.0, snareTailGain - tailStep);

        l += k * pKick.left + s * pSnare.left;
        r += k * pKick.right + s * pSnare.right;

        for (int t = 0; t < 3; ++t)
        {
            const double tom = toms[(size_t) t][0].process() + toms[(size_t) t][1].process()
                             + (tomTailGain[(size_t) t] > 0.0 ? tomTail[(size_t) t].process() * tomTailGain[(size_t) t] : 0.0);
            tomTailGain[(size_t) t] = juce::jmax (0.0, tomTailGain[(size_t) t] - tailStep);
            l += tom * pTom[t].left;
            r += tom * pTom[t].right;
        }

        const double h = hat.process(), rd = ride.process(), c = crash.process();
        const double rm = rim.process(), sh = shaker.process();

        l += h * pHat.left + rd * pRide.left + c * pCrash.left + rm * pRim.left + sh * pShake.left;
        r += h * pHat.right + rd * pRide.right + c * pCrash.right + rm * pRim.right + sh * pShake.right;

        if (roomSend > 0.0)
            room.process ((l + r) * 0.5 * roomSend, l, r);

        if (fading)
        {
            fadeGain -= fadeStep;

            if (fadeGain <= 0.0)
            {
                // The hand is on every piece: everything stops here.
                reset();
                l = r = 0.0;
                fadeGain = 1.0;
                fading = false;
            }
        }

        const double g = fading ? fadeGain : 1.0;
        left[i] = sanitise (l * g);
        right[i] = sanitise (r * g);
        peak = juce::jmax (peak, std::abs (left[i]), std::abs (right[i]));
    }

    lastPeak = peak;
}

} // namespace luthier
