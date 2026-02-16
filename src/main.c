// main.c — GBA Isometric Action Game v0.1
// Walk the World: isometric tile grid, player movement, camera follow
// Mode 4 bitmap for pixel-perfect iso cube rendering
#include "game.h"
#include "../data/hero_walk.h"

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
#define HERO_ANIM_SPEED       6

#define CUBE_SIDE_H    8  // Height of side faces in pixels

//=============================================================================
// Palette: each tile type gets 4 colors
//   type*4+2 = top_left (lighter)
//   type*4+3 = top_right (slightly darker)
//   type*4+4 = side_left (darkest)
//   type*4+5 = side_right (medium)
//=============================================================================
static void setup_palette(void) {
    pal_bg_mem[0] = RGB15(2, 2, 5);     // bg
    pal_bg_mem[1] = RGB15(1, 1, 1);     // border

    // Grass
    pal_bg_mem[2]  = RGB15(10, 22, 5);
    pal_bg_mem[3]  = RGB15(7, 18, 3);
    pal_bg_mem[4]  = RGB15(3, 10, 2);
    pal_bg_mem[5]  = RGB15(5, 14, 3);

    // Stone
    pal_bg_mem[6]  = RGB15(18, 18, 16);
    pal_bg_mem[7]  = RGB15(14, 14, 13);
    pal_bg_mem[8]  = RGB15(7, 7, 6);
    pal_bg_mem[9]  = RGB15(10, 10, 9);

    // Dirt
    pal_bg_mem[10] = RGB15(18, 12, 6);
    pal_bg_mem[11] = RGB15(14, 9, 4);
    pal_bg_mem[12] = RGB15(7, 4, 2);
    pal_bg_mem[13] = RGB15(10, 7, 3);

    // Water
    pal_bg_mem[14] = RGB15(5, 15, 22);
    pal_bg_mem[15] = RGB15(3, 11, 18);
    pal_bg_mem[16] = RGB15(1, 5, 10);
    pal_bg_mem[17] = RGB15(2, 8, 14);

    memcpy16(pal_obj_mem, hero_walkPal, hero_walkPalLen / 2);
}

//=============================================================================
// Mode 4 pixel writing
//=============================================================================
static inline void m4_plot_page(int x, int y, u8 clr, u16 *page) {
    if ((unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H) return;
    int ofs = y * SCREEN_W + x;
    int idx = ofs >> 1;
    if (ofs & 1)
        page[idx] = (page[idx] & 0x00FF) | ((u16)clr << 8);
    else
        page[idx] = (page[idx] & 0xFF00) | clr;
}

//=============================================================================
// Draw isometric cube. (sx, sy) = top vertex of the diamond.
//
// Diamond top face:
//   Top vertex:    (sx, sy)
//   Left vertex:   (sx - hw, sy + hh)
//   Bottom vertex: (sx, sy + 2*hh)
//   Right vertex:  (sx + hw, sy + hh)
//
// Side faces hang below, height = CUBE_SIDE_H:
//   Left side:  parallelogram from left vertex down to bottom vertex+CUBE_SIDE_H
//   Right side: parallelogram from right vertex down to bottom vertex+CUBE_SIDE_H
//=============================================================================
static void draw_iso_cube(int sx, int sy, int type, u16 *page) {
    u8 ctl = type * 4 + 2;   // top left color
    u8 ctr = type * 4 + 3;   // top right color
    u8 csl = type * 4 + 4;   // side left color
    u8 csr = type * 4 + 5;   // side right color
    u8 cbr = 1;               // border

    int hw = ISO_HALF_W;  // 16
    int hh = ISO_HALF_H;  // 8

    // --- TOP FACE ---
    // Upper half: rows 0..hh-1 (expanding)
    for (int dy = 0; dy < hh; dy++) {
        // At row dy, the diamond spans from center outward
        // The slope is hw/hh = 2 pixels per row
        int half_span = (dy * hw + hh / 2) / hh;
        int py = sy + dy;
        int x0 = sx - half_span;
        int x1 = sx + half_span;

        // Top edge borders
        m4_plot_page(x0, py, cbr, page);
        m4_plot_page(x1, py, cbr, page);

        for (int x = x0 + 1; x < sx; x++) m4_plot_page(x, py, ctl, page);
        for (int x = sx; x < x1; x++) m4_plot_page(x, py, ctr, page);
    }

    // Lower half: rows hh..2*hh-1 (contracting)
    for (int dy = 0; dy < hh; dy++) {
        int half_span = ((hh - dy) * hw + hh / 2) / hh;
        int py = sy + hh + dy;
        int x0 = sx - half_span;
        int x1 = sx + half_span;

        m4_plot_page(x0, py, cbr, page);
        m4_plot_page(x1, py, cbr, page);

        for (int x = x0 + 1; x < sx; x++) m4_plot_page(x, py, ctl, page);
        for (int x = sx; x < x1; x++) m4_plot_page(x, py, ctr, page);
    }

    // --- SIDE FACES ---
    // Left side: parallelogram from left_vertex to bottom_vertex, dropped by CUBE_SIDE_H
    // Left vertex = (sx-hw, sy+hh), Bottom vertex = (sx, sy+2*hh)
    // The top edge of the side is the bottom-left edge of the diamond.
    // The bottom edge is the same line shifted down by CUBE_SIDE_H.
    // Left boundary is vertical at x = sx-hw.
    //
    // For each row py in [sy+hh, sy+2*hh+CUBE_SIDE_H):
    //   Top edge x at py: x_top = sx - hw + (py - (sy+hh)) * hw / hh
    //     (valid for py in [sy+hh, sy+2*hh])
    //   Bottom edge x at py: x_bot = sx - hw + (py - (sy+hh+CUBE_SIDE_H)) * hw / hh
    //     (valid for py in [sy+hh+CUBE_SIDE_H, sy+2*hh+CUBE_SIDE_H])
    //
    // The side face at row py fills from the bottom edge to the top edge.

    for (int py = sy + hh; py < sy + 2 * hh + CUBE_SIDE_H; py++) {
        // Top edge (diamond bottom-left edge)
        int dt = py - (sy + hh);
        int x_top;
        if (dt <= hh)
            x_top = sx - hw + (dt * hw) / hh;
        else
            x_top = sx;  // past diamond bottom

        // Bottom edge (shifted down)
        int db = py - (sy + hh + CUBE_SIDE_H);
        int x_bot;
        if (db < 0)
            x_bot = sx - hw;  // before shifted edge starts
        else if (db <= hh)
            x_bot = sx - hw + (db * hw) / hh;
        else
            continue;  // past the bottom edge

        // Side face fills from x_bot to x_top (x_bot is the left/bottom boundary)
        if (x_bot >= x_top) continue;

        // Vertical left border
        m4_plot_page(x_bot, py, cbr, page);
        // Bottom diagonal border (only on the shifted edge)
        if (db >= 0 && db <= hh)
            m4_plot_page(x_bot, py, cbr, page);

        for (int x = x_bot + 1; x < x_top; x++)
            m4_plot_page(x, py, csl, page);
    }

    // Right side: mirror
    for (int py = sy + hh; py < sy + 2 * hh + CUBE_SIDE_H; py++) {
        int dt = py - (sy + hh);
        int x_top;
        if (dt <= hh)
            x_top = sx + hw - (dt * hw) / hh;
        else
            x_top = sx;

        int db = py - (sy + hh + CUBE_SIDE_H);
        int x_bot;
        if (db < 0)
            x_bot = sx + hw;
        else if (db <= hh)
            x_bot = sx + hw - (db * hw) / hh;
        else
            continue;

        // Side face fills from x_top to x_bot
        if (x_top >= x_bot) continue;

        for (int x = x_top; x < x_bot; x++)
            m4_plot_page(x, py, csr, page);

        // Right vertical border
        m4_plot_page(x_bot, py, cbr, page);
    }
}

//=============================================================================
// Draw the entire iso map (back-to-front)
//=============================================================================
static void draw_map(int cam_x, int cam_y, u16 *page) {
    memset16(page, 0, SCREEN_W * SCREEN_H / 2);

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            int wx, wy;
            iso_tile_to_world(col, row, &wx, &wy);

            int sx = wx - cam_x + SCREEN_W / 2;
            int sy = wy - cam_y + SCREEN_H / 2;

            if (sx < -ISO_TILE_W * 2 || sx > SCREEN_W + ISO_TILE_W * 2) continue;
            if (sy < -ISO_TILE_H * 2 || sy > SCREEN_H + ISO_TILE_H * 2) continue;

            draw_iso_cube(sx, sy, world_map[row][col], page);
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

    // Offset by 512 because tiles are in charblock 5 (bitmap mode requirement)
    int tile_id = 512 + (dir_row * HERO_WALK_FRAMES + player.frame) * HERO_TILES_PER_FRAME;

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
    // In bitmap modes (Mode 4), the back framebuffer overlaps tile_mem[4].
    // OBJ tiles must go into tile_mem[5] (charblock 5 = tile ID 512+).
    memcpy32(&tile_mem[5][0], hero_walkTiles, hero_walkTilesLen / 4);

    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_1D;

    oam_init(obj_buffer, 128);
    player_init();
    camera.x = player.world_x;
    camera.y = player.world_y;

    u16 *back_page = (u16 *)vid_mem_back;

    while (1) {
        key_poll();
        player_update();
        camera_update();

        draw_map(FP2INT(camera.x), FP2INT(camera.y), back_page);
        player_draw();

        VBlankIntrWait();
        vid_flip();
        back_page = (u16 *)vid_mem_back;
        oam_copy(oam_mem, obj_buffer, 2);
    }

    return 0;
}
