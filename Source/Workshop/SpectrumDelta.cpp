#include "SpectrumDelta.h"
#include "../LuthierEngine.h"
#include "../Model/Workshop/PartAcoustics.h"

#include <juce_dsp/juce_dsp.h>

namespace luthier
{

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 512;
    constexpr int kFftOrder = 12;
    constexpr int kFftSize = 1 << kFftOrder;   // 4096 samples, 85 ms: 11.7 Hz bins, fine above 60 Hz

    /** The fixture pluck (section 6): the same note, string and velocity every time. */
    std::vector<float> fixtureRender (LuthierEngine& engine, const WorkshopGuitar& guitar, GuitarType type)
    {
        const int loadsBefore = engine.getIrLoadCount();
        engine.applyWorkshopGuitar (mapSpec (guitar), type);

        juce::AudioBuffer<float> buffer (2, kBlock);

        // The first note after a response is loaded comes out a little
        // different from every later one (TODO: known issue); a throwaway
        // pluck then makes renders of the same guitar match. Only when a
        // response actually loaded, so a pickup drag stays inside the budget.
        if (engine.getIrLoadCount() != loadsBefore)
        {
            engine.reset();

            for (int offset = 0; offset < kFftSize; offset += kBlock)
            {
                juce::MidiBuffer midi;
                if (offset == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);
                buffer.clear();
                engine.processBlock (buffer, midi);
            }
        }

        engine.reset();

        std::vector<float> out;
        out.reserve ((size_t) kFftSize);

        for (int offset = 0; offset < kFftSize; offset += kBlock)
        {
            juce::MidiBuffer midi;

            if (offset == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, guitar.family == "bass" ? 40 : 52, 0.8f), 0);

            buffer.clear();
            engine.processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
        }

        return out;
    }

    /** Power per band on the kNumPoints log grid, 1/6 octave wide. */
    std::vector<float> bandPowers (const std::vector<float>& signal, const std::vector<float>& centres)
    {
        juce::dsp::FFT fft (kFftOrder);
        std::vector<float> data ((size_t) kFftSize * 2, 0.0f);

        for (int i = 0; i < kFftSize && i < (int) signal.size(); ++i)
        {
            const float window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kFftSize - 1));
            data[(size_t) i] = signal[(size_t) i] * window;
        }

        fft.performFrequencyOnlyForwardTransform (data.data());

        const float binHz = (float) kRate / (float) kFftSize;
        std::vector<float> powers;

        for (auto centre : centres)
        {
            const float lo = centre * std::pow (2.0f, -1.0f / 12.0f), hi = centre * std::pow (2.0f, 1.0f / 12.0f);
            const int a = juce::jmax (1, (int) std::floor (lo / binHz)), b = juce::jmax (a, (int) std::ceil (hi / binHz));
            double sum = 0.0;

            for (int k = a; k <= b && k < kFftSize / 2; ++k)
                sum += (double) data[(size_t) k] * data[(size_t) k];

            powers.push_back ((float) (sum / (double) (b - a + 1)));
        }

        return powers;
    }

    std::vector<float> logGrid()
    {
        std::vector<float> f;
        for (int i = 0; i < SpectrumDelta::kNumPoints; ++i)
            f.push_back (60.0f * std::pow (12000.0f / 60.0f, (float) i / (float) (SpectrumDelta::kNumPoints - 1)));
        return f;
    }

    juce::int64 keyOf (const WorkshopGuitar& g, GuitarType type)
    {
        return (juce::JSON::toString (g.toEmbeddedVar(), true) + "|" + juce::String ((int) type)).hashCode64();
    }

    /** The committed guitar's bands, kept while it stays the same: during a drag only the candidate changes. */
    struct CommittedCache
    {
        juce::int64 key = 0;
        std::vector<float> powers;
    };

    SpectrumDelta::Result computeWith (LuthierEngine& engine, const WorkshopGuitar& committed,
                                       const WorkshopGuitar& candidate, GuitarType type, CommittedCache* cache = nullptr)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();

        SpectrumDelta::Result r;
        r.frequencies = logGrid();

        std::vector<float> a;
        const auto key = cache != nullptr ? keyOf (committed, type) : 0;

        if (cache != nullptr && cache->key == key && ! cache->powers.empty())
        {
            a = cache->powers;
        }
        else
        {
            a = bandPowers (fixtureRender (engine, committed, type), r.frequencies);

            if (cache != nullptr)
            {
                cache->key = key;
                cache->powers = a;
            }
        }

        const auto b = bandPowers (fixtureRender (engine, candidate, type), r.frequencies);

        // The quietest bands say nothing about tone: 60 dB under the loudest counts as floor.
        float peak = 1.0e-20f;
        for (size_t i = 0; i < a.size(); ++i)
            peak = juce::jmax (peak, a[i], b[i]);

        const float floor = peak * 1.0e-6f;

        double low = 0.0, mid = 0.0, high = 0.0;
        int nl = 0, nm = 0, nh = 0;

        for (size_t i = 0; i < a.size(); ++i)
        {
            const float d = 10.0f * std::log10 ((b[i] + floor) / (a[i] + floor));
            r.deltaDb.push_back (d);
            r.largestDb = juce::jmax (r.largestDb, std::abs (d));

            const float f = r.frequencies[i];
            if (f < 250.0f)         { low += d; ++nl; }
            else if (f < 2000.0f)   { mid += d; ++nm; }
            else                    { high += d; ++nh; }
        }

        r.noChange = r.largestDb < 0.5f;

        // Section 10: a sentence, not a curve.
        if (r.noChange)
        {
            r.summary = "No audible change: within 0.5 dB everywhere.";
        }
        else
        {
            low /= juce::jmax (1, nl);
            mid /= juce::jmax (1, nm);
            high /= juce::jmax (1, nh);

            auto db = [] (double v) { return juce::String (std::abs (v), 1) + " dB"; };

            if (std::abs (high) >= std::abs (low) && std::abs (high) >= std::abs (mid))
                r.summary = "Candidate is " + db (high) + (high > 0 ? " brighter" : " darker") + " above 2 kHz.";
            else if (std::abs (low) >= std::abs (mid))
                r.summary = "Candidate has " + db (low) + (low > 0 ? " more" : " less") + " bass below 250 Hz.";
            else
                r.summary = "Candidate's mids are " + db (mid) + (mid > 0 ? " louder" : " quieter") + " (250 Hz - 2 kHz).";
        }

        r.renderMs = juce::Time::getMillisecondCounterHiRes() - start;
        return r;
    }

    std::unique_ptr<LuthierEngine> makeEngine()
    {
        auto engine = std::make_unique<LuthierEngine>();
        engine->prepare (kRate, kBlock);
        return engine;
    }
}

//==============================================================================
SpectrumDelta::SpectrumDelta() : juce::Thread ("Luthier spectrum delta")
{
    startThread (juce::Thread::Priority::low);
}

SpectrumDelta::~SpectrumDelta()
{
    signalThreadShouldExit();
    wake.signal();
    stopThread (4000);
}

juce::uint32 SpectrumDelta::request (const WorkshopGuitar& committed, const WorkshopGuitar& candidate,
                                     GuitarType standsFor, std::vector<float> combNotchesHz)
{
    auto job = std::make_unique<Job>();
    job->committed = committed;
    job->candidate = candidate;
    job->type = standsFor;
    job->notches = std::move (combNotchesHz);

    juce::uint32 id;

    {
        const juce::ScopedLock sl (lock);
        id = job->id = nextId++;
        pending = std::move (job);   // an older job that has not started is dropped
    }

    wake.signal();
    return id;
}

bool SpectrumDelta::takeResult (Result& out)
{
    const juce::ScopedLock sl (lock);

    if (finished == nullptr)
        return false;

    out = std::move (*finished);
    finished.reset();
    return true;
}

SpectrumDelta::Result SpectrumDelta::compute (const WorkshopGuitar& committed, const WorkshopGuitar& candidate, GuitarType type)
{
    auto engine = makeEngine();
    return computeWith (*engine, committed, candidate, type);
}

std::vector<float> SpectrumDelta::combNotches (double positionMm, double scaleMm, double openHz, double maxHz)
{
    // A pickup at fraction p of the string from the saddle sits on a node of
    // every harmonic n with n * p a whole number: those harmonics vanish.
    std::vector<float> notches;
    const double p = positionMm / juce::jmax (1.0, scaleMm);

    if (p <= 0.0 || openHz <= 0.0)
        return notches;

    for (int k = 1; k < 64; ++k)
    {
        const double harmonic = (double) k / p;
        const double hz = harmonic * openHz;

        if (hz > maxHz)
            break;

        notches.push_back ((float) hz);
    }

    return notches;
}

void SpectrumDelta::run()
{
    std::unique_ptr<LuthierEngine> engine;
    CommittedCache cache;

    while (! threadShouldExit())
    {
        std::unique_ptr<Job> job;

        {
            const juce::ScopedLock sl (lock);
            job = std::move (pending);
        }

        if (job == nullptr)
        {
            wake.wait (500);
            continue;
        }

        if (engine == nullptr)
            engine = makeEngine();

        auto result = std::make_unique<Result> (computeWith (*engine, job->committed, job->candidate, job->type, &cache));
        result->combNotchesHz = std::move (job->notches);
        result->requestId = job->id;

        const juce::ScopedLock sl (lock);
        finished = std::move (result);
    }
}

} // namespace luthier
