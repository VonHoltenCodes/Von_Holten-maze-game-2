/*
 * keyboard.h - key-state keyboard driver (INT 9 handler) for DJGPP
 *
 * The BIOS keyboard buffer only reports typematic repeats, so a game that
 * polls it can't tell "W is held" from "W was tapped" and can't see two keys
 * at once. This driver hooks IRQ 1 and keeps a make/break table of every
 * scancode instead, which is what an FPS needs for WASD + fire.
 *
 * While installed the BIOS buffer receives nothing (kbhit/getch are dead), so
 * install it when the game loop starts and remove it before any text prompt.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Set 1 scancodes used by the game */
#define KEY_ESC    0x01
#define KEY_W      0x11
#define KEY_A      0x1E
#define KEY_S      0x1F
#define KEY_D      0x20
#define KEY_F      0x21
#define KEY_M      0x32
#define KEY_SPACE  0x39
#define KEY_UP     0x48
#define KEY_LEFT   0x4B
#define KEY_RIGHT  0x4D
#define KEY_DOWN   0x50
#define KEY_LCTRL  0x1D
#define KEY_LSHIFT 0x2A
#define KEY_RSHIFT 0x36
#define KEY_ENTER  0x1C
#define KEY_TAB    0x0F
#define KEY_F1     0x3B

int  kb_install(void);          /* hook INT 9; returns 1 on success */
void kb_remove(void);           /* restore the BIOS handler and flush its buffer */
int  kb_down(int scancode);     /* 1 while the key is held */
int  kb_pressed(int scancode);  /* 1 once per press (edge), consumed by the call */
void kb_clear(void);            /* forget all held keys (after a focus loss etc.) */

#endif
