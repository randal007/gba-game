// main.c — GBA Isometric Action Game
// Mode 0 hardware-tiled renderer: pre-render iso world into 8×8 tiles at boot,
// then let the GBA hardware handle display + scrolling. Runs at full 60fps.
#include "game.h"
#include "../data/hero_walk.h"
#include <string.h>

//=============================================================================
// Globals
//=============================================================================
static OBJ_ATTR obj_buffer[128];
static Player player;
static Camera camera;

static const u8 world_map[MAP_ROWS][MAP_COLS] = {
    {0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0},
    {0,0,0,0,0,1,1,1,0,0,0,0,2,2,0,0},
    {0,0,0,0,0,0,1,0,0,0,0,2,2,2,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,3,3,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,3,3,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0},
};

#define HERO_TILES_PER_FRAME  16
#define HERO_WALK_FRAMES      6
#define HERO_ANIM_SPEED       4   // slower at 60fps (was 2 at ~15fps)

//=============================================================================
// EWRAM buffers for pre-rendering
//=============================================================================
EWRAM_BSS static u8 world_pixels[WORLD_PX_W * WORLD_PX_H];

// Tile dedup: each 8bpp 8×8 tile = 64 bytes = 16 u32s
EWRAM_BSS static u32 tile_store[MAX_BG_TILES * 16];
static int num_unique_tiles;

// Tilemap (64×64)
EWRAM_BSS static u16 tilemap[BG_MAP_W * BG_MAP_H];

//=============================================================================
// Palette setup for Mode 0
//=============================================================================
static void setup_palette(void) {
    // BG palette (4bpp, palette 0)
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

    // Water (14-17) — but 4bpp pal0 only has 16 entries (0-15)
    // We'll use palette 0 entries 0-15 in 8bpp mode instead.
    // Actually, let's use 8bpp (256-color) BG to keep it simple with >16 colors.
    pal_bg_mem[14] = RGB15(5, 15, 22);
    pal_bg_mem[15] = RGB15(3, 11, 18);

    // For 8bpp, all 256 entries are in pal_bg_mem directly
    pal_bg_mem[16] = RGB15(1, 5, 10);
    pal_bg_mem[17] = RGB15(2, 8, 14);

    // OBJ palette
    memcpy16(pal_obj_mem, hero_walkPal, hero_walkPalLen / 2);
}

//=============================================================================
// Render iso cube into the pixel buffer (same algorithm as before, but once)
//=============================================================================
static inline void px_plot(int x, int y, u8 clr) {
    if ((unsigned)x < WORLD_PX_W && (unsigned)y < WORLD_PX_H)
        world_pixels[y * WORLD_PX_W + x] = clr;
}

static void px_hline(int x0, int x1, int y, u8 clr) {
    if ((unsigned)y >= WORLD_PX_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= WORLD_PX_W) x1 = WORLD_PX_W - 1;
    u8 *row = &world_pixels[y * WORLD_PX_W];
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
// Pre-render entire world into pixel buffer
//=============================================================================
static void prerender_world(void) {
    // Clear to bg color
    memset(world_pixels, 0, sizeof(world_pixels));

    // Draw back-to-front
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            int wx, wy;
            iso_tile_to_world(col, row, &wx, &wy);
            int px = wx + WORLD_OX;
            int py = wy + WORLD_OY;
            render_iso_cube(px, py, world_map[row][col]);
        }
    }
}

//=============================================================================
// Convert pixel buffer → 8bpp 8×8 tiles + tilemap with deduplication
//=============================================================================

// Compare an 8×8 block from pixel buffer against stored tile
static int tile_match(const u8 *block_start, int stride, const u32 *stored) {
    const u8 *s = (const u8 *)stored;
    for (int r = 0; r < 8; r++) {
        const u8 *row = block_start + r * stride;
        const u8 *sr = s + r * 8;
        for (int c = 0; c < 8; c++) {
            if (row[c] != sr[c]) return 0;
        }
    }
    return 1;
}

static int find_or_add_tile(const u8 *block_start, int stride) {
    // Pack the 8×8 block into tile format (8bpp: 64 bytes = 16 u32s... 
    // but we allocated 8 u32s per tile for 4bpp. Let me use 8bpp properly.)
    // For 8bpp tiles, each tile is 64 bytes = 16 u32s.
    // We need to store them differently. Let me just do a linear search 
    // with direct comparison against the pixel buffer.
    
    for (int i = 0; i < num_unique_tiles; i++) {
        if (tile_match(block_start, stride, &tile_store[i * 16]))
            return i;
    }

    // Add new tile
    if (num_unique_tiles >= MAX_BG_TILES)
        return 0; // fallback

    int id = num_unique_tiles++;
    u8 *dst = (u8 *)&tile_store[id * 16];
    for (int r = 0; r < 8; r++) {
        const u8 *row = block_start + r * stride;
        for (int c = 0; c < 8; c++)
            dst[r * 8 + c] = row[c];
    }
    return id;
}

static void build_tileset_and_map(void) {
    num_unique_tiles = 0;
    memset(tilemap, 0, sizeof(tilemap));

    int tile_cols = WORLD_PX_W / 8;  // 64
    int tile_rows = WORLD_PX_H / 8;  // 40
    if (tile_rows > BG_MAP_H) tile_rows = BG_MAP_H;

    for (int tr = 0; tr < tile_rows; tr++) {
        for (int tc = 0; tc < tile_cols; tc++) {
            const u8 *block = &world_pixels[tr * 8 * WORLD_PX_W + tc * 8];
            int tid = find_or_add_tile(block, WORLD_PX_W);
            tilemap[tr * BG_MAP_W + tc] = (u16)tid;
        }
    }
}

//=============================================================================
// Copy tileset + tilemap to VRAM
//=============================================================================
static void upload_to_vram(void) {
    // 8bpp tiles: each tile = 64 bytes. Copy to charblock 0.
    // tile_mem is TILE (*)[32] where TILE = u32[8] (32 bytes, 4bpp tile).
    // For 8bpp, each logical tile = 2 TILE structs (64 bytes).
    // So tile i occupies tile_mem[BG_CBB][i*2] and tile_mem[BG_CBB][i*2+1].
    u32 *dst = (u32 *)&tile_mem[TILE_CBB][0];
    memcpy32(dst, tile_store, (num_unique_tiles * 64) / 4);

    // Copy tilemap to screenblocks 28-31
    // 64×64 map = 4 screenblocks. Layout: SB28=top-left 32×32, SB29=top-right,
    // SB30=bottom-left, SB31=bottom-right.
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
// Player
//=============================================================================
static void player_init(void) {
    int wx, wy;
    iso_tile_to_world(8, 8, &wx, &wy);
    player.world_x = INT2FP(wx);
    player.world_y = INT2FP(wy);
    player.facing = DIR_SE;
    player.frame = 0;
    player.frame_timer = 0;
    player.moving = 0;
}

static void player_update(void) {
    int dx = 0, dy = 0;

    if (key_is_down(KEY_RIGHT)) { dx += 1; dy += 1; player.facing = DIR_SE; }
    if (key_is_down(KEY_LEFT))  { dx -= 1; dy -= 1; player.facing = DIR_NW; }
    if (key_is_down(KEY_UP))    { dx += 1; dy -= 1; player.facing = DIR_NE; }
    if (key_is_down(KEY_DOWN))  { dx -= 1; dy += 1; player.facing = DIR_SW; }

    if (dx != 0 && dy != 0) {
        player.world_x += (dx * PLAYER_SPEED * 181) >> 8;
        player.world_y += (dy * PLAYER_SPEED * 181) >> 8;
    } else {
        player.world_x += dx * PLAYER_SPEED;
        player.world_y += dy * PLAYER_SPEED;
    }

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
    // Screen position relative to camera
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

    // In Mode 0, OBJ tiles start at charblock 4, tile ID 0
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
}

//=============================================================================
// Main
//=============================================================================
int main(void) {
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    setup_palette();

    // Pre-render the entire isometric world into pixel buffer (one-time cost)
    prerender_world();

    // Convert to hardware tiles + tilemap
    build_tileset_and_map();

    // Upload to VRAM
    upload_to_vram();

    // Load hero sprite tiles into OBJ VRAM (charblock 4, tile 0)
    memcpy32(&tile_mem[4][0], hero_walkTiles, hero_walkTilesLen / 4);

    // Set up Mode 0 with BG0 (8bpp, 64×64 tilemap) + OBJ
    REG_BG0CNT = BG_CBB(TILE_CBB) | BG_SBB(TILE_SBB) | BG_8BPP | BG_SIZE3 | BG_PRIO(1);
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_OBJ | DCNT_OBJ_1D;

    oam_init(obj_buffer, 128);
    player_init();
    camera.x = player.world_x;
    camera.y = player.world_y;

    while (1) {
        key_poll();
        player_update();
        camera_update();

        // Hardware scroll: convert camera world coords to pixel buffer coords
        int cam_wx = FP2INT(camera.x);
        int cam_wy = FP2INT(camera.y);
        int scroll_x = cam_wx + WORLD_OX - SCREEN_W / 2;
        int scroll_y = cam_wy + WORLD_OY - SCREEN_H / 2;

        // Clamp to valid range (BG wraps at 512)
        REG_BG0HOFS = scroll_x & 0x1FF;
        REG_BG0VOFS = scroll_y & 0x1FF;

        player_draw();

        VBlankIntrWait();
        oam_copy(oam_mem, obj_buffer, 2);
    }

    return 0;
}
