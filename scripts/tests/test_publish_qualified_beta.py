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

# Every process shares a JSON fixture and records its exact argv. No network,
# credentials, installed gh binary or real GitHub release is involved.
FAKE_GH = r'''#!/usr/bin/env python3
import json, os, pathlib, shutil, sys
args = sys.argv[1:]
path = pathlib.Path(os.environ["GH_FIXTURE"])
fixture = json.loads(path.read_text())
with open(os.environ["GH_CALL_LOG"], "a") as log:
    log.write(json.dumps(args) + "\n")
prefix = "repos/fixture/luthier"
release = fixture["release"]
if args[:2] == ["release", "download"]:
    assert args[:5] == ["release", "download", "v0.1.0-beta2", "--repo", "fixture/luthier"]
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
    release["html_url"] = "https://github.com/fixture/luthier/releases/tag/v0.1.0-beta2"
    fixture["published"] = True
    path.write_text(json.dumps(fixture))
    print(json.dumps(release))
elif endpoint == "/git/ref/heads/release/v0.1.0-beta2":
    sha = fixture.get("candidate_sha", "1" * 40)
    if fixture.get("downloaded"):
        sha = fixture.get("candidate_after_download", sha)
    print(json.dumps({"ref": "refs/heads/release/v0.1.0-beta2", "object": {"type": "commit", "sha": sha}}))
elif endpoint == "/git/ref/tags/v0.1.0-beta2":
    print(json.dumps({"ref": "refs/tags/v0.1.0-beta2", "object": fixture.get("tag_object", {"type": "commit", "sha": "1" * 40})}))
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


class PublicationGuards(unittest.TestCase):
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
                        GITHUB_EVENT_NAME="push", GITHUB_REF=publisher.CANDIDATE,
                        GITHUB_SHA=SOURCE, QUALIFICATION_RESULT="success",
                        GITHUB_REPOSITORY=REPOSITORY, GITHUB_RUN_ID="1234",
                        GITHUB_SERVER_URL="https://github.com")
        for name in publisher.PAYLOADS:
            (self.local / name).write_bytes(("qualified " + name).encode())
        for name in publisher.SIDECARS:
            archive = name.removesuffix(".sha256")
            (self.local / name).write_text(self.checksum(archive), encoding="ascii")
        self.provenance = {"source_commit": SOURCE, "version": publisher.VERSION,
                           "platforms": ["windows", "macos"], "editions": ["pro", "free"],
                           "ci_run": f"https://github.com/{REPOSITORY}/actions/runs/1234"}
        (self.local / publisher.PROVENANCE).write_text(json.dumps(self.provenance))
        self.manifest()
        self.fixture = {
            "remote": str(self.remote),
            "release": {"id": 91, "tag_name": publisher.TAG, "target_commitish": SOURCE,
                        "name": "Luthier " + publisher.VERSION, "draft": True, "prerelease": True,
                        "url": f"https://api.github.com/repos/{REPOSITORY}/releases/91",
                        "html_url": f"https://github.com/{REPOSITORY}/releases/tag/untagged-fixture"},
        }
        self.sync_assets()

    def tearDown(self):
        self.temporary.cleanup()

    def checksum(self, name):
        return hashlib.sha256((self.local / name).read_bytes()).hexdigest() + "  " + name + "\n"

    def manifest(self):
        (self.local / publisher.MANIFEST).write_text("".join(
            self.checksum(name) for name in sorted(publisher.REQUIRED - {publisher.MANIFEST})))

    def sync_assets(self):
        self.fixture["assets"] = []
        for index, asset in enumerate(sorted(self.local.iterdir())):
            shutil.copy2(asset, self.remote / asset.name)
            self.fixture["assets"].append({"id": 100 + index, "name": asset.name, "state": "uploaded",
                                          "size": asset.stat().st_size,
                                          "digest": "sha256:" + hashlib.sha256(asset.read_bytes()).hexdigest()})

    def run_publisher(self, tag=publisher.TAG):
        self.fixture_path.write_text(json.dumps(self.fixture))
        return subprocess.run([sys.executable, str(SCRIPT), "--directory", str(self.local),
                               "--source", SOURCE, "--tag", tag],
                              env=self.env, capture_output=True, text=True)

    def calls(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def mutations(self):
        return [args for args in self.calls() if "--method" in args or args[:2] in
                (["release", "edit"], ["release", "create"], ["release", "upload"], ["release", "delete"])]

    def assert_refused(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.mutations(), [], result.stderr)

    def test_verified_draft_publishes_exact_numeric_id_once(self):
        result = self.run_publisher()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Published and verified prerelease", result.stdout)
        self.assertEqual(self.mutations(), [["api", "repos/fixture/luthier/releases/91", "--method", "PATCH",
                                             "-F", "draft=false", "-F", "prerelease=true"]])
        self.assertFalse(json.loads(self.fixture_path.read_text())["release"]["draft"])
        self.assertEqual(sum("/git/ref/heads/" in args[1] for args in self.calls() if args[0] == "api"), 3)

    def test_optional_signature_bytes_are_verified(self):
        (self.local / publisher.SIGNATURE).write_text("detached-signature-fixture\n")
        self.sync_assets()
        self.assertEqual(self.run_publisher().returncode, 0)

    def test_matching_annotated_tag_is_supported(self):
        self.fixture["tag_object"] = {"type": "tag", "sha": "2" * 40}
        self.assertEqual(self.run_publisher().returncode, 0)

    def test_dispatch_tag_push_and_other_candidates_do_not_publish(self):
        for changes, tag in [({"GITHUB_EVENT_NAME": "workflow_dispatch"}, publisher.TAG),
                             ({"GITHUB_REF": "refs/tags/" + publisher.TAG}, publisher.TAG),
                             ({"GITHUB_REF": "refs/heads/release/v0.1.0-beta3"}, publisher.TAG),
                             ({}, "v0.1.0-beta3")]:
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
        asset = self.local / next(iter(publisher.PAYLOADS))
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
        sidecar = next(iter(publisher.SIDECARS))
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
        asset = self.remote / next(iter(publisher.PAYLOADS))
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

    def test_workflow_keeps_full_qualification_and_narrows_publication(self):
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        self.assertIn("needs: [version, build]", workflow)
        self.assertIn("needs: [version, installer-checks]", workflow)
        self.assertIn("platforms: ${{ inputs.platforms || 'windows,macos' }}", workflow)
        self.assertIn("editions: ${{ inputs.editions || 'pro,free' }}", workflow)
        for gate in ("strictness: 10", "lto: true", "package: true", "no_clap: true"):
            self.assertIn(gate, workflow)
        self.assertIn("if: github.event_name == 'push' && github.ref == 'refs/heads/release/v0.1.0-beta2' && needs.version.outputs.tag == 'v0.1.0-beta2'", workflow)
        self.assertLess(workflow.index("- name: Create draft release"),
                        workflow.index("- name: Publish the authorized qualified beta2"))
        self.assertIn("QUALIFICATION_RESULT: ${{ needs.build.result }}", workflow)
        self.assertIn("-p test_publish_qualified_beta.py", workflow)
        self.assertNotIn("continue-on-error:", workflow)


if __name__ == "__main__":
    unittest.main()
