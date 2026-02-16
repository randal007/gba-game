# GBA Isometric Action Game — Status

## Current: v0.1 — Walk the World

### Build
- **Status:** ✅ Compiles and links cleanly
- **ROM:** `isogame.gba`

### Recent Fixes
- **[2026-02-16] Fix crash-loop: broken double-buffering + rendering optimization**
  - `vid_mem_back` is a compile-time constant (always page 1 / 0x0600A000). After `vid_flip()`, the code kept drawing to the same page — sometimes the *displayed* page. This caused the screen to rapidly flash between a half-drawn frame and an uninitialized page, appearing as a crash loop while the OBJ sprite (hero) remained visible.
  - Fixed: manual page tracking with `back_id ^= 1` and direct `REG_DISPCNT ^= DCNT_PAGE` flipping.
  - Optimized `draw_iso_cube` to use horizontal span fills (`m4_hline_fast`) instead of per-pixel `m4_plot_page` calls — ~10x fewer VRAM writes per cube.
  - Tightened draw_map culling margins to skip off-screen tiles earlier.

- **[2026-02-15] Fix OBJ VRAM overlap** — moved hero tiles from charblock 4 to charblock 5 (tile ID 512+) since Mode 4 back framebuffer overlaps charblock 4.

### Known Limitations
- Mode 4 software-rendered isometric cubes are CPU-intensive. Currently viable with tight culling and span fills, but a future move to Mode 0 tiled rendering would be ideal for performance.
- Double-buffering now works correctly but drawing still takes significant CPU time per frame.

### Architecture
- **Mode 4** (8bpp bitmap) for isometric terrain rendering
- **OBJ sprites** for hero character (charblock 5, tile ID 512+)
- **16×16 tile map** with 4 terrain types (grass/stone/dirt/water)
- **24.8 fixed-point** world coordinates
- **Isometric 2:1** projection with camera follow
