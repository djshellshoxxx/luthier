"""Structural release gate tests using synthetic headers, never real binaries."""
import hashlib
import io
import json
from pathlib import Path
import struct
import tarfile
import tempfile
import unittest
import zipfile

import prepare_pro_beta_assets as release


def header(platform):
    if platform == "windows":
        data = bytearray(256)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 60, 128)
        data[128:132] = b"PE\0\0"
        struct.pack_into("<H", data, 132, 0x8664)
        return bytes(data)
    if platform == "linux":
        data = bytearray(64)
        data[:6] = b"\x7fELF\x02\x01"
        struct.pack_into("<H", data, 18, 62)
        return bytes(data)
    data = bytearray(128)
    data[:4] = b"\xca\xfe\xba\xbe"
    struct.pack_into(">I", data, 4, 2)
    for index, cpu in enumerate((0x01000007, 0x0100000C)):
        offset = 64 + index * 32
        struct.pack_into(">IIIII", data, 8 + index * 20, cpu, 0, offset, 32, 0)
        data[offset:offset + 4] = b"\xcf\xfa\xed\xfe"
        struct.pack_into("<I", data, offset + 4, cpu)
    return bytes(data)


def payload(platform):
    if platform == "windows":
        names = ["Luthier Pro.exe", "Luthier Pro.vst3/Contents/x86_64-win/Luthier Pro.vst3",
                 "Luthier Pro.clap"]
    elif platform == "linux":
        names = ["luthier", "Luthier Pro.vst3/Contents/x86_64-linux/Luthier Pro.so",
                 "Luthier Pro.clap"]
    else:
        names = [f"Luthier Pro.{kind}/Contents/MacOS/Luthier Pro"
                 for kind in ("app", "vst3", "component", "clap")]
    files = {name: header(platform) for name in names}
    files.update({"README-BETA.md": b"Reviewed beta instructions", "Resources/Presets/a.json": b"{}"})
    if platform == "linux":
        files.update({"install.sh": b"#!/bin/sh\n", "VERSION": release.VERSION.encode()})
    return files


def write_archive(path, files):
    if path.name.endswith(".zip"):
        with zipfile.ZipFile(path, "w") as archive:
            for name, data in files.items():
                archive.writestr(name, data)
    else:
        with tarfile.open(path, "w:gz") as archive:
            for name, data in files.items():
                entry = tarfile.TarInfo("linux-package/" + name)
                entry.size = len(data)
                entry.mode = 0o755
                archive.addfile(entry, io.BytesIO(data))


class ReleaseAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "input"
        self.source.mkdir()
        self.output = self.root / "output"
        self.paths = {}
        for platform, (original, _) in release.CONTRACT.items():
            path = self.source / original
            write_archive(path, payload(platform))
            self.paths[platform] = path

    def prepare(self):
        release.prepare(self.source, self.output, "a" * 40)

    def test_complete_release_contains_exact_archives_provenance_and_matching_checksums(self):
        self.prepare()
        expected = {item[1] for item in release.CONTRACT.values()}
        self.assertEqual({path.name for path in self.output.iterdir()},
                         expected | {"BUILD-PROVENANCE.json", "SHA256SUMS"})
        proof = json.loads((self.output / "BUILD-PROVENANCE.json").read_text())
        self.assertEqual(proof["source_commit"], "a" * 40)
        self.assertEqual(proof["edition"], "Pro")
        self.assertEqual({p["platform"] for p in proof["payloads"]}, set(release.CONTRACT))
        for line in (self.output / "SHA256SUMS").read_text().splitlines():
            digest, name = line.split("  ")
            self.assertEqual(digest, hashlib.sha256((self.output / name).read_bytes()).hexdigest())

    def test_missing_platform_does_not_write_partial_output(self):
        self.paths["macos"].unlink()
        with self.assertRaisesRegex(ValueError, "expected exactly one"):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_duplicate_input_is_rejected(self):
        extra = self.source / "extra"
        extra.mkdir()
        write_archive(extra / self.paths["linux"].name, payload("linux"))
        with self.assertRaisesRegex(ValueError, "found 2"):
            self.prepare()

    def test_renamed_text_cannot_pass_as_windows_executable(self):
        files = payload("windows")
        files["Luthier Pro.exe"] = b"placeholder executable"
        write_archive(self.paths["windows"], files)
        with self.assertRaisesRegex(ValueError, "invalid windows executable"):
            self.prepare()

    def test_wrong_architecture_fails(self):
        data = bytearray(header("windows"))
        struct.pack_into("<H", data, 132, 0x14C)
        self.assertFalse(release.executable(bytes(data), "windows"))
        data = bytearray(header("linux"))
        struct.pack_into("<H", data, 18, 183)
        self.assertFalse(release.executable(bytes(data), "linux"))

    def test_mac_requires_both_real_slices_not_just_fat_magic(self):
        data = bytearray(header("macos"))
        self.assertTrue(release.executable(data, "macos"))
        data[96:100] = b"text"
        self.assertFalse(release.executable(data, "macos"))
        data = bytearray(header("macos"))
        struct.pack_into(">I", data, 4, 1)
        self.assertFalse(release.executable(data, "macos"))

    def test_resources_and_clap_are_required(self):
        for member in ("Resources/Presets/a.json", "Luthier Pro.clap"):
            files = payload("windows")
            del files[member]
            write_archive(self.paths["windows"], files)
            with self.assertRaises(ValueError):
                self.prepare()

    def test_free_edition_and_traversal_are_rejected(self):
        for name in ("Luthier Free.exe", "../outside", "/absolute", "C:/windows"):
            files = payload("windows")
            files[name] = b"unexpected"
            write_archive(self.paths["windows"], files)
            with self.assertRaises(ValueError):
                self.prepare()

    def test_wrong_version_is_rejected(self):
        files = payload("linux")
        files["VERSION"] = b"1.0.0-beta1"
        write_archive(self.paths["linux"], files)
        with self.assertRaisesRegex(ValueError, "incorrect Linux package version"):
            self.prepare()

    def test_output_cannot_be_overwritten(self):
        self.prepare()
        with self.assertRaisesRegex(ValueError, "must be empty"):
            self.prepare()

    def test_invalid_source_commit_is_rejected(self):
        for sha in ("main", "a" * 39, "A" * 40, "$(command)"):
            with self.assertRaisesRegex(ValueError, "source SHA"):
                release.prepare(self.source, self.output, sha)


if __name__ == "__main__":
    unittest.main()
