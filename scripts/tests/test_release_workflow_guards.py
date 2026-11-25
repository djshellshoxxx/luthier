"""Execute release guards against a real scratch Git remote and simulated API statuses."""
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = Path(os.environ.get("LUTHIER_TEST_RELEASE_WORKFLOW", ROOT / ".github/workflows/release.yml"))
BUILD_WORKFLOW = ROOT / ".github/workflows/build.yml"
BETA3_NOTES = "docs/beta/PRO-V01-BETA3-RELEASE-NOTES.md"


def load_publisher():
    module = sys.modules.get("publish_qualified_beta")
    if module is None:
        spec = importlib.util.spec_from_file_location("publish_qualified_beta",
                                                      ROOT / "scripts/publish_qualified_beta.py")
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
    return module


def step_script(name, workflow=None):
    lines = (workflow or WORKFLOW).read_text().splitlines()
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

    def version(self, tag="v0.1.0-preview1", event="workflow_dispatch", ref_type="branch"):
        output = self.root / "output"
        output.write_text("")
        env = dict(os.environ, TAG=tag, REF="candidate", GITHUB_OUTPUT=str(output),
                   GITHUB_EVENT_NAME=event, GITHUB_REF_TYPE=ref_type)
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

    def test_candidate_branch_name_selects_version_and_freezes_source(self):
        result, output = self.version("release/v0.1.0-beta2", "push", "branch")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output, {"version": "0.1.0-beta2", "tag": "v0.1.0-beta2", "ref": self.source})
        self.assertNotEqual(self.version("release/v0.1.0-beta2", "push", "tag")[0].returncode, 0)

    def test_windows_release_build_pins_visual_studio_2022_runner(self):
        build = BUILD_WORKFLOW.read_text()
        self.assertIn('add "Windows x64 / Pro" windows-2022 windows PAID', build)
        self.assertIn('add "Windows x64 / Free" windows-2022 windows FREE', build)
        self.assertNotIn('add "Windows x64 / Pro" windows-latest', build)
        self.assertNotIn('add "Windows x64 / Free" windows-latest', build)
        release = WORKFLOW.read_text()
        self.assertIn("strictness: 10", release)
        self.assertIn("lto: true", release)

    def test_windows_release_lto_is_partitioned_without_disabling_ltcg(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn("if (MSVC AND LUTHIER_LTO)", cmake)
        self.assertIn("add_library(luthier_windows_lto_partition STATIC", cmake)
        self.assertIn("list(REMOVE_ITEM LUTHIER_SOURCES ${LUTHIER_WINDOWS_LTO_PARTITION_SOURCES})", cmake)
        for source in ("Guitar/GuitarRenderer.cpp", "Guitar/StringMotion.cpp",
                       "ReferencePedals/PedalView.cpp", "Faces/AmpFace.cpp",
                       "Faces/FaceMaterials.cpp", "Faces/KnobCaps.cpp", "Faces/PedalFace.cpp",
                       "Search/InlineValue.cpp", "Search/SearchCatalog.cpp", "Search/SearchIndex.cpp",
                       "Search/SearchMatcher.cpp", "AnimationPolicy.cpp", "Theme.cpp", "TunePianoRoll.cpp"):
            self.assertIn(source, cmake)
        self.assertIn("juce::juce_recommended_lto_flags", cmake)
        self.assertIn("target_link_libraries(Luthier PUBLIC luthier_windows_lto_partition)", cmake)
        self.assertIn("list(APPEND LUTHIER_UI_SOURCES ${LUTHIER_WINDOWS_LTO_PARTITION_SOURCES})", cmake)
        self.assertNotIn("LUTHIER_LTO=OFF", (ROOT / ".github/workflows/release.yml").read_text())

    def test_beta_release_scope_is_linux_windows_and_macos_with_clap(self):
        release = (ROOT / ".github/workflows/release.yml").read_text()
        build = (ROOT / ".github/workflows/build.yml").read_text()
        # Owner-approved beta3 scope: Linux joins and CLAP is built, validated and packaged.
        self.assertIn("platforms: ${{ inputs.platforms || 'linux,windows,macos' }}", release)
        self.assertIn("no_clap: false", release)
        self.assertNotIn("no_clap: true", release)
        self.assertIn("default: 'linux,windows,macos'", release)
        self.assertIn("test_shards: 4", release)
        # Sharded runs must still execute the suite: the build job hands its
        # binaries to a test job that passes --shard through TEST_SHARD.
        self.assertIn("TEST_SHARD: ${{ matrix.shard }}", build)
        self.assertIn("needs.plan.outputs.test_matrix", build)
        for script in ("scripts/ci_build.sh", "scripts/ci_build.ps1"):
            text = (ROOT / script).read_text()
            self.assertIn("--shard=", text)
            self.assertIn("LUTHIER_NO_CLAP", text)

    def test_sharded_test_artifacts_survive_leaf_job_reruns(self):
        build = BUILD_WORKFLOW.read_text()
        stable_name = "name: test-binaries-${{ matrix.edition }}-${{ matrix.platform }}-${{ github.run_id }}"
        self.assertEqual(build.count(stable_name), 2)
        self.assertNotIn("github.run_attempt", build)
        self.assertIn("overwrite: true", build)

    def test_clap_validator_is_current_on_all_release_platforms(self):
        unix = (ROOT / "scripts/ci_build.sh").read_text()
        windows = (ROOT / "scripts/ci_build.ps1").read_text()
        for script in (unix, windows):
            self.assertIn("0.4.1", script)
            self.assertIn("0.4.1-127-g152b982", script)
            self.assertNotIn("0.3.2", script)
        self.assertIn("ubuntu-22.04", unix)
        self.assertIn("macos-universal", unix)
        self.assertIn("unzip -oq", unix)
        self.assertIn('$ClapValidatorAssetVersion-windows.zip', windows)

    def test_advanced_candidate_refuses_tag_creation(self):
        bindir = self.root / "bin"; bindir.mkdir()
        stub = bindir / "gh"; log = self.root / "gh-calls"
        stub.write_text('#!/bin/bash\nprintf "%s\\n" "$*" >> "$CALL_LOG"\n'
                        '[[ "$*" == *"git/ref/heads/release/v0.1.0-beta2"* ]] || exit 9\n'
                        'printf "%s\\n" "$CANDIDATE_SHA"\n')
        stub.chmod(0o755)
        env = dict(os.environ, PATH=str(bindir) + os.pathsep + os.environ["PATH"], CALL_LOG=str(log),
                   GITHUB_EVENT_NAME="push", GITHUB_REF_TYPE="branch", GITHUB_REF="refs/heads/release/v0.1.0-beta2",
                   GITHUB_REPOSITORY="fixture/repo", SOURCE_SHA=self.source, TAG="v0.1.0-beta2",
                   CANDIDATE_SHA="0" * 40)
        result = subprocess.run(["bash", "-c", step_script("Verify or create the frozen source tag")],
                                cwd=self.repo, env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Candidate branch advanced", result.stdout)
        self.assertEqual(len(log.read_text().splitlines()), 1)
        self.assertEqual(self.git("ls-remote", "--tags", "origin"), "")

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
                 (200, "v0.1.0-preview1\n" + "v0.1.0-other\n" * 10000, False),
                 (403, "", False), (404, "", False), (500, "", False)]
        for status, tags, accepted in cases:
            with self.subTest(status=status, tag_lines=len(tags.splitlines())):
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
        env = dict(os.environ, SOURCE_SHA=self.git("rev-parse", "HEAD").strip(), TAG="v0.1.0-preview1",
                   GITHUB_EVENT_NAME="workflow_dispatch", GITHUB_REF_TYPE="branch")
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

    def test_candidate_branch_name_selects_beta3_version_and_freezes_source(self):
        result, output = self.version("release/v0.1.0-beta3", "push", "branch")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output, {"version": "0.1.0-beta3", "tag": "v0.1.0-beta3", "ref": self.source})
        self.assertNotEqual(self.version("release/v0.1.0-beta3", "push", "tag")[0].returncode, 0)

    def test_advanced_beta3_candidate_refuses_tag_creation(self):
        bindir = self.root / "bin"; bindir.mkdir()
        stub = bindir / "gh"; log = self.root / "gh-calls"
        stub.write_text('#!/bin/bash\nprintf "%s\\n" "$*" >> "$CALL_LOG"\n'
                        '[[ "$*" == *"git/ref/heads/release/v0.1.0-beta3"* ]] || exit 9\n'
                        'printf "%s\\n" "$CANDIDATE_SHA"\n')
        stub.chmod(0o755)
        env = dict(os.environ, PATH=str(bindir) + os.pathsep + os.environ["PATH"], CALL_LOG=str(log),
                   GITHUB_EVENT_NAME="push", GITHUB_REF_TYPE="branch", GITHUB_REF="refs/heads/release/v0.1.0-beta3",
                   GITHUB_REPOSITORY="fixture/repo", SOURCE_SHA=self.source, TAG="v0.1.0-beta3",
                   CANDIDATE_SHA="0" * 40)
        result = subprocess.run(["bash", "-c", step_script("Verify or create the frozen source tag")],
                                cwd=self.repo, env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Candidate branch advanced", result.stdout)
        self.assertEqual(len(log.read_text().splitlines()), 1)
        self.assertEqual(self.git("ls-remote", "--tags", "origin"), "")

    def test_recorded_provenance_platforms_are_the_built_platforms(self):
        release = WORKFLOW.read_text()
        recorded = re.search(r"PLATFORMS: \$\{\{ inputs\.platforms \|\| '([a-z,]+)' \}\}", release).group(1)
        built = re.search(r"platforms: \$\{\{ inputs\.platforms \|\| '([a-z,]+)' \}\}", release).group(1)
        self.assertEqual(recorded, built)
        self.assertIn(f"default: '{recorded}'", release)
        (self.repo / "release").mkdir()
        env = dict(os.environ, SOURCE_SHA=self.source, RELEASE_VERSION="0.1.0-beta3", PLATFORMS=recorded,
                   EDITIONS="pro,free", GITHUB_SERVER_URL="https://github.com",
                   GITHUB_REPOSITORY="fixture/repo", GITHUB_RUN_ID="77")
        result = subprocess.run(["bash", "-e", "-c", step_script("Record source and qualification provenance")],
                                cwd=self.repo, env=env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        provenance = json.loads((self.repo / "release/BUILD-PROVENANCE.json").read_text())
        self.assertEqual(provenance["platforms"], ["linux", "windows", "macos"])
        self.assertEqual(provenance["editions"], ["pro", "free"])
        beta3 = load_publisher().AUTHORIZED["refs/heads/release/v0.1.0-beta3"]
        self.assertEqual(provenance["platforms"], list(beta3.platforms))
        self.assertEqual(provenance["version"], beta3.version)

    def test_draft_notes_follow_the_candidate_version(self):
        bindir = self.root / "bin"; bindir.mkdir()
        stub = bindir / "gh"; log = self.root / "gh-calls"
        stub.write_text('#!/bin/bash\nprintf "%s\\n" "$*" >> "$CALL_LOG"\n')
        stub.chmod(0o755)
        (self.repo / "release").mkdir(); (self.repo / "release/file.zip").write_text("fixture")
        for version, notes in (("0.1.0-beta3", "--notes-file " + BETA3_NOTES),
                               ("0.1.0-beta2", "--notes-file docs/beta/PRO-V01-BETA2-RELEASE-NOTES.md"),
                               ("0.1.0-preview1", "--generate-notes")):
            with self.subTest(version=version):
                log.unlink(missing_ok=True)
                env = dict(os.environ, PATH=str(bindir) + os.pathsep + os.environ["PATH"], CALL_LOG=str(log),
                           TAG="v" + version, VERSION=version, SOURCE_SHA=self.source)
                result = subprocess.run(["bash", "-c", step_script("Create draft release")],
                                        cwd=self.repo, env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                calls = log.read_text().splitlines()
                self.assertEqual(len(calls), 1)
                self.assertTrue(calls[0].startswith(f"release create v{version} --verify-tag "))
                self.assertIn(notes, calls[0])
                self.assertIn("--draft", calls[0]); self.assertIn("--prerelease", calls[0])
        self.assertTrue((ROOT / BETA3_NOTES).read_text().strip())

    def test_beta3_packages_carry_the_completed_beta3_notes(self):
        build = BUILD_WORKFLOW.read_text()
        self.assertIn("BETA_README: ${{ inputs.version == '0.1.0-beta3' && '" + BETA3_NOTES + "' || "
                      "(inputs.version == '0.1.0-beta2' && 'docs/beta/PRO-V01-BETA2-RELEASE-NOTES.md' || ", build)
        self.assertIn("if ($env:VERSION -eq '0.1.0-beta3') {\n"
                      "            scripts/package_windows.ps1 -BetaReadme " + BETA3_NOTES, build)
        self.assertIn("matrix.edition == 'PAID' && (inputs.version == '0.1.0-beta' || "
                      "inputs.version == '0.1.0-beta2' || inputs.version == '0.1.0-beta3')", build)
        self.assertIn('if [ "${{ inputs.version }}" = "0.1.0-beta3" ]; then\n'
                      "            beta_readme=" + BETA3_NOTES, build)
        # The Windows portable package rejects notes with template fields (the
        # Linux packager's identical gate runs in test_publish_qualified_beta).
        windows = (ROOT / "scripts/package_windows.ps1").read_text()
        gate = re.search(r"\$betaReadmeText -match '([^']+)'", windows).group(1)
        self.assertRegex("Owner: [name]", re.compile(gate, re.IGNORECASE))
        for notes in (BETA3_NOTES, "docs/beta/PRO-V01-BETA2-RELEASE-NOTES.md"):
            with self.subTest(notes=notes):
                self.assertIsNone(re.search(gate, (ROOT / notes).read_text(), re.IGNORECASE))

    def test_release_packaging_requires_the_validated_clap(self):
        lines = BUILD_WORKFLOW.read_text().splitlines()
        index = {line.strip(): i for i, line in enumerate(lines) if line.strip().startswith("- name: ")}
        guard = index["- name: Require the validated CLAP plug-in"]
        self.assertEqual(lines[guard + 1].strip(), "if: inputs.package && !inputs.no_clap")
        self.assertLess(index["- name: Stage products (Unix)"], guard)
        self.assertLess(index["- name: Stage products (Windows)"], guard)
        for package in ("Linux", "macOS", "Windows"):
            self.assertLess(guard, index[f"- name: Package ({package})"])
        script = step_script("Require the validated CLAP plug-in", BUILD_WORKFLOW)
        work = self.root / "ci"
        for edition, product in (("PAID", "Luthier Pro"), ("FREE", "Luthier Free")):
            for platform in ("linux", "windows", "macos"):
                with self.subTest(edition=edition, platform=platform):
                    stage = work / "dist" / platform
                    logs = work / "build-ci/logs"
                    stage.mkdir(parents=True, exist_ok=True); logs.mkdir(parents=True, exist_ok=True)
                    env = dict(os.environ, LUTHIER_EDITION=edition, DIST_DIR="dist", BUILD_DIR="build-ci")
                    run = lambda: subprocess.run(["bash", "-c", script.replace("${{ matrix.platform }}", platform)],
                                                 cwd=work, env=env, capture_output=True, text=True)
                    self.assertNotEqual(run().returncode, 0)  # nothing staged
                    (stage / f"{product}.clap").write_text("clap")
                    result = run()
                    self.assertNotEqual(result.returncode, 0)  # staged but never validated
                    self.assertIn("clap-validator did not run", result.stdout)
                    (logs / "clap-validator.log").write_text("All tests passed\n")
                    result = run()
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    (stage / f"{product}.clap").unlink(); (logs / "clap-validator.log").unlink()

    def test_every_needs_reference_resolves(self):
        for path in (WORKFLOW, BUILD_WORKFLOW):
            with self.subTest(workflow=path.name):
                lines = path.read_text().splitlines()
                body = lines[lines.index("jobs:") + 1:]
                starts = [i for i, line in enumerate(body) if re.fullmatch(r"  [A-Za-z0-9_-]+:", line)]
                jobs = {body[i].strip()[:-1]: body[i:j] for i, j in zip(starts, starts[1:] + [len(body)])}
                self.assertTrue(jobs)
                for job, block in jobs.items():
                    declared = set()
                    for line in block:
                        match = re.fullmatch(r"    needs: (.+)", line)
                        if match:
                            declared |= {name.strip() for name in match.group(1).strip("[]").split(",")}
                    used = set(re.findall(r"\bneeds\.([A-Za-z0-9_-]+)\.", "\n".join(block)))
                    self.assertLessEqual(declared, set(jobs) - {job}, job)
                    self.assertLessEqual(used, declared, job)


if __name__ == "__main__":
    unittest.main()
