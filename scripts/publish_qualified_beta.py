#!/usr/bin/env python3
"""Publish an owner-authorized beta draft after its complete release workflow passes.

Only the candidates listed in CANDIDATES publish, each from a push of its own
release/v<version> branch for its own tag, with the exact asset set that its
qualified platforms package. This creates no releases, uploads no assets, and
never changes a tag. Every other tag, candidate and manual dispatch retains
draft review.
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


MANIFEST = "SHA256SUMS.txt"
SIGNATURE = MANIFEST + ".asc"
PROVENANCE = "BUILD-PROVENANCE.json"
EDITIONS = ("Pro", "Free")
# release.yml records the selected platforms in this order (its default list).
PLATFORMS = ("linux", "windows", "macos")
# scripts/package_linux.sh PKG_NAME: each edition's .deb/.rpm package name.
LINUX_PACKAGES = {"Pro": "luthier-pro", "Free": "luthier-free"}


class PublicationError(Exception):
    """A qualification or publication check failed; do not retry mutations."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise PublicationError(message)


def json_object(value: Any, context: str) -> dict[str, Any]:
    require(isinstance(value, dict), f"{context} must be a JSON object")
    return value


@dataclass(frozen=True)
class Candidate:
    """One owner-authorized candidate and the assets its qualification packages.

    The names are the real packaging outputs: scripts/package_windows.ps1 (and
    packaging/windows/Luthier.iss), scripts/package_macos.sh, build.yml's
    universal Pro archive, and scripts/package_linux.sh.
    """

    version: str
    platforms: tuple[str, ...]

    def __post_init__(self) -> None:
        # A malformed allowlist entry fails closed instead of widening publication.
        if re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+-beta[0-9]+", self.version) is None:
            raise ValueError(f"Malformed authorized version: {self.version!r}")
        if not self.platforms or list(self.platforms) != [p for p in PLATFORMS if p in self.platforms]:
            raise ValueError(f"{self.version} platforms must be distinct and in release.yml order")

    @property
    def tag(self) -> str:
        return "v" + self.version

    @property
    def ref(self) -> str:
        return "refs/heads/release/" + self.tag

    @property
    def linux_payloads(self) -> frozenset[str]:
        """The Linux packages every qualified Linux build writes (no rpmbuild needed)."""
        if "linux" not in self.platforms:
            return frozenset()
        return frozenset(name for edition in EDITIONS for name in (
            f"Luthier-{edition}-{self.version}-linux-x64.tar.gz",
            f"Luthier-{edition}-{self.version}-standalone-linux-x64.tar.gz",
            f"{LINUX_PACKAGES[edition]}_{self.version}_amd64.deb"))

    @property
    def payloads(self) -> frozenset[str]:
        names = set(self.linux_payloads)
        for edition in EDITIONS:
            if "windows" in self.platforms:
                names |= {f"Luthier-{edition}-{self.version}-Setup-win64.exe",
                          f"Luthier-{edition}-{self.version}-portable-win64.zip"}
            if "macos" in self.platforms:
                names.add(f"Luthier-{edition}-{self.version}-macOS.dmg")
        if "macos" in self.platforms:
            names.add(f"Luthier-Pro-{self.version}-macOS-universal.zip")
        return frozenset(names)

    @property
    def sidecars(self) -> frozenset[str]:
        return frozenset(name + ".sha256" for name in self.payloads if "portable-win64" in name)

    @property
    def rpms(self) -> frozenset[str]:
        """Best effort: package_linux.sh writes an .rpm only when rpmbuild succeeds."""
        if "linux" not in self.platforms:
            return frozenset()
        return frozenset(f"{LINUX_PACKAGES[edition]}-{self.version}-1.x86_64.rpm" for edition in EDITIONS)

    @property
    def required(self) -> frozenset[str]:
        return self.payloads | self.sidecars | {MANIFEST, PROVENANCE}

    def accepts(self, names: set[str]) -> bool:
        """Exactly the required assets plus only the explicitly optional extras.

        Those are each edition's .rpm and one complete set of detached
        signatures. The same GPG_PRIVATE_KEY signs SHA256SUMS.txt (release.yml)
        and every Linux package (package_linux.sh writes <package>.asc), so the
        signatures are either all present or all absent.
        """
        if not self.required <= names:
            return False
        extras = names - self.required
        rpms = extras & self.rpms
        signatures = {SIGNATURE} | {name + ".asc" for name in self.linux_payloads | rpms}
        return extras in (rpms, rpms | signatures)


# The owner-authorized candidates and the platforms each was qualified on.
# Nothing else publishes automatically.
CANDIDATES = (
    Candidate("0.1.0-beta2", ("windows", "macos")),
    Candidate("0.1.0-beta3", ("linux", "windows", "macos")),
)
AUTHORIZED = {candidate.ref: candidate for candidate in CANDIDATES}


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


def inventory(directory: Path, candidate: Candidate) -> dict[str, Digest]:
    files = {path.name: file_digest(path) for path in directory.iterdir()}
    require(candidate.accepts(set(files)),
            f"Asset inventory must contain exactly the {candidate.version} binaries, sidecars, "
            "provenance and checksums, plus only the optional .rpm and complete signature set")
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


def validate_local(directory: Path, candidate: Candidate, source: str,
                   repository: str, run_id: str) -> dict[str, Digest]:
    files = inventory(directory, candidate)
    entries = checksum_entries(directory / MANIFEST)
    expected = {name: digest.sha256 for name, digest in files.items()
                if name not in (MANIFEST, SIGNATURE)}
    require(entries == expected, "Checksum manifest does not match every qualified asset")
    for sidecar in candidate.sidecars:
        archive = sidecar.removesuffix(".sha256")
        require(checksum_entries(directory / sidecar) == {archive: files[archive].sha256},
                f"Portable checksum does not match {archive}")
    provenance = json_object(json.loads((directory / PROVENANCE).read_text(encoding="utf-8")),
                             "Build provenance")
    require(provenance.get("source_commit") == source, "Provenance source differs from frozen source")
    require(provenance.get("version") == candidate.version,
            f"Provenance version differs from {candidate.version}")
    require(provenance.get("platforms") == list(candidate.platforms),
            "Provenance must list exactly the qualified platforms: " + ",".join(candidate.platforms))
    require(provenance.get("editions") == ["pro", "free"], "Both qualified editions are required")
    require(provenance.get("ci_run") == f"https://github.com/{repository}/actions/runs/{run_id}",
            "Provenance does not identify this qualification run")
    return files


class GitHub:
    def __init__(self, repository: str, candidate: Candidate):
        self.repository = repository
        self.candidate = candidate
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
        ref, tag_name = self.candidate.ref, self.candidate.tag
        branch = json_object(self.api("/git/ref/" + ref.removeprefix("refs/")), "Candidate ref")
        target = json_object(branch.get("object"), "Candidate object")
        require(branch.get("ref") == ref and target.get("type") == "commit"
                and target.get("sha") == source, "Candidate branch advanced or differs from frozen source")
        tag = json_object(self.api("/git/ref/tags/" + tag_name), "Release tag")
        require(tag.get("ref") == "refs/tags/" + tag_name, "Release tag identity differs")
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
        matches = [release for release in releases if release.get("tag_name") == self.candidate.tag]
        require(len(matches) == 1, f"Expected exactly one newly created {self.candidate.version} draft")
        return matches[0]

    def verify_release(self, release: dict[str, Any], source: str, draft: bool,
                       files: dict[str, Digest]) -> dict[str, int]:
        tag, version = self.candidate.tag, self.candidate.version
        release_id = release.get("id")
        require(type(release_id) is int and release_id > 0, "Invalid release ID")
        require(release.get("tag_name") == tag and release.get("name") == "Luthier " + version,
                f"Release identity differs from the authorized {version}")
        require(release.get("target_commitish") == source, "Release target differs from frozen source")
        require(release.get("draft") is draft and release.get("prerelease") is True,
                "Release must retain its prerelease status and expected draft state")
        require(release.get("url") == f"https://api.github.com/{self.prefix}/releases/{release_id}",
                "Release API identity differs from the authorized repository")
        # GitHub gives drafts an untagged-* web URL until publication.
        if not draft:
            require(release.get("html_url") == f"https://github.com/{self.repository}/releases/tag/{tag}",
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


def authorized_candidate(tag: str) -> Candidate:
    # An exact ref lookup: no prefix, pattern or case folding selects a candidate.
    candidate = AUTHORIZED.get(os.environ.get("GITHUB_REF", ""))
    require(os.environ.get("GITHUB_EVENT_NAME") == "push" and candidate is not None
            and tag == candidate.tag,
            "Automatic publication is authorized only for an allowlisted candidate push of its own tag")
    return candidate


def publish(directory: Path, source: str, tag: str) -> str:
    candidate = authorized_candidate(tag)
    require(re.fullmatch(r"[0-9a-f]{40}", source) is not None
            and os.environ.get("GITHUB_SHA") == source, "Frozen source must match this push")
    require(os.environ.get("QUALIFICATION_RESULT") == "success", "Full build qualification did not succeed")
    repository = os.environ.get("GITHUB_REPOSITORY", "")
    run_id = os.environ.get("GITHUB_RUN_ID", "")
    require(re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository) is not None
            and re.fullmatch(r"[0-9]+", run_id) is not None, "Invalid qualification run identity")
    require(os.environ.get("GITHUB_SERVER_URL") == "https://github.com", "Unexpected GitHub server")
    files = validate_local(directory, candidate, source, repository, run_id)
    github = GitHub(repository, candidate)
    github.check_source(source)
    release = github.find_draft()
    identities = github.verify_release(release, source, True, files)
    release_id = release["id"]
    with tempfile.TemporaryDirectory(prefix=f"luthier-{candidate.version}-download-") as temporary:
        github.run("release", "download", candidate.tag, "--repo", repository, "--dir", temporary, timeout=1800)
        require(inventory(Path(temporary), candidate) == files,
                "Downloaded release bytes differ from qualified assets")
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
        print(f"::error::Qualified beta publication did not complete verification: {error}. "
              "No automatic retry or rollback was attempted; inspect the draft/release state.", file=sys.stderr)
        return 1
    print("Published and verified prerelease: " + url)
    return 0


if __name__ == "__main__":
    sys.exit(main())
