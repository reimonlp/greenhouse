#!/usr/bin/env bash
# Aggregates key health statistics into one CSV line (header emitted with -H)
set -euo pipefail
API_URL=${API_URL:-http://localhost:80}
TOKEN=${API_TOKEN:-}
HEADER=0
while getopts "H" opt; do [[ $opt == H ]] && HEADER=1; done
curl_auth(){
  if [[ -n "$TOKEN" ]]; then curl -s -H "Authorization: Bearer $TOKEN" "$1"; else curl -s "$1"; fi
}
health_json=$(curl_auth "$API_URL/api/system/health" || echo '{}')
heap_json=$(curl_auth "$API_URL/api/system/heap" || echo '{}')
rules_json=$(curl_auth "$API_URL/api/system/rules" || echo '{}')
# naive extraction with grep/sed (avoid jq dependency)
extract(){ echo "$1" | sed -n "s/.*\"$2\":\([0-9.]*\).*/\1/p" | head -n1; }
up=$(extract "$health_json" uptime)
free_heap=$(extract "$heap_json" free_heap)
loop_avg=$(extract "$health_json" loop_avg_us)
# sum rule eval_hour
rule_eval_hour=$(echo "$rules_json" | grep -o '"eval_hour":[0-9]*' | sed 's/.*://' | paste -sd+ - | bc 2>/dev/null || echo 0)
rule_act_hour=$(echo "$rules_json" | grep -o '"act_hour":[0-9]*' | sed 's/.*://' | paste -sd+ - | bc 2>/dev/null || echo 0)
if (( HEADER )); then echo "uptime_ms,free_heap,loop_avg_us,rule_eval_hour,rule_act_hour"; fi
echo "$up,$free_heap,$loop_avg,$rule_eval_hour,$rule_act_hour"
