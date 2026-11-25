#!/usr/bin/env python3
"""Fast-forward master to the owner's qualified beta2 consolidation source.

Run only after this same candidate workflow publishes and verifies beta2.
This never force-pushes or replaces a changed default branch.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys

from publish_qualified_beta import CANDIDATE, TAG, GitHub, PublicationError, json_object, require


EXPECTED_MASTER = "0f62c3d67bdba055b41e66c7ab8ac9ab4b728662"
MUTATION_ID = "luthier-beta2-consolidation"
UPDATE_REFS = """mutation($repositoryId: ID!, $before: GitObjectID!, $after: GitObjectID!) {
  updateRefs(input: {
    repositoryId: $repositoryId, clientMutationId: "luthier-beta2-consolidation",
    refUpdates: [{name: "refs/heads/master", beforeOid: $before, afterOid: $after, force: false}]
  }) { clientMutationId }
}"""


def consolidate(source: str) -> None:
    require(os.environ.get("GITHUB_EVENT_NAME") == "push"
            and os.environ.get("GITHUB_REF") == CANDIDATE
            and os.environ.get("GITHUB_SHA") == source
            and re.fullmatch(r"[0-9a-f]{40}", source) is not None,
            "Consolidation is authorized only for the frozen beta2 candidate push")
    require(os.environ.get("PUBLICATION_RESULT") == "success",
            "The same workflow must publish and verify beta2 before consolidation")
    require(os.environ.get("GITHUB_REPOSITORY") == "djshellshoxxx/luthier"
            and os.environ.get("GITHUB_SERVER_URL") == "https://github.com",
            "Unexpected consolidation repository")

    github = GitHub("djshellshoxxx/luthier")
    github.check_source(source)
    release = json_object(github.api("/releases/tags/" + TAG), "Published beta2")
    require(release.get("tag_name") == TAG and release.get("draft") is False
            and release.get("prerelease") is True and release.get("target_commitish") == source,
            "Published beta2 does not identify the tested consolidation source")

    def master_sha() -> str:
        ref = json_object(github.api("/git/ref/heads/master"), "Default branch")
        target = json_object(ref.get("object"), "Default branch object")
        require(ref.get("ref") == "refs/heads/master" and target.get("type") == "commit",
                "Unexpected default branch ref")
        return target.get("sha")

    current = master_sha()
    if current == source:
        print("master already identifies the verified beta2 source")
        return
    require(current == EXPECTED_MASTER,
            "master changed during qualification; retain the published beta and review consolidation")
    comparison = json_object(github.api(f"/compare/{current}...{source}"), "Consolidation ancestry")
    merge_base = json_object(comparison.get("merge_base_commit"), "Merge base")
    require(comparison.get("status") == "ahead" and comparison.get("behind_by") == 0
            and merge_base.get("sha") == current,
            "Consolidation source must contain every current master commit")
    repository = json_object(github.api(""), "Repository")
    repository_id = repository.get("node_id")
    require(repository.get("full_name") == "djshellshoxxx/luthier"
            and isinstance(repository_id, str) and bool(repository_id), "Repository identity differs")
    # beforeOid is an atomic compare-and-swap, including a concurrent move to
    # another ancestor of source. force:false also forbids history replacement.
    # https://docs.github.com/en/graphql/reference/git#updaterefs
    github.check_source(source)
    require(master_sha() == EXPECTED_MASTER, "master advanced during consolidation checks")
    response = json_object(json.loads(github.run("api", "graphql", "-f", "query=" + UPDATE_REFS,
                           "-f", "repositoryId=" + repository_id, "-f", "before=" + EXPECTED_MASTER,
                           "-f", "after=" + source)), "Atomic branch update")
    require(not response.get("errors"), "GitHub rejected the atomic branch update")
    data = json_object(response.get("data"), "Branch update data")
    updated = json_object(data.get("updateRefs"), "Branch update result")
    require(updated.get("clientMutationId") == MUTATION_ID, "Branch update acknowledgement differs")
    require(master_sha() == source, "Default branch update did not retain the qualified source")
    print("Consolidated master at verified beta2 source " + source)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    args = parser.parse_args()
    try:
        consolidate(args.source)
    except (PublicationError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"::error::Default branch consolidation did not complete verification: {error}. "
              "Inspect master and the published beta2 before any retry.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
