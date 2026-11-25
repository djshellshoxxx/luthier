#!/usr/bin/env python3
"""Fast-forward master to an owner-authorized beta's qualified consolidation source.

Run only after this same candidate workflow publishes and verifies that beta.
Each authorized candidate names the master it was authorized against; any
other master stops for review. This never force-pushes or replaces a changed
default branch.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
import re
import subprocess
import sys

from publish_qualified_beta import AUTHORIZED, Candidate, GitHub, PublicationError, json_object, require


REPOSITORY = "djshellshoxxx/luthier"
# The clientMutationId is filled in from the candidate's own fixed entry.
UPDATE_REFS = """mutation($repositoryId: ID!, $before: GitObjectID!, $after: GitObjectID!) {
  updateRefs(input: {
    repositoryId: $repositoryId, clientMutationId: "%s",
    refUpdates: [{name: "refs/heads/master", beforeOid: $before, afterOid: $after, force: false}]
  }) { clientMutationId }
}"""


@dataclass(frozen=True)
class Consolidation:
    candidate: Candidate
    # master as observed when the owner authorized this candidate.
    expected_master: str
    mutation_id: str

    def __post_init__(self) -> None:
        if (re.fullmatch(r"[0-9a-f]{40}", self.expected_master) is None
                or re.fullmatch(r"luthier-[a-z0-9]+-consolidation", self.mutation_id) is None
                or AUTHORIZED.get(self.candidate.ref) is not self.candidate):
            raise ValueError(f"Malformed consolidation entry for {self.candidate.version}")

    @property
    def query(self) -> str:
        return UPDATE_REFS % self.mutation_id


# beta2: master before its consolidation. beta3: master after beta2's
# consolidation (the published beta2 source), as authorized for beta3.
CONSOLIDATIONS = {entry.candidate.ref: entry for entry in (
    Consolidation(AUTHORIZED["refs/heads/release/v0.1.0-beta2"],
                  "0f62c3d67bdba055b41e66c7ab8ac9ab4b728662", "luthier-beta2-consolidation"),
    Consolidation(AUTHORIZED["refs/heads/release/v0.1.0-beta3"],
                  "a2e6fda8877b93923d2e902ee28dd10fd0257f49", "luthier-beta3-consolidation"),
)}


def consolidate(source: str, tag: str) -> None:
    # An exact ref lookup: no prefix, pattern or case folding selects an entry.
    entry = CONSOLIDATIONS.get(os.environ.get("GITHUB_REF", ""))
    require(os.environ.get("GITHUB_EVENT_NAME") == "push"
            and entry is not None and tag == entry.candidate.tag
            and os.environ.get("GITHUB_SHA") == source
            and re.fullmatch(r"[0-9a-f]{40}", source) is not None,
            "Consolidation is authorized only for a frozen allowlisted candidate push of its own tag")
    candidate = entry.candidate
    require(os.environ.get("PUBLICATION_RESULT") == "success",
            f"The same workflow must publish and verify {candidate.version} before consolidation")
    require(os.environ.get("GITHUB_REPOSITORY") == REPOSITORY
            and os.environ.get("GITHUB_SERVER_URL") == "https://github.com",
            "Unexpected consolidation repository")

    github = GitHub(REPOSITORY, candidate)
    github.check_source(source)
    release = json_object(github.api("/releases/tags/" + candidate.tag), "Published " + candidate.version)
    require(release.get("tag_name") == candidate.tag and release.get("draft") is False
            and release.get("prerelease") is True and release.get("target_commitish") == source,
            f"Published {candidate.version} does not identify the tested consolidation source")

    def master_sha() -> str:
        ref = json_object(github.api("/git/ref/heads/master"), "Default branch")
        target = json_object(ref.get("object"), "Default branch object")
        require(ref.get("ref") == "refs/heads/master" and target.get("type") == "commit",
                "Unexpected default branch ref")
        return target.get("sha")

    current = master_sha()
    if current == source:
        print(f"master already identifies the verified {candidate.version} source")
        return
    require(current == entry.expected_master,
            "master changed during qualification; retain the published beta and review consolidation")
    comparison = json_object(github.api(f"/compare/{current}...{source}"), "Consolidation ancestry")
    merge_base = json_object(comparison.get("merge_base_commit"), "Merge base")
    require(comparison.get("status") == "ahead" and comparison.get("behind_by") == 0
            and merge_base.get("sha") == current,
            "Consolidation source must contain every current master commit")
    repository = json_object(github.api(""), "Repository")
    repository_id = repository.get("node_id")
    require(repository.get("full_name") == REPOSITORY
            and isinstance(repository_id, str) and bool(repository_id), "Repository identity differs")
    # beforeOid is an atomic compare-and-swap, including a concurrent move to
    # another ancestor of source. force:false also forbids history replacement.
    # https://docs.github.com/en/graphql/reference/git#updaterefs
    github.check_source(source)
    require(master_sha() == entry.expected_master, "master advanced during consolidation checks")
    response = json_object(json.loads(github.run("api", "graphql", "-f", "query=" + entry.query,
                           "-f", "repositoryId=" + repository_id, "-f", "before=" + entry.expected_master,
                           "-f", "after=" + source)), "Atomic branch update")
    require(not response.get("errors"), "GitHub rejected the atomic branch update")
    data = json_object(response.get("data"), "Branch update data")
    updated = json_object(data.get("updateRefs"), "Branch update result")
    require(updated.get("clientMutationId") == entry.mutation_id, "Branch update acknowledgement differs")
    require(master_sha() == source, "Default branch update did not retain the qualified source")
    print(f"Consolidated master at verified {candidate.version} source {source}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    parser.add_argument("--tag", required=True)
    args = parser.parse_args()
    try:
        consolidate(args.source, args.tag)
    except (PublicationError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"::error::Default branch consolidation did not complete verification: {error}. "
              "Inspect master and the published beta before any retry.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
