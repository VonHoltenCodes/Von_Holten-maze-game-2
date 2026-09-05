/*
 * MAZE RUNNER 2 - P4-ERA DOS RAYCASTER
 * =====================================
 *
 * Enhanced raycasting engine targeting Pentium 4 era hardware (1.5GHz+)
 * Based on Von_Holten-Maze-Game v3.0 foundation
 *
 * FEATURES:
 * - 64x64 procedural wall textures, wall decorations, baked torch lighting
 * - Enemies that SHOOT BACK with projectile system
 * - Sound Blaster DMA / PC speaker sound effects (non-blocking)
 * - AdLib/OPL MIDI background music (third_party/midiplay)
 * - Doom-style HUD with mini map, FPS overlay on F1
 * - INT 9 key-state keyboard driver: hold WASD, turn with arrows, strafe A/D
 * - Frame-rate independent movement, timed off the BIOS tick clock
 * - Self-contained EXE (CWSDPMI stub bound in), no near pointers: runs on
 *   bare DOS, Win9x DOS boxes, XP NTVDM and DOSBox alike
 *
 * TARGET: any 486/Pentium with VGA (tuned on Pentium 4 era hardware)
 *
 * By: VonHoltenCodes (2025)
 * License: Open Source
 *
 * =============================================================================
 * CODE INDEX - Jump to any section by searching for [SEC-XX]
 * =============================================================================
 *
 * [SEC-01] CONFIGURATION          - Screen, texture, combat settings
 * [SEC-02] DATA STRUCTURES        - Player, Enemy, Projectile structs
 * [SEC-03] LIGHTING SYSTEM        - Torch and fire particle systems
 * [SEC-04] WEAPON SPRITE          - 60x40 pistol bitmap data
 * [SEC-05] ENEMY SPRITES          - 24x32 grunt/soldier/elite bitmaps
 * [SEC-06] GLOBAL STATE           - Buffers, textures, game state vars
 * [SEC-07] LEVEL DATA             - worldMap[24][24] level definition
 * [SEC-08] SOUND SYSTEM           - PC Speaker sound effects
 * [SEC-09] TEXTURE GENERATION     - Procedural 64x64 wall textures
 * [SEC-10] VGA GRAPHICS           - Mode 13h, double buffering, palette
 * [SEC-11] INPUT HANDLING         - Keyboard state reader, mouse
 * [SEC-12] PROJECTILE SYSTEM      - Player and enemy projectiles
 * [SEC-13] ENEMY AI               - Movement, targeting, shooting AI
 * [SEC-14] PLAYER                 - Movement, collision, shooting
 * [SEC-15] RENDERING              - DDA raycasting, textured walls
 * [SEC-16] SPRITE RENDERING       - Z-sorted billboard enemy sprites
 * [SEC-17] HUD & MINIMAP          - Doom-style status bar, mini map
 * [SEC-18] SPLASH SCREENS         - Title, credits, scrolling end
 * [SEC-19] MAIN                   - Game loop, initialization, cleanup
 *
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <conio.h>
#include <dpmi.h>
#include <go32.h>
#include <dos.h>
#include <pc.h>
#include <sys/farptr.h>
#include <stdarg.h>

#include "sound.h"
#include "keyboard.h"

/* MIDI music playback by Steven H Don - see third_party/midiplay/NOTICE.md */
#include "MIDIPLAY.C"

#define MAZE2_VERSION "2.1.1"

/* Interrupt handlers (MIDI timer, INT 9) run with paging possible under Windows'
 * DPMI (Win9x DOS box, NTVDM). Lock the whole program so they can never touch a
 * paged-out page; CWSDPMI without a swap file ignores this. */
#include <crt0.h>
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY | _CRT0_FLAG_NONMOVE_SBRK;

/* 64x64 enemy sprites - hand-drawn pixel art */
#include "sprites/sprite_creeper.h"
#include "sprites/sprite_snowman.h"
#include "sprites/sprite_minion.h"

/* Sprite dimensions for 64x64 */
#define ENEMY_SPRITE_SIZE 64

/*============================================================================
 * [SEC-01] CONFIGURATION - P4 ERA SETTINGS
 *===========================================================================*/

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define SCREEN_CENTER (SCREEN_HEIGHT / 2)
#define BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)

/* P4 ERA: Higher resolution textures */
#define TEX_SIZE 64          /* 64x64 textures (was 8x8) */
#define TEX_MASK (TEX_SIZE - 1)

/* Map dimensions */
#define MAP_WIDTH 24
#define MAP_HEIGHT 24

/* Movement settings */
#define MOVE_SPEED 3.0           /* world units per second */
#define ROTATE_SPEED 2.2         /* radians per second (arrow keys) */
#define MOUSE_SENSITIVITY 0.006  /* radians per screen pixel of mouse travel */
#define MOUSE_CX 320             /* mouse driver centre in mode 13h (x reported doubled) */
#define MOUSE_CY 100
#define MAX_FRAME_DT 0.1         /* clamp for frame time so a stall can't teleport anything */

/* Combat settings */
#define MAX_PROJECTILES 32
#define PROJECTILE_SPEED 9.0     /* world units per second */
#define ENEMY_SPEED 0.5          /* world units per second */
#define ENEMY_FIRE_RATE 2000    /* ms between enemy shots */
#define ENEMY_FIRE_RANGE 15.0   /* Max distance enemy can shoot */
#define PLAYER_STARTING_HEALTH 100
#define PLAYER_STARTING_AMMO 100

/* VGA Color palette */
#define COLOR_BLACK 0
#define COLOR_BLUE 1
#define COLOR_GREEN 2
#define COLOR_CYAN 3
#define COLOR_RED 4
#define COLOR_MAGENTA 5
#define COLOR_BROWN 6
#define COLOR_WHITE 7
#define COLOR_GRAY 8
#define COLOR_LBLUE 9
#define COLOR_LGREEN 10
#define COLOR_LCYAN 11
#define COLOR_LRED 12
#define COLOR_LMAGENTA 13
#define COLOR_YELLOW 14
#define COLOR_BWHITE 15
#define COLOR_DGRAY 8  /* Dark gray (same as GRAY in VGA palette) */

/* Wall types */
#define WALL_NONE 0
#define WALL_BRICK 1
#define WALL_STONE 2
#define WALL_METAL 3
#define WALL_TECH 4

/*============================================================================
 * [SEC-02] DATA STRUCTURES
 *===========================================================================*/

typedef struct {
    double x, y;           /* Position */
    double dirX, dirY;     /* Direction vector */
    double planeX, planeY; /* Camera plane */
    double angle;          /* Current angle */
    int health;
    int ammo;
    int score;
} Player;

typedef struct {
    double x, y;           /* Position */
    double dirX, dirY;     /* Direction */
    int active;
    int isPlayerProjectile; /* 1 = player's bullet, 0 = enemy's */
    unsigned char color;
} Projectile;

typedef struct {
    double x, y;
    double spawnX, spawnY;
    int active;
    int health;
    int maxHealth;         /* For respawn */
    int type;              /* Enemy type */
    unsigned long lastFireTime;  /* game ms when enemy last shot */
    unsigned long deathTime;     /* game ms when enemy died (for respawn) */
    int canShoot;          /* Does this enemy type shoot? */
} Enemy;

#define ENEMY_RESPAWN_TIME 10000  /* 10 seconds to respawn */

/* Enemy types */
#define ENEMY_GRUNT 0      /* Basic, doesn't shoot */
#define ENEMY_SOLDIER 1    /* Shoots back */
#define ENEMY_ELITE 2      /* Shoots fast, more health */
#define ENEMY_BOSS 3       /* Lots of health, rapid fire */

#define MAX_ENEMIES 20

/*============================================================================
 * DYNAMIC LIGHTING SYSTEM - Torches with animated flames
 *===========================================================================*/

#define MAX_TORCHES 16
#define TORCH_LIGHT_RADIUS 6.0
#define TORCH_FLICKER_SPEED 200

/* Torch structure */
typedef struct {
    double x, y;
    int active;
    int animFrame;
    double intensity;
    clock_t lastFlicker;
} Torch;

/*============================================================================
 * WEAPON SPRITE - 60x40 pistol
 *===========================================================================*/

#define WEAPON_WIDTH 60
#define WEAPON_HEIGHT 40

/* Pistol sprite - hand holding gun (0=transparent) */
static unsigned char weaponSprite[WEAPON_WIDTH * WEAPON_HEIGHT] = {
    /* Rows 0-20: Empty */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* Row 20-21: Gun barrel tip */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,8,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,7,7,7,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* Row 22-25: Gun body */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,7,15,15,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,7,7,15,15,7,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,7,7,15,7,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,7,7,7,7,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* Row 26-30: Gun grip/trigger */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,7,6,6,6,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,6,6,6,6,6,6,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* Row 31-35: Hand holding gun */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,14,14,6,6,14,14,6,6,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,14,14,14,6,6,14,14,14,6,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,14,14,14,14,14,14,14,14,6,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,14,14,14,14,14,14,14,14,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,14,14,14,14,14,14,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* Row 36-39: More hand */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,14,14,14,14,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,14,14,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/*============================================================================
 * [SEC-05] ENEMY SPRITES - 32x32 detailed sprites from original maze game
 * Using actual bitmap sprites from sprite header files
 *===========================================================================*/

/* Sprite sizes - using 32x32 sprites from original game */
#define ENEMY_SPRITE_WIDTH 64
#define ENEMY_SPRITE_HEIGHT 64

/* Sprite pointers mapped to enemy types */
/* ENEMY_GRUNT   -> enemy_gremlin (purple gremlin) */
/* ENEMY_SOLDIER -> minion_jump (jumping minion) */
/* ENEMY_ELITE   -> enemy_creeper */
/* ENEMY_BOSS    -> enemy_tomato (boss tomato) */

/* Initialize sprites - just need to verify headers loaded */
void initEnemySprites(void) {
    /* Sprites are already defined in header files as static arrays */
    /* enemy_gremlin[1024], minion_jump[1024], etc. */
}

/*============================================================================
 * [SEC-06] GLOBAL STATE
 *===========================================================================*/

/* Double buffer */
static unsigned char *backBuffer = NULL;

/* Z-buffer for depth testing */
static double zBuffer[SCREEN_WIDTH];

/* Input state */
static int mouseAvailable = 0;
static int playerPitch = 0;
static int showFps = 0;

/* Frame timing - BIOS tick clock (18.2 Hz), immune to the MIDI player's PIT reprogramming */
static double frameDt = 1.0 / 30.0;   /* seconds per frame, smoothed */
static double fpsEstimate = 30.0;

/* Game state */
static Player player;
static Projectile projectiles[MAX_PROJECTILES];
static Enemy enemies[MAX_ENEMIES];
static int numEnemies = 0;
static int gameRunning = 1;
static unsigned long hurtUntil = 0;   /* game ms: red border while > now */
static const char *exitReason = "quit (ESC)";

/* Dynamic lighting */
static Torch torches[MAX_TORCHES];
static int numTorches = 0;

/* Sound state */
static int soundEnabled = 1;  /* Enable sound by default */

/* P4 ERA: 64x64 procedural textures */
static unsigned char wallTextures[4][TEX_SIZE * TEX_SIZE];

/*============================================================================
 * [SEC-08] SOUND SYSTEM - Sound Blaster PCM + AdLib FM Music
 * Uses working modules from original maze game (sound.c, adlib.c)
 *===========================================================================*/

/* Timing helpers - BIOS tick count at 0040:006C (18.2 Hz) */
static unsigned long biosTicks(void) { return _farpeekl(_dos_ds, 0x46C); }
static unsigned long gameMs(void) { return (unsigned long)(biosTicks() * 54.925); }

/* Estimate seconds per frame from frames counted between BIOS ticks. */
static void updateFrameTiming(void) {
    static unsigned long lastTick = 0;
    static int framesSinceTick = 0;
    unsigned long t = biosTicks();
    if (lastTick == 0) { lastTick = t; return; }
    framesSinceTick++;
    if (t != lastTick) {
        double measured = (double)(t - lastTick) * (1.0 / 18.2065) / framesSinceTick;
        frameDt = frameDt * 0.7 + measured * 0.3;
        if (frameDt > MAX_FRAME_DT) frameDt = MAX_FRAME_DT;
        if (frameDt < 0.001) frameDt = 0.001;
        fpsEstimate = 1.0 / frameDt;
        lastTick = t;
        framesSinceTick = 0;
    }
}

/* Sound effects as (Hz, ms) steps - see sound.h. Higher priority interrupts lower. */
static const SoundStep stShoot[]  = { {400, 20}, {200, 15} };
static const SoundStep stHit[]    = { {150, 25} };
static const SoundStep stDeath[]  = { {250, 40}, {120, 50}, {60, 60} };
static const SoundStep stHurt[]   = { {80, 50} };
static const SoundStep stPickup[] = { {600, 20}, {800, 20}, {1000, 25} };
static const SoundStep stStep[]   = { {60, 15} };
static const SoundFx fxShoot  = { stShoot, 2, 2 };
static const SoundFx fxHit    = { stHit, 1, 2 };
static const SoundFx fxDeath  = { stDeath, 3, 3 };
static const SoundFx fxHurt   = { stHurt, 1, 3 };
static const SoundFx fxPickup = { stPickup, 3, 4 };
static const SoundFx fxStep   = { stStep, 1, 0 };

void initSound(void) {
    if (!soundEnabled) return;
    sound_init(1);
}

void soundShoot(void)      { if (soundEnabled) sound_play(&fxShoot); }
void soundHit(void)        { if (soundEnabled) sound_play(&fxHit); }
void soundEnemyDeath(void) { if (soundEnabled) sound_play(&fxDeath); }
void soundPlayerHurt(void) { if (soundEnabled) sound_play(&fxHurt); }
void soundPickup(void)     { if (soundEnabled) sound_play(&fxPickup); }

static unsigned long lastFootstep = 0;
#define FOOTSTEP_INTERVAL 400  /* ms between footsteps */

void soundStep(void) {
    unsigned long now = gameMs();
    if (now - lastFootstep < FOOTSTEP_INTERVAL) return;
    if (!soundEnabled) return;
    sound_play(&fxStep);
    lastFootstep = now;
}

void updateSound(void) {
    if (!soundEnabled) return;
    sound_update();   /* steps the PC-speaker sequencer; no-op on Sound Blaster */
}

/*============================================================================
 * [SEC-07] LEVEL DATA - Single well-crafted dungeon level
 *
 * Wall types: 1=Brick, 2=Stone, 3=Metal, 4=Tech
 * Player starts at (2,2), exit is at the opposite corner
 *
 * Layout concept:
 * - Dungeon entrance (stone) in NW corner
 * - Central hub with pillars
 * - Prison block (brick) in SW
 * - Armory (metal) in NE
 * - Control room (tech) in SE
 *===========================================================================*/
static int worldMap[MAP_HEIGHT][MAP_WIDTH] = {
    /* Row 0: Northern outer wall */
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},
    /* Row 1: Dungeon entrance corridor */
    {2,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,3},
    /* Row 2: START area (stone dungeon) */
    {2,0,2,2,0,2,0,2,2,2,2,2,0,2,2,0,0,0,3,0,3,3,0,3},
    /* Row 3: Dungeon cells */
    {2,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,3,0,0,0,0,3,0,3},
    /* Row 4: */
    {2,2,2,0,2,2,2,2,0,2,0,2,2,2,2,0,3,3,3,0,0,0,0,3},
    /* Row 5: Corridor to central hub */
    {2,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,3,3,3,3},
    /* Row 6: */
    {2,0,2,2,2,0,2,2,0,2,0,1,1,1,1,1,0,3,0,0,0,0,0,3},
    /* Row 7: Central hub entrance */
    {2,0,0,0,2,0,0,0,0,0,0,1,0,0,0,1,0,3,0,3,3,3,0,3},
    /* Row 8: Central hub with pillars */
    {1,1,1,0,1,1,1,0,1,0,0,1,0,2,0,1,0,0,0,3,0,0,0,3},
    /* Row 9: Hub center */
    {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,3,0,4,4,4},
    /* Row 10: Hub pillars */
    {1,0,1,0,0,0,1,0,1,0,0,1,0,2,0,1,0,4,0,0,0,4,0,4},
    /* Row 11: Hub south */
    {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,4,0,4,0,4,0,4},
    /* Row 12: Prison block entrance */
    {1,0,1,0,0,0,1,0,1,1,1,1,1,0,1,1,0,4,0,4,0,0,0,4},
    /* Row 13: Prison corridor */
    {1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,4,0,4,4,0,4,4},
    /* Row 14: Prison cells west */
    {1,0,1,0,1,0,1,1,1,0,4,4,4,4,0,4,0,0,0,0,0,0,0,4},
    /* Row 15: */
    {1,0,1,0,0,0,0,0,0,0,4,0,0,4,0,4,4,4,0,4,4,4,0,4},
    /* Row 16: */
    {1,0,1,1,1,0,1,1,0,0,4,0,0,0,0,0,0,0,0,0,0,4,0,4},
    /* Row 17: */
    {1,0,0,0,0,0,0,1,0,0,4,4,0,4,4,4,0,4,4,4,0,4,0,4},
    /* Row 18: */
    {1,1,1,0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    /* Row 19: Tech control room area */
    {1,0,0,0,0,1,0,0,0,4,4,4,0,4,0,4,4,4,0,4,4,4,0,4},
    /* Row 20: */
    {1,0,1,1,0,1,1,1,0,4,0,0,0,4,0,0,0,0,0,4,0,0,0,4},
    /* Row 21: EXIT corridor */
    {1,0,0,0,0,0,0,0,0,4,0,4,4,4,0,4,4,4,0,4,0,4,0,4},
    /* Row 22: EXIT area */
    {1,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0},
    /* Row 23: Southern outer wall */
    {1,1,1,1,1,1,1,1,1,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4}
};

/*============================================================================
 * [SEC-09] TEXTURE GENERATION - Clean procedural 64x64 textures
 * Original simple textures that looked good
 *===========================================================================*/

/* Generate stone block texture - grayscale for proper lighting */
/* Uses colors 16-31 (VGA grayscale ramp) */
void generateBrickTexture(unsigned char *tex) {
    int x, y;
    int blockW = 16, blockH = 8;

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            int bx = x % blockW;
            int by = y % blockH;
            int row = y / blockH;

            /* Offset every other row for stone block pattern */
            if (row % 2 == 1) {
                bx = (x + blockW / 2) % blockW;
            }

            /* Mortar lines - dark */
            if (bx == 0 || by == 0) {
                tex[y * TEX_SIZE + x] = 17;  /* Very dark gray mortar */
            } else {
                /* Stone color - grayscale with variation */
                int shade = 21 + (rand() % 3);  /* Base mid-gray */
                if (rand() % 6 == 0) shade = 18;  /* Dark spot */
                if (rand() % 8 == 0) shade = 24;  /* Light spot */
                tex[y * TEX_SIZE + x] = shade;
            }
        }
    }
}

/* Generate rough stone texture - grayscale for proper lighting */
/* Uses colors 16-31 (VGA grayscale ramp) */
void generateStoneTexture(unsigned char *tex) {
    int x, y;

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            int localX = x % 12;
            int localY = y % 10;

            /* Mortar lines between blocks */
            if (localX == 0 || localY == 0) {
                tex[y * TEX_SIZE + x] = 16;  /* Nearly black mortar */
            } else {
                /* Stone face - grayscale with variation */
                int base = 20 + (rand() % 4);  /* Mid-dark gray */
                if (rand() % 8 == 0) base = 17;  /* Dark spot */
                if (rand() % 10 == 0) base = 25; /* Light highlight */
                tex[y * TEX_SIZE + x] = base;
            }
        }
    }
}

/* Generate metal/dungeon door texture - grayscale */
/* Uses colors 16-31 (VGA grayscale ramp) */
void generateMetalTexture(unsigned char *tex) {
    int x, y;

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            /* Base dark metal */
            int base = 19;  /* Dark gray metal */

            /* Vertical ridges */
            if (x % 8 == 0) base = 22;  /* Lighter ridge */
            if (x % 8 == 1) base = 16;  /* Shadow */

            /* Horizontal seams */
            if (y % 32 == 0 || y % 32 == 1) base = 16;  /* Dark seam */

            /* Rivets */
            int rivetX = x % 16;
            int rivetY = y % 16;
            if (rivetX >= 6 && rivetX <= 9 && rivetY >= 6 && rivetY <= 9) {
                if (rivetX == 7 || rivetX == 8) {
                    if (rivetY == 7 || rivetY == 8) {
                        base = 27;  /* Bright rivet center */
                    } else {
                        base = 24;  /* Light gray rivet edge */
                    }
                }
            }

            tex[y * TEX_SIZE + x] = base;
        }
    }
}

/* Generate rough hewn stone texture - grayscale */
/* Uses colors 16-31 (VGA grayscale ramp) - same as others for consistency */
void generateTechTexture(unsigned char *tex) {
    int x, y;

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            /* Rough stone with larger blocks */
            int blockX = x / 16;
            int blockY = y / 16;
            int localX = x % 16;
            int localY = y % 16;

            /* Deep cracks between large stones */
            if (localX == 0 || localY == 0) {
                tex[y * TEX_SIZE + x] = 16;  /* Black crack */
            } else {
                /* Vary shade by block for variety */
                int base = 19 + ((blockX + blockY) % 3);
                /* Add noise */
                if (rand() % 5 == 0) base = 17;
                if (rand() % 7 == 0) base = 23;
                tex[y * TEX_SIZE + x] = base;
            }
        }
    }
}

/* Initialize all textures */
void initTextures(void) {
    srand(12345);  /* Fixed seed for consistent textures */
    generateBrickTexture(wallTextures[0]);

    srand(54321);
    generateStoneTexture(wallTextures[1]);

    srand(11111);
    generateMetalTexture(wallTextures[2]);

    srand(99999);
    generateTechTexture(wallTextures[3]);
}

/*============================================================================
 * VGA GRAPHICS
 *===========================================================================*/

void initDoubleBuffer(void) {
    backBuffer = (unsigned char *)malloc(BUFFER_SIZE);
    if (!backBuffer) {
        printf("ERROR: Could not allocate double buffer!\n");
        exit(1);
    }
    memset(backBuffer, 0, BUFFER_SIZE);
}

void freeDoubleBuffer(void) {
    if (backBuffer) {
        free(backBuffer);
        backBuffer = NULL;
    }
}

void waitForVRetrace(void) {
    while ((inp(0x3DA) & 0x08) == 0x08);
    while ((inp(0x3DA) & 0x08) == 0x00);
}

void displayFrame(void) {
    waitForVRetrace();
    dosmemput(backBuffer, BUFFER_SIZE, 0xA0000);   /* no near pointers: works under NTVDM too */
}

void setVideoMode(int mode) {
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = mode;
    int86(0x10, &regs, &regs);
}

/* Set up custom dark dungeon palette for stone walls */
void setupDungeonPalette(void) {
    int i;

    /* Wait for vertical retrace */
    while ((inp(0x3DA) & 0x08));
    while (!(inp(0x3DA) & 0x08));

    outp(0x3C8, 0);  /* Start at color 0 */

    /* Colors 0-15: Keep standard but darker */
    /* 0: Black */
    outp(0x3C9, 0); outp(0x3C9, 0); outp(0x3C9, 0);
    /* 1: Dark blue */
    outp(0x3C9, 0); outp(0x3C9, 0); outp(0x3C9, 20);
    /* 2: Dark green */
    outp(0x3C9, 0); outp(0x3C9, 20); outp(0x3C9, 0);
    /* 3: Dark cyan */
    outp(0x3C9, 0); outp(0x3C9, 20); outp(0x3C9, 20);
    /* 4: Dark red */
    outp(0x3C9, 25); outp(0x3C9, 0); outp(0x3C9, 0);
    /* 5: Dark magenta */
    outp(0x3C9, 20); outp(0x3C9, 0); outp(0x3C9, 20);
    /* 6: Brown */
    outp(0x3C9, 30); outp(0x3C9, 18); outp(0x3C9, 8);
    /* 7: Light gray */
    outp(0x3C9, 40); outp(0x3C9, 40); outp(0x3C9, 40);
    /* 8: Dark gray */
    outp(0x3C9, 20); outp(0x3C9, 20); outp(0x3C9, 20);
    /* 9: Light blue */
    outp(0x3C9, 20); outp(0x3C9, 20); outp(0x3C9, 50);
    /* 10: Light green */
    outp(0x3C9, 20); outp(0x3C9, 50); outp(0x3C9, 20);
    /* 11: Light cyan */
    outp(0x3C9, 20); outp(0x3C9, 50); outp(0x3C9, 50);
    /* 12: Light red */
    outp(0x3C9, 50); outp(0x3C9, 20); outp(0x3C9, 20);
    /* 13: Light magenta */
    outp(0x3C9, 50); outp(0x3C9, 20); outp(0x3C9, 50);
    /* 14: Yellow */
    outp(0x3C9, 55); outp(0x3C9, 55); outp(0x3C9, 20);
    /* 15: White */
    outp(0x3C9, 63); outp(0x3C9, 63); outp(0x3C9, 63);

    /* Colors 16-31: Gray stone ramp (dark to light) */
    for (i = 0; i < 16; i++) {
        int gray = 4 + i * 3;  /* 4 to 49 */
        outp(0x3C9, gray);
        outp(0x3C9, gray);
        outp(0x3C9, gray);
    }

    /* Colors 32-47: Brown/tan brick ramp */
    for (i = 0; i < 16; i++) {
        int r = 12 + i * 3;    /* 12 to 57 */
        int g = 6 + i * 2;     /* 6 to 36 */
        int b = 4 + i;         /* 4 to 19 */
        outp(0x3C9, r);
        outp(0x3C9, g);
        outp(0x3C9, b);
    }

    /* Colors 48-63: Red/flesh tones for enemies */
    for (i = 0; i < 16; i++) {
        int r = 16 + i * 3;
        int g = 8 + i * 2;
        int b = 4 + i;
        outp(0x3C9, r);
        outp(0x3C9, g);
        outp(0x3C9, b);
    }

    /* Colors 64-79: Green tones for creepers */
    for (i = 0; i < 16; i++) {
        int r = 4 + i;
        int g = 16 + i * 3;
        int b = 4 + i;
        outp(0x3C9, r);
        outp(0x3C9, g);
        outp(0x3C9, b);
    }

    /* Colors 80-95: Purple tones for magic/special */
    for (i = 0; i < 16; i++) {
        int r = 12 + i * 2;
        int g = 4 + i;
        int b = 20 + i * 2;
        outp(0x3C9, r);
        outp(0x3C9, g);
        outp(0x3C9, b);
    }

    /* Fill remaining colors 96-255 with neutral grays for sprites */
    for (i = 96; i < 256; i++) {
        int gray = (i - 96) * 63 / 160;
        outp(0x3C9, gray);
        outp(0x3C9, gray);
        outp(0x3C9, gray);
    }
}

void setPixel(int x, int y, unsigned char color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        backBuffer[y * SCREEN_WIDTH + x] = color;
    }
}

void clearScreen(unsigned char color) {
    memset(backBuffer, color, BUFFER_SIZE);
}

/*============================================================================
 * INPUT HANDLING
 *===========================================================================*/

void initMouse(void) {
    union REGS r;
    r.x.ax = 0;
    int86(0x33, &r, &r);

    if (r.x.ax == 0xFFFF) {
        mouseAvailable = 1;
        r.x.ax = 2;  /* Hide cursor */
        int86(0x33, &r, &r);
        r.x.ax = 4;  /* start in the centre (see getMouseDelta) */
        r.x.cx = MOUSE_CX;
        r.x.dx = MOUSE_CY;
        int86(0x33, &r, &r);
    }
}

/* Mouse look reads the cursor position and puts it back in the screen centre
 * every frame. Motion counters (fn 11) look simpler, but Windows XP's NTVDM
 * derives them from the cursor position, which stops dead at a screen edge:
 * turning one way worked and the other way stalled. In mode 13h the driver
 * reports x doubled (0..639) and y 0..199. */
void getMouseDelta(int *dx, int *dy) {
    union REGS r;

    if (!mouseAvailable) {
        *dx = 0;
        *dy = 0;
        return;
    }

    r.x.ax = 3;                       /* position + buttons */
    int86(0x33, &r, &r);
    *dx = ((short)r.x.cx - MOUSE_CX) / 2;
    *dy = (short)r.x.dx - MOUSE_CY;

    if (*dx != 0 || *dy != 0) {
        r.x.ax = 4;                   /* put the cursor back in the centre */
        r.x.cx = MOUSE_CX;
        r.x.dx = MOUSE_CY;
        int86(0x33, &r, &r);
    }
}

int getMouseButton(void) {
    union REGS r;

    if (!mouseAvailable) return 0;

    r.x.ax = 3;
    int86(0x33, &r, &r);

    return (r.x.bx & 1);
}

/*============================================================================
 * PROJECTILE SYSTEM - P4 ERA FEATURE
 *===========================================================================*/

void initProjectiles(void) {
    int i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = 0;
    }
}

/* Fire a projectile */
int fireProjectile(double x, double y, double dirX, double dirY, int isPlayer) {
    int i;

    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].x = x;
            projectiles[i].y = y;
            projectiles[i].dirX = dirX;
            projectiles[i].dirY = dirY;
            projectiles[i].active = 1;
            projectiles[i].isPlayerProjectile = isPlayer;
            projectiles[i].color = isPlayer ? COLOR_YELLOW : COLOR_LRED;
            return 1;
        }
    }
    return 0;  /* No free slot */
}

/* Update all projectiles */
void updateProjectiles(void) {
    int i, j;

    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        /* Move projectile */
        double newX = projectiles[i].x + projectiles[i].dirX * PROJECTILE_SPEED * frameDt;
        double newY = projectiles[i].y + projectiles[i].dirY * PROJECTILE_SPEED * frameDt;

        /* Check wall collision */
        if (worldMap[(int)newY][(int)newX] != 0) {
            projectiles[i].active = 0;
            continue;
        }

        /* Check player collision (enemy projectiles) */
        if (!projectiles[i].isPlayerProjectile) {
            double dx = newX - player.x;
            double dy = newY - player.y;
            if (dx*dx + dy*dy < 0.25) {  /* Hit radius 0.5 */
                player.health -= 10;
                hurtUntil = gameMs() + 250;
                soundPlayerHurt();  /* [SEC-08] Sound effect */
                projectiles[i].active = 0;
                continue;
            }
        }

        /* Check enemy collision (player projectiles) */
        if (projectiles[i].isPlayerProjectile) {
            for (j = 0; j < numEnemies; j++) {
                if (!enemies[j].active) continue;

                double dx = newX - enemies[j].x;
                double dy = newY - enemies[j].y;
                if (dx*dx + dy*dy < 0.36) {  /* Hit radius 0.6 */
                    enemies[j].health -= 25;
                    soundHit();  /* [SEC-08] Sound effect */
                    projectiles[i].active = 0;

                    if (enemies[j].health <= 0) {
                        enemies[j].active = 0;
                        enemies[j].deathTime = gameMs();  /* Record death time for respawn */
                        player.score += (enemies[j].type + 1) * 100;
                        soundEnemyDeath();  /* [SEC-08] Sound effect */
                    }
                    break;
                }
            }
        }

        projectiles[i].x = newX;
        projectiles[i].y = newY;

        /* Kill projectile if too far */
        double distFromPlayer = sqrt(
            (projectiles[i].x - player.x) * (projectiles[i].x - player.x) +
            (projectiles[i].y - player.y) * (projectiles[i].y - player.y)
        );
        if (distFromPlayer > 30.0) {
            projectiles[i].active = 0;
        }
    }
}

/*============================================================================
 * ENEMY AI - P4 ERA: ENEMIES SHOOT BACK!
 *===========================================================================*/

void initEnemies(void) {
    numEnemies = 0;

    /* Spawn enemies at fixed positions */
    /* Grunts - don't shoot */
    enemies[numEnemies].x = enemies[numEnemies].spawnX = 5.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 5.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = enemies[numEnemies].maxHealth = 50;
    enemies[numEnemies].type = ENEMY_GRUNT;
    enemies[numEnemies].canShoot = 0;
    enemies[numEnemies].lastFireTime = 0;
    enemies[numEnemies].deathTime = 0;
    numEnemies++;

    /* Soldiers - they shoot! */
    enemies[numEnemies].x = enemies[numEnemies].spawnX = 18.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 5.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = enemies[numEnemies].maxHealth = 75;
    enemies[numEnemies].type = ENEMY_SOLDIER;
    enemies[numEnemies].canShoot = 1;
    enemies[numEnemies].lastFireTime = 0;
    enemies[numEnemies].deathTime = 0;
    numEnemies++;

    enemies[numEnemies].x = enemies[numEnemies].spawnX = 10.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 13.5;   /* (was 12.5: inside a wall cell) */
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = enemies[numEnemies].maxHealth = 75;
    enemies[numEnemies].type = ENEMY_SOLDIER;
    enemies[numEnemies].canShoot = 1;
    enemies[numEnemies].lastFireTime = 0;
    enemies[numEnemies].deathTime = 0;
    numEnemies++;

    /* Elite - shoots faster */
    enemies[numEnemies].x = enemies[numEnemies].spawnX = 18.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 18.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = enemies[numEnemies].maxHealth = 150;
    enemies[numEnemies].type = ENEMY_ELITE;
    enemies[numEnemies].canShoot = 1;
    enemies[numEnemies].lastFireTime = 0;
    enemies[numEnemies].deathTime = 0;
    numEnemies++;

    /* More grunts */
    enemies[numEnemies].x = enemies[numEnemies].spawnX = 3.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 15.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = enemies[numEnemies].maxHealth = 50;
    enemies[numEnemies].type = ENEMY_GRUNT;
    enemies[numEnemies].canShoot = 0;
    enemies[numEnemies].lastFireTime = 0;
    enemies[numEnemies].deathTime = 0;
    numEnemies++;

    enemies[numEnemies].x = enemies[numEnemies].spawnX = 20.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 11.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = enemies[numEnemies].maxHealth = 50;
    enemies[numEnemies].type = ENEMY_GRUNT;
    enemies[numEnemies].canShoot = 0;
    enemies[numEnemies].lastFireTime = 0;
    enemies[numEnemies].deathTime = 0;
    numEnemies++;
}

/* Initialize torches at wall corners/edges */
void initTorches(void) {
    numTorches = 0;

    /* Dungeon entrance - against left wall (x=0) */
    torches[numTorches].x = 0.15; torches[numTorches].y = 1.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.0; numTorches++;
    /* Against wall at x=5 */
    torches[numTorches].x = 4.85; torches[numTorches].y = 1.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.0; numTorches++;

    /* Corner at row 4 wall */
    torches[numTorches].x = 3.15; torches[numTorches].y = 3.85;
    torches[numTorches].active = 1; torches[numTorches].intensity = 0.9; numTorches++;

    /* Corridor - against left wall */
    torches[numTorches].x = 0.15; torches[numTorches].y = 5.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.0; numTorches++;

    /* Central hub - on pillars */
    torches[numTorches].x = 2.15; torches[numTorches].y = 8.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.2; numTorches++;
    torches[numTorches].x = 6.15; torches[numTorches].y = 8.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.1; numTorches++;
    torches[numTorches].x = 8.15; torches[numTorches].y = 9.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.1; numTorches++;

    /* Stone area - on walls */
    torches[numTorches].x = 11.15; torches[numTorches].y = 6.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.0; numTorches++;
    torches[numTorches].x = 15.15; torches[numTorches].y = 6.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.0; numTorches++;

    /* Prison area - on walls */
    torches[numTorches].x = 0.15; torches[numTorches].y = 13.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 0.8; numTorches++;
    torches[numTorches].x = 4.15; torches[numTorches].y = 13.85;
    torches[numTorches].active = 1; torches[numTorches].intensity = 0.8; numTorches++;

    /* Tech/green area */
    torches[numTorches].x = 17.15; torches[numTorches].y = 5.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.3; numTorches++;
    torches[numTorches].x = 19.85; torches[numTorches].y = 9.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.2; numTorches++;

    /* Exit area */
    torches[numTorches].x = 21.15; torches[numTorches].y = 21.5;
    torches[numTorches].active = 1; torches[numTorches].intensity = 1.5; numTorches++;
}

/* Calculate light level at a position based on torches */
double getTorchLight(double x, double y) {
    double totalLight = 0.7;  /* Base ambient light - good visibility */
    int i;

    for (i = 0; i < numTorches; i++) {
        if (!torches[i].active) continue;

        double dx = x - torches[i].x;
        double dy = y - torches[i].y;
        double dist = sqrt(dx*dx + dy*dy);

        if (dist < TORCH_LIGHT_RADIUS) {
            /* Inverse square falloff with torch intensity */
            double falloff = 1.0 - (dist / TORCH_LIGHT_RADIUS);
            totalLight += falloff * falloff * torches[i].intensity;
        }
    }

    /* Clamp to 0.0 - 1.5 range */
    if (totalLight > 1.5) totalLight = 1.5;
    return totalLight;
}

/* Torches never move, so bake their light per map cell once at startup
 * instead of summing 16 square roots per screen column per frame. */
static double lightMap[MAP_HEIGHT][MAP_WIDTH];
void buildLightMap(void) {
    int x, y;
    for (y = 0; y < MAP_HEIGHT; y++)
        for (x = 0; x < MAP_WIDTH; x++)
            lightMap[y][x] = getTorchLight(x + 0.5, y + 0.5);
}

/* Check line of sight from enemy to player */
int enemyCanSeePlayer(int enemyIdx) {
    double dx = player.x - enemies[enemyIdx].x;
    double dy = player.y - enemies[enemyIdx].y;
    double dist = sqrt(dx*dx + dy*dy);
    double stepX, stepY, rayX, rayY;
    int steps, i;

    if (dist > ENEMY_FIRE_RANGE) return 0;

    /* Raycast to check for walls */
    stepX = dx / dist * 0.1;
    stepY = dy / dist * 0.1;
    rayX = enemies[enemyIdx].x;
    rayY = enemies[enemyIdx].y;
    steps = (int)(dist / 0.1);

    for (i = 0; i < steps; i++) {
        rayX += stepX;
        rayY += stepY;
        if (worldMap[(int)rayY][(int)rayX] != 0) {
            return 0;  /* Wall blocks view */
        }
    }

    return 1;  /* Clear line of sight */
}

void updateEnemyAI(void) {
    unsigned long now = gameMs();
    int i;

    /* Check for enemy respawns */
    for (i = 0; i < numEnemies; i++) {
        if (!enemies[i].active && enemies[i].deathTime > 0) {
            unsigned long elapsed = now - enemies[i].deathTime;
            if (elapsed > ENEMY_RESPAWN_TIME) {
                /* Respawn the enemy at spawn point */
                enemies[i].x = enemies[i].spawnX;
                enemies[i].y = enemies[i].spawnY;
                enemies[i].health = enemies[i].maxHealth;
                enemies[i].active = 1;
                enemies[i].deathTime = 0;
            }
        }
    }

    for (i = 0; i < numEnemies; i++) {
        if (!enemies[i].active) continue;

        /* Calculate direction to player */
        double dx = player.x - enemies[i].x;
        double dy = player.y - enemies[i].y;
        double dist = sqrt(dx*dx + dy*dy);

        /* Move toward player (slowly) */
        if (dist > 1.5) {
            double moveX = (dx / dist) * ENEMY_SPEED * frameDt;
            double moveY = (dy / dist) * ENEMY_SPEED * frameDt;

            double newX = enemies[i].x + moveX;
            double newY = enemies[i].y + moveY;

            if (worldMap[(int)newY][(int)enemies[i].x] == 0) {
                enemies[i].y = newY;
            }
            if (worldMap[(int)enemies[i].y][(int)newX] == 0) {
                enemies[i].x = newX;
            }
        }

        /* SHOOTING AI - P4 ERA FEATURE */
        if (enemies[i].canShoot && enemyCanSeePlayer(i)) {
            /* Fire rate depends on enemy type */
            int fireRate = ENEMY_FIRE_RATE;
            if (enemies[i].type == ENEMY_ELITE) fireRate = 1200;  /* Faster */
            if (enemies[i].type == ENEMY_BOSS) fireRate = 800;    /* Very fast */

            unsigned long elapsed = now - enemies[i].lastFireTime;

            if (elapsed > fireRate) {
                /* Fire at player! */
                double dirX = dx / dist;
                double dirY = dy / dist;
                fireProjectile(enemies[i].x, enemies[i].y, dirX, dirY, 0);
                enemies[i].lastFireTime = now;
            }
        }
    }
}

/*============================================================================
 * PLAYER
 *===========================================================================*/

void initPlayer(void) {
    player.x = 1.5;  /* Start in entrance corridor */
    player.y = 1.5;
    player.angle = 0.0;
    player.dirX = 1.0;
    player.dirY = 0.0;
    player.planeX = 0.0;
    player.planeY = 0.66;
    player.health = PLAYER_STARTING_HEALTH;
    player.ammo = PLAYER_STARTING_AMMO;
    player.score = 0;
}

void movePlayer(double moveDir) {
    double newX = player.x + player.dirX * moveDir * MOVE_SPEED;
    double newY = player.y + player.dirY * moveDir * MOVE_SPEED;
    int moved = 0;

    if (worldMap[(int)player.y][(int)newX] == 0) {
        player.x = newX;
        moved = 1;
    }
    if (worldMap[(int)newY][(int)player.x] == 0) {
        player.y = newY;
        moved = 1;
    }
    if (moved) soundStep();
}

void strafePlayer(double strafeDir) {
    double newX = player.x + player.planeX * strafeDir * MOVE_SPEED;
    double newY = player.y + player.planeY * strafeDir * MOVE_SPEED;
    int moved = 0;

    if (worldMap[(int)player.y][(int)newX] == 0) {
        player.x = newX;
        moved = 1;
    }
    if (worldMap[(int)newY][(int)player.x] == 0) {
        player.y = newY;
        moved = 1;
    }
    if (moved) soundStep();
}

void rotatePlayer(double angle) {
    double oldDirX = player.dirX;
    double oldPlaneX = player.planeX;

    player.dirX = player.dirX * cos(angle) - player.dirY * sin(angle);
    player.dirY = oldDirX * sin(angle) + player.dirY * cos(angle);

    player.planeX = player.planeX * cos(angle) - player.planeY * sin(angle);
    player.planeY = oldPlaneX * sin(angle) + player.planeY * cos(angle);

    player.angle += angle;
}

void playerShoot(void) {
    if (player.ammo > 0) {
        fireProjectile(player.x, player.y, player.dirX, player.dirY, 1);
        player.ammo--;
        soundShoot();  /* [SEC-08] Sound effect */
    }
}

/*============================================================================
 * RENDERING - P4 ERA HIGH-RES TEXTURES
 *===========================================================================*/

void renderFrame(void) {
    int x, y;

    /* Draw ceiling - night sky with stars */
    {
        int horizonY = SCREEN_CENTER + playerPitch;
        for (y = 0; y < horizonY && y < SCREEN_HEIGHT; y++) {
            /* Dark sky gradient - black at top, dark gray near the horizon */
            memset(backBuffer + y * SCREEN_WIDTH, (horizonY - y < 15) ? 8 : 0, SCREEN_WIDTH);
        }

        /* Add stars to sky - shift with player angle for parallax */
        {
            int i, sx, sy;
            int starOffset = (int)(player.angle * 80);
            for (i = 0; i < 60; i++) {
                /* Deterministic star positions */
                sx = ((i * 97 + 13 + starOffset) % SCREEN_WIDTH);
                if (sx < 0) sx += SCREEN_WIDTH;
                sy = ((i * 43 + 7) % 70) + 5;  /* Upper part of sky */
                if (sy < horizonY - 15) {
                    unsigned char starColor = (i % 5 == 0) ? 15 : 7;  /* Bright or dim white */
                    backBuffer[sy * SCREEN_WIDTH + sx] = starColor;
                }
            }
        }
    }

    /* Floor - red/brown tiles locked to world position. One perspective divide
     * per row, then the world position steps across the row in 16.16 fixed
     * point (scaled x4 = 4 tiles per unit), so the pixel loop is integer only. */
    {
        int horizonY = SCREEN_CENTER + playerPitch;
        double rayDirX0 = player.dirX - player.planeX, rayDirY0 = player.dirY - player.planeY;
        double rayDirX1 = player.dirX + player.planeX, rayDirY1 = player.dirY + player.planeY;
        for (y = horizonY < 0 ? 0 : horizonY; y < SCREEN_HEIGHT; y++) {
            int distFromHorizon = y - horizonY;
            double rowDist = (double)(SCREEN_HEIGHT / 2) / (distFromHorizon + 0.1);
            unsigned char *row = backBuffer + y * SCREEN_WIDTH;
            unsigned char fadeColor;
            long fx, fy, fdx, fdy;
            if (distFromHorizon < 8 || rowDist > 10.0) {   /* horizon band and far rows fade to black */
                memset(row, 0, SCREEN_WIDTH);
                continue;
            }
            fadeColor = (rowDist > 6.0) ? 4 : 0;             /* mid distance: both tiles go dark red */
            fx  = (long)((player.x + rowDist * rayDirX0) * (4.0 * 65536.0));
            fy  = (long)((player.y + rowDist * rayDirY0) * (4.0 * 65536.0));
            fdx = (long)(rowDist * (rayDirX1 - rayDirX0) * (4.0 * 65536.0) / SCREEN_WIDTH);
            fdy = (long)(rowDist * (rayDirY1 - rayDirY0) * (4.0 * 65536.0) / SCREEN_WIDTH);
            for (x = 0; x < SCREEN_WIDTH; x++) {
                int cellX = (int)(fx >> 16), cellY = (int)(fy >> 16);
                unsigned char color;
                if ((cellX & 7) == 0 || (cellY & 7) == 0) color = 0;   /* black grout lines */
                else if (fadeColor) color = fadeColor;
                else color = ((cellX + cellY) & 1) ? 4 : 6;           /* dark red / brown checker */
                row[x] = color;
                fx += fdx;
                fy += fdy;
            }
        }
    }

    /* Raycasting */
    for (x = 0; x < SCREEN_WIDTH; x++) {
        double cameraX = 2 * x / (double)SCREEN_WIDTH - 1;
        double rayDirX = player.dirX + player.planeX * cameraX;
        double rayDirY = player.dirY + player.planeY * cameraX;

        int mapX = (int)player.x;
        int mapY = (int)player.y;

        double sideDistX, sideDistY;
        double deltaDistX = fabs(1 / rayDirX);
        double deltaDistY = fabs(1 / rayDirY);
        double perpWallDist;

        int stepX, stepY;
        int hit = 0;
        int side;

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = (player.x - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0 - player.x) * deltaDistX;
        }
        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = (player.y - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0 - player.y) * deltaDistY;
        }

        /* DDA */
        while (hit == 0) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }
            if (worldMap[mapY][mapX] > 0) hit = 1;
        }

        if (side == 0) {
            perpWallDist = (mapX - player.x + (1 - stepX) / 2) / rayDirX;
        } else {
            perpWallDist = (mapY - player.y + (1 - stepY) / 2) / rayDirY;
        }

        zBuffer[x] = perpWallDist;

        int lineHeight = (int)(SCREEN_HEIGHT / perpWallDist);
        if (lineHeight < 1) lineHeight = 1;
        int drawStart = -lineHeight / 2 + SCREEN_CENTER + playerPitch;
        int drawEnd = lineHeight / 2 + SCREEN_CENTER + playerPitch;
        if (drawStart < 0) drawStart = 0;
        if (drawEnd >= SCREEN_HEIGHT) drawEnd = SCREEN_HEIGHT - 1;

        /* Calculate texture coordinate */
        double wallX;
        if (side == 0) wallX = player.y + perpWallDist * rayDirY;
        else wallX = player.x + perpWallDist * rayDirX;
        wallX -= floor(wallX);

        int texX = (int)(wallX * TEX_SIZE);
        if ((side == 0 && rayDirX > 0) || (side == 1 && rayDirY < 0)) {
            texX = TEX_SIZE - texX - 1;
        }

        /* Get wall type and texture */
        int wallType = worldMap[mapY][mapX] - 1;
        if (wallType < 0) wallType = 0;
        if (wallType > 3) wallType = 3;

        /* Draw textured vertical line - texture row steps in 16.16 fixed point */
        long texStep = (long)((double)TEX_SIZE * 65536.0 / lineHeight);
        long texPos = (long)(drawStart - SCREEN_CENTER - playerPitch + lineHeight / 2) * texStep;

        /* Baked torch light for this wall cell */
        double torchLight = lightMap[mapY][mapX];

        /* Check for wall decorations (paintings and TVs) */
        /* Hash wall position for pseudo-random decoration placement */
        int decorHash = (mapX * 7 + mapY * 13) % 17;
        int hasDecor = (decorHash < 5);  /* ~30% of walls have decorations */
        int decorType = decorHash % 3;   /* 0=painting, 1=TV, 2=painting */

        for (y = drawStart; y < drawEnd; y++) {
            int texY = (int)(texPos >> 16) & TEX_MASK;
            texPos += texStep;

            unsigned char color = wallTextures[wallType][texY * TEX_SIZE + texX];

            /* Check if we're in decoration area (middle of wall texture) */
            int inDecorX = (texX >= 8 && texX < 56);
            int inDecorY = (texY >= 12 && texY < 52);

            if (hasDecor && inDecorX && inDecorY && perpWallDist < 6.0) {
                /* Draw decoration */
                int decorX = texX - 8;
                int decorY = texY - 12;
                int decorW = 48;
                int decorH = 40;

                /* Frame border */
                if (decorX < 3 || decorX >= decorW - 3 ||
                    decorY < 3 || decorY >= decorH - 3) {
                    if (decorType == 1) {
                        color = 8;  /* Gray frame for TV */
                    } else {
                        color = 6;  /* Brown frame for painting */
                    }
                } else if (decorType == 1) {
                    /* Broken TV - black with static/cracks */
                    int crackHash = (decorX * 3 + decorY * 7 + mapX + mapY) % 13;
                    if (crackHash == 0 || crackHash == 5) {
                        color = 7;  /* Gray crack lines */
                    } else if (crackHash == 1) {
                        color = 8;  /* Lighter static */
                    } else {
                        color = 0;  /* Black screen */
                    }
                } else {
                    /* Painting - abstract art */
                    int artHash = (decorX / 6 + decorY / 5 + mapX * 3 + mapY * 5) % 8;
                    switch (artHash) {
                        case 0: color = 4; break;   /* Red */
                        case 1: color = 1; break;   /* Blue */
                        case 2: color = 2; break;   /* Green */
                        case 3: color = 14; break;  /* Yellow */
                        case 4: color = 5; break;   /* Magenta */
                        case 5: color = 3; break;   /* Cyan */
                        case 6: color = 6; break;   /* Brown */
                        default: color = 7; break;  /* Gray */
                    }
                }
            } else {
                /* Normal wall texture with lighting */
                /* Base dimming - start darker so torches show up */
                int brightness = color;
                if (brightness >= 16 && brightness <= 31) {
                    brightness -= 4;  /* Make base darker */
                    if (brightness < 16) brightness = 16;
                }

                /* Apply torch lighting - brighten near torches */
                if (torchLight > 0.3) {
                    int boost = (int)((torchLight - 0.3) * 10);
                    brightness += boost;
                    if (brightness > 31) brightness = 31;
                }

                /* Distance darkening */
                if (perpWallDist > 8.0) {
                    int darken = (int)((perpWallDist - 8.0) * 2);
                    brightness -= darken;
                    if (brightness < 16) brightness = 16;
                }

                /* Side shading - darken one side */
                if (side == 1) {
                    brightness -= 2;
                    if (brightness < 16) brightness = 16;
                }

                color = brightness;
            }

            backBuffer[y * SCREEN_WIDTH + x] = color;
        }
    }
}

/*============================================================================
 * SPRITE RENDERING
 *===========================================================================*/

void renderSprites(void) {
    int i, x, y;
    unsigned char *sprite;
    int sprWidth, sprHeight;

    /* Render enemies using bitmap sprites */
    for (i = 0; i < numEnemies; i++) {
        if (!enemies[i].active) continue;

        double spriteX = enemies[i].x - player.x;
        double spriteY = enemies[i].y - player.y;

        double invDet = 1.0 / (player.planeX * player.dirY - player.dirX * player.planeY);
        double transformX = invDet * (player.dirY * spriteX - player.dirX * spriteY);
        double transformY = invDet * (-player.planeY * spriteX + player.planeX * spriteY);

        if (transformY <= 0.2) continue;

        int spriteScreenX = (int)((SCREEN_WIDTH / 2) * (1 + transformX / transformY));
        int spriteHeight = abs((int)(SCREEN_HEIGHT / transformY));
        int spriteWidth = spriteHeight * ENEMY_SPRITE_WIDTH / ENEMY_SPRITE_HEIGHT;

        /* Skip if sprite too small or too big */
        if (spriteWidth < 2 || spriteHeight < 2) continue;
        if (spriteWidth > SCREEN_WIDTH * 2) continue;

        int drawStartY = -spriteHeight / 2 + SCREEN_CENTER + playerPitch;
        int drawEndY = spriteHeight / 2 + SCREEN_CENTER + playerPitch;
        int drawStartX = -spriteWidth / 2 + spriteScreenX;
        int drawEndX = spriteWidth / 2 + spriteScreenX;

        /* Clip to screen */
        if (drawEndX < 0 || drawStartX >= SCREEN_WIDTH) continue;
        if (drawEndY < 0 || drawStartY >= SCREEN_HEIGHT) continue;

        if (drawStartY < 0) drawStartY = 0;
        if (drawEndY >= SCREEN_HEIGHT) drawEndY = SCREEN_HEIGHT - 1;
        if (drawStartX < 0) drawStartX = 0;
        if (drawEndX >= SCREEN_WIDTH) drawEndX = SCREEN_WIDTH - 1;

        /* Select sprite based on enemy type - using 64x64 sprites */
        switch (enemies[i].type) {
            case ENEMY_GRUNT: sprite = (unsigned char *)sprite_minion; break;
            case ENEMY_SOLDIER: sprite = (unsigned char *)sprite_minion; break;
            case ENEMY_ELITE: sprite = (unsigned char *)sprite_creeper; break;
            case ENEMY_BOSS: sprite = (unsigned char *)sprite_snowman; break;
            default: sprite = (unsigned char *)sprite_minion; break;
        }
        sprWidth = 64;
        sprHeight = 64;

        /* Draw scaled bitmap sprite with z-buffer test. Texture coordinates
         * step in 16.16 fixed point so there are no divides in the pixel loops. */
        {
            long stepX = ((long)sprWidth << 16) / spriteWidth;
            long stepY = ((long)sprHeight << 16) / spriteHeight;
            long texXf = (long)(drawStartX - (-spriteWidth / 2 + spriteScreenX)) * stepX;
            long texY0 = (long)(drawStartY - (-spriteHeight / 2 + SCREEN_CENTER + playerPitch)) * stepY;
            for (x = drawStartX; x < drawEndX; x++, texXf += stepX) {
                int texX = (int)(texXf >> 16);
                long texYf = texY0;
                unsigned char *dst;
                const unsigned char *col;
                if (transformY >= zBuffer[x]) continue;   /* a wall is in front of this column */
                if (texX < 0) texX = 0;
                if (texX >= sprWidth) texX = sprWidth - 1;
                col = sprite + texX;
                dst = backBuffer + drawStartY * SCREEN_WIDTH + x;
                for (y = drawStartY; y < drawEndY; y++, texYf += stepY, dst += SCREEN_WIDTH) {
                    int texY = (int)(texYf >> 16);
                    unsigned char pixel;
                    if (texY < 0) texY = 0;
                    if (texY >= sprHeight) texY = sprHeight - 1;
                    pixel = col[texY * sprWidth];
                    if (pixel != 255 && pixel != 0) *dst = pixel;   /* 255 or 0 = transparent */
                }
            }
        }
    }

    /* Render projectiles */
    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        double spriteX = projectiles[i].x - player.x;
        double spriteY = projectiles[i].y - player.y;

        double invDet = 1.0 / (player.planeX * player.dirY - player.dirX * player.planeY);
        double transformX = invDet * (player.dirY * spriteX - player.dirX * spriteY);
        double transformY = invDet * (-player.planeY * spriteX + player.planeX * spriteY);

        if (transformY <= 0.1) continue;

        int screenX = (int)((SCREEN_WIDTH / 2) * (1 + transformX / transformY));
        int screenY = SCREEN_CENTER + playerPitch;

        if (screenX >= 0 && screenX < SCREEN_WIDTH && transformY < zBuffer[screenX]) {
            /* Draw projectile as bright dot */
            int size = (int)(8 / transformY);
            int px, py, drawX, drawY;
            if (size < 1) size = 1;
            if (size > 10) size = 10;

            for (py = -size; py <= size; py++) {
                for (px = -size; px <= size; px++) {
                    drawX = screenX + px;
                    drawY = screenY + py;
                    if (drawX >= 0 && drawX < SCREEN_WIDTH &&
                        drawY >= 0 && drawY < SCREEN_HEIGHT) {
                        if (px*px + py*py <= size*size) {
                            backBuffer[drawY * SCREEN_WIDTH + drawX] = projectiles[i].color;
                        }
                    }
                }
            }
        }
    }

    /* Render torches with animated flames */
    {
        static int flameFrame = 0;
        flameFrame++;

        for (i = 0; i < numTorches; i++) {
            if (!torches[i].active) continue;

            double spriteX = torches[i].x - player.x;
            double spriteY = torches[i].y - player.y;

            double invDet = 1.0 / (player.planeX * player.dirY - player.dirX * player.planeY);
            double transformX = invDet * (player.dirY * spriteX - player.dirX * spriteY);
            double transformY = invDet * (-player.planeY * spriteX + player.planeX * spriteY);

            if (transformY <= 0.2) continue;  /* Behind player */

            int spriteScreenX = (int)((SCREEN_WIDTH / 2) * (1 + transformX / transformY));

            /* Torch size based on distance */
            int torchHeight = (int)(48 / transformY);
            int torchWidth = (int)(16 / transformY);
            if (torchHeight < 4) continue;
            if (torchHeight > 200) torchHeight = 200;
            if (torchWidth < 2) torchWidth = 2;
            if (torchWidth > 60) torchWidth = 60;

            /* Position - mount on wall, slightly above center */
            int drawStartY = SCREEN_CENTER + playerPitch - torchHeight;
            int drawEndY = SCREEN_CENTER + playerPitch;
            int drawStartX = spriteScreenX - torchWidth / 2;
            int drawEndX = spriteScreenX + torchWidth / 2;

            /* Clip to screen */
            if (drawEndX < 0 || drawStartX >= SCREEN_WIDTH) continue;
            if (drawStartY < 0) drawStartY = 0;
            if (drawEndY >= SCREEN_HEIGHT) drawEndY = SCREEN_HEIGHT - 1;
            if (drawStartX < 0) drawStartX = 0;
            if (drawEndX >= SCREEN_WIDTH) drawEndX = SCREEN_WIDTH - 1;

            /* Draw torch with animated flame */
            for (x = drawStartX; x < drawEndX; x++) {
                /* Z-buffer check */
                if (transformY >= zBuffer[x]) continue;

                for (y = drawStartY; y < drawEndY; y++) {
                    int relY = y - drawStartY;
                    int relX = x - spriteScreenX;
                    int flameHeight = torchHeight * 2 / 3;  /* Top 2/3 is flame */

                    unsigned char color = 0;

                    if (relY >= flameHeight) {
                        /* Handle/bracket part - brown/gray */
                        if (relX >= -torchWidth/4 && relX <= torchWidth/4) {
                            color = 6;  /* Brown handle */
                        }
                    } else {
                        /* Flame part - animated orange/yellow/red */
                        int distFromCenter = (relX < 0) ? -relX : relX;

                        /* Flame narrows toward top */
                        int maxWidth = (torchWidth / 2) * (flameHeight - relY) / flameHeight;

                        if (distFromCenter <= maxWidth) {
                            /* Animate flame colors */
                            int flicker = ((flameFrame + relY * 3 + x * 7) / 4) % 4;
                            if (relY < flameHeight / 4) {
                                /* Top of flame - yellow */
                                color = (flicker == 0) ? 14 : 44;  /* Yellow/bright yellow */
                            } else if (relY < flameHeight / 2) {
                                /* Middle - orange */
                                color = (flicker < 2) ? 44 : 6;  /* Orange/brown */
                            } else {
                                /* Base - red/orange */
                                color = (flicker == 0) ? 4 : 6;  /* Red/brown */
                            }
                        }
                    }

                    if (color != 0) {
                        backBuffer[y * SCREEN_WIDTH + x] = color;
                    }
                }
            }
        }
    }
}

/*============================================================================
 * HUD
 *===========================================================================*/

/* Simple 8x8 font - initialized at runtime */
static unsigned char font8x8[128][8];

void initFont(void) {
    int i, j;
    /* Clear all */
    for (i = 0; i < 128; i++) {
        for (j = 0; j < 8; j++) {
            font8x8[i][j] = 0;
        }
    }
    /* Numbers */
    font8x8['0'][0]=0x3C; font8x8['0'][1]=0x66; font8x8['0'][2]=0x6E; font8x8['0'][3]=0x76;
    font8x8['0'][4]=0x66; font8x8['0'][5]=0x66; font8x8['0'][6]=0x3C;
    font8x8['1'][0]=0x18; font8x8['1'][1]=0x38; font8x8['1'][2]=0x18; font8x8['1'][3]=0x18;
    font8x8['1'][4]=0x18; font8x8['1'][5]=0x18; font8x8['1'][6]=0x7E;
    font8x8['2'][0]=0x3C; font8x8['2'][1]=0x66; font8x8['2'][2]=0x06; font8x8['2'][3]=0x0C;
    font8x8['2'][4]=0x18; font8x8['2'][5]=0x30; font8x8['2'][6]=0x7E;
    font8x8['3'][0]=0x3C; font8x8['3'][1]=0x66; font8x8['3'][2]=0x06; font8x8['3'][3]=0x1C;
    font8x8['3'][4]=0x06; font8x8['3'][5]=0x66; font8x8['3'][6]=0x3C;
    font8x8['4'][0]=0x0C; font8x8['4'][1]=0x1C; font8x8['4'][2]=0x2C; font8x8['4'][3]=0x4C;
    font8x8['4'][4]=0x7E; font8x8['4'][5]=0x0C; font8x8['4'][6]=0x0C;
    font8x8['5'][0]=0x7E; font8x8['5'][1]=0x60; font8x8['5'][2]=0x7C; font8x8['5'][3]=0x06;
    font8x8['5'][4]=0x06; font8x8['5'][5]=0x66; font8x8['5'][6]=0x3C;
    font8x8['6'][0]=0x1C; font8x8['6'][1]=0x30; font8x8['6'][2]=0x60; font8x8['6'][3]=0x7C;
    font8x8['6'][4]=0x66; font8x8['6'][5]=0x66; font8x8['6'][6]=0x3C;
    font8x8['7'][0]=0x7E; font8x8['7'][1]=0x06; font8x8['7'][2]=0x0C; font8x8['7'][3]=0x18;
    font8x8['7'][4]=0x30; font8x8['7'][5]=0x30; font8x8['7'][6]=0x30;
    font8x8['8'][0]=0x3C; font8x8['8'][1]=0x66; font8x8['8'][2]=0x66; font8x8['8'][3]=0x3C;
    font8x8['8'][4]=0x66; font8x8['8'][5]=0x66; font8x8['8'][6]=0x3C;
    font8x8['9'][0]=0x3C; font8x8['9'][1]=0x66; font8x8['9'][2]=0x66; font8x8['9'][3]=0x3E;
    font8x8['9'][4]=0x06; font8x8['9'][5]=0x0C; font8x8['9'][6]=0x38;
    /* Letters */
    font8x8['A'][0]=0x3C; font8x8['A'][1]=0x66; font8x8['A'][2]=0x66; font8x8['A'][3]=0x7E;
    font8x8['A'][4]=0x66; font8x8['A'][5]=0x66; font8x8['A'][6]=0x66;
    font8x8['B'][0]=0x7C; font8x8['B'][1]=0x66; font8x8['B'][2]=0x66; font8x8['B'][3]=0x7C;
    font8x8['B'][4]=0x66; font8x8['B'][5]=0x66; font8x8['B'][6]=0x7C;
    font8x8['C'][0]=0x3C; font8x8['C'][1]=0x66; font8x8['C'][2]=0x60; font8x8['C'][3]=0x60;
    font8x8['C'][4]=0x60; font8x8['C'][5]=0x66; font8x8['C'][6]=0x3C;
    font8x8['D'][0]=0x78; font8x8['D'][1]=0x6C; font8x8['D'][2]=0x66; font8x8['D'][3]=0x66;
    font8x8['D'][4]=0x66; font8x8['D'][5]=0x6C; font8x8['D'][6]=0x78;
    font8x8['E'][0]=0x7E; font8x8['E'][1]=0x60; font8x8['E'][2]=0x60; font8x8['E'][3]=0x7C;
    font8x8['E'][4]=0x60; font8x8['E'][5]=0x60; font8x8['E'][6]=0x7E;
    font8x8['F'][0]=0x7E; font8x8['F'][1]=0x60; font8x8['F'][2]=0x60; font8x8['F'][3]=0x7C;
    font8x8['F'][4]=0x60; font8x8['F'][5]=0x60; font8x8['F'][6]=0x60;
    font8x8['G'][0]=0x3C; font8x8['G'][1]=0x66; font8x8['G'][2]=0x60; font8x8['G'][3]=0x6E;
    font8x8['G'][4]=0x66; font8x8['G'][5]=0x66; font8x8['G'][6]=0x3C;
    font8x8['H'][0]=0x66; font8x8['H'][1]=0x66; font8x8['H'][2]=0x66; font8x8['H'][3]=0x7E;
    font8x8['H'][4]=0x66; font8x8['H'][5]=0x66; font8x8['H'][6]=0x66;
    font8x8['I'][0]=0x3C; font8x8['I'][1]=0x18; font8x8['I'][2]=0x18; font8x8['I'][3]=0x18;
    font8x8['I'][4]=0x18; font8x8['I'][5]=0x18; font8x8['I'][6]=0x3C;
    font8x8['K'][0]=0x66; font8x8['K'][1]=0x6C; font8x8['K'][2]=0x78; font8x8['K'][3]=0x70;
    font8x8['K'][4]=0x78; font8x8['K'][5]=0x6C; font8x8['K'][6]=0x66;
    font8x8['L'][0]=0x60; font8x8['L'][1]=0x60; font8x8['L'][2]=0x60; font8x8['L'][3]=0x60;
    font8x8['L'][4]=0x60; font8x8['L'][5]=0x60; font8x8['L'][6]=0x7E;
    font8x8['M'][0]=0x63; font8x8['M'][1]=0x77; font8x8['M'][2]=0x7F; font8x8['M'][3]=0x6B;
    font8x8['M'][4]=0x63; font8x8['M'][5]=0x63; font8x8['M'][6]=0x63;
    font8x8['N'][0]=0x66; font8x8['N'][1]=0x76; font8x8['N'][2]=0x7E; font8x8['N'][3]=0x7E;
    font8x8['N'][4]=0x6E; font8x8['N'][5]=0x66; font8x8['N'][6]=0x66;
    font8x8['O'][0]=0x3C; font8x8['O'][1]=0x66; font8x8['O'][2]=0x66; font8x8['O'][3]=0x66;
    font8x8['O'][4]=0x66; font8x8['O'][5]=0x66; font8x8['O'][6]=0x3C;
    font8x8['P'][0]=0x7C; font8x8['P'][1]=0x66; font8x8['P'][2]=0x66; font8x8['P'][3]=0x7C;
    font8x8['P'][4]=0x60; font8x8['P'][5]=0x60; font8x8['P'][6]=0x60;
    font8x8['R'][0]=0x7C; font8x8['R'][1]=0x66; font8x8['R'][2]=0x66; font8x8['R'][3]=0x7C;
    font8x8['R'][4]=0x6C; font8x8['R'][5]=0x66; font8x8['R'][6]=0x66;
    font8x8['S'][0]=0x3C; font8x8['S'][1]=0x66; font8x8['S'][2]=0x60; font8x8['S'][3]=0x3C;
    font8x8['S'][4]=0x06; font8x8['S'][5]=0x66; font8x8['S'][6]=0x3C;
    font8x8['T'][0]=0x7E; font8x8['T'][1]=0x18; font8x8['T'][2]=0x18; font8x8['T'][3]=0x18;
    font8x8['T'][4]=0x18; font8x8['T'][5]=0x18; font8x8['T'][6]=0x18;
    font8x8['U'][0]=0x66; font8x8['U'][1]=0x66; font8x8['U'][2]=0x66; font8x8['U'][3]=0x66;
    font8x8['U'][4]=0x66; font8x8['U'][5]=0x66; font8x8['U'][6]=0x3C;
    font8x8['V'][0]=0x66; font8x8['V'][1]=0x66; font8x8['V'][2]=0x66; font8x8['V'][3]=0x66;
    font8x8['V'][4]=0x66; font8x8['V'][5]=0x3C; font8x8['V'][6]=0x18;
    font8x8['W'][0]=0x63; font8x8['W'][1]=0x63; font8x8['W'][2]=0x63; font8x8['W'][3]=0x6B;
    font8x8['W'][4]=0x7F; font8x8['W'][5]=0x77; font8x8['W'][6]=0x63;
    font8x8['X'][0]=0x66; font8x8['X'][1]=0x66; font8x8['X'][2]=0x3C; font8x8['X'][3]=0x18;
    font8x8['X'][4]=0x3C; font8x8['X'][5]=0x66; font8x8['X'][6]=0x66;
    font8x8['Y'][0]=0x66; font8x8['Y'][1]=0x66; font8x8['Y'][2]=0x66; font8x8['Y'][3]=0x3C;
    font8x8['Y'][4]=0x18; font8x8['Y'][5]=0x18; font8x8['Y'][6]=0x18;
    font8x8['Z'][0]=0x7E; font8x8['Z'][1]=0x06; font8x8['Z'][2]=0x0C; font8x8['Z'][3]=0x18;
    font8x8['Z'][4]=0x30; font8x8['Z'][5]=0x60; font8x8['Z'][6]=0x7E;
    /* Symbols */
    font8x8[':'][1]=0x18; font8x8[':'][2]=0x18; font8x8[':'][4]=0x18; font8x8[':'][5]=0x18;
    font8x8['-'][3]=0x7E;
    font8x8['+'][1]=0x18; font8x8['+'][2]=0x18; font8x8['+'][3]=0x7E;
    font8x8['+'][4]=0x18; font8x8['+'][5]=0x18;
    font8x8['!'][0]=0x18; font8x8['!'][1]=0x18; font8x8['!'][2]=0x18; font8x8['!'][3]=0x18;
    font8x8['!'][4]=0x18; font8x8['!'][6]=0x18;
    font8x8['.'][6]=0x18;
}

void drawChar(int x, int y, char c, unsigned char color) {
    unsigned char uc = (unsigned char)c;
    int row, col;
    unsigned char rowData;

    if (uc >= 128) return;

    for (row = 0; row < 8; row++) {
        rowData = font8x8[uc][row];
        for (col = 0; col < 8; col++) {
            if (rowData & (0x80 >> col)) {
                setPixel(x + col, y + row, color);
            }
        }
    }
}

void drawText(int x, int y, const char *text, unsigned char color) {
    while (*text) {
        drawChar(x, y, *text, color);
        x += 8;
        text++;
    }
}

/* Draw mini map in corner */
void drawMiniMap(int mapX, int mapY, int mapSize) {
    int x, y, px, py;
    int cellSize = mapSize / MAP_WIDTH;
    int i;

    /* Draw map background */
    for (y = 0; y < mapSize; y++) {
        for (x = 0; x < mapSize; x++) {
            setPixel(mapX + x, mapY + y, COLOR_BLACK);
        }
    }

    /* Draw walls */
    for (y = 0; y < MAP_HEIGHT; y++) {
        for (x = 0; x < MAP_WIDTH; x++) {
            if (worldMap[y][x] != 0) {
                int sx = mapX + x * cellSize;
                int sy = mapY + y * cellSize;
                int i2, j;
                for (i2 = 0; i2 < cellSize; i2++) {
                    for (j = 0; j < cellSize; j++) {
                        setPixel(sx + i2, sy + j, COLOR_GRAY);
                    }
                }
            }
        }
    }

    /* Draw EXIT marker at SE corner (blinking yellow) */
    {
        int exitX = mapX + 22 * cellSize;
        int exitY = mapY + 22 * cellSize;
        unsigned char exitColor = ((gameMs() / 250) % 2) ? COLOR_YELLOW : COLOR_BWHITE;
        setPixel(exitX, exitY, exitColor);
        setPixel(exitX+1, exitY, exitColor);
        setPixel(exitX, exitY+1, exitColor);
        setPixel(exitX+1, exitY+1, exitColor);
    }

    /* Draw enemies as red dots */
    for (i = 0; i < numEnemies; i++) {
        if (enemies[i].active) {
            int ex = mapX + (int)(enemies[i].x * cellSize);
            int ey = mapY + (int)(enemies[i].y * cellSize);
            setPixel(ex, ey, COLOR_LRED);
            setPixel(ex+1, ey, COLOR_LRED);
            setPixel(ex, ey+1, COLOR_LRED);
            setPixel(ex+1, ey+1, COLOR_LRED);
        }
    }

    /* Draw player as green triangle */
    px = mapX + (int)(player.x * cellSize);
    py = mapY + (int)(player.y * cellSize);
    setPixel(px, py, COLOR_LGREEN);
    setPixel(px+1, py, COLOR_LGREEN);
    setPixel(px-1, py, COLOR_LGREEN);
    setPixel(px, py-1, COLOR_LGREEN);
    setPixel(px, py+1, COLOR_LGREEN);

    /* Draw view direction line */
    {
        int dx = (int)(player.dirX * 4);
        int dy = (int)(player.dirY * 4);
        setPixel(px + dx, py + dy, COLOR_YELLOW);
        setPixel(px + dx/2, py + dy/2, COLOR_YELLOW);
    }
}

/* Draw weapon sprite at bottom center */
void drawWeapon(void) {
    int startX = SCREEN_WIDTH / 2 - WEAPON_WIDTH / 2;
    int startY = SCREEN_HEIGHT - WEAPON_HEIGHT - 32;  /* Above status bar */
    int x, y;
    unsigned char pixel;

    for (y = 0; y < WEAPON_HEIGHT; y++) {
        for (x = 0; x < WEAPON_WIDTH; x++) {
            pixel = weaponSprite[y * WEAPON_WIDTH + x];
            if (pixel != 0) {  /* 0 is transparent */
                setPixel(startX + x, startY + y, pixel);
            }
        }
    }
}

void drawHUD(void) {
    char buffer[32];
    int i, j;
    int barY = SCREEN_HEIGHT - 32;  /* Status bar starts here */
    unsigned char color;

    /* Draw weapon first */
    drawWeapon();

    /*=====================================================================
     * DOOM-STYLE STATUS BAR AT BOTTOM (widest at bottom)
     *====================================================================*/

    /* Draw status bar background - gradient from dark to darker */
    for (j = 0; j < 32; j++) {
        unsigned char bgColor = (j < 8) ? 8 : ((j < 16) ? 7 : 6);
        for (i = 0; i < SCREEN_WIDTH; i++) {
            setPixel(i, barY + j, bgColor);
        }
    }

    /* Draw top border (bright line) */
    for (i = 0; i < SCREEN_WIDTH; i++) {
        setPixel(i, barY, COLOR_WHITE);
        setPixel(i, barY + 1, COLOR_GRAY);
    }

    /* Left section: HEALTH */
    drawText(8, barY + 6, "HEALTH", COLOR_RED);
    sprintf(buffer, "%3d%%", player.health);
    drawText(8, barY + 16, buffer, COLOR_LRED);

    /* Health bar below text */
    for (i = 0; i < 50; i++) {
        color = (i < player.health / 2) ?
            (player.health > 50 ? COLOR_LGREEN : (player.health > 25 ? COLOR_YELLOW : COLOR_LRED))
            : COLOR_DGRAY;
        for (j = 0; j < 4; j++) {
            setPixel(56 + i, barY + 18 + j, color);
        }
    }

    /* Center section: SCORE with border box */
    {
        int centerX = SCREEN_WIDTH / 2 - 32;
        /* Draw inset box */
        for (j = 4; j < 28; j++) {
            for (i = 0; i < 64; i++) {
                setPixel(centerX + i, barY + j, COLOR_DGRAY);
            }
        }
        drawText(centerX + 8, barY + 8, "SCORE", COLOR_YELLOW);
        sprintf(buffer, "%6d", player.score);
        drawText(centerX + 8, barY + 18, buffer, COLOR_BWHITE);
    }

    /* Right section: AMMO */
    {
        int rightX = SCREEN_WIDTH - 80;
        drawText(rightX, barY + 6, "AMMO", COLOR_CYAN);
        sprintf(buffer, "%3d", player.ammo);
        drawText(rightX, barY + 16, buffer, COLOR_LCYAN);

        /* Ammo pips */
        for (i = 0; i < (player.ammo > 20 ? 20 : player.ammo); i++) {
            setPixel(rightX + 32 + (i % 10) * 3, barY + 16 + (i / 10) * 4, COLOR_YELLOW);
            setPixel(rightX + 33 + (i % 10) * 3, barY + 16 + (i / 10) * 4, COLOR_YELLOW);
        }
    }

    /* Draw mini map in top-right corner */
    drawMiniMap(SCREEN_WIDTH - 52, 4, 48);

    /* Hit flash: red frame around the view for a moment after taking damage */
    if ((long)(hurtUntil - gameMs()) > 0) {
        int t;
        for (t = 0; t < 3; t++) {
            for (i = 0; i < SCREEN_WIDTH; i++) { setPixel(i, t, COLOR_LRED); setPixel(i, barY - 1 - t, COLOR_LRED); }
            for (j = 0; j < barY; j++) { setPixel(t, j, COLOR_LRED); setPixel(SCREEN_WIDTH - 1 - t, j, COLOR_LRED); }
        }
    }

    /* FPS overlay (F1) */
    if (showFps) {
        sprintf(buffer, "FPS %3d", (int)(fpsEstimate + 0.5));
        drawText(4, 4, buffer, COLOR_LGREEN);
        sprintf(buffer, "X%4.1f Y%4.1f", player.x, player.y);
        drawText(4, 14, buffer, COLOR_GRAY);
    }

    /* Crosshair in center of view */
    {
        int cx = SCREEN_WIDTH / 2;
        int cy = SCREEN_CENTER + playerPitch;
        /* Cross shape */
        setPixel(cx - 4, cy, COLOR_WHITE);
        setPixel(cx - 3, cy, COLOR_WHITE);
        setPixel(cx + 3, cy, COLOR_WHITE);
        setPixel(cx + 4, cy, COLOR_WHITE);
        setPixel(cx, cy - 4, COLOR_WHITE);
        setPixel(cx, cy - 3, COLOR_WHITE);
        setPixel(cx, cy + 3, COLOR_WHITE);
        setPixel(cx, cy + 4, COLOR_WHITE);
        /* Center dot */
        setPixel(cx, cy, COLOR_LRED);
    }
}

/*============================================================================
 * INPUT HANDLING
 *===========================================================================*/

void handleInput(void) {
    static unsigned long lastShot = 0;
    unsigned long now = gameMs();
    int mx, my;
    double move = 0.0, strafe = 0.0, turn = 0.0;

    if (kb_pressed(KEY_ESC)) {
        gameRunning = 0;
        return;
    }
    if (kb_pressed(KEY_F1) || kb_pressed(KEY_F)) showFps = !showFps;

    /* Mouse look */
    getMouseDelta(&mx, &my);
    if (mx != 0) rotatePlayer(mx * MOUSE_SENSITIVITY);
    if (my != 0) {
        playerPitch -= my;
        if (playerPitch > 50) playerPitch = 50;
        if (playerPitch < -50) playerPitch = -50;
    }

    /* Movement: W/S or Up/Down move, A/D strafe, Left/Right turn.
     * Keys are read from the INT 9 key-state table so several can be held at
     * once, and everything scales with frame time. */
    if (kb_down(KEY_W) || kb_down(KEY_UP))   move += 1.0;
    if (kb_down(KEY_S) || kb_down(KEY_DOWN)) move -= 1.0;
    if (kb_down(KEY_A)) strafe -= 1.0;
    if (kb_down(KEY_D)) strafe += 1.0;
    if (kb_down(KEY_LEFT))  turn -= 1.0;
    if (kb_down(KEY_RIGHT)) turn += 1.0;
    if (move != 0.0)   movePlayer(move * frameDt);
    if (strafe != 0.0) strafePlayer(strafe * frameDt);
    if (turn != 0.0)   rotatePlayer(turn * ROTATE_SPEED * frameDt);

    /* Shooting - SPACE, CTRL or mouse button; 8 shots per second max */
    if ((kb_pressed(KEY_SPACE) || kb_down(KEY_SPACE) || kb_down(KEY_LCTRL) || getMouseButton()) && now - lastShot > 125) {
        playerShoot();
        lastShot = now;
    }
}

/*============================================================================
 * SPLASH SCREEN - VGA MODE ASCII ART
 *===========================================================================*/

/* Star positions - pre-computed for animation */
static int starX[150];
static int starY[150];
static int starSpeed[150];  /* 1=slow, 2=medium, 3=fast for parallax */
static int starsInitialized = 0;

/* Initialize star positions once */
void initStars(void) {
    int i;
    srand(12345);
    for (i = 0; i < 150; i++) {
        starX[i] = rand() % SCREEN_WIDTH;
        starY[i] = rand() % SCREEN_HEIGHT;
        starSpeed[i] = 1 + (rand() % 3);  /* Speed 1-3 */
    }
    starsInitialized = 1;
}

/* Draw animated starfield background */
void drawStarfield(int frame) {
    int i, sx, sy;
    unsigned char color;

    if (!starsInitialized) {
        initStars();
    }

    for (i = 0; i < 150; i++) {
        /* Move stars horizontally based on speed (parallax) */
        sx = (starX[i] - frame * starSpeed[i]) % SCREEN_WIDTH;
        if (sx < 0) sx += SCREEN_WIDTH;
        sy = starY[i];

        /* Brighter stars move faster */
        if (starSpeed[i] == 3) {
            color = COLOR_BWHITE;  /* Bright white - fast */
        } else if (starSpeed[i] == 2) {
            color = COLOR_WHITE;   /* White - medium */
        } else {
            color = COLOR_GRAY;    /* Dim gray - slow */
        }
        setPixel(sx, sy, color);
    }
}

/* Cascade letter animation for title */
void drawCascadeTitle(const char *text, int startX, int startY, unsigned char color, int frame) {
    int i, len;
    len = strlen(text);
    for (i = 0; i < len && i <= frame; i++) {
        int dropY = startY;
        int elapsed = frame - i;
        if (elapsed < 10) {
            dropY = -20 + (startY + 20) * elapsed / 10;  /* Drop from top */
        }
        drawChar(startX + i * 8, dropY, text[i], color);
    }
}

void drawSplashScreen(void) {
    int x, frame;
    int centerX = SCREEN_WIDTH / 2;
    const char *title = "MAZE RUNNER 2";
    int titleLen = 13;
    int titleX = centerX - (titleLen * 8) / 2;
    clock_t startTime = clock();

    /* Cascade animation for title (a key skips it) */
    for (frame = 0; frame < 30 && !kbhit(); frame++) {
        clearScreen(COLOR_BLACK);

        /* Draw starfield background */
        drawStarfield(frame);

        /* Draw border */
        for (x = 0; x < SCREEN_WIDTH; x++) {
            setPixel(x, 0, COLOR_RED);
            setPixel(x, SCREEN_HEIGHT-1, COLOR_RED);
        }

        /* Cascade title letters */
        drawCascadeTitle(title, titleX, 30, COLOR_YELLOW, frame);

        /* Subtitle appears after title */
        if (frame > 15) {
            drawText(centerX - 64, 50, "THE NEXT LEVEL", COLOR_LRED);
        }

        displayFrame();

        /* Timing */
        while ((clock() - startTime) * 1000 / CLOCKS_PER_SEC < frame * 80);
    }

    /* Final splash with all content - animate until key press.
     * Keys mashed during loading / the cascade are dropped first so they can't skip the title. */
    while (kbhit()) getch();
    while (!kbhit()) {
        clearScreen(COLOR_BLACK);
        drawStarfield(frame++);

        /* Border */
        for (x = 0; x < SCREEN_WIDTH; x++) {
            setPixel(x, 0, COLOR_RED);
            setPixel(x, SCREEN_HEIGHT-1, COLOR_RED);
        }

        /* Title */
        drawText(titleX, 30, title, COLOR_YELLOW);
        drawText(centerX - 64, 50, "THE NEXT LEVEL", COLOR_LRED);

        /* Features list */
        drawText(60, 75, "STONE DUNGEONS", COLOR_LGREEN);
        drawText(60, 90, "ENEMIES SHOOT BACK!", COLOR_LRED);
        drawText(60, 105, "DYNAMIC LIGHTING", COLOR_YELLOW);
        drawText(60, 120, "WASD + MOUSE", COLOR_LCYAN);

        /* Controls */
        drawText(centerX - 72, 145, "WASD - MOVE", COLOR_WHITE);
        drawText(centerX - 72, 157, "MOUSE - AIM", COLOR_WHITE);
        drawText(centerX - 72, 169, "SPACE - FIRE", COLOR_WHITE);

        /* Press key */
        drawText(centerX - 64, 188, "PRESS ANY KEY", COLOR_BWHITE);

        displayFrame();

        /* Small delay for animation */
        while ((clock() - startTime) * 1000 / CLOCKS_PER_SEC < frame * 50);
    }
    getch();  /* Consume the key */
}

void drawCreditsScreen(void) {
    int x;
    int centerX = SCREEN_WIDTH / 2;

    clearScreen(COLOR_BLACK);
    drawStarfield(0);

    /* Border */
    for (x = 0; x < SCREEN_WIDTH; x++) {
        setPixel(x, 0, COLOR_YELLOW);
        setPixel(x, SCREEN_HEIGHT-1, COLOR_YELLOW);
    }

    /* Title */
    drawText(centerX - 64, 20, "VONHOLTENCODES", COLOR_YELLOW);
    drawText(centerX - 40, 35, "PRESENTS", COLOR_WHITE);

    /* Game title */
    drawText(centerX - 56, 60, "MAZE RUNNER 2", COLOR_LRED);

    /* Credits */
    drawText(centerX - 80, 90, "PROGRAMMING:", COLOR_CYAN);
    drawText(centerX - 80, 102, "TRENT VON HOLTEN", COLOR_WHITE);

    drawText(centerX - 80, 122, "ENGINE:", COLOR_CYAN);
    drawText(centerX - 80, 134, "DDA RAYCASTING", COLOR_WHITE);

    drawText(centerX - 80, 154, "V" MAZE2_VERSION " - 2025", COLOR_GRAY);

    /* Loading text */
    drawText(centerX - 48, 180, "LOADING...", COLOR_LGREEN);

    displayFrame();
}

/* Credit lines - static for C89 compatibility */
static const char *credits[] = {
    "",
    "MAZE RUNNER 2",
    "",
    "THE NEXT LEVEL",
    "",
    "",
    "CREATED BY",
    "TRENT VON HOLTEN",
    "",
    "",
    "PROGRAMMING",
    "VONHOLTENCODES",
    "",
    "",
    "ENGINE",
    "DDA RAYCASTING",
    "64X64 TEXTURES",
    "",
    "",
    "FEATURES",
    "ENEMIES SHOOT BACK",
    "PROJECTILE SYSTEM",
    "WASD + MOUSE",
    "",
    "",
    "SPECIAL THANKS",
    "ID SOFTWARE",
    "DJGPP TEAM",
    "",
    "",
    "VERSION " MAZE2_VERSION " - 2025",
    "",
    "",
    "THANKS FOR PLAYING!",
    "",
    "",
    "",
    "PRESS ANY KEY",
    "",
    ""
};

void drawScrollingCredits(void) {
    int scrollY, x, y, frame;
    int numLines = 40;
    int lineHeight = 12;
    int sx, sy, len, textX, textY;
    unsigned char color;
    clock_t startTime = clock();

    for (frame = 0; frame < 400; frame++) {
        /* Check for keypress to skip */
        if (kbhit()) {
            getch();
            break;
        }

        clearScreen(COLOR_BLACK);

        /* Draw starfield background */
        srand(12345);
        for (y = 0; y < 50; y++) {
            sx = rand() % SCREEN_WIDTH;
            sy = (rand() % SCREEN_HEIGHT + frame) % SCREEN_HEIGHT;
            setPixel(sx, sy, COLOR_WHITE);
        }

        /* Draw scrolling text */
        scrollY = SCREEN_HEIGHT - frame * 2;

        for (y = 0; y < numLines; y++) {
            textY = scrollY + y * lineHeight;
            if (textY >= -10 && textY < SCREEN_HEIGHT) {
                len = strlen(credits[y]);
                textX = (SCREEN_WIDTH - len * 8) / 2;
                color = COLOR_WHITE;

                /* Color based on content */
                if (strstr(credits[y], "MAZE RUNNER") != NULL) color = COLOR_YELLOW;
                else if (strstr(credits[y], "TRENT") != NULL) color = COLOR_LCYAN;
                else if (strstr(credits[y], "VONHOLTEN") != NULL) color = COLOR_LGREEN;
                else if (strstr(credits[y], "THANKS") != NULL) color = COLOR_LRED;
                else if (strstr(credits[y], "2025") != NULL) color = COLOR_GRAY;

                drawText(textX, textY, credits[y], color);
            }
        }

        /* Draw border */
        for (x = 0; x < SCREEN_WIDTH; x++) {
            setPixel(x, 0, COLOR_RED);
            setPixel(x, SCREEN_HEIGHT-1, COLOR_RED);
        }

        displayFrame();

        /* Slow down scroll */
        while ((clock() - startTime) * 1000 / CLOCKS_PER_SEC < frame * 50);
    }
}

/*============================================================================
 * MAIN
 *===========================================================================*/

/* Data files (FM.DAT, 1.MID) live beside MAZE2.EXE, wherever it was launched from */
static char dataDir[260] = "";
static char fmPath[300], midPath[300];

static void initDataDir(const char *argv0) {
    const char *p = argv0 ? strrchr(argv0, '/') : NULL;
    const char *q = argv0 ? strrchr(argv0, '\\') : NULL;
    size_t n;
    if (q > p) p = q;
    if (!p) { dataDir[0] = 0; return; }
    n = (size_t)(p - argv0) + 1;
    if (n >= sizeof dataDir) n = sizeof dataDir - 1;
    memcpy(dataDir, argv0, n);
    dataDir[n] = 0;
}

/* Runs on every exit path (atexit) so a crash-out never leaves the keyboard
 * or timer hooked, the OPL playing, or the screen in mode 13h. */
static int debugSpawn = 0;
static double debugX, debugY, debugDeg;
static int musicEnabled = 1;

/* Stage log written beside the EXE (MAZE2.LOG), flushed after every line, so a
 * crash on real hardware leaves a record of how far start-up got. */
static FILE *logFile = NULL;
static void logStage(const char *fmt, ...) {
    va_list ap;
    if (logFile) {
        va_start(ap, fmt); vfprintf(logFile, fmt, ap); va_end(ap);
        fputc('\n', logFile);
        fflush(logFile);
    }
    va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    printf("\n");
}

static int cleanedUp = 0;
static void cleanup(void) {
    if (cleanedUp) return;
    cleanedUp = 1;
    logStage("[ EXIT   ] cleanup");
    kb_remove();
    StopMIDI();
    UnloadMIDI();
    sound_shutdown();
    setVideoMode(0x03);
    freeDoubleBuffer();
    if (logFile) { fclose(logFile); logFile = NULL; }
}

int main(int argc, char **argv) {
    int musicOn = 0;
    char buffer[64];
    unsigned long frames = 0, startTicks = 0, playTicks;

    initDataDir(argc > 0 ? argv[0] : NULL);
    {
        int i;
        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-nosound") == 0) soundEnabled = musicEnabled = 0;   /* skip SB probe + music */
            else if (strcmp(argv[i], "-nomusic") == 0) musicEnabled = 0;            /* skip OPL probe + MIDI */
            else if (strcmp(argv[i], "-at") == 0 && i + 3 < argc) {         /* -at X Y DEGREES: debug spawn */
                debugSpawn = 1;
                debugX = atof(argv[i + 1]); debugY = atof(argv[i + 2]); debugDeg = atof(argv[i + 3]);
                i += 3;
            }
        }
    }
    {
        char logPath[300];
        sprintf(logPath, "%sMAZE2.LOG", dataDir);
        logFile = fopen(logPath, "w");
    }
    logStage("MAZE RUNNER 2 v%s - VonHoltenCodes", MAZE2_VERSION);
    logStage("[ START  ] dir '%s' sound=%d music=%d", dataDir, soundEnabled, musicEnabled);

    /* Initialize systems */
    initDoubleBuffer();
    initFont();
    initTextures();
    initEnemySprites();
    logStage("[ INIT   ] buffers, font, textures");
    initPlayer();
    if (debugSpawn && worldMap[(int)debugY][(int)debugX] == 0) {
        player.x = debugX;
        player.y = debugY;
        rotatePlayer(debugDeg * 3.14159265 / 180.0);
    }
    initProjectiles();
    initEnemies();
    initTorches();
    buildLightMap();
    logStage("[ INIT   ] level, enemies, light map");
    initSound();
    logStage("[ AUDIO  ] %s", soundEnabled ? (sound_blaster_present() ? "Sound Blaster" : "PC speaker") : "off (-nosound)");
    initMouse();
    logStage("[ INPUT  ] %s", mouseAvailable ? "mouse driver found" : "no mouse driver - keyboard only (arrows turn)");
    atexit(cleanup);

    /* Start MIDI music: InitMIDI saves the timer vector, SetFM probes the OPL and loads FM.DAT */
    InitMIDI();
    sprintf(fmPath, "%sFM.DAT", dataDir);
    sprintf(midPath, "%s1.MID", dataDir);
    FMDataFile = fmPath;
    if (musicEnabled) {
        logStage("[ MUSIC  ] probing OPL synth");
        if (SetFM()) {
            logStage("[ MUSIC  ] OPL found, loading %s", midPath);
            if (LoadMIDI(midPath)) {
                SetVol(200);
                PlayMIDI();
                musicOn = 1;
            }
        }
    }
    logStage("[ MUSIC  ] %s", musicOn ? "MIDI playing (timer hooked)" : musicEnabled ? "no OPL synth or 1.MID - music off" : "off (-nomusic)");

    /* Enter VGA mode */
    logStage("[ VIDEO  ] entering mode 13h");
    setVideoMode(0x13);

    /* Show credits splash, then the title (waits for a key via the BIOS) */
    drawCreditsScreen();
    delay(1500);
    drawSplashScreen();
    getch();
    logStage("[ GAME   ] title dismissed, hooking keyboard");

    /* Take over the keyboard for the game loop */
    if (!kb_install()) {
        setVideoMode(0x03);
        printf("ERROR: could not install the keyboard handler\n");
        return 1;
    }

    /* Discard mouse motion that piled up during the title so the view doesn't jerk */
    {
        int mx, my;
        getMouseDelta(&mx, &my);
    }

    logStage("[ GAME   ] running");

    /* Main game loop */
    startTicks = biosTicks();
    while (gameRunning && player.health > 0) {
        frames++;
        updateFrameTiming();
        handleInput();
        updateProjectiles();
        updateEnemyAI();
        updateSound();

        /* Check for exit - SE corner of map (around position 22,22) */
        if (player.x > 21.5 && player.y > 21.5) {
            player.score += 1000;  /* Bonus for escaping */
            soundPickup();

            clearScreen(14);  /* Yellow flash */
            drawText(100, 90, "EXIT REACHED!", 0);
            displayFrame();
            delay(1500);
            exitReason = "escaped";
            break;
        }

        renderFrame();
        renderSprites();
        drawHUD();
        displayFrame();
    }

    playTicks = biosTicks() - startTicks;
    if (player.health <= 0) {
        exitReason = "killed";
        clearScreen(4);   /* red */
        drawText(120, 80, "YOU DIED", 15);
        drawText(76, 100, "THE DUNGEON WINS", 14);
        sprintf(buffer, "SCORE %d", player.score);
        drawText(120, 120, buffer, 7);
        displayFrame();
        delay(2500);
    }
    logStage("[ GAME   ] over: %s, score %d, %lu frames in %.1f s", exitReason, player.score, frames, playTicks / 18.2065);

    /* Hand the keyboard back to the BIOS before the "press any key" credits */
    kb_remove();
    drawScrollingCredits();

    /* Return to text mode */
    cleanup();

    printf("\n");
    printf("========================================\n");
    if (player.health <= 0) {
        printf("          GAME OVER - YOU DIED\n");
    } else if (strcmp(exitReason, "escaped") == 0) {
        printf("       ESCAPE SUCCESSFUL!\n");
    } else {
        printf("          QUIT\n");
    }
    printf("========================================\n");
    printf("  Final Score: %d\n", player.score);
    printf("  %lu frames in %.1f s = %.1f fps average\n", frames, playTicks / 18.2065,
           playTicks ? frames * 18.2065 / playTicks : 0.0);
    printf("========================================\n");
    printf("\n");
    printf("  MAZE RUNNER 2 v%s\n", MAZE2_VERSION);
    printf("  By VonHoltenCodes 2025\n");
    printf("  Thanks for playing!\n");
    printf("\n");
    return 0;
}
