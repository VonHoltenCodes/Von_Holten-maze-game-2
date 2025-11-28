/*
 * MAZE RUNNER 2 - P4-ERA DOS RAYCASTER
 * =====================================
 *
 * Enhanced raycasting engine targeting Pentium 4 era hardware (1.5GHz+)
 * Based on Von_Holten-Maze-Game v3.0 foundation
 *
 * NEW FEATURES:
 * - 64x64 high-resolution wall textures
 * - Enemies that SHOOT BACK with projectile system
 * - Smooth shading and lighting effects
 * - Procedural brick/stone textures
 * - Multiple weapon types
 * - Advanced enemy AI with ranged attacks
 *
 * TARGET: Pentium 4, 256MB RAM, VGA Mode 13h (320x200x256)
 *
 * By: VonHoltenCodes (2025)
 * License: Open Source
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <conio.h>

#ifdef __DJGPP__
#include <dpmi.h>
#include <go32.h>
#include <sys/nearptr.h>
#include <dos.h>
#include <bios.h>
#include <pc.h>
#endif

/*============================================================================
 * CONFIGURATION - P4 ERA SETTINGS
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
#define MOVE_SPEED 0.12
#define ROTATE_SPEED 0.08
#define MOUSE_SENSITIVITY 0.003

/* Combat settings */
#define MAX_PROJECTILES 32
#define PROJECTILE_SPEED 0.3
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

/* Wall types */
#define WALL_NONE 0
#define WALL_BRICK 1
#define WALL_STONE 2
#define WALL_METAL 3
#define WALL_TECH 4

/*============================================================================
 * DATA STRUCTURES
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
    int type;              /* Enemy type */
    clock_t lastFireTime;  /* When enemy last shot */
    int canShoot;          /* Does this enemy type shoot? */
} Enemy;

/* Enemy types */
#define ENEMY_GRUNT 0      /* Basic, doesn't shoot */
#define ENEMY_SOLDIER 1    /* Shoots back */
#define ENEMY_ELITE 2      /* Shoots fast, more health */
#define ENEMY_BOSS 3       /* Lots of health, rapid fire */

#define MAX_ENEMIES 20

/*============================================================================
 * GLOBAL STATE
 *===========================================================================*/

/* VGA memory pointer */
#ifdef __DJGPP__
static unsigned char *VGA_MEMORY = (unsigned char *)0xA0000;
#else
static unsigned char *VGA_MEMORY;
#endif

/* Double buffer */
static unsigned char *backBuffer = NULL;

/* Z-buffer for depth testing */
static double zBuffer[SCREEN_WIDTH];

/* Input state */
static unsigned char keyDown[128] = {0};
static int mouseAvailable = 0;
static int playerPitch = 0;

/* Game state */
static Player player;
static Projectile projectiles[MAX_PROJECTILES];
static Enemy enemies[MAX_ENEMIES];
static int numEnemies = 0;
static clock_t gameStartTime;
static int gameRunning = 1;

/* P4 ERA: 64x64 procedural textures */
static unsigned char wallTextures[4][TEX_SIZE * TEX_SIZE];

/* Map layout */
static int worldMap[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,2,2,2,2,0,3,3,3,3,0,4,4,4,4,0,2,2,2,2,2,0,1},
    {1,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,1},
    {1,0,2,0,1,1,1,0,1,1,1,1,1,1,0,1,1,1,0,1,0,2,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,1},
    {1,0,3,0,1,0,2,2,2,0,3,3,3,0,4,0,0,1,0,1,0,3,0,1},
    {1,0,3,0,0,0,2,0,2,0,3,0,3,0,4,0,0,0,0,1,0,3,0,1},
    {1,0,3,0,1,0,2,0,2,0,3,0,3,0,4,4,4,1,0,1,0,3,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1},
    {1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,1,0,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,4,4,4,0,2,0,3,3,3,3,3,3,3,0,2,0,4,4,4,4,0,1},
    {1,0,4,0,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,4,0,1},
    {1,0,4,0,1,1,2,1,1,0,1,1,1,0,1,1,2,1,1,0,0,4,0,1},
    {1,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,3,0,1,1,1,1,1,0,1,0,1,0,1,1,1,1,1,0,3,3,0,1},
    {1,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,1},
    {1,0,3,3,3,0,2,2,2,2,2,2,2,2,2,2,2,0,3,3,3,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,1,1,0,1,1,1,1,1,1,0,1,1,1,1,1,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,2,2,2,2,2,2,2,2,2,0,0,2,2,2,2,2,2,2,2,2,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

/*============================================================================
 * P4 ERA: PROCEDURAL TEXTURE GENERATION
 * Generate high-quality 64x64 textures at runtime
 *===========================================================================*/

/* Generate brick wall texture */
void generateBrickTexture(unsigned char *tex) {
    int x, y;
    int brickW = 16, brickH = 8;

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            int bx = x % brickW;
            int by = y % brickH;
            int row = y / brickH;

            /* Offset every other row */
            if (row % 2 == 1) {
                bx = (x + brickW / 2) % brickW;
            }

            /* Mortar lines */
            if (bx == 0 || by == 0) {
                tex[y * TEX_SIZE + x] = COLOR_GRAY;
            } else {
                /* Brick color with variation */
                int shade = 4 + (rand() % 3);  /* Red shades */
                /* Add some noise for texture */
                if (rand() % 8 == 0) shade = COLOR_BROWN;
                tex[y * TEX_SIZE + x] = shade;
            }
        }
    }
}

/* Generate stone wall texture */
void generateStoneTexture(unsigned char *tex) {
    int x, y;

    /* Fill with base gray */
    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            int base = 7 + (rand() % 2);  /* White/gray base */

            /* Add darker stone outlines */
            int blockX = x / 12;
            int blockY = y / 10;
            int localX = x % 12;
            int localY = y % 10;

            if (localX == 0 || localY == 0) {
                base = COLOR_GRAY;  /* Mortar */
            }

            /* Random dark spots for texture */
            if (rand() % 12 == 0) base = COLOR_GRAY;

            tex[y * TEX_SIZE + x] = base;
        }
    }
}

/* Generate metal panel texture */
void generateMetalTexture(unsigned char *tex) {
    int x, y;

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            /* Base gray metal */
            int base = 8;  /* Dark gray */

            /* Vertical ridges */
            if (x % 8 == 0) base = 7;  /* Lighter ridge */
            if (x % 8 == 1) base = 0;  /* Shadow */

            /* Horizontal seams */
            if (y % 32 == 0 || y % 32 == 1) base = 0;

            /* Rivets */
            int rivetX = x % 16;
            int rivetY = y % 16;
            if (rivetX >= 6 && rivetX <= 9 && rivetY >= 6 && rivetY <= 9) {
                if (rivetX == 7 || rivetX == 8) {
                    if (rivetY == 7 || rivetY == 8) {
                        base = 15;  /* Bright rivet center */
                    } else {
                        base = 7;  /* Rivet edge */
                    }
                }
            }

            tex[y * TEX_SIZE + x] = base;
        }
    }
}

/* Generate tech/circuit texture */
void generateTechTexture(unsigned char *tex) {
    int x, y;

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            /* Dark blue base */
            int base = COLOR_BLUE;

            /* Circuit traces */
            if (x % 16 == 8 || y % 16 == 8) {
                base = COLOR_CYAN;  /* Trace lines */
            }

            /* Connection nodes */
            int nodeX = x % 16;
            int nodeY = y % 16;
            if (nodeX >= 6 && nodeX <= 10 && nodeY >= 6 && nodeY <= 10) {
                if (x % 16 == 8 && y % 16 == 8) {
                    base = COLOR_LCYAN;  /* Bright node */
                }
            }

            /* Random LED-like dots */
            if (rand() % 64 == 0) base = COLOR_LGREEN;

            tex[y * TEX_SIZE + x] = base;
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
#ifdef __DJGPP__
    dosmemput(backBuffer, BUFFER_SIZE, 0xA0000);
#else
    memcpy(VGA_MEMORY, backBuffer, BUFFER_SIZE);
#endif
}

void setVideoMode(int mode) {
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = mode;
    int86(0x10, &regs, &regs);
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

void readKeyboardState(unsigned char *keys) {
    memset(keys, 0, 128);

    if (kbhit()) {
        int ch = getch();
        if (ch == 27) keys[1] = 1;  /* ESC */
    }

#ifdef __DJGPP__
    __dpmi_regs regs;
    regs.h.ah = 0x12;
    __dpmi_int(0x16, &regs);

    while (_bios_keybrd(0x11) != 0) {
        int keycode = _bios_keybrd(0x11);
        int scancode = (keycode >> 8) & 0xFF;

        if (scancode == 0x48) keys[72] = 1;  /* Up */
        if (scancode == 0x50) keys[80] = 1;  /* Down */
        if (scancode == 0x4B) keys[75] = 1;  /* Left */
        if (scancode == 0x4D) keys[77] = 1;  /* Right */
        if (scancode == 0x39) keys[57] = 1;  /* SPACE */
        if (scancode == 0x01) keys[1] = 1;   /* ESC */
        if (scancode == 0x1E) keys[30] = 1;  /* A */
        if (scancode == 0x20) keys[32] = 1;  /* D */
        if (scancode == 0x11) keys[17] = 1;  /* W */
        if (scancode == 0x1F) keys[31] = 1;  /* S */

        _bios_keybrd(0x10);
    }
#endif
}

void initMouse(void) {
    union REGS r;
    r.x.ax = 0;
    int86(0x33, &r, &r);

    if (r.x.ax == 0xFFFF) {
        mouseAvailable = 1;
        r.x.ax = 2;  /* Hide cursor */
        int86(0x33, &r, &r);
    }
}

void getMouseDelta(int *dx, int *dy) {
    union REGS r;

    if (!mouseAvailable) {
        *dx = 0;
        *dy = 0;
        return;
    }

    r.x.ax = 11;
    int86(0x33, &r, &r);

    *dx = (short)r.x.cx;
    *dy = (short)r.x.dx;
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
        double newX = projectiles[i].x + projectiles[i].dirX * PROJECTILE_SPEED;
        double newY = projectiles[i].y + projectiles[i].dirY * PROJECTILE_SPEED;

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
                    projectiles[i].active = 0;

                    if (enemies[j].health <= 0) {
                        enemies[j].active = 0;
                        player.score += (enemies[j].type + 1) * 100;
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
    enemies[numEnemies].health = 50;
    enemies[numEnemies].type = ENEMY_GRUNT;
    enemies[numEnemies].canShoot = 0;
    enemies[numEnemies].lastFireTime = 0;
    numEnemies++;

    /* Soldiers - they shoot! */
    enemies[numEnemies].x = enemies[numEnemies].spawnX = 18.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 5.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = 75;
    enemies[numEnemies].type = ENEMY_SOLDIER;
    enemies[numEnemies].canShoot = 1;
    enemies[numEnemies].lastFireTime = 0;
    numEnemies++;

    enemies[numEnemies].x = enemies[numEnemies].spawnX = 10.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 12.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = 75;
    enemies[numEnemies].type = ENEMY_SOLDIER;
    enemies[numEnemies].canShoot = 1;
    enemies[numEnemies].lastFireTime = 0;
    numEnemies++;

    /* Elite - shoots faster */
    enemies[numEnemies].x = enemies[numEnemies].spawnX = 18.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 18.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = 150;
    enemies[numEnemies].type = ENEMY_ELITE;
    enemies[numEnemies].canShoot = 1;
    enemies[numEnemies].lastFireTime = 0;
    numEnemies++;

    /* More grunts */
    enemies[numEnemies].x = enemies[numEnemies].spawnX = 3.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 15.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = 50;
    enemies[numEnemies].type = ENEMY_GRUNT;
    enemies[numEnemies].canShoot = 0;
    enemies[numEnemies].lastFireTime = 0;
    numEnemies++;

    enemies[numEnemies].x = enemies[numEnemies].spawnX = 20.5;
    enemies[numEnemies].y = enemies[numEnemies].spawnY = 11.5;
    enemies[numEnemies].active = 1;
    enemies[numEnemies].health = 50;
    enemies[numEnemies].type = ENEMY_GRUNT;
    enemies[numEnemies].canShoot = 0;
    enemies[numEnemies].lastFireTime = 0;
    numEnemies++;
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
    clock_t now = clock();
    int i;

    for (i = 0; i < numEnemies; i++) {
        if (!enemies[i].active) continue;

        /* Calculate direction to player */
        double dx = player.x - enemies[i].x;
        double dy = player.y - enemies[i].y;
        double dist = sqrt(dx*dx + dy*dy);

        /* Move toward player (slowly) */
        if (dist > 1.5) {
            double moveX = (dx / dist) * 0.015;
            double moveY = (dy / dist) * 0.015;

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

            long elapsed = (now - enemies[i].lastFireTime) * 1000 / CLOCKS_PER_SEC;

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
    player.x = 2.0;
    player.y = 2.0;
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

    if (worldMap[(int)player.y][(int)newX] == 0) {
        player.x = newX;
    }
    if (worldMap[(int)newY][(int)player.x] == 0) {
        player.y = newY;
    }
}

void strafePlayer(double strafeDir) {
    double newX = player.x + player.planeX * strafeDir * MOVE_SPEED;
    double newY = player.y + player.planeY * strafeDir * MOVE_SPEED;

    if (worldMap[(int)player.y][(int)newX] == 0) {
        player.x = newX;
    }
    if (worldMap[(int)newY][(int)player.x] == 0) {
        player.y = newY;
    }
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
    }
}

/*============================================================================
 * RENDERING - P4 ERA HIGH-RES TEXTURES
 *===========================================================================*/

void renderFrame(void) {
    int x, y;

    /* Draw ceiling (dark gray gradient) */
    for (y = 0; y < SCREEN_CENTER + playerPitch; y++) {
        int shade = y * 8 / SCREEN_CENTER;
        unsigned char color = (shade < 8) ? shade : 8;
        for (x = 0; x < SCREEN_WIDTH; x++) {
            backBuffer[y * SCREEN_WIDTH + x] = color;
        }
    }

    /* Draw floor (brown gradient) */
    for (y = SCREEN_CENTER + playerPitch; y < SCREEN_HEIGHT; y++) {
        int shade = (SCREEN_HEIGHT - y) * 6 / SCREEN_CENTER;
        unsigned char color = 6;  /* Brown base */
        if (shade > 0) color = COLOR_BROWN;
        for (x = 0; x < SCREEN_WIDTH; x++) {
            backBuffer[y * SCREEN_WIDTH + x] = color;
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

        /* Draw textured vertical line */
        double step = (double)TEX_SIZE / lineHeight;
        double texPos = (drawStart - SCREEN_CENTER - playerPitch + lineHeight / 2) * step;

        for (y = drawStart; y < drawEnd; y++) {
            int texY = ((int)texPos) & TEX_MASK;
            texPos += step;

            unsigned char color = wallTextures[wallType][texY * TEX_SIZE + texX];

            /* Distance fog */
            if (perpWallDist > 12.0) {
                color = COLOR_BLACK;
            } else if (perpWallDist > 8.0) {
                /* Darken */
                if (color > 8) color -= 8;
                else color = 0;
            }

            /* Side shading */
            if (side == 1) {
                if (color > 0) color = (color > 8) ? color - 4 : color / 2;
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

    /* Render enemies */
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
        int spriteWidth = spriteHeight;

        int drawStartY = -spriteHeight / 2 + SCREEN_CENTER + playerPitch;
        int drawEndY = spriteHeight / 2 + SCREEN_CENTER + playerPitch;
        int drawStartX = -spriteWidth / 2 + spriteScreenX;
        int drawEndX = spriteWidth / 2 + spriteScreenX;

        if (drawStartY < 0) drawStartY = 0;
        if (drawEndY >= SCREEN_HEIGHT) drawEndY = SCREEN_HEIGHT - 1;
        if (drawStartX < 0) drawStartX = 0;
        if (drawEndX >= SCREEN_WIDTH) drawEndX = SCREEN_WIDTH - 1;

        /* Enemy color based on type */
        unsigned char enemyColor;
        switch (enemies[i].type) {
            case ENEMY_GRUNT: enemyColor = COLOR_GREEN; break;
            case ENEMY_SOLDIER: enemyColor = COLOR_RED; break;
            case ENEMY_ELITE: enemyColor = COLOR_MAGENTA; break;
            case ENEMY_BOSS: enemyColor = COLOR_YELLOW; break;
            default: enemyColor = COLOR_WHITE; break;
        }

        /* Draw simple sprite (will be replaced with bitmap later) */
        for (x = drawStartX; x < drawEndX; x++) {
            if (transformY < zBuffer[x]) {
                for (y = drawStartY; y < drawEndY; y++) {
                    /* Simple humanoid shape */
                    int localX = x - drawStartX;
                    int localY = y - drawStartY;
                    int centerX = spriteWidth / 2;
                    int centerY = spriteHeight / 2;

                    /* Head (top 1/4) */
                    if (localY < spriteHeight / 4) {
                        int headCenterX = centerX;
                        int headCenterY = spriteHeight / 8;
                        int headRadius = spriteHeight / 8;
                        int dx = localX - headCenterX;
                        int dy = localY - headCenterY;
                        if (dx*dx + dy*dy < headRadius*headRadius) {
                            backBuffer[y * SCREEN_WIDTH + x] = enemyColor;
                        }
                    }
                    /* Body (middle 1/2) */
                    else if (localY < spriteHeight * 3 / 4) {
                        if (localX > centerX - spriteWidth/6 && localX < centerX + spriteWidth/6) {
                            backBuffer[y * SCREEN_WIDTH + x] = enemyColor;
                        }
                    }
                    /* Legs (bottom 1/4) */
                    else {
                        if ((localX > centerX - spriteWidth/4 && localX < centerX - spriteWidth/12) ||
                            (localX > centerX + spriteWidth/12 && localX < centerX + spriteWidth/4)) {
                            backBuffer[y * SCREEN_WIDTH + x] = enemyColor;
                        }
                    }
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

void drawHUD(void) {
    char buffer[32];
    int i, j;
    unsigned char color;

    /* Health bar */
    drawText(5, 5, "HEALTH:", COLOR_WHITE);
    for (i = 0; i < 50; i++) {
        color = (i < player.health / 2) ?
            (player.health > 50 ? COLOR_LGREEN : (player.health > 25 ? COLOR_YELLOW : COLOR_LRED))
            : COLOR_GRAY;
        for (j = 0; j < 4; j++) {
            setPixel(60 + i, 5 + j, color);
        }
    }

    /* Ammo */
    sprintf(buffer, "AMMO: %d", player.ammo);
    drawText(5, 15, buffer, COLOR_YELLOW);

    /* Score */
    sprintf(buffer, "SCORE: %d", player.score);
    drawText(5, 25, buffer, COLOR_LCYAN);

    /* Crosshair */
    int cx = SCREEN_WIDTH / 2;
    int cy = SCREEN_CENTER + playerPitch;
    setPixel(cx - 5, cy, COLOR_WHITE);
    setPixel(cx + 5, cy, COLOR_WHITE);
    setPixel(cx, cy - 5, COLOR_WHITE);
    setPixel(cx, cy + 5, COLOR_WHITE);
    setPixel(cx, cy, COLOR_LRED);
}

/*============================================================================
 * INPUT HANDLING
 *===========================================================================*/

void handleInput(void) {
    static clock_t lastShot = 0;
    clock_t now = clock();

    readKeyboardState(keyDown);

    /* ESC to quit */
    if (keyDown[1]) {
        gameRunning = 0;
        return;
    }

    /* Mouse look */
    int mx, my;
    getMouseDelta(&mx, &my);
    if (mx != 0) {
        rotatePlayer(mx * MOUSE_SENSITIVITY);
    }
    if (my != 0) {
        playerPitch -= my;
        if (playerPitch > 80) playerPitch = 80;
        if (playerPitch < -80) playerPitch = -80;
    }

    /* Movement - WASD or arrows */
    if (keyDown[72] || keyDown[17]) movePlayer(1.0);   /* Up / W */
    if (keyDown[80] || keyDown[31]) movePlayer(-1.0);  /* Down / S */
    if (keyDown[75] || keyDown[30]) strafePlayer(-1.0); /* Left / A */
    if (keyDown[77] || keyDown[32]) strafePlayer(1.0);  /* Right / D */

    /* Shooting - SPACE or mouse */
    if ((keyDown[57] || getMouseButton()) &&
        (now - lastShot) > CLOCKS_PER_SEC / 8) {  /* 8 shots per second max */
        playerShoot();
        lastShot = now;
    }
}

/*============================================================================
 * SPLASH SCREEN - VGA MODE ASCII ART
 *===========================================================================*/

void drawSplashScreen(void) {
    int x, y, i, px, py;
    int centerX = SCREEN_WIDTH / 2;

    clearScreen(COLOR_BLACK);

    /* Draw cool border */
    for (x = 0; x < SCREEN_WIDTH; x++) {
        setPixel(x, 0, COLOR_LRED);
        setPixel(x, 1, COLOR_RED);
        setPixel(x, SCREEN_HEIGHT-2, COLOR_RED);
        setPixel(x, SCREEN_HEIGHT-1, COLOR_LRED);
    }
    for (y = 0; y < SCREEN_HEIGHT; y++) {
        setPixel(0, y, COLOR_LRED);
        setPixel(1, y, COLOR_RED);
        setPixel(SCREEN_WIDTH-2, y, COLOR_RED);
        setPixel(SCREEN_WIDTH-1, y, COLOR_LRED);
    }

    /* MAZE RUNNER 2 title - big centered */
    drawText(centerX - 56, 20, "MAZE RUNNER 2", COLOR_YELLOW);

    /* Subtitle */
    drawText(centerX - 64, 35, "THE NEXT LEVEL", COLOR_LRED);

    /* Cool divider line */
    for (x = 40; x < SCREEN_WIDTH - 40; x++) {
        setPixel(x, 48, COLOR_CYAN);
        setPixel(x, 49, COLOR_LCYAN);
    }

    /* Features list */
    drawText(60, 60, "64X64 TEXTURES", COLOR_LGREEN);
    drawText(60, 75, "ENEMIES SHOOT BACK!", COLOR_LRED);
    drawText(60, 90, "WASD + MOUSE", COLOR_LCYAN);
    drawText(60, 105, "PROJECTILE COMBAT", COLOR_YELLOW);

    /* Draw some decorative pixels for cyberpunk feel */
    for (i = 0; i < 50; i++) {
        px = 20 + (rand() % 20);
        py = 60 + (rand() % 60);
        setPixel(px, py, COLOR_CYAN);
        px = SCREEN_WIDTH - 20 - (rand() % 20);
        setPixel(px, py, COLOR_CYAN);
    }

    /* Controls hint */
    drawText(centerX - 72, 130, "WASD - MOVE", COLOR_WHITE);
    drawText(centerX - 72, 142, "MOUSE - AIM", COLOR_WHITE);
    drawText(centerX - 72, 154, "SPACE - FIRE", COLOR_WHITE);

    /* Bottom divider */
    for (x = 40; x < SCREEN_WIDTH - 40; x++) {
        setPixel(x, 170, COLOR_CYAN);
    }

    /* Press key prompt */
    drawText(centerX - 80, 185, "PRESS ANY KEY", COLOR_BWHITE);

    displayFrame();
}

void drawCreditsScreen(void) {
    int x;
    int centerX = SCREEN_WIDTH / 2;

    clearScreen(COLOR_BLACK);

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

    drawText(centerX - 80, 154, "YEAR: 2025", COLOR_GRAY);

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
    "YEAR 2025",
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

int main(void) {
#ifdef __DJGPP__
    if (!__djgpp_nearptr_enable()) {
        printf("ERROR: Could not enable near pointers!\n");
        return 1;
    }
#endif

    /* Initialize systems */
    initDoubleBuffer();
    initFont();
    initTextures();
    initPlayer();
    initProjectiles();
    initEnemies();
    initMouse();

    /* Enter VGA mode */
    setVideoMode(0x13);

    /* Show credits splash */
    drawCreditsScreen();
    delay(1500);

    /* Show main splash screen */
    drawSplashScreen();
    getch();

    gameStartTime = clock();

    /* Main game loop */
    while (gameRunning && player.health > 0) {
        handleInput();
        updateProjectiles();
        updateEnemyAI();

        renderFrame();
        renderSprites();
        drawHUD();

        displayFrame();
    }

    /* Show scrolling credits */
    drawScrollingCredits();

    /* Return to text mode */
    setVideoMode(0x03);

    printf("\n");
    printf("========================================\n");
    if (player.health <= 0) {
        printf("          GAME OVER\n");
    } else {
        printf("       ESCAPE SUCCESSFUL!\n");
    }
    printf("========================================\n");
    printf("  Final Score: %d\n", player.score);
    printf("========================================\n");
    printf("\n");
    printf("  MAZE RUNNER 2\n");
    printf("  By VonHoltenCodes 2025\n");
    printf("  Thanks for playing!\n");
    printf("\n");

    freeDoubleBuffer();

#ifdef __DJGPP__
    __djgpp_nearptr_disable();
#endif

    return 0;
}
