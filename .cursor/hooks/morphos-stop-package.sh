#!/usr/bin/env bash
# If agent stops after MorphOS make without package, nudge one follow-up.
set -euo pipefail

MARKER_NAME=".cursor/morphos-package-pending"
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

marker="$repo/$MARKER_NAME"
if [[ ! -f "$marker" ]]; then
  exit 0
fi

rm -f "$marker"
printf '%s\n' '{"followup_message":"MorphOS packaging is still required: run BUILD=0 bash package-morphos-cross.sh or bash ship-morphos.sh from morphos/AmigaGPT and report version, MD5, and Z: deploy path."}'
exit 0
