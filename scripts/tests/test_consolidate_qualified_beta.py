"""Check that default-branch consolidation preserves concurrent source work."""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location("consolidate_qualified_beta", SCRIPTS / "consolidate_qualified_beta.py")
consolidator = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = consolidator  # dataclasses resolve annotations through it
spec.loader.exec_module(consolidator)
SOURCE = "1" * 40
OTHER = "2" * 40
BETA2 = consolidator.CONSOLIDATIONS["refs/heads/release/v0.1.0-beta2"]
BETA3 = consolidator.CONSOLIDATIONS["refs/heads/release/v0.1.0-beta3"]


def expected_query(mutation_id):
    """The exact atomic, non-forced master update each candidate sends."""
    return ("mutation($repositoryId: ID!, $before: GitObjectID!, $after: GitObjectID!) {\n"
            "  updateRefs(input: {\n"
            f'    repositoryId: $repositoryId, clientMutationId: "{mutation_id}",\n'
            '    refUpdates: [{name: "refs/heads/master", beforeOid: $before, afterOid: $after, force: false}]\n'
            "  }) { clientMutationId }\n"
            "}")


class FixtureGitHub:
    def __init__(self, entry):
        self.entry = entry
        self.master = entry.expected_master
        self.release = {"tag_name": entry.candidate.tag, "draft": False,
                        "prerelease": True, "target_commitish": SOURCE}
        self.comparison = {"status": "ahead", "behind_by": 0,
                           "merge_base_commit": {"sha": self.master}}
        self.master_reads = 0
        self.source_checks = 0
        self.mutations = []
        self.advance_master = False
        self.advance_candidate = False
        self.reject_update = False
        self.ignore_update = False
        self.advance_before_mutation = False
        self.graphql_error = False

    def check_source(self, source):
        assert source == SOURCE
        self.source_checks += 1
        if self.advance_candidate and self.source_checks == 2:
            raise consolidator.PublicationError("candidate advanced")

    def api(self, endpoint, *args):
        if endpoint == "":
            return {"full_name": "djshellshoxxx/luthier", "node_id": "fixture-repository"}
        if endpoint == "/releases/tags/" + self.entry.candidate.tag:
            return self.release
        if endpoint == "/git/ref/heads/master":
            self.master_reads += 1
            if self.advance_master and self.master_reads == 2:
                self.master = OTHER
            return {"ref": "refs/heads/master", "object": {"type": "commit", "sha": self.master}}
        if endpoint == f"/compare/{self.entry.expected_master}...{SOURCE}":
            return self.comparison
        raise AssertionError("unexpected API endpoint " + endpoint)

    def run(self, *args):
        assert args[:2] == ("api", "graphql")
        assert all(value == "-f" for value in args[2::2])
        fields = dict(value.split("=", 1) for value in args[3::2])
        assert fields == {"query": expected_query(self.entry.mutation_id), "repositoryId": "fixture-repository",
                          "before": self.entry.expected_master, "after": SOURCE}
        assert 'name: "refs/heads/master", beforeOid: $before, afterOid: $after, force: false' in fields["query"]
        self.mutations.append(args)
        if self.advance_before_mutation:
            self.master = OTHER  # Another ancestor is still an intervening edit.
        if self.reject_update or self.master != fields["before"]:
            raise subprocess.CalledProcessError(1, ["gh", "api", "graphql"])
        if self.graphql_error:
            return json.dumps({"errors": [{"message": "rejected"}], "data": {"updateRefs": None}})
        if not self.ignore_update:
            self.master = SOURCE
        return json.dumps({"data": {"updateRefs": {"clientMutationId": self.entry.mutation_id}}})


class ConsolidationGuards:
    """Checks every authorized candidate keeps; each subclass names one entry."""

    entry: consolidator.Consolidation

    def setUp(self):
        self.api = FixtureGitHub(self.entry)
        self.tag = self.entry.candidate.tag
        self.env = {"GITHUB_EVENT_NAME": "push", "GITHUB_REF": self.entry.candidate.ref,
                    "GITHUB_SHA": SOURCE, "PUBLICATION_RESULT": "success",
                    "GITHUB_REPOSITORY": "djshellshoxxx/luthier", "GITHUB_SERVER_URL": "https://github.com"}

    def run_consolidation(self):
        with patch.dict(os.environ, self.env, clear=True), \
                patch.object(consolidator, "GitHub", return_value=self.api) as client:
            try:
                consolidator.consolidate(SOURCE, self.tag)
            finally:
                # Any client is bound to exactly this candidate.
                for call in client.call_args_list:
                    self.assertEqual(call.args, ("djshellshoxxx/luthier", self.entry.candidate))

    def refuse_without_mutation(self):
        with self.assertRaises(consolidator.PublicationError):
            self.run_consolidation()
        self.assertEqual(self.api.mutations, [])

    def test_fast_forwards_only_verified_source_without_force(self):
        self.run_consolidation()
        self.assertEqual(self.api.master, SOURCE)
        self.assertEqual(len(self.api.mutations), 1)
        self.assertEqual(self.api.source_checks, 2)
        self.assertEqual(self.api.master_reads, 3)

    def test_already_consolidated_source_is_read_only(self):
        self.api.master = SOURCE
        self.run_consolidation()
        self.assertEqual(self.api.mutations, [])

    def test_unqualified_or_unrelated_runs_are_read_only(self):
        for key, value in (("GITHUB_EVENT_NAME", "workflow_dispatch"), ("GITHUB_REF", "refs/heads/master"),
                           ("GITHUB_SHA", OTHER), ("PUBLICATION_RESULT", "failure"),
                           ("GITHUB_REPOSITORY", "other/repo"), ("GITHUB_SERVER_URL", "https://example.invalid")):
            with self.subTest(key=key):
                original = self.env[key]
                self.env[key] = value
                self.refuse_without_mutation()
                self.env[key] = original
        self.assertEqual(self.api.source_checks, 0)

    def test_other_candidates_refs_and_tags_are_read_only(self):
        other = BETA3 if self.entry is BETA2 else BETA2
        for ref, tag in ((other.candidate.ref, self.tag), (self.entry.candidate.ref, other.candidate.tag),
                         ("refs/tags/" + self.tag, self.tag), ("refs/heads/release/v0.1.0-beta4", "v0.1.0-beta4"),
                         (self.entry.candidate.ref + "-fix", self.tag), (self.entry.candidate.ref, self.tag + " ")):
            with self.subTest(ref=ref, tag=tag):
                self.env["GITHUB_REF"], self.tag = ref, tag
                self.refuse_without_mutation()
        self.assertEqual(self.api.source_checks, 0)

    def test_unpublished_or_different_release_is_read_only(self):
        for key, value in (("draft", True), ("prerelease", False), ("target_commitish", OTHER), ("tag_name", "v0.2.0")):
            with self.subTest(key=key):
                original = self.api.release[key]
                self.api.release[key] = value
                self.refuse_without_mutation()
                self.api.release[key] = original

    def test_changed_master_is_preserved(self):
        self.api.master = OTHER
        self.refuse_without_mutation()
        self.assertEqual(self.api.master, OTHER)

    def test_divergent_or_incomplete_ancestry_is_refused(self):
        for comparison in ({"status": "diverged", "behind_by": 1, "merge_base_commit": {"sha": self.entry.expected_master}},
                           {"status": "ahead", "behind_by": 0, "merge_base_commit": {"sha": OTHER}}):
            self.api.comparison = comparison
            self.refuse_without_mutation()

    def test_master_change_during_checks_is_preserved(self):
        self.api.advance_master = True
        self.refuse_without_mutation()
        self.assertEqual(self.api.master, OTHER)

    def test_candidate_change_during_checks_stops_mutation(self):
        self.api.advance_candidate = True
        self.refuse_without_mutation()

    def test_server_rejection_never_retries_or_forces_update(self):
        self.api.reject_update = True
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_consolidation()
        self.assertEqual(len(self.api.mutations), 1)
        self.assertEqual(self.api.master, self.entry.expected_master)

    def test_unconfirmed_update_is_reported_without_retry(self):
        self.api.ignore_update = True
        with self.assertRaises(consolidator.PublicationError):
            self.run_consolidation()
        self.assertEqual(len(self.api.mutations), 1)

    def test_atomic_expected_head_rejects_a_last_moment_ancestor_move(self):
        self.api.advance_before_mutation = True
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_consolidation()
        self.assertEqual(len(self.api.mutations), 1)
        self.assertEqual(self.api.master, OTHER)

    def test_graphql_error_is_not_treated_as_a_successful_update(self):
        self.api.graphql_error = True
        with self.assertRaises(consolidator.PublicationError):
            self.run_consolidation()
        self.assertEqual(len(self.api.mutations), 1)
        self.assertEqual(self.api.master, self.entry.expected_master)


class Beta2ConsolidationGuards(ConsolidationGuards, unittest.TestCase):
    entry = BETA2

    def test_beta2_entry_is_unchanged(self):
        self.assertEqual(self.entry.candidate.ref, "refs/heads/release/v0.1.0-beta2")
        self.assertEqual(self.entry.candidate.tag, "v0.1.0-beta2")
        self.assertEqual(self.entry.expected_master, "0f62c3d67bdba055b41e66c7ab8ac9ab4b728662")
        self.assertEqual(self.entry.mutation_id, "luthier-beta2-consolidation")
        self.assertEqual(self.entry.query, expected_query("luthier-beta2-consolidation"))


class Beta3ConsolidationGuards(ConsolidationGuards, unittest.TestCase):
    entry = BETA3

    def test_beta3_expects_the_published_beta2_source_as_master(self):
        self.assertEqual(self.entry.candidate.ref, "refs/heads/release/v0.1.0-beta3")
        self.assertEqual(self.entry.candidate.tag, "v0.1.0-beta3")
        self.assertEqual(self.entry.expected_master, "a2e6fda8877b93923d2e902ee28dd10fd0257f49")
        self.assertEqual(self.entry.mutation_id, "luthier-beta3-consolidation")
        self.assertEqual(self.entry.query, expected_query("luthier-beta3-consolidation"))

    def test_beta2_era_master_is_not_accepted_for_beta3(self):
        self.api.master = BETA2.expected_master
        self.refuse_without_mutation()
        self.assertEqual(self.api.master, BETA2.expected_master)


class ConsolidationAllowlist(unittest.TestCase):
    def test_only_the_authorized_candidates_consolidate(self):
        self.assertEqual(set(consolidator.CONSOLIDATIONS), {"refs/heads/release/v0.1.0-beta2",
                                                            "refs/heads/release/v0.1.0-beta3"})
        for ref, entry in consolidator.CONSOLIDATIONS.items():
            self.assertEqual(ref, entry.candidate.ref)
            self.assertIs(consolidator.AUTHORIZED[ref], entry.candidate)
        self.assertEqual(len({entry.mutation_id for entry in consolidator.CONSOLIDATIONS.values()}), 2)

    def test_malformed_entries_fail_closed(self):
        candidate = BETA3.candidate
        for master, mutation_id, entry_candidate in (
                ("A2E6FDA8877B93923D2E902EE28DD10FD0257F49", "luthier-beta3-consolidation", candidate),
                ("a2e6fda8", "luthier-beta3-consolidation", candidate),
                (BETA3.expected_master, 'luthier-beta3"-consolidation', candidate),
                (BETA3.expected_master, "luthier-beta3-consolidation",
                 consolidator.Candidate("0.1.0-beta4", ("linux", "windows", "macos")))):
            with self.subTest(master=master, mutation_id=mutation_id, version=entry_candidate.version):
                with self.assertRaises(ValueError):
                    consolidator.Consolidation(entry_candidate, master, mutation_id)

    def test_command_line_requires_the_candidate_tag_before_any_request(self):
        with tempfile.TemporaryDirectory(prefix="luthier-consolidation-cli-") as folder:
            log = Path(folder) / "gh-calls"
            gh = Path(folder) / "gh"
            gh.write_text('#!/bin/sh\necho "$*" >> "$GH_CALL_LOG"\nexit 9\n')
            gh.chmod(0o755)
            env = dict(os.environ, PATH=folder + os.pathsep + os.environ["PATH"], GH_CALL_LOG=str(log),
                       GITHUB_EVENT_NAME="push", GITHUB_REF=BETA3.candidate.ref, GITHUB_SHA=SOURCE,
                       PUBLICATION_RESULT="success", GITHUB_REPOSITORY="djshellshoxxx/luthier",
                       GITHUB_SERVER_URL="https://github.com")
            script = str(SCRIPTS / "consolidate_qualified_beta.py")
            missing = subprocess.run([sys.executable, script, "--source", SOURCE], env=env,
                                     capture_output=True, text=True)
            self.assertEqual(missing.returncode, 2, missing.stderr)
            for tag in (BETA2.candidate.tag, "v0.1.0-beta4", ""):
                with self.subTest(tag=tag):
                    wrong = subprocess.run([sys.executable, script, "--source", SOURCE, "--tag", tag], env=env,
                                           capture_output=True, text=True)
                    self.assertEqual(wrong.returncode, 1, wrong.stderr)
                    self.assertIn("allowlisted candidate push", wrong.stderr)
            self.assertFalse(log.exists())


if __name__ == "__main__":
    unittest.main()
