# GBA Game — Status

## Current Version: v0.3 (in progress)

### v0.3 — Collision, Jump, Fall, Occlusion
**Status:** ✅ Initial implementation complete

#### What's done:
1. **Wall collision** — player is blocked from walking into tiles higher than their current height. Uses world-to-tile conversion to check destination tile height before allowing movement.
2. **Jump (A button)** — press A to hop up 1 height level onto the adjacent tile in the player's facing direction (only if exactly 1 higher). Includes a parabolic visual hop animation (16 frames, 12px peak).
3. **Fall** — when walking off an edge to a lower tile, the player drops down with a visual fall animation (3px/frame descent).
4. **Occlusion** — when the player is behind a taller tile (in front of camera), the sprite priority is set to 2 (behind BG at priority 1), causing the player to be hidden behind walls and structures.

#### Technical details:
- Added `world_to_tile()` helper for converting world pixel coords to map col/row
- Player struct now tracks `tile_col`, `tile_row`, `height`, jump/fall state
- Camera Y target accounts for height offset so view follows player up/down elevation
- Height-based sprite Y offset: sprite drawn at `world_y - height * SIDE_HEIGHT`
- Jump/fall lock out movement during animation to prevent glitches

### Previous versions:
- **v0.2** ✅ — Metatile engine, tile stacking, iso parallelogram walls, Pixel's art
- **v0.1** ✅ — Iso tilemap, hardware scrolling, player movement, 200×16 world
