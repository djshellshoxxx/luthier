#!/bin/bash
# auto-articulation.md AA-01 (FEAT-ASSIST): "Off is today."
#
# Renders the eight ComboHarness phrases (plus a legato melody) through every
# factory preset with two luthier-render builds - one from before Performance
# Assist, one after - and requires byte-identical 32-bit WAVs. The Assist
# presets are skipped (they did not exist before).
#
#   scripts/assist_off_golden_check.sh <old luthier-render> <new luthier-render>
#
# Build the old renderer from the pre-feature commit (ee09a86) in a worktree:
#   git worktree add /tmp/base ee09a86 && ln -s "$PWD/ThirdParty/JUCE" /tmp/base/ThirdParty/JUCE
#   cmake -S /tmp/base -B /tmp/base/build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C /tmp/base/build LuthierRender
set -u
OLD="$1"; NEW="$2"
WORK=$(mktemp -d)
python3 "$(dirname "$0")/assist_phrases_mid.py" "$WORK/phrases.mid"
"$NEW" --list-presets 2>&1 | grep factory | grep -v "^  Assist" | sed 's/ *factory$//; s/^  //' > "$WORK/presets.txt"
fail=0
while IFS= read -r p; do
  "$OLD" --midi "$WORK/phrases.mid" --preset "$p" --out "$WORK/old.wav" --depth 32 --tail 2 >/dev/null 2>&1
  "$NEW" --midi "$WORK/phrases.mid" --preset "$p" --out "$WORK/new.wav" --depth 32 --tail 2 >/dev/null 2>&1
  if cmp -s "$WORK/old.wav" "$WORK/new.wav"; then echo "SAME $p"; else echo "DIFF $p"; fail=1; fi
done < "$WORK/presets.txt"
rm -rf "$WORK"
exit $fail
