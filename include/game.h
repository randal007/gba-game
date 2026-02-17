// game.h — Core game types and constants (Mode 0 tiled renderer + tilemap streaming)
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
#define ISO_TILE_W   32    // Full diamond width in pixels
#define ISO_TILE_H   16    // Full diamond height in pixels
#define ISO_HALF_W   16    // Half-width
#define ISO_HALF_H   8     // Half-height
#define CUBE_SIDE_H  8     // Height of side faces

//=============================================================================
// Map — long side-scroller strip
//=============================================================================
#define MAP_COLS     200
#define MAP_ROWS     16

// Tile types
#define TILE_GRASS   0
#define TILE_STONE   1
#define TILE_DIRT    2
#define TILE_WATER   3

//=============================================================================
// Sliding render window
// The pixel buffer is 512×320, representing a window into world-pixel-space.
// When the camera drifts too far from window center, we re-render.
//=============================================================================
#define WIN_PX_W     512
#define WIN_PX_H     320
#define WIN_HALF_W   (WIN_PX_W / 2)   // 256
#define WIN_HALF_H   (WIN_PX_H / 2)   // 160

// Re-render when camera drifts this far from window center
#define RERENDER_THRESHOLD  110

// Tilemap: 64×64 tiles for BG_SIZE3 (512×512)
#define BG_MAP_W     64
#define BG_MAP_H     64

// VRAM layout
#define TILE_CBB     0     // charblock for BG tiles
#define TILE_SBB     28    // screenblock for tilemap (64×64 uses SBB 28-31)
#define MAX_BG_TILES 896   // max unique 8×8 tiles

//=============================================================================
// Fixed-point (24.8)
//=============================================================================
#define FP_SHIFT     8
#define FP_ONE       (1 << FP_SHIFT)
#define INT2FP(x)   ((x) << FP_SHIFT)
#define FP2INT(x)   ((x) >> FP_SHIFT)

//=============================================================================
// Directions (isometric)
//=============================================================================
enum {
    DIR_SE = 0,
    DIR_NE = 1,
    DIR_NW = 2,
    DIR_SW = 3,
};

//=============================================================================
// Player
//=============================================================================
#define PLAYER_SPEED     (FP_ONE * 1)
#define PLAYER_SPR_W     32
#define PLAYER_SPR_H     32

typedef struct {
    int world_x;   // Fixed-point world position
    int world_y;
    int facing;
    int frame;
    int frame_timer;
    int moving;
} Player;

//=============================================================================
// Camera
//=============================================================================
typedef struct {
    int x;  // Fixed-point
    int y;
} Camera;

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

//=============================================================================
// World pixel extent helpers
// For MAP_COLS=200, MAP_ROWS=16:
//   wx range: (0-15)*16=-240  to  (199-0)*16=3184
//   wy range: (0+0)*8=0       to  (199+15)*8=1712
//=============================================================================
#define WORLD_WX_MIN  ((0 - (MAP_ROWS-1)) * ISO_HALF_W)       // -240
#define WORLD_WX_MAX  (((MAP_COLS-1) - 0) * ISO_HALF_W)       //  3184
#define WORLD_WY_MIN  0
#define WORLD_WY_MAX  (((MAP_COLS-1) + (MAP_ROWS-1)) * ISO_HALF_H + 2*ISO_HALF_H + CUBE_SIDE_H)

#endif // GAME_H
