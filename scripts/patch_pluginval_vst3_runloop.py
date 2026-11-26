#!/usr/bin/env python3
"""Apply/check JUCE 04e167d on the JUCE 8.0.3 that pluginval v1.0.4 builds with.

pluginval v1.0.4 for Linux hosts VST3 plugins through JUCE 8.0.3's RunLoop. Its
fd callbacks capture the shared RunLoop::Impl by `this`. The Linux event loop
copies the callbacks it is about to call; when releasing the last plugin
instance in one of them destroys the Impl, a later copied callback reads the
freed handler map and pluginval segfaults (Tracktion/pluginval#185; release
run 45658630418, Linux Free VST3, at the deletion after "Fuzz parameters").
JUCE fixed it upstream in 04e167d (first released in 9.0.2): the callbacks hold
the map through a weak_ptr. This is that change, verbatim, on the 8.0.3 file.

Usage: patch_pluginval_vst3_runloop.py <juce_root> [--check]
"""
import argparse
import hashlib
import os
from pathlib import Path
import stat
import sys
import tempfile

RELATIVE = Path("modules/juce_audio_processors/format_types/juce_VST3PluginFormat.cpp")
BEFORE_SHA256 = "1c5c414039b17db0536fe51ac58fa06f69df3254e2269fb75b55848ca48b2183"
AFTER_SHA256 = "a906d5c31cb41808bc595b85ddacb267b90e794392debe58a4aee0282f9a3f83"

HUNKS = [
    (b"            for (const auto& h : eventHandlerMap)\n"
     b"                LinuxEventLoop::unregisterFdCallback (h.first);\n",
     b"            for (const auto& h : *eventHandlerMap)\n"
     b"                LinuxEventLoop::unregisterFdCallback (h.first);\n"),
    (b"            auto& handlers = eventHandlerMap[fd];\n",
     b"            auto& handlers = (*eventHandlerMap)[fd];\n"),
    (b"                LinuxEventLoop::registerFdCallback (fd, [this] (int descriptor)\n"
     b"                {\n"
     b"                    for (auto* h : eventHandlerMap[descriptor])\n"
     b"                        h->onFDIsSet (descriptor);\n",
     b"                // On each iteration, the Linux event loop makes a copy of the callbacks to invoke\n"
     b"                // before invoking them one-by-one. This means that, if the callback for FD A removes\n"
     b"                // the callback for FD B, FD B's callback might still get called on this iteration.\n"
     b"                // It's possible for ~Impl() to be called in FD A's callback before the callback for\n"
     b"                // FD B, which was just unregistered by ~Impl(). We use a weak_ref to check whether\n"
     b"                // the eventHandlerMap is still valid, and avoid accessing the map if not.\n"
     b"                LinuxEventLoop::registerFdCallback (fd, [weak = std::weak_ptr { eventHandlerMap }] (int descriptor)\n"
     b"                {\n"
     b"                    if (const auto strong = weak.lock())\n"
     b"                    {\n"
     b"                        const auto iter = strong->find (descriptor);\n"
     b"\n"
     b"                        if (iter == strong->end())\n"
     b"                            return true;\n"
     b"\n"
     b"                        for (auto* h : iter->second)\n"
     b"                            h->onFDIsSet (descriptor);\n"
     b"                    }\n"),
    (b"            for (auto iter = eventHandlerMap.begin(), end = eventHandlerMap.end(); iter != end;)\n",
     b"            for (auto iter = eventHandlerMap->begin(), end = eventHandlerMap->end(); iter != end;)\n"),
    (b"                        iter = eventHandlerMap.erase (iter);\n",
     b"                        iter = eventHandlerMap->erase (iter);\n"),
    (b"        std::unordered_map<Linux::FileDescriptor, std::vector<Linux::IEventHandler*>> eventHandlerMap;\n",
     b"        using Map = std::unordered_map<Linux::FileDescriptor, std::vector<Linux::IEventHandler*>>;\n"
     b"        std::shared_ptr<Map> eventHandlerMap = std::make_shared<Map>();\n"),
]


def patched_bytes(canonical):
    for before, after in HUNKS:
        if canonical.count(before) != 1:
            raise ValueError("pluginval JUCE source differs from the pinned 8.0.3 preimage; refusing to patch")
        canonical = canonical.replace(before, after, 1)
    return canonical


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("juce_root", type=Path)
    parser.add_argument("--check", action="store_true", help="Require the patch without writing")
    args = parser.parse_args()
    target = args.juce_root / RELATIVE
    try:
        original = target.read_bytes()
        canonical = original.replace(b"\r\n", b"\n")
        digest = hashlib.sha256(canonical).hexdigest()
        if digest == AFTER_SHA256:
            print("pluginval JUCE 8.0.3 VST3 run-loop patch verified (JUCE 04e167d)")
            return 0
        if digest != BEFORE_SHA256:
            raise ValueError("pluginval JUCE source differs from the pinned 8.0.3 preimage; refusing to patch")
        if args.check:
            raise ValueError("pluginval JUCE 8.0.3 VST3 run-loop patch is not applied")
        patched = patched_bytes(canonical)
        if hashlib.sha256(patched).hexdigest() != AFTER_SHA256:
            raise ValueError("pluginval JUCE VST3 run-loop patch postimage hash mismatch")
        if b"\r\n" in original:
            patched = patched.replace(b"\n", b"\r\n")
        mode = stat.S_IMODE(target.stat().st_mode)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=target.parent, prefix=".luthier-runloop-", delete=False) as stream:
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
        print("Applied JUCE 04e167d to pluginval's JUCE 8.0.3 (VST3 host RunLoop)")
        return 0
    except (OSError, ValueError) as error:
        print("patch_pluginval_vst3_runloop.py: %s" % error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
