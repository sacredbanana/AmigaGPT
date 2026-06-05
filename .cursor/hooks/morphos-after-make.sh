#!/usr/bin/env bash
# After MorphOS make: remind agent to package in the same turn.
set -euo pipefail

input=$(cat)
command=""
exit_code=1

if [[ "$input" =~ \"command\"[[:space:]]*:[[:space:]]*\"([^\"]+)\" ]]; then
  command="${BASH_REMATCH[1]}"
fi
if [[ "$input" =~ \"exit_code\"[[:space:]]*:[[:space:]]*([0-9]+) ]]; then
  exit_code="${BASH_REMATCH[1]}"
elif [[ "$input" =~ \"exitCode\"[[:space:]]*:[[:space:]]*([0-9]+) ]]; then
  exit_code="${BASH_REMATCH[1]}"
fi

MARKER_NAME=".cursor/morphos-package-pending"
MAKEFILE="Makefile.MorphOS"

find_repo() {
  local candidate
  if [[ "$command" =~ -C[[:space:]]+([^[:space:]]+) ]]; then
    candidate="${BASH_REMATCH[1]}"
    candidate="${candidate%\'}"
    candidate="${candidate#\'}"
    if [[ -f "$candidate/$MAKEFILE" ]]; then
      printf '%s\n' "$(cd "$candidate" && pwd)"
      return 0
    fi
  fi
  for candidate in "$(pwd)" "$(pwd)/morphos/AmigaGPT"; do
    if [[ -f "$candidate/$MAKEFILE" ]]; then
      printf '%s\n' "$(cd "$candidate" && pwd)"
      return 0
    fi
  done
  return 1
}

is_package_command() {
  [[ "$command" == *package-morphos-cross* ]] && return 0
  [[ "$command" == *ship-morphos* ]] && return 0
  [[ "$command" == *Makefile.MorphOS* && "$command" == *make* && "$command" == *ship* ]] && return 0
  return 1
}

is_morphos_make_without_ship() {
  [[ "$command" == *"$MAKEFILE"* ]] || return 1
  [[ "$command" == *make* ]] || return 1
  [[ "$command" == *ship* ]] && return 1
  return 0
}

repo=$(find_repo || true)
if [[ -z "$repo" ]]; then
  exit 0
fi

marker="$repo/$MARKER_NAME"

if is_package_command && [[ "$exit_code" -eq 0 ]]; then
  rm -f "$marker"
  exit 0
fi

if is_morphos_make_without_ship && [[ "$exit_code" -eq 0 ]]; then
  mkdir -p "$(dirname "$marker")"
  printf '%s\n' "$command" >"$marker"
  printf '%s\n' '{"additional_context":"MORPHOS DELIVERY REQUIRED: make -f Makefile.MorphOS ran without packaging. In this same response run BUILD=0 bash package-morphos-cross.sh or bash ship-morphos.sh and report version + MD5 + Z:/morphos/out-crosscompile/AmigaGPT-MorphOS-cross.lha. Do not end the turn until packaging succeeded or deploy failed with exit code."}'
  exit 0
fi

exit 0
