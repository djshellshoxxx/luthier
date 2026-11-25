"""Check that default-branch consolidation preserves concurrent source work."""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location("consolidate_qualified_beta", SCRIPTS / "consolidate_qualified_beta.py")
consolidator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(consolidator)
SOURCE = "1" * 40
OTHER = "2" * 40


class FixtureGitHub:
    def __init__(self):
        self.master = consolidator.EXPECTED_MASTER
        self.release = {"tag_name": consolidator.TAG, "draft": False,
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
        if endpoint == "/releases/tags/" + consolidator.TAG:
            return self.release
        if endpoint == "/git/ref/heads/master":
            self.master_reads += 1
            if self.advance_master and self.master_reads == 2:
                self.master = OTHER
            return {"ref": "refs/heads/master", "object": {"type": "commit", "sha": self.master}}
        if endpoint == f"/compare/{consolidator.EXPECTED_MASTER}...{SOURCE}":
            return self.comparison
        raise AssertionError("unexpected API endpoint " + endpoint)

    def run(self, *args):
        assert args[:2] == ("api", "graphql")
        assert all(value == "-f" for value in args[2::2])
        fields = dict(value.split("=", 1) for value in args[3::2])
        assert fields == {"query": consolidator.UPDATE_REFS, "repositoryId": "fixture-repository",
                          "before": consolidator.EXPECTED_MASTER, "after": SOURCE}
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
        return json.dumps({"data": {"updateRefs": {"clientMutationId": consolidator.MUTATION_ID}}})


class ConsolidationGuards(unittest.TestCase):
    def setUp(self):
        self.api = FixtureGitHub()
        self.env = {"GITHUB_EVENT_NAME": "push", "GITHUB_REF": consolidator.CANDIDATE,
                    "GITHUB_SHA": SOURCE, "PUBLICATION_RESULT": "success",
                    "GITHUB_REPOSITORY": "djshellshoxxx/luthier", "GITHUB_SERVER_URL": "https://github.com"}

    def run_consolidation(self):
        with patch.dict(os.environ, self.env, clear=True), patch.object(consolidator, "GitHub", return_value=self.api):
            consolidator.consolidate(SOURCE)

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
        for comparison in ({"status": "diverged", "behind_by": 1, "merge_base_commit": {"sha": consolidator.EXPECTED_MASTER}},
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
        self.assertEqual(self.api.master, consolidator.EXPECTED_MASTER)

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
        self.assertEqual(self.api.master, consolidator.EXPECTED_MASTER)


if __name__ == "__main__":
    unittest.main()
