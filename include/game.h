// game.h — Core game types and constants (Mode 0 tiled renderer)
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
// Map
//=============================================================================
#define MAP_COLS     16
#define MAP_ROWS     16

//=============================================================================
// Pre-rendered world pixel buffer dimensions
// World x range: (col-row)*16, col/row 0..15 → -240..240, +offset 256 → 16..496
// World y range: (col+row)*8, 0..240, +cube bottom +24 → 0..264, +offset 16 → 16..280
// Use 512×320, fits in 512×512 BG
//=============================================================================
#define WORLD_PX_W   512
#define WORLD_PX_H   320
#define WORLD_OX     256   // pixel offset for x (makes all coords positive)
#define WORLD_OY     16    // pixel offset for y

// Tilemap: 64×40 tiles covers 512×320 pixels, but BG is 64×64 (512×512)
#define BG_MAP_W     64
#define BG_MAP_H     64

// VRAM layout
#define TILE_CBB     0     // charblock for BG tiles
#define TILE_SBB     28    // screenblock for tilemap (64×64 uses SBB 28-31)
#define MAX_BG_TILES 896   // max unique 8×8 tiles (limited by VRAM: tiles end before SBB28)

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
// Player — speed tuned for 60fps now!
//=============================================================================
#define PLAYER_SPEED     (FP_ONE * 1)   // 1 pixel/frame at 60fps
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

#endif // GAME_H
