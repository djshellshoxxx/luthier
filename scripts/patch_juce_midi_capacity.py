#!/usr/bin/env python3
"""Apply/check Luthier's narrow, hash-pinned JUCE 8.0.10 MIDI reserve patch.

Only AudioProcessorPlayer's constructor changes. The upstream licence is retained.
Call during configuration, before compiling juce_audio_utils, never from audio.
"""
import argparse
import hashlib
import os
from pathlib import Path
import stat
import sys
import tempfile

RELATIVE = Path("modules/juce_audio_utils/players/juce_AudioProcessorPlayer.cpp")
BEFORE_SHA256 = "6116f592c5230bf0daab76771f98a51695bc3c7ed038b776a6682b4a4dbb7f1b"
AFTER_SHA256 = "c307e3a5edf5a6f2862554673b186370edc43ee942b8c48a159f43ffff72541c"
BEFORE = b"AudioProcessorPlayer::AudioProcessorPlayer (bool doDoublePrecisionProcessing)\n    : isDoublePrecision (doDoublePrecisionProcessing)\n{\n}"
AFTER = BEFORE[:-1] + b"    // Luthier: reserve MIDI output storage off the audio thread (JUCE 8.0.10 patch).\n    incomingMidi.ensureSize (65536);\n}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("juce_root", type=Path)
    parser.add_argument("--check", action="store_true", help="Require the patch without writing")
    args = parser.parse_args()
    target = args.juce_root / RELATIVE
    try:
        original = target.read_bytes()
        canonical = original.replace(b"\r\n", b"\n")
        digest = hashlib.sha256(canonical).hexdigest()
        if digest == AFTER_SHA256:
            print("JUCE 8.0.10 MIDI reserve patch verified (65536 bytes)")
            return 0
        if digest != BEFORE_SHA256 or canonical.count(BEFORE) != 1:
            raise ValueError("JUCE MIDI source differs from the pinned 8.0.10 preimage; refusing to patch")
        if args.check:
            raise ValueError("JUCE MIDI reserve patch is not applied")
        patched = canonical.replace(BEFORE, AFTER, 1)
        if hashlib.sha256(patched).hexdigest() != AFTER_SHA256:
            raise ValueError("JUCE MIDI reserve patch postimage hash mismatch")
        if b"\r\n" in original:
            patched = patched.replace(b"\n", b"\r\n")
        mode = stat.S_IMODE(target.stat().st_mode)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=target.parent, prefix=".luthier-midi-", delete=False) as stream:
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
        print("Applied JUCE 8.0.10 MIDI reserve patch (65536 bytes, constructor only)")
        return 0
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
