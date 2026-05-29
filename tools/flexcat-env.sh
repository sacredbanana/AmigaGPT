#!/usr/bin/env bash
# FlexCat for AmigaGPT catalog build (morphos/flexcat sibling of AmigaGPT).
# Source:  . tools/flexcat-env.sh
set -euo pipefail

_amigagpt_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
_morphos_root="$(cd "$_amigagpt_root/.." && pwd)"

export FLEXCAT_ROOT="${FLEXCAT_ROOT:-$_morphos_root/flexcat}"
export FLEXCAT_BIN="${FLEXCAT_BIN:-$FLEXCAT_ROOT/src/bin_unix}"
export FLEXCAT="$FLEXCAT_BIN/flexcat"

if [[ ! -x "$FLEXCAT" ]]; then
  echo "flexcat missing: $FLEXCAT" >&2
  echo "Build: cd \"$FLEXCAT_ROOT\" && make bootstrap && make" >&2
  exit 1
fi

export PATH="$FLEXCAT_BIN:${PATH:-}"

for _sd in C_h.sd C_c.sd; do
  _target="$FLEXCAT_ROOT/src/sd/$_sd"
  _link="$_amigagpt_root/$_sd"
  if [[ -f "$_target" ]]; then
    if [[ ! -e "$_link" ]] || [[ "$(readlink -f "$_link" 2>/dev/null || readlink "$_link")" != "$_target" ]]; then
      ln -sf "$_target" "$_link"
    fi
  fi
done
