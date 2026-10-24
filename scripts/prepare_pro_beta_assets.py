#!/usr/bin/env python3
"""Validate the three Pro beta payloads and prepare immutable release assets.

This inspects archive members without extracting or executing target binaries.
The calling workflow establishes the source SHA, edition and passing CI gates.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import struct
import tarfile
import zipfile

VERSION = "0.1.0-beta"
TAG = "v0.1-beta"


def make_contract(version: str, tag: str) -> dict:
    return {
        "windows": (f"Luthier-Pro-{version}-portable-win64.zip", f"Luthier-Pro-{tag}-Windows-x64.zip"),
        "linux": (f"Luthier-Pro-{version}-linux-x64.tar.gz", f"Luthier-Pro-{tag}-Linux-x64.tar.gz"),
        "macos": (f"Luthier-Pro-{version}-macOS-universal.zip", f"Luthier-Pro-{tag}-macOS-universal.zip"),
    }


CONTRACT = make_contract(VERSION, TAG)


def configure_release(version: str, tag: str) -> None:
    """Bind all archive and provenance identities to one validated beta version."""
    import re
    if not re.fullmatch(r"0\.1\.0-beta[0-9]+", version):
        raise ValueError("invalid beta version")
    if tag != "v" + version:
        raise ValueError("beta tag does not match version")
    global VERSION, TAG, CONTRACT
    VERSION, TAG, CONTRACT = version, tag, make_contract(version, tag)


def executable(data: bytes, platform: str) -> bool:
    """Check executable format/architecture, rather than trusting extensions."""
    if platform == "windows":
        if len(data) < 64 or data[:2] != b"MZ":
            return False
        offset = struct.unpack_from("<I", data, 60)[0]
        return (offset + 6 <= len(data) and data[offset:offset + 4] == b"PE\0\0"
                and struct.unpack_from("<H", data, offset + 4)[0] == 0x8664)
    if platform == "linux":
        return (len(data) >= 20 and data[:6] == b"\x7fELF\x02\x01"
                and struct.unpack_from("<H", data, 18)[0] == 62)
    # Universal Mach-O must contain both Intel and Apple Silicon slices.
    if len(data) < 8 or data[:4] not in (b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"):
        return False
    count = struct.unpack_from(">I", data, 4)[0]
    wide = data[:4] == b"\xca\xfe\xba\xbf"
    stride = 32 if wide else 20
    if count < 2 or count > 16 or 8 + count * stride > len(data):
        return False
    architectures = set()
    for index in range(count):
        at = 8 + index * stride
        cpu = struct.unpack_from(">I", data, at)[0]
        offset, size = struct.unpack_from(">QQ" if wide else ">II", data, at + 8)
        if size < 8 or offset + size > len(data):
            return False
        if data[offset:offset + 4] != b"\xcf\xfa\xed\xfe":
            return False
        if struct.unpack_from("<I", data, offset + 4)[0] != cpu:
            return False
        architectures.add(cpu)
    return {0x01000007, 0x0100000C} <= architectures


def members(path: Path) -> dict[str, bytes]:
    """Read ordinary files only; reject traversal, links and duplicate paths."""
    result = {}
    def add(name: str, content: bytes) -> None:
        name = name.replace("\\", "/")
        parts = PurePosixPath(name).parts
        if not parts or name.startswith("/") or ".." in parts or ":" in parts[0]:
            raise ValueError(f"unsafe archive path: {name}")
        normal = str(PurePosixPath(name))
        if normal in result:
            raise ValueError(f"duplicate archive member: {normal}")
        if "Luthier Free" in normal or "Luthier-Free" in normal:
            raise ValueError(f"Free edition in Pro archive: {normal}")
        result[normal] = content
    if path.name.endswith(".zip"):
        with zipfile.ZipFile(path) as archive:
            for entry in archive.infolist():
                if entry.is_dir():
                    continue
                if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                    raise ValueError(f"archive symlink: {entry.filename}")
                add(entry.filename, archive.read(entry))
    else:
        with tarfile.open(path, "r:gz") as archive:
            for entry in archive:
                if entry.isdir():
                    continue
                if not entry.isfile():
                    raise ValueError(f"non-file archive member: {entry.name}")
                stream = archive.extractfile(entry)
                if stream is None:
                    raise ValueError(f"unreadable archive member: {entry.name}")
                add(entry.name, stream.read())
    return result


def validate(path: Path, platform: str) -> dict:
    files = members(path)
    def require(suffix: str, binary: bool = False) -> str:
        found = [(name, data) for name, data in files.items()
                 if (name == suffix or name.endswith("/" + suffix)) and data]
        if len(found) != 1:
            raise ValueError(f"{path.name}: expected one nonempty {suffix}")
        name, data = found[0]
        if binary and not executable(data, platform):
            raise ValueError(f"{path.name}: invalid {platform} executable: {name}")
        return name
    if platform == "windows":
        binaries = [require("Luthier Pro.exe", True),
                    require("Luthier Pro.vst3/Contents/x86_64-win/Luthier Pro.vst3", True),
                    require("Luthier Pro.clap", True)]
    elif platform == "linux":
        binaries = [require("luthier", True),
                    require("Luthier Pro.vst3/Contents/x86_64-linux/Luthier Pro.so", True),
                    require("Luthier Pro.clap", True)]
        require("install.sh")
        version = files[require("VERSION")].decode().strip()
        if version != VERSION:
            raise ValueError(f"incorrect Linux package version: {version}")
    else:
        binaries = [require(f"Luthier Pro.{kind}/Contents/MacOS/Luthier Pro", True)
                    for kind in ("app", "vst3", "component", "clap")]
    require("README-BETA.md")
    if not any("Resources" in PurePosixPath(name).parts and data for name, data in files.items()):
        raise ValueError(f"{path.name}: missing factory Resources")
    return {"platform": platform, "binaries": binaries, "file_count": len(files)}


def prepare(source: Path, output: Path, sha: str) -> None:
    if len(sha) != 40 or any(c not in "0123456789abcdef" for c in sha):
        raise ValueError("source SHA must be 40 lowercase hexadecimal characters")
    if output.exists() and any(output.iterdir()):
        raise ValueError("release output directory must be empty")
    validated = []
    for platform, (original, destination) in CONTRACT.items():
        matches = list(source.rglob(original))
        if len(matches) != 1:
            raise ValueError(f"expected exactly one {original}; found {len(matches)}")
        proof = validate(matches[0], platform)
        proof["asset"] = destination
        validated.append((matches[0], destination, proof))
    # Write only after all platforms pass, preventing partial release payloads.
    output.mkdir(parents=True, exist_ok=True)
    for original, destination, _ in validated:
        shutil.copyfile(original, output / destination)
    proof = {"tag": TAG, "version": VERSION, "edition": "Pro", "source_commit": sha,
             "payloads": [item[2] for item in validated]}
    (output / "BUILD-PROVENANCE.json").write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    lines = []
    for path in sorted(output.iterdir()):
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        lines.append(f"{digest}  {path.name}\n")
    (output / "SHA256SUMS").write_text("".join(lines), encoding="ascii")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--version", default=None,
                        help="Explicit beta archive version, for example 0.1.0-beta2")
    parser.add_argument("--tag", default=None,
                        help="Matching immutable release tag, for example v0.1.0-beta2")
    args = parser.parse_args()
    try:
        if (args.version is None) != (args.tag is None):
            raise ValueError("version and tag must be specified together")
        if args.version is not None:
            configure_release(args.version, args.tag)
        prepare(args.input, args.output, args.source_sha)
    except (ValueError, OSError, zipfile.BadZipFile, tarfile.TarError) as error:
        parser.exit(1, f"release asset validation failed: {error}\n")


if __name__ == "__main__":
    main()
