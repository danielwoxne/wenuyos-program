#!/bin/bash
# Alla prov (Retrospel behövs: WENUYOS_RETRO=<sökväg till wenuyos-retro> eller /usr/bin/wenuyos-retro): bash tests/run.sh
cd "$(dirname "$0")" || exit 1
fail=0
for t in test-*.py; do
  out=$(python3 "$t" 2>&1); rc=$?
  [ $rc = 77 ] && { echo "$out" | tail -1; continue; }
  [ $rc != 0 ] && { fail=1; echo "$out" | grep -v '^  ok'; }
  echo "$out" | tail -1
done
exit $fail
