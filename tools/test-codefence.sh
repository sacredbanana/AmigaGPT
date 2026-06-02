#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make -f src/test/Makefile.codefence test
