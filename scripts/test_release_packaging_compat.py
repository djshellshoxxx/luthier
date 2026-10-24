"""Synthetic payload tests; no installation, build, network or publication."""
import hashlib
import os
from pathlib import Path
import subprocess
import shutil
import tarfile
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
PACKAGE = Path(os.environ.get("LUTHIER_PACKAGE_SCRIPT", REPO / "scripts/package_beta_linux.sh"))
VERIFY = Path(os.environ.get("LUTHIER_VERIFY_SCRIPT", REPO / "scripts/verify_release.sh"))


class PackagingFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="luthier packaging ")
        self.addCleanup(self.temp.cleanup)
        self.dist = Path(self.temp.name) / "dist with spaces"
        self.dist.mkdir()

    def stage(self, product, clap=True):
        stage = self.dist / "linux"
        binary = stage / f"{product}.vst3/Contents/x86_64-linux/{product}.so"
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_bytes(b"synthetic VST3 fixture\n")
        (stage / "luthier").write_bytes(b"#!/bin/sh\nexit 0\n")
        (stage / "luthier").chmod(0o755)
        (stage / "Resources/Presets").mkdir(parents=True, exist_ok=True)
        (stage / "Resources/Presets/factory.json").write_text('{"fixture":true}\n')
        if clap:
            (stage / f"{product}.clap").write_bytes(b"synthetic CLAP fixture\n")
        return stage

    def package(self, edition="PAID", success=True):
        env = dict(os.environ, DIST_DIR=str(self.dist), VERSION="0.1.0-preview",
                   GIT_SHA="01234567", SOURCE_DATE_EPOCH="1600000000", LUTHIER_EDITION=edition)
        result = subprocess.run(["bash", str(PACKAGE)], env=env, capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def verify(self, success=True, key=None):
        args = ["bash", str(VERIFY), str(self.dist)]
        if key:
            args.append(str(key))
        result = subprocess.run(args, capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result


class PackagingCompatibility(PackagingFixture):
    def test_pro_free_and_legacy_payloads_and_reproducibility(self):
        for product, edition, slug in (("Luthier Pro", "PAID", "luthier-pro"),
                                       ("Luthier Free", "FREE", "luthier-free"),
                                       ("Luthier", "PAID", "luthier")):
            with self.subTest(product=product), tempfile.TemporaryDirectory(dir=self.temp.name) as folder:
                self.dist = Path(folder)
                self.stage(product)
                self.package(edition)
                name = f"{slug}-0.1.0-preview-01234567-linux-x64"
                archive = self.dist / f"{name}.tar.gz"
                before = archive.read_bytes()
                self.verify(success=False)  # sidecar also protects extracted binaries
                with tarfile.open(archive) as payload:
                    names = payload.getnames()
                    self.assertIn(f"{name}/{product}.clap", names)
                    self.assertIn(f"{name}/{product}.vst3/Contents/x86_64-linux/{product}.so", names)
                    instructions = payload.extractfile(f"{name}/INSTALL.md").read().decode()
                    self.assertIn(f'cp -R "{product}.vst3"', instructions)
                    self.assertIn(f"{name}.SHA256SUMS", instructions)
                    payload.extractall(self.dist, filter="data")
                self.verify()
                self.package(edition)
                self.assertEqual(before, archive.read_bytes())
                (self.dist / name / "luthier").write_bytes(b"corrupted extracted binary")
                self.verify(success=False)

    def test_optional_clap_omission_still_has_valid_checksums(self):
        self.stage("Luthier Pro", clap=False)
        self.package()
        archive = next(self.dist.glob("*.tar.gz"))
        with tarfile.open(archive) as payload:
            self.assertFalse(any(n.endswith(".clap") for n in payload.getnames()))
            payload.extractall(self.dist, filter="data")
        self.verify()

    def test_requested_edition_is_selected_when_both_exist(self):
        self.stage("Luthier Pro")
        self.stage("Luthier Free")
        self.package("FREE")
        with tarfile.open(next(self.dist.glob("*.tar.gz"))) as payload:
            self.assertTrue(any("Luthier Free.vst3" in n for n in payload.getnames()))
            self.assertFalse(any("Luthier Pro" in n for n in payload.getnames()))

    def test_wrong_edition_and_invalid_edition_refused(self):
        self.stage("Luthier Pro")
        self.package("FREE", success=False)
        self.package("unknown", success=False)
        self.assertFalse(list(self.dist.glob("*.tar.gz")))

    def test_incomplete_payload_refused_before_archive_creation(self):
        stage = self.stage("Luthier Pro")
        for path in (stage / "luthier", stage / "Luthier Pro.clap",
                     stage / "Luthier Pro.vst3/Contents/x86_64-linux/Luthier Pro.so"):
            with self.subTest(path=path):
                saved = path.read_bytes()
                path.write_bytes(b"")
                self.package(success=False)
                self.assertFalse(list(self.dist.glob("*.tar.gz")))
                path.write_bytes(saved)
        (stage / "luthier").chmod(0o644)
        self.package(success=False)
        (stage / "luthier").chmod(0o755)
        (stage / "Resources/Presets/factory.json").unlink()
        self.package(success=False)


class ChecksumCompatibility(PackagingFixture):
    def record(self, name="payload [x]+.zip", contents=b"payload", binary=False):
        (self.dist / name).write_bytes(contents)
        return f"{hashlib.sha256(contents).hexdigest()} {'*' if binary else ' '}{name}\n"

    def test_each_manifest_filename_spaces_regex_chars_crlf_and_binary_marker(self):
        for manifest in ("SHA256SUMS.txt", "SHA256SUMS", "preview.SHA256SUMS"):
            with self.subTest(manifest=manifest):
                content = self.record(binary=True).replace("\n", "\r\n")
                path = self.dist / manifest
                path.write_bytes(content.encode())
                self.assertIn("signatures were not checked", self.verify().stdout)
                path.unlink()

    def test_missing_empty_malformed_and_duplicate_records_fail(self):
        line = self.record()
        self.verify(success=False)
        for content in ("", "\n", "not a hash\n", line + "garbage\n", line + line,
                        line.replace("  ", "\t", 1), "a" * 63 + "  payload [x]+.zip\n"):
            with self.subTest(content=content):
                (self.dist / "SHA256SUMS").write_text(content)
                self.verify(success=False)

    def test_corrupt_missing_unlisted_and_hidden_payloads_fail(self):
        path = self.dist / "SHA256SUMS.txt"
        path.write_text(self.record())
        self.verify()
        (self.dist / "payload [x]+.zip").write_bytes(b"corrupt")
        self.verify(success=False)
        (self.dist / "payload [x]+.zip").unlink()
        self.verify(success=False)
        path.write_text(self.record())
        for extra in ("unlisted.dmg", ".hidden.zip"):
            (self.dist / extra).write_bytes(b"extra")
            self.verify(success=False)
            (self.dist / extra).unlink()

    def test_all_manifests_checked_and_conflicts_rejected(self):
        line = self.record()
        (self.dist / "SHA256SUMS.txt").write_text(line)
        (self.dist / "SHA256SUMS").write_text(line)
        self.verify()
        (self.dist / "SHA256SUMS").write_text("0" * 64 + line[64:])
        self.verify(success=False)
        (self.dist / "SHA256SUMS").write_text("")
        self.verify(success=False)

    def test_legacy_manifest_summary_must_match_complete_hash_and_size_set(self):
        line = self.record(contents=b"payload")
        (self.dist / "SHA256SUMS").write_text(line)
        summary = self.dist / "manifest.txt"
        text = "Luthier 0.1.0 release manifest\n# sha256  bytes  file\n" + line[:64] + "  7  payload [x]+.zip\n"
        summary.write_text(text)
        self.verify()
        for bad in (text.replace("  7  ", "  8  "), text[:text.rfind("\n", 0, -1) + 1],
                    text + text.splitlines()[-1] + "\n", text.replace(line[:64], "0" * 64)):
            with self.subTest(summary=bad):
                summary.write_text(bad)
                self.verify(success=False)

    def test_traversal_absolute_and_symlink_paths_rejected(self):
        outside = Path(self.temp.name) / "outside.zip"
        outside.write_bytes(b"outside")
        digest = hashlib.sha256(b"outside").hexdigest()
        (self.dist / "link.zip").symlink_to(outside)
        (self.dist / "dirlink").symlink_to(outside.parent, target_is_directory=True)
        for name in ("../outside.zip", str(outside), "link.zip", "dirlink/outside.zip", "./link.zip", "a/../link.zip"):
            with self.subTest(name=name):
                (self.dist / "SHA256SUMS").write_text(f"{digest}  {name}\n")
                result = self.verify(success=False)
                self.assertRegex(result.stdout, "unsafe path|traverses a symlink")

    @unittest.skipUnless(shutil.which("gpg"), "gpg not installed")
    def test_each_manifest_signature_and_relative_public_key(self):
        keyring = Path(self.temp.name) / "fixture keyring"
        keyring.mkdir(mode=0o700)
        def gpg(*args):
            result = subprocess.run(["gpg", "--homedir", str(keyring), "--batch", "--yes",
                                     "--pinentry-mode", "loopback", "--passphrase", "", *args],
                                    capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            return result.stdout
        self.addCleanup(lambda: subprocess.run(["gpgconf", "--homedir", str(keyring), "--kill", "gpg-agent"],
                                               capture_output=True))
        gpg("--quick-generate-key", "Luthier fixture <fixture@example.invalid>", "ed25519", "sign", "0")
        public = Path(self.temp.name) / "fixture public.asc"
        public.write_bytes(gpg("--armor", "--export"))
        relative_key = os.path.relpath(public, Path.cwd())
        for name in ("SHA256SUMS.txt", "SHA256SUMS", "preview.SHA256SUMS"):
            with self.subTest(name=name):
                manifest = self.dist / name
                manifest.write_text(self.record(contents=b"signed payload"))
                self.assertIn(f"{name}.asc is missing", self.verify(success=False, key=relative_key).stdout)
                gpg("--armor", "--detach-sign", "--output", str(manifest) + ".asc", str(manifest))
                self.verify(key=relative_key)
                manifest.write_text(self.record(contents=b"changed payload"))
                self.assertIn("does not verify", self.verify(success=False, key=relative_key).stdout)
                manifest.unlink()
                Path(str(manifest) + ".asc").unlink()
        line = self.record(contents=b"legacy payload")
        (self.dist / "SHA256SUMS").write_text(line)
        summary = self.dist / "manifest.txt"
        summary.write_text("Luthier 0.1.0 release manifest\n# sha256  bytes  file\n" + line[:64] + "  14  payload [x]+.zip\n")
        gpg("--armor", "--detach-sign", "--output", str(summary) + ".asc", str(summary))
        self.verify(key=relative_key)
        summary.write_text(summary.read_text().replace("0.1.0", "0.1.1"))
        self.assertIn("does not verify", self.verify(success=False, key=relative_key).stdout)

if __name__ == "__main__":
    unittest.main()
