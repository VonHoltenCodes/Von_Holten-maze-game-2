#!/usr/bin/env bash
# Build MAZE RUNNER 2 with the local DJGPP toolchain.
#   tools/build.sh          -> build/MAZE2.EXE (+ 1.MID, FM.DAT beside it)
#   tools/build.sh dist     -> also dist/MAZE2.IMG floppy image and dist/MAZE2.zip
#   tools/build.sh clean
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$HOME/djgpp/bin:$PATH"
case "${1:-all}" in
  all)   make -C "$ROOT" ;;
  dist)  make -C "$ROOT" dist ;;
  clean) make -C "$ROOT" clean ;;
  *) echo "usage: $0 [all|dist|clean]" >&2; exit 2 ;;
esac
