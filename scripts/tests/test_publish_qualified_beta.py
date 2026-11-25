"""Exercise the publication boundary with actual subprocesses and a fake gh CLI."""
from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/publish_qualified_beta.py"
spec = importlib.util.spec_from_file_location("publish_qualified_beta", SCRIPT)
publisher = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = publisher
spec.loader.exec_module(publisher)
SOURCE = "1" * 40
REPOSITORY = "fixture/luthier"
BETA2 = publisher.AUTHORIZED["refs/heads/release/v0.1.0-beta2"]
BETA3 = publisher.AUTHORIZED["refs/heads/release/v0.1.0-beta3"]

# Every process shares a JSON fixture and records its exact argv. No network,
# credentials, installed gh binary or real GitHub release is involved. The
# fixture names the one tag (and so candidate ref) this fake GitHub serves.
FAKE_GH = r'''#!/usr/bin/env python3
import json, os, pathlib, shutil, sys
args = sys.argv[1:]
path = pathlib.Path(os.environ["GH_FIXTURE"])
fixture = json.loads(path.read_text())
with open(os.environ["GH_CALL_LOG"], "a") as log:
    log.write(json.dumps(args) + "\n")
prefix = "repos/fixture/luthier"
release = fixture["release"]
tag = fixture["tag"]
if args[:2] == ["release", "download"]:
    assert args[:5] == ["release", "download", tag, "--repo", "fixture/luthier"]
    assert args[5] == "--dir" and len(args) == 7
    for source in pathlib.Path(fixture["remote"]).iterdir():
        shutil.copy2(source, pathlib.Path(args[6]) / source.name)
    fixture["downloaded"] = True
    path.write_text(json.dumps(fixture))
    sys.exit(0)
assert args[0] == "api", args
endpoint = args[1].removeprefix(prefix)
if fixture.get("fail_endpoint") == endpoint:
    sys.exit(9)
if fixture.get("malformed_endpoint") == endpoint:
    print("invalid JSON")
    sys.exit(0)
if "--method" in args:
    assert args == ["api", prefix + "/releases/91", "--method", "PATCH", "-F", "draft=false", "-F", "prerelease=true"], args
    if fixture.get("fail_publish"):
        sys.exit(8)
    release["draft"] = False
    release["html_url"] = "https://github.com/fixture/luthier/releases/tag/" + tag
    fixture["published"] = True
    path.write_text(json.dumps(fixture))
    print(json.dumps(release))
elif endpoint == "/git/ref/heads/release/" + tag:
    sha = fixture.get("candidate_sha", "1" * 40)
    if fixture.get("downloaded"):
        sha = fixture.get("candidate_after_download", sha)
    print(json.dumps({"ref": "refs/heads/release/" + tag, "object": {"type": "commit", "sha": sha}}))
elif endpoint == "/git/ref/tags/" + tag:
    print(json.dumps({"ref": "refs/tags/" + tag, "object": fixture.get("tag_object", {"type": "commit", "sha": "1" * 40})}))
elif endpoint == "/git/tags/" + "2" * 40:
    print(json.dumps({"object": fixture.get("annotation", {"type": "commit", "sha": "1" * 40})}))
elif endpoint == "/releases?per_page=100":
    assert args[2:] == ["--paginate"]
    print(json.dumps([{"id": 90, "tag_name": "v0.0.1"}]))
    print(json.dumps(fixture.get("releases", [release])))
elif endpoint == "/releases/91":
    if fixture.get("downloaded"):
        release.update(fixture.get("release_after_download", {}))
    if fixture.get("published"):
        release.update(fixture.get("release_after_publish", {}))
    print(json.dumps(release))
elif endpoint == "/releases/91/assets?per_page=100":
    assert args[2:] == ["--paginate"]
    assets = fixture["assets"]
    if fixture.get("downloaded") and fixture.get("change_asset_id"):
        assets[0]["id"] += 500
    if fixture.get("published") and fixture.get("change_published_asset"):
        assets[0]["size"] += 1
    print(json.dumps(assets[:5]))
    print(json.dumps(assets[5:]))
else:
    raise AssertionError(args)
'''


class PublicationGuards:
    """Checks every authorized candidate keeps; each subclass names one candidate."""

    candidate: publisher.Candidate
    # As release.yml's "Record source and qualification provenance" step writes them.
    platforms: list[str]

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="luthier-publication-test-")
        self.root = Path(self.temporary.name)
        self.local = self.root / "local"
        self.remote = self.root / "remote"
        self.local.mkdir()
        self.remote.mkdir()
        self.fixture_path = self.root / "fixture.json"
        self.log = self.root / "calls.jsonl"
        binary = self.root / "bin"
        binary.mkdir()
        (binary / "gh").write_text(FAKE_GH)
        (binary / "gh").chmod(0o755)
        self.env = dict(os.environ, PATH=str(binary) + os.pathsep + os.environ["PATH"],
                        GH_FIXTURE=str(self.fixture_path), GH_CALL_LOG=str(self.log),
                        GITHUB_EVENT_NAME="push", GITHUB_REF=self.candidate.ref,
                        GITHUB_SHA=SOURCE, QUALIFICATION_RESULT="success",
                        GITHUB_REPOSITORY=REPOSITORY, GITHUB_RUN_ID="1234",
                        GITHUB_SERVER_URL="https://github.com")
        for name in self.candidate.payloads:
            (self.local / name).write_bytes(("qualified " + name).encode())
        for name in self.candidate.sidecars:
            archive = name.removesuffix(".sha256")
            (self.local / name).write_text(self.checksum(archive), encoding="ascii")
        self.provenance = {"source_commit": SOURCE, "version": self.candidate.version,
                           "platforms": self.platforms, "editions": ["pro", "free"],
                           "ci_run": f"https://github.com/{REPOSITORY}/actions/runs/1234"}
        (self.local / publisher.PROVENANCE).write_text(json.dumps(self.provenance))
        self.manifest()
        self.fixture = {
            "remote": str(self.remote),
            "tag": self.candidate.tag,
            "release": {"id": 91, "tag_name": self.candidate.tag, "target_commitish": SOURCE,
                        "name": "Luthier " + self.candidate.version, "draft": True, "prerelease": True,
                        "url": f"https://api.github.com/repos/{REPOSITORY}/releases/91",
                        "html_url": f"https://github.com/{REPOSITORY}/releases/tag/untagged-fixture"},
        }
        self.sync_assets()

    def tearDown(self):
        self.temporary.cleanup()

    def checksum(self, name):
        return hashlib.sha256((self.local / name).read_bytes()).hexdigest() + "  " + name + "\n"

    def manifest(self):
        # release.yml checksums every downloaded asset; only SHA256SUMS* are left out.
        (self.local / publisher.MANIFEST).write_text("".join(
            self.checksum(path.name) for path in sorted(self.local.iterdir())
            if path.name not in (publisher.MANIFEST, publisher.SIGNATURE)))

    def sync_assets(self):
        self.fixture["assets"] = []
        for stale in self.remote.iterdir():
            stale.unlink()
        for index, asset in enumerate(sorted(self.local.iterdir())):
            shutil.copy2(asset, self.remote / asset.name)
            self.fixture["assets"].append({"id": 100 + index, "name": asset.name, "state": "uploaded",
                                          "size": asset.stat().st_size,
                                          "digest": "sha256:" + hashlib.sha256(asset.read_bytes()).hexdigest()})

    def add(self, *names):
        for name in names:
            (self.local / name).write_text("optional " + name + "\n")
        self.manifest()
        self.sync_assets()

    def sign(self, *packages):
        """The complete signature set one configured GPG key writes."""
        signed = set(self.candidate.linux_payloads) | set(packages)
        for name in sorted(signed):
            (self.local / (name + ".asc")).write_text("detached-signature-fixture " + name + "\n")
        self.manifest()
        (self.local / publisher.SIGNATURE).write_text("detached-signature-fixture\n")
        self.sync_assets()

    def run_publisher(self, tag=None):
        self.fixture_path.write_text(json.dumps(self.fixture))
        return subprocess.run([sys.executable, str(SCRIPT), "--directory", str(self.local),
                               "--source", SOURCE, "--tag", tag or self.candidate.tag],
                              env=self.env, capture_output=True, text=True)

    def calls(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def mutations(self):
        return [args for args in self.calls() if "--method" in args or args[:2] in
                (["release", "edit"], ["release", "create"], ["release", "upload"], ["release", "delete"])]

    def assert_refused(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.mutations(), [], result.stderr)

    def assert_published(self, result):
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(self.mutations()), 1)

    def test_verified_draft_publishes_exact_numeric_id_once(self):
        result = self.run_publisher()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Published and verified prerelease", result.stdout)
        self.assertEqual(self.mutations(), [["api", "repos/fixture/luthier/releases/91", "--method", "PATCH",
                                             "-F", "draft=false", "-F", "prerelease=true"]])
        self.assertFalse(json.loads(self.fixture_path.read_text())["release"]["draft"])
        self.assertEqual(sum("/git/ref/heads/" in args[1] for args in self.calls() if args[0] == "api"), 3)

    def test_optional_signature_bytes_are_verified(self):
        self.sign()
        self.assertEqual(self.run_publisher().returncode, 0)

    def test_matching_annotated_tag_is_supported(self):
        self.fixture["tag_object"] = {"type": "tag", "sha": "2" * 40}
        self.assertEqual(self.run_publisher().returncode, 0)

    def test_dispatch_tag_push_and_other_candidates_do_not_publish(self):
        other = BETA3 if self.candidate is BETA2 else BETA2
        for changes, tag in [({"GITHUB_EVENT_NAME": "workflow_dispatch"}, self.candidate.tag),
                             ({"GITHUB_REF": "refs/tags/" + self.candidate.tag}, self.candidate.tag),
                             ({"GITHUB_REF": other.ref}, self.candidate.tag),
                             ({}, other.tag),
                             # Unlisted candidates and near-miss refs keep draft review.
                             ({"GITHUB_EVENT_NAME": "pull_request"}, self.candidate.tag),
                             ({"GITHUB_REF": "refs/heads/release/v0.1.0-beta4"}, "v0.1.0-beta4"),
                             ({"GITHUB_REF": self.candidate.ref + "-fix"}, self.candidate.tag + "-fix"),
                             ({"GITHUB_REF": self.candidate.ref.upper()}, self.candidate.tag),
                             ({"GITHUB_REF": "refs/heads/" + self.candidate.tag}, self.candidate.tag)]:
            with self.subTest(changes=changes, tag=tag):
                previous = self.env.copy()
                self.env.update(changes)
                self.assert_refused(self.run_publisher(tag))
                self.assertEqual(self.calls(), [])
                self.env = previous

    def test_failed_qualification_and_wrong_push_source_are_refused(self):
        for key, value in [("QUALIFICATION_RESULT", "failure"), ("QUALIFICATION_RESULT", "skipped"),
                           ("GITHUB_SHA", "3" * 40), ("GITHUB_SERVER_URL", "https://other.invalid")]:
            with self.subTest(key=key, value=value):
                previous = self.env[key]
                self.env[key] = value
                self.assert_refused(self.run_publisher())
                self.assertEqual(self.calls(), [])
                self.env[key] = previous

    def test_candidate_advance_before_or_after_download_is_refused(self):
        for field in ("candidate_sha", "candidate_after_download"):
            with self.subTest(field=field):
                self.fixture[field] = "3" * 40
                self.assert_refused(self.run_publisher())
                del self.fixture[field]

    def test_mismatched_tag_and_recursive_annotation_are_refused(self):
        for target in ({"type": "commit", "sha": "3" * 40}, {"type": "tag", "sha": "2" * 40}):
            with self.subTest(target=target):
                self.fixture["tag_object"] = target
                self.fixture["annotation"] = target
                self.assert_refused(self.run_publisher())

    def test_provenance_requires_exact_source_run_platforms_and_editions(self):
        for field, value in [("source_commit", "3" * 40), ("version", "0.1.0-beta1"),
                             ("ci_run", "https://github.com/fixture/luthier/actions/runs/12"),
                             ("platforms", ["windows"]), ("editions", ["pro"])]:
            with self.subTest(field=field):
                changed = dict(self.provenance, **{field: value})
                (self.local / publisher.PROVENANCE).write_text(json.dumps(changed))
                self.manifest()
                self.sync_assets()
                self.assert_refused(self.run_publisher())

    def test_missing_extra_and_symlink_assets_are_refused(self):
        asset = self.local / sorted(self.candidate.payloads)[0]
        original = asset.read_bytes()
        asset.unlink()
        self.assert_refused(self.run_publisher())
        asset.write_bytes(original)
        extra = self.local / "unexpected.zip"
        extra.write_text("extra")
        self.assert_refused(self.run_publisher())
        extra.unlink()
        asset.unlink()
        asset.symlink_to(self.remote / asset.name)
        self.assert_refused(self.run_publisher())

    def test_bad_duplicate_and_omitted_checksums_are_refused(self):
        path = self.local / publisher.MANIFEST
        original = path.read_text()
        first = original.splitlines(keepends=True)[0]
        for contents in (original + first, original.replace(first, ""), original.replace(first[:64], "0" * 64),
                         original + "0" * 64 + "  ../unexpected.zip\n"):
            with self.subTest(contents=contents[:70]):
                path.write_text(contents)
                self.assert_refused(self.run_publisher())

    def test_sidecar_must_match_portable_archive_even_with_valid_global_manifest(self):
        sidecar = sorted(self.candidate.sidecars)[0]
        (self.local / sidecar).write_text("0" * 64 + "  " + sidecar.removesuffix(".sha256") + "\n")
        self.manifest()
        self.assert_refused(self.run_publisher())

    def test_draft_identity_and_publication_state_are_required(self):
        original = copy.deepcopy(self.fixture["release"])
        for field, value in [("id", True), ("name", "Other release"), ("draft", False),
                             ("prerelease", False), ("target_commitish", "master"),
                             ("url", "https://api.github.com/repos/other/repo/releases/91")]:
            with self.subTest(field=field):
                self.fixture["release"] = dict(original, **{field: value})
                self.assert_refused(self.run_publisher())

    def test_no_draft_and_duplicate_drafts_are_refused_across_pages(self):
        for releases in ([], [self.fixture["release"], self.fixture["release"]]):
            with self.subTest(count=len(releases)):
                self.fixture["releases"] = releases
                self.assert_refused(self.run_publisher())

    def test_asset_sizes_digests_states_and_identity_are_required(self):
        original = copy.deepcopy(self.fixture["assets"])
        cases = [("size", 1), ("size", True), ("state", "new"), ("digest", None),
                 ("digest", "sha256:" + "0" * 64), ("id", True), ("name", "unexpected.zip")]
        for field, value in cases:
            with self.subTest(field=field, value=value):
                self.fixture["assets"] = copy.deepcopy(original)
                self.fixture["assets"][0][field] = value
                self.assert_refused(self.run_publisher())
        for assets in (original[:-1], original + [original[0]]):
            self.fixture["assets"] = assets
            self.assert_refused(self.run_publisher())

    def test_downloaded_bytes_must_match_even_when_metadata_matches(self):
        asset = self.remote / sorted(self.candidate.payloads)[0]
        asset.write_bytes(b"mismatch")
        self.assert_refused(self.run_publisher())

    def test_changed_draft_or_asset_ids_after_download_are_refused(self):
        for changes in ({"change_asset_id": True}, {"release_after_download": {"id": 92}},
                        {"release_after_download": {"draft": False}}):
            with self.subTest(changes=changes):
                self.fixture.update(changes)
                self.assert_refused(self.run_publisher())
                for field in changes:
                    del self.fixture[field]

    def test_api_errors_and_malformed_json_fail_before_publication(self):
        for field in ("fail_endpoint", "malformed_endpoint"):
            with self.subTest(field=field):
                self.fixture[field] = "/releases?per_page=100"
                self.assert_refused(self.run_publisher())
                del self.fixture[field]

    def test_publication_failure_is_not_retried(self):
        self.fixture["fail_publish"] = True
        result = self.run_publisher()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(len(self.mutations()), 1)
        self.assertTrue(json.loads(self.fixture_path.read_text())["release"]["draft"])

    def test_post_publication_failures_report_failure_without_rollback(self):
        for changes in ({"release_after_publish": {"prerelease": False}}, {"change_published_asset": True}):
            with self.subTest(changes=changes):
                self.log.unlink(missing_ok=True)
                self.fixture.update(changes)
                result = self.run_publisher()
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(len(self.mutations()), 1)
                self.assertIn("inspect the draft/release state", result.stderr)
                self.assertFalse(json.loads(self.fixture_path.read_text())["release"]["draft"])
                for field in changes:
                    del self.fixture[field]

    def test_each_missing_package_signature_in_a_signed_set_is_refused(self):
        # A signature set is complete for this candidate or the inventory is refused.
        for name in sorted(self.candidate.linux_payloads):
            with self.subTest(unsigned=name):
                self.sign()
                (self.local / (name + ".asc")).unlink()
                self.manifest()
                self.sync_assets()
                self.assert_refused(self.run_publisher())
                for signature in self.local.glob("*.asc"):
                    signature.unlink()
        self.manifest()
        self.sync_assets()
        self.assert_published(self.run_publisher())


class Beta2PublicationGuards(PublicationGuards, unittest.TestCase):
    candidate = BETA2
    platforms = ["windows", "macos"]

    def test_beta2_asset_set_is_unchanged(self):
        version = "0.1.0-beta2"
        payloads = {f"Luthier-{edition}-{version}-{suffix}" for edition in ("Pro", "Free")
                    for suffix in ("Setup-win64.exe", "portable-win64.zip", "macOS.dmg")}
        payloads.add(f"Luthier-Pro-{version}-macOS-universal.zip")
        sidecars = {f"Luthier-{edition}-{version}-portable-win64.zip.sha256" for edition in ("Pro", "Free")}
        self.assertEqual(self.candidate.ref, "refs/heads/release/v0.1.0-beta2")
        self.assertEqual(self.candidate.tag, "v0.1.0-beta2")
        self.assertEqual(self.candidate.platforms, ("windows", "macos"))
        self.assertEqual(self.candidate.payloads, payloads)
        self.assertEqual(self.candidate.sidecars, sidecars)
        self.assertEqual(self.candidate.required,
                         payloads | sidecars | {"SHA256SUMS.txt", "BUILD-PROVENANCE.json"})
        self.assertEqual(self.candidate.rpms, set())
        self.assertEqual(self.candidate.linux_payloads, set())

    def test_manifest_signature_alone_is_the_complete_beta2_set(self):
        (self.local / publisher.SIGNATURE).write_text("detached-signature-fixture\n")
        self.sync_assets()
        self.assert_published(self.run_publisher())

    def test_linux_assets_are_not_part_of_beta2(self):
        for name in ("Luthier-Pro-0.1.0-beta2-linux-x64.tar.gz", "luthier-pro_0.1.0-beta2_amd64.deb",
                     "luthier-pro-0.1.0-beta2-1.x86_64.rpm"):
            with self.subTest(name=name):
                self.add(name)
                self.assert_refused(self.run_publisher())
                (self.local / name).unlink()
                self.manifest()
                self.sync_assets()

    def test_beta3_provenance_scope_does_not_publish_beta2(self):
        changed = dict(self.provenance, platforms=["linux", "windows", "macos"])
        (self.local / publisher.PROVENANCE).write_text(json.dumps(changed))
        self.manifest()
        self.sync_assets()
        self.assert_refused(self.run_publisher())


class Beta3PublicationGuards(PublicationGuards, unittest.TestCase):
    candidate = BETA3
    platforms = ["linux", "windows", "macos"]
    LINUX = {"Luthier-Pro-0.1.0-beta3-linux-x64.tar.gz", "Luthier-Free-0.1.0-beta3-linux-x64.tar.gz",
             "Luthier-Pro-0.1.0-beta3-standalone-linux-x64.tar.gz",
             "Luthier-Free-0.1.0-beta3-standalone-linux-x64.tar.gz",
             "luthier-pro_0.1.0-beta3_amd64.deb", "luthier-free_0.1.0-beta3_amd64.deb"}
    RPMS = {"luthier-pro-0.1.0-beta3-1.x86_64.rpm", "luthier-free-0.1.0-beta3-1.x86_64.rpm"}

    def test_beta3_asset_set_is_exact(self):
        version = "0.1.0-beta3"
        native = {f"Luthier-{edition}-{version}-{suffix}" for edition in ("Pro", "Free")
                  for suffix in ("Setup-win64.exe", "portable-win64.zip", "macOS.dmg")}
        native.add(f"Luthier-Pro-{version}-macOS-universal.zip")
        sidecars = {f"Luthier-{edition}-{version}-portable-win64.zip.sha256" for edition in ("Pro", "Free")}
        self.assertEqual(self.candidate.ref, "refs/heads/release/v0.1.0-beta3")
        self.assertEqual(self.candidate.tag, "v0.1.0-beta3")
        self.assertEqual(self.candidate.platforms, ("linux", "windows", "macos"))
        self.assertEqual(self.candidate.linux_payloads, self.LINUX)
        self.assertEqual(self.candidate.payloads, native | self.LINUX)
        self.assertEqual(self.candidate.sidecars, sidecars)
        self.assertEqual(self.candidate.required,
                         native | self.LINUX | sidecars | {"SHA256SUMS.txt", "BUILD-PROVENANCE.json"})
        self.assertEqual(self.candidate.rpms, self.RPMS)

    def test_each_missing_linux_asset_is_refused(self):
        for name in sorted(self.LINUX):
            with self.subTest(name=name):
                original = (self.local / name).read_bytes()
                (self.local / name).unlink()
                self.manifest()
                self.sync_assets()
                self.assert_refused(self.run_publisher())
                (self.local / name).write_bytes(original)
                self.manifest()
                self.sync_assets()
        self.assert_published(self.run_publisher())

    def test_rpm_is_optional_for_each_edition(self):
        for rpms in (["luthier-pro-0.1.0-beta3-1.x86_64.rpm"], ["luthier-free-0.1.0-beta3-1.x86_64.rpm"],
                     sorted(self.RPMS)):
            with self.subTest(rpms=rpms):
                self.log.unlink(missing_ok=True)
                self.fixture["release"]["draft"] = True
                self.add(*rpms)
                self.assert_published(self.run_publisher())
                for name in rpms:
                    (self.local / name).unlink()
                self.manifest()
                self.sync_assets()

    def test_unexpected_linux_extras_are_refused(self):
        for name in ("luthier-pro-0.1.0-beta3-2.x86_64.rpm", "luthier-pro-0.1.0-beta2-1.x86_64.rpm",
                     "luthier-0.1.0-beta3-1.x86_64.rpm", "luthier-pro_0.1.0-beta3_arm64.deb",
                     "luthier_0.1.0-beta3_amd64.deb", "Luthier-Pro-0.1.0-beta3-linux-arm64.tar.gz",
                     "Luthier-Pro-0.1.0-beta3-linux-x64.tar.gz.sha256",
                     "Luthier-Pro-0.1.0-beta2-linux-x64.tar.gz", "Luthier-Pro-0.1.0-beta3-linux-x64.zip"):
            with self.subTest(name=name):
                self.add(name)
                self.assert_refused(self.run_publisher())
                (self.local / name).unlink()
                self.manifest()
                self.sync_assets()

    def test_complete_signature_sets_publish_with_or_without_rpms(self):
        for rpms in ([], sorted(self.RPMS)):
            with self.subTest(rpms=rpms):
                self.log.unlink(missing_ok=True)
                self.fixture["release"]["draft"] = True
                self.add(*rpms)
                self.sign(*rpms)
                self.assert_published(self.run_publisher())
                for name in [*rpms, *(path.name for path in self.local.glob("*.asc"))]:
                    (self.local / name).unlink()
                self.manifest()
                self.sync_assets()

    def test_partial_signature_sets_are_refused(self):
        def manifest_signature_only():
            (self.local / publisher.SIGNATURE).write_text("signature\n")

        def package_signatures_only():
            for name in self.LINUX:
                (self.local / (name + ".asc")).write_text("signature\n")

        def unsigned_rpm_in_a_signed_set():
            self.add("luthier-pro-0.1.0-beta3-1.x86_64.rpm")
            self.sign()

        def signature_for_an_absent_rpm():
            (self.local / "luthier-free-0.1.0-beta3-1.x86_64.rpm.asc").write_text("signature\n")
            self.sign()

        for prepare in (manifest_signature_only, package_signatures_only,
                        unsigned_rpm_in_a_signed_set, signature_for_an_absent_rpm):
            with self.subTest(prepare.__name__):
                prepare()
                self.manifest()
                self.sync_assets()
                self.assert_refused(self.run_publisher())
                for path in [*self.local.glob("*.asc"), *self.local.glob("*.rpm")]:
                    path.unlink()
                self.manifest()
                self.sync_assets()
        self.assert_published(self.run_publisher())

    def test_provenance_platforms_must_be_the_recorded_beta3_list(self):
        for platforms in (["windows", "macos"], ["macos", "windows", "linux"], ["linux", "macos", "windows"],
                          ["linux", "windows", "macos", "linux"], ["linux"], ["linux", "windows"],
                          "linux,windows,macos", None):
            with self.subTest(platforms=platforms):
                changed = dict(self.provenance, platforms=platforms)
                (self.local / publisher.PROVENANCE).write_text(json.dumps(changed))
                self.manifest()
                self.sync_assets()
                self.assert_refused(self.run_publisher())

    def test_wrong_ref_or_tag_for_beta3_is_refused_before_any_request(self):
        for ref, tag in [(BETA3.ref, BETA2.tag), (BETA2.ref, BETA3.tag), ("refs/tags/v0.1.0-beta3", BETA3.tag),
                         ("refs/heads/release/v0.1.0-beta3 ", BETA3.tag), (BETA3.ref, "v0.1.0-beta3 "),
                         ("refs/heads/release/v0.1.0-Beta3", BETA3.tag), (BETA3.ref, "0.1.0-beta3")]:
            with self.subTest(ref=ref, tag=tag):
                self.env["GITHUB_REF"] = ref
                self.assert_refused(self.run_publisher(tag))
                self.assertEqual(self.calls(), [])
        self.env["GITHUB_REF"] = BETA3.ref
        self.assert_published(self.run_publisher())


class AllowlistShape(unittest.TestCase):
    def test_only_beta2_and_beta3_are_authorized(self):
        self.assertEqual(set(publisher.AUTHORIZED), {"refs/heads/release/v0.1.0-beta2",
                                                     "refs/heads/release/v0.1.0-beta3"})
        for ref, candidate in publisher.AUTHORIZED.items():
            self.assertEqual(ref, "refs/heads/release/" + candidate.tag)

    def test_malformed_candidates_fail_closed(self):
        for version, platforms in [("0.1.0", ("linux",)), ("0.1.0-beta3/x", ("linux",)), ("0.1.0-beta3", ()),
                                   ("0.1.0-beta3", ("macos", "windows")), ("0.1.0-beta3", ("linux", "linux")),
                                   ("0.1.0-beta3", ("freebsd",))]:
            with self.subTest(version=version, platforms=platforms):
                with self.assertRaises(ValueError):
                    publisher.Candidate(version, platforms)


class PackagerOutputNames(unittest.TestCase):
    """The publisher's names follow the packaging scripts that write them."""

    def test_windows_and_macos_templates_match_the_publisher(self):
        iss = (ROOT / "packaging/windows/Luthier.iss").read_text()
        windows = (ROOT / "scripts/package_windows.ps1").read_text()
        macos = (ROOT / "scripts/package_macos.sh").read_text()
        build = (ROOT / ".github/workflows/build.yml").read_text()
        self.assertIn("OutputBaseFilename=Luthier-{#EditionSlug}-{#AppVersion}-Setup-win64", iss)
        self.assertIn('$zip = Join-Path $out "Luthier-$slug-$Version-portable-win64.zip"', windows)
        self.assertIn('Set-Content -Path "$zip.sha256"', windows)
        self.assertIn("'PAID'  { $Edition = 'PAID'; $slug = 'Pro' }", windows)
        self.assertIn("'FREE'  { $Edition = 'FREE'; $slug = 'Free' }", windows)
        self.assertIn('dmg="$OUT/Luthier-$EDITION_SLUG-$VERSION-macOS.dmg"', macos)
        self.assertIn('"$DIST_DIR/installers/Luthier-Pro-${{ inputs.version }}-macOS-universal.zip"', build)

    def test_linux_templates_match_the_publisher(self):
        linux = (ROOT / "scripts/package_linux.sh").read_text()
        for line in ('PAID) PRODUCT_NAME="Luthier Pro";  EDITION_SLUG=Pro;  PKG_NAME=luthier-pro;',
                     'FREE) PRODUCT_NAME="Luthier Free"; EDITION_SLUG=Free; PKG_NAME=luthier-free;',
                     'name="Luthier-$EDITION_SLUG-$VERSION-linux-x64"',
                     'sname="Luthier-$EDITION_SLUG-$VERSION-standalone-linux-x64"',
                     'deb="$OUT/${PKG_NAME}_${VERSION}_amd64.deb"',
                     'Name:           $PKG_NAME', 'Release:        1', 'BuildArch:      x86_64',
                     'for f in "$OUT"/*.tar.gz "$OUT"/*.deb "$OUT"/*.rpm; do',
                     '--armor --detach-sign --output "$f.asc" "$f"'):
            self.assertIn(line, linux)


class LinuxPackagingOutputs(unittest.TestCase):
    """Run the real scripts/package_linux.sh on a synthetic stage for both editions."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="luthier-linux-names-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def package(self, edition, product, **extra):
        dist = self.root / ("dist-" + edition)
        stage = dist / "linux"
        library = stage / f"{product}.vst3/Contents/x86_64-linux/{product}.so"
        library.parent.mkdir(parents=True)
        library.write_bytes(b"synthetic VST3 fixture\n")
        (stage / f"{product}.clap").write_bytes(b"synthetic CLAP fixture\n")
        (stage / "luthier").write_bytes(b"#!/bin/sh\nexit 0\n")
        (stage / "luthier").chmod(0o755)
        (stage / "Resources/Presets").mkdir(parents=True)
        (stage / "Resources/Presets/factory.json").write_text('{"fixture":true}\n')
        env = {key: value for key, value in os.environ.items() if not key.startswith("GPG_")}
        env.update(DIST_DIR=str(dist), VERSION=BETA3.version, LUTHIER_EDITION=edition,
                   SOURCE_DATE_EPOCH="1600000000",
                   # The packaging gate also rejects unfinished notes.
                   BETA_README=str(ROOT / "docs/beta/PRO-V01-BETA3-RELEASE-NOTES.md"), **extra)
        result = subprocess.run(["bash", str(ROOT / "scripts/package_linux.sh")], env=env,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return {path.name for path in (dist / "installers").iterdir()}

    def outputs(self, **extra):
        return self.package("PAID", "Luthier Pro", **extra) | self.package("FREE", "Luthier Free", **extra)

    @unittest.skipUnless(shutil.which("dpkg-deb"), "dpkg-deb not installed")
    def test_unsigned_outputs_are_the_beta3_linux_payloads(self):
        names = self.outputs()
        rpms = names & BETA3.rpms  # rpmbuild is best effort and may be absent
        self.assertEqual(names - rpms, BETA3.linux_payloads)
        others = BETA3.required - BETA3.linux_payloads
        self.assertTrue(BETA3.accepts(others | names))

    @unittest.skipUnless(shutil.which("dpkg-deb") and shutil.which("gpg"), "dpkg-deb or gpg not installed")
    def test_signed_outputs_are_the_payloads_and_their_detached_signatures(self):
        keyring = self.root / "k"
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
        key = gpg("--armor", "--export-secret-keys").decode()
        names = self.outputs(GPG_PRIVATE_KEY=key, GPG_PASSPHRASE="")
        packages = BETA3.linux_payloads | (names & BETA3.rpms)
        self.assertEqual(names, packages | {name + ".asc" for name in packages})
        others = BETA3.required - BETA3.linux_payloads
        # release.yml signs the manifest with the same key: the set is complete with it only.
        self.assertTrue(BETA3.accepts(others | names | {publisher.SIGNATURE}))
        self.assertFalse(BETA3.accepts(others | names))


class WorkflowPublicationGuards(unittest.TestCase):
    def test_workflow_keeps_full_qualification_and_narrows_publication(self):
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        self.assertIn("needs: [version, build]", workflow)
        self.assertIn("needs: [version, installer-checks]", workflow)
        # Owner-approved beta3 scope: Linux joins, and CLAP is built, validated and packaged.
        self.assertIn("platforms: ${{ inputs.platforms || 'linux,windows,macos' }}", workflow)
        self.assertIn("editions: ${{ inputs.editions || 'pro,free' }}", workflow)
        for gate in ("strictness: 10", "lto: true", "package: true", "no_clap: false"):
            self.assertIn(gate, workflow)
        self.assertNotIn("no_clap: true", workflow)
        for version in ("0.1.0-beta2", "0.1.0-beta3"):
            with self.subTest(version=version):
                self.assertIn("if: github.event_name == 'push' && github.ref == 'refs/heads/release/v"
                              + version + "' && needs.version.outputs.tag == 'v" + version + "'", workflow)
                self.assertLess(workflow.index("- name: Create draft release"),
                                workflow.index("- name: Publish the authorized qualified " + version[6:]))
        self.assertIn("QUALIFICATION_RESULT: ${{ needs.build.result }}", workflow)
        self.assertEqual(workflow.count("QUALIFICATION_RESULT: ${{ needs.build.result }}"), 2)
        self.assertIn("-p test_publish_qualified_beta.py", workflow)
        self.assertNotIn("continue-on-error:", workflow)

    def test_every_publication_and_consolidation_is_gated_by_an_exact_candidate(self):
        lines = (ROOT / ".github/workflows/release.yml").read_text().splitlines()
        gates = {}
        for index, line in enumerate(lines):
            for script in ("scripts/publish_qualified_beta.py", "scripts/consolidate_qualified_beta.py"):
                if script in line and "python3 " + script in line:
                    gate = next(lines[i].strip() for i in range(index, -1, -1)
                                if lines[i].strip().startswith("if: "))
                    gates.setdefault(script, []).append(gate)
        exact = {"if: github.event_name == 'push' && github.ref == 'refs/heads/release/v" + version
                 + "' && needs.version.outputs.tag == 'v" + version + "'" for version in ("0.1.0-beta2", "0.1.0-beta3")}
        for script in ("scripts/publish_qualified_beta.py", "scripts/consolidate_qualified_beta.py"):
            with self.subTest(script=script):
                self.assertEqual(sorted(gates[script]), sorted(exact))
        release_refs = [line for line in lines if "refs/heads/release/" in line]
        self.assertEqual(len(release_refs), 4)
        self.assertTrue(all(line.strip() in exact for line in release_refs))


if __name__ == "__main__":
    unittest.main()
