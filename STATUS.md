# GBA Isometric Action Game — Status

## Current Version: v0.2

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
