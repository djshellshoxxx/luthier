#include "Snapshots.h"
#include "../Parameters.h"   // FEAT-JAM: ParamIDs::isJamTransient

namespace luthier
{

//==============================================================================
const char* getMorphCurveName (MorphCurve curve) noexcept
{
    switch (curve)
    {
        case MorphCurve::linear:      return "Linear";
        case MorphCurve::sCurve:      return "S-Curve";
        case MorphCurve::exponential: return "Exponential";
        case MorphCurve::bezier:      return "Bezier";
        case MorphCurve::numCurves:
        default:                      return "Linear";
    }
}

namespace
{
    /** One axis of a cubic Bezier with its first and last control points pinned
        to 0 and 1, which is the shape every easing curve of this kind uses. */
    double bezierAxis (double t, double p1, double p2) noexcept
    {
        const double u = 1.0 - t;

        return 3.0 * u * u * t * p1
             + 3.0 * u * t * t * p2
             + t * t * t;
    }

    /** Solves the Bezier for the t that gives this x, then reads y off it.

        A cubic Bezier easing curve is defined with x and y both as functions of
        a parameter, so getting y for a given x means inverting x(t) first. Ten
        steps of bisection is well inside the 0.001 tolerance the tests ask for
        and costs nothing at the rate a morph knob moves. */
    double bezierYForX (double x, double x1, double y1, double x2, double y2) noexcept
    {
        double low = 0.0, high = 1.0;

        for (int i = 0; i < 24; ++i)
        {
            const double mid = 0.5 * (low + high);

            if (bezierAxis (mid, x1, x2) < x)
                low = mid;
            else
                high = mid;
        }

        return bezierAxis (0.5 * (low + high), y1, y2);
    }
}

double applyMorphCurve (double position, MorphCurve curve,
                        double bezierX1, double bezierY1,
                        double bezierX2, double bezierY2) noexcept
{
    const double p = juce::jlimit (0.0, 1.0, position);

    switch (curve)
    {
        case MorphCurve::linear:
            return p;

        case MorphCurve::sCurve:
            // Smoothstep: flat at both ends, steepest in the middle.
            return p * p * (3.0 - 2.0 * p);

        case MorphCurve::exponential:
            return p * p;

        case MorphCurve::bezier:
            return juce::jlimit (0.0, 1.0,
                                 bezierYForX (p, bezierX1, bezierY1, bezierX2, bezierY2));

        case MorphCurve::numCurves:
        default:
            return p;
    }
}

//==============================================================================
juce::var Snapshot::toVar() const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("label", label);
    object->setProperty ("colour", colourTag);
    object->setProperty ("parameters", parameters);

    if (modMatrix.getDynamicObject() != nullptr) object->setProperty ("modMatrix", modMatrix);
    if (rhythm.getDynamicObject() != nullptr)    object->setProperty ("rhythm", rhythm);
    if (bypasses.getDynamicObject() != nullptr)  object->setProperty ("bypasses", bypasses);

    return { object };
}

Snapshot Snapshot::fromVar (const juce::var& state)
{
    Snapshot snapshot;

    auto* object = state.getDynamicObject();

    if (object == nullptr)
        return snapshot;

    snapshot.label = object->getProperty ("label").toString()
                       .substring (0, kMaxLabelLength);

    snapshot.colourTag = juce::jlimit (0, kNumColourTags - 1,
                                       (int) object->getProperty ("colour"));

    snapshot.parameters = object->getProperty ("parameters");
    snapshot.modMatrix  = object->getProperty ("modMatrix");
    snapshot.rhythm     = object->getProperty ("rhythm");
    snapshot.bypasses   = object->getProperty ("bypasses");

    return snapshot;
}

//==============================================================================
SnapshotBank::SnapshotBank (juce::AudioProcessor& p)
    : processor (p)
{
}

SnapshotBank::~SnapshotBank() = default;

//==============================================================================
bool SnapshotBank::isDiscrete (const juce::AudioProcessorParameter& parameter) noexcept
{
    // A choice or a boolean has no meaningful value between two settings. Asking
    // the parameter itself rather than testing its type catches the custom
    // parameter classes too.
    return parameter.isDiscrete() || parameter.isBoolean()
             || parameter.getNumSteps() <= 2;
}

//==============================================================================
const Snapshot& SnapshotBank::getSnapshot (int index) const noexcept
{
    static const Snapshot empty;

    return juce::isPositiveAndBelow (index, (int) snapshots.size())
             ? snapshots[(size_t) index] : empty;
}

void SnapshotBank::growTo (int index)
{
    if (index >= (int) snapshots.size())
        snapshots.resize ((size_t) index + 1);
}

bool SnapshotBank::setSnapshot (int index, const Snapshot& snapshot)
{
    if (! juce::isPositiveAndBelow (index, kMaxSnapshots))
        return false;

    growTo (index);
    snapshots[(size_t) index] = snapshot;

    sendChangeMessage();
    return true;
}

bool SnapshotBank::capture (int index, const juce::String& label, int colourTag)
{
    if (! juce::isPositiveAndBelow (index, kMaxSnapshots))
        return false;

    growTo (index);

    auto& snapshot = snapshots[(size_t) index];

    auto* parameters = new juce::DynamicObject();

    for (auto* p : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (! ParamIDs::isJamTransient (withId->paramID))   // FEAT-JAM: jam-mode 10
                parameters->setProperty (withId->paramID, (double) withId->getValue());

    snapshot.parameters = juce::var (parameters);

    if (label.isNotEmpty())
        snapshot.label = label.substring (0, Snapshot::kMaxLabelLength);
    else if (snapshot.label.isEmpty())
        snapshot.label = "Snapshot " + juce::String (index + 1);

    if (colourTag >= 0)
        snapshot.colourTag = juce::jlimit (0, Snapshot::kNumColourTags - 1, colourTag);

    sendChangeMessage();
    return true;
}

bool SnapshotBank::remove (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) snapshots.size()))
        return false;

    snapshots.erase (snapshots.begin() + index);

    sendChangeMessage();
    return true;
}

void SnapshotBank::clear()
{
    snapshots.clear();
    recallActive = false;

    sendChangeMessage();
}

void SnapshotBank::setLabel (int index, const juce::String& label)
{
    if (! juce::isPositiveAndBelow (index, (int) snapshots.size()))
        return;

    snapshots[(size_t) index].label = label.substring (0, Snapshot::kMaxLabelLength);
    sendChangeMessage();
}

void SnapshotBank::setColourTag (int index, int colourTag)
{
    if (! juce::isPositiveAndBelow (index, (int) snapshots.size()))
        return;

    snapshots[(size_t) index].colourTag =
        juce::jlimit (0, Snapshot::kNumColourTags - 1, colourTag);

    sendChangeMessage();
}

//==============================================================================
void SnapshotBank::setCrossfadeMs (double ms) noexcept
{
    crossfadeMs = juce::jlimit (0.0, 500.0, ms);
}

bool SnapshotBank::recall (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) snapshots.size()))
        return false;

    const auto& target = snapshots[(size_t) index];

    if (target.isEmpty())
        return false;

    // Where everything stands right now, so the fade has a fixed start.
    auto* from = new juce::DynamicObject();

    for (auto* p : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            from->setProperty (withId->paramID, (double) withId->getValue());

    recallFrom = juce::var (from);
    recallTarget = index;
    recallPosition = 0.0;
    recallSeconds = 0.0;
    recallMidpointDone = false;

    if (crossfadeMs <= 0.0)
    {
        // No crossfade asked for: apply it outright and be done inside this call.
        applyBlend (recallFrom, target.parameters, 1.0, false);
        applyNonParameterState (target);

        recallActive = false;
        recallPosition = 1.0;
        recallMidpointDone = true;
        currentIndex = index;

        sendChangeMessage();
        return true;
    }

    recallActive = true;
    currentIndex = index;

    sendChangeMessage();
    return true;
}

void SnapshotBank::advance (double secondsElapsed)
{
    if (! recallActive)
        return;

    recallSeconds += juce::jmax (0.0, secondsElapsed);

    const double total = juce::jmax (1.0e-6, crossfadeMs * 0.001);
    recallPosition = juce::jlimit (0.0, 1.0, recallSeconds / total);

    const auto& target = getSnapshot (recallTarget);

    applyBlend (recallFrom, target.parameters, recallPosition, false);

    // live-performance 1: bypass changes land on the crossfade midpoint, and the
    // rest of the non-parameter state goes with them.
    if (! recallMidpointDone && recallPosition >= 0.5)
    {
        applyNonParameterState (target);
        recallMidpointDone = true;
    }

    if (recallPosition >= 1.0)
    {
        recallActive = false;
        sendChangeMessage();
    }
}

//==============================================================================
void SnapshotBank::applyBlend (const juce::var& from, const juce::var& to, double blend,
                               bool honourExclusions)
{
    auto* fromObject = from.getDynamicObject();
    auto* toObject = to.getDynamicObject();

    if (toObject == nullptr)
        return;

    const double b = juce::jlimit (0.0, 1.0, blend);

    for (auto* p : processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);

        if (withId == nullptr)
            continue;

        const juce::Identifier id (withId->paramID);

        if (! toObject->hasProperty (id))
            continue;

        const double target = (double) toObject->getProperty (id);

        // A parameter the start state does not name has nothing to travel from,
        // so it simply takes the target.
        const double start = (fromObject != nullptr && fromObject->hasProperty (id))
                               ? (double) fromObject->getProperty (id)
                               : target;

        double value;

        if (honourExclusions && morphExclusions.contains (withId->paramID))
        {
            // live-performance 3: an excluded parameter takes snapshot A's value
            // for the whole travel, rather than switching at the midpoint.
            value = start;
        }
        else if (isDiscrete (*p))
        {
            value = (b >= 0.5) ? target : start;
        }
        else
        {
            value = start + (target - start) * b;
        }

        value = juce::jlimit (0.0, 1.0, value);

        if (std::abs (value - (double) withId->getValue()) > 1.0e-9)
            withId->setValueNotifyingHost ((float) value);
    }
}

void SnapshotBank::applyNonParameterState (const Snapshot& snapshot)
{
    if (onNonParameterState != nullptr)
        onNonParameterState (snapshot);
}

//==============================================================================
void SnapshotBank::setMorphSlots (int slotA, int slotB)
{
    morphA = juce::jlimit (0, kMaxSnapshots - 1, slotA);
    morphB = juce::jlimit (0, kMaxSnapshots - 1, slotB);

    sendChangeMessage();
}

void SnapshotBank::setMorphEnabled (bool shouldMorph)
{
    if (morphEnabled == shouldMorph)
        return;

    morphEnabled = shouldMorph;
    sendChangeMessage();
}

void SnapshotBank::setBezierControlPoints (double x1, double y1, double x2, double y2) noexcept
{
    bezier[0] = juce::jlimit (0.0, 1.0, x1);
    bezier[1] = juce::jlimit (0.0, 1.0, y1);
    bezier[2] = juce::jlimit (0.0, 1.0, x2);
    bezier[3] = juce::jlimit (0.0, 1.0, y2);
}

void SnapshotBank::setMorphPosition (double position)
{
    if (! morphEnabled)
        return;

    morphPosition = juce::jlimit (0.0, 1.0, position);

    const auto& a = getSnapshot (morphA);
    const auto& b = getSnapshot (morphB);

    if (a.isEmpty() || b.isEmpty())
        return;

    const double shaped = applyMorphCurve (morphPosition, morphCurve,
                                           bezier[0], bezier[1], bezier[2], bezier[3]);

    applyBlend (a.parameters, b.parameters, shaped, true);
}

void SnapshotBank::setParameterExcludedFromMorph (const juce::String& parameterId, bool excluded)
{
    if (excluded)
        morphExclusions.addIfNotAlreadyThere (parameterId);
    else
        morphExclusions.removeString (parameterId);
}

bool SnapshotBank::isParameterExcludedFromMorph (const juce::String& parameterId) const
{
    return morphExclusions.contains (parameterId);
}

//==============================================================================
juce::var SnapshotBank::toVar() const
{
    auto* root = new juce::DynamicObject();

    juce::Array<juce::var> array;

    for (const auto& snapshot : snapshots)
        array.add (snapshot.toVar());

    root->setProperty ("snapshots", array);
    root->setProperty ("crossfadeMs", crossfadeMs);
    root->setProperty ("current", currentIndex);
    root->setProperty ("morphEnabled", morphEnabled);
    root->setProperty ("morphA", morphA);
    root->setProperty ("morphB", morphB);
    root->setProperty ("morphPosition", morphPosition);
    root->setProperty ("morphCurve", (int) morphCurve);

    juce::Array<juce::var> bezierArray;

    for (double value : bezier)
        bezierArray.add (value);

    root->setProperty ("bezier", bezierArray);

    juce::Array<juce::var> exclusions;

    for (const auto& id : morphExclusions)
        exclusions.add (id);

    root->setProperty ("morphExclusions", exclusions);

    return { root };
}

void SnapshotBank::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    snapshots.clear();

    if (const auto* array = root->getProperty ("snapshots").getArray())
        for (const auto& item : *array)
            if ((int) snapshots.size() < kMaxSnapshots)
                snapshots.push_back (Snapshot::fromVar (item));

    if (root->hasProperty ("crossfadeMs"))
        setCrossfadeMs ((double) root->getProperty ("crossfadeMs"));

    currentIndex  = juce::jlimit (0, kMaxSnapshots - 1, (int) root->getProperty ("current"));
    morphEnabled  = (bool) root->getProperty ("morphEnabled");
    morphA        = juce::jlimit (0, kMaxSnapshots - 1, (int) root->getProperty ("morphA"));
    morphB        = juce::jlimit (0, kMaxSnapshots - 1, (int) root->getProperty ("morphB"));
    morphPosition = juce::jlimit (0.0, 1.0, (double) root->getProperty ("morphPosition"));

    morphCurve = (MorphCurve) juce::jlimit (0, (int) MorphCurve::numCurves - 1,
                                            (int) root->getProperty ("morphCurve"));

    if (const auto* bezierArray = root->getProperty ("bezier").getArray())
        for (int i = 0; i < juce::jmin (4, bezierArray->size()); ++i)
            bezier[i] = juce::jlimit (0.0, 1.0, (double) (*bezierArray)[i]);

    morphExclusions.clear();

    if (const auto* exclusions = root->getProperty ("morphExclusions").getArray())
        for (const auto& item : *exclusions)
            morphExclusions.addIfNotAlreadyThere (item.toString());

    recallActive = false;
    recallPosition = 1.0;

    sendChangeMessage();
}

} // namespace luthier
