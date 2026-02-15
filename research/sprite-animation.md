# GBA Sprite Animation Techniques for Real-Time Action Games

## 1. OAM Structure — How GBA Sprites Work

### Object Attribute Memory (OAM)

OAM starts at `0x07000000`, is 1024 bytes, and holds **128 OBJ_ATTR entries** (sprite descriptors) and **32 OBJ_AFFINE matrices** interleaved in the same memory space.

Each `OBJ_ATTR` is 3 × 16-bit attributes (+ 16-bit filler that's part of the affine matrix):

```c
typedef struct {
    u16 attr0;  // Y pos, shape, mode, palette mode, mosaic, affine flags
    u16 attr1;  // X pos, size, flip flags (regular) or affine matrix index
    u16 attr2;  // tile index, priority, palette bank (4bpp)
    s16 fill;   // part of OBJ_AFFINE
} OBJ_ATTR;
```

### Attribute Breakdown

**attr0:**
| Bits | Purpose |
|------|---------|
| 0-7 | Y coordinate (0-255, wraps) |
| 8-9 | Object mode: 00=regular, 01=affine, 10=hidden/double-size affine, 11=double-size affine |
| 10-11 | GFX mode: 00=normal, 01=semi-transparent, 10=obj window |
| 12 | Mosaic enable |
| 13 | Color mode: 0=4bpp (16 colors), 1=8bpp (256 colors) |
| 14-15 | Shape: 00=square, 01=horizontal, 10=vertical |

**attr1:**
| Bits | Purpose |
|------|---------|
| 0-8 | X coordinate (0-511, wraps, 9 bits) |
| 9-13 | Affine matrix index (affine mode) OR bits 12-13 = H/V flip (regular mode) |
| 14-15 | Size (combined with shape determines pixel dimensions) |

**attr2:**
| Bits | Purpose |
|------|---------|
| 0-9 | Tile index (base tile number in OVRAM) |
| 10-11 | Priority (0=highest, 3=lowest) |
| 12-15 | Palette bank (4bpp mode only, selects which 16-color palette) |

### Available Sprite Sizes

| Shape\Size | 0 | 1 | 2 | 3 |
|------------|-------|-------|-------|--------|
| Square (00) | 8×8 | 16×16 | 32×32 | 64×64 |
| Horizontal (01) | 16×8 | 32×8 | 32×16 | 64×32 |
| Vertical (10) | 8×16 | 8×32 | 16×32 | 32×64 |

**Maximum sprite size: 64×64 pixels.** Larger characters must be composed of multiple OBJ_ATTRs.

### Palette Modes

- **4bpp (16 colors):** Each pixel is 4 bits. Sprite selects one of 16 palette banks (16 colors each) via attr2 bits 12-15. Tiles are 32 bytes each ("s-tiles").
- **8bpp (256 colors):** Each pixel is 8 bits, uses the full 256-color object palette. Tiles are 64 bytes each ("d-tiles"). Uses twice the VRAM but no palette bank management.

**For action games with many characters:** 4bpp is almost always preferred — half the VRAM per sprite, and 16 palette banks allow color-swapping enemies cheaply.

### Tile Mapping Modes

- **1D mapping** (`DISPCNT` bit 6 = 1): Tiles for a sprite are stored consecutively in VRAM. **Preferred for games** — simpler memory management.
- **2D mapping** (default): Tiles are laid out in a 32-tile-wide virtual bitmap. Easier for art creation, harder for programming.

### Regular vs Affine Sprites

**Regular sprites:**
- Simple position + optional H/V flip
- Flip flags are free (no CPU cost, no extra VRAM)
- Great for mirroring walk cycles (draw right-facing, flip for left)

**Affine sprites:**
- Can rotate and scale via a 2×2 affine matrix (pa, pb, pc, pd as 8.8 fixed-point)
- Only 32 affine matrices available, shared across all affine sprites
- **Double-size mode** recommended to prevent clipping during rotation
- More expensive: each matrix costs fill entries in OAM, and the hardware renderer is slower for affine objects
- Origin of transformation is the sprite center

**For isometric action games:** Use regular sprites for most characters (with H-flip for mirroring). Reserve affine sprites for special effects (rotating weapons, spell effects, scaling bosses).

### Object VRAM

- OVRAM is 32KB total (charblocks 4 and 5, starting at `0x06010000`)
- In bitmap modes (3-5), only the upper 16KB (charblock 5, tiles 512-1023) is available
- In tile modes (0-2), all 32KB available = 1024 s-tiles (4bpp) or 512 d-tiles (8bpp)

---

## 2. Frame-Based Animation

### Basic Approach

Sprite animation on GBA is done by **changing the tile index** in attr2 each frame (or every N frames). The actual pixel data for all frames lives in OVRAM; you just point to different tiles.

```c
// Minimal animation system
typedef struct {
    u16 *frame_tiles;    // array of tile indices for each frame
    u8 num_frames;
    u8 frame_duration;   // how many game frames per anim frame
    u8 current_frame;
    u8 frame_counter;
} SpriteAnim;

void anim_update(SpriteAnim *anim, OBJ_ATTR *obj) {
    if (++anim->frame_counter >= anim->frame_duration) {
        anim->frame_counter = 0;
        anim->current_frame++;
        if (anim->current_frame >= anim->num_frames)
            anim->current_frame = 0;
        // Update tile index in OAM
        obj->attr2 = (obj->attr2 & 0xFC00) | anim->frame_tiles[anim->current_frame];
    }
}
```

### Optimization: Pre-load vs DMA Streaming

**Method 1 — All frames in VRAM (pre-loaded):**
- Load all animation frames into OVRAM at startup
- Animation = just changing the tile index in attr2
- Fastest at runtime, zero CPU cost for frame changes
- Limited by OVRAM size (32KB)

**Method 2 — DMA streaming from ROM:**
- Keep only current frame in VRAM, DMA new tile data from ROM each frame
- Uses less VRAM but costs CPU/DMA bandwidth
- Typically done during VBlank
- Common for games with many unique animations

**Method 3 — Hybrid (most common in commercial games):**
- Pre-load commonly used frames (idle, walk) into VRAM
- DMA-stream rare animations (special attacks, death) on demand
- Best balance of memory and performance

### Animation Timing

- GBA runs at ~60fps (59.7275 Hz)
- Most sprite animations run at 10-15 fps (change frame every 4-6 VBlanks)
- Walk cycles: typically 4-6 frames, cycled every 4-5 VBlanks = ~12-15 fps
- Attack animations: 3-6 frames, often faster (every 2-3 VBlanks)
- Idle: 2-4 frames, slow cycle (every 8-15 VBlanks)

### VBlank Synchronization

**Critical rule:** Only write to OAM during VBlank. Writing during active rendering causes visual glitches.

```c
// Shadow OAM in IWRAM, copied to real OAM each VBlank
OBJ_ATTR shadow_oam[128] IWRAM_DATA;

void vblank_handler(void) {
    // Copy all 128 entries (1KB) via DMA
    DMA3Copy(OAM, shadow_oam, 256 /* 256 words = 1024 bytes */);
}
```

Using a shadow OAM buffer is standard practice — modify sprites freely during gameplay logic, then bulk-copy during VBlank.

---

## 3. Character Animation for Isometric Games

### Direction System

Isometric games typically need **4 or 8 directional sprites**. For a ¾-view isometric style (like FFTA):

**4-direction approach (most common on GBA):**
- Down (toward camera)
- Up (away from camera)
- Left
- Right (= H-flip of Left)

This cuts sprite data in half by mirroring left/right. The H-flip flag in attr1 is free.

**8-direction approach (less common due to memory):**
- Add diagonal variants or interpolate with animation blending
- Games like Zelda: Minish Cap use 4 directions for most NPCs, 8 for Link

### Walk Cycle Structure

A typical 4-direction walk cycle:

```
Direction  | Frames | Notes
-----------|--------|------
Down       | 4-6    | Most detailed, player sees this most
Up         | 4-6    | Back view
Side       | 4-6    | H-flip for opposite direction
```

**Standard walk cycle pattern (4 frames):**
1. Stand/contact (right foot forward)
2. Passing (feet together, body rises slightly)
3. Contact (left foot forward)
4. Passing (feet together)

Or simpler 3-frame: Stand → Step-right → Stand → Step-left (where frame 1 is reused)

### Animation State Machine

```c
typedef enum {
    ANIM_IDLE,
    ANIM_WALK,
    ANIM_ATTACK,
    ANIM_HIT,
    ANIM_DEATH,
    ANIM_CAST,
    ANIM_COUNT
} AnimState;

typedef enum {
    DIR_DOWN,
    DIR_UP,
    DIR_SIDE,   // H-flip determines left vs right
    DIR_COUNT
} Direction;

typedef struct {
    u16 base_tile;          // first tile of this animation
    u8 num_frames;
    u8 frame_duration;      // VBlanks per frame
    u8 tiles_per_frame;     // how many tiles one frame occupies
    bool looping;
} AnimDef;

// Animation table: [state][direction]
const AnimDef anim_table[ANIM_COUNT][DIR_COUNT] = {
    [ANIM_IDLE] = {
        [DIR_DOWN] = { .base_tile = 0,   .num_frames = 2, .frame_duration = 30, .tiles_per_frame = 4 },
        [DIR_UP]   = { .base_tile = 8,   .num_frames = 2, .frame_duration = 30, .tiles_per_frame = 4 },
        [DIR_SIDE] = { .base_tile = 16,  .num_frames = 2, .frame_duration = 30, .tiles_per_frame = 4 },
    },
    [ANIM_WALK] = {
        [DIR_DOWN] = { .base_tile = 24,  .num_frames = 4, .frame_duration = 5,  .tiles_per_frame = 4 },
        [DIR_UP]   = { .base_tile = 40,  .num_frames = 4, .frame_duration = 5,  .tiles_per_frame = 4 },
        [DIR_SIDE] = { .base_tile = 56,  .num_frames = 4, .frame_duration = 5,  .tiles_per_frame = 4 },
    },
    // ... etc
};
```

### Attack Animations

For isometric action games, attack animations need:

1. **Anticipation frame** (1-2 frames): Wind-up, gives player readable telegraph
2. **Active frames** (1-3 frames): The actual strike, hitbox is active
3. **Recovery frames** (1-2 frames): Return to idle, vulnerability window

**Multi-sprite attacks:** Weapon swings or spell effects often use additional OBJ_ATTRs positioned relative to the character. A sword slash might be a separate sprite overlaid.

### Sprite Compositing for Characters

Larger isometric characters (24×32, 32×32) are built from multiple hardware sprites:

```
Character = 16×16 body + 16×16 head (allows separate head animation)
         or 32×32 single sprite (simpler, uses more VRAM)
         or 16×32 + extra weapon sprite
```

**FFTA approach:** Characters are typically 16×24 to 32×32, using the 32×32 OBJ size. Different job classes have entirely separate sprite sheets.

---

## 4. Memory Management

### VRAM Budget

With 1D mapping in tile modes, OVRAM = 32KB = 1024 s-tiles (4bpp):

**Example budget for a 16×16 character (4 s-tiles per frame, 4bpp):**
- 1 frame = 4 tiles = 128 bytes
- 4-frame walk cycle × 3 directions = 12 frames = 48 tiles = 1,536 bytes
- Add idle (2 frames × 3 dirs = 6 frames = 24 tiles = 768 bytes)
- Add attack (4 frames × 3 dirs = 12 frames = 48 tiles = 1,536 bytes)
- **Total per character: ~120 tiles = 3,840 bytes** (~3.75 KB)

With 32KB OVRAM, that's roughly **8 fully-animated characters** simultaneously, leaving room for UI sprites, projectiles, and effects.

### VRAM Allocation Strategies

**Static allocation:**
- Reserve fixed tile regions for each sprite type
- Simple but wastes space when not all sprites are on screen
- Good for games with predictable sprite counts

**Dynamic allocation (pool/slot system):**
```c
#define TILE_POOL_SIZE 1024
u8 tile_used[TILE_POOL_SIZE]; // bitmap of used tiles

u16 alloc_tiles(u16 count) {
    // Find 'count' contiguous free tiles
    u16 run = 0, start = 0;
    for (u16 i = 0; i < TILE_POOL_SIZE; i++) {
        if (!tile_used[i]) {
            if (run == 0) start = i;
            if (++run >= count) {
                for (u16 j = start; j < start + count; j++)
                    tile_used[j] = 1;
                return start;
            }
        } else {
            run = 0;
        }
    }
    return 0xFFFF; // allocation failed
}
```

**Frame streaming (most memory-efficient):**
- Each character gets a fixed VRAM slot sized for one frame
- DMA the current frame's tiles into that slot each VBlank
- All animation data lives in ROM (cheap and plentiful)
- Cost: DMA bandwidth (~128 bytes per 16×16 frame, trivial)

### OAM Slot Management

128 OBJ_ATTR entries must be managed:
- Unused entries should be **hidden** (set attr0 bits 8-9 to `10` = double-size on non-affine = hidden)
- Sort by priority for correct draw order (lower Y = higher on screen in isometric)
- **OAM sort by Y-coordinate** is essential for isometric games to get correct overlap

```c
// Simple Y-sort for isometric sprite ordering
void sort_oam_by_y(void) {
    // Sort shadow_oam entries by Y position
    // Use insertion sort (128 entries, nearly sorted = fast)
    for (int i = 1; i < active_sprite_count; i++) {
        OBJ_ATTR temp = shadow_oam[i];
        int y = temp.attr0 & 0xFF;
        int j = i - 1;
        while (j >= 0 && (shadow_oam[j].attr0 & 0xFF) > y) {
            shadow_oam[j+1] = shadow_oam[j];
            j--;
        }
        shadow_oam[j+1] = temp;
    }
}
```

### Palette Optimization

With 4bpp mode, you have 16 palette banks × 16 colors = 256 colors for sprites:

- **Color swapping:** Same tiles, different palette bank = recolored enemy. Extremely common (costs zero VRAM)
- **Shared palettes:** Design multiple characters to share a 16-color palette
- Plan palettes early in development — retrofitting is painful

---

## 5. Practical Tips & Limits

### Sprite Rendering Limits

The GBA renders sprites per-scanline. The hardware has a **per-scanline pixel budget of ~960 sprite pixels** (1210 cycles):

- Each regular sprite pixel costs ~1 cycle
- Each affine sprite pixel costs ~2 cycles  
- Transparent pixels still cost cycles if they're within the sprite's bounding box

**Practical limit per scanline:**
- ~960 regular sprite pixels, or
- ~480 affine sprite pixels, or
- A mix

**What this means:** If a scanline has too many sprites, the rightmost ones (by OAM order) are dropped. This is the classic **sprite dropout/flickering** issue.

### Sprite Flickering Solutions

**Problem:** When too many sprites share a scanline, hardware drops the excess.

**Solution 1 — OAM rotation:**
```c
// Rotate OAM priority each frame so different sprites drop
static u8 oam_rotation_offset = 0;

void rotate_oam(void) {
    // Shift all sprites by an offset in OAM each frame
    // Dropped sprites alternate, creating flicker instead of permanent invisibility
    oam_rotation_offset = (oam_rotation_offset + 1) % active_sprite_count;
    // Copy sprites to shadow_oam starting at rotation offset...
}
```
This distributes dropout across frames — sprites flicker rather than vanish. NES games used this extensively; on GBA it's less necessary but still relevant for sprite-heavy games.

**Solution 2 — Design constraints:**
- Keep character sprites small (16×16 or 16×24)
- Limit enemy count per screen region
- Use backgrounds for large static objects instead of sprites
- Stagger enemy formations vertically so they don't share scanlines

**Solution 3 — Split large sprites:**
- Break a 64×64 boss into 4×4 16×16 sprites
- If some get clipped, only part of the boss flickers

### HBlank Tricks

**HBlank OAM manipulation** is rarely used for sprites (unlike backgrounds) because OAM isn't double-buffered and writing during HBlank is timing-critical. However:

- **Sprite multiplexing:** Change OAM entries during HBlank to reuse OBJ slots for sprites on different scanlines. Extremely tricky timing but allows >128 visible sprites theoretically.
- **Affine matrix HBlank updates:** Update affine matrices per-scanline for effects like sprite distortion, pseudo-3D scaling (Mode 7-like effects on sprites)
- **Priority swapping:** Change sprite priorities mid-frame for complex layering

Most commercial GBA games avoid HBlank sprite tricks due to complexity. Background HBlank effects (parallax scrolling, wavy water) are far more common.

### Performance Tips

1. **DMA for OAM copies:** Always use DMA3 to copy shadow OAM → real OAM. CPU memcpy is too slow.
2. **IWRAM for animation code:** Put tight animation loops in IWRAM (32-bit bus, ~6× faster than ROM).
3. **Avoid per-frame VRAM writes when possible:** Changing attr2 tile index is free; DMA-ing new tile data costs bandwidth.
4. **Batch OAM updates:** Don't write individual OBJ_ATTRs to OAM — use shadow buffer + single DMA.
5. **Use 4bpp over 8bpp:** Half the VRAM, and palette swaps are a bonus.
6. **H-flip is free:** Only draw sprites facing one direction; mirror with the flip bit.

---

## 6. Case Studies: Commercial GBA Games

### Final Fantasy Tactics Advance (FFTA)

**Sprite approach:**
- Characters are ~16×24 to 24×32 pixels, using 32×32 OBJ entries
- Each job class has a complete sprite sheet with all animations
- **5 directions** rendered: down, down-side, side, up-side, up (with H-flip for the other side)
- Heavy use of palette swapping for team colors (blue team vs red team)
- Battle sprites are more detailed than map sprites (separate assets)

**Animation system:**
- Frame-based with variable timing per animation
- Ability animations use additional overlay sprites (spell effects as separate OBJs)
- Map movement uses simple 4-frame walk cycles
- Battle has: idle, walk, attack (weapon-specific), cast, hit, KO animations

**Memory management:**
- Only loads sprites for currently visible units
- Streams tile data from ROM for active animations
- Background is tile-based, keeping OVRAM free for characters
- Typically 6-10 units visible in battle = 6-10 character sprites + effects

### The Legend of Zelda: The Minish Cap

**Sprite approach:**
- Link is a multi-sprite composite: body + hat + sword/item are separate OBJs
- This allows independent animation of equipment and the iconic hat physics
- 4 directional movement with smooth sub-pixel positioning
- Link's sprite is ~16×16 body with overlaid equipment sprites

**Animation highlights:**
- **Hat squash & stretch:** Ezlo (the hat) has its own animation system, bouncing and reacting with 2-3 frame mini-animations
- **Sword trails:** Separate sprites placed behind Link showing motion blur
- **Size mechanic:** When Link shrinks, they scale down his sprite sheet (separate mini-Link assets, not affine scaling, for pixel-perfect look)
- Very expressive idle animations with many frames

**Technical details:**
- Aggressive VRAM streaming — only current frame data in OVRAM
- Uses backgrounds for large bosses and environmental hazards (saves OBJ slots)
- Particle effects (leaves, sparkles) use tiny 8×8 sprites to stay within limits
- Priority/Y-sorting for correct occlusion with environment

### Common Patterns Across Commercial Games

1. **Separate map/battle sprites:** Smaller overworld sprites, detailed battle sprites
2. **Composite characters:** Head + body + weapon as separate OBJs for mix-and-match
3. **Palette swapping everywhere:** Enemy recolors, team colors, status effects (poisoned = green tint)
4. **Backgrounds for large things:** Bosses, vehicles, UI elements use BG layers, not sprites
5. **Animation data in ROM:** Compact animation tables in ROM, only active frame tiles in VRAM
6. **Shadow/highlight sprites:** Small circle shadow under characters (8×8 or 16×8 sprite), often semi-transparent mode

---

## Quick Reference Summary

| Aspect | Limit/Recommendation |
|--------|---------------------|
| Max hardware sprites | 128 OBJ_ATTRs |
| Max affine matrices | 32 OBJ_AFFINEs |
| Max sprite size | 64×64 pixels |
| OVRAM total | 32KB (16KB in bitmap modes) |
| Sprite pixels/scanline | ~960 (regular), ~480 (affine) |
| Palette (4bpp) | 16 banks × 16 colors |
| Palette (8bpp) | 1 bank × 256 colors |
| Recommended char size | 16×16 to 32×32 |
| Walk cycle frames | 4-6 per direction |
| Animation fps | 10-15 fps typical |
| OAM update | VBlank only, via DMA shadow copy |

---

*Sources: TONC (coranac.com/tonc), GBATek, training knowledge from GBA homebrew community*
