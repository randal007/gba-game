# GBA Game Assets

Created by Pixel 🎨 — FFTA-style sprite art for isometric action game.

---

## sprites/hero_walk.png

**Layout:** 192×128px sprite sheet  
**Frame size:** 32×32px  
**Frames:** 6 per direction  
**Directions:** 4 rows (SW, SE, NW, NE)

```
Row 0: SW-facing  (6 frames) ← player walks toward camera-left
Row 1: SE-facing  (6 frames) ← H-flip of SW
Row 2: NW-facing  (6 frames) ← player walks away camera-left
Row 3: NE-facing  (6 frames) ← H-flip of NW
```

**grit conversion:**
```bash
grit assets/sprites/hero_walk.png -gB4 -Mw4 -Mh4 -pn16 -o assets/sprites/hero_walk
```

**Palette (11 colors + transparent):**
- `#1a0c00` — dark brown outline
- `#dcb478` — fur light
- `#b48c50` — fur mid  
- `#785a32` — fur dark/boots
- `#78a0dc` — armor light blue
- `#506eb4` — armor mid blue
- `#283c82` — armor dark blue
- `#ffdc64` — gold trim light
- `#c8a03c` — gold trim mid
- `#f0f0dc` — sword/claw white
- `#3cc8ff` — cyan eyes

---

## tiles/floor_iso.png

**Layout:** 64×8px tileset  
**Tile size:** 16×8px diamond (isometric 2:1 ratio)  
**Sub-tiles:** Each 16×8 = two 8×8 halves for GBA BG hardware

```
Tile 0 (x=0):  Grass  — green, top-lit
Tile 1 (x=16): Stone  — grey, flat
Tile 2 (x=32): Dirt   — brown/tan
Tile 3 (x=48): Water  — deep blue, shaded
```

**grit conversion:**
```bash
grit assets/tiles/floor_iso.png -gB4 -Mw2 -Mh1 -mRtpf -o assets/tiles/floor_iso
```

**Usage in code:**
```c
// Isometric grid: tile at world (tx, ty) draws to screen:
// screen_x = (tx - ty) * 16 - cam_x
// screen_y = (tx + ty) * 8  - cam_y
// Use sub-tile index pairs from floor_iso tileset
```

---

## Palette Notes

- All sprites use **4bpp** (16 colors per bank, 15 + transparent)
- Color 0 = transparent in every palette
- Hero uses palette bank 0
- Floor tiles use BG palette bank 0
- GBA color format: BGR555 (multiply RGB values by ~8 to convert from 0-31 range)

---

*Art by Pixel | FFTA-inspired chibi style | GBA constraints respected*
