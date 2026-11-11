#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Build the PlatformIO package for a tagged release from a CLEAN export of that
# tag and refuse to continue unless it is exactly what the tag contains.
#
#   tools/release/check_package.sh vX.Y.Z [OUTPUT_DIR]
#
# Why this exists: `pio pkg pack` and `pio pkg publish` package the files on
# disk, not what git tracks. Run in a working folder they would ship ignored
# files (bench captures, notes, local config). This script never packs the
# working folder: it exports the tag into an empty directory, packs that, and
# then checks the result file-for-file against the tag.
#
# On success it prints the path of the verified tarball as its last line.
set -euo pipefail

TAG="${1:?usage: check_package.sh vX.Y.Z [OUTPUT_DIR]}"
OUT="${2:-dist}"
PIO="${PIO:-pio}"
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

fail() { echo "CHECK FAILED: $*" >&2; exit 1; }

[[ "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "tag '$TAG' is not vMAJOR.MINOR.PATCH"
git rev-parse -q --verify "refs/tags/$TAG" >/dev/null || fail "tag $TAG does not exist"
[[ "$(git cat-file -t "refs/tags/$TAG")" == "tag" ]] || fail "$TAG is not an annotated tag"
VERSION="${TAG#v}"

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/clean" "$WORK/unpacked"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"

# 1. Clean export: only files committed at the tag can be in here.
git archive "$TAG" | tar -x -C "$WORK/clean"

# 2. The manifests must carry the tag's version.
grep -q "\"version\": \"$VERSION\"" "$WORK/clean/library.json" \
  || fail "library.json version is not $VERSION"
grep -q "^version=$VERSION\$" "$WORK/clean/library.properties" \
  || fail "library.properties version is not $VERSION"

# 3. Pack from the clean export.
( cd "$WORK/clean" && "$PIO" pkg pack -o "$OUT/" . >/dev/null )
TARBALL="$OUT/ESP32-C5-BSP-$VERSION.tar.gz"
[[ -f "$TARBALL" ]] || fail "expected $TARBALL was not produced"

# 4. Every packed file must exist in the tag and be byte-identical to it.
tar -xzf "$TARBALL" -C "$WORK/unpacked"
count=0
while IFS= read -r -d '' f; do
  rel="${f#"$WORK/unpacked/"}"
  git cat-file -e "$TAG:$rel" 2>/dev/null || fail "packed file is not in the tag: $rel"
  git show "$TAG:$rel" | cmp -s - "$f" || fail "packed file differs from the tag: $rel"
  count=$((count + 1))
done < <(find "$WORK/unpacked" -type f -print0)
[[ "$count" -gt 0 ]] || fail "package is empty"

# 5. Generic content scan. Belt and braces: the content is already public in
#    the tag, but a hit here means something got committed that should not be.
scan() {  # scan "label" 'extended-regex'
  local hits
  hits="$(grep -rIilE -- "$2" "$WORK/unpacked" || true)"
  [[ -z "$hits" ]] || fail "$1 found in: $(echo "$hits" | sed "s|$WORK/unpacked/||" | head -5 | tr '\n' ' ')"
}
# (Written with bracketed letters so this file does not itself look like a
#  personal path to scanners that read the repository.)
scan "home-directory path"      '/[U]sers/[a-z]|/[h]ome/[a-z]+/|C:\\[U]sers\\'
scan "email address"            '[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[a-z]{2,}'
scan "full MAC address"         '([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}'
scan "GPS position (decimal)"   '-?[0-9]{1,3}\.[0-9]{4,}[[:space:]]*,[[:space:]]*-?[0-9]{1,3}\.[0-9]{4,}'
scan "GPS position (NMEA)"      '[0-9]{4,5}\.[0-9]{3,},[NSEW]'
scan "attribution trailer"      'co-authored-by|signed-off-by'
scan "generated-by notice"      'generated (with|by) '

echo "verified: $count files, all present in $TAG and byte-identical" >&2
echo "$TARBALL"
