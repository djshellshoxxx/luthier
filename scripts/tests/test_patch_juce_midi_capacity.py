#!/usr/bin/env python3
"""Isolated patch verification against untouched or exactly patched JUCE 8.0.10."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "patch_juce_midi_capacity.py"
SOURCE = Path(sys.argv.pop(1))
sys.path.insert(0, str(SCRIPT.parent))
import patch_juce_midi_capacity as patch
RELATIVE = Path("modules/juce_audio_utils/players/juce_AudioProcessorPlayer.cpp")


class PatchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="luthier-juce-patch-")
        self.root = Path(self.temporary.name)
        self.target = self.root / RELATIVE
        self.target.parent.mkdir(parents=True)
        shutil.copyfile(SOURCE / RELATIVE, self.target)
        original = self.target.read_bytes()
        canonical = original.replace(b"\r\n", b"\n")
        digest = hashlib.sha256(canonical).hexdigest()
        if digest == patch.AFTER_SHA256:
            # Reconstruct only the verified original in this isolated fixture.
            canonical = canonical.replace(patch.AFTER, patch.BEFORE, 1)
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

    def test_exact_patch_is_idempotent_and_preserves_licence(self):
        self.assertEqual(self.run_patch().returncode, 0)
        patched = self.target.read_bytes()
        self.assertEqual(patched.count(b"incomingMidi.ensureSize (65536);"), 1)
        self.assertEqual(self.original.split(b"namespace juce")[0], patched.split(b"namespace juce")[0])
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
        self.target.write_bytes(self.original.replace(b"\r\n", b"\n"))
        self.assertEqual(self.run_patch().returncode, 0)
        self.assertNotIn(b"\r\n", self.target.read_bytes())
        digest = hashlib.sha256(self.target.read_bytes()).hexdigest()
        self.assertEqual(digest, "c307e3a5edf5a6f2862554673b186370edc43ee942b8c48a159f43ffff72541c")


if __name__ == "__main__":
    unittest.main()
