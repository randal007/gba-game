// game.h — Core game types and constants
#ifndef GAME_H
#define GAME_H

#include <tonc.h>

//=============================================================================
// Screen
//=============================================================================
#define SCREEN_W     240
#define SCREEN_H     160

//=============================================================================
// Isometric tile dimensions (FFTA-style 2:1)
//=============================================================================
#define ISO_TILE_W   32
#define ISO_TILE_H   16
#define ISO_HALF_W   16
#define ISO_HALF_H   8
#define CUBE_SIDE_H  8

//=============================================================================
// Map — long side-scroller strip
//=============================================================================
#define MAP_COLS     200
#define MAP_ROWS     16

#define TILE_GRASS   0
#define TILE_STONE   1
#define TILE_DIRT    2
#define TILE_WATER   3

//=============================================================================
// BG tilemap: 64×64 (BG_SIZE3 = 512×512 pixels)
//=============================================================================
#define BG_MAP_W     64
#define BG_MAP_H     64
#define TILE_CBB     0
#define TILE_SBB     28

//=============================================================================
// World pixel extents (derived from map + iso transform)
// Cube at (col,row): wx = (col-row)*16, wy = (col+row)*8
// Cube renders: x in [wx-16, wx+16], y in [wy, wy+24]
//=============================================================================
#define WORLD_PX_X0  (-256)   // leftmost rendered pixel
#define WORLD_PX_Y0  (0)      // topmost rendered pixel
#define WORLD_PX_X1  (3200)   // rightmost rendered pixel
#define WORLD_PX_Y1  (1736)   // bottommost rendered pixel

#define WORLD_TILE_W ((WORLD_PX_X1 - WORLD_PX_X0) / 8)  // 432
#define WORLD_TILE_H ((WORLD_PX_Y1 - WORLD_PX_Y0) / 8)  // 217

// World bounds for clamping (center of occupied area)
#define WORLD_WX_MIN ((0 - (MAP_ROWS-1)) * ISO_HALF_W)
#define WORLD_WX_MAX (((MAP_COLS-1) - 0) * ISO_HALF_W)
#define WORLD_WY_MIN 0
#define WORLD_WY_MAX (((MAP_COLS-1) + (MAP_ROWS-1)) * ISO_HALF_H + 2*ISO_HALF_H + CUBE_SIDE_H)

//=============================================================================
// Pre-computed tilemap limits
//=============================================================================
#define MAX_PRECOMP_TILES 384

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
    int world_x, world_y;  // fixed-point
    int facing, frame, frame_timer, moving;
} Player;

//=============================================================================
// Camera
//=============================================================================
typedef struct { int x, y; } Camera;  // fixed-point

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

#endif // GAME_H
