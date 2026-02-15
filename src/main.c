// main.c — GBA Isometric Action Game v0.1
// Walk the World: isometric tile grid, player movement, camera follow
#include "game.h"

//=============================================================================
// Placeholder tile data (32x16 iso diamond as 4x2 grid of 8x8 tiles)
// 4bpp = 32 bytes per 8x8 tile
// We'll replace this with grit-converted art from Pixel later
//=============================================================================

// Macro for compile-time color: BGR555
#define CLR15(r,g,b) ((r)|((g)<<5)|((b)<<10))

// Simple diamond shape palette (4bpp, palette bank 0)
static const u16 bg_palette[] = {
    CLR15(0, 0, 0),       // 0: transparent
    CLR15(8, 20, 8),      // 1: dark grass
    CLR15(10, 24, 10),    // 2: mid grass
    CLR15(12, 28, 12),    // 3: light grass
    CLR15(6, 14, 6),      // 4: grass shadow
    CLR15(16, 16, 10),    // 5: dirt
    CLR15(20, 20, 14),    // 6: light dirt
    CLR15(4, 10, 4),      // 7: very dark
    0, 0, 0, 0, 0, 0, 0, 0,  // 8–15 unused
};

// 4x2 sub-tiles forming one 32x16 iso diamond
// Each 8x8 tile = 32 bytes (4bpp: 2 pixels per byte)
// Row-major: each row = 4 bytes (8 pixels / 2)
// Tile layout in the 32x16 diamond:
//   [0][1][2][3]   (top row, 8px tall)
//   [4][5][6][7]   (bottom row, 8px tall)

// Top-left (8x8): transparent except lower-right triangle
static const u32 tile_tl[] = {
    0x00000000, 0x00000000,  // rows 0-1: empty
    0x00000000, 0x00000020,  // rows 2-3
    0x00000200, 0x00002200,  // rows 4-5
    0x00022200, 0x00222200,  // rows 6-7
};

// Top-center-left (8x8): mostly filled
static const u32 tile_tcl[] = {
    0x00000000, 0x00000002,
    0x00000022, 0x00000222,
    0x00002222, 0x00022222,
    0x00222222, 0x02222222,
};

// Top-center-right (8x8): mostly filled (mirror of tcl)
static const u32 tile_tcr[] = {
    0x00000000, 0x20000000,
    0x22000000, 0x22200000,
    0x22220000, 0x22222000,
    0x22222200, 0x22222220,
};

// Top-right (8x8): transparent except lower-left triangle
static const u32 tile_tr[] = {
    0x00000000, 0x00000000,
    0x00000000, 0x02000000,
    0x00200000, 0x00220000,
    0x00222000, 0x00222200,
};

// Bottom-left (8x8): upper-right triangle
static const u32 tile_bl[] = {
    0x00222200, 0x00022200,
    0x00002200, 0x00000200,
    0x00000020, 0x00000000,
    0x00000000, 0x00000000,
};

// Bottom-center-left (8x8)
static const u32 tile_bcl[] = {
    0x22222222, 0x02222222,
    0x00222222, 0x00022222,
    0x00002222, 0x00000222,
    0x00000022, 0x00000002,
};

// Bottom-center-right (8x8)
static const u32 tile_bcr[] = {
    0x22222222, 0x22222220,
    0x22222200, 0x22222000,
    0x22220000, 0x22200000,
    0x22000000, 0x20000000,
};

// Bottom-right (8x8): upper-left triangle
static const u32 tile_br[] = {
    0x00222200, 0x00222000,
    0x00220000, 0x00200000,
    0x02000000, 0x00000000,
    0x00000000, 0x00000000,
};

// Placeholder player sprite palette
static const u16 spr_palette[] = {
    CLR15(0, 0, 0),       // 0: transparent
    CLR15(31, 20, 0),     // 1: orange/claw color
    CLR15(28, 16, 0),     // 2: dark orange
    CLR15(31, 25, 8),     // 3: light orange
    CLR15(4, 4, 6),       // 4: dark outline
    CLR15(31, 31, 28),    // 5: white/eye
    CLR15(10, 10, 14),    // 6: mid gray
    CLR15(20, 8, 8),      // 7: red accent
    0, 0, 0, 0, 0, 0, 0, 0,
};

// Placeholder 32x32 player sprite (simple figure)
// 16 tiles (4x4 grid of 8x8) in 4bpp
// For now: a simple colored rectangle with outline
static const u32 player_tiles[16][8] = {
    // Row 0 of sprite (tiles 0-3, top 8 rows)
    // Tile 0,0 — empty
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    // Tile 1,0 — top of head
    { 0x00000000, 0x00044400, 0x00411140, 0x04111140,
      0x04151140, 0x04111140, 0x04111140, 0x00411140 },
    // Tile 2,0 — top of head right
    { 0x00000000, 0x00444000, 0x04111400, 0x04111140,
      0x04111540, 0x04111140, 0x04111140, 0x04111400 },
    // Tile 3,0 — empty
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },

    // Row 1 (tiles 4-7)
    // Tile 0,1
    { 0x00000000, 0x00000000, 0x00000000, 0x00000044,
      0x00000041, 0x00000041, 0x00000041, 0x00000041 },
    // Tile 1,1 — body center-left
    { 0x00444400, 0x00000000, 0x00044400, 0x04111140,
      0x11111111, 0x11111111, 0x11111111, 0x11111111 },
    // Tile 2,1 — body center-right
    { 0x00444400, 0x00000000, 0x00444000, 0x04111140,
      0x11111111, 0x11111111, 0x11111111, 0x11111111 },
    // Tile 3,1
    { 0x00000000, 0x00000000, 0x00000000, 0x44000000,
      0x14000000, 0x14000000, 0x14000000, 0x14000000 },

    // Row 2 (tiles 8-11)
    // Tile 0,2
    { 0x00000041, 0x00000041, 0x00000044, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    // Tile 1,2 — lower body left
    { 0x11111111, 0x11111111, 0x04111140, 0x00411400,
      0x00041400, 0x00041400, 0x00044400, 0x00044400 },
    // Tile 2,2 — lower body right
    { 0x11111111, 0x11111111, 0x04111140, 0x00411400,
      0x00041400, 0x00041400, 0x00044400, 0x00044400 },
    // Tile 3,2
    { 0x14000000, 0x14000000, 0x44000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },

    // Row 3 (tiles 12-15, bottom — feet/empty)
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00041400, 0x00041400, 0x00044400, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00041400, 0x00041400, 0x00044400, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },
};

//=============================================================================
// Globals
//=============================================================================
static OBJ_ATTR obj_buffer[128];
static Player player;
static Camera camera;

// Simple map: 1 = grass tile, 0 = empty
static const u8 world_map[MAP_ROWS][MAP_COLS] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

//=============================================================================
// Load placeholder graphics into VRAM
//=============================================================================
static void load_graphics(void) {
    // BG palette (bank 0)
    memcpy16(pal_bg_mem, bg_palette, 16);

    // Load the 8 sub-tiles for our iso diamond into charblock 0
    // Tile 0 = empty (already zeroed), tiles 1-8 = diamond parts
    memcpy32(&tile_mem[0][1], tile_tl,  32);   // tile 1
    memcpy32(&tile_mem[0][2], tile_tcl, 32);   // tile 2
    memcpy32(&tile_mem[0][3], tile_tcr, 32);   // tile 3
    memcpy32(&tile_mem[0][4], tile_tr,  32);   // tile 4
    memcpy32(&tile_mem[0][5], tile_bl,  32);   // tile 5
    memcpy32(&tile_mem[0][6], tile_bcl, 32);   // tile 6
    memcpy32(&tile_mem[0][7], tile_bcr, 32);   // tile 7
    memcpy32(&tile_mem[0][8], tile_br,  32);   // tile 8

    // Sprite palette (bank 0)
    memcpy16(pal_obj_mem, spr_palette, 16);

    // Player sprite tiles into charblock 4 (sprite VRAM)
    memcpy32(&tile_mem[4][0], player_tiles, sizeof(player_tiles));
}

//=============================================================================
// Build the BG screenblock from the isometric map
// Each iso tile = 4x2 sub-tiles in the screenblock
// We use a 64x64 tile BG (512x512 px) — SBB 30
//=============================================================================
static void build_iso_map(void) {
    // Clear screenblock
    memset16(&se_mem[30][0], 0, 64 * 64);

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            if (world_map[row][col] == 0) continue;

            // Convert iso (col, row) to screenblock position
            // Screen entry coordinates for top-left of this diamond
            // In a 64-wide screenblock, each iso tile occupies 4 columns, 2 rows
            // Offset pattern: even rows shift right by 2 tiles
            int se_x = (col + row) * 2;     // each tile is 4 sub-tiles wide but they overlap
            int se_y = (col - row) + MAP_ROWS; // center vertically

            // Bounds check for 64x64 screenblock
            if (se_x < 0 || se_x + 3 >= 64 || se_y < 0 || se_y + 1 >= 64) continue;

            // Top row of diamond: tiles 1,2,3,4
            if (se_x + 3 < 32 && se_y + 1 < 32 && se_x >= 0 && se_y >= 0) {
                int top = se_y * 32 + se_x;
                se_mem[30][top + 0] = 1;  // TL
                se_mem[30][top + 1] = 2;  // TCL
                se_mem[30][top + 2] = 3;  // TCR
                se_mem[30][top + 3] = 4;  // TR

                int bot = (se_y + 1) * 32 + se_x;
                se_mem[30][bot + 0] = 5;  // BL
                se_mem[30][bot + 1] = 6;  // BCL
                se_mem[30][bot + 2] = 7;  // BCR
                se_mem[30][bot + 3] = 8;  // BR
            }
        }
    }
}

//=============================================================================
// Player
//=============================================================================
static void player_init(void) {
    // Start at map center (tile 8,8)
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

    // Isometric d-pad mapping:
    // RIGHT → move SE (+x, +y in iso world)
    // LEFT  → move NW (-x, -y)
    // UP    → move NE (+x, -y)
    // DOWN  → move SW (-x, +y)
    if (key_is_down(KEY_RIGHT)) { dx += 1; dy += 1; player.facing = DIR_SE; }
    if (key_is_down(KEY_LEFT))  { dx -= 1; dy -= 1; player.facing = DIR_NW; }
    if (key_is_down(KEY_UP))    { dx += 1; dy -= 1; player.facing = DIR_NE; }
    if (key_is_down(KEY_DOWN))  { dx -= 1; dy += 1; player.facing = DIR_SW; }

    // Normalize diagonal speed (approximate: multiply by ~0.7 = 181/256)
    if (dx != 0 && dy != 0) {
        player.world_x += (dx * PLAYER_SPEED * 181) >> 8;
        player.world_y += (dy * PLAYER_SPEED * 181) >> 8;
    } else {
        player.world_x += dx * PLAYER_SPEED;
        player.world_y += dy * PLAYER_SPEED;
    }

    player.moving = (dx != 0 || dy != 0);

    // Animation timer
    if (player.moving) {
        player.frame_timer++;
        if (player.frame_timer >= 8) {
            player.frame_timer = 0;
            player.frame ^= 1;  // Toggle between frame 0 and 1
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

    // Center the sprite on the position
    sx -= PLAYER_SPR_W / 2;
    sy -= PLAYER_SPR_H / 2;

    // Set up OBJ 0 as our player: 32x32, 4bpp
    obj_buffer[0].attr0 = ATTR0_Y(sy & 0xFF) | ATTR0_SQUARE | ATTR0_4BPP;
    obj_buffer[0].attr1 = ATTR1_X(sx & 0x1FF) | ATTR1_SIZE_32;
    obj_buffer[0].attr2 = ATTR2_ID(0) | ATTR2_PRIO(1) | ATTR2_PALBANK(0);

    // H-flip for NW/NE facing (mirror the SE/SW sprites)
    if (player.facing == DIR_NW || player.facing == DIR_NE) {
        obj_buffer[0].attr1 |= ATTR1_HFLIP;
    }
}

//=============================================================================
// Camera
//=============================================================================
static void camera_update(void) {
    // Smooth follow: lerp toward player position
    camera.x += (player.world_x - camera.x) >> 3;
    camera.y += (player.world_y - camera.y) >> 3;
}

//=============================================================================
// Main
//=============================================================================
int main(void) {
    // IRQ setup
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    // Load all graphics
    load_graphics();

    // Build the isometric BG tilemap
    build_iso_map();

    // Configure BG0 for iso floor: charblock 0, screenblock 30, 4bpp, 32x32 tiles
    REG_BG0CNT = BG_CBB(0) | BG_SBB(30) | BG_4BPP | BG_REG_32x32 | BG_PRIO(2);

    // Display: Mode 0, BG0 on, sprites on, 1D sprite mapping
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_OBJ | DCNT_OBJ_1D;

    // Init sprites (hide all)
    oam_init(obj_buffer, 128);

    // Init player at map center
    player_init();
    camera.x = player.world_x;
    camera.y = player.world_y;

    // Main loop
    while (1) {
        VBlankIntrWait();

        // Copy shadow OAM to hardware
        oam_copy(oam_mem, obj_buffer, 2);

        // Scroll BG to camera
        REG_BG0HOFS = FP2INT(camera.x) - SCREEN_W / 2 + MAP_ROWS * ISO_HALF_W;
        REG_BG0VOFS = FP2INT(camera.y) - SCREEN_H / 2;

        // Poll input
        key_poll();

        // Update
        player_update();
        camera_update();
        player_draw();
    }

    return 0;
}
