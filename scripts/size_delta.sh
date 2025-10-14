#!/usr/bin/env bash
# size_delta.sh: Compare current build firmware size vs previous cached size.
# Usage: ./scripts/size_delta.sh <env> [--fail-delta=1.0] [--flash-threshold=92]
set -euo pipefail
ENV="${1:-nodemcu32}"
FAIL_DELTA=1.0
FLASH_THRESH=92
for arg in "$@"; do
  case "$arg" in
    --fail-delta=*) FAIL_DELTA="${arg#*=}" ;;
    --flash-threshold=*) FLASH_THRESH="${arg#*=}" ;;
  esac
done
CACHE_DIR=".sizecache"
mkdir -p "$CACHE_DIR"
OUT=$(pio run -e "$ENV" -t size 2>/dev/null | sed -n '1,120p') || { echo "Build failed" >&2; exit 2; }
FLASH_LINE=$(echo "$OUT" | grep -E "^Firmware\s+size" || true)
if [[ -z "$FLASH_LINE" ]]; then
  # Fallback parse for advanced summary
  FLASH_LINE=$(echo "$OUT" | grep -E "Flash:" | head -1 || true)
fi
if [[ -z "$FLASH_LINE" ]]; then
  echo "Could not parse size output" >&2; exit 3;
fi
# Extract numeric flash percent from advanced line if present
FLASH_PCT=$(echo "$OUT" | grep -E "Flash:" | head -1 | sed -E 's/.*Flash:[[:space:]]*([0-9]+\.[0-9]+)%.*/\1/' || true)
if [[ -z "$FLASH_PCT" ]]; then FLASH_PCT=0; fi
# Extract .elf path
ELF=$(echo "$OUT" | grep -Eo '/[^ ]+\.elf' | head -1)
if [[ -z "$ELF" ]]; then echo "ELF not found" >&2; exit 4; fi
CURRENT_SIZE=$(stat -c %s "$ELF")
CACHE_FILE="$CACHE_DIR/$ENV.size"
PREV_SIZE=0
if [[ -f "$CACHE_FILE" ]]; then PREV_SIZE=$(cat "$CACHE_FILE"); fi
DELTA=$(( CURRENT_SIZE - PREV_SIZE ))
DELTA_ABS=${DELTA#-}
if [[ $PREV_SIZE -gt 0 ]]; then
  DELTA_PCT=$(awk -v d=$DELTA_ABS -v p=$PREV_SIZE 'BEGIN{ if(p==0){print 0}else{printf("%.2f", (d/p)*100)} }')
else
  DELTA_PCT=0
fi
printf "Env: %s\n" "$ENV"
printf "Current ELF bytes: %s\n" "$CURRENT_SIZE"
printf "Previous ELF bytes: %s\n" "$PREV_SIZE"
printf "Delta bytes: %s (%s%%)\n" "$DELTA" "$DELTA_PCT"
printf "Flash usage: %s%% (threshold %s%%)\n" "$FLASH_PCT" "$FLASH_THRESH"
# Store current size
echo "$CURRENT_SIZE" > "$CACHE_FILE"
EXIT=0
awk -v p=$FLASH_PCT -v t=$FLASH_THRESH 'BEGIN{ if(p>t){ exit 5 } }' || EXIT=5
awk -v d=$DELTA_PCT -v f=$FAIL_DELTA 'BEGIN{ if(d>f){ exit 6 } }' || { [[ $PREV_SIZE -gt 0 ]] && EXIT=6 || true; }
if [[ $EXIT -ne 0 ]]; then
  echo "FAIL: Threshold exceeded (code $EXIT)" >&2
fi
exit $EXIT
