#!/usr/bin/env bash
# MorphOS-only test package from native WSL cross-build (no Docker).
#
# Handlungsanweisung (Projekt): „Paketieren“ = LHA bauen **und** nach Z: deployen
# (nur AmigaGPT-MorphOS-cross.lha) — siehe docs/HANDLUNGSANWEISUNG-GIT.md.
# Entpacken/Install auf MorphOS: separates Deploy-Skript auf der MorphOS-Seite (nicht im Repo).
# Mit DEPLOY=0 nur lokal out/*.lha; Exit 0, aber kein abgeschlossenes Paketieren.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
STAGE="${PACKAGE_STAGE:-$ROOT/out/package-morphos}"
ARCHIVE_BASE="${PACKAGE_ARCHIVE:-$ROOT/out/AmigaGPT-MorphOS-cross}"
DEPLOY_WIN="${MORPHOS_DEPLOY:-Z:/morphos/out-crosscompile}"
BUILD="${BUILD:-1}"
FLEXCAT_BIN="${FLEXCAT_BIN:-$HOME/development/morphos/flexcat/src/bin_unix}"

export PATH="${FLEXCAT_BIN}:${PATH:-}"
log() { printf '==> %s\n' "$*"; }

if [[ "$BUILD" == "1" ]]; then
  log "Building AmigaGPT + daemon…"
  make -C "$ROOT" -f Makefile.MorphOS
  make -C "$ROOT" -f Makefile.MorphOS daemon
fi

for bin in AmigaGPT_MorphOS AmigaGPTD_MorphOS; do
  [[ -f "$ROOT/out/$bin" ]] || { echo "Missing out/$bin" >&2; exit 1; }
done

log "Staging: $STAGE"
rm -rf "$STAGE"
mkdir -p "$STAGE/AmigaGPT/AmigaGPT" "$STAGE/catalogs"

cp -f "$ROOT/out/AmigaGPT_MorphOS" "$ROOT/out/AmigaGPTD_MorphOS" "$STAGE/AmigaGPT/AmigaGPT/"
if [[ -f "$ROOT/bundle/AmigaGPT/AmigaGPT/AmigaGPT_MorphOS.info" ]]; then
  cp -f "$ROOT/bundle/AmigaGPT/AmigaGPT/AmigaGPT_MorphOS.info" "$STAGE/AmigaGPT/AmigaGPT/"
elif [[ -f "$ROOT/out/AmigaGPT_MorphOS.info" ]]; then
  cp -f "$ROOT/out/AmigaGPT_MorphOS.info" "$STAGE/AmigaGPT/AmigaGPT/"
fi

if [[ -d "$ROOT/bundle/AmigaGPT/AmigaGPT/rexx" ]]; then
  cp -a "$ROOT/bundle/AmigaGPT/AmigaGPT/rexx" "$ROOT/bundle/AmigaGPT/AmigaGPT/devs" "$STAGE/AmigaGPT/AmigaGPT/"
elif [[ -d "$ROOT/out/rexx" ]]; then
  cp -a "$ROOT/out/rexx" "$ROOT/out/devs" "$STAGE/AmigaGPT/AmigaGPT/"
fi

for cat in "$ROOT"/catalogs/*/*.catalog; do
  [[ -f "$cat" ]] || continue
  lang=$(basename "$(dirname "$cat")")
  mkdir -p "$STAGE/catalogs/$lang"
  cp -f "$cat" "$STAGE/catalogs/$lang/"
done

APP_VER="$(
  awk -F'"' '
    /#define APP_VERSION_MAJOR/ { m = $2 }
    /#define APP_VERSION_MINOR/ { n = $2 }
    /#define APP_VERSION_PATCH/ { p = $2 }
    END { if (m != "" && n != "" && p != "") print m "." n "." p }
  ' "$ROOT/src/version.h"
)"
BUILD_NUM="$(awk -F'"' '/#define BUILD_NUMBER/ { print $2; exit }' "$ROOT/src/version.h")"
PKG_SHA="$(sha256sum "$STAGE/AmigaGPT/AmigaGPT/AmigaGPT_MorphOS" | awk '{print $1}')"
PKG_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
GIT_REV="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
cat >"$STAGE/PACKAGE-BUILDINFO.txt" <<EOF
AmigaGPT MorphOS cross-package
==============================
version=${APP_VER:-?}
build=${BUILD_NUM:-?}
git=${GIT_REV}
packaged_utc=${PKG_UTC}
AmigaGPT_MorphOS.sha256=${PKG_SHA}

Prüfen auf MorphOS: diese Datei nach dem Entpacken lesen (nicht nur LHA-Datum
am Share — SMB/Copy-Item kann altes Erstellungsdatum behalten).
EOF
log "Buildinfo: ${APP_VER:-?} build ${BUILD_NUM:-?} (${PKG_UTC})"

cat >"$STAGE/AmigaGPT/AmigaGPT/fix-protection.rexx" <<'EOF'
/* Einmal nach Entpacken: AmigaDOS-Protection-Bits (fehlen nach LHA aus Linux/WSL). */
Address COMMAND
'protect' 'AmigaGPT_MorphOS' '+e'
'protect' 'AmigaGPTD_MorphOS' '+e'
'protect' 'rexx/#?' '+s'
Say 'Protection bits gesetzt: +e Executables, +s rexx/#?'
Exit 0
EOF

cat >"$STAGE/INSTALL-MORPHOS-CROSSBUILD.txt" <<'EOF'
AmigaGPT — MorphOS cross-build (Testpaket)
==========================================

WICHTIG — Protection Bits
-------------------------
LHA/Zip aus WSL/Linux enthalten KEINE Amiga-Protection-Bits (nur Unix-[generic]).
Nach dem Entpacken einmalig in AmigaGPT/AmigaGPT/:

  rx fix-protection.rexx

oder von Hand:

  protect AmigaGPT_MorphOS +e
  protect AmigaGPTD_MorphOS +e
  protect rexx/#? +s

Ohne +s koennen mitgelieferte ARexx-Skripte in rexx/ scheitern (+e fuer Workbench
oft optional — Shell/run funktioniert bei vielen Cross-Builds auch ohne fix-protection).
Die .info-Dateien (Icon, Stack) sind im Archiv und getrennt von den Protection Bits.

ARexx — erste Zeile MUSS /* Kommentar */ sein
--------------------------------------------
Laut AmigaOS-Handbuch (Elements of ARexx): Jedes Programm beginnt mit /* ... */.
Ohne diesen Kopf: rx meldet oft Fehler 5. fix-protection.rexx beginnt mit /* ... */.

Installation
------------
1. LHA entpacken (z.B. nach Work:AmigaGPT/), Ordner package-morphos/AmigaGPT/...
2. Optional in AmigaGPT/AmigaGPT/: rx fix-protection.rexx (vor allem fuer rexx/#?)
3. assign AMIGAGPT: Work:AmigaGPT/AmigaGPT/AmigaGPT  (Pfad nach eurem Entpacken)
4. Optional: run >nil: AMIGAGPT:AmigaGPTD_MorphOS
5. Start: run AMIGAGPT:AmigaGPT_MorphOS  (oder Doppelklick)

Share-Deploy (WSL): nur AmigaGPT-MorphOS-cross.lha nach morphos/out-crosscompile/
(z.B. HDSFGO4-share:morphos/out-crosscompile/). Entpacken: MorphOS-Deploy-Skript.

Version prüfen: PACKAGE-BUILDINFO.txt im Archiv (oder About: Version/Build).

Kataloge: catalogs/<sprache>/AmigaGPT.catalog → LOCALE:Catalogs/<sprache>/

Laufzeit: MUI, AmiSSL 5.x, codesets.library, guigfx, TCP/IP
Code-Viewer (Phase 6): Scintilla.mcc (MUI-Classes), ttengine.library (UTF-8/FreeType)
EOF

ARCHIVE=""
rm -f "${ARCHIVE_BASE}.lha" "${ARCHIVE_BASE}.zip"

if command -v lha >/dev/null 2>&1; then
  ARCHIVE="${ARCHIVE_BASE}.lha"
  log "Archive: $ARCHIVE (lha)"
  (cd "$(dirname "$STAGE")" && lha a "$ARCHIVE" "$(basename "$STAGE")")
elif command -v jlha >/dev/null 2>&1; then
  ARCHIVE="${ARCHIVE_BASE}.lha"
  log "Archive: $ARCHIVE (jlha)"
  (cd "$(dirname "$STAGE")" && jlha a "$ARCHIVE" "$(basename "$STAGE")")
elif command -v zip >/dev/null 2>&1; then
  ARCHIVE="${ARCHIVE_BASE}.zip"
  log "Archive: $ARCHIVE (zip)"
  (cd "$(dirname "$STAGE")" && zip -qr "$ARCHIVE" "$(basename "$STAGE")")
else
  log "Kein lha/jlha/zip — nur Staging-Ordner"
fi

DEPLOY_FAILED=0
if [[ "${DEPLOY:-1}" == "1" ]]; then
  if [[ -z "$ARCHIVE" ]]; then
    log "Fehler: DEPLOY=1, aber kein Archiv (lha/jlha/zip fehlt)."
    DEPLOY_FAILED=1
  elif command -v powershell.exe >/dev/null 2>&1; then
    WSL_ARCH=$(wslpath -w "$ARCHIVE")
    ARCH_NAME="$(basename "$ARCHIVE")"
    LOCAL_BYTES="$(wc -c <"$ARCHIVE" | tr -d ' ')"
    LOCAL_SHA="$(sha256sum "$ARCHIVE" | awk '{print $1}')"
    log "Deploy (nur LHA) → $DEPLOY_WIN/$ARCH_NAME (${LOCAL_BYTES} Bytes)"
    if ! powershell.exe -NoProfile -Command "
      \$ErrorActionPreference = 'Stop'
      \$d='${DEPLOY_WIN}'
      New-Item -ItemType Directory -Force -Path \$d | Out-Null
      \$arch = Join-Path \$d '${ARCH_NAME}'
      \$src = '${WSL_ARCH}'
      if (Test-Path \$arch) { Remove-Item -Force -LiteralPath \$arch }
      if (Test-Path \$arch) { throw \"Alte LHA konnte nicht gelöscht werden: \$arch\" }
      Copy-Item -LiteralPath \$src -Destination \$d -Force
      if (-not (Test-Path -LiteralPath \$arch)) { throw \"Kopie fehlgeschlagen: \$arch\" }
      \$dst = Get-Item -LiteralPath \$arch
      if (\$dst.Length -ne ${LOCAL_BYTES}) {
        throw \"Größe abweichend: Ziel=\$(\$dst.Length) erwartet=${LOCAL_BYTES}\"
      }
      \$h = (Get-FileHash -LiteralPath \$arch -Algorithm SHA256).Hash.ToLower()
      if (\$h -ne '${LOCAL_SHA}') {
        throw \"SHA256 abweichend auf Z: (erwartet ${LOCAL_SHA})\"
      }
      \$legacy = Join-Path \$d 'package-morphos'
      if (Test-Path \$legacy) { Remove-Item -Recurse -Force \$legacy }
      Write-Host \"LHA ersetzt OK: \$(\$dst.Name) \$(\$dst.Length) Bytes SHA256=\$h\"
      \$dst | Format-List Name,Length,LastWriteTime
    "; then
      log "Fehler: Deploy nach $DEPLOY_WIN fehlgeschlagen (Laufwerk nicht erreichbar?)."
      DEPLOY_FAILED=1
    else
      log "Z: LHA verifiziert (Größe + SHA256)."
    fi
  else
    log "Fehler: DEPLOY=1, aber powershell.exe nicht gefunden — kein Deploy nach Z: möglich."
    DEPLOY_FAILED=1
  fi
else
  log "DEPLOY=0 — kein automatischer Deploy."
fi

if [[ "$DEPLOY_FAILED" -ne 0 ]] || [[ "${DEPLOY:-1}" == "0" ]]; then
  log "Manuell auf MorphOS-Share (nur LHA):"
  [[ -n "$ARCHIVE" ]] && wslpath -w "$ARCHIVE" 2>/dev/null
  log "  Ziel: $DEPLOY_WIN/$(basename "${ARCHIVE:-AmigaGPT-MorphOS-cross.lha}")"
fi

log "Fertig: $STAGE"
[[ -n "$ARCHIVE" ]] && log "Archiv: $ARCHIVE"

if [[ "${DEPLOY:-1}" == "1" ]] && [[ "$DEPLOY_FAILED" -ne 0 ]]; then
  log "Abbruch mit Exit 1: Paketieren verlangt erfolgreiche Bereitstellung unter Z:."
  exit 1
fi
