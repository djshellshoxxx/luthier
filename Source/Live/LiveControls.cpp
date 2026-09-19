#include "LiveControls.h"

namespace luthier
{

//==============================================================================
void KillSwitch::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (1.0, sampleRate);

    // One fade's worth of samples, expressed as the gain change per sample.
    step = 1.0 / juce::jmax (1.0, kFadeMs * 0.001 * sr);

    reset();
}

void KillSwitch::reset() noexcept
{
    // Reset leaves the switch wherever it is being held, but with no fade in
    // flight, so two renders of the same state produce the same audio.
    gain = active.load (std::memory_order_relaxed) ? 0.0 : 1.0;
}

void KillSwitch::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const bool wantMute = active.load (std::memory_order_relaxed);
    const double target = wantMute ? 0.0 : 1.0;

    // Nothing to do at all when the gain is already where it should be, which is
    // every block but the few during a fade.
    if (gain == target)
    {
        if (target == 1.0)
            return;

        buffer.clear();
        return;
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    double localGain = gain;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        localGain = (target > localGain) ? juce::jmin (target, localGain + step)
                                         : juce::jmax (target, localGain - step);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            data[sample] = (float) ((double) data[sample] * localGain);
        }
    }

    gain = localGain;
}

//==============================================================================
void MonitorMix::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = juce::jmax (1.0, sampleRate);

    // Twenty milliseconds: fast enough to follow a hand on a fader, slow enough
    // that no step in a level is audible as a click.
    levelSmooth.prepare (sr, 0.02);
    mainSmooth.prepare (sr, 0.02);
    sidechainSmooth.prepare (sr, 0.02);
    clickSmooth.prepare (sr, 0.02);

    dcL.prepare (sr);
    dcR.prepare (sr);

    lastLow = lastMid = lastHigh = std::numeric_limits<double>::quiet_NaN();
    updateFilters();

    reset();
}

void MonitorMix::reset() noexcept
{
    lowShelfL.reset();  lowShelfR.reset();
    midBellL.reset();   midBellR.reset();
    highShelfL.reset(); highShelfR.reset();

    dcL.reset();
    dcR.reset();

    levelSmooth.snapToTarget();
    mainSmooth.snapToTarget();
    sidechainSmooth.snapToTarget();
    clickSmooth.snapToTarget();

    outputLevel.store (0.0, std::memory_order_relaxed);
}

//==============================================================================
void MonitorMix::setLevelDb (double db) noexcept
{
    levelDb.store (juce::jlimit (-100.0, 12.0, db), std::memory_order_relaxed);
}

void MonitorMix::setPan (double p) noexcept
{
    pan.store (juce::jlimit (-1.0, 1.0, p), std::memory_order_relaxed);
}

void MonitorMix::setSidechainLevelDb (double db) noexcept
{
    sidechainDb.store (juce::jlimit (-100.0, 12.0, db), std::memory_order_relaxed);
}

void MonitorMix::setMainLevelDb (double db) noexcept
{
    mainDb.store (juce::jlimit (-100.0, 12.0, db), std::memory_order_relaxed);
}

void MonitorMix::setClickLevelDb (double db) noexcept
{
    clickDb.store (juce::jlimit (-100.0, 12.0, db), std::memory_order_relaxed);
}

void MonitorMix::setEqLowDb (double db) noexcept  { eqLowDb.store (juce::jlimit (-18.0, 18.0, db), std::memory_order_relaxed); }
void MonitorMix::setEqMidDb (double db) noexcept  { eqMidDb.store (juce::jlimit (-18.0, 18.0, db), std::memory_order_relaxed); }
void MonitorMix::setEqHighDb (double db) noexcept { eqHighDb.store (juce::jlimit (-18.0, 18.0, db), std::memory_order_relaxed); }

void MonitorMix::updateFilters() noexcept
{
    const double low = eqLowDb.load (std::memory_order_relaxed);
    const double mid = eqMidDb.load (std::memory_order_relaxed);
    const double high = eqHighDb.load (std::memory_order_relaxed);

    if (low == lastLow && mid == lastMid && high == lastHigh)
        return;

    lastLow = low;
    lastMid = mid;
    lastHigh = high;

    // A monitor EQ is for fixing a wedge or a pair of in-ears, not for shaping
    // tone, so the bands are broad and sit where a room actually misbehaves.
    lowShelfL.setLowShelf (sr, 120.0, 0.7, low);
    lowShelfR.setLowShelf (sr, 120.0, 0.7, low);

    midBellL.setPeaking (sr, 900.0, 0.8, mid);
    midBellR.setPeaking (sr, 900.0, 0.8, mid);

    highShelfL.setHighShelf (sr, 4000.0, 0.7, high);
    highShelfR.setHighShelf (sr, 4000.0, 0.7, high);
}

//==============================================================================
bool MonitorMix::isActive (bool haveSidechain, bool haveClick) const noexcept
{
    // live-performance 7: with nothing to monitor and the level down, the whole
    // path is skipped and costs nothing.
    if (levelDb.load (std::memory_order_relaxed) <= kSilenceDb)
        return false;

    return haveSidechain || haveClick
             || mainDb.load (std::memory_order_relaxed) > kSilenceDb;
}

void MonitorMix::processBlock (juce::AudioBuffer<float>& destination,
                               const juce::AudioBuffer<float>& main,
                               const juce::AudioBuffer<float>* sidechain,
                               const float* click,
                               int numSamples) noexcept
{
    if (destination.getNumChannels() < 2 || numSamples <= 0)
        return;

    destination.clear();

    if (! isActive (sidechain != nullptr, click != nullptr))
    {
        outputLevel.store (0.0, std::memory_order_relaxed);
        return;
    }

    updateFilters();

    levelSmooth.setTarget (dbToGain (levelDb.load (std::memory_order_relaxed)));
    mainSmooth.setTarget (dbToGain (mainDb.load (std::memory_order_relaxed)));
    sidechainSmooth.setTarget (dbToGain (sidechainDb.load (std::memory_order_relaxed)));
    clickSmooth.setTarget (dbToGain (clickDb.load (std::memory_order_relaxed)));

    // Constant-power pan, so sweeping the monitor across does not dip in the
    // middle the way a linear law does.
    const double panPosition = (pan.load (std::memory_order_relaxed) + 1.0) * 0.25 * juce::MathConstants<double>::pi;
    const double panL = std::cos (panPosition);
    const double panR = std::sin (panPosition);

    const int mainChannels = main.getNumChannels();
    const int sidechainChannels = (sidechain != nullptr) ? sidechain->getNumChannels() : 0;

    auto* outL = destination.getWritePointer (0);
    auto* outR = destination.getWritePointer (1);

    double peak = 0.0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const double level = levelSmooth.next();
        const double mainGain = mainSmooth.next();
        const double sidechainGain = sidechainSmooth.next();
        const double clickGain = clickSmooth.next();

        double left = 0.0, right = 0.0;

        if (mainChannels > 0 && sample < main.getNumSamples())
        {
            const double l = (double) main.getReadPointer (0)[sample];
            const double r = (mainChannels > 1) ? (double) main.getReadPointer (1)[sample] : l;

            left  += l * mainGain;
            right += r * mainGain;
        }

        if (sidechainChannels > 0 && sample < sidechain->getNumSamples())
        {
            const double l = (double) sidechain->getReadPointer (0)[sample];
            const double r = (sidechainChannels > 1) ? (double) sidechain->getReadPointer (1)[sample] : l;

            left  += l * sidechainGain;
            right += r * sidechainGain;
        }

        if (click != nullptr)
        {
            const double c = (double) click[sample] * clickGain;
            left += c;
            right += c;
        }

        left  = lowShelfL.process (left);
        left  = midBellL.process (left);
        left  = highShelfL.process (left);

        right = lowShelfR.process (right);
        right = midBellR.process (right);
        right = highShelfR.process (right);

        left  = dcL.process (left) * level * panL * juce::MathConstants<double>::sqrt2;
        right = dcR.process (right) * level * panR * juce::MathConstants<double>::sqrt2;

        left = sanitise (left);
        right = sanitise (right);

        outL[sample] = (float) left;
        outR[sample] = (float) right;

        peak = juce::jmax (peak, std::abs (left), std::abs (right));
    }

    outputLevel.store (peak, std::memory_order_relaxed);
}

//==============================================================================
const char* getExpressionCurveName (ExpressionCalibration::Curve curve) noexcept
{
    switch (curve)
    {
        case ExpressionCalibration::Curve::linear:      return "Linear";
        case ExpressionCalibration::Curve::logarithmic: return "Logarithmic";
        case ExpressionCalibration::Curve::exponential: return "Exponential";
        case ExpressionCalibration::Curve::sCurve:      return "S-Curve";
        case ExpressionCalibration::Curve::numCurves:
        default:                                        return "Linear";
    }
}

double ExpressionCalibration::map (int rawValue) const noexcept
{
    if (! isCalibrated())
        return juce::jlimit (0.0, 1.0, (double) rawValue / 127.0);

    const double span = (double) (rawMaximum - rawMinimum);

    double position = ((double) rawValue - (double) rawMinimum) / span;
    position = juce::jlimit (0.0, 1.0, position);

    // live-performance 8: the dead zones are the ends of the travel where the
    // pedal has stopped moving. Clamp inside them, then rescale what is left so
    // the usable travel still covers the whole 0 to 1.
    const double heel = juce::jlimit (0.0, 0.45, heelDeadZone);
    const double toe = juce::jlimit (0.0, 0.45, toeDeadZone);

    const double usable = 1.0 - heel - toe;

    if (usable <= 1.0e-6)
        return position;

    position = juce::jlimit (0.0, 1.0, (position - heel) / usable);

    switch (curve)
    {
        case Curve::logarithmic:
            // Fine control at the heel, where a volume pedal needs it.
            return std::log10 (1.0 + 9.0 * position);

        case Curve::exponential:
            return position * position;

        case Curve::sCurve:
            return position * position * (3.0 - 2.0 * position);

        case Curve::linear:
        case Curve::numCurves:
        default:
            return position;
    }
}

juce::var ExpressionCalibration::toVar() const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("cc", ccNumber);
    object->setProperty ("min", rawMinimum);
    object->setProperty ("max", rawMaximum);
    object->setProperty ("heel_dead_zone", heelDeadZone);
    object->setProperty ("toe_dead_zone", toeDeadZone);
    object->setProperty ("curve", getExpressionCurveName (curve));

    return { object };
}

ExpressionCalibration ExpressionCalibration::fromVar (const juce::var& state)
{
    ExpressionCalibration calibration;

    auto* object = state.getDynamicObject();

    if (object == nullptr)
        return calibration;

    calibration.ccNumber   = juce::jlimit (-1, 127, (int) object->getProperty ("cc"));
    calibration.rawMinimum = juce::jlimit (0, 127, (int) object->getProperty ("min"));
    calibration.rawMaximum = juce::jlimit (0, 127, (int) object->getProperty ("max"));

    if (object->hasProperty ("heel_dead_zone"))
        calibration.heelDeadZone = juce::jlimit (0.0, 0.45, (double) object->getProperty ("heel_dead_zone"));

    if (object->hasProperty ("toe_dead_zone"))
        calibration.toeDeadZone = juce::jlimit (0.0, 0.45, (double) object->getProperty ("toe_dead_zone"));

    const auto curveName = object->getProperty ("curve").toString();

    for (int i = 0; i < (int) Curve::numCurves; ++i)
        if (curveName.equalsIgnoreCase (getExpressionCurveName ((Curve) i)))
            calibration.curve = (Curve) i;

    return calibration;
}

//==============================================================================
ExpressionCalibrationSet::ExpressionCalibrationSet()
{
    present.fill (false);

    for (int cc = 0; cc < 128; ++cc)
        calibrations[(size_t) cc].ccNumber = cc;
}

ExpressionCalibration ExpressionCalibrationSet::get (int ccNumber) const
{
    if (! juce::isPositiveAndBelow (ccNumber, 128))
        return {};

    return calibrations[(size_t) ccNumber];
}

bool ExpressionCalibrationSet::has (int ccNumber) const
{
    return juce::isPositiveAndBelow (ccNumber, 128) && present[(size_t) ccNumber];
}

void ExpressionCalibrationSet::set (const ExpressionCalibration& calibration)
{
    if (! juce::isPositiveAndBelow (calibration.ccNumber, 128))
        return;

    calibrations[(size_t) calibration.ccNumber] = calibration;
    present[(size_t) calibration.ccNumber] = true;
}

void ExpressionCalibrationSet::remove (int ccNumber)
{
    if (! juce::isPositiveAndBelow (ccNumber, 128))
        return;

    present[(size_t) ccNumber] = false;
    calibrations[(size_t) ccNumber] = ExpressionCalibration {};
    calibrations[(size_t) ccNumber].ccNumber = ccNumber;
}

void ExpressionCalibrationSet::clear()
{
    present.fill (false);

    for (int cc = 0; cc < 128; ++cc)
    {
        calibrations[(size_t) cc] = ExpressionCalibration {};
        calibrations[(size_t) cc].ccNumber = cc;
    }
}

double ExpressionCalibrationSet::map (int ccNumber, int rawValue) const
{
    if (! has (ccNumber))
        return juce::jlimit (0.0, 1.0, (double) rawValue / 127.0);

    return calibrations[(size_t) ccNumber].map (rawValue);
}

juce::Array<int> ExpressionCalibrationSet::getCalibratedCcNumbers() const
{
    juce::Array<int> result;

    for (int cc = 0; cc < 128; ++cc)
        if (present[(size_t) cc])
            result.add (cc);

    return result;
}

//==============================================================================
void ExpressionCalibrationSet::beginCalibration (int ccNumber) noexcept
{
    wizardCc = juce::isPositiveAndBelow (ccNumber, 128) ? ccNumber : -1;
    stage = (wizardCc >= 0) ? WizardStage::heel : WizardStage::idle;

    observedMin = 127;
    observedMax = 0;
    heelValue = 0;
}

void ExpressionCalibrationSet::cancelCalibration() noexcept
{
    stage = WizardStage::idle;
    wizardCc = -1;
}

ExpressionCalibrationSet::WizardStage
ExpressionCalibrationSet::observe (int ccNumber, int rawValue) noexcept
{
    if (stage == WizardStage::idle || stage == WizardStage::done || ccNumber != wizardCc)
        return stage;

    const int value = juce::jlimit (0, 127, rawValue);

    observedMin = juce::jmin (observedMin, value);
    observedMax = juce::jmax (observedMax, value);

    return stage;
}

ExpressionCalibrationSet::WizardStage ExpressionCalibrationSet::confirmStage() noexcept
{
    switch (stage)
    {
        case WizardStage::heel:
            // Whatever the pedal was sending while rocked fully back is the heel
            // end. Take the lowest of it: a pedal that rattles slightly should
            // calibrate to its true extreme, not to where it happened to settle.
            heelValue = (observedMin <= observedMax) ? observedMin : 0;

            observedMin = 127;
            observedMax = 0;
            stage = WizardStage::toe;
            break;

        case WizardStage::toe:
        {
            const int toeValue = (observedMax >= observedMin) ? observedMax : 127;

            ExpressionCalibration calibration;
            calibration.ccNumber = wizardCc;

            // A pedal wired backwards sends its highest value at the heel. Order
            // the two ends rather than rejecting it.
            calibration.rawMinimum = juce::jmin (heelValue, toeValue);
            calibration.rawMaximum = juce::jmax (heelValue, toeValue);

            // A pedal that produced no usable travel is not committed: keeping
            // the old calibration is better than installing a broken one.
            if (calibration.rawMaximum > calibration.rawMinimum)
                set (calibration);

            stage = WizardStage::done;
            break;
        }

        case WizardStage::idle:
        case WizardStage::done:
        default:
            break;
    }

    return stage;
}

//==============================================================================
juce::File ExpressionCalibrationSet::getConfigFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("config")
             .getChildFile ("expression.json");
}

juce::var ExpressionCalibrationSet::toVar() const
{
    auto* root = new juce::DynamicObject();

    juce::Array<juce::var> array;

    for (int cc = 0; cc < 128; ++cc)
        if (present[(size_t) cc])
            array.add (calibrations[(size_t) cc].toVar());

    root->setProperty ("pedals", array);

    return { root };
}

void ExpressionCalibrationSet::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    clear();

    if (const auto* array = root->getProperty ("pedals").getArray())
        for (const auto& item : *array)
            set (ExpressionCalibration::fromVar (item));
}

bool ExpressionCalibrationSet::load()
{
    const auto file = getConfigFile();

    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.getDynamicObject() == nullptr)
        return false;

    fromVar (parsed);
    return true;
}

bool ExpressionCalibrationSet::save() const
{
    const auto file = getConfigFile();

    file.getParentDirectory().createDirectory();

    return file.replaceWithText (juce::JSON::toString (toVar(), false));
}

} // namespace luthier
