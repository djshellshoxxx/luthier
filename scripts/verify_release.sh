#!/bin/bash
# Verify a downloaded release directory without installing anything.
# Usage: verify_release.sh <dir> [public-key.asc]
# Accepts SHA256SUMS.txt, SHA256SUMS and per-archive *.SHA256SUMS manifests.
# Every manifest is checked; each must be nonempty and contain strict GNU-style
# SHA-256 records. Paths must be relative, without traversal or symlink targets.
# Every top-level payload must be listed. With a key, each checksum manifest
# must have a valid signature, or the legacy SHA256SUMS must be fully covered by
# a validated signed manifest.txt summary. Supplied Linux package signatures
# also verify in an isolated keyring.
# Missing optional signatures without a key are warnings, never verified claims.
set -uo pipefail
dir="${1:?usage: verify_release.sh <dir> [public-key.asc]}"
key="${2:-}"
if [ -n "$key" ]; then
    key_dir="$(cd "$(dirname "$key")" && pwd -P)" || exit 1
    key="$key_dir/$(basename "$key")"
fi
cd "$dir" || exit 1
fail=0 warn=0
ok()   { echo "  ok    $*"; }
bad()  { echo "  FAIL  $*"; fail=$((fail + 1)); }
note() { echo "  warn  $*"; warn=$((warn + 1)); }
contains() { local wanted="$1" entry; shift; for entry in "$@"; do [ "$entry" != "$wanted" ] || return 0; done; return 1; }

shopt -s nullglob dotglob
manifests=()
for candidate in SHA256SUMS.txt SHA256SUMS *.SHA256SUMS; do
    if [ -e "$candidate" ] || [ -L "$candidate" ]; then manifests+=("$candidate"); fi
done
[ ${#manifests[@]} -gt 0 ] || { echo "No supported SHA256SUMS manifest found"; exit 1; }
listed=() hashes=()
echo "1. checksums"
for manifest in "${manifests[@]}"; do
    if [ ! -s "$manifest" ] || [ ! -f "$manifest" ] || [ -L "$manifest" ]; then bad "$manifest is empty or not a regular manifest"; continue; fi
    records=0 seen=() line_number=0
    while IFS= read -r line || [ -n "$line" ]; do
        line_number=$((line_number + 1)); line="${line%$'\r'}"
        sum="${line:0:64}"; marker="${line:64:2}"; file="${line:66}"
        if [ ${#line} -lt 67 ] || ! [[ "$sum" =~ ^[[:xdigit:]]{64}$ ]] || { [ "$marker" != '  ' ] && [ "$marker" != ' *' ]; }; then
            bad "$manifest:$line_number is not a SHA-256 record"; continue
        fi
        case "$file" in /*|*\\*|.|..|./*|../*|*/./*|*/../*|*/.|*/..|*//*|*/)
            bad "$manifest:$line_number has an unsafe path"; continue ;;
        esac
        if [[ "$file" =~ [[:cntrl:]] ]]; then bad "$manifest:$line_number has a control character in its path"; continue; fi
        records=$((records + 1))
        if contains "$file" ${seen[@]+"${seen[@]}"}; then bad "$file is duplicated in $manifest"; continue; fi
        seen+=("$file")
        sum="$(printf '%s' "$sum" | tr 'A-F' 'a-f')"
        for ((index=0;index<${#listed[@]};++index)); do
            if [ "${listed[index]}" = "$file" ] && [ "${hashes[index]}" != "$sum" ]; then bad "$file has conflicting manifest hashes"; fi
        done
        listed+=("$file"); hashes+=("$sum")
        # Reject symlinks at every component, including a dangling final link.
        IFS=/ read -r -a components <<< "$file"
        prefix=""; linked=0
        for component in "${components[@]}"; do
            prefix="${prefix:+$prefix/}$component"
            if [ -L "$prefix" ]; then linked=1; break; fi
        done
        if [ "$linked" = 1 ]; then bad "$file traverses a symlink"; continue; fi
        if [ ! -f "$file" ]; then bad "$file is listed but missing or not a regular file"; continue; fi
        if ! actual="$(sha256sum -- "$file")"; then bad "$file could not be hashed"; continue; fi
        actual="${actual:0:64}"
        [ "$actual" = "$sum" ] && ok "$file" || bad "$file does not match its checksum"
    done < "$manifest"
    [ "$records" -gt 0 ] || bad "$manifest has no checksum records"
done

# release_manifest.sh historically emits an unsigned checksum file plus a
# separately signable manifest.txt summary. Accept that metadata only after its
# hashes, byte sizes and complete file set agree with the verified records.
summary_valid=0
if [ -e manifest.txt ] || [ -L manifest.txt ]; then
    if [ "$fail" -ne 0 ] || [ ! -s manifest.txt ] || [ ! -f manifest.txt ] || [ -L manifest.txt ]; then
        bad "manifest.txt cannot be validated against the checksum records"
    else
        summary_start_fail=$fail; summary_files=()
        { IFS= read -r summary_header; IFS= read -r summary_columns; } < manifest.txt
        [[ "$summary_header" == Luthier\ *\ release\ manifest ]] || bad "manifest.txt has an invalid header"
        [ "$summary_columns" = '# sha256  bytes  file' ] || bad "manifest.txt has invalid columns"
        summary_pattern='^([[:xdigit:]]{64})  ([0-9]+)  (.+)$'
        while IFS= read -r record || [ -n "$record" ]; do
            if ! [[ "$record" =~ $summary_pattern ]]; then bad "manifest.txt has an invalid record"; continue; fi
            summary_hash="$(printf '%s' "${BASH_REMATCH[1]}" | tr 'A-F' 'a-f')"
            summary_size="${BASH_REMATCH[2]}"; summary_file="${BASH_REMATCH[3]}"
            if ! contains "$summary_file" ${listed[@]+"${listed[@]}"} || contains "$summary_file" ${summary_files[@]+"${summary_files[@]}"}; then
                bad "manifest.txt has an unlisted or duplicate file"; continue
            fi
            summary_files+=("$summary_file")
            for ((index=0;index<${#listed[@]};++index)); do
                if [ "${listed[index]}" = "$summary_file" ] && [ "${hashes[index]}" != "$summary_hash" ]; then bad "manifest.txt has a conflicting hash"; fi
            done
            actual_size="$(wc -c < "$summary_file" | tr -d '[:space:]')"
            [ "$actual_size" = "$summary_size" ] || bad "manifest.txt has an incorrect byte size"
        done < <(tail -n +3 manifest.txt)
        for file in ${listed[@]+"${listed[@]}"}; do contains "$file" ${summary_files[@]+"${summary_files[@]}"} || bad "manifest.txt does not cover $file"; done
        [ "$fail" -ne "$summary_start_fail" ] || summary_valid=1
    fi
fi

echo "2. every top-level payload is listed"
for file in *; do
    contains "$file" "${manifests[@]}" && continue
    [ "$file" != manifest.txt ] || [ "$summary_valid" != 1 ] || continue
    case "$file" in *.asc|*.sha256) continue ;; esac
    [ -f "$file" ] || [ -L "$file" ] || continue
    contains "$file" ${listed[@]+"${listed[@]}"} && ok "$file is listed" || bad "$file is not in a checksum manifest"
done

echo "3. signatures"
if [ -z "$key" ]; then
    note "no public key given: signatures were not checked"
elif ! command -v gpg >/dev/null; then
    bad "gpg is not installed: requested signature verification cannot run"
else
    release_keyring="$(mktemp -d)" || exit 1
    chmod 700 "$release_keyring"
    trap 'rm -rf "$release_keyring"' EXIT
    gpg --homedir "$release_keyring" --batch --quiet --import "$key" 2>/dev/null || { bad "could not import public key"; exit 1; }
    signatures=()
    for manifest in "${manifests[@]}"; do
        if [ -f "$manifest.asc" ]; then
            signatures+=("$manifest.asc")
        elif [ "$manifest" = SHA256SUMS ] && [ "$summary_valid" = 1 ] && [ -f manifest.txt.asc ]; then
            signatures+=(manifest.txt.asc)
        else
            bad "$manifest.asc is missing"
        fi
    done
    for signature in *.deb.asc *.rpm.asc *.tar.gz.asc; do signatures+=("$signature"); done
    for signature in ${signatures[@]+"${signatures[@]}"}; do
        gpg --homedir "$release_keyring" --batch --verify "$signature" "${signature%.asc}" >/dev/null 2>&1 && ok "$signature" || bad "$signature does not verify"
    done
fi

echo
echo "verify_release: $fail failure(s), $warn warning(s)"
[ "$fail" -eq 0 ]
