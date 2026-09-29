#!/bin/bash
# Restores each modified tracked file's original line endings (the repo mixes CRLF and LF).
# Run before committing if an editor or script rewrote line endings.
cd "$(dirname "$0")/.."
for f in $(git diff --name-only HEAD); do
  [ -f "$f" ] || continue
  if git show HEAD:"$f" 2>/dev/null | head -1 | grep -q $'\r'; then
    python3 -c "
import sys;p=sys.argv[1];s=open(p,newline='').read();s=s.replace('\r\n','\n').replace('\n','\r\n');open(p,'w',newline='').write(s)" "$f"
  fi
done
