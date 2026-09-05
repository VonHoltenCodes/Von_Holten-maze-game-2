#!/usr/bin/env bash
# Copy the built game to the retro test boxes at C:\claude\MAZE2\.
#   tools/stage-dos.sh            both boxes
#   tools/stage-dos.sh gx1|xp     one box
# GX1 = Dell OptiPlex GX1, Windows 98, share "C" (no password), NetBIOS DELL_GX1.
# XP  = RetroBeast, Windows XP, admin share C$.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXE="$ROOT/build/MAZE2.EXE"
[ -f "$EXE" ] || { echo "Build first: tools/build.sh"; exit 1; }
GX1_IP="${GX1_IP:-192.168.68.75}"
XP_IP="${XP_IP:-192.168.68.100}"
NT1=(--option='client min protocol=NT1' --option='client max protocol=NT1')
# Windows 98's SMB server rejects smbclient's "cd", so every command uses a full remote path.
CMDS="mkdir claude; mkdir claude/MAZE2; put $EXE claude/MAZE2/MAZE2.EXE; put $ROOT/assets/music/1.MID claude/MAZE2/1.MID; put $ROOT/assets/music/FM.DAT claude/MAZE2/FM.DAT; put $ROOT/dos/README.TXT claude/MAZE2/README.TXT; put $ROOT/dos/MAZE2.BAT claude/MAZE2/MAZE2.BAT; ls claude/MAZE2/*"

stage_gx1() {
  echo "==> GX1 (Win98) $GX1_IP"
  smbclient //DELL_GX1/C -I "$GX1_IP" -p 139 -N "${NT1[@]}" -c "$CMDS"
}
stage_xp() {
  echo "==> RetroBeast (XP) $XP_IP"
  smbclient "//$XP_IP/C\$" -U 'RetroBeast%VonHolten2025' --option='client min protocol=NT1' -c "$CMDS"
}
# diag: also copy the triage binaries from `make diag` (HELLO.EXE, MAZE2NL.EXE)
if [ "${2:-}" = "diag" ] || [ "${1:-}" = "diag" ]; then
  CMDS="$CMDS; put $ROOT/build/diag/HELLO.EXE claude/MAZE2/HELLO.EXE; put $ROOT/build/diag/MAZE2NL.EXE claude/MAZE2/MAZE2NL.EXE; ls claude/MAZE2/*"
fi
case "${1:-all}" in
  gx1) stage_gx1 ;;
  xp)  stage_xp ;;
  all|diag) stage_gx1; stage_xp ;;
  *) echo "usage: $0 [gx1|xp|all|diag] [diag]" >&2; exit 2 ;;
esac
