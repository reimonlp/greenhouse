#!/usr/bin/env bash
set -euo pipefail

# CI size guard: builds firmware and enforces flash usage & delta thresholds.
# Exits non-zero on violation.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR"

echo "[ci_size_guard] Building release (default env)" >&2
platformio run -e release >/dev/null

ELF=$(find .pio/build -name '*.elf' | head -n1)
if [[ -z "$ELF" ]]; then
  echo "[ci_size_guard] ERROR: ELF not found" >&2
  exit 2
fi

SIZE_LINE=$(xtensa-esp32-elf-size "$ELF" | tail -n1)
TEXT=$(echo "$SIZE_LINE" | awk '{print $1}')
DATA=$(echo "$SIZE_LINE" | awk '{print $2}')
BSS=$(echo "$SIZE_LINE" | awk '{print $3}')
FLASH=$((TEXT + DATA))
FLASH_LIMIT=$(( 1310720 * 92 / 100 )) # 92% of 1.25MB partition example

echo "[ci_size_guard] Flash usage: $FLASH bytes (limit $FLASH_LIMIT)" >&2
if (( FLASH > FLASH_LIMIT )); then
  echo "[ci_size_guard] FAIL: flash usage above 92% budget" >&2
  exit 3
fi

# Optional: compare against cached previous size if provided via env PREV_FLASH
if [[ -n "${PREV_FLASH:-}" ]]; then
  DIFF=$(( FLASH - PREV_FLASH ))
  PCT=$(( DIFF * 100 / (PREV_FLASH == 0 ? 1 : PREV_FLASH) ))
  if (( PCT > 1 )); then
     echo "[ci_size_guard] FAIL: flash grew by >1% (diff=$DIFF bytes, ${PCT}%)" >&2
     exit 4
  fi
  echo "[ci_size_guard] Delta vs previous: $DIFF bytes (${PCT}%)" >&2
fi

echo "[ci_size_guard] PASS" >&2