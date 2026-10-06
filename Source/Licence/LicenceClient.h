#pragma once

#include "LicenceFile.h"
#include "LicenceState.h"
#include "LicenceStore.h"
#include "MachineFingerprint.h"
#include "../Updates/Telemetry.h"

namespace luthier
{

class LicenceClient
{
public:
    enum class ResultCode
    {
        ok,
        invalidInput,
        networkError,
        rejected,
        invalidSignature,
        machineMismatch,
        storageError
    };

    struct Result
    {
        ResultCode code = ResultCode::networkError;
        juce::String message;
        std::optional<VerifiedLicence> licence;
        bool succeeded() const noexcept { return code == ResultCode::ok; }
    };

    LicenceClient (Transport& transport,
                   LicenceStore store,
                   std::vector<LicencePublicKey> trustedKeys);

    Result activate (const juce::String& key,
                     const juce::String& endpoint,
                     bool requestTrial = false);

    Result revalidate (const juce::String& endpoint);

    /** Install an already-signed envelope, used by offline activation. */
    Result installSignedEnvelope (const juce::String& envelope)
    {
        return acceptServerEnvelope (envelope);
    }

    void deactivateLocal() noexcept;

    std::optional<VerifiedLicence> currentVerified (juce::String* error = nullptr) const;
    LicenceStatus currentStatus (juce::Time now = juce::Time::getCurrentTime()) const;

private:
    Result acceptServerEnvelope (const juce::String&);
    static juce::String jsonRequest (const juce::String& key,
                                     const juce::StringArray& fingerprints,
                                     bool trial);

    Transport& transport;
    LicenceStore store;
    std::vector<LicencePublicKey> keys;
};

} // namespace luthier
