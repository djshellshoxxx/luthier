#!/bin/bash
# Verify a downloaded (or CI-built) release directory (installer.md 10, 13; IN-1, IN-47).
#
#   scripts/verify_release.sh <dir> [public-key.asc]
#
# <dir> holds the installers plus SHA256SUMS.txt (and SHA256SUMS.txt.asc, and
# per-installer *.asc for the Linux packages). Checks:
#   1. every file listed in SHA256SUMS.txt exists and matches its SHA-256;
#   2. every installer in the directory is listed (nothing unpublished slipped in);
#   3. with a public key: SHA256SUMS.txt.asc and every <file>.asc verify with gpg
#      (in a throw-away keyring, so your own keyring is untouched);
#      without one, a missing signature is reported as a warning, not a pass.
# Exit status: number of failures (warnings do not count).
set -uo pipefail
dir="${1:?usage: verify_release.sh <dir> [public-key.asc]}"
key="${2:-}"
cd "$dir" || exit 1
fail=0 warn=0
ok()   { echo "  ok    $*"; }
bad()  { echo "  FAIL  $*"; fail=$((fail + 1)); }
note() { echo "  warn  $*"; warn=$((warn + 1)); }

[ -s SHA256SUMS.txt ] || { echo "SHA256SUMS.txt is missing or empty"; exit 1; }

echo "1. checksums"
while read -r sum file; do
    file="${file#\*}"
    [ -n "$file" ] || continue
    if [ ! -e "$file" ]; then bad "$file is listed but missing"; continue; fi
    actual="$(sha256sum -- "$file" | cut -d' ' -f1)"
    [ "$actual" = "$sum" ] && ok "$file" || bad "$file does not match its checksum"
done < SHA256SUMS.txt

echo "2. every installer is listed"
for f in *; do
    case "$f" in SHA256SUMS.txt|*.asc|*.sha256) continue ;; esac
    [ -f "$f" ] || continue
    grep -qE "[ *]${f//./\\.}\$" SHA256SUMS.txt && ok "$f is listed" || bad "$f is not in SHA256SUMS.txt"
done

echo "3. signatures"
if [ -z "$key" ]; then
    note "no public key given: signatures were not checked"
elif ! command -v gpg >/dev/null; then
    note "gpg is not installed: signatures were not checked"
else
    export GNUPGHOME="$(mktemp -d)"; chmod 700 "$GNUPGHOME"
    trap 'rm -rf "$GNUPGHOME"' EXIT
    gpg --batch --quiet --import "$key" 2>/dev/null || { bad "could not import $key"; exit 1; }
    for sig in SHA256SUMS.txt.asc *.deb.asc *.rpm.asc *.tar.gz.asc; do
        [ -e "$sig" ] || continue
        gpg --batch --verify "$sig" "${sig%.asc}" >/dev/null 2>&1 && ok "$sig" || bad "$sig does not verify"
    done
    [ -e SHA256SUMS.txt.asc ] || bad "SHA256SUMS.txt.asc is missing"
fi

echo
echo "verify_release: $fail failure(s), $warn warning(s)"
exit $fail
