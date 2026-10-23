#!/usr/bin/env bash
# Copy the core into a client repository, or check that a copy matches.
#
#   scripts/sync-core.sh <dest-dir>          copy core/ and port/esp32/ into <dest-dir>
#   scripts/sync-core.sh --check <dest-dir>  exit 1 when <dest-dir> differs
#
# Run it from a checkout of the core at the tag the client pins. The copy
# is plain files, never a submodule: the Arduino Library Manager and GitHub
# archives publish submodules as empty folders. <dest-dir> holds only the
# core's files plus CORE_VERSION, so anything else there is reported.
#
# Examples, from a client repository with the core checked out beside it:
#   ../SimpleOTAClient-core/scripts/sync-core.sh src/sota_core
#   ../SimpleOTAClient-core/scripts/sync-core.sh simpleota/src/core

set -euo pipefail

check=0
if [ "${1:-}" = "--check" ]; then
    check=1
    shift
fi
if [ $# -ne 1 ]; then
    sed -n '2,8p' "$0" >&2
    exit 2
fi
dest=$1
root=$(cd "$(dirname "$0")/.." && pwd)

# core/ and port/esp32/ land in one flat folder, so a client needs one
# include path (Arduino adds only src/ to it).
src=$(mktemp -d)
trap 'rm -rf "$src"' EXIT
cp "$root"/core/* "$root"/port/esp32/* "$src"/

version=$(git -C "$root" describe --tags --exact-match 2>/dev/null ||
          git -C "$root" rev-parse HEAD)

if [ $check -eq 1 ]; then
    status=0
    if ! diff -r --exclude=CORE_VERSION "$src" "$dest"; then
        status=1
    fi
    if [ "$(cat "$dest/CORE_VERSION" 2>/dev/null)" != "$version" ]; then
        echo "CORE_VERSION is '$(cat "$dest/CORE_VERSION" 2>/dev/null)', expected '$version'" >&2
        status=1
    fi
    [ $status -eq 0 ] && echo "$dest matches core $version"
    exit $status
fi

mkdir -p "$dest"
find "$dest" -mindepth 1 -maxdepth 1 ! -name CORE_VERSION -exec rm -rf {} +
cp -R "$src"/. "$dest"/
echo "$version" > "$dest/CORE_VERSION"
echo "copied core $version into $dest"
