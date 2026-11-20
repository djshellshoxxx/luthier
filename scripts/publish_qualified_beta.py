#!/usr/bin/env python3
"""Publish the authorized beta2 draft after its complete release workflow passes.

This deliberately supports one candidate only. It creates no releases, uploads
no assets, and never changes a tag. Other release paths retain draft review.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Any


VERSION = "0.1.0-beta2"
TAG = "v" + VERSION
CANDIDATE = "refs/heads/release/" + TAG
MANIFEST = "SHA256SUMS.txt"
SIGNATURE = MANIFEST + ".asc"
PROVENANCE = "BUILD-PROVENANCE.json"
PAYLOADS = frozenset(
    f"Luthier-{edition}-{VERSION}-{suffix}"
    for edition in ("Pro", "Free")
    for suffix in ("Setup-win64.exe", "portable-win64.zip", "macOS.dmg")
) | {f"Luthier-Pro-{VERSION}-macOS-universal.zip"}
SIDECARS = frozenset(name + ".sha256" for name in PAYLOADS if "portable-win64" in name)
REQUIRED = PAYLOADS | SIDECARS | {MANIFEST, PROVENANCE}


class PublicationError(Exception):
    """A qualification or publication check failed; do not retry mutations."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise PublicationError(message)


def json_object(value: Any, context: str) -> dict[str, Any]:
    require(isinstance(value, dict), f"{context} must be a JSON object")
    return value


@dataclass(frozen=True)
class Digest:
    size: int
    sha256: str


def file_digest(path: Path) -> Digest:
    require(path.is_file() and not path.is_symlink(), f"Not a regular asset: {path.name}")
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            size += len(chunk)
            digest.update(chunk)
    require(size > 0, f"Empty asset: {path.name}")
    return Digest(size, digest.hexdigest())


def inventory(directory: Path) -> dict[str, Digest]:
    files = {path.name: file_digest(path) for path in directory.iterdir()}
    require(set(files) in (REQUIRED, REQUIRED | {SIGNATURE}),
            "Asset inventory must contain exactly the beta2 binaries, sidecars, provenance and checksums")
    return files


def checksum_entries(path: Path) -> dict[str, str]:
    entries: dict[str, str] = {}
    for line in path.read_text(encoding="ascii").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64}) [ *]([^/\\]+)", line)
        require(match is not None, f"Malformed checksum entry in {path.name}")
        digest, name = match.groups()
        require(name not in entries, f"Duplicate checksum for {name}")
        entries[name] = digest
    return entries


def validate_local(directory: Path, source: str, repository: str, run_id: str) -> dict[str, Digest]:
    files = inventory(directory)
    entries = checksum_entries(directory / MANIFEST)
    expected = {name: digest.sha256 for name, digest in files.items()
                if name not in (MANIFEST, SIGNATURE)}
    require(entries == expected, "Checksum manifest does not match every qualified asset")
    for sidecar in SIDECARS:
        archive = sidecar.removesuffix(".sha256")
        require(checksum_entries(directory / sidecar) == {archive: files[archive].sha256},
                f"Portable checksum does not match {archive}")
    provenance = json_object(json.loads((directory / PROVENANCE).read_text(encoding="utf-8")),
                             "Build provenance")
    require(provenance.get("source_commit") == source, "Provenance source differs from frozen source")
    require(provenance.get("version") == VERSION, "Provenance version differs from beta2")
    require(provenance.get("platforms") == ["windows", "macos"], "Both qualified platforms are required")
    require(provenance.get("editions") == ["pro", "free"], "Both qualified editions are required")
    require(provenance.get("ci_run") == f"https://github.com/{repository}/actions/runs/{run_id}",
            "Provenance does not identify this qualification run")
    return files


class GitHub:
    def __init__(self, repository: str):
        self.repository = repository
        self.prefix = "repos/" + repository

    @staticmethod
    def run(*args: str, timeout: int = 120) -> str:
        result = subprocess.run(["gh", *args], check=True, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
        return result.stdout

    def api(self, endpoint: str, *args: str) -> Any:
        return json.loads(self.run("api", self.prefix + endpoint, *args))

    def pages(self, endpoint: str) -> list[Any]:
        # --paginate emits one JSON array per page. Decode the stream directly
        # so the runner does not need a particular gh --slurp version.
        remaining = self.run("api", self.prefix + endpoint, "--paginate").strip()
        items: list[Any] = []
        require(bool(remaining), "Empty paginated GitHub response")
        while remaining:
            page, end = json.JSONDecoder().raw_decode(remaining)
            require(isinstance(page, list), "Expected complete paginated GitHub array")
            items.extend(page)
            remaining = remaining[end:].lstrip()
        return items

    def check_source(self, source: str) -> None:
        branch = json_object(self.api("/git/ref/" + CANDIDATE.removeprefix("refs/")), "Candidate ref")
        target = json_object(branch.get("object"), "Candidate object")
        require(branch.get("ref") == CANDIDATE and target.get("type") == "commit"
                and target.get("sha") == source, "Candidate branch advanced or differs from frozen source")
        tag = json_object(self.api("/git/ref/tags/" + TAG), "Release tag")
        require(tag.get("ref") == "refs/tags/" + TAG, "Release tag identity differs")
        target = json_object(tag.get("object"), "Tag object")
        # The workflow creates lightweight tags; allow an already verified
        # annotated tag, while bounding malformed/cyclic API responses.
        for _ in range(8):
            if target.get("type") != "tag":
                break
            sha = target.get("sha")
            require(isinstance(sha, str) and re.fullmatch(r"[0-9a-f]{40}", sha) is not None,
                    "Malformed annotated tag SHA")
            annotation = json_object(self.api("/git/tags/" + sha), "Annotated tag")
            target = json_object(annotation.get("object"), "Annotated tag object")
        require(target.get("type") == "commit" and target.get("sha") == source,
                "Release tag differs from frozen source")

    def find_draft(self) -> dict[str, Any]:
        releases = [json_object(item, "Release") for item in self.pages("/releases?per_page=100")]
        matches = [release for release in releases if release.get("tag_name") == TAG]
        require(len(matches) == 1, "Expected exactly one newly created beta2 draft")
        return matches[0]

    def verify_release(self, release: dict[str, Any], source: str, draft: bool,
                       files: dict[str, Digest]) -> dict[str, int]:
        release_id = release.get("id")
        require(type(release_id) is int and release_id > 0, "Invalid release ID")
        require(release.get("tag_name") == TAG and release.get("name") == "Luthier " + VERSION,
                "Release identity differs from the authorized beta2")
        require(release.get("target_commitish") == source, "Release target differs from frozen source")
        require(release.get("draft") is draft and release.get("prerelease") is True,
                "Release must retain its prerelease status and expected draft state")
        require(release.get("url") == f"https://api.github.com/{self.prefix}/releases/{release_id}",
                "Release API identity differs from the authorized repository")
        # GitHub gives drafts an untagged-* web URL until publication.
        if not draft:
            require(release.get("html_url") == f"https://github.com/{self.repository}/releases/tag/{TAG}",
                    "Published release URL differs from the authorized repository and tag")
        assets = self.pages(f"/releases/{release_id}/assets?per_page=100")
        identities: dict[str, int] = {}
        for item in assets:
            asset = json_object(item, "Release asset")
            name, asset_id = asset.get("name"), asset.get("id")
            require(isinstance(name, str) and name in files and name not in identities,
                    "Unexpected or duplicate uploaded asset")
            require(type(asset_id) is int and asset_id > 0 and asset_id not in identities.values(),
                    "Invalid or duplicate asset ID")
            expected = files[name]
            require(asset.get("state") == "uploaded" and type(asset.get("size")) is int
                    and asset["size"] == expected.size, f"Uploaded size/state differs for {name}")
            require(asset.get("digest") == "sha256:" + expected.sha256,
                    f"Uploaded digest differs or is unavailable for {name}")
            identities[name] = asset_id
        require(set(identities) == set(files), "Uploaded release is missing qualified assets")
        return identities


def publish(directory: Path, source: str, tag: str) -> str:
    require(os.environ.get("GITHUB_EVENT_NAME") == "push"
            and os.environ.get("GITHUB_REF") == CANDIDATE and tag == TAG,
            "Automatic publication is authorized only for the beta2 candidate push")
    require(re.fullmatch(r"[0-9a-f]{40}", source) is not None
            and os.environ.get("GITHUB_SHA") == source, "Frozen source must match this push")
    require(os.environ.get("QUALIFICATION_RESULT") == "success", "Full build qualification did not succeed")
    repository = os.environ.get("GITHUB_REPOSITORY", "")
    run_id = os.environ.get("GITHUB_RUN_ID", "")
    require(re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository) is not None
            and re.fullmatch(r"[0-9]+", run_id) is not None, "Invalid qualification run identity")
    require(os.environ.get("GITHUB_SERVER_URL") == "https://github.com", "Unexpected GitHub server")
    files = validate_local(directory, source, repository, run_id)
    github = GitHub(repository)
    github.check_source(source)
    release = github.find_draft()
    identities = github.verify_release(release, source, True, files)
    release_id = release["id"]
    with tempfile.TemporaryDirectory(prefix="luthier-beta2-download-") as temporary:
        github.run("release", "download", TAG, "--repo", repository, "--dir", temporary, timeout=1800)
        require(inventory(Path(temporary)) == files, "Downloaded release bytes differ from qualified assets")
    # Re-read the exact draft and its asset IDs after downloading, then check
    # the candidate immediately before the sole state-changing request.
    current = json_object(github.api(f"/releases/{release_id}"), "Current draft")
    require(current.get("id") == release_id, "Draft identity changed")
    require(github.verify_release(current, source, True, files) == identities,
            "Draft asset identities changed during verification")
    github.check_source(source)
    # Address the verified numeric ID, never re-resolve the tag for a mutation.
    # No retries, asset replacement, ref updates, or release creation occur here.
    github.api(f"/releases/{release_id}", "--method", "PATCH", "-F", "draft=false", "-F", "prerelease=true")
    published = json_object(github.api(f"/releases/{release_id}"), "Published release")
    require(published.get("id") == release_id, "Published release identity changed")
    require(github.verify_release(published, source, False, files) == identities,
            "Published asset identities changed")
    github.check_source(source)
    return published["html_url"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--tag", required=True)
    args = parser.parse_args()
    try:
        url = publish(args.directory, args.source, args.tag)
    except (PublicationError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"::error::Beta2 publication did not complete verification: {error}. "
              "No automatic retry or rollback was attempted; inspect the draft/release state.", file=sys.stderr)
        return 1
    print("Published and verified prerelease: " + url)
    return 0


if __name__ == "__main__":
    sys.exit(main())
