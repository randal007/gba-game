# GBA Isometric Action Game — Status

## Current Version: v0.2.2

### v0.2.2 — Isometric Parallelogram Side Faces (2026-02-16)
- **Fixed flat rectangular wall faces**: Side faces were rendered as axis-aligned rectangles (straight vertical drop). Now rendered as proper 2:1 isometric parallelograms following the diamond edges — left face slopes down-right, right face slopes down-left, matching FFTA-style iso rendering.
- **New `stamp_side_face()` function**: Renders left/right parallelogram faces with per-pixel compositing. Samples texture from existing 32×16 side metatile PNGs (left half → left face, right half → right face) with vertical tiling for tall walls.
- **Single face per cell**: Instead of stacking h separate rectangular side faces, draws one continuous parallelogram per face with height = h × SIDE_HEIGHT. Eliminates stacking artifacts.
- **Geometry**: Left face parallelogram vertices: diamond-left → diamond-bottom → drop by face_h → back. Right face mirrors. Top/bottom edges slope at 2:1 iso angle, side edges are vertical.
- **Increased limits**: MAX_PRECOMP_TILES 512→896 (VRAM safe with SBB 28), HASH_SIZE 1024→2048, WORLD_PX_Y1 1752→1800 (accounts for parallelogram overhang).
- **Removed old dead code**: stamp_side_left/stamp_side_right (unused precursor functions).

### v0.2.1 — Transparency Fix (2026-02-16)
- **Fixed magenta bleed bug**: Source tile PNGs used opaque magenta (255,0,255) as transparency key, but the converter only checked alpha channel. Magenta pixels were assigned palette index 1 and rendered visibly between diamond tiles.
- **convert_tiles.py**: Now treats both alpha < 128 AND opaque magenta (R≥240, G<16, B≥240) as transparent (palette index 0). Magenta removed from palette entirely (52→51 colors).
- **Palette index 0**: Set `pal_bg_mem[0]` to `RGB15(2,2,5)` (dark blue-black) so any remaining index-0 pixels show as the background color instead of black.
- **Regenerated metatiles.c/h**: Clean tile data with proper transparency in diamond corners.

### v0.2 — Metatile Engine + Height Stacking (2025-02-16)
Major engine rewrite:
- **Metatile system**: Replaced pixel-by-pixel boot rendering with pre-made 8×8 tile compositing. Each iso tile type (ground/side) is a 4×2 metatile arrangement. Boot builds world tilemap by stamping metatile indices with transparency compositing and hash-based dedup. Boot time: near-instant (was ~30 seconds).
- **Height stacking**: Each map cell has a height value (0–4). Renderer draws back-to-front, bottom-to-top: side face metatiles stack below the top face. Creates visible cliffs, walls, and elevation changes.
- **Pixel's tile art integrated**: 10 tile PNGs from `assets/tiles/` converted via `tools/convert_tiles.py` into 81 unique 8×8 tiles with a 52-color unified palette. Ground types: grass, stone, dirt, water, roof. Side types: grass_edge, stone_wall, dirt_wall, brick_wall, roof_edge.
- **World design**: 200×16 strip with varied terrain:
  - Rolling grass hills (height 2–3) at three locations
  - River valley (height 0 water) with dirt banks
  - Lake near center
  - Brick fortress with corner towers (height 4), courtyard, and inner hall
  - Stone ruins at the far end
  - Dirt patches and a small pond near the start
- **Kept**: Player movement, camera follow, OBJ sprite hero — all unchanged

### v0.1.1 — Pre-computed World Tilemap (2025-02-15)
- Entire world rendered at boot → tile dictionary + world tilemap in EWRAM
- Hardware ring buffer streams tiles as camera scrolls
- Zero runtime rendering cost

### v0.1 — Initial Working Build (2025-02-15)
- Isometric rendering with procedural cube art
- Hero sprite with 4-direction walk animation
- Camera follow, D-pad movement
- 200×16 world with grass, stone, dirt, water
