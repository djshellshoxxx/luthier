#include "TestFramework.h"
#include "../ReferencePedals/Midi/ExtendedHostAutomationCapture.h"

using namespace luthier::tests;
using namespace luthier::reference;

LUTHIER_TEST (RefPedalHostCapture, zeroGainRemainsExactAcrossHostCapture)
{
    ExtendedHostAutomationCapture capture;
    for (auto field : {ExtendedHostField::ReampGainDb, ExtendedHostField::Lane1GainDb,
                       ExtendedHostField::Lane2GainDb, ExtendedHostField::Lane3GainDb,
                       ExtendedHostField::Lane4GainDb, ExtendedHostField::Loop1SendDb,
                       ExtendedHostField::Loop1ReturnDb, ExtendedHostField::Loop2SendDb,
                       ExtendedHostField::Loop2ReturnDb})
    {
        const int index = (int) field;
        const auto& parameter = extendedHostParameters[(size_t) index];
        volatile float hostValue = -parameter.minimum / (parameter.maximum - parameter.minimum);
        CHECK (capture.write (index, hostValue));
        CHECK_MSG (capture.value ((size_t) index) == 0.f, juce::String (parameter.id) + " must retain exact 0 dB");
    }
}

LUTHIER_TEST (RefPedalHostCapture, allControlsAreMonotonicWithExactEndpoints)
{
    ExtendedHostAutomationCapture capture;
    for (size_t index = 0; index < extendedHostParameters.size(); ++index)
    {
        if (index == (size_t) ExtendedHostField::FeedbackKill) continue;
        const auto& parameter = extendedHostParameters[index];
        capture.write ((int) index, 0.f);
        CHECK (capture.value (index) == parameter.minimum);
        float previous = capture.value (index);
        bool monotonic = true;
        for (int step = 1; step <= 100; ++step)
        {
            volatile float hostValue = (float) step / 100.f;
            capture.write ((int) index, hostValue);
            const auto value = capture.value (index);
            monotonic &= std::isfinite (value) && value >= previous;
            previous = value;
        }
        CHECK (monotonic);
        CHECK (capture.value (index) == parameter.maximum);
    }
}
