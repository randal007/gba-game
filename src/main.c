// main.c — GBA Isometric Action Game v0.1
// Walk the World: isometric tile grid, player movement, camera follow
#include "game.h"
#include "../data/hero_walk.h"
#include "../data/floor_iso.h"

//=============================================================================
// Globals
//=============================================================================
static OBJ_ATTR obj_buffer[128];
static Player player;
static Camera camera;

// Simple map: tile type index (0=grass, 1=stone, 2=dirt, 3=water)
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
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
};

// Animation: 6 frames per direction, 4 directions
// hero_walkTiles layout: 4x4 metatiles = 16 tiles per 32x32 frame
// Sheet is 6 cols x 4 rows of 32x32 frames
#define HERO_TILES_PER_FRAME  16    // 4x4 tiles for 32x32 sprite
#define HERO_WALK_FRAMES      6
#define HERO_ANIM_SPEED       6     // Game frames per anim frame

//=============================================================================
// Load graphics into VRAM
//=============================================================================
static void load_graphics(void) {
    // BG palette from floor tiles
    memcpy16(pal_bg_mem, floor_isoPal, floor_isoPalLen / 2);

    // Floor tiles into charblock 0
    memcpy32(&tile_mem[0][0], floor_isoTiles, floor_isoTilesLen / 4);

    // Sprite palette from hero
    memcpy16(pal_obj_mem, hero_walkPal, hero_walkPalLen / 2);

    // Hero sprite tiles into charblock 4 (sprite VRAM)
    memcpy32(&tile_mem[4][0], hero_walkTiles, hero_walkTilesLen / 4);
}

//=============================================================================
// Build the BG screenblock from the isometric map
// Each iso tile maps to a meta-tile pair from floor_iso
// Using 32x32 screenblock (SBB 30)
//=============================================================================
static void build_iso_map(void) {
    // Clear screenblock
    memset16(&se_mem[30][0], 0, 32 * 32);

    // floor_isoMetaMap has 4 entries (one per tile type: grass/stone/dirt/water)
    // Each meta-tile is 2x1 sub-tiles (16x8 diamond)
    // We need to place these as diamonds in the screenblock

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            int tile_type = world_map[row][col];

            // Get the meta-tile index for this tile type
            u16 meta_idx = floor_isoMetaMap[tile_type];

            // Each meta-tile = 2 consecutive sub-tile entries in floor_isoMetaTiles
            u16 sub_l = floor_isoMetaTiles[meta_idx * 2 + 0];
            u16 sub_r = floor_isoMetaTiles[meta_idx * 2 + 1];

            // Convert iso (col, row) to screenblock position
            int se_x = (col + row) * 2;
            int se_y = (col - row) + MAP_ROWS;

            if (se_x >= 0 && se_x + 1 < 32 && se_y >= 0 && se_y < 32) {
                int idx = se_y * 32 + se_x;
                se_mem[30][idx + 0] = sub_l;
                se_mem[30][idx + 1] = sub_r;
            }
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

    // Isometric d-pad mapping
    if (key_is_down(KEY_RIGHT)) { dx += 1; dy += 1; player.facing = DIR_SE; }
    if (key_is_down(KEY_LEFT))  { dx -= 1; dy -= 1; player.facing = DIR_NW; }
    if (key_is_down(KEY_UP))    { dx += 1; dy -= 1; player.facing = DIR_NE; }
    if (key_is_down(KEY_DOWN))  { dx -= 1; dy += 1; player.facing = DIR_SW; }

    // Normalize diagonal speed (~0.707 = 181/256)
    if (dx != 0 && dy != 0) {
        player.world_x += (dx * PLAYER_SPEED * 181) >> 8;
        player.world_y += (dy * PLAYER_SPEED * 181) >> 8;
    } else {
        player.world_x += dx * PLAYER_SPEED;
        player.world_y += dy * PLAYER_SPEED;
    }

    player.moving = (dx != 0 || dy != 0);

    // Walk animation
    if (player.moving) {
        player.frame_timer++;
        if (player.frame_timer >= HERO_ANIM_SPEED) {
            player.frame_timer = 0;
            player.frame++;
            if (player.frame >= HERO_WALK_FRAMES)
                player.frame = 0;
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

    // Sprite sheet layout: 6 frames x 4 directions
    // Row 0=SW, 1=SE, 2=NW, 3=NE
    // Each frame = 16 tiles (4x4 metatile)
    int dir_row;
    switch (player.facing) {
        case DIR_SW: dir_row = 0; break;
        case DIR_SE: dir_row = 1; break;
        case DIR_NW: dir_row = 2; break;
        case DIR_NE: dir_row = 3; break;
        default:     dir_row = 0; break;
    }

    // Tile offset: (dir_row * frames_per_row + frame) * tiles_per_frame
    int tile_id = (dir_row * HERO_WALK_FRAMES + player.frame) * HERO_TILES_PER_FRAME;

    obj_buffer[0].attr0 = ATTR0_Y(sy & 0xFF) | ATTR0_SQUARE | ATTR0_4BPP;
    obj_buffer[0].attr1 = ATTR1_X(sx & 0x1FF) | ATTR1_SIZE_32;
    obj_buffer[0].attr2 = ATTR2_ID(tile_id) | ATTR2_PRIO(1) | ATTR2_PALBANK(0);
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

    load_graphics();
    build_iso_map();

    // BG0: floor layer
    REG_BG0CNT = BG_CBB(0) | BG_SBB(30) | BG_4BPP | BG_REG_32x32 | BG_PRIO(2);

    // Display: Mode 0, BG0 + sprites, 1D mapping
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_OBJ | DCNT_OBJ_1D;

    oam_init(obj_buffer, 128);
    player_init();
    camera.x = player.world_x;
    camera.y = player.world_y;

    while (1) {
        VBlankIntrWait();

        oam_copy(oam_mem, obj_buffer, 2);

        REG_BG0HOFS = FP2INT(camera.x) - SCREEN_W / 2 + MAP_ROWS * ISO_HALF_W;
        REG_BG0VOFS = FP2INT(camera.y) - SCREEN_H / 2;

        key_poll();
        player_update();
        camera_update();
        player_draw();
    }

    return 0;
}
