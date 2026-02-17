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
#define ISO_TILE_W   32    // Full diamond width in pixels
#define ISO_TILE_H   16    // Full diamond height in pixels
#define ISO_HALF_W   16    // Half-width
#define ISO_HALF_H   8     // Half-height

//=============================================================================
// Map
//=============================================================================
#define MAP_COLS     16    // World map size in iso tiles
#define MAP_ROWS     16

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
    DIR_SE = 0,  // d-pad RIGHT
    DIR_NE = 1,  // d-pad UP
    DIR_NW = 2,  // d-pad LEFT
    DIR_SW = 3,  // d-pad DOWN
};

//=============================================================================
// Player
//=============================================================================
#define PLAYER_SPEED     (FP_ONE * 3)  // 3 pixels/frame in world space
#define PLAYER_SPR_W     32
#define PLAYER_SPR_H     32

typedef struct {
    int world_x;   // Fixed-point world position (iso pixel space)
    int world_y;
    int facing;     // DIR_SE, DIR_NE, etc.
    int frame;      // Animation frame index
    int frame_timer;
    int moving;     // Is the player moving this frame?
} Player;

//=============================================================================
// Camera
//=============================================================================
typedef struct {
    int x;  // Fixed-point, world pixel coords of screen center
    int y;
} Camera;

//=============================================================================
// Isometric math (inline)
//=============================================================================

// World tile (col, row) → world pixel position (center of tile)
static inline void iso_tile_to_world(int col, int row, int *wx, int *wy) {
    *wx = (col - row) * ISO_HALF_W;
    *wy = (col + row) * ISO_HALF_H;
}

// World pixel → screen pixel (given camera)
static inline void world_to_screen(int wx, int wy, int cam_x, int cam_y, int *sx, int *sy) {
    *sx = wx - cam_x + SCREEN_W / 2;
    *sy = wy - cam_y + SCREEN_H / 2;
}

// Screen pixel → approximate world tile (for picking / debug)
static inline void screen_to_iso_tile(int sx, int sy, int cam_x, int cam_y, int *col, int *row) {
    int wx = sx - SCREEN_W / 2 + cam_x;
    int wy = sy - SCREEN_H / 2 + cam_y;
    // Inverse of iso transform
    *col = (wx / ISO_HALF_W + wy / ISO_HALF_H) / 2;
    *row = (wy / ISO_HALF_H - wx / ISO_HALF_W) / 2;
}

#endif // GAME_H
