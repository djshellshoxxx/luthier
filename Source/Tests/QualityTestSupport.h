#pragma once

/*  cpu-quality-modes.md 12: helpers shared by the CPU-quality tests. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../Support/QualityProfile.h"
#include "../Support/PerformanceSettings.h"
#include "../DSP/Common/IrVariants.h"
#include "../DSP/Amp/AmpEngine.h"
#include "../DSP/Amp/CabinetEngine.h"
#include "../DSP/Body/BodyEngine.h"
#include "../DSP/Effects/Pedal.h"
#include "../DSP/Effects/PedalsDrive.h"
#include "../DSP/String/StringEngine.h"

#include <cmath>
#include <vector>

#if JUCE_LINUX
 #include <malloc.h>
#endif

namespace luthier::QualityTestSupport
{

/** The Source folder, found from this file's own path. */
inline juce::File sourceRoot()
{
    return juce::File (juce::String (__FILE__)).getParentDirectory().getParentDirectory();
}

/** Points PerformanceSettings at a fresh file for the life of a test, so the
    tests never touch the real Documents/Luthier/config/performance.json. */
struct ScopedTempSettings
{
    ScopedTempSettings()
    {
        folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("luthier-perf-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        folder.createDirectory();
        PerformanceSettings::get().setFileForTesting (folder.getChildFile ("performance.json"));
        PerformanceSettings::get().resetToDefaults();
    }

    ~ScopedTempSettings()
    {
        PerformanceSettings::get().setFileForTesting (folder.getChildFile ("performance.json"));
        PerformanceSettings::get().resetToDefaults();
        folder.deleteRecursively();
    }

    juce::File folder;
    PerformanceSettings::Values scratch;
};

inline bool setChoice (LuthierAudioProcessor& p, const juce::String& id, int index)
{
    auto* prm = p.getState().getParameter (id);

    if (prm == nullptr)
        return false;

    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
    return true;
}

inline void insertDrivePedal (LuthierAudioProcessor& p, PedalType type = PedalType::Overdrive, int slot = 0)
{
    setChoice (p, ParamIDs::slotType (false, slot), (int) type);
    p.getParameterBridge().applyAllNow();
}

/** First index at or above `fraction` of the peak. */
inline int onset (const std::vector<float>& x, double fraction)
{
    float peak = 0.0f;

    for (auto v : x)
        peak = juce::jmax (peak, std::abs (v));

    if (peak <= 0.0f)
        return -1;

    for (size_t i = 0; i < x.size(); ++i)
        if (std::abs (x[i]) >= (float) fraction * peak)
            return (int) i;

    return -1;
}

/** One string at `hz` under `profile`: its fundamental and tenth partial. */
inline void measureString (const StringEngine::Physical& physical, double hz, const QualityProfile& profile,
                           double& f0, double& p10)
{
    constexpr double sr = 48000.0;
    constexpr int n = 1 << 17;

    StringEngine s;
    s.prepare (sr, 512);
    s.setPhysical (physical);
    s.setSustainScale (4.0);   // long enough for the tenth partial to be measured
    s.setDispersionRule (profile.fourStageFromHz, profile.twoStageFromHz);
    s.snapToFrequency (hz);
    s.excite (Excitation::Params {});

    for (int i = 0; i < 1024; ++i)
        s.processSample (0.0);

    std::vector<double> buf ((size_t) n);

    for (int i = 0; i < n; ++i)
        buf[(size_t) i] = s.processSample (0.0);

    f0 = tests::findPeakFrequency (buf.data(), n, sr, hz * 0.9, hz * 1.1);
    const auto partials = tests::findPartials (buf.data(), n, sr, 10, f0);
    p10 = partials.size() >= 10 ? partials[9] : 0.0;
}

/** The largest alias component relative to the fundamental, in dBc, of a
    stage fed a ~1 kHz sine at 44.1 kHz. The sine sits exactly on an FFT bin,
    so its harmonics land on bins and whatever is between them is alias. */
template <typename ProcessFn>
inline double aliasDbc (ProcessFn&& process, double amplitude, double maxHz = 22050.0)
{
    constexpr double sr = 44100.0;
    constexpr int order = 16, n = 1 << order;
    constexpr int k = 1486;   // 1486 * 44100 / 65536 = 999.96 Hz
    const double w = 2.0 * juce::MathConstants<double>::pi * (double) k / (double) n;

    // Warm up, then the analysed window.
    for (int i = 0; i < (int) sr; ++i)
        process (amplitude * std::sin (w * i));

    std::vector<float> fft ((size_t) n * 2, 0.0f);

    for (int i = 0; i < n; ++i)
    {
        const double x = process (amplitude * std::sin (w * (i + (int) sr)));
        const double a = (double) i / (double) (n - 1);
        const double bh = 0.35875 - 0.48829 * std::cos (2.0 * juce::MathConstants<double>::pi * a)
                          + 0.14128 * std::cos (4.0 * juce::MathConstants<double>::pi * a)
                          - 0.01168 * std::cos (6.0 * juce::MathConstants<double>::pi * a);
        fft[(size_t) i] = (float) (x * bh);
    }

    juce::dsp::FFT transform (order);
    transform.performFrequencyOnlyForwardTransform (fft.data());

    const double fundamental = (double) fft[(size_t) k] * fft[(size_t) k];
    double worst = 0.0;

    const int lastBin = juce::jmin (n / 2, (int) (maxHz / sr * n));

    for (int b = 8; b < lastBin; ++b)
    {
        const int nearest = (int) std::round ((double) b / (double) k);
        const bool harmonic = nearest >= 1 && std::abs (b - nearest * k) <= 4;

        if (! harmonic)
            worst = juce::jmax (worst, (double) fft[(size_t) b] * fft[(size_t) b]);
    }

    return 10.0 * std::log10 (juce::jmax (1.0e-30, worst) / juce::jmax (1.0e-30, fundamental));
}

inline double ampAliasDbc (AmpModel model, double gain, int factor, double maxHz = 22050.0)
{
    AmpEngine amp;
    amp.prepare (44100.0, 512);
    amp.setModel (model);
    amp.setGain (gain);
    amp.setOversamplingFactor (factor);
    amp.reset();

    return aliasDbc ([&amp] (double x) { return amp.processSample (x); }, 0.25, maxHz);
}

inline double pedalAliasDbc (PedalType type, int factor)
{
    auto pedal = Pedal::create (type);
    auto* drive = dynamic_cast<DrivePedalBase*> (pedal.get());

    if (drive == nullptr)
        return 0.0;

    drive->prepare (44100.0, 512);
    drive->resetParametersToDefault();
    drive->setOversamplingFactor (factor);
    drive->reset();

    return aliasDbc ([drive] (double x)
    {
        double l = x, r = x;
        drive->process (&l, &r, 1);
        return l;
    }, 0.25);
}

/** RSS in MB, or -1 where it cannot be read. */
inline double residentMegabytes()
{
   #if JUCE_LINUX
    const auto statm = juce::File ("/proc/self/statm").loadFileAsString();
    const auto fields = juce::StringArray::fromTokens (statm, " ", "");

    if (fields.size() >= 2)
        return (double) fields[1].getLargeIntValue() * (double) getpagesize() / (1024.0 * 1024.0);
   #endif

    return -1.0;
}

/** Heap bytes in use, in MB (glibc's mallinfo2 over every arena, mmapped
    chunks included), or -1 where it cannot be read. What a set of live
    objects holds, without the noise RSS picks up from freed pages. */
inline double heapInUseMegabytes()
{
   #if JUCE_LINUX && defined (__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
    const auto info = mallinfo2();
    return (double) (info.uordblks + info.hblkhd) / (1024.0 * 1024.0);
   #else
    return -1.0;
   #endif
}

/** How much memory stays held while every IR slot (body, both cabinet mics)
    is filled with a 4 s response, with or without the variants. */
inline double memoryHeldWithFourSecondIrs (bool withVariants)
{
    IrVariants::setEnabledForTesting (withVariants);

    const double before = heapInUseMegabytes();
    double after = -1.0;

    {
        BodyEngine body;
        CabinetEngine cabinet;
        body.prepare (48000.0, 256);
        cabinet.prepare (48000.0, 256);

        std::vector<float> ir ((size_t) (4.0 * 48000.0));
        juce::Random rng (3);

        // A 4 s response that falls 60 dB over its first 2 s, like a long
        // room-and-cabinet capture, then keeps a quiet tail to the end.
        for (size_t i = 0; i < ir.size(); ++i)
            ir[i] = (rng.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-6.9 * (double) i / (2.0 * 48000.0));

        body.loadImpulseResponse (ir.data(), (int) ir.size(), 48000.0);
        cabinet.loadImpulseResponse (0, ir.data(), (int) ir.size(), 48000.0);
        cabinet.loadImpulseResponse (1, ir.data(), (int) ir.size(), 48000.0);

        // Let the convolutions' background threads retire what they replaced.
        juce::Thread::sleep (200);
        after = heapInUseMegabytes();

        if (withVariants)
            std::cout << "    variants held (estimate): body " << body.getIrVariants().getHeldMegabytes()
                      << " MB, cabinet " << cabinet.getIrVariants (0).getHeldMegabytes() << " + "
                      << cabinet.getIrVariants (1).getHeldMegabytes() << " MB" << std::endl;
    }

    IrVariants::setEnabledForTesting (true);

    return (before >= 0.0 && after >= 0.0) ? after - before : -1.0;
}

} // namespace luthier::QualityTestSupport
