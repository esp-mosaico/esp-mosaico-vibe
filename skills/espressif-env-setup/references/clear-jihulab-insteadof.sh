#!/usr/bin/env bash
# Clear jihulab url.*insteadOf only if present, then verify.
# See mirrors.md § clear-jihulab-insteadof: check → clear if any → verify

set -euo pipefail

keys="$(git config --global --get-regexp '^url\..*jihulab.*\.insteadof$' 2>/dev/null || true)"
if [ -z "$keys" ]; then
  echo "No jihulab url.*insteadOf entries found. Skip clear."
  exit 0
fi

while read -r key _; do
  [ -n "${key:-}" ] || continue
  git config --global --unset-all "$key" 2>/dev/null || true
  echo "Unset: $key"
done <<< "$keys"

git config --global --unset-all 'url.https://jihulab.com/esp-mirror/.insteadOf' 2>/dev/null || true

echo
echo "Remaining jihulab entries (should be empty):"
if git config --global --get-regexp jihulab 2>/dev/null; then
  :
else
  echo "(none)"
fi
