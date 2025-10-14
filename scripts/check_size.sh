#!/usr/bin/env bash
# Simple size guard for PlatformIO builds.
# Usage: scripts/check_size.sh .pio/build/nodemcu32/firmware.elf
# Parses size output from PlatformIO. Works with explicit size table lines.

set -euo pipefail

ENV=${1:-}
FLASH_LIMIT_PCT=${FLASH_LIMIT_PCT:-92}
RAM_LIMIT_PCT=${RAM_LIMIT_PCT:-85}

if [[ -z "$ENV" ]]; then
  echo "Usage: $0 <environment|path-to-elf>" >&2
  exit 1
fi

SIZE_OUTPUT=$(platformio run -e "$ENV" -t size 2>/dev/null || true)

# Extract the size table section
LINE=$(echo "$SIZE_OUTPUT" | awk '/filename$/ {getline; print}' | grep firmware.elf || true)
if [[ -z "$LINE" ]]; then
  # fallback: scan all lines for pattern
  LINE=$(echo "$SIZE_OUTPUT" | grep -E '[[:space:]]+[0-9]+[[:space:]]+[0-9]+[[:space:]]+[0-9]+[[:space:]]+[0-9]+[[:space:]]+[0-9a-fA-F]+[[:space:]]+.*firmware\.elf' | tail -n1 || true)
fi
if [[ -z "$LINE" ]]; then
  echo "Could not parse size table" >&2
  exit 2
fi
# shellcheck disable=SC2206
PARTS=($LINE)
TEXT=${PARTS[0]}
DATA=${PARTS[1]}
BSS=${PARTS[2]}
FLASH_USED=$((TEXT + DATA))
FLASH_TOTAL=1310720
FLASH_PCT=$(awk -v u=$FLASH_USED -v t=$FLASH_TOTAL 'BEGIN{printf("%.1f", (u/t)*100)}')
RAM_USED=$((DATA + BSS))
RAM_TOTAL=327680
RAM_PCT=$(awk -v u=$RAM_USED -v t=$RAM_TOTAL 'BEGIN{printf("%.1f", (u/t)*100)}')

warn=0
if (( ${FLASH_PCT%.*} > FLASH_LIMIT_PCT )); then
  echo "WARNING: Flash usage ${FLASH_PCT}% exceeds limit ${FLASH_LIMIT_PCT}%" >&2
  warn=1
fi
if (( ${RAM_PCT%.*} > RAM_LIMIT_PCT )); then
  echo "WARNING: RAM usage ${RAM_PCT}% exceeds limit ${RAM_LIMIT_PCT}%" >&2
  warn=1
fi

echo "Flash: ${FLASH_PCT}%  RAM: ${RAM_PCT}% (limits Flash ${FLASH_LIMIT_PCT}%, RAM ${RAM_LIMIT_PCT}%)"
exit $warn
