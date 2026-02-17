# GBA Game — Isometric Action-Adventure

## Concept
Isometric real-time action game in the style of FFTA's art direction, with Zelda-like gameplay.
Think: Link's Awakening meets Final Fantasy Tactics Advance visuals.

## Platform
- Game Boy Advance (ARM7TDMI, 240×160, 16.78MHz)
- Toolchain: devkitARM / libtonc
- Testing: mGBA emulator → EZ Flash cart on modded GBA

## Core Mechanics
- 4-direction isometric movement
- Sword slash (short range hitbox)
- Jump (visual hop + ground shadow)
- Enemies: wander + chase when close
- Health bar, pickups

## Art Style
- FFTA aesthetic: rich jewel tones, dark outlines, chibi proportions
- Isometric tile map with detailed environment tiles
- 32×32 character sprites, 16×16 or 16×8 map tiles
- 15-color palettes per sprite/tileset (GBA 4bpp)

## Milestones

### v0.1 — Walk the World
- Render isometric tile grid
- Character sprite walks in 4 directions
- Camera follows player
- D-pad controls

### v0.2 — Metatile Engine + Tile Stacking ✅
- Metatile-based renderer (instant boot, no pixel rendering)
- Height system (0-4) with stacking
- FFTA-style parallelogram wall faces
- Pixel's tile art integrated (ground + side tiles)
- 200×16 side-scroller world with hills, fortress, ruins

### v0.3 — Collision, Jump, Occlusion
- Wall collision — block movement into tiles higher than current
- Jump (A button) — hop up 1 height level onto adjacent tile
- Fall — walk off an edge → drop to lower tile height
- Occlusion — character hides behind walls/mountains in front (dual BG layers or OBJ window)

### v0.4 — Sword Attack
- Sword swing animation
- Hitbox spawns in facing direction
- Visual feedback (slash effect)

### v0.5 — First Enemy
- One enemy type (slime? bat?)
- Wander AI + chase when player is close
- Takes damage from sword, dies

### v0.6 — Playable Area
- Multiple connected zones/rooms
- Screen transitions or scrolling
- Environmental variety

### v1.0 — Mini Adventure
- Title screen
- A few areas to explore
- Multiple enemy types
- Pickups (hearts, items)
- Polish and juice

## Canonical Repo
**https://github.com/randal007/gba-game**
- Branch: `main` (everyone works here, no feature branches for now)
- Clone: `git clone https://github.com/randal007/gba-game.git`
- Always `git pull` before starting work, always `git push` when done

## Team
| Agent | Role | Commit prefix |
|-------|------|---------------|
| Hex | Code (C, devkitARM, libtonc) | `[Hex]` |
| Pixel | Art (sprites, tiles, palettes) | `🎨 [Pixel]` |
| Molty | PM, docs, planning | `[Molty]` |

## Status
- [x] devkitARM setup (gcc 15.2.0, mGBA installed)
- [x] v0.1 — iso tilemap, hardware scrolling, player movement, 200×16 world
- [x] v0.2 — metatile engine, tile stacking, iso parallelogram walls, Pixel's art
- [ ] v0.3 — collision, jump, fall, occlusion
- [ ] v0.4 — sword attack
- [ ] v0.5 — first enemy
- [ ] v0.6 — playable area
- [ ] v1.0 — mini adventure

## Side Projects
### HTML Level Editor (planned)
- Visual 16×200 grid editor in browser
- Drag-and-drop tile placement with height control
- Export to format convertible to game level data
- Serve via Canvas or local HTML file
