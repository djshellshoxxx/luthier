#pragma once
#include "../DSP/Effects/EffectsChain.h"
#include <functional>
#include <iostream>
#include <string>
namespace luthier::tests {
struct TestContext {int checks=0,failures=0;};
struct Entry {std::string name;std::function<void(TestContext&)> run;};
inline std::vector<Entry>& registry(){static std::vector<Entry> entries;return entries;}
struct Register {Register(const char* n,std::function<void(TestContext&)> f){registry().push_back({n,f});}};
}
#define LUTHIER_TEST(s,n) static void s##n(luthier::tests::TestContext& ctx); static luthier::tests::Register reg##s##n(#s "." #n,s##n); static void s##n(luthier::tests::TestContext& ctx)
#define CHECK(x) do {++ctx.checks;if(!(x)){++ctx.failures;std::cerr<<__LINE__<<": " #x " failed\n";}}while(false)
