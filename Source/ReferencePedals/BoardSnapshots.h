// OWNER: WP4
#pragma once
#include "BoardState.h"
namespace luthier::reference
{
// Eight persistent scenes. Indices are zero based (MIDI PC 0..7).
// Each capture stores the same schema as a board preset plus its name.
class BoardSnapshots
{
public:
    static constexpr int count = 8;
    static bool capture (BoardModel&, int index, const juce::String& name = {}, juce::String* error = nullptr);
    static bool recall (BoardModel&, int index, juce::String* error = nullptr);
    static juce::var get (const BoardModel&, int index);
};
}
