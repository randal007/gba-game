# Final Fantasy Tactics Advance — Isometric Implementation Research

## Overview

FFTA uses a classic 2:1 isometric projection on the GBA (240×160 LCD). The game renders a diamond-shaped tile grid with height variation, layered backgrounds, and sprite-based characters. This doc breaks down how it works and what we can adapt for a real-time Zelda-like on GBA.

---

## 1. Tile Map Structure

### Isometric Diamond Grid

FFTA uses a **2:1 isometric ratio** — each tile's diamond shape is twice as wide as it is tall. Standard tile dimensions:

- **Tile footprint:** 32×16 pixels (the diamond base)
- **Full tile graphic:** 32×24 to 32×32 pixels (includes the vertical "side" face for height)

The map is a 2D grid in memory (typically around 15–30 tiles per side depending on the battle map), but rendered as a rotated diamond on screen.

### How Diamond Maps to GBA Backgrounds

The GBA's background system expects rectangular tile grids (8×8 pixel hardware tiles arranged in 32×32 or 64×64 tile maps). FFTA does **not** use hardware tilemaps directly for the isometric floor. Instead:

- **The isometric terrain is pre-rendered into background layers** — the map graphics are composited into large background images stored in VRAM, updated as the camera scrolls.
- Alternatively, the floor tiles are arranged in a **staggered rectangular pattern** in the tilemap: even rows are offset by half a tile width. This is the classic trick to fake isometric with a rectangular tilemap engine.

In practice, FFTA pre-composes map chunks and streams them into BG VRAM, using the GBA's hardware scroll registers to pan the camera.

### Map Data Format

Internally, each map cell stores:
- **Terrain type** (grass, stone, water, etc.)
- **Height level** (0–N, typically 0–7+)
- **Surface flags** (walkable, impassable, special)
- **Tile graphic index** referencing the tileset

Height is critical — it determines the vertical offset when rendering and affects gameplay (movement cost, line of sight, etc.).

---

## 2. Rendering & Depth Sorting

### The Painter's Algorithm

Isometric games draw back-to-front. In a 2:1 iso grid, "back" means tiles with lower (y + x) values in map coordinates. FFTA renders:

1. **Ground tiles** — drawn back-to-front across the entire visible map
2. **Vertical elements** (walls, pillars, tall objects) — drawn with the ground tile they sit on, or in a second pass
3. **Sprites** (characters, effects) — sorted by their map position and drawn on top

### Sorting Order

For a standard 2:1 isometric grid, the draw order is:

```
for each row (back to front, i.e. increasing screen-Y):
    for each column (left to right within that row):
        draw ground tile
        draw any object/character at this cell
```

The **sort key** is essentially `(map_y + map_x)` or equivalently the screen-Y position of the tile's base. Tiles with higher screen-Y are drawn later (in front).

### Height Complication

FFTA has multi-level terrain. A tile at height 3 is drawn higher on screen (lower screen-Y) but still needs to occlude things behind it. The approach:

- Each tile's **screen position** is offset upward by `height * pixels_per_height_unit` (typically 8–12 pixels per height level)
- Sort key remains based on **map grid position**, not screen position
- Tall objects on low tiles can still be occluded by ground on higher tiles behind them — this is handled by the strict back-to-front grid traversal

### Sprite Depth Sorting

Characters (rendered as OAM sprites) need to interleave with the background terrain. FFTA handles this by:

- Using **BG priority bits** and **OAM priority bits** to layer sprites between background layers
- Splitting the terrain into a **lower BG layer** (ground below the character) and an **upper BG layer** (terrain that should appear in front of the character)
- Characters at different heights/positions may change their OAM priority dynamically

For a real-time game, you'd sort all sprites each frame by their map position (y + x + height bias) and assign OAM entries accordingly.

---

## 3. Coordinate System & Math

### Map ↔ Screen Conversion

Given a 2:1 isometric projection with tile size `tw × th` (e.g., 32×16):

**Map → Screen:**
```
screen_x = (map_x - map_y) * (tw / 2)
screen_y = (map_x + map_y) * (th / 2) - (height * height_pixel_offset)
```

With tw=32, th=16:
```
screen_x = (map_x - map_y) * 16
screen_y = (map_x + map_y) * 8 - (height * 10)
```

Add a camera offset to both for scrolling:
```
screen_x = (map_x - map_y) * 16 - camera_x
screen_y = (map_x + map_y) * 8 - (height * 10) - camera_y
```

**Screen → Map (for input/cursor picking):**
```
map_x = (screen_x / (tw/2) + screen_y / (th/2)) / 2
map_y = (screen_y / (th/2) - screen_x / (tw/2)) / 2
```

This gives floating-point map coords. Floor to get the tile. For pixel-perfect picking, test against the diamond shape within the tile (check which quadrant of the bounding rectangle the point falls in).

### Sub-tile Positioning (for Real-Time Movement)

Since we want Zelda-like free movement, characters need sub-tile positions. Use **fixed-point map coordinates** (e.g., 8.8 fixed point — 8 bits integer tile, 8 bits sub-tile fraction). The screen conversion math is the same, just with fractional inputs.

Height can also be fractional for jumping/falling animations:
```
screen_y = (map_x + map_y) * 8 - (height_fixedpoint * 10 >> 8)
```

### Collision

For real-time movement on an iso grid, collision is done in **map space**, not screen space. The map is a regular 2D grid — movement and collision are simple axis-aligned checks in map coordinates, just like a top-down game. The isometric projection is purely visual.

---

## 4. GBA-Specific Techniques

### Background Modes

**Mode 0** (most likely used by FFTA):
- 4 background layers (BG0–BG3), all tile-based
- Each BG is a grid of 8×8 tiles with palette, flip, and priority bits
- Hardware scroll per layer (per-scanline tricks possible via HBlank IRQ)
- Best for layered 2D — FFTA uses this for its map layers + UI

**Mode 1:**
- 2 regular BGs + 1 affine BG — sometimes used for rotation effects but less relevant here

**Mode 2:**
- 2 affine (rotation/scale) BGs — used by games that rotate the camera (like FF6 world map)
- FFTA does **not** rotate the camera, so Mode 2 is unnecessary
- Affine mode also has smaller tilemap sizes and no flip bits

**FFTA almost certainly uses Mode 0** for battle maps, switching to other modes only for special effects (world map, menus).

### Background Layer Usage in FFTA

Typical allocation:
| Layer | Purpose | Priority |
|-------|---------|----------|
| BG0   | UI / text / menus | 0 (frontmost) |
| BG1   | Upper terrain (walls, rooftops — things that occlude characters) | 1 |
| BG2   | Lower terrain (ground, floor surfaces) | 2 |
| BG3   | Background / sky / distant scenery | 3 (backmost) |

Characters (OAM sprites) are rendered between BG1 and BG2 by setting OAM priority = 2. This lets the ground (BG2) sit behind characters while upper terrain (BG1) occludes them.

### OAM / Sprites

The GBA has **128 OAM entries** (hardware sprites):
- Sizes from 8×8 to 64×64
- 4bpp (16 colors) or 8bpp (256 colors)
- Each has X, Y, priority, palette, flip, and affine transform index

FFTA characters are typically **32×32 or 64×64 OAM sprites** (multi-cell for large characters). With 128 OAM slots, you can show ~6–10 detailed characters plus UI elements, cursors, and effects.

For depth sorting, FFTA assigns OAM priority bits to interleave sprites with BG layers. Within the same priority level, **OAM index determines draw order** (lower index = drawn first = behind higher index). So sprites are sorted each frame and OAM entries are written in back-to-front order.

### Affine Transforms

The GBA has 32 affine parameter sets for OAM. FFTA uses these sparingly:
- Scaling effects for magic spells
- Rotation for certain ability animations
- Not used for the base isometric projection (that's baked into the art)

The isometric look is **entirely in the artwork** — tiles are drawn as diamonds by the artists. No runtime rotation.

### VRAM Management

GBA VRAM is tight (96 KB total):
- **BG tile data:** ~32 KB for map tilesets (shared across layers)
- **BG tilemaps:** ~2–8 KB per map (32×32 = 2 KB, 64×64 = 8 KB)
- **OAM tile data:** ~16–32 KB for character sprites
- Maps stream tiles in/out of VRAM as the camera scrolls (only the visible portion + buffer zone is loaded)

FFTA's tile art is heavily **palette-swapped** — same tile shapes with different 16-color palettes for terrain variants, reducing VRAM usage.

---

## 5. How FFTA Specifically Handles Its Maps

### Layered Isometric Terrain

FFTA battle maps feature:
- **Multi-height terrain** — tiles at different elevation levels create hills, cliffs, ramps
- **Layered rendering** — ground layer + overlay layer for things that should appear in front of characters
- **No camera rotation** — fixed isometric angle (unlike FFT on PS1 which had 4 rotation angles)

### Height System

- Discrete height levels (integers), typically 0–7
- Each height level shifts the tile up ~8–12 pixels on screen
- Ramps/slopes exist as special tile types connecting adjacent height levels
- Characters standing on different height levels are Y-sorted correctly via the BG layer split + OAM priority system

### Map Construction

Each FFTA map is defined by:
1. **A 2D grid of tile descriptors** — terrain type, height, surface properties
2. **A tileset** — the 8×8 hardware tiles composing the isometric art
3. **Pre-built BG tilemaps** — the isometric tiles pre-arranged into GBA-compatible 8×8 tile grids for BG2 (floor) and BG1 (upper)
4. **Object/decoration placements** — trees, rocks, crystals, etc.

The map likely stores the logical grid and has **pre-rendered the BG tilemaps offline** (in the ROM) rather than computing them at runtime. This is common for GBA tactical games.

### Scrolling

- Camera follows the active unit or cursor
- Hardware scroll registers (BG0HOFS/BG0VOFS etc.) handle smooth pixel scrolling
- When the camera moves far enough, edge tiles are streamed into the circular tilemap buffer (GBA tilemaps wrap)

### Character Rendering on Maps

- Each character is 1–2 OAM sprites (body + weapon/effect)
- Positioned each frame based on their map coordinates → screen coordinate conversion
- Sorted each frame by map position for correct depth
- Animation frames index into OAM tile memory
- Shadow sprites rendered beneath characters on the ground plane

---

## 6. Adaptation Notes for Real-Time Zelda-Like

### What Changes

| FFTA (Turn-Based) | Our Game (Real-Time) |
|---|---|
| Characters snap to tiles | Characters move freely (sub-tile positions) |
| One character moves at a time | Multiple entities moving simultaneously |
| Simple depth sort (re-sort on move) | Must re-sort every frame |
| Grid-based collision | Continuous collision detection in map space |
| Camera follows cursor | Camera follows player smoothly |

### Key Recommendations

1. **Keep collision in map space** — treat it as a top-down game internally. The iso projection is just rendering.

2. **Fixed-point math** — use 24.8 or 16.8 fixed point for positions. The GBA has no FPU.

3. **Depth sort every frame** — with free movement, you need to sort all visible entities by (map_x + map_y) each frame. With <32 entities this is trivial (insertion sort).

4. **Height as gameplay** — support jumping/falling with a Z component. Render offset is just `screen_y -= z * scale`. Collision with height: check if entity Z overlaps tile height.

5. **Use Mode 0 with 2–3 BG layers** — ground, upper terrain, UI. Characters as OAM sprites between ground and upper layers.

6. **Tile streaming** — pre-build isometric tilemaps in ROM, stream visible portions into VRAM as camera moves. Use the GBA's tilemap wrapping.

7. **OAM budget** — 128 sprites total. Budget: ~64 for game entities, ~32 for effects/particles, ~32 for UI. Each character may need 2–4 OAM entries.

8. **Avoid affine for core rendering** — bake the isometric look into art. Use affine only for special effects.

---

## References & Further Reading

- GBA hardware docs: GBATEK by Martin Korth (the definitive GBA technical reference)
- Tonc (GBA programming tutorial) — excellent Mode 0/affine/OAM coverage
- "Isometric Game Programming with DirectX 7.0" (Ernest Pazera) — general iso math
- FFTA disassembly/ROM analysis communities for specifics
