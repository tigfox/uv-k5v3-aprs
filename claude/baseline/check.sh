#!/usr/bin/env bash
# Byte-identical guard: compare build/<Preset>/f4hwn.<preset>.bin with the saved baseline.
# __DATE__/__TIME__ (App/version.c) differ per build, so up to 24 differing bytes are allowed.
set -euo pipefail
cd "$(dirname "$0")/../.."
status=0
for p in Fusion Transfer FieldOps Labs Max; do
  s=$(echo "$p" | tr '[:upper:]' '[:lower:]')
  new="build/$p/f4hwn.$s.bin"; old="claude/baseline/f4hwn.$s.bin"
  if [[ ! -f "$new" ]]; then echo "$p: not built"; status=1; continue; fi
  if [[ $(wc -c <"$new") -ne $(wc -c <"$old") ]]; then echo "$p: SIZE DIFFERS"; status=1; continue; fi
  n=$( (cmp -l "$new" "$old" || true) | wc -l | tr -d ' ')
  if (( n <= 24 )); then echo "$p: OK ($n timestamp bytes differ)"; else echo "$p: DIFFERS ($n bytes)"; status=1; fi
done
exit $status
