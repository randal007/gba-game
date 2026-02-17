// main.c — GBA Isometric Action Game
// Mode 0 hardware-tiled renderer with tilemap streaming for large worlds.
// A 512×320 pixel window slides over the world; re-renders when camera drifts.
#include "game.h"
#include "../data/hero_walk.h"
#include <string.h>

//=============================================================================
// Globals
//=============================================================================
static OBJ_ATTR obj_buffer[128];
static Player player;
static Camera camera;

// World map — generated procedurally at startup
EWRAM_BSS static u8 world_map[MAP_ROWS][MAP_COLS];

#define HERO_TILES_PER_FRAME  16
#define HERO_WALK_FRAMES      6
#define HERO_ANIM_SPEED       4

//=============================================================================
// EWRAM buffers for sliding-window renderer
//=============================================================================
EWRAM_BSS static u8 win_pixels[WIN_PX_W * WIN_PX_H];

// Tile dedup: each 8bpp 8×8 tile = 64 bytes = 16 u32s
EWRAM_BSS static u32 tile_store[MAX_BG_TILES * 16];
static int num_unique_tiles;

// Tilemap (64×64)
EWRAM_BSS static u16 tilemap[BG_MAP_W * BG_MAP_H];

// Current window center in world-pixel-space
static int win_cx, win_cy;

//=============================================================================
// Progressive re-render state
// Spreads the expensive re-render across multiple frames to avoid hitching.
// Phase 0: idle
// Phase 1: rendering iso cubes (RERENDER_CUBE_FRAMES frames)
// Phase 2: building tileset+map (RERENDER_TILE_FRAMES frames)
// Phase 3: upload to VRAM (1 frame)
//=============================================================================
#define RERENDER_CUBE_FRAMES  4
#define RERENDER_TILE_FRAMES  5

static int rerender_phase = 0;
static int rerender_step = 0;
static int rerender_target_cx, rerender_target_cy;

//=============================================================================
// Simple pseudo-random number generator
//=============================================================================
static u32 rng_state = 0xDEADBEEF;
static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

//=============================================================================
// Procedural world generation — 200×16 strip with varied terrain
//=============================================================================
static void generate_world(void) {
    // Fill with grass base
    memset(world_map, TILE_GRASS, sizeof(world_map));

    // Create terrain zones along the strip
    for (int col = 0; col < MAP_COLS; col++) {
        for (int row = 0; row < MAP_ROWS; row++) {
            // --- Stone paths/roads running through the middle ---
            if (row >= 6 && row <= 9) {
                // Winding stone road
                int road_center = 7 + ((col * 3 + col/7) % 4) - 1;
                int dist = row - road_center;
                if (dist < 0) dist = -dist;
                if (dist <= 1) {
                    world_map[row][col] = TILE_STONE;
                }
            }

            // --- Water features ---
            // River at col 40-55
            if (col >= 40 && col <= 55) {
                int river_center = 4 + ((col - 40) * 2 + 3) / 5;
                int dist = row - river_center;
                if (dist < 0) dist = -dist;
                if (dist <= 1) {
                    world_map[row][col] = TILE_WATER;
                }
            }

            // Lake at col 100-115, rows 8-14
            if (col >= 100 && col <= 115 && row >= 8 && row <= 14) {
                int cx = 107, cy = 11;
                int dx = col - cx, dy = row - cy;
                if (dx*dx + dy*dy*3 <= 30) {
                    world_map[row][col] = TILE_WATER;
                }
            }

            // Small pond at col 70-75
            if (col >= 70 && col <= 75 && row >= 1 && row <= 4) {
                int dx = col - 72, dy = row - 2;
                if (dx*dx + dy*dy <= 4) {
                    world_map[row][col] = TILE_WATER;
                }
            }

            // --- Dirt patches ---
            // Desert area col 130-160
            if (col >= 130 && col <= 160) {
                // Base dirt with some grass
                rng_state = (u32)(col * 31 + row * 97 + 12345);
                rng_next();
                if ((rng_next() % 100) < 60) {
                    if (world_map[row][col] == TILE_GRASS) {
                        world_map[row][col] = TILE_DIRT;
                    }
                }
            }

            // Dirt trail along bottom
            if (row >= 13 && row <= 15 && col >= 20 && col <= 90) {
                rng_state = (u32)(col * 17 + row * 53);
                rng_next();
                if ((rng_next() % 100) < 50) {
                    if (world_map[row][col] == TILE_GRASS) {
                        world_map[row][col] = TILE_DIRT;
                    }
                }
            }

            // --- Stone fortress at the far right ---
            if (col >= 175 && col <= 195) {
                // Walls
                if (row == 3 || row == 12) {
                    world_map[row][col] = TILE_STONE;
                }
                if ((col == 175 || col == 195) && row >= 3 && row <= 12) {
                    world_map[row][col] = TILE_STONE;
                }
                // Floor
                if (col >= 177 && col <= 193 && row >= 5 && row <= 10) {
                    world_map[row][col] = TILE_STONE;
                }
            }

            // --- Scattered stone clusters ---
            if (col >= 15 && col <= 25 && row >= 0 && row <= 3) {
                rng_state = (u32)(col * 23 + row * 67);
                rng_next();
                if ((rng_next() % 100) < 40) {
                    world_map[row][col] = TILE_STONE;
                }
            }
        }
    }
}

//=============================================================================
// Palette setup for Mode 0
//=============================================================================
static void setup_palette(void) {
    pal_bg_mem[0] = RGB15(2, 2, 5);     // bg / transparent
    pal_bg_mem[1] = RGB15(1, 1, 1);     // border

    // Grass (indices 2-5)
    pal_bg_mem[2]  = RGB15(10, 22, 5);
    pal_bg_mem[3]  = RGB15(7, 18, 3);
    pal_bg_mem[4]  = RGB15(3, 10, 2);
    pal_bg_mem[5]  = RGB15(5, 14, 3);

    // Stone (6-9)
    pal_bg_mem[6]  = RGB15(18, 18, 16);
    pal_bg_mem[7]  = RGB15(14, 14, 13);
    pal_bg_mem[8]  = RGB15(7, 7, 6);
    pal_bg_mem[9]  = RGB15(10, 10, 9);

    // Dirt (10-13)
    pal_bg_mem[10] = RGB15(18, 12, 6);
    pal_bg_mem[11] = RGB15(14, 9, 4);
    pal_bg_mem[12] = RGB15(7, 4, 2);
    pal_bg_mem[13] = RGB15(10, 7, 3);

    // Water (14-17)
    pal_bg_mem[14] = RGB15(5, 15, 22);
    pal_bg_mem[15] = RGB15(3, 11, 18);
    pal_bg_mem[16] = RGB15(1, 5, 10);
    pal_bg_mem[17] = RGB15(2, 8, 14);

    // OBJ palette
    memcpy16(pal_obj_mem, hero_walkPal, hero_walkPalLen / 2);
}

//=============================================================================
// Render iso cube into the pixel buffer
//=============================================================================
static inline void px_plot(int x, int y, u8 clr) {
    if ((unsigned)x < WIN_PX_W && (unsigned)y < WIN_PX_H)
        win_pixels[y * WIN_PX_W + x] = clr;
}

static void px_hline(int x0, int x1, int y, u8 clr) {
    if ((unsigned)y >= WIN_PX_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= WIN_PX_W) x1 = WIN_PX_W - 1;
    u8 *row = &win_pixels[y * WIN_PX_W];
    for (int x = x0; x <= x1; x++)
        row[x] = clr;
}

static void render_iso_cube(int sx, int sy, int type) {
    u8 ctl = type * 4 + 2;
    u8 ctr = type * 4 + 3;
    u8 csl = type * 4 + 4;
    u8 csr = type * 4 + 5;
    u8 cbr = 1;
    int hw = ISO_HALF_W, hh = ISO_HALF_H;

    // Top face upper half
    for (int dy = 0; dy < hh; dy++) {
        int half_span = (dy * hw + hh / 2) / hh;
        int py = sy + dy;
        px_plot(sx - half_span, py, cbr);
        px_plot(sx + half_span, py, cbr);
        if (sx - half_span + 1 < sx) px_hline(sx - half_span + 1, sx - 1, py, ctl);
        if (sx < sx + half_span)     px_hline(sx, sx + half_span - 1, py, ctr);
    }

    // Top face lower half
    for (int dy = 0; dy < hh; dy++) {
        int half_span = ((hh - dy) * hw + hh / 2) / hh;
        int py = sy + hh + dy;
        px_plot(sx - half_span, py, cbr);
        px_plot(sx + half_span, py, cbr);
        if (sx - half_span + 1 < sx) px_hline(sx - half_span + 1, sx - 1, py, ctl);
        if (sx < sx + half_span)     px_hline(sx, sx + half_span - 1, py, ctr);
    }

    // Left side
    for (int py = sy + hh; py < sy + 2 * hh + CUBE_SIDE_H; py++) {
        int dt = py - (sy + hh);
        int x_top = (dt <= hh) ? sx - hw + (dt * hw) / hh : sx;
        int db = py - (sy + hh + CUBE_SIDE_H);
        int x_bot;
        if (db < 0)        x_bot = sx - hw;
        else if (db <= hh) x_bot = sx - hw + (db * hw) / hh;
        else               continue;
        if (x_bot >= x_top) continue;
        px_plot(x_bot, py, cbr);
        if (x_bot + 1 < x_top) px_hline(x_bot + 1, x_top - 1, py, csl);
    }

    // Right side
    for (int py = sy + hh; py < sy + 2 * hh + CUBE_SIDE_H; py++) {
        int dt = py - (sy + hh);
        int x_top = (dt <= hh) ? sx + hw - (dt * hw) / hh : sx;
        int db = py - (sy + hh + CUBE_SIDE_H);
        int x_bot;
        if (db < 0)        x_bot = sx + hw;
        else if (db <= hh) x_bot = sx + hw - (db * hw) / hh;
        else               continue;
        if (x_top >= x_bot) continue;
        if (x_top < x_bot - 1) px_hline(x_top, x_bot - 1, py, csr);
        px_plot(x_bot, py, cbr);
    }
}

//=============================================================================
// Render the visible window of the world
// Window is centered at (cx, cy) in world-pixel-space.
// Only renders cubes that intersect the 512×320 pixel buffer.
//=============================================================================
// Render a range of map columns [col_start, col_end) into the pixel buffer.
// Call with col_start=0, col_end=MAP_COLS for full render.
// win_cx/win_cy must already be set. Buffer must be cleared before first call.
static void render_window_cols(int col_start, int col_end) {
    int win_left = win_cx - WIN_HALF_W;
    int win_top  = win_cy - WIN_HALF_H;

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = col_start; col < col_end; col++) {
            int wx, wy;
            iso_tile_to_world(col, row, &wx, &wy);
            int px = wx - win_left;
            int py = wy - win_top;
            if (px < -32 || px >= WIN_PX_W + 32) continue;
            if (py < -24 || py >= WIN_PX_H + 24) continue;
            render_iso_cube(px, py, world_map[row][col]);
        }
    }
}

static void render_window(int cx, int cy) {
    win_cx = cx;
    win_cy = cy;
    memset(win_pixels, 0, sizeof(win_pixels));
    render_window_cols(0, MAP_COLS);
}

//=============================================================================
// Convert pixel buffer → 8bpp 8×8 tiles + tilemap with deduplication
//=============================================================================
static int tile_match(const u8 *block_start, int stride, const u32 *stored) {
    const u8 *s = (const u8 *)stored;
    for (int r = 0; r < 8; r++) {
        const u8 *row_ptr = block_start + r * stride;
        const u8 *sr = s + r * 8;
        for (int c = 0; c < 8; c++) {
            if (row_ptr[c] != sr[c]) return 0;
        }
    }
    return 1;
}

static int find_or_add_tile(const u8 *block_start, int stride) {
    for (int i = 0; i < num_unique_tiles; i++) {
        if (tile_match(block_start, stride, &tile_store[i * 16]))
            return i;
    }

    if (num_unique_tiles >= MAX_BG_TILES)
        return 0;

    int id = num_unique_tiles++;
    u8 *dst = (u8 *)&tile_store[id * 16];
    for (int r = 0; r < 8; r++) {
        const u8 *row_ptr = block_start + r * stride;
        for (int c = 0; c < 8; c++)
            dst[r * 8 + c] = row_ptr[c];
    }
    return id;
}

// Build tileset+map for tile rows [tr_start, tr_end).
// Call with reset=1 on the first batch to clear state.
static void build_tileset_and_map_rows(int tr_start, int tr_end, int reset) {
    if (reset) {
        num_unique_tiles = 0;
        memset(tilemap, 0, sizeof(tilemap));
    }

    int tile_cols = WIN_PX_W / 8;  // 64
    int tile_rows = WIN_PX_H / 8;  // 40
    if (tr_end > tile_rows) tr_end = tile_rows;
    if (tr_end > BG_MAP_H) tr_end = BG_MAP_H;

    for (int tr = tr_start; tr < tr_end; tr++) {
        for (int tc = 0; tc < tile_cols; tc++) {
            const u8 *block = &win_pixels[tr * 8 * WIN_PX_W + tc * 8];
            int tid = find_or_add_tile(block, WIN_PX_W);
            tilemap[tr * BG_MAP_W + tc] = (u16)tid;
        }
    }
}

static void build_tileset_and_map(void) {
    int tile_rows = WIN_PX_H / 8;
    if (tile_rows > BG_MAP_H) tile_rows = BG_MAP_H;
    build_tileset_and_map_rows(0, tile_rows, 1);
}

//=============================================================================
// Copy tileset + tilemap to VRAM
//=============================================================================
static void upload_to_vram(void) {
    u32 *dst = (u32 *)&tile_mem[TILE_CBB][0];
    memcpy32(dst, tile_store, (num_unique_tiles * 64) / 4);

    for (int r = 0; r < BG_MAP_H; r++) {
        for (int c = 0; c < BG_MAP_W; c++) {
            int sb_x = c / 32;
            int sb_y = r / 32;
            int sb = sb_x + sb_y * 2;
            int local_c = c % 32;
            int local_r = r % 32;
            u16 *sb_base = (u16 *)se_mem[TILE_SBB + sb];
            sb_base[local_r * 32 + local_c] = tilemap[r * BG_MAP_W + c];
        }
    }
}

//=============================================================================
// Full re-render pipeline: render window → build tiles → upload
//=============================================================================
static void rerender_at(int cx, int cy) {
    render_window(cx, cy);
    build_tileset_and_map();
    upload_to_vram();
}

//=============================================================================
// Player
//=============================================================================
static void player_init(void) {
    // Start on the LEFT side of the world (col=3, row=8)
    int wx, wy;
    iso_tile_to_world(3, 8, &wx, &wy);
    player.world_x = INT2FP(wx);
    player.world_y = INT2FP(wy);
    player.facing = DIR_SE;
    player.frame = 0;
    player.frame_timer = 0;
    player.moving = 0;
}

// World bounds for player clamping (in world-pixel coords, fixed-point)
static int bound_wx_min, bound_wx_max;
static int bound_wy_min, bound_wy_max;

static void compute_world_bounds(void) {
    // Give a small margin inside the edges
    int margin = 16;
    bound_wx_min = INT2FP(WORLD_WX_MIN + margin);
    bound_wx_max = INT2FP(WORLD_WX_MAX - margin);
    bound_wy_min = INT2FP(WORLD_WY_MIN + margin);
    bound_wy_max = INT2FP(WORLD_WY_MAX - margin);
}

static void player_update(void) {
    int iso_dx = 0, iso_dy = 0;

    if (key_is_down(KEY_RIGHT)) { iso_dx += 1; player.facing = DIR_SE; }
    if (key_is_down(KEY_LEFT))  { iso_dx -= 1; player.facing = DIR_NW; }
    if (key_is_down(KEY_UP))    { iso_dy -= 1; player.facing = DIR_NE; }
    if (key_is_down(KEY_DOWN))  { iso_dy += 1; player.facing = DIR_SW; }

    if (iso_dx > 0 && iso_dy < 0) player.facing = DIR_NE;
    if (iso_dx > 0 && iso_dy > 0) player.facing = DIR_SE;
    if (iso_dx < 0 && iso_dy < 0) player.facing = DIR_NW;
    if (iso_dx < 0 && iso_dy > 0) player.facing = DIR_SW;

    int dx = iso_dx * 2 - iso_dy * 2;
    int dy = iso_dx * 1 + iso_dy * 1;

    int spd = PLAYER_SPEED;
    if (iso_dx != 0 && iso_dy != 0) {
        player.world_x += (dx * spd) >> 1;
        player.world_y += (dy * spd) >> 1;
    } else {
        player.world_x += dx * spd;
        player.world_y += dy * spd;
    }

    // Clamp to world bounds — NO wrapping
    if (player.world_x < bound_wx_min) player.world_x = bound_wx_min;
    if (player.world_x > bound_wx_max) player.world_x = bound_wx_max;
    if (player.world_y < bound_wy_min) player.world_y = bound_wy_min;
    if (player.world_y > bound_wy_max) player.world_y = bound_wy_max;

    player.moving = (dx != 0 || dy != 0);

    if (player.moving) {
        player.frame_timer++;
        if (player.frame_timer >= HERO_ANIM_SPEED) {
            player.frame_timer = 0;
            player.frame = (player.frame + 1) % HERO_WALK_FRAMES;
        }
    } else {
        player.frame = 0;
        player.frame_timer = 0;
    }
}

static void player_draw(void) {
    int sx, sy;
    world_to_screen(FP2INT(player.world_x), FP2INT(player.world_y),
                    FP2INT(camera.x), FP2INT(camera.y), &sx, &sy);

    sx -= PLAYER_SPR_W / 2;
    sy -= PLAYER_SPR_H / 2;

    int dir_row;
    switch (player.facing) {
        case DIR_SW: dir_row = 0; break;
        case DIR_SE: dir_row = 1; break;
        case DIR_NW: dir_row = 2; break;
        case DIR_NE: dir_row = 3; break;
        default:     dir_row = 0; break;
    }

    int tile_id = (dir_row * HERO_WALK_FRAMES + player.frame) * HERO_TILES_PER_FRAME;

    obj_buffer[0].attr0 = ATTR0_Y(sy & 0xFF) | ATTR0_SQUARE | ATTR0_4BPP;
    obj_buffer[0].attr1 = ATTR1_X(sx & 0x1FF) | ATTR1_SIZE_32;
    obj_buffer[0].attr2 = ATTR2_ID(tile_id) | ATTR2_PRIO(0) | ATTR2_PALBANK(0);
}

//=============================================================================
// Camera
//=============================================================================
static void camera_update(void) {
    camera.x += (player.world_x - camera.x) >> 3;
    camera.y += (player.world_y - camera.y) >> 3;

    // Clamp camera to world bounds (no scrolling past edges)
    if (camera.x < bound_wx_min) camera.x = bound_wx_min;
    if (camera.x > bound_wx_max) camera.x = bound_wx_max;
    if (camera.y < bound_wy_min) camera.y = bound_wy_min;
    if (camera.y > bound_wy_max) camera.y = bound_wy_max;
}

//=============================================================================
// Main
//=============================================================================
int main(void) {
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    setup_palette();
    generate_world();
    compute_world_bounds();

    // Load hero sprite tiles into OBJ VRAM
    memcpy32(&tile_mem[4][0], hero_walkTiles, hero_walkTilesLen / 4);

    // Set up Mode 0 with BG0 (8bpp, 64×64 tilemap) + OBJ
    REG_BG0CNT = BG_CBB(TILE_CBB) | BG_SBB(TILE_SBB) | BG_8BPP | BG_SIZE3 | BG_PRIO(1);
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_OBJ | DCNT_OBJ_1D;

    oam_init(obj_buffer, 128);
    player_init();
    camera.x = player.world_x;
    camera.y = player.world_y;

    // Initial render centered on player (synchronous, only at boot)
    rerender_at(FP2INT(camera.x), FP2INT(camera.y));

    while (1) {
        key_poll();
        player_update();
        camera_update();

        int cam_wx = FP2INT(camera.x);
        int cam_wy = FP2INT(camera.y);

        //-------------------------------------------------------------
        // Progressive re-render state machine
        //-------------------------------------------------------------
        if (rerender_phase == 0) {
            // Check if we need to start a re-render
            int drift_x = cam_wx - win_cx;
            int drift_y = cam_wy - win_cy;
            if (drift_x < 0) drift_x = -drift_x;
            if (drift_y < 0) drift_y = -drift_y;

            if (drift_x > RERENDER_THRESHOLD || drift_y > RERENDER_THRESHOLD) {
                rerender_target_cx = cam_wx;
                rerender_target_cy = cam_wy;
                rerender_phase = 1;
                rerender_step = 0;
                // Set new window center and clear buffer on first step
                win_cx = rerender_target_cx;
                win_cy = rerender_target_cy;
                memset(win_pixels, 0, sizeof(win_pixels));
            }
        }

        if (rerender_phase == 1) {
            // Render iso cubes in column slices
            int cols_per_step = (MAP_COLS + RERENDER_CUBE_FRAMES - 1) / RERENDER_CUBE_FRAMES;
            int col_start = rerender_step * cols_per_step;
            int col_end = col_start + cols_per_step;
            if (col_end > MAP_COLS) col_end = MAP_COLS;

            render_window_cols(col_start, col_end);
            rerender_step++;

            if (rerender_step >= RERENDER_CUBE_FRAMES) {
                rerender_phase = 2;
                rerender_step = 0;
            }
        } else if (rerender_phase == 2) {
            // Build tileset+map in row slices
            int total_tile_rows = WIN_PX_H / 8;  // 40
            if (total_tile_rows > BG_MAP_H) total_tile_rows = BG_MAP_H;
            int rows_per_step = (total_tile_rows + RERENDER_TILE_FRAMES - 1) / RERENDER_TILE_FRAMES;
            int tr_start = rerender_step * rows_per_step;
            int tr_end = tr_start + rows_per_step;

            build_tileset_and_map_rows(tr_start, tr_end, (rerender_step == 0) ? 1 : 0);
            rerender_step++;

            if (rerender_step >= RERENDER_TILE_FRAMES) {
                rerender_phase = 3;
            }
        } else if (rerender_phase == 3) {
            // Upload to VRAM
            upload_to_vram();
            rerender_phase = 0;
        }

        // Hardware scroll: camera world coords → buffer coords → BG scroll
        // Buffer pixel (0,0) = world pixel (win_cx - WIN_HALF_W, win_cy - WIN_HALF_H)
        // Camera pixel in buffer = (cam_wx - (win_cx - WIN_HALF_W), cam_wy - (win_cy - WIN_HALF_H))
        // Scroll = buffer_cam - screen_center
        int scroll_x = (cam_wx - win_cx + WIN_HALF_W) - SCREEN_W / 2;
        int scroll_y = (cam_wy - win_cy + WIN_HALF_H) - SCREEN_H / 2;

        REG_BG0HOFS = scroll_x & 0x1FF;
        REG_BG0VOFS = scroll_y & 0x1FF;

        player_draw();

        VBlankIntrWait();
        oam_copy(oam_mem, obj_buffer, 2);
    }

    return 0;
}
