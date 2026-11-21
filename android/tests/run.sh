#!/bin/bash
# Alla prov för Android-appar (inget Android behövs): bash tests/run.sh
cd "$(dirname "$0")" || exit 1
fail=0
for t in test-*.py; do
  out=$(python3 "$t" 2>&1) || { fail=1; echo "$out" | grep -v '^  ok'; }
  echo "$out" | tail -1
done
exit $fail
