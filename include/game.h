// game.h — v0.3: Collision, jump, fall, occlusion
#ifndef GAME_H
#define GAME_H

#include <tonc.h>

//=============================================================================
// Screen
//=============================================================================
#define SCREEN_W     240
#define SCREEN_H     160

//=============================================================================
// Isometric tile dimensions (2:1 ratio)
//=============================================================================
#define ISO_TILE_W   32
#define ISO_TILE_H   16
#define ISO_HALF_W   16
#define ISO_HALF_H   8
#define SIDE_HEIGHT  16   // each side face is 16px tall (= 2 tile rows)

//=============================================================================
// Map — long side-scroller strip
//=============================================================================
#define MAP_COLS     200
#define MAP_ROWS     16
#define MAX_HEIGHT   4

// Ground types (top face)
#define GROUND_GRASS  0
#define GROUND_STONE  1
#define GROUND_DIRT   2
#define GROUND_WATER  3
#define GROUND_ROOF   4
#define NUM_GROUND    5

// Side types
#define SIDE_GRASS    0
#define SIDE_STONE    1
#define SIDE_DIRT     2
#define SIDE_BRICK    3
#define SIDE_ROOF     4
#define NUM_SIDES     5

//=============================================================================
// BG tilemap: 64×64 (BG_SIZE3 = 512×512 pixels)
//=============================================================================
#define BG_MAP_W     64
#define BG_MAP_H     64
#define TILE_CBB     0
#define TILE_SBB     28

//=============================================================================
// World pixel extents (with height stacking)
// Cell (col,row): base wx = (col-row)*16, base wy = (col+row)*8
// Top face at height h: y = wy - h*16
// Max upward shift = MAX_HEIGHT*16 = 64
// Bottom of tallest column: wy + 16 (top face height)
//=============================================================================
#define WORLD_PX_X0  (-256)
#define WORLD_PX_Y0  (-64)     // account for max height elevation
#define WORLD_PX_X1  (3200)
#define WORLD_PX_Y1  (1800)    // (199+15)*8 + 8 + MAX_HEIGHT*16 + 8 = 1792, round up

#define WORLD_TILE_W ((WORLD_PX_X1 - WORLD_PX_X0) / 8)  // 432
#define WORLD_TILE_H ((WORLD_PX_Y1 - WORLD_PX_Y0) / 8)  // 227

// World bounds for clamping
#define WORLD_WX_MIN ((0 - (MAP_ROWS-1)) * ISO_HALF_W)
#define WORLD_WX_MAX (((MAP_COLS-1) - 0) * ISO_HALF_W)
#define WORLD_WY_MIN (-MAX_HEIGHT * SIDE_HEIGHT)
#define WORLD_WY_MAX (((MAP_COLS-1) + (MAP_ROWS-1)) * ISO_HALF_H + ISO_TILE_H)

//=============================================================================
// Pre-computed tilemap limits
//=============================================================================
#define MAX_PRECOMP_TILES 1024

//=============================================================================
// Fixed-point (24.8)
//=============================================================================
#define FP_SHIFT     8
#define FP_ONE       (1 << FP_SHIFT)
#define INT2FP(x)   ((x) << FP_SHIFT)
#define FP2INT(x)   ((x) >> FP_SHIFT)

//=============================================================================
// Directions
//=============================================================================
enum { DIR_SE = 0, DIR_NE = 1, DIR_NW = 2, DIR_SW = 3 };

//=============================================================================
// Player
//=============================================================================
#define PLAYER_SPEED     (FP_ONE * 1)
#define PLAYER_SPR_W     32
#define PLAYER_SPR_H     32

typedef struct {
    int world_x, world_y;  // fixed-point (isometric base plane)
    int facing, frame, frame_timer, moving;
    int tile_col, tile_row; // current map tile
    int height;             // current logical height level (0..MAX_HEIGHT)
    int jumping;            // 1 if in jump animation
    int jump_timer;         // frame counter for jump arc
    int jump_visual_dy;     // visual Y offset during jump (pixels, negative = up)
    int fall_timer;         // frame counter for fall animation
    int falling;            // 1 if falling
    int fall_visual_dy;     // visual Y offset during fall
    int fall_start_h;       // height we started falling from
    int fall_target_h;      // height we're falling to
} Player;

//=============================================================================
// Jump constants
//=============================================================================
#define JUMP_DURATION   16   // frames for jump animation
#define JUMP_PEAK_H     12   // max visual rise in pixels
#define FALL_SPEED      3    // pixels per frame during fall

//=============================================================================
// Camera
//=============================================================================
typedef struct { int x, y; } Camera;  // fixed-point

//=============================================================================
// Map cell
//=============================================================================
typedef struct {
    u8 ground;   // GROUND_*
    u8 side;     // SIDE_*
    u8 height;   // 0..MAX_HEIGHT
    u8 pad;
} MapCell;

//=============================================================================
// Isometric math (inline)
//=============================================================================
static inline void iso_tile_to_world(int col, int row, int *wx, int *wy) {
    *wx = (col - row) * ISO_HALF_W;
    *wy = (col + row) * ISO_HALF_H;
}

static inline void world_to_screen(int wx, int wy, int cam_x, int cam_y, int *sx, int *sy) {
    *sx = wx - cam_x + SCREEN_W / 2;
    *sy = wy - cam_y + SCREEN_H / 2;
}

// Convert world pixel coords to tile col/row (nearest tile center)
static inline void world_to_tile(int wx, int wy, int *col, int *row) {
    // wx = (c-r)*16, wy = (c+r)*8
    // c = (wx + 2*wy) / 32, r = (2*wy - wx) / 32
    // Add 16 to numerator for rounding to nearest
    *col = (wx + 2 * wy + 16) / 32;
    *row = (2 * wy - wx + 16) / 32;
}

// Get map cell safely (returns NULL if out of bounds)
static inline MapCell *get_map_cell(MapCell map[][MAP_COLS], int col, int row) {
    if (col < 0 || col >= MAP_COLS || row < 0 || row >= MAP_ROWS)
        return (MapCell *)0;
    return &map[row][col];
}

#endif // GAME_H
