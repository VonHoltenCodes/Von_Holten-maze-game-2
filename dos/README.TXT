================================================================================
  MAZE RUNNER 2 - THE NEXT LEVEL                                 VonHoltenCodes
  DOS raycasting shooter                                            version 2.1
================================================================================

WHAT YOU NEED
-------------
  * 486 or better with a VGA card (a Pentium runs it smoothly; it was tuned
    on a Pentium 4 era box). Anything from a 486DX to a Core 2 in DOS mode.
  * MS-DOS 5+, a Windows 9x DOS box, Windows XP NTVDM, FreeDOS or DOSBox.
  * ~1 MB of free memory. No DPMI host needed: CWSDPMI is built into the EXE.
  * Optional: a mouse driver (MOUSE.COM) for mouse look and fire.
  * Optional: Sound Blaster or compatible for sound effects (SET BLASTER=A220 D1
    is honoured for the base address / DMA channel) and an AdLib/OPL FM chip
    (every Sound Blaster has one) for the MIDI soundtrack.
    Without a card the PC speaker plays the effects and there is no music.

FILES
-----
  MAZE2.EXE       the game (self-contained, DPMI stub bound in)
  FM.DAT          OPL instrument bank for the MIDI player - keep beside the EXE
  1.MID           soundtrack - keep beside the EXE
  MAZE2.BAT       loads a mouse driver if one is found, then runs the game
  README_DOS.TXT  this file

  MAZE2.IMG is a 1.44 MB floppy image with all of the above on it:
  write it with `dd if=MAZE2.IMG of=/dev/fd0` (Linux) or rawrite (DOS).

RUNNING
-------
  MAZE2                 normal start
  MAZE2 -nosound        skip the Sound Blaster probe and the music
                        (use this if the sound hardware hangs the machine)

CONTROLS
--------
  W / Up        move forward           S / Down     move backward
  A             strafe left            D            strafe right
  Left / Right  turn (keyboard only)   Mouse        look / turn
  SPACE / CTRL / left button   fire
  F1 (or F)     FPS counter on/off     ESC          quit

GOAL
----
  Fight through the dungeon from the north-west corner to the exit in the
  south-east corner (the blinking marker on the mini map). Enemies shoot back.
  Soldiers and the elite respawn 10 s after you drop them, so keep moving.

TROUBLESHOOTING
---------------
  "Invalid Opcode / SIGILL" on old CPUs: this build targets the i586 (Pentium)
      instruction set. It will not run on a 386/486 below a Pentium.
  No music but sound works: FM.DAT or 1.MID is not next to MAZE2.EXE.
  Keyboard feels stuck after a Windows task switch: release all keys once.
  Black screen after the title: press ESC to leave, then run with -nosound.

BUILDING FROM SOURCE
--------------------
  https://github.com/VonHoltenCodes/Von_Holten-maze-game-2
  DJGPP cross-compiler + make, see README.md. CI builds every push and attaches
  MAZE2.EXE, MAZE2.IMG and MAZE2.zip to tagged releases.

CREDITS
-------
  Game, engine, art:  Trent Von Holten / VonHoltenCodes, 2025
  MIDI player:        Steven H Don (MIDIPLAY.C, FM.DAT), 1999
  DPMI host:          CWSDPMI r7 by Charles W Sandmann
  Compiler:           DJGPP (GCC 12)
================================================================================
