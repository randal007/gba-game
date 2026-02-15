# GBA Isometric Game Research

*Compiled: 2026-02-15*

## 1. Open Source Isometric GBA Games & Demos

### FarmSimAdvance (Assembly)
- **Repo:** https://github.com/NotImplementedLife/FarmSimAdvance
- **Description:** Isometric farm simulator on GBA. Written in ARM Assembly.
- **Status:** Archived (Dec 2022), 31 commits, 2 stars
- **Notes:** The only explicitly isometric GBA project found on GitHub. Small codebase, good for studying raw hardware-level isometric tile rendering without library abstractions.

### Skyland GBA (C++)
- **Repo:** https://github.com/evanbowman/skyland-gba
- **Description:** Realtime strategy game for GBA with an **isometric 3D island building macrocosm mode**
- **Features:** Roguelike adventure, isometric building, multiplayer, integrated LISP interpreter, event-driven engine, Unicode support
- **License:** MPL 2.0
- **Notes:** Very mature project. The "macro" mode uses isometric perspective for island construction. Built on devkitARM. Excellent reference for camera management, tile-based building systems, and game architecture on GBA.

### Blind Jump Portable (C++)
- **Repo:** https://github.com/evanbowman/blind-jump-portable
- **Description:** Action/adventure with procedurally generated levels. Same author as Skyland.
- **Notes:** Not isometric per se, but uses similar engine patterns. Good reference for sprite management, animation, and cross-platform GBA engine design.

### IsometricEngine (Java)
- **Repo:** https://github.com/patn8092/IsometricEngine
- **Description:** Basic isometric tile engine, appears GBA-targeted in concept
- **Notes:** Java prototype, not directly usable but may have algorithmic reference value

## 2. Key GBA Development Libraries & Frameworks

### Butano (Recommended — Modern C++ Engine)
- **Repo:** https://github.com/GValiente/butano
- **Description:** Modern C++ high-level engine for GBA
- **Key Features:**
  - One-line sprite/background/text creation
  - Asset import pipeline
  - RAII-based resource management (no heap allocs, no exceptions)
  - Mode 7 support (pseudo-3D, useful for isometric-like effects)
  - Multiple full game examples included
  - Detailed documentation
- **Built on:** devkitARM or Wonderful Toolchain, Tonclib, Maxmod
- **License:** zlib
- **Why it matters:** Highest-level abstraction available. Handles VRAM management, sprite allocation, background scrolling, and affine transformations. Could significantly reduce boilerplate for an isometric game.

### DUsK (C)
- **Repo:** https://github.com/bmchtech/dusk
- **Description:** Simple, lightweight GBA dev framework
- **Features:** Scene architecture, 8bpp texture atlas packing, sprite/animation helpers, Tiled map exporter/loader
- **Notes:** Good middle-ground between raw libtonc and Butano. The Tiled integration is valuable for isometric map editing.

### libtonc / Tonc Tutorial
- **URL:** https://gbadev.net/tonc/
- **Description:** The definitive GBA programming tutorial, covers everything from basics to advanced
- **Topics:** Tile modes, sprites (OAM), affine transformations, DMA, interrupts, sound
- **Notes:** Essential reading regardless of which framework you use. Updated community version maintained at gbadev.net.

### Other Notable Libraries
| Library | Language | URL | Use Case |
|---------|----------|-----|----------|
| rust-console/gba | Rust | https://github.com/rust-console/gba | GBA in Rust + tutorial |
| agb | Rust | https://github.com/agbrs/agb | High-level Rust GBA lib |
| natu | Nim | https://github.com/exelotl/natu | Nim wrapper around libtonc |
| gba-modern | C++ | https://github.com/JoaoBaptMG/gba-modern | Modern C++ GBA patterns |
| sdk-seven | C | https://github.com/sdk-seven | Modern libgba/libtonc replacement |
| libseven | C | https://github.com/sdk-seven/libseven | From-scratch low-level lib |

## 3. Technical Patterns for Isometric on GBA

### Tile Rendering
- GBA has 4 background layers in tile mode (Mode 0), each with hardware scrolling
- Each BG layer is a tilemap of 8×8 pixel tiles; max 1024 unique tiles per character block
- **Isometric approach:** Use Mode 0 with a pre-rendered isometric tileset. Map world coordinates to screen via standard iso transform: `screen_x = (world_x - world_y) * tile_half_width`, `screen_y = (world_x + world_y) * tile_half_height`
- Affine backgrounds (Mode 1/2) allow rotation/scaling — could enable true isometric rotation but limited to 1-2 affine BGs
- Mode 7 (affine BG) can create pseudo-3D floor effects (Butano has examples)

### Sprite Management
- GBA OAM supports 128 sprites (hardware objects)
- Sprites can be 8×8 up to 64×64 pixels
- VRAM for sprites is limited (~32KB in 1D mapping mode)
- **VRAM streaming:** For many animated sprites, swap tile data in/out of VRAM each frame via DMA
- **Sprite sorting:** For isometric depth, sort sprites by Y-position (or iso depth) each frame before writing to OAM
- Metasprites: Combine multiple hardware sprites for larger characters (common pattern)

### Camera/Scrolling
- Hardware BG scroll registers (REG_BGxHOFS/VOFS) provide free per-pixel scrolling
- For isometric: scroll the background layers and offset sprite positions by camera delta
- Split the world into screen-sized chunks; only load visible tilemap data
- Use DMA to update tilemap entries when scrolling reveals new areas

### Memory Constraints
- 96KB VRAM total (64KB BG tiles + 32KB sprite tiles in typical config)
- 256KB IWRAM + 32KB fast EWRAM
- Plan tile budget carefully: isometric tiles are often larger logical tiles composed of multiple 8×8 hardware tiles
- Palette: 256 colors (or 16 palettes × 16 colors in 4bpp mode)

## 4. Community Resources

### Primary Hubs
- **gbadev.net** — Main community site, hosts Tonc, resource lists, game jams
- **gbadev Discord** — https://discord.gg/ctGSNxRkg2 (most active real-time help)
- **gbadev Forum** — https://forum.gbadev.net (announcements, long-form discussion)
- **gbadev.org** — Legacy homepage, still updated with news

### Documentation
- **GBATEK** — https://problemkaputt.de/gbatek.htm (definitive hardware reference)
- **CowBite Spec** — https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm
- **Tonc** — https://gbadev.net/tonc/ (programming tutorial)

### Tooling
- **devkitARM** — https://devkitpro.org (standard C/C++ cross-compiler)
- **Tiled** — Map editor with GBA export support (via Tiled2GBA or custom exporters)
- **Maxmod** — Music/sound library (supports .mod, .xm, .s3m, .it)
- **mGBA** — Recommended emulator for development (has debugging features)

### Decompilation Projects (Study Reference)
- **pokeemerald** — https://github.com/pret/pokeemerald (full Pokémon Emerald decompilation, 2.9k stars)
  - While not isometric, it's the most complete open-source GBA game codebase
  - Excellent reference for overworld tile engines, sprite management, event systems
  - Shows professional-grade GBA patterns for maps, NPCs, scripting

### Game Jams
- GBA Jam 2021, GBA Jam 2022 — Hosted by gbadev.net, many open-source entries

## 5. Recommendations for Our Project

1. **Use Butano** as the primary framework — it handles the tedious VRAM/OAM management and lets us focus on game logic. Mode 7 examples could be adapted for isometric views.

2. **Study FarmSimAdvance** for raw isometric tile layout on GBA hardware (even though it's Assembly, the tile/map structure is instructive).

3. **Study Skyland's macro mode** for isometric building/camera patterns in a real shipped game.

4. **Read Tonc chapters** on: regular backgrounds (ch 9), regular sprites (ch 8), affine backgrounds (ch 12), and the mode 7 chapters for pseudo-3D.

5. **Use Tiled** for map editing — DUsK and Butano both have Tiled integration.

6. **Join gbadev Discord** for real-time help from experienced GBA developers.

7. **Tile budget planning** is critical — sketch out how many unique isometric tiles we need and verify they fit in VRAM before committing to an art style.
