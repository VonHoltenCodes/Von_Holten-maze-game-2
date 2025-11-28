# MAZE RUNNER 2 - P4 ERA DOS RAYCASTER

A next-generation raycasting FPS game for DOS, targeting Pentium 4 era hardware.

## Features

### P4-Era Enhancements (over Maze Runner 1)
- **64x64 Procedural Textures** - High-resolution brick, stone, metal, and tech wall textures
- **Enemies That SHOOT BACK** - Real combat AI with projectile system
- **Projectile System** - Track bullets in 3D space, both player and enemy
- **WASD + Mouse Controls** - Modern FPS control scheme
- **Distance Fog** - Atmospheric depth effects
- **Gradient Shading** - Smooth ceiling and floor rendering

### Enemy Types
- **Grunt (Green)** - Basic melee enemy, 50 HP
- **Soldier (Red)** - Ranged attacker, fires at player, 75 HP
- **Elite (Magenta)** - Fast-firing ranged enemy, 150 HP
- **Boss (Yellow)** - Rapid fire, high HP

### Technical Specs
- **Target Hardware:** Pentium 4 / 1.5GHz+
- **Resolution:** 320x200 VGA Mode 13h (256 colors)
- **Textures:** 64x64 procedural (runtime generated)
- **Compiler:** DJGPP cross-compiler
- **Optimizations:** -O3, -march=pentium4, -ffast-math

## Controls

| Key | Action |
|-----|--------|
| W / Up | Move Forward |
| S / Down | Move Backward |
| A / Left | Strafe Left |
| D / Right | Strafe Right |
| Mouse | Look Around |
| Space / LMB | Shoot |
| ESC | Quit |

## Building

### Requirements
- DJGPP cross-compiler (i586-pc-msdosdjgpp-gcc)
- Make

### Compile
```bash
make
```

### Create Floppy Image
```bash
make floppy
```

## Evolution from Maze Runner 1

| Feature | Maze Runner 1 | Maze Runner 2 |
|---------|---------------|---------------|
| Target CPU | 133MHz Pentium | Pentium 4 1.5GHz+ |
| Texture Size | 8x8 | 64x64 |
| Textures | Hardcoded | Procedural |
| Enemy Combat | Melee only | Ranged (shoot back!) |
| Controls | Arrows only | WASD + Mouse |
| Projectiles | No | Yes |

## Credits

**Created by:** Trent Von Holten
**Studio:** VonHoltenCodes
**Year:** 2025
**License:** Open Source

Based on the foundation of [Von_Holten-Maze-Game](https://github.com/VonHoltenCodes/Von_Holten-Maze-Game) v3.0
