#!/usr/bin/env python3
"""Preserve same-value CLAP host-write intent in pinned clap-juce-extensions.

Upstream commit 55525c9858d4b25687be7759a5e0f70eccef218e; licence retained.
Only paramSetValueAndNotifyIfChanged changes: equal writes notify listeners,
while value mutation remains changed-only and outgoing feedback stays suppressed.
Run during configuration before compiling the CLAP wrapper, never from audio.
"""
import argparse
import hashlib
import os
from pathlib import Path
import stat
import sys
import tempfile

RELATIVE = Path("src/wrapper/clap-juce-wrapper.cpp")
BEFORE_SHA256 = "80de1ed18a3ba3a753a1f07c80fb8ab6323a8b28098a3506e12249be9384cee5"
AFTER_SHA256 = "d930bb74aaa55a2fd94e65a0610b7e50d3962821a1e0bf4c4a67354122cd6d1f"
BEFORE = b'        if (param.processorParam->getValue() == newValue)\n            return;\n        JUCE_END_IGNORE_WARNINGS_GCC_LIKE\n\n        param.processorParam->setValue(newValue);\n'
AFTER = b'        // Even an unchanged host value carries explicit write intent. Notify\n        // listeners so deferred instrument defaults cannot replace that value.\n        const bool changed = param.processorParam->getValue() != newValue;\n        JUCE_END_IGNORE_WARNINGS_GCC_LIKE\n\n        if (changed)\n            param.processorParam->setValue(newValue);\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("extensions_root", type=Path)
    parser.add_argument("--check", action="store_true", help="Require the patch without writing")
    args = parser.parse_args()
    target = args.extensions_root / RELATIVE
    try:
        original = target.read_bytes()
        canonical = original.replace(b"\r\n", b"\n")
        digest = hashlib.sha256(canonical).hexdigest()
        if digest == AFTER_SHA256:
            print("CLAP host-write intent patch verified")
            return 0
        if digest != BEFORE_SHA256 or canonical.count(BEFORE) != 1:
            raise ValueError("CLAP wrapper differs from pinned 55525c98 preimage; refusing to patch")
        if args.check:
            raise ValueError("CLAP host-write intent patch is not applied")
        patched = canonical.replace(BEFORE, AFTER, 1)
        if hashlib.sha256(patched).hexdigest() != AFTER_SHA256:
            raise ValueError("CLAP host-write intent patch postimage hash mismatch")
        if b"\r\n" in original:
            patched = patched.replace(b"\n", b"\r\n")
        mode = stat.S_IMODE(target.stat().st_mode)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=target.parent, prefix=".luthier-clap-intent-", delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(patched)
                stream.flush()
                os.fsync(stream.fileno())
            temporary.chmod(mode)
            os.replace(temporary, target)
            temporary = None
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
        print("Applied CLAP host-write intent patch (55525c98)")
        return 0
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
