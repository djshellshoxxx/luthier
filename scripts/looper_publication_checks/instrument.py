"""Insert a scheduling pause into a test-only copy; never edit product sources."""
from pathlib import Path
import json
import sys

source, destination = map(Path, sys.argv[1:])
original = source.read_text()
needle = 'manifestReady.store(job.epoch,std::memory_order_release);hashSequence.fetch_add(1,std::memory_order_release);'
if original.count(needle) != 1:
    raise SystemExit('Publication boundary changed; update and review the fault-injection harness.')
replacement = 'manifestReady.store(job.epoch,std::memory_order_release);::luthierManifestPublicationProbe();hashSequence.fetch_add(1,std::memory_order_release);'
prefix = 'void luthierManifestPublicationProbe();\n#line 1 ' + json.dumps(source.resolve().as_posix()) + '\n'
destination.write_text(prefix + original.replace(needle, replacement))
