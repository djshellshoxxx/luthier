#!/usr/bin/env python3
"""Exact runtime identity check from an existing unsanitized build's real flags.

Usage: python build-stamp-identity.py BUILD_DIR OUTPUT_DIR EXPECTED_FULL_VERSION
No configure/download/install. Compile actual Diagnostics.cpp with test-only
function/data sections to discard unrelated report/asset methods, not stubs.
Link existing matching JUCE objects/libraries. This is not a product qualification.
"""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
build, out = map(lambda p: Path(p).resolve(), sys.argv[1:3])
expected = sys.argv[3]
out.mkdir(parents=True, exist_ok=True)
database_path = build / "compile_commands.json"
database = (json.loads(database_path.read_text()) if database_path.is_file()
            else json.loads(subprocess.check_output(["ninja", "-C", str(build), "-t", "compdb"], text=True)))


def entry(suffix):
    return next(x for x in database if x["file"].endswith(suffix)
                and "LuthierTests.dir/" in x["command"])


def flags(row):
    args = shlex.split(row["command"])
    result, skip = [], False
    for arg in args[1:]:
        if skip:
            skip = False
        elif arg in ("-o", "-c", "-MF", "-MT", "-MQ"):
            skip = True
        elif arg not in ("-MD", "-MMD"):
            result.append(arg)
    assert not any(x.startswith("-fsanitize=") for x in result), "Use an existing unsanitized build"
    return args[0], result + ["-I"+str(root), "-ffunction-sections", "-fdata-sections"]


diagnostics = entry("Source/Support/Diagnostics.cpp")
compiler, diagnostic_flags = flags(diagnostics)
_, main_flags = flags(entry("Source/ReferencePedals/Models/mxr_phase90_script.cpp"))
assert not any(x.startswith("-DLUTHIER_BUILD_STRING=") for x in main_flags), "Build stamp leaked outside Diagnostics"
commands, objects = [], []
sources = [root / "Source/Support/Diagnostics.cpp", Path(__file__).with_suffix(".cpp")]
t = time.monotonic()
for source, source_flags in zip(sources, [diagnostic_flags, main_flags]):
    obj = out / (source.stem + ".o")
    command = [compiler, *source_flags, "-c", str(source), "-o", str(obj)]
    commands.append(command)
    subprocess.run(command, cwd=build, check=True)
    objects.append(obj)
for row in database:
    if "/JUCE/modules/" not in row["file"] or "LuthierTests.dir/" not in row["command"]:
        continue
    args = shlex.split(row["command"])
    obj = build / args[args.index("-o")+1]
    assert obj.is_file(), obj
    objects.append(obj)
# Diagnostics' included theme constants initialize actual JUCE colours. Keep
# their real module dependencies, even when unused report methods are discarded.
lines = (build / "build.ninja").read_text().splitlines()
at = next(i for i, line in enumerate(lines) if line.startswith("build LuthierTests_artefacts/")
          and "/LuthierTests: CXX_EXECUTABLE_LINKER" in line)
link_libraries = next(line.split(" = ", 1)[1] for line in lines[at+1:]
                      if line.startswith("  LINK_LIBRARIES = "))
binary = out / "build-stamp-identity"
command = [compiler, "-Wl,--gc-sections", *map(str, objects), *shlex.split(link_libraries), "-o", str(binary)]
commands.append(command)
subprocess.run(command, cwd=build, check=True)
result = subprocess.run([str(binary), expected], capture_output=True, text=True, check=False)
report = {"expected": expected, "actual": result.stdout.strip(), "exit": result.returncode,
          "stderr": result.stderr, "seconds": time.monotonic()-t, "commands": commands,
          "source_sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
          "object_sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in objects},
          "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
          "scope": "actual getter using root-generated metadata flags; diagnostic only; no product/full-suite pass"}
(out / "RESULT.json").write_text(json.dumps(report, indent=2)+"\n")
print(json.dumps({k: report[k] for k in ("expected", "actual", "exit", "seconds")}), flush=True)
sys.exit(result.returncode)
