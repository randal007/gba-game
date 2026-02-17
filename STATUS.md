# GBA Isometric Action Game — Status

## Current State: Pre-computed World Tilemap (v3)

### Architecture
- **Boot phase (~2-5s):** Renders entire 200×16 iso world in 256×80 pixel patches, deduplicates tiles into a dictionary (≤384 unique 8×8 tiles), builds a complete 432×217 world tilemap in EWRAM
- **Runtime:** 64×64 hardware tilemap is a ring buffer / sliding window into the pre-computed world tilemap. Scrolling = copying tilemap indices (u16 writes). **ZERO pixel rendering or tile dedup at runtime.**

### Memory Budget (EWRAM = 256KB)
| Buffer | Size |
|--------|------|
| world_tilemap (432×217×2) | ~183KB |
| tile_dict (384×64) | ~24KB |
| strip_buf (256×80) | ~20KB |
| world_map (200×16) | ~3KB |
| **Total** | **~230KB** |

### Key Files
- `src/main.c` — All game logic
- `include/game.h` — Constants, types, inline math
- `data/hero_walk.c/.h` — Hero sprite data
- `data/floor_iso.c/.h` — (legacy, unused in current renderer)

### What Works
- Procedurally generated 200×16 tile world (grass, stone, dirt, water, fortress)
- Isometric cube rendering with 4 terrain types
- Hardware Mode 0 BG with 8bpp tiles, 64×64 tilemap (BG_SIZE3)
- Smooth scrolling via ring buffer — no lag
- Hero sprite with 4-direction walk animation
- Camera follows player with lerp

### Commit History
- v1: Direct render + full re-render on drift (laggy)
- v2: Amortized re-render spread across frames (still laggy)
- v3: **Pre-compute everything at boot** (current — should be lag-free)
