#!/usr/bin/env bash
# Small helper to find TODO/FIXME/HACK comments and large commented blocks
# Usage: ./scripts/find_orphan_comments.sh

set -euo pipefail

ROOT_DIR="$(dirname "${BASH_SOURCE[0]}")/.."
cd "$ROOT_DIR"

echo "Searching for TODO/FIXME/HACK/XXX markers..."
grep -RIn --exclude-dir=.pio --exclude-dir=.git --exclude=*.bin -E "TODO|FIXME|HACK|XXX|@deprecated" || true

echo
echo "Searching for large block comments (/* ... */) files with more than 5 lines inside..."
# Find files containing /* and */ and print the file and number of comment lines
for f in $(grep -RIl --exclude-dir=.pio --exclude-dir=.git "\/\*" || true); do
  count=$(sed -n '/\/\*/,/\*\//p' "$f" | wc -l | tr -d ' ')
  if [ "$count" -ge 5 ]; then
    echo "Large block comment in $f -> $count lines"
  fi
done

echo
echo "Searching for lines commented-out that look like code (e.g., // if (, // return ) ... )"
grep -RIn --exclude-dir=.pio --exclude-dir=.git -E "^\s*//\s*(if\s*\(|for\s*\(|while\s*\(|return\s+|#include|int\s+|void\s+)" || true

exit 0
