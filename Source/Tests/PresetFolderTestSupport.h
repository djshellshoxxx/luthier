// Native-test fixture only; production preset interfaces stay unchanged.
#pragma once
#include <JuceHeader.h>

namespace luthier
{
#if defined(LUTHIER_ALLOCATION_COUNTER)
/** Native-test fixture only. Construct on the message thread before processors,
    destroy after their editors/workers/processors, then remove the temporary
    folder. Nested scopes restore the previous root; production builds retain
    the normal Documents path and contain no override branch. */
class ScopedUserPresetFolderForTesting
{
public:
    explicit ScopedUserPresetFolderForTesting(const juce::File&);
    ~ScopedUserPresetFolderForTesting();
private:
    juce::File previous;
    JUCE_DECLARE_NON_COPYABLE(ScopedUserPresetFolderForTesting)
};
#endif
}
