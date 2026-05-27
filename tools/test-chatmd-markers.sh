#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../src/test"
make -f Makefile.chatmd_markers test
