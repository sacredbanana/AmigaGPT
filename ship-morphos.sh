#!/usr/bin/env bash
# make + daemon + package-morphos-cross.sh — default MorphOS delivery (see .cursor/rules).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=tools/flexcat-env.sh
source "$ROOT/tools/flexcat-env.sh"
log() { printf '==> %s\n' "$*"; }

log "flexcat: $FLEXCAT"
log "make ship (build + package + Z:)…"
make -C "$ROOT" -f Makefile.MorphOS ship "$@"
