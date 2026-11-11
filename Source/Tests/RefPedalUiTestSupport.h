// OWNER: WP17 / WP19
#pragma once
#include "TestFramework.h"
#include "../ReferencePedals/BoardModel.h"
namespace luthier::tests
{
// A rejected prerequisite is a test failure, never permission to dereference
// an empty order or continue operating a control that was not constructed.
inline bool requireReferenceEdit(TestContext& context,reference::BoardModel& model,const reference::BoardCommand& command)
{
    ++context.checks;juce::String reason;
    if(model.apply(command,&reason))return true;
    context.fail("Reference board setup rejected: "+reason);return false;
}
}
