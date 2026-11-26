#!/usr/bin/env python3
"""Isolated patch verification against the JUCE 8.0.3 that pluginval v1.0.4 pins,
untouched or exactly patched.

Usage: python3 scripts/tests/test_patch_pluginval_vst3_runloop.py <pluginval source>/modules/juce
"""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "patch_pluginval_vst3_runloop.py"
SOURCE = Path(sys.argv.pop(1))
sys.path.insert(0, str(SCRIPT.parent))
import patch_pluginval_vst3_runloop as patch


def canonical_digest(data):
    return hashlib.sha256(data.replace(b"\r\n", b"\n")).hexdigest()


class PatchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="luthier-pluginval-patch-")
        self.root = Path(self.temporary.name)
        self.target = self.root / patch.RELATIVE
        self.target.parent.mkdir(parents=True)
        shutil.copyfile(SOURCE / patch.RELATIVE, self.target)
        original = self.target.read_bytes()
        canonical = original.replace(b"\r\n", b"\n")
        digest = hashlib.sha256(canonical).hexdigest()
        if digest == patch.AFTER_SHA256:
            # Reconstruct only the verified original in this isolated fixture.
            for before, after in reversed(patch.HUNKS):
                self.assertEqual(canonical.count(after), 1)
                canonical = canonical.replace(after, before, 1)
            self.assertEqual(hashlib.sha256(canonical).hexdigest(), patch.BEFORE_SHA256)
            original = canonical.replace(b"\n", b"\r\n") if b"\r\n" in original else canonical
            self.target.write_bytes(original)
        else:
            self.assertEqual(digest, patch.BEFORE_SHA256)
        self.original = original

    def tearDown(self):
        self.temporary.cleanup()

    def run_patch(self, *args):
        return subprocess.run([sys.executable, str(SCRIPT), str(self.root), *args], capture_output=True, text=True)

    def test_exact_patch_is_idempotent(self):
        self.assertEqual(self.run_patch().returncode, 0)
        patched = self.target.read_bytes()
        self.assertEqual(canonical_digest(patched), patch.AFTER_SHA256)
        # The fd callbacks hold the handler map weakly, never the RunLoop by `this`.
        self.assertEqual(patched.count(b"[weak = std::weak_ptr { eventHandlerMap }]"), 1)
        self.assertNotIn(b"registerFdCallback (fd, [this]", patched)
        self.assertEqual(self.run_patch().returncode, 0)
        self.assertEqual(self.run_patch("--check").returncode, 0)
        self.assertEqual(self.target.read_bytes(), patched)

    def test_unknown_preimage_refused_without_change(self):
        self.target.write_bytes(self.original + b"\n// unexpected change\n")
        before = self.target.read_bytes()
        self.assertNotEqual(self.run_patch().returncode, 0)
        self.assertEqual(self.target.read_bytes(), before)

    def test_check_refuses_unpatched_without_change(self):
        self.assertNotEqual(self.run_patch("--check").returncode, 0)
        self.assertEqual(self.target.read_bytes(), self.original)

    def test_line_endings_are_retained(self):
        canonical = self.original.replace(b"\r\n", b"\n")
        for ending in (b"\n", b"\r\n"):
            with self.subTest(ending=ending):
                self.target.write_bytes(canonical.replace(b"\n", ending))
                self.assertEqual(self.run_patch().returncode, 0)
                patched = self.target.read_bytes()
                self.assertEqual(canonical_digest(patched), patch.AFTER_SHA256)
                if ending == b"\n":
                    self.assertNotIn(b"\r\n", patched)
                else:
                    self.assertEqual(patched.count(b"\n"), patched.count(b"\r\n"))


if __name__ == "__main__":
    unittest.main()
