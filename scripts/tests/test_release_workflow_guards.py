"""Execute release guards against a real scratch Git remote and simulated API statuses."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = Path(os.environ.get("LUTHIER_TEST_RELEASE_WORKFLOW", ROOT / ".github/workflows/release.yml"))
BUILD_WORKFLOW = ROOT / ".github/workflows/build.yml"
BETA3_NOTES = "docs/beta/PRO-V01-BETA3-RELEASE-NOTES.md"
BETA3_REF = "refs/heads/claude/determined-ptolemy-mlbclp"
# The authorized beta3 dispatch, exactly as release.yml gates it.
BETA3_DISPATCH = ("github.event_name == 'workflow_dispatch' && github.ref == '" + BETA3_REF + "'"
                  " && inputs.tag == 'v0.1.0-beta3' && inputs.ref == 'claude/determined-ptolemy-mlbclp'")
HEAD_CHECK = "Require the authorized beta3 dispatch to build its dispatched commit"
# The pinned clap-validator 0.4.1-127-g152b982 release zips.
CLAP_VALIDATOR_SHA256 = {
    "ubuntu-22.04": "49edadcfb407ea0dd946ce418300e853fbd2660fa4b0d00e4f19ff8eef24ad90",
    "macos-universal": "bbec8cd7d18274e549d5d8c12ece3cec54be966129388dd2e742b9957f2ba9f1",
    "windows": "d935c3af0a45c3911ea2e900f4aa5d6709dac82bb485f0c4ce28648ab2cd0c10",
}


def load_workflow(path=None):
    import yaml  # PyYAML; imported here so the other guards still run and report without it
    workflow = yaml.safe_load((path or WORKFLOW).read_text())
    # YAML 1.1 reads the bare key "on" as true.
    workflow["on"] = workflow.pop(True, workflow.get("on"))
    return workflow


# A release.yml gate is a plain conjunction of equality tests. Anything else
# (||, !, functions, other operands) is refused rather than approximated.
GATE_TERM = re.compile(r"(github\.event_name|github\.ref|inputs\.tag|inputs\.ref|needs\.version\.outputs\.tag)"
                       r" == '([^']*)'")


def admits(gate, run):
    """Whether a run (context name -> value; absent means null) passes the gate.

    GitHub compares strings case-insensitively, so this does too; the publisher's
    own exact, case-sensitive checks are tested in test_publish_qualified_beta.py.
    """
    required = {}
    for term in gate.split(" && "):
        match = GATE_TERM.fullmatch(term)
        if match is None or match.group(1) in required:
            raise AssertionError(f"Unsupported release gate term: {term!r}")
        required[match.group(1)] = match.group(2)
    return all(isinstance(run.get(name), str) and run[name].lower() == value.lower()
               for name, value in required.items())


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
        # A release/v0.1.0-beta3 push still qualifies a draft; it never publishes.
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
        beta3 = load_publisher().AUTHORIZED[BETA3_REF]
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

    def test_beta3_dispatch_must_build_its_dispatched_commit(self):
        version = load_workflow()["jobs"]["version"]
        names = [step.get("name") for step in version["steps"]]
        check = version["steps"][names.index(HEAD_CHECK)]
        self.assertEqual(check["if"], BETA3_DISPATCH)
        self.assertEqual(check["env"], {"SOURCE_SHA": "${{ steps.v.outputs.ref }}"})
        # After the source is frozen, before anything is built, tagged or released.
        self.assertLess(names.index("Tag matches CMakeLists.txt"), names.index(HEAD_CHECK))
        self.assertLess(names.index(HEAD_CHECK), names.index("Refuse an existing release before building"))
        dispatched = self.source
        # The branch advances after the dispatch, before the job checks it out.
        (self.repo / "later.txt").write_text("Later source\n")
        self.git("add", "."); self.git("commit", "-m", "Source B")
        result, output = self.version("v0.1.0-beta3", "workflow_dispatch")
        self.assertEqual(result.returncode, 0, result.stderr)
        checked_out = output["ref"]
        self.assertNotEqual(checked_out, dispatched)
        script = step_script(HEAD_CHECK)

        def check_run(github_sha, frozen):
            env = dict(os.environ, GITHUB_SHA=github_sha, SOURCE_SHA=frozen)
            return subprocess.run(["bash", "-e", "-c", script], cwd=self.repo, env=env,
                                  capture_output=True, text=True)

        refused = check_run(dispatched, checked_out)  # HEAD != github.sha
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("not the dispatched commit " + dispatched, refused.stdout)
        self.assertNotEqual(check_run(checked_out, dispatched).returncode, 0)  # frozen output differs
        self.assertNotEqual(check_run(checked_out, "").returncode, 0)  # no frozen output
        accepted = check_run(checked_out, checked_out)
        self.assertEqual(accepted.returncode, 0, accepted.stdout + accepted.stderr)

    def test_publication_gates_admit_only_their_authorized_runs(self):
        workflow = load_workflow()
        steps = {step.get("name"): step for job in workflow["jobs"].values() for step in job.get("steps", [])}
        gates = {"beta2": steps["Publish the authorized qualified beta2"]["if"],
                 "beta3": steps["Publish the authorized qualified beta3"]["if"],
                 "consolidate": workflow["jobs"]["consolidate"]["if"],
                 "head check": steps[HEAD_CHECK]["if"]}
        self.assertEqual(gates["beta3"], BETA3_DISPATCH + " && needs.version.outputs.tag == 'v0.1.0-beta3'")
        dispatch = {"github.event_name": "workflow_dispatch", "github.ref": BETA3_REF, "inputs.tag": "v0.1.0-beta3",
                    "inputs.ref": "claude/determined-ptolemy-mlbclp", "needs.version.outputs.tag": "v0.1.0-beta3"}
        beta2_push = {"github.event_name": "push", "github.ref": "refs/heads/release/v0.1.0-beta2",
                      "needs.version.outputs.tag": "v0.1.0-beta2"}

        def push(ref, tag="v0.1.0-beta3"):
            return {"github.event_name": "push", "github.ref": ref, "needs.version.outputs.tag": tag}

        def retarget(**changes):
            return dict(dispatch, **{name.replace("_", "."): value for name, value in changes.items()})

        runs = {  # run -> the gates it passes (push runs have no inputs)
            "the authorized beta3 dispatch": (dispatch, {"beta3", "head check"}),
            "the beta2 candidate push": (beta2_push, {"beta2", "consolidate"}),
            "push of release/v0.1.0-beta3": (push("refs/heads/release/v0.1.0-beta3"), set()),
            "push of the dispatch branch": (push(BETA3_REF), set()),
            "push of tag v0.1.0-beta3": (push("refs/tags/v0.1.0-beta3"), set()),
            "dispatch from master": (retarget(github_ref="refs/heads/master"), set()),
            "dispatch from release/v0.1.0-beta3": (retarget(github_ref="refs/heads/release/v0.1.0-beta3"), set()),
            "dispatch from a near-miss branch": (retarget(github_ref=BETA3_REF + "-fix"), set()),
            "dispatch for v0.1.0-beta4": (dict(retarget(inputs_tag="v0.1.0-beta4"),
                                               **{"needs.version.outputs.tag": "v0.1.0-beta4"}), set()),
            "dispatch for v0.1.0-beta2": (dict(retarget(inputs_tag="v0.1.0-beta2"),
                                               **{"needs.version.outputs.tag": "v0.1.0-beta2"}), set()),
            "dispatch without inputs.ref": (retarget(inputs_ref=""), set()),
            "dispatch of the full ref name": (retarget(inputs_ref=BETA3_REF), set()),
            "dispatch of a commit": (retarget(inputs_ref="3a365fb7666cca86c34d6ee6987b229e890abf40"), set()),
            "dispatch of another branch": (retarget(inputs_ref="master"), set()),
            "dispatch of release/v0.1.0-beta3": (retarget(inputs_ref="release/v0.1.0-beta3"), set()),
            "dispatch of the beta2 branch": (dict(beta2_push, **{"github.event_name": "workflow_dispatch",
                                                                 "inputs.tag": "v0.1.0-beta2"}), set()),
        }
        for name, (run, admitted) in runs.items():
            with self.subTest(run=name):
                self.assertEqual({gate for gate, expression in gates.items() if admits(expression, run)}, admitted)
        # GitHub compares strings case-insensitively, so a differently cased
        # branch passes the step gate; the publisher's exact lookup refuses it.
        cased = retarget(github_ref=BETA3_REF.replace("claude/", "Claude/"))
        self.assertTrue(admits(gates["beta3"], cased))
        self.assertIsNone(load_publisher().AUTHORIZED.get(cased["github.ref"]))
        for unsupported in ("github.ref == 'x' || github.ref == 'y'", "!(github.ref == 'x')",
                            "startsWith(github.ref, 'refs/heads/release/')", "github.ref != 'x'"):
            with self.subTest(unsupported=unsupported):
                with self.assertRaises(AssertionError):
                    admits(unsupported, dispatch)

    def test_publication_steps_run_one_command_with_the_build_result(self):
        draft = load_workflow()["jobs"]["draft"]
        self.assertNotIn("continue-on-error", draft)
        publish = [step for step in draft["steps"]
                   if step.get("name", "").startswith("Publish the authorized qualified ")]
        self.assertEqual([step["name"] for step in publish],
                         ["Publish the authorized qualified beta2", "Publish the authorized qualified beta3"])
        # Nothing runs after publication that could mask or retry it.
        self.assertEqual(draft["steps"][-2:], publish)
        for step in publish:
            with self.subTest(step=step["name"]):
                # No continue-on-error, shell, working-directory or action: one run command.
                self.assertEqual(set(step), {"name", "if", "env", "run"})
                self.assertEqual(step["run"], 'python3 scripts/publish_qualified_beta.py --directory release '
                                              '--source "$SOURCE_SHA" --tag "$TAG"\n')
                command = step["run"].strip()
                self.assertEqual(command.count("python3"), 1)
                for token in ("||", "&&", ";", "|", "`", "$(", "\n", "for ", "while ", "until ", "retry"):
                    self.assertNotIn(token, command)
                self.assertEqual(step["env"]["QUALIFICATION_RESULT"], "${{ needs.build.result }}")
                self.assertEqual(step["env"], {"GH_TOKEN": "${{ github.token }}",
                                               "TAG": "${{ needs.version.outputs.tag }}",
                                               "SOURCE_SHA": "${{ needs.version.outputs.ref }}",
                                               "QUALIFICATION_RESULT": "${{ needs.build.result }}"})

    def test_only_the_draft_and_beta2_consolidation_jobs_write_contents(self):
        workflow = load_workflow()
        self.assertEqual(workflow["permissions"], {"contents": "read"})
        # Beta3 does not consolidate master: the beta2 job is the only consolidation.
        self.assertEqual(set(workflow["jobs"]), {"version", "installer-checks", "build", "draft", "consolidate"})
        self.assertEqual(workflow["jobs"]["consolidate"]["if"],
                         "github.event_name == 'push' && github.ref == 'refs/heads/release/v0.1.0-beta2'"
                         " && needs.version.outputs.tag == 'v0.1.0-beta2'")
        writers = {}
        for name, job in workflow["jobs"].items():
            permissions = job.get("permissions", {})
            self.assertIsInstance(permissions, dict, name)  # no write-all shorthand
            granted = {scope for scope, level in permissions.items() if level == "write"}
            if granted:
                writers[name] = granted
        self.assertEqual(writers, {"draft": {"contents"}, "consolidate": {"contents"}})

    def test_beta2_consolidation_job_is_unchanged(self):
        # master (a2e6fda8) content: scripts/consolidate_qualified_beta.py takes only --source.
        self.assertEqual(load_workflow()["jobs"]["consolidate"], {
            "name": "Consolidate the published beta2 source",
            "needs": ["version", "draft"],
            "if": "github.event_name == 'push' && github.ref == 'refs/heads/release/v0.1.0-beta2'"
                  " && needs.version.outputs.tag == 'v0.1.0-beta2'",
            "runs-on": "ubuntu-latest",
            "timeout-minutes": 10,
            "permissions": {"contents": "write"},
            "steps": [
                {"uses": "actions/checkout@v4",
                 "with": {"ref": "${{ needs.version.outputs.ref }}", "persist-credentials": False}},
                {"name": "Fast-forward unchanged master to the verified release source",
                 "env": {"GH_TOKEN": "${{ github.token }}", "SOURCE_SHA": "${{ needs.version.outputs.ref }}",
                         "PUBLICATION_RESULT": "${{ needs.draft.result }}"},
                 "run": 'python3 scripts/consolidate_qualified_beta.py --source "$SOURCE_SHA"\n'},
            ],
        })

    def test_clap_validator_download_is_verified_before_it_is_unpacked(self):
        unix = (ROOT / "scripts/ci_build.sh").read_text()
        windows = (ROOT / "scripts/ci_build.ps1").read_text()
        self.assertIn(f'CLAP_VALIDATOR_SHA256_LINUX="{CLAP_VALIDATOR_SHA256["ubuntu-22.04"]}"', unix)
        self.assertIn(f'CLAP_VALIDATOR_SHA256_MACOS="{CLAP_VALIDATOR_SHA256["macos-universal"]}"', unix)
        self.assertIn(f"$ClapValidatorWindowsSha256 = '{CLAP_VALIDATOR_SHA256['windows']}'", windows)
        fetch = re.search(r"^fetch_clap_validator\(\) \{\n.*?^\}\n", unix, re.M | re.S).group(0)
        self.assertIn('sha256sum "$TOOLS_DIR/$archive"', fetch)
        self.assertIn('shasum -a 256 "$TOOLS_DIR/$archive"', fetch)
        self.assertLess(fetch.index("return 1"), fetch.index("unzip"))
        get = re.search(r"^function Get-ClapValidator \{\n.*?^\}\n", windows, re.M | re.S).group(0)
        self.assertLess(get.index("Get-FileHash -Algorithm SHA256 -LiteralPath $zip"), get.index("Expand-Archive"))
        self.assertLess(get.index("throw"), get.index("Expand-Archive"))
        # Both keep the cached-binary fast path.
        self.assertIn('if [ ! -x "$exe" ]; then', fetch)
        self.assertIn("if (-not (Test-Path $exe)) {", get)

        # Run the real shell function on a synthetic zip shaped like the release
        # asset (a .tar.gz holding the binary), with curl and unzip stubbed.
        payload = self.root / "clap-validator"
        payload.write_text("#!/bin/sh\nexit 0\n")
        tarball = self.root / "clap-validator-fixture.tar.gz"
        with tarfile.open(tarball, "w:gz") as archive:
            archive.add(payload, arcname="clap-validator")
        fixture = self.root / "fixture.zip"
        with zipfile.ZipFile(fixture, "w") as archive:
            archive.write(tarball, tarball.name)
        digest = hashlib.sha256(fixture.read_bytes()).hexdigest()
        bindir = self.root / "bin"; bindir.mkdir()
        (bindir / "curl").write_text(
            '#!/bin/bash\nout=""\nwhile [ $# -gt 0 ]; do\n'
            '  case "$1" in -o) out="$2"; shift 2 ;; -*) shift ;; *) echo "curl $1" >> "$CALLS"; shift ;; esac\n'
            'done\ncp "$ZIP" "$out"\n')
        (bindir / "unzip").write_text(
            '#!/bin/bash\necho unzip >> "$CALLS"\n'
            'exec python3 -c "import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])" "$2" "$4"\n')
        for tool in bindir.iterdir():
            tool.chmod(0o755)
        url = "https://github.com/free-audio/clap-validator/releases/download/0.4.1/clap-validator-0.4.1-127-g152b982-"

        def fetch_run(platform, label, linux, macos):
            tools, calls = self.root / label, self.root / (label + ".calls")
            calls.write_text("")
            program = "\n".join([
                "set -euo pipefail", f"PLATFORM={platform}", f"TOOLS_DIR={shlex.quote(str(tools))}",
                "CLAP_VALIDATOR_VERSION=0.4.1", "CLAP_VALIDATOR_ASSET_VERSION=0.4.1-127-g152b982",
                f"CLAP_VALIDATOR_SHA256_LINUX={linux}", f"CLAP_VALIDATOR_SHA256_MACOS={macos}",
                fetch, 'exe="$(fetch_clap_validator)"', 'printf "%s\\n" "$exe"', ""])
            env = dict(os.environ, PATH=str(bindir) + os.pathsep + os.environ["PATH"],
                       CALLS=str(calls), ZIP=str(fixture))
            result = subprocess.run(["bash", "-c", program], env=env, capture_output=True, text=True)
            return result, calls.read_text().splitlines(), tools

        pinned = (CLAP_VALIDATOR_SHA256["ubuntu-22.04"], CLAP_VALIDATOR_SHA256["macos-universal"])
        for platform, asset, tool in (("linux", "ubuntu-22.04", "sha256sum"), ("macos", "macos-universal", "shasum")):
            with self.subTest(platform=platform):
                if shutil.which(tool) is None:
                    self.skipTest(tool + " not installed")
                # Bytes that are not the pinned zip are refused before anything is unpacked.
                result, calls, tools = fetch_run(platform, platform + "-other", *pinned)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("refusing to unpack", result.stderr)
                self.assertEqual(calls, ["curl " + url + asset + ".zip"])
                self.assertEqual(list(tools.iterdir()), [])
                # Bytes that match the pinned digest are unpacked and the binary is found.
                matching = (digest, pinned[1]) if platform == "linux" else (pinned[0], digest)
                result, calls, tools = fetch_run(platform, platform + "-pinned", *matching)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(calls, ["curl " + url + asset + ".zip", "unzip"])
                self.assertEqual(result.stdout.strip(), str(tools / "clap-validator"))
                # A cached executable is reused without another download.
                result, calls, _ = fetch_run(platform, platform + "-pinned", *pinned)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(calls, [])

    def test_every_needs_reference_resolves_in_the_parsed_workflows(self):
        for path in (WORKFLOW, BUILD_WORKFLOW):
            with self.subTest(workflow=path.name):
                jobs = load_workflow(path)["jobs"]
                for name, job in jobs.items():
                    needs = job.get("needs", [])
                    needs = [needs] if isinstance(needs, str) else needs
                    self.assertLessEqual(set(needs), set(jobs) - {name}, name)
                    used = set(re.findall(r"\bneeds\.([A-Za-z0-9_-]+)\.", json.dumps(job)))
                    self.assertLessEqual(used, set(needs), name)

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
