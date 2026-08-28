#!/usr/bin/env bash
# MorphOS-only test package from native WSL cross-build (no Docker).
#
# Handlungsanweisung (Projekt): „Paketieren“ = LHA bauen **und** nach Z: deployen
# (nur AmigaGPT-MorphOS-cross.lha) — siehe docs/HANDLUNGSANWEISUNG-GIT.md.
# Entpacken/Install auf MorphOS: separates Deploy-Skript auf der MorphOS-Seite (nicht im Repo).
# Mit DEPLOY=0 nur lokal out/*.lha; Exit 0, aber kein abgeschlossenes Paketieren.
#
# TODO (optional): Deploy alternativ per smbclient aus WSL (//hdsfgo4/share/…, Credentials
# per Env) — siehe docs/WSL-SETUP-STATUS.md und HANDLUNGSANWEISUNG-GIT.md §8.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
STAGE="${PACKAGE_STAGE:-$ROOT/out/package-morphos}"
ARCHIVE_BASE="${PACKAGE_ARCHIVE:-$ROOT/out/AmigaGPT-MorphOS-cross}"
DEPLOY_WIN="${MORPHOS_DEPLOY:-Z:/morphos/out-crosscompile}"
BUILD="${BUILD:-1}"
# shellcheck source=tools/flexcat-env.sh
source "$ROOT/tools/flexcat-env.sh"
log() { printf '==> %s\n' "$*"; }

file_md5() {
  if command -v md5sum >/dev/null 2>&1; then
    md5sum "$1" | awk '{print $1}'
  elif command -v md5 >/dev/null 2>&1; then
    md5 -q "$1"
  else
    openssl md5 "$1" | awk '{print $NF}'
  fi
}

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

read_version_field() {
  awk -F'"' -v def="$1" '$0 ~ "^#define " def " " { print $2; exit }' "$ROOT/src/version.h"
}
APP_VER_MAJOR="$(read_version_field 'APP_VERSION_MAJOR')"
APP_VER_MINOR="$(read_version_field 'APP_VERSION_MINOR')"
BUILD_NUM="$(read_version_field 'BUILD_NUMBER')"
FULL_VERSION="${APP_VER_MAJOR}.${APP_VER_MINOR}.${BUILD_NUM}"
BIN_PATH="$STAGE/AmigaGPT/AmigaGPT/AmigaGPT_MorphOS"
PKG_MD5="$(file_md5 "$BIN_PATH")"
PKG_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
BUILD_UTC="$(date -u -r "$BIN_PATH" +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || echo "$PKG_UTC")"
GIT_REV="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
if git -C "$ROOT" diff --quiet 2>/dev/null; then
  GIT_LABEL="$GIT_REV"
else
  GIT_LABEL="${GIT_REV}-dirty"
fi
cat >"$STAGE/PACKAGE-BUILDINFO.txt" <<EOF
AmigaGPT MorphOS cross-package
==============================
full_version=${FULL_VERSION}
version=${APP_VER_MAJOR}.${APP_VER_MINOR}
revision=${BUILD_NUM}
git=${GIT_LABEL}
build_utc=${BUILD_UTC}
packaged_utc=${PKG_UTC}
AmigaGPT_MorphOS.md5=${PKG_MD5}

Prüfen auf MorphOS: diese Datei nach dem Entpacken lesen (nicht nur LHA-Datum
am Share — SMB/Copy-Item kann altes Erstellungsdatum behalten).
Deploy-Protokoll auf dem Share: DEPLOY-HISTORY.txt (letzte 10 Z:-Kopien).
EOF
log "Buildinfo: ${FULL_VERSION} git=${GIT_LABEL} build=${BUILD_UTC}"

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

Installation (MorphOS — LHA vom Share entpacken, nicht WSL)
------------
1. LHA entpacken, z. B. nach Work:Tmp/AmigaGPT-test/
   → package-morphos/AmigaGPT/AmigaGPT/...  (HW-Test-Sandbox; kein Work:Programme/)
2. Optional in package-morphos/AmigaGPT/AmigaGPT/: rx fix-protection.rexx (vor allem rexx/#?)
3. assign AMIGAGPT: <Entpackpfad>/package-morphos/AmigaGPT/AmigaGPT
   (Beispiel: Work:Tmp/AmigaGPT-test/package-morphos/AmigaGPT/AmigaGPT)
4. Optional: run >nil: AMIGAGPT:AmigaGPTD_MorphOS
5. Start: run AMIGAGPT:AmigaGPT_MorphOS  (oder Doppelklick)

Dauerhafte User-Installation: eigener Pfad nach Wahl — nicht vom Agent/MCP überschreiben.

Share-Deploy (WSL): nur AmigaGPT-MorphOS-cross.lha nach morphos/out-crosscompile/
(z.B. HDSFGO4-share:morphos/out-crosscompile/). Entpacken: MorphOS-Deploy-Skript.

Version prüfen: PACKAGE-BUILDINFO.txt (full_version, AmigaGPT_MorphOS.md5) oder About.
MD5 in Ambient (Datei-Info) mit BUILDINFO / DEPLOY-HISTORY.txt auf dem Share vergleichen.
Share: DEPLOY-HISTORY.txt = letzte 10 erfolgreiche Z:-Deploys (WSL).

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
    LOCAL_MD5="$(file_md5 "$ARCHIVE")"
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
      \$h = (Get-FileHash -LiteralPath \$arch -Algorithm MD5).Hash.ToLower()
      if (\$h -ne '${LOCAL_MD5}') {
        throw \"MD5 abweichend auf Z: (erwartet ${LOCAL_MD5})\"
      }
      \$legacy = Join-Path \$d 'package-morphos'
      if (Test-Path \$legacy) { Remove-Item -Recurse -Force \$legacy }
      Write-Host \"LHA ersetzt OK: \$(\$dst.Name) \$(\$dst.Length) Bytes MD5=\$h\"
      \$dst | Format-List Name,Length,LastWriteTime
    "; then
      log "Fehler: Deploy nach $DEPLOY_WIN fehlgeschlagen (Laufwerk nicht erreichbar?)."
      DEPLOY_FAILED=1
    else
      log "Z: LHA verifiziert (Größe + MD5)."
      COPY_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
      HISTORY_LINE="${FULL_VERSION}|${GIT_LABEL}|${BUILD_UTC}|${COPY_UTC}|${LOCAL_MD5}"
      if powershell.exe -NoProfile -Command "
        \$ErrorActionPreference = 'Stop'
        \$d = '${DEPLOY_WIN}'
        \$history = Join-Path \$d 'DEPLOY-HISTORY.txt'
        \$header = @(
          '# DEPLOY-HISTORY.txt - last 10 successful deploys to this folder (newest last)',
          '# version | git | build_utc | copy_utc | lha_md5 (Ambient-kompatibel)',
          ''
        )
        \$lines = [System.Collections.ArrayList]@()
        if (Test-Path -LiteralPath \$history) {
          foreach (\$row in Get-Content -LiteralPath \$history -Encoding utf8) {
            \$t = \$row.Trim()
            if (\$t -match '^\d+\.\d+\.\d+\|[^\|]+\|[^|]+\|[^|]+\|[a-fA-F0-9]{32}\$') {
              [void]\$lines.Add(\$t)
            }
          }
        }
        [void]\$lines.Add('${HISTORY_LINE}')
        while (\$lines.Count -gt 10) {
          \$lines.RemoveAt(0)
        }
        (\$header + [string[]]\$lines.ToArray()) | Set-Content -LiteralPath \$history -Encoding utf8
        Write-Host \"DEPLOY-HISTORY: \$(\$lines.Count) Eintrag(e), neueste: ${FULL_VERSION}\"
      "; then
        log "Deploy-Protokoll: $DEPLOY_WIN/DEPLOY-HISTORY.txt"
      else
        log "Warnung: DEPLOY-HISTORY.txt konnte nicht geschrieben werden."
      fi
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
