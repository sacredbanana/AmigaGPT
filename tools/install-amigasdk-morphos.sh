#!/usr/bin/env bash
# Merge AmigaSDK-gcc morphos/sdk into /opt/amiga (Plan A native cross-build).
set -euo pipefail

LOG="${1:-$HOME/development/morphos/AmigaGPT/build-install.log}"
exec > >(tee -a "$LOG") 2>&1

echo "=== $(date -Iseconds) install-amigasdk-morphos ==="

MORPHOS_DIR="${MORPHOS_DIR:-$HOME/development/morphos}"
SDK_REPO="$MORPHOS_DIR/AmigaSDK-gcc"
AMIGAGPT_DIR="${AMIGAGPT_DIR:-$HOME/development/morphos/AmigaGPT}"

mkdir -p "$MORPHOS_DIR"
if [[ ! -d "$SDK_REPO/.git" ]]; then
  git clone --depth 1 https://github.com/sacredbanana/AmigaSDK-gcc.git "$SDK_REPO"
fi

cd "$SDK_REPO"
git pull --depth 1 2>/dev/null || true

echo "Merging morphos/sdk/ -> /opt/amiga/"
sudo cp -a morphos/sdk/. /opt/amiga/

echo "Checks:"
test -f /opt/amiga/ppc-morphos/include/SDI_hook.h && echo "  SDI_hook.h: OK" || echo "  SDI_hook.h: MISSING"
ls /opt/amiga/lib/libjson-c* 2>/dev/null | head -3 || echo "  libjson-c: (check lib/ manually)"

export PATH="$HOME/development/morphos/flexcat/src/bin_unix:${PATH:-}"
cd "$AMIGAGPT_DIR"
echo "Building AmigaGPT (Makefile.MorphOS)..."
make -f Makefile.MorphOS
echo "=== Build OK: $AMIGAGPT_DIR/out/AmigaGPT_MorphOS ==="
