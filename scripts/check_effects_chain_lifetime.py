#!/usr/bin/env python3
"""Compile real EffectsChain + native lifetime tests against a tiny boundary shim.

Useful without JUCE/system GUI headers. Runs ASan/UBSan; does not validate DSP,
JUCE threading semantics, or hosts. Native CI must also run the identical tests.
--unsafe-reclaim deliberately breaks the reclamation predicate to check that the
regressions detect premature destruction (expected nonzero / ASan failure).
"""
import argparse
import pathlib
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--unsafe-reclaim', action='store_true')
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    fixtures = root / 'scripts/lifetime_harness'
    with tempfile.TemporaryDirectory(prefix='luthier-lifetime-') as temp:
        work = pathlib.Path(temp)
        effects = work / 'Source/DSP/Effects'
        tests = work / 'Source/Tests'
        effects.mkdir(parents=True)
        tests.mkdir(parents=True)
        for name in ('EffectsChain.h', 'EffectsChain.cpp'):
            shutil.copy2(root / 'Source/DSP/Effects' / name, effects / name)
        shutil.copy2(fixtures / 'Pedal.h', effects / 'Pedal.h')
        (effects / 'PedalsDrive.h').write_text('// Unused by EffectsChain.\n')
        shutil.copy2(fixtures / 'TestFramework.h', tests / 'TestFramework.h')
        shutil.copy2(root / 'Source/Tests/EffectsChainLifetimeTests.cpp', tests)
        if args.unsafe_reclaim:
            source = effects / 'EffectsChain.cpp'
            text = source.read_text()
            predicate = 'if (unlockedReaders.load() == 0)'
            assert text.count(predicate) == 1
            source.write_text(text.replace(predicate, 'if (true)'))
        (work / 'main.cpp').write_text('''#include "Source/Tests/EffectsChainLifetimeTests.cpp"
int main() { luthier::tests::TestContext ctx;
 for (const auto& t:luthier::tests::registry()) { t.run(ctx);std::cout<<t.name<<"\\n"; }
 std::cout<<ctx.checks<<" checks, "<<ctx.failures<<" failures\\n";
 return ctx.failures ? 1 : 0;
}
''')
        executable = work / 'lifetime-tests'
        subprocess.run(['g++', '-std=c++17', '-pthread', '-g', '-O1',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        str(effects / 'EffectsChain.cpp'), str(work / 'main.cpp'),
                        '-o', str(executable)], check=True)
        return subprocess.run([str(executable)]).returncode


if __name__ == '__main__':
    raise SystemExit(main())
