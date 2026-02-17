#!/usr/bin/env python3
"""Generate FFTA-style isometric tiles for GBA game v0.2 stacking system."""
from PIL import Image, ImageDraw
import os

OUT = os.path.dirname(os.path.abspath(__file__))

# --- Palettes (max 16 colors each, index 0 = transparent) ---
# Format: list of (R,G,B) tuples, index 0 is transparent (magenta)
TRANS = (255, 0, 255)

PAL_GRASS = [
    TRANS,
    (26, 12, 0),     # 1: dark outline
    (56, 120, 40),   # 2: grass dark
    (72, 144, 48),   # 3: grass mid
    (96, 168, 64),   # 4: grass light
    (120, 184, 80),  # 5: grass highlight
    (80, 56, 32),    # 6: dirt dark
    (112, 80, 48),   # 7: dirt mid
    (144, 104, 64),  # 8: dirt light
    (64, 136, 44),   # 9: grass mid-dark
    (104, 176, 72),  # 10: grass mid-light
    (96, 72, 40),    # 11: dirt mid-dark
    (128, 96, 56),   # 12: dirt mid-light
    (48, 104, 32),   # 13: grass very dark
    (160, 120, 80),  # 14: dirt highlight
    (40, 88, 24),    # 15: grass deepest
]

PAL_STONE = [
    TRANS,
    (26, 12, 0),
    (80, 80, 88),    # 2: stone dark
    (104, 104, 112),  # 3: stone mid
    (128, 128, 136),  # 4: stone light
    (152, 152, 160),  # 5: stone highlight
    (64, 64, 72),    # 6: stone very dark
    (96, 96, 104),   # 7: stone mid2
    (144, 144, 152),  # 8: stone light2
    (56, 56, 64),    # 9: darkest
    (112, 112, 120),  # 10
    (136, 136, 144),  # 11
    (72, 72, 80),    # 12
    (88, 88, 96),    # 13
    (160, 160, 168),  # 14: brightest
    (48, 48, 56),    # 15
]

PAL_DIRT = [
    TRANS,
    (26, 12, 0),
    (96, 64, 32),    # 2: dirt dark
    (128, 88, 48),   # 3: dirt mid
    (160, 112, 64),  # 4: dirt light
    (184, 136, 80),  # 5: dirt highlight
    (72, 48, 24),    # 6: very dark
    (112, 76, 40),   # 7
    (144, 100, 56),  # 8
    (80, 56, 28),    # 9
    (120, 84, 44),   # 10
    (152, 108, 60),  # 11
    (176, 128, 72),  # 12
    (64, 40, 20),    # 13: darkest
    (192, 144, 88),  # 14: brightest
    (104, 72, 36),   # 15
]

PAL_WATER = [
    TRANS,
    (26, 12, 0),
    (32, 64, 128),   # 2: water dark
    (48, 88, 152),   # 3: water mid
    (64, 112, 176),  # 4: water light
    (96, 144, 200),  # 5: water highlight
    (24, 48, 104),   # 6: very dark
    (40, 72, 136),   # 7
    (56, 96, 160),   # 8
    (80, 128, 192),  # 9
    (112, 160, 208),  # 10: sparkle
    (144, 184, 224),  # 11: bright sparkle
    (36, 56, 112),   # 12
    (72, 104, 168),  # 13
    (128, 168, 216),  # 14
    (16, 40, 96),    # 15: deepest
]

PAL_BRICK = [
    TRANS,
    (26, 12, 0),
    (120, 64, 40),   # 2: brick dark
    (152, 88, 56),   # 3: brick mid
    (176, 112, 72),  # 4: brick light
    (200, 136, 88),  # 5: brick highlight
    (96, 48, 32),    # 6: mortar dark
    (144, 136, 120), # 7: mortar light
    (104, 56, 36),   # 8
    (136, 80, 52),   # 9
    (168, 104, 68),  # 10
    (184, 120, 80),  # 11
    (80, 40, 24),    # 12: darkest brick
    (160, 144, 128), # 13: mortar highlight
    (112, 72, 44),   # 14
    (192, 128, 84),  # 15
]

PAL_ROOF = [
    TRANS,
    (26, 12, 0),
    (136, 48, 32),   # 2: roof dark red
    (168, 64, 40),   # 3: roof mid
    (192, 80, 48),   # 4: roof light
    (216, 104, 64),  # 5: roof highlight
    (112, 40, 24),   # 6: very dark
    (152, 56, 36),   # 7
    (180, 72, 44),   # 8
    (200, 88, 56),   # 9
    (224, 120, 80),  # 10: brightest
    (104, 32, 20),   # 11: darkest
    (144, 48, 32),   # 12
    (160, 60, 38),   # 13
    (208, 96, 60),   # 14
    (88, 28, 16),    # 15
]


def draw_iso_diamond(img, ox, oy, w, h, fill_func):
    """Draw an isometric diamond (top face). w=32, h=16 for our tiles.
    fill_func(x, y) -> color index or None for transparent.
    """
    draw = ImageDraw.Draw(img)
    cx, cy = ox + w // 2, oy
    # Diamond: top=(cx, oy), right=(ox+w, oy+h//2), bottom=(cx, oy+h), left=(ox, oy+h//2)
    for py in range(h):
        if py < h // 2:
            # Top half: expanding
            half_w = (py * w) // h
        else:
            # Bottom half: contracting
            half_w = ((h - 1 - py) * w) // h
        for px_off in range(-half_w, half_w + 1):
            px = cx + px_off - 1
            if 0 <= px < img.width and 0 <= (oy + py) < img.height:
                c = fill_func(px - ox, py)
                if c is not None:
                    img.putpixel((px, oy + py), c)


def make_ground_tile(name, palette, texture_func):
    """Create a 32x16 ground tile (iso diamond top face)."""
    img = Image.new('RGB', (32, 16), TRANS)
    
    def fill(x, y):
        return texture_func(x, y)
    
    draw_iso_diamond(img, 0, 0, 32, 16, fill)
    
    # Draw outline
    draw = ImageDraw.Draw(img)
    outline_col = palette[1]
    # Top-left edge
    for i in range(16):
        px, py = 15 - i, i // 2
        if 0 <= px < 32 and 0 <= py < 16:
            img.putpixel((px, py), outline_col)
    # Top-right edge
    for i in range(16):
        px, py = 16 + i, i // 2
        if 0 <= px < 32 and 0 <= py < 16:
            img.putpixel((px, py), outline_col)
    # Bottom-left edge
    for i in range(16):
        px, py = i, 8 + i // 2
        if 0 <= px < 32 and 0 <= py < 16:
            img.putpixel((px, py), outline_col)
    # Bottom-right edge
    for i in range(16):
        px, py = 31 - i, 8 + i // 2
        if 0 <= px < 32 and 0 <= py < 16:
            img.putpixel((px, py), outline_col)
    
    path = os.path.join(OUT, f"ground_{name}.png")
    img.save(path)
    print(f"  Saved {path}")
    return img


def make_side_tile(name, palette, texture_func, width=32, height=16):
    """Create a side/wall tile for stacking. Shows the vertical face.
    Shape: parallelogram - top edge slopes 2:1 right, bottom edge slopes 2:1 right.
    Actually for iso stacking, the side is visible as two faces:
    - Left face: parallelogram leaning right
    - Right face: parallelogram leaning left
    We'll make a combined side tile that shows both left and right faces of one column.
    Width=32, Height=16 (one elevation unit = 16px to match our scale).
    """
    img = Image.new('RGB', (width, height), TRANS)
    outline = palette[1]
    
    # Left face: from x=0..15, slanting
    # Top edge goes from (0, 0) to (15, 8) - the bottom-left iso edge
    # Bottom edge goes from (0, height) to (15, 8+height)... but we clip
    # Actually let's think simpler: the side tile is a rectangle 32x16 with
    # the left half being the left face (darker) and right half being right face (slightly less dark)
    
    # Left face (darker)
    for y in range(height):
        for x in range(16):
            # Check if inside left parallelogram
            # Top edge: y = x/2 (from point 0,0 going to 16,8)
            # Bottom edge: y = x/2 + height (but clipped)
            # Left edge: x = 0
            # Right edge: x = 15
            top_y = x // 2
            if y >= 0:  # Always visible in our rectangular tile
                c = texture_func(x, y, 'left')
                if c is not None:
                    img.putpixel((x, y), c)
    
    # Right face (slightly lighter than left, but still darker than top)
    for y in range(height):
        for x in range(16, 32):
            top_y = (31 - x) // 2
            c = texture_func(x, y, 'right')
            if c is not None:
                img.putpixel((x, y), c)
    
    # Vertical center line
    for y in range(height):
        img.putpixel((15, y), outline)
        img.putpixel((16, y), outline)
    
    # Outer edges
    for y in range(height):
        img.putpixel((0, y), outline)
        img.putpixel((31, y), outline)
    
    path = os.path.join(OUT, f"side_{name}.png")
    img.save(path)
    print(f"  Saved {path}")
    return img


def dither_2col(x, y, c1, c2, density=0.5):
    """Simple checkerboard dither between two colors."""
    if (x + y) % 2 == 0:
        return c1 if ((x * 7 + y * 13) % 10) / 10.0 > density else c2
    else:
        return c2 if ((x * 7 + y * 13) % 10) / 10.0 > density else c1


def noise_select(x, y, colors):
    """Pseudo-random color selection from list based on position."""
    h = (x * 2654435761 + y * 40503) & 0xFFFFFFFF
    return colors[h % len(colors)]


# ============ GROUND TILES ============

print("Generating ground tiles...")

# Grass ground
def grass_texture(x, y):
    h = ((x * 7 + y * 13 + x * y) * 2654435761) & 0xFFFF
    if h % 17 == 0:
        return PAL_GRASS[5]  # highlight tuft
    elif h % 7 == 0:
        return PAL_GRASS[2]  # dark patch
    elif h % 3 == 0:
        return PAL_GRASS[4]  # light
    elif h % 5 == 0:
        return PAL_GRASS[9]  # mid-dark
    else:
        return PAL_GRASS[3]  # mid base

make_ground_tile("grass", PAL_GRASS, grass_texture)

# Stone ground
def stone_texture(x, y):
    h = ((x * 31 + y * 17) * 48271) & 0xFFFF
    # Add some crack lines
    if (x + y * 3) % 11 == 0:
        return PAL_STONE[2]  # dark crack
    elif h % 13 == 0:
        return PAL_STONE[5]  # highlight
    elif h % 5 == 0:
        return PAL_STONE[4]  # light
    elif h % 3 == 0:
        return PAL_STONE[7]  # mid2
    else:
        return PAL_STONE[3]  # mid base

make_ground_tile("stone", PAL_STONE, stone_texture)

# Dirt ground
def dirt_texture(x, y):
    h = ((x * 23 + y * 41) * 16807) & 0xFFFF
    if h % 19 == 0:
        return PAL_DIRT[5]  # highlight pebble
    elif h % 7 == 0:
        return PAL_DIRT[2]  # dark
    elif h % 3 == 0:
        return PAL_DIRT[4]  # light
    elif h % 5 == 0:
        return PAL_DIRT[7]  # mid2
    else:
        return PAL_DIRT[3]  # mid

make_ground_tile("dirt", PAL_DIRT, dirt_texture)

# Water ground
def water_texture(x, y):
    h = ((x * 11 + y * 37) * 65521) & 0xFFFF
    # Wavy pattern
    wave = ((x + y * 2) % 8)
    if wave < 2:
        if h % 11 == 0:
            return PAL_WATER[10]  # sparkle
        return PAL_WATER[2]  # dark trough
    elif wave < 4:
        return PAL_WATER[3]  # mid
    elif wave < 6:
        return PAL_WATER[4]  # light crest
    else:
        if h % 7 == 0:
            return PAL_WATER[11]  # bright sparkle
        return PAL_WATER[5]  # highlight

make_ground_tile("water", PAL_WATER, water_texture)


# ============ SIDE/WALL TILES ============

print("Generating side tiles...")

# Grass side (dirt cross-section with grass on top edge)
def grass_side_tex(x, y, face):
    pal = PAL_GRASS
    if face == 'left':
        # Darker face
        if y < 2:
            # Grass roots at top
            h = (x * 31 + y * 7) & 0xFF
            return pal[13] if h % 3 == 0 else pal[2]
        else:
            # Dirt/earth body
            h = ((x * 23 + y * 17) * 48271) & 0xFFFF
            if h % 11 == 0:
                return pal[8]  # light dirt
            elif h % 5 == 0:
                return pal[6]  # dark dirt
            else:
                return pal[7]  # mid dirt
    else:
        # Right face - slightly lighter
        if y < 2:
            h = (x * 31 + y * 7) & 0xFF
            return pal[9] if h % 3 == 0 else pal[15]
        else:
            h = ((x * 23 + y * 17) * 48271) & 0xFFFF
            if h % 11 == 0:
                return pal[14]  # highlight
            elif h % 5 == 0:
                return pal[7]  # mid
            else:
                return pal[8]  # light

make_side_tile("grass_edge", PAL_GRASS, grass_side_tex)

# Stone wall side
def stone_side_tex(x, y, face):
    pal = PAL_STONE
    # Brick-like pattern
    row = y // 4
    offset = 8 if row % 2 == 0 else 0
    brick_x = (x + offset) % 16
    # Mortar lines
    if y % 4 == 0:
        return pal[9]  # dark mortar line
    if brick_x == 0:
        return pal[9]  # vertical mortar
    
    if face == 'left':
        h = ((x * 13 + y * 29) * 16807) & 0xFFFF
        if h % 7 == 0:
            return pal[2]  # dark
        elif h % 5 == 0:
            return pal[6]  # very dark
        else:
            return pal[12]  # mid-dark
    else:
        h = ((x * 13 + y * 29) * 16807) & 0xFFFF
        if h % 7 == 0:
            return pal[4]  # light
        elif h % 5 == 0:
            return pal[7]  # mid
        else:
            return pal[3]  # mid base

make_side_tile("stone_wall", PAL_STONE, stone_side_tex)

# Dirt wall side
def dirt_side_tex(x, y, face):
    pal = PAL_DIRT
    # Layered earth look
    layer = y // 3
    if face == 'left':
        base_colors = [pal[6], pal[2], pal[9], pal[13], pal[6]]
        base = base_colors[layer % len(base_colors)]
        h = ((x * 17 + y * 23) * 65521) & 0xFFFF
        if h % 9 == 0:
            return pal[7]  # lighter spot
        elif h % 13 == 0:
            return pal[13]  # dark spot
        return base
    else:
        base_colors = [pal[7], pal[3], pal[10], pal[15], pal[7]]
        base = base_colors[layer % len(base_colors)]
        h = ((x * 17 + y * 23) * 65521) & 0xFFFF
        if h % 9 == 0:
            return pal[8]  # lighter
        elif h % 13 == 0:
            return pal[6]  # darker
        return base

make_side_tile("dirt_wall", PAL_DIRT, dirt_side_tex)


# ============ BUILDING TILES ============

print("Generating building tiles...")

# Brick wall
def brick_wall_tex(x, y, face):
    pal = PAL_BRICK
    row = y // 4
    offset = 6 if row % 2 == 0 else 0
    brick_x = (x + offset) % 12
    
    # Mortar
    if y % 4 == 0:
        return pal[7] if face == 'right' else pal[6]
    if brick_x == 0:
        return pal[7] if face == 'right' else pal[6]
    
    if face == 'left':
        h = ((x * 7 + y * 19 + row * 31) * 48271) & 0xFFFF
        if h % 11 == 0:
            return pal[2]  # dark brick
        elif h % 7 == 0:
            return pal[8]  # mid-dark
        else:
            return pal[9]  # mid
    else:
        h = ((x * 7 + y * 19 + row * 31) * 48271) & 0xFFFF
        if h % 11 == 0:
            return pal[3]  # mid
        elif h % 7 == 0:
            return pal[10]  # light
        else:
            return pal[4]  # light

make_side_tile("brick_wall", PAL_BRICK, brick_wall_tex)

# Roof tile (top face - iso diamond like ground but with roof texture)
def roof_texture(x, y):
    pal = PAL_ROOF
    # Ridge lines going diagonally
    ridge = (x + y * 2) % 6
    h = ((x * 41 + y * 13) * 16807) & 0xFFFF
    if ridge < 1:
        return pal[6]  # dark ridge shadow
    elif ridge < 2:
        return pal[2]  # dark
    elif ridge < 4:
        if h % 7 == 0:
            return pal[5]  # highlight
        return pal[3]  # mid
    else:
        if h % 9 == 0:
            return pal[8]  # light detail
        return pal[4]  # light

make_ground_tile("roof", PAL_ROOF, roof_texture)

# Also make a roof side tile (the eave/edge visible from the side)
def roof_side_tex(x, y, face):
    pal = PAL_ROOF
    if y < 3:
        # Eave overhang - darker
        if face == 'left':
            return pal[11]  # darkest
        else:
            return pal[6]
    else:
        # Under-eave shadow / wall transition
        if face == 'left':
            h = ((x * 13 + y * 7) * 65521) & 0xFFFF
            return pal[12] if h % 3 == 0 else pal[2]
        else:
            h = ((x * 13 + y * 7) * 65521) & 0xFFFF
            return pal[7] if h % 3 == 0 else pal[3]

make_side_tile("roof_edge", PAL_ROOF, roof_side_tex)

print("\nAll tiles generated!")
print(f"Output directory: {OUT}")
