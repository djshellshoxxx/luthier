"""Execute release guards against a real scratch Git remote and simulated API statuses."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = Path(os.environ.get("LUTHIER_TEST_RELEASE_WORKFLOW", ROOT / ".github/workflows/release.yml"))


def step_script(name):
    lines = WORKFLOW.read_text().splitlines()
    start = next(i for i, line in enumerate(lines) if line.strip() == "name: " + name or line.strip() == "- name: " + name)
    run = next(i for i in range(start, len(lines)) if lines[i].strip() == "run: |")
    end = run + 1
    while end < len(lines) and (not lines[end].strip() or lines[end].startswith("          ")):
        end += 1
    return "\n".join(line[10:] for line in lines[run + 1:end]) + "\n"


class ReleaseGuards(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="luthier-release-guard-")
        self.root = Path(self.tmp.name)
        self.repo = self.root / "repo"
        self.remote = self.root / "remote.git"
        self.git("init", "--bare", str(self.remote), cwd=self.root)
        self.git("init", str(self.repo), cwd=self.root)
        self.git("remote", "add", "origin", str(self.remote))
        (self.repo / "CMakeLists.txt").write_text("project(Luthier VERSION 0.1.0 LANGUAGES CXX)\n")
        self.git("add", ".")
        self.git("commit", "-m", "Source A")
        self.source = self.git("rev-parse", "HEAD").strip()
        self.git("push", "origin", "HEAD:refs/heads/candidate")

    def tearDown(self):
        self.tmp.cleanup()

    def git(self, *args, cwd=None):
        return subprocess.check_output(
            ["git", "-c", "user.name=Release guard fixture", "-c", "user.email=fixture@example.invalid", *args],
            cwd=cwd or self.repo, stderr=subprocess.DEVNULL, text=True)

    def version(self, tag="v0.1.0-preview1"):
        output = self.root / "output"
        output.write_text("")
        env = dict(os.environ, TAG=tag, REF="candidate", GITHUB_OUTPUT=str(output))
        result = subprocess.run(["bash", "-e", "-c", step_script("Tag matches CMakeLists.txt")],
                                cwd=self.repo, env=env, capture_output=True, text=True)
        return result, dict(line.split("=", 1) for line in output.read_text().splitlines())

    def test_new_release_freezes_commit_even_when_branch_advances(self):
        result, output = self.version()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output["ref"], self.source)
        (self.repo / "later.txt").write_text("Later source\n")
        self.git("add", "."); self.git("commit", "-m", "Source B")
        self.git("push", "origin", "HEAD:refs/heads/candidate")
        self.assertNotEqual(self.git("rev-parse", "HEAD").strip(), output["ref"])

    def test_annotated_tag_at_same_source_is_accepted(self):
        self.git("tag", "-a", "v0.1.0-preview1", "-m", "Fixture")
        self.git("push", "origin", "refs/tags/v0.1.0-preview1")
        result, output = self.version()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output["ref"], self.source)

    def test_existing_tag_at_other_source_is_refused(self):
        self.git("tag", "v0.1.0-preview1"); self.git("push", "origin", "refs/tags/v0.1.0-preview1")
        (self.repo / "later.txt").write_text("Later source\n")
        self.git("add", "."); self.git("commit", "-m", "Source B")
        result, _ = self.version()
        self.assertNotEqual(result.returncode, 0)

    def test_legacy_short_beta_and_wrong_version(self):
        result, output = self.version("v0.1-beta")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output["version"], "0.1.0-beta")
        self.assertNotEqual(self.version("v1.2.3-preview1")[0].returncode, 0)

    def test_version_cannot_include_endpoint_path(self):
        self.assertNotEqual(self.version("v0.1.0-preview/other")[0].returncode, 0)

    def test_release_lookup_checks_drafts_and_fails_closed(self):
        bindir = self.root / "bin"; bindir.mkdir()
        stub = bindir / "gh"
        stub.write_text('#!/bin/bash\n[[ " $* " == *" --paginate "* ]] || exit 9\n[ "$STATUS" = 200 ] || exit 1\nprintf "%s\\n" "$TAGS"\n')
        stub.chmod(0o755)
        cases = [(200, "v0.1.0-old", True), (200, "v0.1.0-old\nv0.1.0-preview1", False),
                 (403, "", False), (404, "", False), (500, "", False)]
        for status, tags, accepted in cases:
            with self.subTest(status=status, tags=tags):
                env = dict(os.environ, PATH=str(bindir) + os.pathsep + os.environ["PATH"],
                           STATUS=str(status), TAGS=tags, RUNNER_TEMP=str(self.root),
                           GITHUB_REPOSITORY="fixture/repo", TAG="v0.1.0-preview1")
                result = subprocess.run(["bash", "-c", step_script("Refuse an existing release")],
                                        cwd=self.repo, env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode == 0, accepted, result.stderr)

    def test_publication_rechecks_tag_after_build(self):
        self.git("tag", "v0.1.0-preview1"); self.git("push", "origin", "refs/tags/v0.1.0-preview1")
        (self.repo / "later.txt").write_text("Later source\n")
        self.git("add", "."); self.git("commit", "-m", "Source B")
        env = dict(os.environ, SOURCE_SHA=self.git("rev-parse", "HEAD").strip(), TAG="v0.1.0-preview1")
        result = subprocess.run(["bash", "-c", step_script("Verify or create the frozen source tag")],
                                cwd=self.repo, env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.git("ls-remote", "--tags", "origin", "refs/tags/v0.1.0-preview1").split()[0], self.source)

    def test_create_failure_never_falls_back_to_update_or_upload(self):
        bindir = self.root / "bin"; bindir.mkdir()
        stub = bindir / "gh"; log = self.root / "gh-calls"
        stub.write_text('#!/bin/bash\nprintf "%s\\n" "$*" >> "$CALL_LOG"\nexit 1\n')
        stub.chmod(0o755)
        (self.repo / "release").mkdir(); (self.repo / "release/file.zip").write_text("fixture")
        env = dict(os.environ, PATH=str(bindir) + os.pathsep + os.environ["PATH"], CALL_LOG=str(log),
                   TAG="v0.1.0-preview1", VERSION="0.1.0-preview1", SOURCE_SHA=self.source)
        result = subprocess.run(["bash", "-c", step_script("Create draft release")],
                                cwd=self.repo, env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        calls = log.read_text().splitlines()
        self.assertEqual(len(calls), 1)
        self.assertTrue(calls[0].startswith("release create "))
        self.assertIn("--draft", calls[0]); self.assertIn("--verify-tag", calls[0])


if __name__ == "__main__":
    unittest.main()
