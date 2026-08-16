#!/usr/bin/env bash
# If agent stops after MorphOS make/package issues, nudge follow-up or HARD STOP.
set -euo pipefail

MARKER_PENDING=".cursor/morphos-package-pending"
MARKER_FAILED=".cursor/morphos-package-failed"
MAKEFILE="Makefile.MorphOS"

find_repo() {
  local candidate
  for candidate in "$(pwd)" "$(pwd)/morphos/AmigaGPT"; do
    if [[ -f "$candidate/$MAKEFILE" ]]; then
      printf '%s\n' "$(cd "$candidate" && pwd)"
      return 0
    fi
  done
  return 1
}

repo=$(find_repo || true)
if [[ -z "$repo" ]]; then
  exit 0
fi

marker_failed="$repo/$MARKER_FAILED"
if [[ -f "$marker_failed" ]]; then
  detail=$(cat "$marker_failed" 2>/dev/null || true)
  rm -f "$marker_failed"
  printf '%s\n' '{"followup_message":"MORPHOS HARD STOP: packaging/deploy failed ('"$detail"'). Tell user to mount Z:. No commit/push until ship-morphos.sh succeeds with DEPLOY=1."}'
  exit 0
fi

marker_pending="$repo/$MARKER_PENDING"
if [[ -f "$marker_pending" ]]; then
  rm -f "$marker_pending"
  printf '%s\n' '{"followup_message":"MorphOS packaging still required: run bash ship-morphos.sh from morphos/AmigaGPT and report version, MD5, and Z: deploy path."}'
  exit 0
fi

exit 0
