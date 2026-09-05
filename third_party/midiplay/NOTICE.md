# MIDIPLAY.C — background MIDI playback for DOS

`MIDIPLAY.C`, `PM.C`, `FM.DAT`, `1.MID` and `2.MID` come from the `midi-c.zip`
package by **Steven H Don** (shd@earthling.net, http://shd.cjb.net, 1999),
kept here as `midi-c.zip` for provenance. The package is a freely distributed
demo unit; no license text ships with it. It is included unmodified except for:

- `FMDataFile` — the OPL instrument bank path is a variable instead of the
  literal `"FM.DAT"`, so the game can load it from its own directory.
- `StopMIDI()` — the all-notes-off loop ran to index 17 over the 15-entry
  `InUse[]` / `RealVoice[]` arrays (undefined behaviour flagged by GCC 12) and
  passed the array index instead of the OPL voice to `DisableNote()`.
- `RealVoice` given an explicit `const char` type (implicit int is not C99).
- **Timer chaining (DJGPP build)** — the original installed `TimerHandler` with
  `_go32_dpmi_chain_protected_mode_interrupt_vector`, so the BIOS INT 8 handler
  ran on *every* MIDI tick (125-500 Hz) instead of every 65536 PIT counts. The DOS
  clock then ran 7-15x fast for as long as music played (measured: 160 "seconds"
  in 10 real ones), which also broke any game timing derived from it. The handler
  is now installed through an iret wrapper and far-calls the saved BIOS vector
  itself when `ClockTicks` overflows, exactly like the Borland build already did.

The instrument bank `FM.DAT` and the two songs are shipped with the game under
`assets/music/`.
