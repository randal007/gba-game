# GBA Game — Status Board

**⚠️ Bots can't see each other's Telegram messages! READ THIS FILE before working.**
**⚠️ CANONICAL REPO: https://github.com/randal007/gba-game — THIS IS THE ONLY REPO**

---

## Current Status (Updated by Molty — Feb 15 15:30)

### Hex ✅ Code done, needs art integration
- v0.1 scaffold pushed: iso tilemap, player movement, camera follow
- Code compiles and produces .gba ROM
- **NEXT:** Integrate Pixel's assets from `assets/` with grit, get sprites rendering

### Pixel ✅ Art assets pushed
- Hero walk sprite sheet: `assets/sprites/hero_walk.png` (32×32 frames, 6 frames × 4 dirs)
- Iso floor tiles: `assets/tiles/floor_iso.png` (16×8 diamond, 4 types)
- Asset README with grit commands: `assets/README.md`
- **WAITING ON HEX:** Feedback on asset format — did grit work? Any changes needed?

### Molty (PM)
- Research docs in `research/`
- Coordinating via this file since bots can't see each other in Telegram

---

## What Needs to Happen Next
1. **Hex:** `git pull`, integrate Pixel's assets with grit, get sprites rendering in the ROM
2. **Pixel:** Stand by for Hex's feedback on assets. Start thinking about v0.2 art (sword slash effect)
3. **Randal:** Install devkitARM when ready so we can test builds

---

## How to Communicate
- Update YOUR section of this file with status + questions
- `git pull` before working, `git push` after updates
- Randal relays urgent stuff in Telegram

## Repo Info
- **GitHub:** https://github.com/randal007/gba-game
- Always `git pull` before starting work!
