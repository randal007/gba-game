# libtonc (Tonc) GBA Development — Research Notes

> Source: [Tonc v1.4.2](https://www.coranac.com/tonc/text/toc.htm) — the definitive GBA development guide by J. Vijn
> Researched: 2026-02-15

---

## Table of Contents
1. [Project Structure & Toolchain Setup](#1-project-structure--toolchain-setup)
2. [Mode 0 Backgrounds (Regular Tiled Backgrounds)](#2-mode-0-backgrounds-regular-tiled-backgrounds)
3. [OAM Sprite Management](#3-oam-sprite-management)
4. [Key Input Handling](#4-key-input-handling)
5. [VBlank Timing & Game Loop Structure](#5-vblank-timing--game-loop-structure)
6. [Memory Map Quick Reference](#6-memory-map-quick-reference)

---

## 1. Project Structure & Toolchain Setup

### Toolchain
- **devkitPro** + **devkitARM** — the standard GBA homebrew toolchain
- Available for Windows, Mac, Linux
- Download: https://devkitpro.org
- Key tool: `arm-none-eabi-gcc` (the cross-compiler)

### Project Directory Structure
```
my_game/
├── Makefile
├── source/         ← C/C++/asm source files go here
├── include/        ← header files (create manually if needed)
└── build/          ← auto-created by the build system
```

### Makefile Essentials
Use the devkitPro template from `$(DEVKITPRO)/examples/gba/template`. Key variables to set:

```makefile
TARGET  := my_game_mb       # _mb suffix = multiboot build
BUILD   := build
SOURCES := source
DATA    :=
INCLUDES := include

ARCH    := -mthumb -mthumb-interwork
CFLAGS  := -g -Wall -O2 -mcpu=arm7tdmi -mtune=arm7tdmi \
           -fomit-frame-pointer -ffast-math $(ARCH)
LDFLAGS := -g $(ARCH) -Wl,-Map,$(notdir $@).map

LIBS    := -ltonc                  # link libtonc
LIBDIRS := $(DEVKITPRO)/libtonc    # where libtonc lives
```

> **Note:** Use `-O2` not `-O3`; -O3 can bloat code significantly.

### Build Steps (Manual)
```bash
# 1. Compile
arm-none-eabi-gcc -mthumb -mthumb-interwork -c source/main.c

# 2. Link
arm-none-eabi-gcc -specs=gba.specs -mthumb -mthumb-interwork main.o -ltonc -o game.elf

# 3. Strip to binary
arm-none-eabi-objcopy -O binary game.elf game.gba

# 4. Fix ROM header
gbafix game.gba
```

### Two Build Types
| Type | linker specs | code/data lives in | size |
|------|-------------|-------------------|------|
| Cart | `-specs=gba.specs` | ROM at 0x08000000 | 32 MB |
| Multiboot | `-specs=gba_mb.specs` | EWRAM at 0x02000000 | 256 KB |

> If TARGET ends in `_mb`, the template makefile builds multiboot automatically.

### Include libtonc
```c
#include <tonc.h>   // includes everything: types, memmap, video, input, etc.
```

Key subheaders you might include individually:
- `tonc_types.h` — u8/u16/u32/s8/s16/s32 typedefs
- `tonc_memmap.h` — memory map defines (tile_mem, pal_bg_mem, se_mem, oam_mem…)
- `tonc_memdef.h` — register/bit defines
- `tonc_video.h` — video helpers
- `tonc_input.h` — key polling

### Good/Bad Practices (from Tonc)
- ✅ Use **ints (32-bit)** for loop variables and temporaries — 8/16-bit vars add extra shift instructions
- ✅ Use **makefiles**, not batch files
- ✅ Always compile with `-mthumb -mthumb-interwork -Wall`
- ✅ Use **named constants** — never raw hex in game code
- ❌ Don't put data in header files
- ❌ Don't use spaces in project paths (GCC toolchain issue)

---

## 2. Mode 0 Backgrounds (Regular Tiled Backgrounds)

### Concept Overview
- GBA screen = 240×160 pixels; tiles = 8×8 pixels
- A **tileset** (charblock) stores the unique pixel data for tiles
- A **tilemap** (screenblock) stores a grid of tile indices telling the hardware which tile to draw where
- Tiled rendering is done **in hardware** — very fast, no per-pixel work needed

### VRAM Layout
| Memory | Address | Size | Structure |
|--------|---------|------|-----------|
| Tile VRAM | 0x06000000 | 64 KB | 4 charblocks × 16 KB each |
| Charblock 0 | 0x06000000 | 16 KB = SBB 0–7 | |
| Charblock 1 | 0x06004000 | 16 KB = SBB 8–15 | |
| Charblock 2 | 0x06008000 | 16 KB = SBB 16–23 | |
| Charblock 3 | 0x0600C000 | 16 KB = SBB 24–31 | |

> **Charblocks and screenblocks share the same VRAM!** Be careful not to let tile data overwrite map data or vice versa.
>
> Common layout: tiles in charblock 0, map in screenblock 28–31 (high end of VRAM)

### Accessing VRAM via libtonc
```c
// tile_mem[cbb][tile_index]  — 4bpp tiles (32 bytes each)
// tile8_mem[cbb][tile_index] — 8bpp tiles (64 bytes each)
// se_mem[sbb][entry_index]   — screen entries (map data, u16 each)
// pal_bg_mem[index]          — background palette (256 u16 colors)
// pal_bg_bank[bank][index]   — palette bank access (16-color mode)
```

### Screen Entry Format (Regular BG, 16-bit each)
```
Bits 0–9:   Tile index (TID)
Bits 10–11: H-flip (HF), V-flip (VF)
Bits 12–15: Palette bank (PB) — only for 4bpp mode
```

### REG_BGxCNT — Background Control Register
```
Address: 0x04000008 + 2*x  (x = 0,1,2,3 for BG0–BG3)

Bits 0–1:  Priority (0=highest, 3=lowest)
Bits 2–3:  Character Base Block (CBB) — which charblock holds the tileset (0–3)
Bit 6:     Mosaic enable
Bit 7:     Color mode: 0=4bpp (16 colors), 1=8bpp (256 colors)
Bits 8–C:  Screen Base Block (SBB) — which screenblock holds the map (0–31)
Bit D:     Affine wrap (irrelevant for regular BGs)
Bits E–F:  Size
```

Regular BG sizes:
| Size bits | Define | Tiles | Pixels |
|-----------|--------|-------|--------|
| 00 | `BG_REG_32x32` | 32×32 | 256×256 |
| 01 | `BG_REG_64x32` | 64×32 | 512×256 |
| 10 | `BG_REG_32x64` | 32×64 | 256×512 |
| 11 | `BG_REG_64x64` | 64×64 | 512×512 |

### Scrolling Registers
```
REG_BG0HOFS @ 0x04000010  ← write-only!
REG_BG0VOFS @ 0x04000012  ← write-only!
```

**Important:** These give the **map position** of the top-left screen corner (not the sprite/screen position).
- Increasing HOFS scrolls the map LEFT (screen moves right over the map)
- Because they're **write-only**, you must keep your own x/y variables and write to them each frame

```c
// Correct pattern — keep your own variables:
int scroll_x = 0, scroll_y = 0;

// In game loop:
scroll_x += key_tri_horz();
scroll_y += key_tri_vert();
REG_BG0HOFS = scroll_x;
REG_BG0VOFS = scroll_y;
```

### Essential Steps to Show a Tiled Background

```c
#include <tonc.h>
#include "my_tileset.h"   // generated by grit/gfx2gba

int main() {
    // 1. Load palette into background palette memory
    memcpy(pal_bg_mem, myTilesPal, myTilesPalLen);

    // 2. Load tiles into charblock 0
    memcpy(&tile_mem[0][0], myTilesTiles, myTilesTilesLen);

    // 3. Load map into screenblock 28
    memcpy(&se_mem[28][0], myTilesMap, myTilesMapLen);

    // 4. Configure BG0: CBB=0, SBB=28, 4bpp, 32x32 tiles
    REG_BG0CNT = BG_CBB(0) | BG_SBB(28) | BG_4BPP | BG_REG_32x32;

    // 5. Enable Mode 0 and BG0
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;

    int x = 0, y = 0;
    while (1) {
        vid_vsync();
        key_poll();
        x += key_tri_horz();
        y += key_tri_vert();
        REG_BG0HOFS = x;
        REG_BG0VOFS = y;
    }
    return 0;
}
```

### Video Mode 0 — All 4 BGs are Regular
```c
REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;           // BG0 only
REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1; // BG0 + BG1
// Can use up to DCNT_BG0 | DCNT_BG1 | DCNT_BG2 | DCNT_BG3
```

### BG Priority (layering)
- Priority 0 = drawn on top; priority 3 = drawn behind
- Sprites cover backgrounds of the same priority
- Set via `BG_PRIO(n)` in `REG_BGxCNT`

### Graphics Conversion Tools
```bash
# grit (recommended) — produces .c + .h with tiles, map, palette
grit level.bmp -gB4 -mRtpf -mLs
# -gB4 = 4bpp tiles
# -mRtpf = map with reduction (unique tiles), flipping allowed
# -mLs = screenblock (SBB) layout for multi-SBB maps

# gfx2gba (older)
gfx2gba -fsrc -c16 -t8 -m level.bmp -mm 32
```

### Updating Individual Map Tiles at Runtime
```c
// Set tile at map position (tx, ty) to tile index tid with palette bank pb:
// (for a 32x32 map in SBB 28)
se_mem[28][ty * 32 + tx] = SE_PALBANK(pb) | tid;
```

For larger maps spanning multiple SBBs, use the helper:
```c
// From regbg.htm — works for any size:
uint se_index(uint tx, uint ty, uint pitch) {
    uint sbb = (ty/32) * (pitch/32) + (tx/32);
    return sbb * 1024 + (ty%32)*32 + tx%32;
}
// Usage: se_mem[SBB_BASE][se_index(tx, ty, map_width_in_tiles)]
```

---

## 3. OAM Sprite Management

### Overview
- Up to **128 sprites** (called "objects" in hardware) available at once
- Sprite graphics stored in **Object VRAM** (charblocks 4 & 5, starting at 0x06010000)
- Sprite attributes stored in **OAM** at 0x07000000
- Sprite palette at 0x05000200 (separate from BG palette!)
- Sprites always use **1D mapping** in practice (enable in REG_DISPCNT bit 6)

### Tile Indexing for Sprites
- Sprites always count tiles in **32-byte (4bpp tile) units**, regardless of bitdepth
- tile 0 = 0x06010000, tile 1 = 0x06010020, etc.
- In 8bpp mode, use even tile indices only (tile 0, 2, 4…)
- In bitmap modes 3–5, only tiles 512–1023 are available (lower block used by bitmap)

```c
// Load sprite tiles into object VRAM
memcpy(&tile_mem[4][0], mySpriteTiles, mySpriteTilesLen);  // charblock 4 = sprite VRAM
// Load sprite palette (NOT bg palette!)
memcpy(pal_obj_mem, mySpritePal, mySpritePalLen);
```

### OBJ_ATTR Structure
```c
typedef struct OBJ_ATTR {
    u16 attr0;
    u16 attr1;
    u16 attr2;
    s16 fill;     // used by OBJ_AFFINE — don't touch for regular sprites
} ALIGN4 OBJ_ATTR;
```

**attr0 bits:**
```
Bits 0–7:  Y coordinate (top of sprite)
Bits 8–9:  Object mode: 00=normal, 01=affine, 10=hidden, 11=affine double
Bits 10–11: GFX mode: 00=normal, 01=alpha blend, 10=obj window
Bit 12:    Mosaic
Bit 13:    Color mode: 0=4bpp, 1=8bpp
Bits 14–15: Shape: 00=square, 01=wide, 10=tall
```

**attr1 bits:**
```
Bits 0–8:  X coordinate (left of sprite)
Bits 9–13: Affine index (if affine mode) OR:
Bit 12:    H-flip
Bit 13:    V-flip
Bits 14–15: Size (combined with shape gives actual dimensions)
```

**attr2 bits:**
```
Bits 0–9:  Base tile index (starting tile in object VRAM)
Bits 10–11: Priority (0=front, 3=back)
Bits 12–15: Palette bank (4bpp mode only)
```

### Sprite Sizes (shape × size)
| Shape\Size | 00 | 01 | 10 | 11 |
|------------|----|----|----|----|
| Square (00) | 8×8 | 16×16 | 32×32 | 64×64 |
| Wide (01) | 16×8 | 32×8 | 32×16 | 64×32 |
| Tall (10) | 8×16 | 8×32 | 16×32 | 32×64 |

### OAM Double Buffer Pattern (recommended)
```c
// Shadow buffer — write here any time
OBJ_ATTR obj_buffer[128];
OBJ_AFFINE *obj_aff_buffer = (OBJ_AFFINE*)obj_buffer;

// Copy to real OAM only during VBlank
oam_copy(oam_mem, obj_buffer, 128);  // or just the count you're using
```

### Essential Sprite Functions (tonc)
```c
// Initialize all sprites (hides them all)
oam_init(obj_buffer, 128);

// Set all three attributes at once
obj_set_attr(obj, attr0, attr1, attr2);

// Set position
obj_set_pos(obj, x, y);

// Hide / show
obj_hide(obj);
obj_unhide(obj, ATTR0_REG);  // ATTR0_REG = normal mode (0)

// Copy shadow buffer to real OAM
oam_copy(oam_mem, obj_buffer, count);
```

### Setting Up a Sprite — Full Example
```c
#include <tonc.h>
#include "my_sprite.h"

OBJ_ATTR obj_buffer[128];

int main() {
    // 1. Load graphics
    memcpy(&tile_mem[4][0], mySpriteTiles, mySpriteTilesLen);
    memcpy(pal_obj_mem, mySpritePal, mySpritePalLen);

    // 2. Hide all sprites initially
    oam_init(obj_buffer, 128);

    // 3. Enable sprites + 1D mapping in display control
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_OBJ | DCNT_OBJ_1D;

    // 4. Set up sprite 0: 16x16, 4bpp, tile 0, palette bank 0
    obj_set_attr(&obj_buffer[0],
        ATTR0_SQUARE | ATTR0_4BPP,   // shape=square, 4bpp color
        ATTR1_SIZE_16,               // size → 16×16
        ATTR2_PALBANK(0) | 0         // palette bank 0, tile index 0
    );

    int x = 100, y = 60;
    obj_set_pos(&obj_buffer[0], x, y);
    oam_copy(oam_mem, obj_buffer, 1);

    while (1) {
        vid_vsync();
        key_poll();

        x += 2 * key_tri_horz();
        y += 2 * key_tri_vert();

        obj_set_pos(&obj_buffer[0], x, y);
        oam_copy(oam_mem, obj_buffer, 1);
    }
    return 0;
}
```

### Animation
Animation works by changing the **base tile index** in attr2 each frame:
```c
// Sprite sheet: each frame is 4 tiles wide (16×16 sprite = 2×2 tiles)
// Frame 0 = tiles 0-3, Frame 1 = tiles 4-7, Frame 2 = tiles 8-11, etc.

int frame = 0;
int frame_timer = 0;
int tiles_per_frame = 4;   // for a 16x16 sprite
int anim_speed = 8;        // change frame every 8 game frames

// In game loop:
frame_timer++;
if (frame_timer >= anim_speed) {
    frame_timer = 0;
    frame = (frame + 1) % total_frames;
}
// Update tile index:
BF_SET(obj_buffer[0].attr2, frame * tiles_per_frame, ATTR2_ID);
```

Alternative: **overwrite VRAM** with new frame data instead (less common, wastes bandwidth):
```c
memcpy(&tile_mem[4][0], frame_data[current_frame], frame_size);
```

### Flipping
```c
// Horizontal flip:
obj_buffer[0].attr1 ^= ATTR1_HFLIP;

// Vertical flip:
obj_buffer[0].attr1 ^= ATTR1_VFLIP;
```

---

## 4. Key Input Handling

### The GBA Has 10 Buttons
A, B, Select, Start, Right, Left, Up, Down, R, L

### REG_KEYINPUT (0x04000130) — Low-Active!
```
Bits 0–9: A, B, Select, Start, Right, Left, Up, Down, R, L
```
**IMPORTANT:** Bits are **0 when pressed** (active LOW). libtonc inverts this for you.

### libtonc Key API (tonc_input.h)

```c
// Call once per frame at start of game loop:
key_poll();

// Then use these functions (all work on the current frame's snapshot):
key_is_down(KEY_A)       // true while A is held
key_is_up(KEY_A)         // true while A is NOT held
key_hit(KEY_A)           // true only on the frame A was first pressed (edge detect)
key_released(KEY_A)      // true only on the frame A was released
key_held(KEY_A)          // true while A held (was down last frame AND this frame)
key_transit(KEY_A)       // true when state changed (hit OR released)
```

Key defines:
```c
#define KEY_A       0x0001
#define KEY_B       0x0002
#define KEY_SELECT  0x0004
#define KEY_START   0x0008
#define KEY_RIGHT   0x0010
#define KEY_LEFT    0x0020
#define KEY_UP      0x0040
#define KEY_DOWN    0x0080
#define KEY_R       0x0100
#define KEY_L       0x0200
```

### Tribool Functions — Compact Direction Handling
Instead of if/else chains, use tribools that return -1, 0, or +1:

```c
key_tri_horz()      // right=+1, left=-1, neither=0
key_tri_vert()      // down=+1, up=-1, neither=0
key_tri_shoulder()  // R=+1, L=-1
key_tri_fire()      // A=+1, B=-1
```

Usage:
```c
// Move a sprite:
x += speed * key_tri_horz();
y += speed * key_tri_vert();

// Cycle through options:
selected_item += bit_tribool(key_hit(-1), KI_RIGHT, KI_LEFT);
```

### Why key_poll() Instead of Direct Reads?
- Reads REG_KEYINPUT **once** per frame → consistent state throughout frame logic
- Enables **edge detection** (key_hit/key_released) — critical for pause toggles, menus, etc.
- Without it: holding Start for 2 frames = pause + unpause = random state

### Implementation (what tonc does internally)
```c
u16 __key_curr = 0, __key_prev = 0;

INLINE void key_poll() {
    __key_prev = __key_curr;
    __key_curr = ~REG_KEYINPUT & KEY_MASK;  // invert: now 1=pressed
}

INLINE u32 key_hit(u32 key) {
    return (__key_curr & ~__key_prev) & key;  // down now, not before
}
```

---

## 5. VBlank Timing & Game Loop Structure

### GBA Display Timing
| Period | Duration | Cycles |
|--------|----------|--------|
| HDraw (one scanline) | 240 pixels | 960 |
| HBlank | 68 pixels | 272 |
| Full scanline | — | 1232 |
| VDraw (all scanlines) | 160 lines | 197120 |
| VBlank | 68 lines | 83776 |
| Full frame | — | 280896 |

- **Refresh rate:** ~59.73 Hz
- **VBlank** = scanlines 160–227 = ~1.4 ms of "safe" time to update graphics data
- During VDraw, OAM is **locked** — you can't write to it

### REG_VCOUNT (0x04000006, read-only)
- Current scanline being drawn (0–159 = VDraw, 160–227 = VBlank)

### REG_DISPSTAT (0x04000004)
```
Bit 0: VBlank status (1 = currently in VBlank)
Bit 1: HBlank status
Bit 3: VBlank interrupt enable
Bit 4: HBlank interrupt enable
```

### Method 1: Busy-Wait VSync (Simple, Wastes Power)
```c
void vid_vsync() {
    while (REG_VCOUNT >= 160);  // wait for VDraw (in case we're already in VBlank)
    while (REG_VCOUNT < 160);   // wait for VBlank to start
}
```

libtonc provides `vid_vsync()` — use this directly.

**Limitation:** CPU burns 100% of cycles spinning. Bad for battery. Fine for demos.

### Method 2: Interrupt-Based VSync (Recommended for Real Games)
Uses the VBlank interrupt + BIOS `VBlankIntrWait()` call to put CPU in low-power mode:

```c
#include <tonc.h>

// In main(), before game loop:
irq_init(NULL);                     // init interrupt system
irq_add(II_VBLANK, NULL);           // register VBlank interrupt (no custom ISR needed)
// or with a custom VBlank handler:
irq_add(II_VBLANK, my_vblank_isr);

// To wait for VBlank (CPU goes to sleep until interrupt fires):
VBlankIntrWait();    // BIOS call — very efficient
```

Requires:
1. `REG_DISPSTAT |= DSTAT_VBL_IRQ;` (done by `irq_enable(II_VBLANK)`)
2. `REG_IE |= IRQ_VBLANK;`
3. `REG_IME = 1;`
4. `REG_IFBIOS` acknowledgment (handled by tonclib's `isr_master`)

### Standard Game Loop Structure

```c
#include <tonc.h>
// ... include your asset headers

OBJ_ATTR obj_buffer[128];

int main() {
    // ── Initialization ──────────────────────────────────────────
    // 1. Set up interrupt system for VBlank timing
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    // 2. Load graphics
    memcpy(pal_bg_mem,      bgPal,    bgPalLen);
    memcpy(&tile_mem[0][0], bgTiles,  bgTilesLen);
    memcpy(&se_mem[28][0],  bgMap,    bgMapLen);
    memcpy(&tile_mem[4][0], sprTiles, sprTilesLen);
    memcpy(pal_obj_mem,     sprPal,   sprPalLen);

    // 3. Configure backgrounds
    REG_BG0CNT = BG_CBB(0) | BG_SBB(28) | BG_4BPP | BG_REG_32x32;

    // 4. Configure sprites
    oam_init(obj_buffer, 128);
    obj_set_attr(&obj_buffer[0], ATTR0_SQUARE, ATTR1_SIZE_16, 0);
    obj_set_pos(&obj_buffer[0], 100, 60);

    // 5. Enable display
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_OBJ | DCNT_OBJ_1D;

    // ── State variables ─────────────────────────────────────────
    int player_x = 100, player_y = 60;
    int scroll_x = 0, scroll_y = 0;

    // ── Main Game Loop ───────────────────────────────────────────
    while (1) {
        // 1. Wait for VBlank (sync to ~60 fps)
        VBlankIntrWait();  // or vid_vsync() for simple projects

        // 2. Update OAM during VBlank (OAM is writable now)
        oam_copy(oam_mem, obj_buffer, 128);

        // ── Logic update (runs during VBlank + next VDraw) ──────
        // 3. Read input
        key_poll();

        // 4. Update game state
        player_x += 2 * key_tri_horz();
        player_y += 2 * key_tri_vert();

        if (key_hit(KEY_A)) {
            // fire / jump / action
        }

        // 5. Clamp/wrap player position
        player_x = CLAMP(player_x, 0, 240 - 16);
        player_y = CLAMP(player_y, 0, 160 - 16);

        // 6. Update sprite positions in shadow OAM
        obj_set_pos(&obj_buffer[0], player_x, player_y);

        // 7. Update background scroll
        REG_BG0HOFS = scroll_x;
        REG_BG0VOFS = scroll_y;
    }
    return 0;
}
```

### VBlank ISR Pattern (optional, for VBlank work)
```c
volatile int vblank_count = 0;

void my_vblank_isr() {
    vblank_count++;
    // Safe to update OAM here too
    // oam_copy(oam_mem, obj_buffer, 128);
    REG_IF = IRQ_VBLANK;  // acknowledge interrupt
}

// In main():
irq_init(NULL);
irq_add(II_VBLANK, my_vblank_isr);
```

### Timing Tip
- You have ~83,776 cycles of VBlank time (~1.4ms at 16.78 MHz)
- OAM copy of 128 sprites takes ~256 cycles (very fast)
- Most game logic will fit in VBlank + VDraw combined
- If game logic is too slow, reduce to 30fps by only rendering every other VBlank

---

## 6. Memory Map Quick Reference

| Region | Address | Size | Use |
|--------|---------|------|-----|
| IO Registers | 0x04000000 | 1 KB | REG_DISPCNT, REG_BGxCNT, etc. |
| BG Palette | 0x05000000 | 512 B | 256 × u16 colors |
| Sprite Palette | 0x05000200 | 512 B | 256 × u16 colors |
| BG VRAM (charblocks 0–3) | 0x06000000 | 64 KB | Tilesets + tilemaps |
| Sprite VRAM (charblocks 4–5) | 0x06010000 | 32 KB | Sprite tiles |
| OAM | 0x07000000 | 1 KB | 128 × OBJ_ATTR (sprite attributes) |
| ROM | 0x08000000 | up to 32 MB | Game data (cart build) |
| EWRAM | 0x02000000 | 256 KB | Main work RAM (multiboot build) |
| IWRAM | 0x03000000 | 32 KB | Fast internal RAM |

### Key Registers
| Register | Address | Purpose |
|----------|---------|---------|
| `REG_DISPCNT` | 0x04000000 | Display control (mode, layers enabled) |
| `REG_DISPSTAT` | 0x04000004 | VBlank/HBlank status + IRQ enables |
| `REG_VCOUNT` | 0x04000006 | Current scanline (read-only) |
| `REG_BG0CNT` | 0x04000008 | BG0 control (CBB, SBB, size, bpp) |
| `REG_BG1CNT` | 0x0400000A | BG1 control |
| `REG_BG2CNT` | 0x0400000C | BG2 control |
| `REG_BG3CNT` | 0x0400000E | BG3 control |
| `REG_BG0HOFS` | 0x04000010 | BG0 horizontal scroll (write-only) |
| `REG_BG0VOFS` | 0x04000012 | BG0 vertical scroll (write-only) |
| `REG_KEYINPUT` | 0x04000130 | Key states (0=pressed, active low) |
| `REG_IE` | 0x04000200 | Interrupt Enable |
| `REG_IF` | 0x04000202 | Interrupt Flags (acknowledge by writing 1) |
| `REG_IME` | 0x04000208 | Master Interrupt Enable |

### libtonc Color Macros
```c
COLOR c = RGB15(r, g, b);  // r/g/b in range 0–31
// GBA uses 15-bit BGR: bits 0-4=R, 5-9=G, 10-14=B

// Predefined colors:
CLR_BLACK, CLR_RED, CLR_LIME, CLR_BLUE, CLR_WHITE, CLR_YELLOW, CLR_CYAN, CLR_MAG
```

---

## Further Reading

- **Tonc full text:** https://www.coranac.com/tonc/text/toc.htm
- **GBATek (hardware reference):** https://problemkaputt.de/gbatek.htm
- **libtonc API docs:** https://www.coranac.com/man/tonclib/
- **grit (graphics converter):** included with devkitPro
- **mGBA (best emulator with debugger):** https://mgba.io
