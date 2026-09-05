# Changelog

## 2.1.0 — 2026-09-05

Modernization pass, same program as BONK v10: make the shipped EXE real,
self-contained and testable, and fix what was broken underneath.

### Runs everywhere
- **Self-contained EXE.** The CWSDPMI r7 stub is bound into `MAZE2.EXE`
  (`tools/exe2coff.py`), so no `CWSDPMI.EXE` is needed on bare DOS, a floppy or
  DOSBox. Previous builds silently required one.
- **No near pointers.** VGA writes already went through `dosmemput`; the leftover
  `__djgpp_nearptr_enable()` call, which fails under Windows NT/XP NTVDM, is gone.
- **Data files next to the EXE.** `FM.DAT` and `1.MID` are loaded from the
  directory `MAZE2.EXE` lives in, not the current directory.
- `-nosound` switch skips the Sound Blaster probe and music.

### Input
- **INT 9 key-state driver** (`src/keyboard.c`) replaces polling the BIOS key
  buffer. Held keys register every frame, several keys work at once, and ESC /
  fire are edge-triggered so a quick tap is never missed.
- **Keyboard-only play:** Left/Right arrows turn, A/D strafe, W/S or Up/Down move,
  SPACE / CTRL / left mouse button fire. Before, turning required a mouse driver.

### Timing
- **Frame-rate independent movement.** Player, enemy and projectile speeds are in
  world units per second, scaled by the measured frame time (clamped at 100 ms).
  The old per-frame constants made a Pentium 4 unplayably fast and a 486 a crawl.
- **DOS clock no longer runs fast.** The bundled MIDI player chained the BIOS
  timer on every MIDI tick under DJGPP, so the DOS clock advanced 7-15x too fast
  while music played (see `third_party/midiplay/NOTICE.md`). Game timing now runs
  off the BIOS tick counter, which that fix keeps honest.
- `F1` (or `F`) toggles an FPS / position overlay; the exit summary prints the
  average frame rate.

### Sound
- **Sound Blaster DMA fixed.** The old code programmed the DMA controller with a
  protected-mode `malloc()` pointer, which is not a physical address; the card
  played whatever lived at that address below 1 MB. The buffer is now real DOS
  memory that never crosses a 64 KB DMA page, `SET BLASTER=A… D…` is honoured.
- **Non-blocking effects.** Each effect is one single-cycle DMA transfer (or a
  PC-speaker sequence stepped per frame). The game loop no longer stalls
  15-150 ms on every footstep, shot and hit.
- The unused `adlib.c` sequencer (never called; the MIDI player owns the OPL) is
  removed from the build.

### Renderer
- Sky rows are `memset`, the floor caster does one perspective divide per row and
  steps in 16.16 fixed point, wall texture rows and sprite texels step in fixed
  point, and torch light is baked per map cell at start-up instead of summing 16
  square roots per column per frame. About 45 % more frames at the same emulated
  CPU speed (dosbox-x, fixed 30000 cycles: 6.8 → 9.8 fps standing at the spawn).
- Removed dead code (`VGA_MEMORY`, fire particles, unused locals) so the build is
  clean under `-Wall -Wextra`.

### Repo, build, CI
- New layout: `src/`, `assets/`, `third_party/midiplay/`, `dos/`, `tools/`,
  `docs/`. Build outputs are no longer committed; CI builds every push and
  attaches `MAZE2.EXE`, `MAZE2.IMG` (1.44 MB floppy image) and `MAZE2.zip` to
  `v*` tag releases.
- `tools/dos-shots.py` plays the game in dosbox-x on a virtual display with held
  keys and screenshots every stage; `tools/stage-dos.sh` copies a build to the
  GX1 (Win98) and RetroBeast (XP) test boxes.
- MIDI player bug fixes: `StopMIDI` all-notes-off loop overran its 15-entry
  arrays and passed array indexes instead of OPL voices.

## 2.0 — 2025-11-28

MAZE RUNNER 2 as first published: 64x64 procedural textures, enemies that shoot
back, projectiles, WASD + mouse, torches, wall decorations, VGA splash and credit
screens, AdLib/OPL MIDI music, Sound Blaster effects. SIGILL on non-P4 CPUs fixed
by building for `-march=i586`.
