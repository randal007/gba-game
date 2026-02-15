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

### v0.2 — Sword Attack
- Sword swing animation
- Hitbox spawns in facing direction
- Visual feedback (slash effect)

### v0.3 — First Enemy
- One enemy type (slime? bat?)
- Wander AI + chase when player is close
- Takes damage from sword, dies

### v0.4 — Jump + Height
- Jump mechanic with shadow
- Tiles at different heights
- Landing on elevated tiles

### v0.5 — Playable Area
- Multiple connected zones/rooms
- Screen transitions or scrolling
- Environmental variety (grass, stone, water)

### v1.0 — Mini Adventure
- Title screen
- A few areas to explore
- Multiple enemy types
- Pickups (hearts, items)
- Polish and juice

## Status
- [ ] devkitARM setup
- [ ] v0.1
- [ ] v0.2
- [ ] v0.3
- [ ] v0.4
- [ ] v0.5
- [ ] v1.0
