#!/usr/bin/env bash
set -euo pipefail

repo_url="https://github.com/sacredbanana/AmigaSDK-gcc.git"
cache_root=".cursor/cache"
cache_dir="$cache_root/amigasdk-gcc"
lock_dir="$cache_root/amigasdk-gcc.sync.lock"
wait_secs="${SYNC_WAIT_SECS:-300}"

is_valid_repo() {
  [ -d "$cache_dir/.git" ] && git -C "$cache_dir" rev-parse --is-inside-work-tree >/dev/null 2>&1
}

mkdir -p "$cache_root"

# If another sync is already running, wait for it to finish rather than
# triggering fallback behavior while the clone/pull is still in progress.
if ! mkdir "$lock_dir" 2>/dev/null; then
  waited=0
  while [ -d "$lock_dir" ]; do
    if [ "$waited" -ge "$wait_secs" ]; then
      echo "Timed out waiting for SDK cache sync to finish." >&2
      exit 1
    fi
    sleep 2
    waited=$((waited + 2))
  done

  if is_valid_repo; then
    exit 0
  fi

  if ! mkdir "$lock_dir" 2>/dev/null; then
    echo "Another SDK cache sync started while waiting; retry shortly." >&2
    exit 1
  fi
fi

trap 'rm -rf "$lock_dir"' EXIT

if is_valid_repo; then
  git -C "$cache_dir" pull --ff-only
else
  rm -rf "$cache_dir"
  git clone --depth 1 "$repo_url" "$cache_dir"
fi
