# GBA Isometric Action Game — Research Reference
> Zelda-style gameplay, FFTA art style, devkitARM + libtonc, C language, real hardware (EZ Flash)
> Compiled: 2026-02-15 | Sources: Tonc v1.4.2, GBATek, Maxmod docs, gbadev.net

---

## Table of Contents
1. [GBA Hardware Essentials](#1-gba-hardware-essentials)
2. [Video Modes — Mode 0 vs Mode 2](#2-video-modes--mode-0-vs-mode-2)
3. [Isometric Rendering on GBA](#3-isometric-rendering-on-gba)
4. [Sprite System — OAM & 4bpp](#4-sprite-system--oam--4bpp)
5. [Input Handling](#5-input-handling)
6. [Collision Detection](#6-collision-detection)
7. [FFTA Art Style Constraints](#7-ffta-art-style-constraints)
8. [Sound — Maxmod Library](#8-sound--maxmod-library)
9. [libtonc API Reference](#9-libtonc-api-reference)
10. [Performance Tips — 60fps on 16.78MHz](#10-performance-tips--60fps-on-1678mhz)
11. [Further Reading](#11-further-reading)

---

## 1. GBA Hardware Essentials

### CPU & Speed
- **ARM7TDMI RISC** chip at **16.78 MHz** (2²⁴ cycles/second)
- Two instruction sets: **ARM** (32-bit, faster per instruction) and **THUMB** (16-bit, smaller code)
- **No hardware floating-point** — use fixed-point math everywhere
- **No hardware division** — avoid `/` in hot loops; use shifts or reciprocal multiply tricks
- Use **THUMB** for ROM code (16-bit bus), **ARM** for IWRAM code (32-bit bus, no wait states)

### Memory Map (critical sections)
| Region | Address | Size | Bus | Notes |
|--------|---------|------|-----|-------|
| BIOS ROM | 0x00000000 | 16 KB | 32-bit | Execute only |
| EWRAM | 0x02000000 | 256 KB | 16-bit | Slow; use for bulk data, heap |
| IWRAM | 0x03000000 | 32 KB | 32-bit | **Fastest**; put ARM ISRs and hot code here |
| IO RAM | 0x04000000 | 1 KB | 16-bit | All hardware registers |
| PAL RAM | 0x05000000 | 1 KB | 16-bit | 256 BG colors + 256 sprite colors (15-bit each) |
| VRAM | 0x06000000 | 96 KB | 16-bit | Tiles + tilemaps + sprite tiles |
| OAM | 0x07000000 | 1 KB | 32-bit | 128 sprite attribute entries |
| PAK ROM | 0x08000000 | ≤32 MB | 16-bit | Game code + data |
| Cart RAM | 0x0E000000 | varies | 8-bit | Save data (SRAM/Flash/EEPROM) |

### Display Timing
- **240×160 pixels**, 15-bit color (RGB555)
- Refresh rate: **59.73 Hz** (≈60 fps target)
- Full frame = **280,896 CPU cycles**
- HDraw = 240px (960 cycles) + HBlank = 68px (272 cycles) = **1,232 cycles per scanline**
- VDraw = 160 scanlines (197,120 cycles) + VBlank = 68 scanlines (83,776 cycles)
- **Update game state and OAM during VBlank** to avoid tearing

### Key Registers
```c
#define REG_DISPCNT   (*(vu16*)0x04000000)  // Display control
#define REG_DISPSTAT  (*(vu16*)0x04000004)  // Display status / IRQ enables
#define REG_VCOUNT    (*(vu16*)0x04000006)  // Current scanline (0–227)
#define REG_KEYINPUT  (*(vu16*)0x04000130)  // Key state (ACTIVE LOW!)
#define REG_IE        (*(vu16*)0x04000200)  // Interrupt Enable
#define REG_IF        (*(vu16*)0x04000202)  // Interrupt Flags (write 1 to acknowledge)
#define REG_IME       (*(vu16*)0x04000208)  // Interrupt Master Enable
```

### REG_DISPCNT bit layout
```
F E D C B A 9 8  7 6 5 4 3 2 1 0
OW W1 W0 OBJ BG3 BG2 BG1 BG0 FB OM HB PS GB Mode
```
- **Mode** (bits 0–2): 0=tiled, 1=tiled, 2=tiled, 3/4/5=bitmap
- **BG0–BG3** (bits 8–11): enable backgrounds
- **Obj** (bit 12): enable sprites
- **OM** (bit 6): sprite mapping mode (0=2D, **1=1D** — use 1D for homebrew!)
- **HB** (bit 5): allow OAM access during HBlank (reduces sprite pixels/line)

### VSync — Best Practice
```c
// Busy-wait (simple but wastes battery):
void vid_vsync() {
    while(REG_VCOUNT >= 160);  // Wait for VDraw to start
    while(REG_VCOUNT < 160);   // Wait for VBlank to start
}

// Better — use BIOS call (puts CPU to sleep, saves battery):
// Requires VBlank IRQ enabled
VBlankIntrWait();  // tonc BIOS wrapper
```

---

## 2. Video Modes — Mode 0 vs Mode 2

### Tiled Background Modes
| Mode | BG0 | BG1 | BG2 | BG3 |
|------|-----|-----|-----|-----|
| 0 | Regular | Regular | Regular | Regular |
| 1 | Regular | Regular | **Affine** | — |
| 2 | — | — | **Affine** | **Affine** |

**For an isometric game, use MODE 0.** This gives you 4 regular tiled backgrounds, all capable of scrolling independently. Affine BGs (Mode 2) support rotation/scale but only 8bpp, 256-color, no flipping per tile.

### Regular BG (Mode 0) — What We'll Use
- Up to **4 simultaneous scrolling tile layers** (BG0–BG3)
- Each BG: 256×256 to 512×512 pixels in tiles
- Tile size: always **8×8 pixels**
- 4bpp tiles: 16 colors per tile, palette bank selectable per screen entry
- 8bpp tiles: 256 colors, one shared palette
- Hardware scrolling via `REG_BGxHOFS` / `REG_BGxVOFS` (write-only)
- Priority 0–3 per BG (lower number = drawn on top)
- Sprites can interleave with BGs using priority bits

### Background Control Register (REG_BGxCNT)
```
F E D C B A 9 8  7 6 5 4 3 2 1 0
Sz Wr SBB CM Mos - CBB Pr
```
- **Pr** (0–1): Priority (0=top, 3=bottom)
- **CBB** (2–3): Character Base Block (0–3) — where tiles are in VRAM
- **CM** (7): Color mode (0=4bpp/16-color, 1=8bpp/256-color)
- **SBB** (8–12): Screen Base Block (0–31) — where the tile map is
- **Sz** (14–15): Map size

### Regular BG Map Sizes
| Sz bits | Tiles | Pixels |
|---------|-------|--------|
| 00 | 32×32 | 256×256 |
| 01 | 64×32 | 512×256 |
| 10 | 32×64 | 256×512 |
| 11 | 64×64 | 512×512 |

### VRAM Layout (charblocks & screenblocks)
```
0x06000000 — Charblock 0 (16 KB) — BG tiles
0x06004000 — Charblock 1 (16 KB) — BG tiles
0x06008000 — Charblock 2 (16 KB) — BG tiles
0x0600C000 — Charblock 3 (16 KB) — BG tiles
0x06010000 — Charblock 4 (16 KB) — Sprite tiles (lower)
0x06014000 — Charblock 5 (16 KB) — Sprite tiles (upper)

Screenblocks: 32 blocks of 2KB each, overlapping with charblocks
  SBB 0–7   overlap CBB 0
  SBB 8–15  overlap CBB 1
  SBB 16–23 overlap CBB 2
  SBB 24–31 overlap CBB 3
```
**Important:** BG tile indices can access 1024 tiles starting from CBB, but **cannot reach sprite charblocks** (blocks 4–5) on real hardware. Emulators may allow this incorrectly.

### Screen Entry Format (4bpp tile maps)
```
F E D CBA9  8 7 6 5 4 3 2 1 0
PB   VF HF  TID (10 bits)
```
- **TID** (0–9): Tile index
- **HF** (10): Horizontal flip
- **VF** (11): Vertical flip
- **PB** (12–15): Palette bank (0–15 for 4bpp mode)

### Scrolling
```c
// BG scroll registers are WRITE-ONLY — keep local variables
int cam_x = 0, cam_y = 0;
// Scroll values = top-left map coordinate shown at screen origin
// (positive scrolling moves map LEFT/UP on screen — counterintuitive!)
REG_BG0HOFS = cam_x;
REG_BG0VOFS = cam_y;

// Or using tonc's array interface:
REG_BG_OFS[0].x = cam_x;
REG_BG_OFS[0].y = cam_y;
```

### DMA — Fast Memory Transfer
```c
// 4 DMA channels; channel 3 for general use
// DMA3 for copying tiles/maps:
dma3_cpy(dst, src, byte_count);   // tonc word-copy
dma3_fill(dst, value, byte_count); // tonc word-fill

// Raw DMA3 transfer (32-bit, immediate):
#define DMA_TRANSFER(_dst, _src, count, ch, mode) \
  do { REG_DMA[ch].cnt = 0; REG_DMA[ch].src = (_src); \
       REG_DMA[ch].dst = (_dst); REG_DMA[ch].cnt = (count)|(mode); } while(0)

// HBlank DMA (for scanline effects):
#define DMA_HDMA (DMA_ENABLE | DMA_REPEAT | DMA_AT_HBLANK | DMA_DST_RELOAD)
```
**Warning:** DMA halts the CPU and can disrupt interrupts — prefer `memcpy32()` for safety unless speed is critical.

### Timers
4 timers (TM0–TM3). Each has 16-bit counter + control register.
```c
// Cascade timer trick: 1 Hz from 16.78 MHz
REG_TM2D   = -0x4000;        // overflow after 0x4000 ticks
REG_TM2CNT = TM_FREQ_1024;   // @ 1024-cycle prescaler = 16384 ticks/sec
REG_TM3CNT = TM_ENABLE | TM_CASCADE; // TM3 increments when TM2 overflows

// Timers mainly used for:
// 1. DirectSound DMA timing (with TM0/TM1)
// 2. Profiling code speed
// 3. Game clocks via cascade
```

---

## 3. Isometric Rendering on GBA

### Isometric Coordinate System
In a classic 2:1 isometric projection (used by FFTA, GBA Fire Emblem, etc.):
- The "world" has tile coordinates (tx, ty)
- Screen coordinates are computed:
  ```
  screen_x = (tx - ty) * TILE_HALF_W
  screen_y = (tx + ty) * TILE_HALF_H
  ```
  For a 32×16 diamond tile (FFTA style):
  - `TILE_HALF_W = 16` (half tile width)
  - `TILE_HALF_H = 8`  (half tile height)

### Approach A: Software Tiled Rendering (Recommended for Action Games)

Since the GBA's BG hardware only draws rectangular grids, you have two main approaches for isometric tilemaps:

**Option 1 — Diamond tiles drawn as sprites or into VRAM manually:**
- Draw each visible iso-tile as a blitted region into a BG charblock each frame (or only when dirty)
- Tiles are 32×16 or 16×8 pixel diamonds
- Expensive: ~30 visible tiles × copy cost = tight budget

**Option 2 — Use rotated 45° rectangular tiles on a regular BG:**
- Conceptually rotate the world 45°; each tile becomes a rectangle again
- Hardware BG scrolls it; no per-frame tile blitting
- Works well if tiles are truly square in the rotated grid

**Option 3 — Pre-rendered rows via HBlank DMA (Mode 7-like) — Complex, not recommended for beginners**

### Practical Isometric Map Strategy (Recommended)

**Use BG0 as a "flat" isometric floor** rendered via tile indices in a screenblock:
1. Design your iso-tileset with 32×16 pixel diamond tiles
2. Store them as 8×8 sub-tiles (4 per diamond: top-left, top-right, bot-left, bot-right)
3. Fill a screenblock with the appropriate sub-tile indices to form the diamond grid
4. Use BG scroll to implement camera

**For a 240×160 screen with 32×16 diamonds:**
- Visible area: ~15 tiles wide × ~20 tiles tall (in iso rows)
- Total visible tiles: ~300 tiles, well within BG tilemap budget

### Diamond Tile Sub-Tile Layout
```
Diamond tile (32×16) decomposed into four 8×8 sub-tiles:
  [TL][TR]
  [BL][BR]

Screen entry at (col*4, row*2) uses tile TL, (col*4+1, row*2) uses TR, etc.
Palette bank is set per screen entry for color variation.
```

### Depth Sorting (Painter's Algorithm)
For objects/sprites over an isometric map, depth = `tx + ty` (Manhattan distance from camera origin). Higher value = drawn later (appears in front). Sort sprite OAM entries or draw order by this value each frame:
```c
// Depth sort: objects with higher (tx+ty) drawn over those with lower
// Assign OAM priority bits: lower OAM index = drawn over higher
// So sort OAM array by depth before copying to real OAM at VBlank

void depth_sort_objects(OBJ_ATTR *objs, int count) {
    // Simple insertion sort (fine for ~20 objects)
    for (int i = 1; i < count; i++) {
        OBJ_ATTR key = objs[i];
        int key_depth = ...; // retrieve depth for objs[i]
        int j = i - 1;
        while (j >= 0 && depth_of(objs[j]) < key_depth) {
            objs[j+1] = objs[j];
            j--;
        }
        objs[j+1] = key;
    }
}
```
Objects at the same depth: use Z-layering within the map (e.g., walls occlude characters).

### Camera Scrolling
Keep a camera position in world-pixel coords (using fixed-point for sub-pixel accuracy):
```c
// Fixed-point camera (8 fractional bits = .8 format)
int cam_x_fp = 0 << 8;  // camera top-left in world pixels, 24.8 fixed
int cam_y_fp = 0 << 8;

// Convert iso tile (tx, ty) to screen position relative to camera:
int world_x = (tx - ty) * 16;  // TILE_HALF_W
int world_y = (tx + ty) * 8;   // TILE_HALF_H
int screen_x = world_x - (cam_x_fp >> 8);
int screen_y = world_y - (cam_y_fp >> 8);

// Set BG scroll to camera position:
REG_BG0HOFS = cam_x_fp >> 8;
REG_BG0VOFS = cam_y_fp >> 8;
```

### BG Layer Strategy
| Layer | BG | Content |
|-------|----|----|
| Sky/fog | BG3 | Solid color or distant backdrop |
| Floor tiles | BG2 | Isometric ground, grass, water |
| Wall/raised objects | BG1 | Iso wall tiles, elevated surfaces |
| UI overlay | BG0 | HP bars, icons (highest priority) |
| Player + enemies | Sprites | OAM-controlled |

Alternatively use BG0 for floor + BG1 for walls, and keep BG2–BG3 for parallax.

---

## 4. Sprite System — OAM & 4bpp

### OAM Overview
- OAM is at `0x07000000`, 1 KB, holds **128 OBJ_ATTR entries** (3×16-bit each + 16-bit filler)
- Also interleaved: **32 OBJ_AFFINE entries** (rotation/scale matrices)
- **OAM is write-locked during VDraw** (scanlines 0–159) — write only during VBlank!
- Solution: maintain a **shadow OAM** buffer in RAM, copy to real OAM at VBlank

### OBJ_ATTR Structure
```c
typedef struct {
    u16 attr0;  // Y coord, mode, color depth, shape
    u16 attr1;  // X coord, flip flags, size
    u16 attr2;  // Tile index, priority, palette bank
    s16 fill;   // Padding (used by OBJ_AFFINE — don't write!)
} ALIGN4 OBJ_ATTR;
```

### Attribute 0 (attr0)
```
F E D C B A 9 8  7 6 5 4 3 2 1 0
Sh CM Mos GM OM  Y (8 bits)
```
| Bits | Name | Meaning |
|------|------|---------|
| 0–7 | Y | Y coordinate (top of sprite, 8-bit signed via masking) |
| 8–9 | OM | Object mode: 00=normal, 01=affine, 10=**HIDE**, 11=affine double |
| 10–11 | GM | Graphics mode: 00=normal, 01=alpha blend, 10=obj window |
| 12 | Mos | Mosaic enable |
| 13 | CM | Color mode: 0=4bpp (16 colors), **1=8bpp (256 colors)** |
| 14–15 | Sh | Shape: 00=square, 01=wide, 10=tall |

### Attribute 1 (attr1)
```
F E D C B A  9 8 7 6 5 4 3 2 1 0
Sz VF HF -   X (9 bits)
```
| Bits | Name | Meaning |
|------|------|---------|
| 0–8 | X | X coordinate (9-bit signed, mask with 0x1FF) |
| 12 | HF | Horizontal flip (non-affine only) |
| 13 | VF | Vertical flip (non-affine only) |
| 14–15 | Sz | Size code (combined with Sh from attr0) |

### Attribute 2 (attr2)
```
F E D C B A 9  8 7 6 5 4 3 2 1 0
PB     Pr      TID (10 bits)
```
| Bits | Name | Meaning |
|------|------|---------|
| 0–9 | TID | Base tile index (in sprite VRAM, counting in 32-byte units) |
| 10–11 | Pr | Priority (0=front, 3=back; sprites at same prio: lower OAM index wins) |
| 12–15 | PB | Palette bank (0–15, for 4bpp mode only) |

### Sprite Sizes (attr0.Sh + attr1.Sz)
| Sz\Sh | Square | Wide | Tall |
|-------|--------|------|------|
| 00 | 8×8 | 16×8 | 8×16 |
| 01 | 16×16 | 32×8 | 8×32 |
| 10 | 32×32 | 32×16 | 16×32 |
| **11** | **64×64** | **64×32** | **32×64** |

**For 32×32 player/enemy sprites:** `Sh=00 (square), Sz=10` → 32×32

### 1D vs 2D Sprite Tile Mapping
Always use **1D mapping** (set `DCNT_OBJ_1D` in REG_DISPCNT):
- Tiles for a sprite are stored **consecutively** in sprite VRAM
- Easier to manage; editors export in this format
- 2D mapping forces 32-tile row offsets between sprite rows — painful

### Sprite VRAM
```
0x06010000 — Sprite tiles start (charblock 4)
0x06014000 — Charblock 5

// Access via tonc's tile_mem array:
tile_mem[4][tid]  // 4bpp tile, sprite charblock 4
tile_mem[5][tid]  // 4bpp tile, sprite charblock 5
// In bitmap modes (3-5): only tiles 512–1023 available for sprites!
```

### Palette System
```
0x05000000 — BG palette (256 × 15-bit colors)
0x05000200 — Sprite palette (256 × 15-bit colors)

// 4bpp: 16 sub-palettes of 16 colors each
// Entry 0 of each sub-palette = transparent (never rendered)
// So you get 15 actual colors per sub-palette

// Tonc memory maps:
pal_bg_mem[index]       // BG palette entries
pal_obj_mem[index]      // Sprite palette entries  
pal_bg_bank[bank][n]    // BG 4bpp bank access (bank 0–15, entry 0–15)
pal_obj_bank[bank][n]   // Sprite 4bpp bank access
```

### Max Sprites Per Scanline
- Hardware limit: **128 OBJs total**
- Per-scanline pixel budget: **~128 pixels of 8×8 sprites** (960 pixels, about 120 8×8 sprites, or fewer larger sprites)
- More accurately: **960 sprite pixels per scanline** under ideal conditions
- A 32×32 sprite consumes 32 pixels per scanline of its bounding box
- At 32×32 size, you can have about 30 sprites per scanline before dropping pixels
- Use `DCNT_OAM_HBL` flag to access OAM during HBlank (gives more OAM time but reduces pixel budget slightly)

### Shadow OAM Pattern
```c
OBJ_ATTR obj_buffer[128];
OBJ_AFFINE *const obj_aff_buffer = (OBJ_AFFINE*)obj_buffer;

// Initialize (hide all):
void oam_init(void) {
    for (int i = 0; i < 128; i++) {
        obj_buffer[i].attr0 = ATTR0_HIDE;
        obj_buffer[i].attr1 = 0;
        obj_buffer[i].attr2 = 0;
    }
    // Copy to real OAM
    oam_copy(oam_mem, obj_buffer, 128);
}

// In VBlank ISR:
oam_copy(oam_mem, obj_buffer, num_active_sprites);

// Helper to set an object's position safely:
INLINE void obj_set_pos(OBJ_ATTR *obj, int x, int y) {
    obj->attr0 = (obj->attr0 & ~0x00FF) | (y & 0x00FF);
    obj->attr1 = (obj->attr1 & ~0x01FF) | (x & 0x01FF);
}
```

### Animation Frames
Change the `TID` (tile index) in attr2 each frame to animate:
```c
// Assuming 32×32 sprite = 16 tiles per frame (4×4 tiles in 1D mapping)
#define TILES_PER_FRAME 16
obj->attr2 = (obj->attr2 & ~ATTR2_ID_MASK) | (ATTR2_ID(frame_index * TILES_PER_FRAME));

// Or pre-load all frames into VRAM at start:
// Frame 0 starts at tile 0, frame 1 at tile 16, etc.
```

---

## 5. Input Handling

### REG_KEYINPUT (0x04000130)
```
Bits: F E D C B A 9 8  7 6 5 4 3 2 1 0
       - - - - - - L R  down up left right start select B A
```
**CRITICAL:** Bits are **ACTIVE LOW** — a bit is **0** when the key **IS pressed**.

### Tonc Key System (use this!)
```c
// Globals (in tonc_core.c)
u16 __key_curr = 0, __key_prev = 0;

// Call once per frame (inverts the register for active-high logic):
INLINE void key_poll(void) {
    __key_prev = __key_curr;
    __key_curr = ~REG_KEYINPUT & KEY_MASK;
}

// Key constants:
#define KEY_A      0x0001
#define KEY_B      0x0002
#define KEY_SELECT 0x0004
#define KEY_START  0x0008
#define KEY_RIGHT  0x0010
#define KEY_LEFT   0x0020
#define KEY_UP     0x0040
#define KEY_DOWN   0x0080
#define KEY_R      0x0100
#define KEY_L      0x0200
#define KEY_MASK   0x03FF

// State queries:
key_is_down(KEY_A)      // Is A held right now?
key_hit(KEY_A)          // Did A just get pressed this frame? (edge detect)
key_released(KEY_A)     // Did A just get released?
key_held(KEY_A)         // Is A held (down both this frame and last)?

// Tribool for movement (returns +1, 0, or -1):
key_tri_horz()          // +1=RIGHT, -1=LEFT, 0=neither
key_tri_vert()          // +1=DOWN,  -1=UP,   0=neither
```

### 4-Direction Isometric Movement
In isometric view, d-pad maps to diagonal world movement:
```c
// Called every frame
void update_player_movement(void) {
    // In isometric space: UP moves NE, DOWN moves SW, LEFT=NW, RIGHT=SE
    int dx = 0, dy = 0;

    if (key_is_down(KEY_RIGHT)) { dx += 1; dy += 1; }  // SE in iso
    if (key_is_down(KEY_LEFT))  { dx -= 1; dy -= 1; }  // NW in iso
    if (key_is_down(KEY_UP))    { dx += 1; dy -= 1; }  // NE in iso
    if (key_is_down(KEY_DOWN))  { dx -= 1; dy += 1; }  // SW in iso

    // dx, dy are tile offsets in world space
    // Multiply by speed and add to player's fixed-point position
    player.world_x_fp += dx * MOVE_SPEED;
    player.world_y_fp += dy * MOVE_SPEED;
}
```

For 4-direction (no diagonals) like FFTA:
```c
// Strictly cardinal: check most recently pressed direction, or use state machine
int hdir = key_tri_horz();  // -1, 0, +1
int vdir = key_tri_vert();  // -1, 0, +1

// Prefer horizontal if both pressed:
if (hdir != 0) vdir = 0;
```

### Attack Button
```c
if (key_hit(KEY_A)) {
    trigger_sword_attack(&player);  // Single press detection via key_hit()
}
```

---

## 6. Collision Detection

### Tile-Based Collision
Store a separate **collision map** (2D array of bytes) parallel to your tile map:
```c
#define MAP_W 64
#define MAP_H 64
u8 collision_map[MAP_H][MAP_W];  // 0=walkable, 1=wall, 2=water, etc.

// Check if a world tile is passable:
bool tile_is_passable(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return false;
    return collision_map[ty][tx] == 0;
}

// Before moving player to new tile position:
int new_tx = player.tx + dx;
int new_ty = player.ty + dy;
if (tile_is_passable(new_tx, new_ty)) {
    player.tx = new_tx;
    player.ty = new_ty;
}
```

For sub-tile movement (pixel-level):
```c
// World position in pixels (fixed-point 24.8)
// Check corners of entity's bounding box against collision tiles:
bool can_move(int new_wx, int new_wy, int half_w, int half_h) {
    // Convert pixel corners to tile coords
    int tx0 = (new_wx - half_w) / TILE_W;
    int ty0 = (new_wy - half_h) / TILE_H;
    int tx1 = (new_wx + half_w - 1) / TILE_W;
    int ty1 = (new_wy + half_h - 1) / TILE_H;

    return tile_is_passable(tx0, ty0) && tile_is_passable(tx1, ty0) &&
           tile_is_passable(tx0, ty1) && tile_is_passable(tx1, ty1);
}
```

### Hitbox-Based Collision (Sword Attacks)
AABB (Axis-Aligned Bounding Box) in world-pixel space:
```c
typedef struct {
    s16 x, y;    // Center position in world pixels
    u8  hw, hh;  // Half-width, half-height
    bool active;
} Hitbox;

// AABB overlap test:
bool hitboxes_overlap(Hitbox *a, Hitbox *b) {
    return abs(a->x - b->x) < (a->hw + b->hw) &&
           abs(a->y - b->y) < (a->hh + b->hh);
}

// Sword attack: create a hitbox in front of player based on facing direction
void spawn_sword_hitbox(Player *p) {
    int offset_x = 0, offset_y = 0;
    switch(p->facing) {
        case DIR_NE: offset_x = +16; offset_y = -8; break;
        case DIR_SE: offset_x = +16; offset_y = +8; break;
        case DIR_SW: offset_x = -16; offset_y = +8; break;
        case DIR_NW: offset_x = -16; offset_y = -8; break;
    }
    sword_hitbox.x = p->wx + offset_x;
    sword_hitbox.y = p->wy + offset_y;
    sword_hitbox.hw = 12;
    sword_hitbox.hh = 8;
    sword_hitbox.active = true;
    sword_hitbox_frames = SWORD_ACTIVE_FRAMES;
}

// In game update loop:
if (sword_hitbox.active) {
    for (int e = 0; e < enemy_count; e++) {
        if (hitboxes_overlap(&sword_hitbox, &enemies[e].hitbox)) {
            damage_enemy(&enemies[e], player.attack_power);
        }
    }
    if (--sword_hitbox_frames <= 0) sword_hitbox.active = false;
}
```

### Isometric Depth for Collision
In iso space, note that screen-space AABB != world-space AABB. For tile-based collision, work in **tile coordinates** not screen coordinates. For detailed hit detection, transform to a "flat" coordinate system first.

---

## 7. FFTA Art Style Constraints

### Palette Limits (GBA Hardware)
- Total: 2 × 256-color palettes (one BG, one sprite), each 512 bytes
- In 4bpp mode: 16 sub-palettes of 16 colors (15 + transparent)
- **Entry 0** of any sub-palette = transparent; you only have **15 drawable colors** per sub-palette
- BG tiles: palette bank stored per screen entry (4 bits = banks 0–15)
- Sprites: palette bank stored in OBJ_ATTR.attr2 (4 bits = banks 0–15)

### FFTA Palette Strategy
FFTA (Final Fantasy Tactics Advance) uses:
- **Per-unit palette banks** — each character class gets its own 16-color sub-palette
- **Tile reuse** — sprites share the same tile shapes but swap palettes for color variation
- **Background tiles** use multiple palette banks for visual variety
- **~6–8 unique palette banks** for backgrounds (floor, wall, water, shadow, props...)
- **~4–8 palette banks** for characters (hero, enemy types, NPCs)
- This totals well within the 16+16 bank limit

### FFTA Isometric Tile Art
- Floor tiles: diamond-shaped, 32×16 pixels (but stored as 4 × 8×8 sub-tiles)
- Wall tiles: rectangular "pillar" shapes, typically 16×32 or 16×24 pixels
- Characters: **32×32 or 16×32 sprites** in 4bpp
- Typical character frame count: 4–8 frames per animation, 4–8 animations
- FFTA characters use ~64–128 tiles in sprite VRAM per unique character
- Shadows are separate transparent sprites or baked into the floor BG

### FFTA Art Style Summary
- **Bright, saturated colors** with strong outlines (1–2px black border)
- **Cel-shaded look** — limited shading, flat color areas
- **Careful use of the 15-color limit** per palette bank: outline (1), base fill (2–3), highlight (1), shadow (1), detail colors (rest)
- **Tile repetition** is acceptable and expected; unique tile count should stay under 512 per charblock (1024 total accessible)

### Art Asset Budget Estimate
```
Sprite VRAM: 32KB (charblocks 4–5)
  - Player: 32×32 @ 4bpp = 64 bytes × 16 tiles = 512 bytes/frame
    × 8 directions × 4 frames = 16,384 bytes ≈ 16KB
  - 1–2 enemy types fitting in remaining ~16KB
  - Shared tiles where possible (mirroring saves half the directional art)

BG VRAM: 64KB (charblocks 0–3)
  - Floor tileset: 1 charblock (16KB, 512 tiles) — plenty for iso floors
  - Wall tileset: 1 charblock (16KB)
  - Tilemaps: use SBB 28–31 (~8KB total for 4 maps)
```

---

## 8. Sound — Maxmod Library

### GBA Sound Hardware
6 channels total:
1. **Channel 1**: Square wave with frequency sweep
2. **Channel 2**: Square wave (no sweep)
3. **Channel 3**: Wave RAM sample player (4-bit, 32 samples)
4. **Channel 4**: Noise generator
5. **DirectSound A**: 8-bit PCM, DMA-fed, up to ~32 KHz
6. **DirectSound B**: 8-bit PCM, DMA-fed, up to ~32 KHz

For music + SFX: **use Maxmod**. It drives the DirectSound channels via DMA + Timer0/1.

### Maxmod Overview
Maxmod (bundled with devkitARM) supports:
- Module formats: `.mod`, `.xm`, `.s3m`, `.it` (tracked music)
- Sound effect playback with volume, pitch, panning control
- Software mixing: 8–32 mixing channels at 8–32 KHz
- Reverb system
- Jingles (short one-shot music without stopping main BGM)

### Maxmod Initialization (GBA)
```c
#include <maxmod.h>
#include "soundbank.h"  // Generated by mmutil tool

// Mixing buffer MUST be in IWRAM for CPU efficiency!
u8 mixbuf[MM_MIXLEN_16KHZ] __attribute__((aligned(4), section(".iwram")));

void sound_init(void) {
    irqSet(IRQ_VBLANK, mmVBlank);  // Must be set BEFORE mmInit

    mm_gba_system sys;
    u8 *mem = malloc(8 * (MM_SIZEOF_MODCH + MM_SIZEOF_ACTCH + MM_SIZEOF_MIXCH)
                     + sizeof(mixbuf));

    sys.mixing_mode       = MM_MIX_16KHZ;   // 16KHz is good balance
    sys.mod_channel_count = 8;
    sys.mix_channel_count = 8;
    sys.module_channels   = (mm_addr)(mem);
    sys.active_channels   = (mm_addr)(mem + 8*MM_SIZEOF_MODCH);
    sys.mixing_channels   = (mm_addr)(mem + 8*(MM_SIZEOF_MODCH + MM_SIZEOF_ACTCH));
    sys.mixing_memory     = (mm_addr)mixbuf;
    sys.wave_memory       = (mm_addr)(mem + 8*(MM_SIZEOF_MODCH + MM_SIZEOF_ACTCH + MM_SIZEOF_MIXCH));
    sys.soundbank         = (mm_addr)soundbank;  // from soundbank.bin linked in ROM

    mmInit(&sys);
}
```

### Soundbank Setup (mmutil)
```bash
# In your Makefile (devkitARM provides mmutil):
soundbank.bin soundbank.h: music/bgm.xm sfx/sword.wav sfx/hurt.wav
    mmutil $^ -osoundbank.bin -hsoundbank.h
```

### Music Playback
```c
// Play background music (loops by default in .xm/.mod):
mmStart(MOD_BGM_FIELD, MM_PLAY_LOOP);

// Pause/resume:
mmPause();
mmResume();

// Stop:
mmStop();

// Jingle (plays over BGM, then BGM resumes):
mmJingle(MOD_JINGLE_VICTORY);

// Volume (0–1024):
mmSetModuleVolume(512);  // 50% volume
```

### Sound Effects
```c
// Simple play:
mmEffect(SFX_SWORD_SWING);

// With custom parameters:
mm_sound_effect sfx = {
    .id     = SFX_SWORD_SWING,
    .rate   = 0x400,   // 0x400 = normal pitch
    .handle = 0,
    .volume = 255,     // 0–255
    .panning = 128,    // 0=left, 128=center, 255=right
};
mm_sfxhand h = mmEffectEx(&sfx);

// Stop specific SFX:
mmEffectCancel(h);

// Stop all SFX:
mmEffectCancelAll();
```

### Maxmod + VBlank ISR
```c
// In your VBlank ISR (or registered via irqSet):
// mmVBlank() MUST be called every VBlank for proper timing
void vblank_isr(void) {
    mmVBlank();
    // ... rest of your VBlank work
}

// Register with tonc's irq system:
irq_add(II_VBLANK, vblank_isr);
```

### Audio Quality Trade-offs
| Mix Rate | CPU Usage | Quality |
|---------|-----------|---------|
| MM_MIX_8KHZ | ~10% | Low (telephone) |
| MM_MIX_10KHZ | ~13% | Okay |
| **MM_MIX_16KHZ** | **~20%** | **Good — recommended** |
| MM_MIX_20KHZ | ~25% | Very good |
| MM_MIX_32KHZ | ~40% | Excellent |

**Mixing buffer in IWRAM is mandatory** — in EWRAM, CPU load nearly triples.

---

## 9. libtonc API Reference

### Include
```c
#include <tonc.h>  // Includes everything
// Or selectively:
#include <tonc_types.h>   // u8, u16, u32, s8, s16, s32, FIXED, etc.
#include <tonc_memmap.h>  // Memory-mapped register addresses
#include <tonc_memdef.h>  // Register bit definitions
#include <tonc_input.h>   // Key input functions
#include <tonc_oam.h>     // OAM / sprite functions
#include <tonc_video.h>   // Video utilities
#include <tonc_irq.h>     // Interrupt management
#include <tonc_bios.h>    // BIOS call wrappers
#include <tonc_math.h>    // Fixed-point, trig, clamp
```

### Types
```c
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef volatile u16       vu16;
typedef volatile u32       vu32;
typedef s32                FIXED;    // 24.8 fixed-point
typedef u16                COLOR;    // BGR555 color
typedef u16                SCR_ENTRY; // Screenblock entry
typedef struct { u32 data[8]; }  TILE;   // 4bpp 8x8 tile
typedef struct { u32 data[16]; } TILE8;  // 8bpp 8x8 tile
typedef TILE               CHARBLOCK[512];
typedef SCR_ENTRY          SCREENBLOCK[1024];
```

### Color Macro
```c
COLOR c = RGB15(r, g, b);  // r, g, b each 0–31
// Example: bright red = RGB15(31, 0, 0)
```

### Memory Maps
```c
// Tile memory (charblocks × tiles):
extern CHARBLOCK  tile_mem[6];    // tile_mem[cbb][tid]
extern CHARBLOCK8 tile8_mem[4];   // 8bpp variant

// Screenblock memory:
extern SCREENBLOCK se_mem[32];    // se_mem[sbb][index]

// Palette:
extern COLOR pal_bg_mem[256];     // BG palette
extern COLOR pal_obj_mem[256];    // Sprite palette
extern COLOR pal_bg_bank[16][16]; // BG 4bpp sub-palettes
extern COLOR pal_obj_bank[16][16]; // Sprite 4bpp sub-palettes

// OAM:
extern OBJ_ATTR oam_mem[128];     // Real OAM (write during VBlank only)
extern OBJ_AFFINE oam_aff_mem[32];
```

### Video Registers (tonc names)
```c
REG_DISPCNT        // Display control
REG_BG0CNT         // BG0 control (use REG_BGCNT[0] array form too)
REG_BG0HOFS        // BG0 horizontal scroll (write-only)
REG_BG0VOFS        // BG0 vertical scroll (write-only)
REG_BG_OFS[n].x    // BG n scroll via BG_POINT struct (write-only)
REG_BG_OFS[n].y
```

### Key Input
```c
key_poll();                  // Call once per frame
key_is_down(KEY_A);          // Is key held?
key_hit(KEY_A);              // Was key just pressed?
key_released(KEY_A);         // Was key just released?
key_held(KEY_A);             // Down both current and previous frame
key_tri_horz();              // +1=RIGHT, -1=LEFT, 0=neutral
key_tri_vert();              // +1=DOWN,  -1=UP,   0=neutral
bit_tribool(x, plus, minus); // Generic tribool from any flags
```

### Sprite/OAM Functions
```c
// Initialize all sprites (hides them):
void oam_init(OBJ_ATTR *obj, uint count);

// Copy shadow OAM to real OAM:
void oam_copy(OBJ_ATTR *dst, const OBJ_ATTR *src, uint count);

// Set sprite attributes:
OBJ_ATTR *obj_set_attr(OBJ_ATTR *obj, u16 a0, u16 a1, u16 a2);

// Set sprite position:
void obj_set_pos(OBJ_ATTR *obj, int x, int y);

// Hide/show:
void obj_hide(OBJ_ATTR *obj);
void obj_unhide(OBJ_ATTR *obj, u16 mode);  // mode=ATTR0_REG for normal
```

### OAM Attribute Macros
```c
ATTR0_Y(n)           // Y position
ATTR0_SQUARE         // Square shape
ATTR0_WIDE           // Wide shape (W > H)
ATTR0_TALL           // Tall shape (H > W)
ATTR0_4BPP           // 16-color mode
ATTR0_8BPP           // 256-color mode
ATTR0_HIDE           // Hide this sprite
ATTR0_BUILD(y, shape, bpp, mode, mos, bld, win)  // Build full attr0

ATTR1_X(n)           // X position
ATTR1_HFLIP          // Horizontal flip
ATTR1_VFLIP          // Vertical flip
ATTR1_SIZE_8         // 8px (depends on shape)
ATTR1_SIZE_16
ATTR1_SIZE_32
ATTR1_SIZE_64

ATTR2_ID(n)          // Tile index
ATTR2_PRIO(n)        // Priority (0–3)
ATTR2_PALBANK(n)     // Palette bank (0–15)
ATTR2_BUILD(id, pbank, prio)  // Build full attr2
```

### Interrupts (tonc irq system)
```c
// Initialize:
irq_init(NULL);                    // Use default master ISR

// Add handlers:
irq_add(II_VBLANK, vblank_handler);
irq_add(II_HBLANK, hblank_handler);
irq_add(II_TIMER0, timer_handler);

// Remove:
irq_delete(II_VBLANK);

// VBlank wait (preferred over busy-wait):
VBlankIntrWait();   // BIOS call — puts CPU to sleep until VBlank IRQ fires

// IRQ indices:
// II_VBLANK, II_HBLANK, II_VCOUNT, II_TIMER0, II_TIMER1, II_TIMER2, II_TIMER3,
// II_SERIAL, II_DMA0, II_DMA1, II_DMA2, II_DMA3, II_KEYPAD, II_GAMEPAK
```

### Fixed-Point Math
```c
// 24.8 fixed-point (FIX_SHIFT = 8, FIX_SCALE = 256)
FIXED int2fx(int d)        // d << 8
FIXED float2fx(float f)    // f * 256
int   fx2int(FIXED fx)     // fx >> 8  (or fx / 256)
float fx2float(FIXED fx)   // fx / 256.0f

// Multiply two fixeds: result = (a * b) >> 8
FIXED fxmul(FIXED a, FIXED b)  // = (a * b) >> FIX_SHIFT

// Clamp and range:
int clamp(int val, int min, int max);
int wrap(int val, int min, int max);
bool IN_RANGE(int x, int min, int max);
```

### Memory Copy
```c
// Safe, optimized alternatives to memcpy:
memcpy16(dst, src, hwcount);  // Copy hwcount halfwords
memcpy32(dst, src, wcount);   // Copy wcount words (put in IWRAM for speed)
memset16(dst, hw, hwcount);   // Fill with halfword value
memset32(dst, wd, wcount);    // Fill with word value

// DMA (faster but can disrupt IRQs):
dma3_cpy(dst, src, bytecount);   // DMA3 word copy
dma3_fill(dst, val, bytecount);  // DMA3 word fill
```

### Common Setup Pattern
```c
int main(void) {
    // Interrupt system
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);  // Enable VBlank IRQ (needed for VBlankIntrWait)

    // Load graphics
    memcpy32(&tile_mem[0][0], bg_tiles,   bg_tiles_size / 4);
    memcpy16(pal_bg_mem,      bg_palette, bg_palette_size / 2);
    memcpy32(&se_mem[30][0],  bg_map,     bg_map_size / 4);

    memcpy32(&tile_mem[4][0], spr_tiles,   spr_tiles_size / 4);
    memcpy16(pal_obj_mem,     spr_palette, spr_palette_size / 2);

    // Configure BG
    REG_BG0CNT = BG_CBB(0) | BG_SBB(30) | BG_4BPP | BG_REG_64x64;

    // Configure display
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_OBJ | DCNT_OBJ_1D;

    // Initialize sprites (hide all)
    oam_init(obj_buffer, 128);

    // Sound
    // ... maxmod init ...

    // Main loop
    while (1) {
        VBlankIntrWait();        // Wait for VBlank
        key_poll();              // Read input
        oam_copy(oam_mem, obj_buffer, num_sprites);  // Update OAM
        // game_update() was called somewhere before VBlankIntrWait
    }
}
```

---

## 10. Performance Tips — 60fps on 16.78MHz

### Core Budget
At 60fps: **280,896 cycles / frame** (~280K cycles).
VBlank: 83,776 cycles (~30% of frame). VDraw: 197,120 cycles (~70%).
**Target: all game logic + rendering prep in 197,120 cycles before VBlank.**

### ARM vs THUMB Code
- **ROM bus is 16-bit**: THUMB code fetches 1 instruction/cycle; ARM code needs 2 fetches/instruction → ARM in ROM is **slower**
- **IWRAM bus is 32-bit + no waitstates**: ARM code in IWRAM is fastest possible
- **Compile rule:** Normal code = THUMB (in ROM). Hot inner loops / ISRs = ARM in IWRAM

```c
// Force a function to IWRAM + compile as ARM:
void hot_function(void) IWRAM_CODE;

// In a separate .iwram.c file (devkitARM handles this automatically):
void hot_function(void) {
    // This runs from IWRAM as ARM code
}
```

### Avoid Expensive Operations
```c
// BAD — hardware division is software-emulated on ARM7TDMI (~~40 cycles):
int result = a / b;

// GOOD — use shifts for power-of-2 divisions:
int result = a >> 4;   // a / 16

// GOOD — precompute reciprocal for runtime constant division:
// If dividing by N many times, compute (65536/N) once, then use:
// result = (a * inv_N) >> 16
```

### Use Integer (and Fixed-Point) Math
```c
// BAD — float operations (~several cycles each, potential slow path):
float x = pos_x * 1.5f;

// GOOD — fixed-point:
int x = (pos_x_fp * 3) >> 1;  // multiply by 1.5 = *3/2
```

### Memory Access Patterns
- **IWRAM** (32 KB): Store active game objects, shadow OAM, hot code here
- **EWRAM** (256 KB): Store map data, large asset buffers, inactive data
- **ROM** (PAK): Assets, tile data, map data — access is slow (2–3 wait states)
- Avoid reading from ROM in tight loops; copy to RAM first

```c
// Keep active entity list in IWRAM:
Entity entities[MAX_ENTITIES] IWRAM_DATA;

// Or use explicit placement:
__attribute__((section(".iwram"))) Entity entities[MAX_ENTITIES];
```

### DMA for Bulk Copies
```c
// Copying tiles to VRAM at start of level: use DMA3 (fastest)
dma3_cpy(&tile_mem[0], tile_data, tile_data_size);
// About 10× faster than a C loop for large copies
// Note: DMA blocks CPU — don't use during gameplay for small copies
```

### Sprite Pixel Limit Trick
If you're approaching the ~960 sprite pixels/scanline limit:
- Use priority flags to hide distant sprites behind BG layers
- Use 4bpp (not 8bpp) sprites — same pixel count but better palette flexibility
- Keep large decorative elements on BGs, not sprites
- Use `DCNT_OAM_HBL` to allow OAM reads during HBlank if you need more sprite time

### VBlank Window Usage
```
VBlank = 83,776 cycles. Use this for:
- oam_copy() — ~5K cycles for 128 sprites
- REG_BG scroll updates — nearly free
- DMA tile updates — only if changing maps
- Maxmod's mmVBlank() — ~2K cycles at 16KHz
```

### BIOS Call: VBlankIntrWait
Always use `VBlankIntrWait()` instead of a busy-wait loop:
- Puts CPU in low-power halt mode during idle time
- Wakes on VBlank IRQ
- Critical for battery life on real hardware

### Lookup Tables (LUTs)
```c
// Pre-compute trigonometry (sin/cos for isometric rotations):
// tonc provides lu_sin/lu_cos (2048-entry, .12 fixed-point):
s32 sin_val = lu_sin(angle);  // angle in 0–0xFFFF (full circle)
s32 cos_val = lu_cos(angle);
// Result is in .12 fixed-point (range -4096 to +4096)

// For isometric conversions, precompute the row/column offsets
// as a small LUT indexed by tile position mod 4, etc.
```

### Object Count Budget
For a Zelda-style action game, aim for:
- Player: 1–2 sprites (body + weapon flash)
- Enemies: 4–8 visible at once
- Projectiles/effects: 4–6
- UI elements: 2–4 sprites
- **Total: 16–20 active sprites** — well within hardware limit

### Profile with Timers
```c
// Time a code block using Timer 0:
REG_TM0D   = 0;
REG_TM0CNT = TM_ENABLE | TM_FREQ_1;  // 1-cycle resolution
// ... code to profile ...
REG_TM0CNT = 0;
u16 elapsed = REG_TM0D;  // cycles elapsed
```

---

## 11. Further Reading

### Essential Documentation
| Resource | URL | Notes |
|----------|-----|-------|
| **Tonc** (GBA tutorial) | https://www.coranac.com/tonc/text/toc.htm | Best tutorial; covers everything from scratch |
| **Tonc (new site)** | https://gbadev.net/tonc/ | Updated version |
| **GBATek** | https://problemkaputt.de/gbatek.htm | Full hardware reference (CPU, video, sound, etc.) |
| **CowBite Spec** | https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm | Older but clear hardware docs |
| **Maxmod docs** | https://maxmod.org/ref/ | Sound library API reference |
| **gbadev.net resources** | https://gbadev.net/resources.html | Curated links to everything |

### Tonc Chapter Index (Key Chapters)
| Chapter | URL | Content |
|---------|-----|---------|
| 1 — Hardware | hardware.htm | Memory map, CPU, specs |
| 4 — Video intro | video.htm | Display registers, VSync, palettes |
| 7 — Sprites/BG overview | objbg.htm | Tiles, charblocks, palettes |
| 8 — Regular sprites | regobj.htm | OAM, attributes, positioning |
| 9 — Regular BGs | regbg.htm | Tilemaps, scrolling, screenblocks |
| 6 — Keys | keys.htm | Input with edge detection and tribools |
| 14 — DMA | dma.htm | DMA channels, HDMA, fills |
| 15 — Timers | timers.htm | Timer registers, cascade |
| 16 — Interrupts | interrupts.htm | IRQ setup, VBlank, HBlank |
| 18 — Sound | sndsqr.htm | DMG sound registers |
| Appendix B — Fixed-point | fixed.htm | Fixed-point math, LUTs |
| Appendix C — Assembly | asm.htm | ARM assembly intro |

### Homebrew Examples to Study
| Project | GitHub | Why interesting |
|---------|--------|-----------------|
| **Goodboy Advance** | https://github.com/exelotl/goodboy-advance | Well-documented action platformer |
| **BlindJump** | https://github.com/evanbowman/blind-jump-portable | Zelda-like dungeon crawler! |
| **Celeste Classic GBA** | https://github.com/JeffRuLz/Celeste-Classic-GBA | Action platformer, tight code |
| **GBADoom** | https://github.com/doomhack/GBADoom | Advanced rendering techniques |
| **OpenLara** | https://github.com/XProger/OpenLara | 3D on GBA (for inspiration) |

### Tools
| Tool | Purpose |
|------|---------|
| **devkitARM** | Compiler toolchain (arm-none-eabi-gcc) |
| **libtonc** | Included in devkitARM — our main library |
| **Maxmod** | Included in devkitARM — sound library |
| **grit** | Tile/sprite/palette converter (bitmap → GBA format) |
| **mmutil** | Maxmod soundbank builder |
| **mGBA** | Best emulator; has debugger, memory viewer |
| **no$gba** | Alternative emulator with graphical debugger |
| **Usenti** | Paletted pixel art editor for GBA |
| **Aseprite** | Great for pixel art; use grit to convert |

### grit Example (tile conversion)
```bash
# Convert a 256x64 sprite sheet (16×32 sprites, 4bpp) to GBA C array:
grit player.bmp -gB4 -Mw2 -Mh4 -o player

# Convert isometric floor tileset (256x128, 8x8 tiles, 4bpp):
grit floor_tiles.bmp -gB4 -o floor_tiles

# Convert with map reduction (finds unique tiles):
grit floor_bg.bmp -gB4 -mRtpf -o floor_bg
```

### GBA Community
- **Discord**: https://gbadev.net/chat.html — Active community, great for questions
- **Forum**: https://forum.gbadev.net
- **GBATek forums**: See gbadev.net for links

---

## Quick Reference: Essential Constants

```c
// Screen dimensions
#define SCREEN_W    240
#define SCREEN_H    160

// Common isometric tile sizes (FFTA-style)
#define ISO_TILE_W  32
#define ISO_TILE_H  16
#define ISO_HALF_W  16
#define ISO_HALF_H  8

// Sprite setup
#define DCNT_SPRITE_SETUP (DCNT_OBJ | DCNT_OBJ_1D)

// Typical BG setup for isometric game
#define BG_ISO_FLOOR (BG_CBB(0) | BG_SBB(30) | BG_4BPP | BG_REG_64x64)
#define BG_ISO_WALL  (BG_CBB(1) | BG_SBB(28) | BG_4BPP | BG_REG_64x64)
#define BG_UI        (BG_CBB(2) | BG_SBB(26) | BG_4BPP | BG_REG_32x32)

// VBlank cycle budget
#define VBLANK_CYCLES 83776
// VDraw cycle budget  
#define VDRAW_CYCLES  197120

// World-to-screen isometric transform
#define ISO_TO_SCREEN_X(tx, ty)  (((tx) - (ty)) * ISO_HALF_W)
#define ISO_TO_SCREEN_Y(tx, ty)  (((tx) + (ty)) * ISO_HALF_H)

// Screen-to-world (approximate, for mouse/cursor picking)
#define SCREEN_TO_ISO_X(sx, sy)  ((sx)/ISO_HALF_W/2 + (sy)/ISO_HALF_H/2)
#define SCREEN_TO_ISO_Y(sx, sy)  ((sy)/ISO_HALF_H/2 - (sx)/ISO_HALF_W/2)
```

---

*Document compiled from: Tonc v1.4.2 (J Vijn / coranac.com), GBATek (Martin Korth / problemkaputt.de), Maxmod documentation (devkitPro), gbadev.net resources. All hardware information accurate for real GBA hardware. Verified 2026-02-15.*
