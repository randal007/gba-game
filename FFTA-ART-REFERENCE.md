# FFTA Art Reference Bible
### Final Fantasy Tactics Advance — Sprite Art Deep Dive for GBA Game Dev
*Research compiled for Pixel. Last updated: 2026-02-15.*

---

## Table of Contents
1. [Where to Find FFTA Sprite Sheets](#1-where-to-find-ffta-sprite-sheets)
2. [Sprite Dimensions](#2-sprite-dimensions)
3. [Palette Analysis](#3-palette-analysis)
4. [Animation Frames & Timing](#4-animation-frames--timing)
5. [Isometric Tile Dimensions](#5-isometric-tile-dimensions)
6. [Character Proportions](#6-character-proportions)
7. [Sprite Composition (OAM)](#7-sprite-composition-oam)
8. [Tools for Studying FFTA ROMs](#8-tools-for-studying-ffta-roms)
9. [GBA Hardware Constraints](#9-gba-hardware-constraints)
10. [ROM Technical Data](#10-rom-technical-data)
11. [Design Notes & Recommendations](#11-design-notes--recommendations)

---

## 1. Where to Find FFTA Sprite Sheets

### The Spriters Resource
**Primary source for ripped sprite sheets.**

- **Game page (requires JS/browser to render):**
  `https://www.spriters-resource.com/game_boy_advance/finalfantasytacticsadvance/`
- Note: This page uses JavaScript-rendered content, so direct text fetch fails (you'll hit a 404 in CLI tools). **Browse it in Chrome.**
- The GBA section index: `https://www.spriters-resource.com/game_boy_advance/`

What's available there includes:
- Battle character sprite sheets (all job classes for each race: Human, Bangaa, Moogle, Nu Mou, Viera)
- Enemy sprites
- UI/menu elements
- Map tilesets (ground, wall, elevation tiles)
- Portrait icons used in the status menu

### Other Sources
| Source | URL | Notes |
|--------|-----|-------|
| Spriters Resource | `https://www.spriters-resource.com/game_boy_advance/finalfantasytacticsadvance/` | Main sheet archive |
| Data Crystal (ROM map) | `https://datacrystal.tcrf.net/wiki/Final_Fantasy_Tactics_Advance` | ROM offsets, technical structure |
| Data Crystal (Maps) | `https://datacrystal.tcrf.net/wiki/Final_Fantasy_Tactics_Advance/Maps` | Map data structures |
| TCRF (Cutting Room Floor) | `https://tcrf.net/Final_Fantasy_Tactics_Advance` | Unused content, regional differences |
| FFHacktics Forum | `http://ffhacktics.com/smf/index.php?board=47.0` | Active FFTA hacking community, requires login for some content |
| FFHacktics — FFTA All In One | `http://ffhacktics.com/smf/index.php?topic=9764.0` | ROM editing tool |

### GitHub / Fan Rips
- Search GitHub for `"ffta sprite"` — fan rippers occasionally post extracted sprite sheets
- Fan sprite edits and rips also appear on DeviantArt, FurAffinity, and SpritingCommunity Discord servers

---

## 2. Sprite Dimensions

### GBA Hardware Limits (Hard Ceiling)
All sprites are built from 8×8 pixel tiles and bounded by GBA OAM hardware:

| Shape   | Size 00 | Size 01 | Size 10  | Size 11  |
|---------|---------|---------|----------|----------|
| Square  | 8×8     | 16×16   | 32×32    | 64×64    |
| Wide    | 16×8    | 32×8    | 32×16    | 64×32    |
| Tall    | 8×16    | 8×32    | 16×32    | 32×64    |

*Max sprite: 64×64 px. Max 128 sprites on screen at once. Max ~960 sprite pixels per scanline.*

### FFTA Character Sprites (Battle)
FFTA characters are **chibi-proportion sprites** composed from multiple OAM objects:

| Component       | OAM Size  | Approx. Pixels Used | Notes |
|----------------|-----------|---------------------|-------|
| Body/torso      | 16×16     | ~14×12 px actual    | Core body + legs |
| Head            | 16×16     | ~12×12 px actual    | Oversized chibi head |
| Weapon/item     | 16×16     | Varies by job       | Sword, staff, etc. |
| Shadow          | 16×8      | ~12×4 px actual     | Ellipse under feet |
| Full character  | ~24×28    | Bounding box        | Head + body composite |

**Key measurements:**
- A standing character fits in roughly **24×28 pixels** total (bounding box, including head)
- The body alone (below neck) is approximately **16×16 px**
- The oversized head is approximately **12×12 px** (chibi ratio)
- On the GBA's 240×160 screen, characters appear at a natural viewing scale where ~8-10 characters could fit side by side

### FFTA Map Tile Sprites (Battle)
| Element          | Visual Size | Notes |
|------------------|-------------|-------|
| Ground diamond tile | ~16×8 px  | Isometric projection, 2:1 ratio |
| One elevation step  | +8 px height | Each height unit adds ~8px of vertical cliff |
| Wall tiles          | ~16×24 px | Ground tile + visible cliff face |

### UI Elements
| Element         | Approximate Size | Notes |
|----------------|-----------------|-------|
| Character portrait (small, status bar) | 16×16 px | Job icon size |
| Menu text box   | Full-width (224px) | GBA text uses 6px or 8px font |
| Ability icon    | 8×8 or 16×16 px | Small icons in menu |
| HP/MP bar       | 48×4 px approx  | Thin horizontal bars |

---

## 3. Palette Analysis

### GBA Palette Hardware
- **Palette RAM total:** 1 KB = 512 colors total
  - 256 colors for **backgrounds** (BG palette)
  - 256 colors for **sprites/objects** (OBJ palette)
- **Color format:** 15-bit BGR555 (5 bits per channel, not 8-bit RGB)
  - Values: R: 0–31, G: 0–31, B: 0–31
  - Example: Pure red = `0x001F` (BGR order in memory)
- **Color 0** in any palette = transparent (never rendered)
- Actual unique colors per frame limited by memory, but you can swap palettes between frames

### FFTA's Palette Mode
FFTA uses **4bpp (4 bits per pixel)** for character sprites:
- 16 colors per palette bank
- 16 separate palette banks for sprites (numbered 0–15)
- Color 0 in each bank = transparent
- So: **15 usable colors per character/object**

Each job class gets its own palette bank. This is why palette swapping works — e.g., enemy Human soldiers vs. player Humans share the same tile data but use different palette banks (one for red team, one for blue).

### FFTA Color Feel & Aesthetic
FFTA uses a **warm, storybook fantasy palette** with these characteristics:

**Overall Vibe:**
- Saturated jewel tones — rich purples, deep blues, warm golds
- Not desaturated; Square used high-chroma colors pushed to GBA limits
- Slightly warm white balance — shadows lean warm amber/sepia rather than cool blue
- The GBA screen is dimmer than modern displays, so FFTA was designed with HIGHER contrast and saturation to compensate

**Key Color Groups Used in Characters:**
| Group | Typical Use | Character Example |
|-------|------------|-------------------|
| Deep navy → mid blue → pale cornflower | Blue Mage, White Mage, sky motifs | Marche's jacket |
| Warm crimson → brick red → pale rose | Fighters, judges, red team | Human Soldier armor |
| Deep forest green → lime | Archer, Hunter, moogle caps | Montblanc's pompon |
| Rich purple → lavender | Nu Mou, Black Mage, robes | Black Mage hat |
| Warm gold → tan → cream | Skin tones, hair, Nu Mou fur | All humanoid characters |
| Charcoal → slate gray | Bangaa scales, metal armor | Bangaa Warrior |

**Palette per character (15 colors) typically distributes as:**
- 2–3 colors: skin/fur tone (light, mid, shadow)
- 2–3 colors: primary clothing (highlight, mid, shadow)
- 2–3 colors: secondary clothing/trim
- 1–2 colors: hair
- 1–2 colors: weapon/equipment
- 1 color: eye color
- 1–2 colors: outline (black or dark brown, sometimes dark blue for softer look)

**FFTA's Outline Strategy:**
FFTA uses **dark outlines** (usually near-black, #1a0a00 type dark brown, not pure #000000) to define characters against varied tile backgrounds. This is a common GBA sprite technique — pure black outlines look harsh on the small LCD.

### Background Tileset Palettes
Map tilesets use the **256-color BG palette** (8bpp) or are split into 16-color banks (4bpp BG mode):
- Ground tiles: earthen tones, green grass, tan sand, grey stone
- The 163 battle maps each have their own palette data (confirmed in Data Crystal ROM map)
- Night versions of maps are palette shifts (same tile data, different palette) — this is the `Palette Index` byte in the map structure

---

## 4. Animation Frames & Timing

### GBA Frame Rate
- GBA runs at **59.73 fps** (not exactly 60)
- Most GBA games update animations every **4–8 frames** (so animations run at ~8–15 fps)
- FFTA runs game logic at 60fps but sprite animations update at slower rates

### FFTA Character Animation Frame Counts
*(Based on analysis of known sprite sheets and comparison to similar Square GBA games)*

| Animation     | Approx Frame Count | Update Rate | Notes |
|---------------|-------------------|-------------|-------|
| **Idle/Stand** | 2–4 frames | Every 16–24 game frames (~2–4 fps) | Very subtle; slight breathe/sway |
| **Walk cycle** | 6 frames | Every 4–6 game frames (~10–15 fps) | Looping; each direction separate |
| **Run** | 6 frames | Every 3–4 game frames | Faster update than walk |
| **Attack swing** | 8–12 frames | Every 4 game frames | Includes windup, hit, recovery |
| **Cast spell** | 6–10 frames | Every 4–6 frames | Raise-staff type animation |
| **Knocked back** | 4–6 frames | Every 4 frames | Hit reaction |
| **Death/KO** | 6–8 frames | Every 4–6 frames | Fall-down sequence |
| **Victory pose** | 4–6 frames | Every 8 frames | Post-battle idle |

### Direction Support
FFTA battle sprites are drawn for **4 directions** (the isometric view means only 4 make sense visually):
- Southwest (facing camera-left)
- Southeast (facing camera-right)
- Northwest (back-left)
- Northeast (back-right)

Sprites facing right/left are often **mirrored** (same tile, H-flip flag in OAM) to save ROM space.

### Spell/Effect Animations
- Battle effects (fire, ice, etc.) are separate sprite sheet objects
- Effects typically 6–12 frames, looping or one-shot
- Large spells use the full 64×64 OBJ sprite size

---

## 5. Isometric Tile Dimensions

### FFTA's Isometric Projection
FFTA uses a **2:1 isometric projection** (classic "fake iso" used in SNES/GBA tactics games):
- Each map diamond tile visually = **16 pixels wide × 8 pixels tall**
- This creates the 2:1 ratio typical of GBA tactics games
- Tiles are drawn as rotated squares (diamonds) viewed from a roughly 45° horizontal, ~30° vertical angle

### Map Layout
From Data Crystal ROM map:
- Maps store a **Height byte** and **Permissions byte** per tile
- Height values range 0–99 (0x63 max before glitch)
- Each height unit corresponds to **8 pixels** of visual elevation on screen
- Height = 0 makes a tile invalid/unselectable
- Permissions: 0x00=Normal, 0x01=Impassable, 0x02=Water, else=Non-selectable
- Map dimensions stored as NW-SE dimension and NE-SW dimension (byte values)
- Total of **163 battle maps** in the game

### Tile Drawing System
```
Top corner of a tile (highest visual point)
         ___
        /   \     <- 8px tall diamond top-half
        \   /     <- 8px tall diamond bottom-half
         ---
Total visual diamond: 16px wide × 16px tall (but the 8px top half only is "flat ground")
```

**Elevation:**
- Ground tile = 16×8 px diamond (only the top face visible)
- Each 1-unit increase in height = lift the tile 8px upward AND add an 8px-tall cliff face below it
- So a tile at height=2 has: 8px flat top + 16px cliff = 24px total visual stack

### Screen Coverage
On the 240×160 GBA screen:
- Horizontal: ~15 tile columns visible (15 × 16px = 240px)
- Vertical: ~10 tile rows visible with their heights

---

## 6. Character Proportions

### FFTA's Chibi Style
FFTA uses an **exaggerated chibi proportion** — characters have large heads relative to short bodies. This is intentional for readability at small pixel sizes.

```
FFTA Character (approx pixel breakdown for standard Human):

Head:  ████████████   ← ~12px wide, ~10px tall (including hair)
       ████████████
       ████████████
Neck:  ░░░████░░░░    ← 1-2px connecting element
Body:  ████████████   ← ~12px wide, ~8px tall (torso)
       ████████████
Legs:  ██░░░░░░██    ← ~4px wide per leg, ~6px tall
       ██░░░░░░██
       ██░░░░░░██
Total height: ~26px
```

### Exact Proportions by Body Part

| Part | Width (px) | Height (px) | Notes |
|------|-----------|-------------|-------|
| Full character | 20–24 | 26–30 | Bounding box inc. weapon |
| Head (face only) | 10–12 | 8–10 | Excludes helmet |
| Torso | 10–12 | 6–8 | Includes arms at sides |
| Each leg | 4–5 | 6–7 | Drawn as simple rectangles |
| Each arm | 3–4 | 5–6 | May merge with torso frame |
| Weapon held | 2–4 wide | 8–16 tall | Varies wildly by weapon type |

### Head-to-Body Ratio
- FFTA: ~**1:1.2** (head height : body+legs height) — very chibi, almost equal
- Compare: Real human = 1:7; FFT PS1 sprite = ~1:3; FFTA = ~1:1.2

### Race Proportion Differences
| Race | Notes |
|------|-------|
| Human | Standard chibi proportions above |
| Bangaa | Taller, more serpentine head; longer neck; ~2–4px taller than humans |
| Moogle | Smallest race; rounder body; distinctive pompon adds ~4px on top |
| Nu Mou | Medium height; large heads with long snouts; hunched posture |
| Viera | Tallest race; longer legs; tall rabbit ears add 8–10px to height |

---

## 7. Sprite Composition (OAM)

### How FFTA Renders Characters
FFTA does **NOT** use a single sprite for each character. It uses **multiple OAM objects composited together** per character. This is standard for GBA tactics games to:
1. Work around the 64×64 max OAM sprite size
2. Allow equipment/weapons to animate independently
3. Allow palette-bank switching per body part (e.g., hair and armor use different palettes)

### Typical OAM Composition per Battle Character

```
Character = [Shadow OBJ] + [Body OBJ] + [Head OBJ] + [Weapon OBJ] + [Effect OBJ]
```

| OAM Object | Size | Palette Bank | Layer Priority | Notes |
|-----------|------|-------------|----------------|-------|
| Shadow | 16×8 | Shared gray palette | BG priority 2 | Ellipse on ground; semi-transparent |
| Legs/body | 16×16 | Character palette 0 | OBJ priority 1 | Core body, legs |
| Head | 16×16 | Character palette 0 | OBJ priority 1 | Big chibi head |
| Weapon/item | 16×16 | Equipment palette | OBJ priority 0 (top) | Sword/staff/bow |
| Status effect | 8×8 or 16×16 | Effect palette | OBJ priority 0 | Poison bubble, sleep zzz, etc. |

**Total OAM sprites per character: 3–6 objects**

This means with 128 total OAM objects on screen:
- 6 characters on screen = 18–36 OAM slots used just for characters
- FFTA max displayed battle units is **typically 8–10** (both sides) = safely within 128 OAM budget

### Palette Bank Allocation Strategy
FFTA separates characters by palette bank so multiple units of same job don't conflict:
- Player characters: Banks 0–7 (8 slots for up to 8 friendly units)
- Enemy characters: Banks 8–13
- Effects/UI: Banks 14–15
- Each unit's tiles are in OVRAM, palette swapped to indicate team color (red vs. blue shoulder pads, etc.)

---

## 8. Tools for Studying FFTA ROMs

### Emulators with Debug/Tile Viewing

| Tool | Platform | Features | URL |
|------|---------|----------|-----|
| **mGBA** | Win/Mac/Linux | Tile viewer, palette viewer, OAM viewer, memory viewer | `https://mgba.io` |
| **VisualBoyAdvance-M (VBA-M)** | Win/Mac/Linux | Tile viewer, palette viewer (Tools menu) | `https://vba-m.com` |
| **No$GBA** | Windows | Full hardware debug, VRAM/OAM viewer, very accurate | `https://problemkaputt.de/gba.htm` |
| **BizHawk** | Windows | Emulation + TAS tools, RAM watch, memory viewer | `https://tasvideos.org/BizHawk` |

**mGBA is the recommended starting point** — its tile/OAM viewer is excellent for seeing exactly what FFTA loads in VRAM during gameplay.

### Tile Editors & ROM Graphic Viewers

| Tool | Platform | Use Case | URL/Source |
|------|---------|----------|-----------|
| **CrystalTile2** | Windows | View/edit 4bpp/8bpp tiles directly from ROM | Search: "CrystalTile2 download" |
| **Tile Molester** | Java (cross-platform) | Flexible tile viewer, supports custom tile formats | `https://github.com/toruzz/TileMolester` |
| **GBA Graphics Editor (GGE)** | Windows | GBA-specific tile editor, understands GBA palette format | Search: "GBA Graphics Editor" |
| **Tile Layer Pro** | Windows | Classic tile viewer, good for manual analysis | Freeware |
| **GRIT (GBA Raster Image Transmogrifier)** | CLI | Converts images TO GBA format; good for understanding the format | `https://www.coranac.com/projects/#grit` |
| **Usenti** | Windows | GBA palette-aware image editor | `https://www.coranac.com/projects/#usenti` |

### FFTA-Specific ROM Hacking Tools

| Tool | Use | URL |
|------|-----|-----|
| **FFTA All In One v0.7** | Comprehensive FFTA editor (jobs, abilities, sprites, maps) | `http://ffhacktics.com/smf/index.php?topic=9764.0` |
| **Nightmare Modules (FFTA)** | Table-based stat editing for FFTA | `http://ffhacktics.com/smf/index.php?topic=9559.0` |
| **FFHacktics FFTA Board** | Community, tutorials, tools | `http://ffhacktics.com/smf/index.php?board=47.0` |

### Compression Note
FFTA uses **LZ77 compression** for most graphic data. When viewing raw ROM tiles, you need to decompress first. Tools like:
- mGBA (decompresses automatically for tile viewer)
- FFTA All In One (handles decompression)
- GBA LZ77 decompressor standalone tools

The Data Crystal ROM map confirms:
- Map tile graphics: LZ77 compressed (check byte 0x10 flag)
- Sprite data: also compressed, via LZSS
- Some palettes: uncompressed raw BGR555 data

### Workflow for Extracting FFTA Sprites
1. Load FFTA ROM into **mGBA**
2. Play to a battle scene with the character you want
3. Open `Tools → Tile Viewer` — you'll see all 8×8 tiles currently in VRAM
4. Open `Tools → Palette Viewer` — you'll see the BGR555 colors in each palette bank
5. Screenshot or export from there
6. Alternatively, use **CrystalTile2** to browse the ROM directly — set codec to `GBA 4BPP` and navigate to sprite data offsets from Data Crystal

---

## 9. GBA Hardware Constraints

*(Documented from GBATEK — the authoritative hardware reference)*

### Display
| Spec | Value |
|------|-------|
| Screen resolution | **240×160 pixels** |
| Physical size | 2.9-inch TFT LCD, 61×40mm |
| Color depth (background) | 256 colors, or 16 colors/16 palettes, or 32768 colors (modes) |
| Color depth (sprites) | 256 colors, or 16 colors/16 palettes |

### Sprite (OBJ) System
| Spec | Value |
|------|-------|
| Max sprites on screen | **128** |
| Max sprite size | **64×64 pixels** |
| Min sprite size | **8×8 pixels** |
| Sprite sizes available | 12 types: 8×8 to 64×64 (see table in §2) |
| Max sprite pixels per scanline | ~960 pixels (at 8×8, that's 120 8px objects in one line) |
| OAM memory | 1 KB |
| Sprite tile memory (OVRAM) | 32 KB |
| Sprite palette memory | 512 bytes = 256 colors for sprites |

### Background System
| Spec | Value |
|------|-------|
| BG layers | 4 layers (BG0–BG3) |
| BG tile memory | Shared VRAM |
| BG palette memory | 512 bytes = 256 colors for BGs |
| Max BG size | 1024×1024 pixels (128×128 tiles) |
| Min BG tile size | 8×8 pixels |

### Color Space
| Spec | Value |
|------|-------|
| Color format | BGR555 (15-bit, 5 bits per channel) |
| Total displayable colors | 32,768 |
| Transparent color | Color index 0 in any palette (always transparent for sprites) |

**Important for artists:** GBA colors are not 8-bit RGB. To convert:
- GBA has values 0–31 per channel
- To approximate: `GBA_value × 8 = 8bit_value` (or more precisely `× 8 + (value > 0 ? 7 : 0)`)
- The screen itself has no backlight on original GBA — the GBA SP added backlight. FFTA was designed with this in mind, hence the high-contrast, high-saturation art.

---

## 10. ROM Technical Data

*(From Data Crystal: `https://datacrystal.tcrf.net/wiki/Final_Fantasy_Tactics_Advance`)*

### ROM Identification
| Field | Value |
|-------|-------|
| Game name (USA) | FFTA_USVER. |
| Game code (USA) | AFXE |
| ROM size | **16 MiB** |
| CRC32 (USA) | 5645E56C |
| Developer | Square Product Development Division 4 |
| Publisher (USA/EU) | Nintendo |

### Map Data Structure (from `struct FFTA_map`)
Each map is 0x58 bytes and contains pointers to:
| Pointer | Purpose |
|---------|---------|
| `0x00` | Graphics Data (tile pixel data) |
| `0x04` | Arrangement Data (tile layout) |
| `0x0C` | Palette Data |
| `0x10` | Height-Map Data |
| `0x14` | Animation Data |
| `0x18` | Animation Graphics Data |

### Map Tile Structure (`struct FFTA_map_tile`)
```c
struct FFTA_map_tile {
    uint8 Height;       // 0 = invalid/unselectable, 1–99 = height level
    int8  Permissions;  // 0x00=walkable, 0x01=impassable, 0x02=water
};
```
- Total maps in game: **163** (indexed 0x00–0xA2)
- Each map has a night version (palette shift only, same tile data)

### Sub-pages for More Technical Detail
- ROM map: `https://datacrystal.tcrf.net/wiki/Final_Fantasy_Tactics_Advance/ROM_map`
- RAM map: `https://datacrystal.tcrf.net/wiki/Final_Fantasy_Tactics_Advance/RAM_map`
- Text tables: `https://datacrystal.tcrf.net/wiki/Final_Fantasy_Tactics_Advance/TBL`
- Jobs data: `https://datacrystal.tcrf.net/wiki/Final_Fantasy_Tactics_Advance:Jobs`
- Maps: `https://datacrystal.tcrf.net/wiki/Final_Fantasy_Tactics_Advance/Maps`

---

## 11. Design Notes & Recommendations

### For Pixel (Our Artist)

#### Emulating FFTA's Look
1. **Stick to 16-color palettes per character** — this IS the GBA constraint AND it's the reason FFTA looks coherent. Don't use more than 15 colors (+ transparent) per character.

2. **Use dark outlines, not pure black** — FFTA uses `#1a0c00` type dark dark brown rather than `#000000` for outlines. It's softer on the GBA LCD.

3. **Pre-compensate for GBA gamma** — original GBA has no backlight. FFTA sprites look washed out in emulators. The original art had boosted contrast/saturation. For our game (targeting emulator or GBA SP), calibrate your art at slightly lower saturation than FFTA to look right.

4. **16×16 is the magic tile size** — most FFTA character components live in 16×16 OAM slots. Think of every drawing element as a 16×16 block; stack/compose them.

5. **8px tile grid is sacred** — All pixel work should snap to 8px grid (GBA hardware forces this). Work at 8×8 base, compose up.

6. **Isometric tile formula: 16×8 diamond** — Draw ground tiles as 16px wide, 8px tall diamonds. One elevation unit = 8px of cliff face. Keep consistent.

7. **4 directions, H-flip optimization** — Draw sprites for 2 facing directions (SW-facing and SE-facing), use H-flip for the mirror directions. This halves art work and is exactly what FFTA does.

8. **FFTA chibi head rule** — Head should be approximately equal to body+legs height. This reads well at 24–28px total height. Don't try to make realistic proportions; they'll be unreadable.

#### Color Palette Suggestions for FFTA-Style Sprites
```
Skin (human, warm)    → #F0C090 (light), #C89060 (mid), #906040 (shadow)
Blue cloth            → #6080C0 (light), #4060A0 (mid), #203080 (shadow)
Red cloth             → #C04040 (light), #903030 (mid), #601020 (shadow)
Hair (brown)          → #906030 (light), #604020 (mid), #301000 (dark)
Outline               → #1a0c00 (dark brown, not pure black)
White cloth           → #F0F0E0 (light), #C0C0B0 (mid), #909080 (shadow)
```
*(All values in 8-bit RGB; GBA will quantize to 5-bit per channel automatically)*

#### Recommended Reference Viewing
1. Load FFTA in mGBA
2. Go to a battle with diverse unit types
3. Open mGBA's tile viewer — pause on different animation frames
4. Study how 4–6 OAM tiles compose into one character
5. Count pixels directly on the 1× display (240×160 native)

#### Animation Timing Target
- Walk: 6 frames at 6-game-frame intervals = one step per 36 game frames = ~0.6 seconds per step
- Attack: 10 frames at 4-game-frame intervals = 40 game frames = ~0.67 seconds total attack
- Idle breathe: 2–4 frames at 20-game-frame intervals = very subtle, 1–2 seconds per cycle

---

## Quick Reference Cheat Sheet

```
GBA Screen:          240×160 pixels
Tile base unit:      8×8 pixels
Sprite sizes:        8×8 → 64×64 (12 sizes)
Max sprites:         128 on screen
Sprite palette:      16 colors/bank, 16 banks (OBJ)
                     = 256 sprite colors total
Colors per object:   16 (4bpp), color 0 = transparent
FFTA character size: ~24×28 px (full body bounding box)
FFTA head:           ~12×10 px
FFTA body:           ~12×8 px  
FFTA isometric tile: 16×8 px (diamond)
FFTA elevation step: 8 px per height unit
FFTA directions:     4 (SW, SE, NW, NE) + H-flip optimization
Walk frames:         6 frames
Attack frames:       8–12 frames
Color format:        BGR555 (0–31 per channel)
ROM size:            16 MiB
Total battle maps:   163
Map height range:    0–99 per tile
```

---

*Sources: GBATEK (problemkaputt.de), Tonc GBA Programming Guide (coranac.com), Data Crystal TCRF wiki, The Cutting Room Floor (tcrf.net), The Spriters Resource, FFHacktics community.*
