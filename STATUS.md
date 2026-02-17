# GBA Isometric Game — Status

## Current State (2026-02-16)

### ✅ v0.2 Stackable Isometric Tile Assets (NEW)

Created FFTA-style isometric tiles designed for height/elevation stacking system.
All tiles are 4bpp indexed PNGs, grit-compatible, in `assets/tiles/`.

**Ground tiles (32×16 iso diamond, top face):**
- `ground_grass.png` — Lush green with subtle tuft highlights
- `ground_stone.png` — Grey stone with crack details
- `ground_dirt.png` — Warm brown earth with pebble texture
- `ground_water.png` — Blue with wave pattern and sparkle highlights
- `ground_roof.png` — Red-orange clay roof with ridge lines

**Side/wall tiles (32×16, left+right vertical faces):**
- `side_grass_edge.png` — Grass roots on top, dirt/earth cross-section below
- `side_stone_wall.png` — Brick-pattern stone with mortar lines
- `side_dirt_wall.png` — Layered earth strata
- `side_brick_wall.png` — Building brick wall with mortar
- `side_roof_edge.png` — Roof eave/overhang edge

**Stacking system:** Each side tile has left face (darker) and right face (lighter) split at center. Top faces are bright/detailed, sides are darker per FFTA style. Side tiles tile vertically without seams for arbitrary cliff heights.

**Palettes:** 6 palettes (grass, stone, dirt, water, brick, roof), each ≤16 colors, FFTA warm color style with dark brown outlines (#1a0c00).

**Generator script:** `assets/tiles/gen_tiles.py` — can regenerate/modify all tiles.

### ✅ Side-Scroller World with Tilemap Streaming

Changed from 16×16 diamond world to **200×16 long strip** with sliding-window renderer:

- **World:** 200 tiles wide × 16 tiles tall — a long side-scrolling isometric strip
- **Renderer:** Sliding-window tilemap streaming. A 512×320 pixel buffer covers the visible area + margin. When the camera drifts >80px from window center, the visible portion is re-rendered, converted to tiles, and uploaded to VRAM.
- **Terrain:** Procedurally generated with varied features along the strip:
  - Winding stone road through the middle
  - River crossing (cols 40-55)
  - Small pond (cols 70-75)
  - Large lake (cols 100-115)
  - Desert/dirt region (cols 130-160)
  - Stone fortress at the far right (cols 175-195)
  - Scattered stone clusters and dirt trails
- **Player spawn:** Left side of the world (col 3, row 8)
- **Camera:** Follows player with lerp, clamped to world edges — no wrapping
- **Player bounds:** Clamped to world edges — cannot walk off the map

### ✅ Scroll Hitch Fix (Progressive Re-render)

Previously, crossing the 80px drift threshold triggered a full re-render of the pixel buffer + tile dedup + VRAM upload in a single frame, causing 2-3 second freezes every ~80px of camera movement.

**Fix:** Amortized the re-render across ~10 frames:
- **Phase 1** (4 frames): Render iso cubes in column slices (~50 columns/frame)
- **Phase 2** (5 frames): Build tileset+tilemap in row slices (~8 tile rows/frame)
- **Phase 3** (1 frame): Upload to VRAM

Old VRAM content stays visible during phases 1-2, so no visual glitch. The display updates atomically on phase 3. Threshold also increased from 80px to 110px to reduce re-render frequency.

### Architecture
- Mode 0, BG0: 8bpp, 64×64 tilemap (512×512px BG), charblock 0, screenblock 28
- OBJ sprite hero: 4bpp, charblock 4
- World map generated procedurally at startup into EWRAM
- Sliding window re-renders only when camera drifts past threshold (not every frame)
- Tile deduplication keeps unique tile count within VRAM limits

### Previous: Mode 0 Tiled Renderer
- Pre-rendered entire 16×16 world once at boot into 512×320 pixel buffer
- Converted to deduplicated 8bpp tiles + 64×64 tilemap
- Achieved 60fps via hardware BG scrolling

### Features
- 200×16 isometric world with 4 tile types (grass, stone, dirt, water)
- Isometric cube rendering with diamond top + side faces
- 4-direction hero sprite with walk animation
- Smooth camera follow (lerp) with edge clamping
- Tilemap streaming for worlds larger than 512×512px hardware limit

### Build
```
make        # builds isogame.gba
make run    # launches in mgba-qt
make clean  # clean build artifacts
```
