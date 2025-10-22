#!/usr/bin/env bash
# Simple health check script for the greenhouse firmware.
# Usage:
#   scripts/health_check.sh <host> [--heap <token>] [--timeout 3]
# Examples:
#   scripts/health_check.sh 192.168.1.120
#   scripts/health_check.sh 192.168.1.120 --heap my_token
#   scripts/health_check.sh esp32.local --timeout 5 --heap $TOKEN

set -euo pipefail
HOST=${1:-}
shift || true
TOKEN=""
TIMEOUT=3
DO_HEAP=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --heap)
      DO_HEAP=1; TOKEN=${2:-}; shift 2;;
    --timeout)
      TIMEOUT=${2:-3}; shift 2;;
    *) echo "Unknown arg: $1" >&2; exit 2;;
  esac
done

if [[ -z "$HOST" ]]; then
  echo "Usage: $0 <host> [--heap <token>] [--timeout N]" >&2
  exit 1
fi

base_url="http://$HOST"

http_code=$(curl -s -o /dev/null -w '%{http_code}' --max-time "$TIMEOUT" "$base_url/api/healthz" || echo 000)
if [[ "$http_code" != "200" ]]; then
  echo "HEALTHZ_FAIL code=$http_code host=$HOST"; exit 1
fi

echo "HEALTHZ_OK host=$HOST"

if [[ $DO_HEAP -eq 1 ]]; then
  if [[ -z "$TOKEN" ]]; then
    echo "--heap specified but token empty" >&2; exit 2
  fi
  heap_json=$(curl -s --max-time "$TIMEOUT" -H "Authorization: Bearer $TOKEN" "$base_url/api/system/heap" || true)
  if [[ -z "$heap_json" ]]; then
    echo "HEAP_FAIL empty_response"; exit 3
  fi
  # Extract free_heap with lightweight parsing
  free_heap=$(echo "$heap_json" | sed -n 's/.*"free_heap":\([0-9]\+\).*/\1/p')
  if [[ -n "$free_heap" ]]; then
    echo "HEAP_OK free_heap=$free_heap"
  else
    echo "HEAP_WARN parse_error raw=$heap_json"
  fi
fi
