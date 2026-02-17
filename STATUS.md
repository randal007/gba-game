# GBA Isometric Game — Status

## Current State (2026-02-16)

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
