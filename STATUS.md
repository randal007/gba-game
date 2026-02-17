# GBA Isometric Game — Status

## Current State (2026-02-16)

### ✅ Mode 0 Tiled Renderer (MAJOR PERFORMANCE FIX)

Switched from Mode 4 bitmap software rendering to Mode 0 hardware tiled background:

- **Before:** CPU rendered every iso cube pixel every frame → ~15fps, unplayable
- **After:** Pre-renders world once at boot into 8×8 tiles, hardware BG handles display + scrolling → solid 60fps

#### How it works:
1. At startup, all iso cubes are rendered into an EWRAM pixel buffer (512×320)
2. The buffer is converted to deduplicated 8bpp 8×8 tiles + a 64×64 tilemap
3. Tiles and tilemap are uploaded to VRAM once
4. Main loop only updates BG scroll registers (REG_BG0HOFS/VOFS) + OBJ sprite
5. Zero per-frame rendering cost for the map

#### Technical details:
- Mode 0, BG0: 8bpp, 64×64 tilemap (BG_SIZE3 = 512×512px), charblock 0, screenblock 28
- OBJ sprite hero: 4bpp, charblock 4, unchanged from before
- Player speed: 2 px/frame (was 12 to compensate for low framerate)
- Anim speed: 4 ticks (was 2, adjusted for 60fps)

### Features
- 16×16 isometric world map with 4 tile types (grass, stone, dirt, water)
- Isometric cube rendering with diamond top + side faces
- 4-direction hero sprite with walk animation
- Smooth camera follow (lerp)
- Hardware scrolling via BG scroll registers

### Build
```
make        # builds isogame.gba
make run    # launches in mgba-qt
make clean  # clean build artifacts
```
