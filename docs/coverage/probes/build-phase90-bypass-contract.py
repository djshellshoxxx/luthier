#!/usr/bin/env python3
"""Build the audit from current sources with an existing configured toolchain.

No configuration/download/installation. Usage: script BUILD_DIR OUTPUT_DIR.
The existing LuthierTests compile database supplies matching platform flags.
"""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
database_path = build / "compile_commands.json"
if database_path.is_file():
    database = json.loads(database_path.read_text())
    database_origin = str(database_path)
else:
    # Read the already-configured Ninja rules; do not reconfigure or fetch.
    database = json.loads(subprocess.check_output(["ninja", "-C", str(build), "-t", "compdb"], text=True))
    database_origin = "existing Ninja -t compdb"


def entry(suffix):
    return next(x for x in database if x["file"].endswith(suffix)
                and "LuthierTests.dir/" in x["command"])


base = shlex.split(entry("Source/ReferencePedals/Models/mxr_phase90_script.cpp")["command"])
flags = []
skip = False
for arg in base[1:]:
    if skip:
        skip = False
    elif arg in ("-o", "-c", "-MF", "-MT", "-MQ"):
        skip = True
    elif arg not in ("-MD", "-MMD"):
        flags.append(arg)
flags += ["-I" + str(root), "-ffunction-sections", "-fdata-sections"]
sources = [root / "docs/coverage/probes/phase90-bypass-contract.cpp"]
sources += [root / "Source/ReferencePedals" / p for p in
            ("Models/mxr_phase90_script.cpp", "NodeTable.cpp", "PedalPool.cpp")]
objects = []
commands = []
for source in sources:
    obj = out / (source.stem + ".o")
    command = [base[0], *flags, "-c", str(source), "-o", str(obj)]
    commands.append(command)
    print("compile", source.relative_to(root), flush=True)
    subprocess.run(command, cwd=build, check=True)
    objects.append(str(obj))
dependencies = ["juce_core/juce_core.cpp", "juce_core/juce_core_CompilationTime.cpp",
                "juce_audio_basics/juce_audio_basics.cpp"]
if any(arg.startswith("-fsanitize=") for arg in flags):
    # ASan global registration / UBSan RTTI also retain the unused node function
    # table and AssetStore type info in these complete production translation
    # units. Supply their real existing objects, never stubs or disabled checks.
    dependencies += ["Source/ReferencePedals/Routing/" + name + ".cpp" for name in
                     ("Lanes", "LatencyEqualiser", "SendReturn", "Feedback", "StereoWidth", "AbSwitch")]
    dependencies += ["Source/ReferencePedals/Services/AssetStore.cpp",
                     "juce_cryptography/juce_cryptography.cpp"]
for suffix in dependencies:
    args = shlex.split(entry(suffix)["command"])
    obj = build / args[args.index("-o") + 1]
    if not obj.is_file():
        raise FileNotFoundError(obj)
    objects.append(str(obj))
binary = out / "phase90-bypass-contract"
command = [base[0], *[arg for arg in flags if arg.startswith("-fsanitize=")],
           "-Wl,--gc-sections", *objects, "-lpthread", "-ldl", "-lrt", "-o", str(binary)]
commands.append(command)
subprocess.run(command, cwd=build, check=True)
provenance = {
    "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
    "build_directory": str(build), "compile_database_origin": database_origin, "commands": commands,
    "sources": {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
    "objects": {p: hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in objects},
    "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
    "production_changes": subprocess.check_output(["git", "diff", "f1efeded", "--", "Source/ReferencePedals"], cwd=root, text=True),
}
(out / "build-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
print("built", binary, provenance["binary_sha256"], flush=True)
