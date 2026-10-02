#include "MicPlacementMigration.h"
#include "ExactRestore.h"
#include "../Parameters.h"

namespace luthier
{

namespace MicPlacementMigration
{
    namespace
    {
        const MicIds kIds[2] =
        {
            { ParamIDs::micPosition,  ParamIDs::micDistance,  ParamIDs::micX,  ParamIDs::micY,
              ParamIDs::micDist,  ParamIDs::micAngle,  ParamIDs::micSpeaker,  ParamIDs::micRear },
            { ParamIDs::micPosition2, ParamIDs::micDistance2, ParamIDs::micX2, ParamIDs::micY2,
              ParamIDs::micDist2, ParamIDs::micAngle2, ParamIDs::micSpeaker2, ParamIDs::micRear2 }
        };

        /** The processor's parameters by id: works for a snapshot bank, which
            holds the processor rather than its value tree. */
        struct Lookup
        {
            const juce::AudioProcessor& processor;

            juce::RangedAudioParameter* getParameter (const char* id) const
            {
                for (auto* p : processor.getParameters())
                    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                        if (r->paramID == id)
                            return r;

                return nullptr;
            }

            std::atomic<float>* getRawParameterValue (const char*) const { return nullptr; }
        };

        template <typename State>
        juce::RangedAudioParameter* ranged (const State& state, const char* id)
        {
            return dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id));
        }

        template <typename State>
        double toNormalised (const State& state, const char* id, double plain)
        {
            if (auto* p = ranged (state, id))
                return (double) p->convertTo0to1 ((float) plain);

            return plain;
        }

        template <typename State>
        double toPlain (const State& state, const char* id, double normalised)
        {
            if (auto* p = ranged (state, id))
                return (double) p->convertFrom0to1 ((float) juce::jlimit (0.0, 1.0, normalised));

            return normalised;
        }

        template <typename State>
        double defaultPlain (const State& state, const char* id)
        {
            if (auto* p = ranged (state, id))
                return (double) p->convertFrom0to1 (p->getDefaultValue());

            return 0.0;
        }

        double currentPlain (const juce::AudioProcessorValueTreeState& state, const char* id)
        {
            if (auto* raw = state.getRawParameterValue (id))
                return (double) raw->load();

            return defaultPlain (state, id);
        }

        /** The mapped plain values for one mic, id -> value. */
        std::vector<std::pair<const char*, double>> mappedValues (int mic, int position, int distance)
        {
            const auto m = mapLegacy (position, distance);
            const auto& ids = kIds[juce::jlimit (0, 1, mic)];

            return { { ids.x, m.x }, { ids.y, m.y }, { ids.dist, m.distCm }, { ids.angle, m.angleDeg },
                     { ids.speaker, 1.0 }, { ids.rear, m.rear ? 1.0 : 0.0 } };
        }
    }

    //==========================================================================
    Mapped mapLegacy (int position, int distance) noexcept
    {
        Mapped m;

        switch (position)
        {
            case 0:  m.x = 0.0;  break;                          // On-Axis Centre
            case 1:  m.x = 0.35; break;                          // Cap Edge
            case 2:  m.x = 0.35; m.angleDeg = 45.0; break;       // Off-Axis 45
            case 3:  m.x = 0.90; break;                          // Cone Edge
            case 4:  m.x = 0.35; m.rear = true; break;           // Rear
            default: break;
        }

        m.distCm = distance == 1 ? 15.0 : distance >= 2 ? 30.0 : 2.5;
        return m;
    }

    int mirrorPosition (double x, double y, double angleDeg, bool rear) noexcept
    {
        const double u = std::hypot (x, y);

        if (rear)             return 4;
        if (u >= 0.62)        return 3;
        if (angleDeg >= 22.5) return 2;
        if (u < 0.175)        return 0;
        return 1;
    }

    int mirrorDistance (double distCm) noexcept
    {
        // Geometric midpoints of 2.5, 15 and 30 cm.
        if (distCm < 6.1)  return 0;
        if (distCm < 21.2) return 1;
        return 2;
    }

    const MicIds& idsFor (int mic) noexcept
    {
        return kIds[juce::jlimit (0, 1, mic)];
    }

    //==========================================================================
    template <typename State>
    static int applyTo (juce::var& parameters, const State& state)
    {
        auto* obj = parameters.getDynamicObject();

        if (obj == nullptr)
            return 0;

        int migrated = 0;

        for (int mic = 0; mic < 2; ++mic)
        {
            const auto& ids = kIds[mic];

            if (! obj->hasProperty (ids.position) || obj->hasProperty (ids.x))
                continue;

            const int position = juce::roundToInt (toPlain (state, ids.position, (double) obj->getProperty (ids.position)));
            const int distance = obj->hasProperty (ids.distance)
                                   ? juce::roundToInt (toPlain (state, ids.distance, (double) obj->getProperty (ids.distance)))
                                   : juce::roundToInt (defaultPlain (state, ids.distance));

            for (const auto& [id, plain] : mappedValues (mic, position, distance))
                obj->setProperty (id, toNormalised (state, id, plain));

            migrated |= (1 << mic);
        }

        if (migrated != 0)
        {
            if (! obj->hasProperty (ParamIDs::micTofMode))
                obj->setProperty (ParamIDs::micTofMode, toNormalised (state, ParamIDs::micTofMode, 0.0));

            if (! obj->hasProperty (ParamIDs::micLevelMatch))
                obj->setProperty (ParamIDs::micLevelMatch, toNormalised (state, ParamIDs::micLevelMatch, 1.0));
        }

        return migrated;
    }

    int apply (juce::var& parameters, const juce::AudioProcessorValueTreeState& state)
    {
        return applyTo (parameters, state);
    }

    int apply (juce::var& parameters, const juce::AudioProcessor& processor)
    {
        return applyTo (parameters, Lookup { processor });
    }

    int apply (juce::ValueTree& tree, const juce::AudioProcessorValueTreeState& state)
    {
        const auto find = [&tree] (const char* id) -> juce::ValueTree
        {
            for (auto child : tree)
                if (child.getProperty ("id").toString() == id)
                    return child;

            return {};
        };

        const auto set = [&tree, &find] (const char* id, double plain)
        {
            auto child = find (id);

            if (! child.isValid())
            {
                child = juce::ValueTree ("PARAM");
                child.setProperty ("id", juce::String (id), nullptr);
                tree.appendChild (child, nullptr);
            }

            child.setProperty ("value", plain, nullptr);
        };

        int migrated = 0;

        for (int mic = 0; mic < 2; ++mic)
        {
            const auto& ids = kIds[mic];
            const auto legacy = find (ids.position);

            if (! legacy.isValid() || find (ids.x).isValid())
                continue;

            const auto distanceNode = find (ids.distance);
            const int position = juce::roundToInt ((double) legacy.getProperty ("value"));
            const int distance = distanceNode.isValid() ? juce::roundToInt ((double) distanceNode.getProperty ("value"))
                                                        : juce::roundToInt (defaultPlain (state, ids.distance));

            for (const auto& [id, plain] : mappedValues (mic, position, distance))
                set (id, plain);

            migrated |= (1 << mic);
        }

        if (migrated != 0)
        {
            if (! find (ParamIDs::micTofMode).isValid())    set (ParamIDs::micTofMode, 0.0);
            if (! find (ParamIDs::micLevelMatch).isValid()) set (ParamIDs::micLevelMatch, 1.0);
        }

        return migrated;
    }

    //==========================================================================
    void mirror (juce::var& parameters, const juce::AudioProcessorValueTreeState& state)
    {
        auto* obj = parameters.getDynamicObject();

        if (obj == nullptr)
            return;

        // The Acoustic DI's legacy choices are its voicing (FEAT-MIC decision).
        if (juce::roundToInt (currentPlain (state, ParamIDs::cabType)) == 9)
            return;

        for (int mic = 0; mic < 2; ++mic)
        {
            const auto& ids = kIds[mic];
            const int position = mirrorPosition (currentPlain (state, ids.x), currentPlain (state, ids.y),
                                                 currentPlain (state, ids.angle), currentPlain (state, ids.rear) > 0.5);
            const int distance = mirrorDistance (currentPlain (state, ids.dist));

            obj->setProperty (ids.position, toNormalised (state, ids.position, (double) position));
            obj->setProperty (ids.distance, toNormalised (state, ids.distance, (double) distance));
        }
    }

    juce::var captureLiveLegacy (const juce::AudioProcessorValueTreeState& state)
    {
        auto* obj = new juce::DynamicObject();

        for (int mic = 0; mic < 2; ++mic)
            for (const char* id : { kIds[mic].position, kIds[mic].distance })
                if (auto* p = state.getParameter (id))
                    obj->setProperty (id, (double) p->getValue());

        return juce::var (obj);
    }

    void restoreLiveLegacy (const juce::var& block, juce::AudioProcessorValueTreeState& state)
    {
        auto* obj = block.getDynamicObject();

        if (obj == nullptr)
            return;

        for (int mic = 0; mic < 2; ++mic)
            for (const char* id : { kIds[mic].position, kIds[mic].distance })
                if (obj->hasProperty (id))
                    if (auto* p = state.getParameter (id))
                        ExactRestore::applyNormalised (*p, (double) obj->getProperty (id));
    }

    void keepPlacementPlausible (juce::AudioProcessorValueTreeState& state, const juce::StringArray& locked)
    {
        const auto write = [&state, &locked] (const char* id, double plain)
        {
            if (locked.contains (id))
                return;

            if (auto* p = ranged (state, id))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) plain));
        };

        for (int mic = 0; mic < 2; ++mic)
        {
            const auto& ids = kIds[mic];
            const double x = currentPlain (state, ids.x), y = currentPlain (state, ids.y);
            const double u = std::hypot (x, y);

            if (u > 1.0)
            {
                write (ids.x, x / u);
                write (ids.y, y / u);
            }

            if (currentPlain (state, ids.dist) > 30.0)
                write (ids.dist, 30.0);
        }

        for (const char* id : { ParamIDs::acMicDist, ParamIDs::acMicDist2 })
            if (currentPlain (state, id) > 30.0)
                write (id, 30.0);
    }

    void writeMapped (juce::AudioProcessorValueTreeState& state, int mic, int position, int distance)
    {
        for (const auto& [id, plain] : mappedValues (mic, position, distance))
            if (auto* p = ranged (state, id))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) plain));
    }
}

//==============================================================================
MicLegacyAutomation::MicLegacyAutomation (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    for (int mic = 0; mic < 2; ++mic)
    {
        const auto& ids = MicPlacementMigration::idsFor (mic);

        for (const char* id : { ids.position, ids.distance })
            if (auto* p = apvts.getParameter (id))
                watched.push_back ({ p, mic, true });

        for (const char* id : { ids.x, ids.y, ids.dist, ids.angle, ids.speaker, ids.rear })
            if (auto* p = apvts.getParameter (id))
                watched.push_back ({ p, mic, false });

        lastLegacyWrite[(size_t) mic] = -1.0e9;
        lastContinuousWrite[(size_t) mic] = -1.0e9;
        pending[(size_t) mic] = false;
    }

    for (auto& w : watched)
        w.param->addListener (this);

    startTimer (50);
}

MicLegacyAutomation::~MicLegacyAutomation()
{
    stopTimer();

    for (auto& w : watched)
        w.param->removeListener (this);
}

void MicLegacyAutomation::parameterValueChanged (int parameterIndex, float)
{
    // Any thread: a host writes automation from the audio thread. Lock-free.
    for (const auto& w : watched)
    {
        if (w.param->getParameterIndex() != parameterIndex)
            continue;

        const double t = now();

        if (w.legacy)
        {
            lastLegacyWrite[(size_t) w.mic] = t;
            pending[(size_t) w.mic] = true;
        }
        else if (! writingMapped.load())
        {
            lastContinuousWrite[(size_t) w.mic] = t;
        }

        return;
    }
}

void MicLegacyAutomation::processPending (double nowMs)
{
    for (int mic = 0; mic < 2; ++mic)
    {
        if (! pending[(size_t) mic].load())
            continue;

        const double legacyAt = lastLegacyWrite[(size_t) mic].load();

        // Wait out the window: a restore writes the continuous values after.
        if (nowMs - legacyAt < kWindowMs)
            continue;

        pending[(size_t) mic] = false;

        if (std::abs (lastContinuousWrite[(size_t) mic].load() - legacyAt) <= kWindowMs)
            continue;

        const auto& ids = MicPlacementMigration::idsFor (mic);
        const auto* pos = apvts.getRawParameterValue (ids.position);
        const auto* dist = apvts.getRawParameterValue (ids.distance);

        if (pos == nullptr || dist == nullptr)
            continue;

        // A mapping is not a user action: no gesture, no undo entry.
        writingMapped = true;
        MicPlacementMigration::writeMapped (apvts, mic, juce::roundToInt (pos->load()), juce::roundToInt (dist->load()));
        writingMapped = false;
        ++mappings;
    }
}

} // namespace luthier
