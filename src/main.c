// main.c — GBA Isometric Action Game
// Pre-computed world tilemap: render everything at boot, zero runtime rendering.
#include "game.h"
#include "../data/hero_walk.h"
#include <string.h>

//=============================================================================
// Globals
//=============================================================================
static OBJ_ATTR obj_buffer[128];
static Player player;
static Camera camera;

EWRAM_BSS static u8 world_map[MAP_ROWS][MAP_COLS];

#define HERO_TILES_PER_FRAME  16
#define HERO_WALK_FRAMES      6
#define HERO_ANIM_SPEED       4

//=============================================================================
// EWRAM: pre-computed world tilemap + tile dictionary + strip buffer
// world_tilemap: 432*217*2 = 187,488 bytes
// tile_dict:     384*64    =  24,576 bytes
// strip_buf:     256*80    =  20,480 bytes
// world_map:     200*16    =   3,200 bytes
// Total:                    ~235,744 bytes (fits in 256KB EWRAM)
//=============================================================================
EWRAM_BSS static u16 world_tilemap[WORLD_TILE_H * WORLD_TILE_W];
EWRAM_BSS static u32 tile_dict[MAX_PRECOMP_TILES * 16];
static int num_tiles;

#define STRIP_W 256
#define STRIP_H 80
EWRAM_BSS static u8 strip_buf[STRIP_W * STRIP_H];

// Ring buffer: which world tile col/row is at hw position 0
static int loaded_col_min, loaded_row_min;

// World bounds for player clamping (fixed-point)
static int bound_wx_min, bound_wx_max;
static int bound_wy_min, bound_wy_max;

//=============================================================================
// RNG
//=============================================================================
static u32 rng_state = 0xDEADBEEF;
static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

//=============================================================================
// World generation (same as before)
//=============================================================================
static void generate_world(void) {
    memset(world_map, TILE_GRASS, sizeof(world_map));
    for (int col = 0; col < MAP_COLS; col++) {
        for (int row = 0; row < MAP_ROWS; row++) {
            if (row >= 6 && row <= 9) {
                int road_center = 7 + ((col * 3 + col/7) % 4) - 1;
                int dist = row - road_center;
                if (dist < 0) dist = -dist;
                if (dist <= 1) world_map[row][col] = TILE_STONE;
            }
            if (col >= 40 && col <= 55) {
                int river_center = 4 + ((col - 40) * 2 + 3) / 5;
                int dist = row - river_center;
                if (dist < 0) dist = -dist;
                if (dist <= 1) world_map[row][col] = TILE_WATER;
            }
            if (col >= 100 && col <= 115 && row >= 8 && row <= 14) {
                int cx = 107, cy = 11, dx = col - cx, dy = row - cy;
                if (dx*dx + dy*dy*3 <= 30) world_map[row][col] = TILE_WATER;
            }
            if (col >= 70 && col <= 75 && row >= 1 && row <= 4) {
                int dx = col - 72, dy = row - 2;
                if (dx*dx + dy*dy <= 4) world_map[row][col] = TILE_WATER;
            }
            if (col >= 130 && col <= 160) {
                rng_state = (u32)(col * 31 + row * 97 + 12345);
                rng_next();
                if ((rng_next() % 100) < 60 && world_map[row][col] == TILE_GRASS)
                    world_map[row][col] = TILE_DIRT;
            }
            if (row >= 13 && row <= 15 && col >= 20 && col <= 90) {
                rng_state = (u32)(col * 17 + row * 53);
                rng_next();
                if ((rng_next() % 100) < 50 && world_map[row][col] == TILE_GRASS)
                    world_map[row][col] = TILE_DIRT;
            }
            if (col >= 175 && col <= 195) {
                if (row == 3 || row == 12) world_map[row][col] = TILE_STONE;
                if ((col == 175 || col == 195) && row >= 3 && row <= 12)
                    world_map[row][col] = TILE_STONE;
                if (col >= 177 && col <= 193 && row >= 5 && row <= 10)
                    world_map[row][col] = TILE_STONE;
            }
            if (col >= 15 && col <= 25 && row >= 0 && row <= 3) {
                rng_state = (u32)(col * 23 + row * 67);
                rng_next();
                if ((rng_next() % 100) < 40) world_map[row][col] = TILE_STONE;
            }
        }
    }
}

//=============================================================================
// Palette
//=============================================================================
static void setup_palette(void) {
    pal_bg_mem[0]  = RGB15(2, 2, 5);
    pal_bg_mem[1]  = RGB15(1, 1, 1);
    pal_bg_mem[2]  = RGB15(10, 22, 5);
    pal_bg_mem[3]  = RGB15(7, 18, 3);
    pal_bg_mem[4]  = RGB15(3, 10, 2);
    pal_bg_mem[5]  = RGB15(5, 14, 3);
    pal_bg_mem[6]  = RGB15(18, 18, 16);
    pal_bg_mem[7]  = RGB15(14, 14, 13);
    pal_bg_mem[8]  = RGB15(7, 7, 6);
    pal_bg_mem[9]  = RGB15(10, 10, 9);
    pal_bg_mem[10] = RGB15(18, 12, 6);
    pal_bg_mem[11] = RGB15(14, 9, 4);
    pal_bg_mem[12] = RGB15(7, 4, 2);
    pal_bg_mem[13] = RGB15(10, 7, 3);
    pal_bg_mem[14] = RGB15(5, 15, 22);
    pal_bg_mem[15] = RGB15(3, 11, 18);
    pal_bg_mem[16] = RGB15(1, 5, 10);
    pal_bg_mem[17] = RGB15(2, 8, 14);
    memcpy16(pal_obj_mem, hero_walkPal, hero_walkPalLen / 2);
}

//=============================================================================
// Strip-buffer iso cube renderer (boot-time only)
// Renders in world pixel coordinates; strip_buf origin = (strip_ox, strip_oy)
//=============================================================================
static int strip_ox, strip_oy;

static inline void s_plot(int wx, int wy, u8 clr) {
    int x = wx - strip_ox, y = wy - strip_oy;
    if ((unsigned)x < STRIP_W && (unsigned)y < STRIP_H)
        strip_buf[y * STRIP_W + x] = clr;
}

static void s_hline(int wx0, int wx1, int wy, u8 clr) {
    int y = wy - strip_oy;
    if ((unsigned)y >= STRIP_H) return;
    int x0 = wx0 - strip_ox, x1 = wx1 - strip_ox;
    if (x0 < 0) x0 = 0;
    if (x1 >= STRIP_W) x1 = STRIP_W - 1;
    if (x0 > x1) return;
    u8 *row = &strip_buf[y * STRIP_W];
    for (int x = x0; x <= x1; x++) row[x] = clr;
}

static void render_cube_strip(int sx, int sy, int type) {
    u8 ctl = type * 4 + 2, ctr = type * 4 + 3;
    u8 csl = type * 4 + 4, csr = type * 4 + 5;
    u8 cbr = 1;
    int hw = ISO_HALF_W, hh = ISO_HALF_H;

    // Top face upper half
    for (int dy = 0; dy < hh; dy++) {
        int hs = (dy * hw + hh / 2) / hh;
        int py = sy + dy;
        s_plot(sx - hs, py, cbr);
        s_plot(sx + hs, py, cbr);
        if (sx - hs + 1 < sx) s_hline(sx - hs + 1, sx - 1, py, ctl);
        if (sx < sx + hs)     s_hline(sx, sx + hs - 1, py, ctr);
    }
    // Top face lower half
    for (int dy = 0; dy < hh; dy++) {
        int hs = ((hh - dy) * hw + hh / 2) / hh;
        int py = sy + hh + dy;
        s_plot(sx - hs, py, cbr);
        s_plot(sx + hs, py, cbr);
        if (sx - hs + 1 < sx) s_hline(sx - hs + 1, sx - 1, py, ctl);
        if (sx < sx + hs)     s_hline(sx, sx + hs - 1, py, ctr);
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
        s_plot(x_bot, py, cbr);
        if (x_bot + 1 < x_top) s_hline(x_bot + 1, x_top - 1, py, csl);
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
        if (x_top < x_bot - 1) s_hline(x_top, x_bot - 1, py, csr);
        s_plot(x_bot, py, cbr);
    }
}

//=============================================================================
// Tile dedup (boot-time)
//=============================================================================
static int find_or_add_tile(const u8 *buf, int stride) {
    for (int i = 0; i < num_tiles; i++) {
        const u8 *s = (const u8 *)&tile_dict[i * 16];
        int match = 1;
        for (int r = 0; r < 8 && match; r++)
            for (int c = 0; c < 8 && match; c++)
                if (buf[r * stride + c] != s[r * 8 + c]) match = 0;
        if (match) return i;
    }
    if (num_tiles >= MAX_PRECOMP_TILES) return 0;
    int id = num_tiles++;
    u8 *dst = (u8 *)&tile_dict[id * 16];
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            dst[r * 8 + c] = buf[r * stride + c];
    return id;
}

//=============================================================================
// Boot: pre-compute entire world tilemap
// Process in 2D patches: 256×80 pixels each.
// Safe extraction per patch: 31 tile cols × 10 tile rows.
//=============================================================================
static void precompute_world(void) {
    num_tiles = 0;
    memset(world_tilemap, 0, sizeof(world_tilemap));

    // Step: 31 tile cols (248px) horizontally, 10 tile rows (80px) vertically
    for (int py = WORLD_PX_Y0; py < WORLD_PX_Y1; py += 80) {
        for (int px = WORLD_PX_X0; px < WORLD_PX_X1; px += 248) {
            strip_ox = px;
            strip_oy = py;
            memset(strip_buf, 0, sizeof(strip_buf));

            // Render all cubes overlapping extended range
            for (int row = 0; row < MAP_ROWS; row++) {
                for (int col = 0; col < MAP_COLS; col++) {
                    int wx, wy;
                    iso_tile_to_world(col, row, &wx, &wy);
                    if (wx + 16 <= px || wx - 16 >= px + STRIP_W) continue;
                    if (wy + 24 <= py || wy >= py + STRIP_H) continue;
                    render_cube_strip(wx, wy, world_map[row][col]);
                }
            }

            // Extract tiles from safe region into world_tilemap
            int wtc_start = (px - WORLD_PX_X0) / 8;
            int wtc_end = wtc_start + 31;
            if (wtc_end > WORLD_TILE_W) wtc_end = WORLD_TILE_W;

            int wtr_start = (py - WORLD_PX_Y0) / 8;
            int wtr_end = wtr_start + 10;
            if (wtr_end > WORLD_TILE_H) wtr_end = WORLD_TILE_H;

            for (int wtr = wtr_start; wtr < wtr_end; wtr++) {
                for (int wtc = wtc_start; wtc < wtc_end; wtc++) {
                    int bx = (WORLD_PX_X0 + wtc * 8) - px;
                    int by = (WORLD_PX_Y0 + wtr * 8) - py;
                    if (bx < 0 || bx + 8 > STRIP_W) continue;
                    if (by < 0 || by + 8 > STRIP_H) continue;
                    int tid = find_or_add_tile(&strip_buf[by * STRIP_W + bx], STRIP_W);
                    world_tilemap[wtr * WORLD_TILE_W + wtc] = (u16)tid;
                }
            }
        }
    }
}

//=============================================================================
// Hardware screenblock helpers
//=============================================================================
static inline void hw_write_entry(int hc, int hr, u16 tid) {
    int sb = (hc >> 5) + (hr >> 5) * 2;
    ((u16 *)se_mem[TILE_SBB + sb])[(hr & 31) * 32 + (hc & 31)] = tid;
}

static void load_hw_col(int wtc) {
    int hc = wtc & 63;
    for (int i = 0; i < 64; i++) {
        int wtr = loaded_row_min + i;
        u16 tid = 0;
        if (wtc >= 0 && wtc < WORLD_TILE_W && wtr >= 0 && wtr < WORLD_TILE_H)
            tid = world_tilemap[wtr * WORLD_TILE_W + wtc];
        hw_write_entry(hc, wtr & 63, tid);
    }
}

static void load_hw_row(int wtr) {
    int hr = wtr & 63;
    for (int i = 0; i < 64; i++) {
        int wtc = loaded_col_min + i;
        u16 tid = 0;
        if (wtc >= 0 && wtc < WORLD_TILE_W && wtr >= 0 && wtr < WORLD_TILE_H)
            tid = world_tilemap[wtr * WORLD_TILE_W + wtc];
        hw_write_entry(wtc & 63, hr, tid);
    }
}

static void load_hw_full(void) {
    for (int i = 0; i < 64; i++)
        load_hw_col(loaded_col_min + i);
}

//=============================================================================
// Runtime: update hardware tilemap ring buffer as camera scrolls
//=============================================================================
static void update_hw_tilemap(int cam_wx, int cam_wy) {
    int cam_tc = (cam_wx - WORLD_PX_X0) / 8;
    int cam_tr = (cam_wy - WORLD_PX_Y0) / 8;

    int desired_col = cam_tc - 32;
    int desired_row = cam_tr - 32;

    // Clamp
    if (desired_col < 0) desired_col = 0;
    if (desired_col > WORLD_TILE_W - 64) desired_col = WORLD_TILE_W - 64;
    if (desired_row < 0) desired_row = 0;
    if (desired_row > WORLD_TILE_H - 64) desired_row = WORLD_TILE_H - 64;

    // Update columns first
    while (loaded_col_min < desired_col) {
        load_hw_col(loaded_col_min + 64);
        loaded_col_min++;
    }
    while (loaded_col_min > desired_col) {
        loaded_col_min--;
        load_hw_col(loaded_col_min);
    }

    // Then rows
    while (loaded_row_min < desired_row) {
        load_hw_row(loaded_row_min + 64);
        loaded_row_min++;
    }
    while (loaded_row_min > desired_row) {
        loaded_row_min--;
        load_hw_row(loaded_row_min);
    }
}

//=============================================================================
// Player
//=============================================================================
static void compute_world_bounds(void) {
    int margin = 16;
    bound_wx_min = INT2FP(WORLD_WX_MIN + margin);
    bound_wx_max = INT2FP(WORLD_WX_MAX - margin);
    bound_wy_min = INT2FP(WORLD_WY_MIN + margin);
    bound_wy_max = INT2FP(WORLD_WY_MAX - margin);
}

static void player_init(void) {
    int wx, wy;
    iso_tile_to_world(3, 8, &wx, &wy);
    player.world_x = INT2FP(wx);
    player.world_y = INT2FP(wy);
    player.facing = DIR_SE;
    player.frame = 0;
    player.frame_timer = 0;
    player.moving = 0;
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

    // Load hero sprite tiles
    memcpy32(&tile_mem[4][0], hero_walkTiles, hero_walkTilesLen / 4);

    // === BOOT: pre-compute entire world tilemap (takes a few seconds) ===
    precompute_world();

    // Upload tile dictionary to VRAM
    memcpy32(&tile_mem[TILE_CBB][0], tile_dict, (num_tiles * 64) / 4);

    // Set up Mode 0: BG0 (8bpp, 64×64) + OBJ
    REG_BG0CNT = BG_CBB(TILE_CBB) | BG_SBB(TILE_SBB) | BG_8BPP | BG_SIZE3 | BG_PRIO(1);
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_OBJ | DCNT_OBJ_1D;

    oam_init(obj_buffer, 128);
    player_init();
    camera.x = player.world_x;
    camera.y = player.world_y;

    // Initial hw tilemap load centered on camera
    int cam_wx = FP2INT(camera.x);
    int cam_wy = FP2INT(camera.y);
    int init_tc = (cam_wx - WORLD_PX_X0) / 8;
    int init_tr = (cam_wy - WORLD_PX_Y0) / 8;
    loaded_col_min = init_tc - 32;
    loaded_row_min = init_tr - 32;
    if (loaded_col_min < 0) loaded_col_min = 0;
    if (loaded_col_min > WORLD_TILE_W - 64) loaded_col_min = WORLD_TILE_W - 64;
    if (loaded_row_min < 0) loaded_row_min = 0;
    if (loaded_row_min > WORLD_TILE_H - 64) loaded_row_min = WORLD_TILE_H - 64;
    load_hw_full();

    // === MAIN LOOP: zero rendering, just tilemap index copies ===
    while (1) {
        key_poll();
        player_update();
        camera_update();

        cam_wx = FP2INT(camera.x);
        cam_wy = FP2INT(camera.y);

        // Update ring buffer (a few u16 writes per frame)
        update_hw_tilemap(cam_wx, cam_wy);

        // BG scroll: hardware wraps at 512 naturally
        int scroll_x = cam_wx - WORLD_PX_X0 - SCREEN_W / 2;
        int scroll_y = cam_wy - WORLD_PX_Y0 - SCREEN_H / 2;
        REG_BG0HOFS = scroll_x & 0x1FF;
        REG_BG0VOFS = scroll_y & 0x1FF;

        player_draw();

        VBlankIntrWait();
        oam_copy(oam_mem, obj_buffer, 2);
    }

    return 0;
}
