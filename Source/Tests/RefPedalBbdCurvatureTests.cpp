#include "TestFramework.h"
#include "../ReferencePedals/Common/BbdCore.h"
#include "../ReferencePedals/Models/boss_ce2.h"
#include <array>
#include <complex>

using namespace luthier::tests;
using luthier::reference::common::BbdCore;
using luthier::reference::models::BossCe2;

LUTHIER_TEST (RefPedalBbdCurvature, disabledCurvatureAndUnclockedModePreserveSamples)
{
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0})
    {
        BbdCore plain, zero, inactive;
        for (auto* core : {&plain, &zero, &inactive})
        {
            core->prepare (rate, 12.0);
            core->setClipOnset (0.5);
        }
        zero.setTransferCurvature (0.0);
        inactive.setTransferCurvature (0.0151);
        bool exact = true;
        for (int i = 0; i < 16000; ++i)
        {
            const double input = 0.6 * std::sin (i * 0.13) + 0.1 * std::cos (i * 0.47);
            const double reference = plain.process (input, 3.4, 0.0, false);
            exact &= reference == zero.process (input, 3.4, 0.0, false);
            exact &= reference == inactive.process (input, 3.4, 0.0, false);
        }
        CHECK (exact);
    }
}

LUTHIER_TEST (RefPedalBbdCurvature, resetReproducesFreshCoreAndSilenceHasNoDc)
{
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0})
    {
        BbdCore used, fresh;
        for (auto* core : {&used, &fresh})
        {
            core->prepare (rate, 12.0);
            core->setClipOnset (0.5);
            core->setTransferCurvature (0.0151);
        }
        for (int i = 0; i < 10000; ++i)
            used.process (0.25 * std::sin (i * 0.13), 3.4, 0.0, true);
        used.reset();
        bool exact = true;
        for (int i = 0; i < 10000; ++i)
        {
            const double input = 0.25 * std::sin (i * 0.17);
            exact &= used.process (input, 3.4, 0.0, true) == fresh.process (input, 3.4, 0.0, true);
        }
        CHECK (exact);
        used.reset();
        double peak = 0.0;
        for (int i = 0; i < 2000; ++i)
            peak = std::max (peak, std::abs (used.process (0.0, 3.4, 0.0, true)));
        CHECK (peak == 0.0);
    }
}

LUTHIER_TEST (RefPedalBbdCurvature, clockedPedalIsIndependentOfBlockPartition)
{
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
    {
        BossCe2 blocks, samples;
        for (auto* pedal : {&blocks, &samples})
        {
            pedal->setParam (4, 1.0f);
            pedal->prepare (rate, 512, 1);
        }
        std::array<double, 512> left{}, right{}, singleLeft{}, singleRight{};
        bool exact = true;
        for (int block = 0; block < 32; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                left[i] = singleLeft[i] = 0.25 * std::sin ((block * 512 + i) * 0.13);
                right[i] = singleRight[i] = 0.15 * std::cos ((block * 512 + i) * 0.17);
            }
            blocks.process (left.data(), right.data(), 512);
            for (int i = 0; i < 512; ++i)
                samples.process (&singleLeft[i], &singleRight[i], 1);
            exact &= left == singleLeft && right == singleRight;
        }
        CHECK (exact);
    }
}

LUTHIER_TEST (RefPedalBbdCurvature, wetDistortionIsFiniteAndAcCoupled)
{
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0})
    {
        BbdCore core;
        core.prepare (rate, 12.0);
        core.setClipOnset (0.5);
        core.setTransferCurvature (2.0 * 0.005 / (std::sqrt (2.0) * 0.78 * 9.0 / 15.0));
        std::array<std::complex<double>, 10> harmonics{};
        double mean = 0.0;
        bool finite = true;
        for (int i = 0; i < int (rate * 2); ++i)
        {
            const double angle = 2.0 * juce::MathConstants<double>::pi * 1000.0 * i / rate;
            const double output = core.process (std::pow (10.0, -12.0 / 20.0) * std::sin (angle), 3.4, 0.0, true);
            finite &= std::isfinite (output);
            if (i >= int (rate))
            {
                mean += output;
                const std::complex<double> rotation (std::cos (angle), -std::sin (angle));
                auto phase = rotation;
                for (int h = 1; h < 10; ++h)
                {
                    harmonics[h] += output * phase;
                    phase *= rotation;
                }
            }
        }
        double harmonicPower = 0.0;
        for (int h = 2; h < 10; ++h)
            harmonicPower += std::norm (harmonics[h]);
        const double thd = std::sqrt (harmonicPower) / std::abs (harmonics[1]);
        CHECK (finite);
        CHECK (std::abs (mean / rate) < 1.0e-5);
        // Regression for this estimated software transfer law, not a measured
        // hardware tolerance. The unchanged pedal-output acceptance gate is separate.
        CHECK (thd > 0.001 && thd < 0.003);
    }
}
