#!/usr/bin/env python3
"""
Convert PNG sprites to 64x64 VGA palette C headers
Uses nearest-color matching to VGA 256-color palette
"""

from PIL import Image
import os
import sys

# Dark dungeon VGA palette - first 64 colors for our game
# Format: (R, G, B) for each index
DUNGEON_PALETTE = [
    # 0-15: Basic colors (darker than standard VGA)
    (0, 0, 0),        # 0: Black
    (0, 0, 42),       # 1: Dark blue
    (0, 32, 0),       # 2: Dark green
    (0, 32, 42),      # 3: Dark cyan
    (42, 0, 0),       # 4: Dark red
    (42, 0, 42),      # 5: Dark magenta
    (42, 21, 0),      # 6: Brown
    (48, 48, 48),     # 7: Light gray
    (24, 24, 24),     # 8: Dark gray
    (21, 21, 63),     # 9: Light blue
    (21, 63, 21),     # 10: Light green
    (21, 63, 63),     # 11: Light cyan
    (63, 21, 21),     # 12: Light red
    (63, 21, 63),     # 13: Light magenta
    (63, 63, 21),     # 14: Yellow
    (63, 63, 63),     # 15: White

    # 16-31: Grayscale ramp (for stone walls)
    (4, 4, 4),        # 16: Near black
    (8, 8, 8),        # 17
    (12, 12, 12),     # 18
    (16, 16, 16),     # 19
    (20, 20, 20),     # 20
    (24, 24, 24),     # 21
    (28, 28, 28),     # 22
    (32, 32, 32),     # 23
    (36, 36, 36),     # 24
    (40, 40, 40),     # 25
    (44, 44, 44),     # 26
    (48, 48, 48),     # 27
    (52, 52, 52),     # 28
    (56, 56, 56),     # 29
    (60, 60, 60),     # 30
    (63, 63, 63),     # 31: White

    # 32-47: Brown/tan ramp (for bricks, wood)
    (16, 8, 4),       # 32: Very dark brown
    (20, 12, 6),      # 33
    (24, 14, 8),      # 34
    (28, 16, 10),     # 35
    (32, 18, 12),     # 36
    (36, 22, 14),     # 37
    (40, 26, 16),     # 38
    (44, 30, 18),     # 39
    (48, 34, 20),     # 40
    (52, 38, 24),     # 41
    (56, 44, 28),     # 42
    (60, 50, 34),     # 43
    (63, 56, 40),     # 44
    (63, 60, 48),     # 45
    (63, 62, 54),     # 46
    (63, 63, 60),     # 47

    # 48-63: Red/flesh tones (for enemies)
    (16, 0, 0),       # 48: Dark red
    (24, 4, 4),       # 49
    (32, 8, 8),       # 50
    (40, 12, 8),      # 51
    (48, 16, 12),     # 52
    (56, 24, 16),     # 53
    (63, 32, 24),     # 54
    (63, 40, 32),     # 55
    (63, 48, 40),     # 56
    (63, 56, 48),     # 57
    (48, 32, 24),     # 58: Skin tone dark
    (56, 40, 32),     # 59: Skin tone mid
    (63, 52, 44),     # 60: Skin tone light
    (32, 48, 32),     # 61: Green (for creeper)
    (24, 40, 24),     # 62: Dark green
    (16, 32, 16),     # 63: Darker green
]

def find_nearest_color(r, g, b, a):
    """Find nearest palette color for given RGBA"""
    if a < 128:  # Transparent
        return 255  # Use 255 as transparent marker

    # Convert from 0-255 to 0-63 (VGA range)
    r6 = r >> 2
    g6 = g >> 2
    b6 = b >> 2

    best_idx = 0
    best_dist = 999999

    for idx, (pr, pg, pb) in enumerate(DUNGEON_PALETTE):
        dist = (r6-pr)**2 + (g6-pg)**2 + (b6-pb)**2
        if dist < best_dist:
            best_dist = dist
            best_idx = idx

    return best_idx

def convert_sprite(input_path, output_name, size=64):
    """Convert PNG to C header with palette indices"""
    img = Image.open(input_path).convert('RGBA')
    img = img.resize((size, size), Image.Resampling.LANCZOS)

    pixels = []
    for y in range(size):
        row = []
        for x in range(size):
            r, g, b, a = img.getpixel((x, y))
            idx = find_nearest_color(r, g, b, a)
            row.append(idx)
        pixels.append(row)

    # Generate C header
    header = f"""/* {output_name}.h - 64x64 sprite converted from {os.path.basename(input_path)} */
#ifndef {output_name.upper()}_H
#define {output_name.upper()}_H

#define {output_name.upper()}_WIDTH 64
#define {output_name.upper()}_HEIGHT 64
#define {output_name.upper()}_TRANSPARENT 255

static const unsigned char {output_name}[64 * 64] = {{
"""

    for y, row in enumerate(pixels):
        header += "    "
        for x, idx in enumerate(row):
            header += f"{idx:3d},"
        header += f"  /* row {y} */\n"

    header += "};\n\n#endif\n"

    return header

def main():
    sprites = [
        ("/home/vonholten/Von_Holten-Maze-Game/sprites_minions/gremlin_boss.png", "sprite_boss"),
        ("/home/vonholten/Von_Holten-Maze-Game/sprites_minions/gremlin_purple.png", "sprite_grunt"),
        ("/home/vonholten/Von_Holten-Maze-Game/sprites_minions/minion_jump.png", "sprite_soldier"),
        ("/home/vonholten/Von_Holten-Maze-Game/sprites_new/Creeper-1.png.png", "sprite_creeper"),
    ]

    for input_path, output_name in sprites:
        if os.path.exists(input_path):
            print(f"Converting {input_path}...")
            header = convert_sprite(input_path, output_name, 64)
            output_path = f"/home/vonholten/Von_Holten-maze-game-2/sprites/{output_name}.h"
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            with open(output_path, 'w') as f:
                f.write(header)
            print(f"  -> {output_path}")
        else:
            print(f"NOT FOUND: {input_path}")

    print("\nDone! Sprites converted to 64x64 headers.")

if __name__ == "__main__":
    main()
