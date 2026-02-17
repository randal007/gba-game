// main.c — v0.2: Metatile engine with height stacking
#include "game.h"
#include "../data/metatiles.h"
#include "../data/hero_walk.h"
#include <string.h>

//=============================================================================
// Globals
//=============================================================================
static OBJ_ATTR obj_buffer[128];
static Player player;
static Camera camera;

static MapCell world_map[MAP_ROWS][MAP_COLS];

#define HERO_TILES_PER_FRAME  16
#define HERO_WALK_FRAMES      6
#define HERO_ANIM_SPEED       4

//=============================================================================
// EWRAM: pre-computed world tilemap + tile pixel dictionary
//=============================================================================
EWRAM_BSS static u16 world_tilemap[WORLD_TILE_H * WORLD_TILE_W];
EWRAM_BSS static u8 tile_dict[MAX_PRECOMP_TILES][64];  // 8bpp pixel data per tile
static int num_tiles;

// Ring buffer tracking
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
// World generation with height
//=============================================================================
static void generate_world(void) {
    // Default: flat grass at height 1
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            world_map[r][c].ground = GROUND_GRASS;
            world_map[r][c].side = SIDE_GRASS;
            world_map[r][c].height = 1;
        }
    }

    // === ROAD: stone path through center ===
    for (int c = 0; c < MAP_COLS; c++) {
        int road_center = 7 + ((c * 3 + c / 7) % 4) - 1;
        for (int r = 0; r < MAP_ROWS; r++) {
            int dist = r - road_center;
            if (dist < 0) dist = -dist;
            if (dist <= 1) {
                world_map[r][c].ground = GROUND_STONE;
                world_map[r][c].side = SIDE_STONE;
            }
        }
    }

    // === HILLS: rolling terrain (height 2-3) ===
    // Hill cluster 1: cols 10-25, rows 0-5
    for (int c = 10; c <= 25; c++) {
        for (int r = 0; r <= 5; r++) {
            int dc = c - 17, dr = r - 2;
            int d2 = dc * dc + dr * dr * 2;
            if (d2 < 20) {
                world_map[r][c].height = (d2 < 8) ? 3 : 2;
            }
        }
    }

    // Hill cluster 2: cols 55-70, rows 0-6
    for (int c = 55; c <= 70; c++) {
        for (int r = 0; r <= 6; r++) {
            int dc = c - 62, dr = r - 3;
            int d2 = dc * dc + dr * dr * 2;
            if (d2 < 30) {
                world_map[r][c].height = (d2 < 10) ? 3 : 2;
            }
        }
    }

    // Hill cluster 3: cols 120-135, rows 10-15
    for (int c = 120; c <= 135; c++) {
        for (int r = 10; r <= 15; r++) {
            int dc = c - 127, dr = r - 12;
            int d2 = dc * dc + dr * dr * 2;
            if (d2 < 25) {
                world_map[r][c].height = (d2 < 8) ? 3 : 2;
            }
        }
    }

    // === RIVER VALLEY: cols 40-55, height 0 with water ===
    for (int c = 38; c <= 57; c++) {
        int river_center = 4 + ((c - 38) * 6) / 20;
        for (int r = 0; r < MAP_ROWS; r++) {
            int dist = r - river_center;
            if (dist < 0) dist = -dist;
            if (dist <= 2) {
                world_map[r][c].height = 0;
                world_map[r][c].ground = GROUND_WATER;
                world_map[r][c].side = SIDE_DIRT;
            } else if (dist == 3) {
                world_map[r][c].ground = GROUND_DIRT;
                world_map[r][c].side = SIDE_DIRT;
            }
        }
    }

    // === LAKE: cols 100-115, rows 8-14, height 0 ===
    for (int c = 100; c <= 115; c++) {
        for (int r = 8; r <= 14; r++) {
            int dc = c - 107, dr = r - 11;
            int d2 = dc * dc + dr * dr * 3;
            if (d2 <= 35) {
                world_map[r][c].height = 0;
                world_map[r][c].ground = GROUND_WATER;
                world_map[r][c].side = SIDE_DIRT;
            }
        }
    }

    // === FORTRESS: cols 150-170, rows 3-12, brick walls + roof ===
    for (int c = 150; c <= 170; c++) {
        for (int r = 3; r <= 12; r++) {
            int on_wall = 0;
            // Outer walls
            if (r == 3 || r == 12) on_wall = 1;
            if (c == 150 || c == 170) on_wall = 1;

            if (on_wall) {
                world_map[r][c].ground = GROUND_ROOF;
                world_map[r][c].side = SIDE_BRICK;
                world_map[r][c].height = 4;
            } else if (c >= 152 && c <= 168 && r >= 5 && r <= 10) {
                // Inner courtyard floor - stone at height 2
                world_map[r][c].ground = GROUND_STONE;
                world_map[r][c].side = SIDE_STONE;
                world_map[r][c].height = 2;
            }

            // Corner towers (even higher)
            if ((c >= 149 && c <= 151 && (r >= 2 && r <= 4)) ||
                (c >= 169 && c <= 171 && (r >= 2 && r <= 4)) ||
                (c >= 149 && c <= 151 && (r >= 11 && r <= 13)) ||
                (c >= 169 && c <= 171 && (r >= 11 && r <= 13))) {
                world_map[r][c].ground = GROUND_ROOF;
                world_map[r][c].side = SIDE_BRICK;
                world_map[r][c].height = 4;
            }

            // Main hall inside fortress
            if (c >= 155 && c <= 165 && r >= 6 && r <= 9) {
                world_map[r][c].ground = GROUND_ROOF;
                world_map[r][c].side = SIDE_BRICK;
                world_map[r][c].height = 3;
            }
        }
    }

    // === DIRT PATCHES: cols 80-95 ===
    for (int c = 80; c <= 95; c++) {
        for (int r = 0; r < MAP_ROWS; r++) {
            rng_state = (u32)(c * 31 + r * 97 + 12345);
            rng_next();
            if ((rng_next() % 100) < 50 && world_map[r][c].ground == GROUND_GRASS) {
                world_map[r][c].ground = GROUND_DIRT;
                world_map[r][c].side = SIDE_DIRT;
            }
        }
    }

    // === STONE RUINS: cols 175-195 ===
    for (int c = 175; c <= 195; c++) {
        for (int r = 0; r < MAP_ROWS; r++) {
            if (r == 3 || r == 12) {
                world_map[r][c].ground = GROUND_STONE;
                world_map[r][c].side = SIDE_STONE;
                world_map[r][c].height = 2;
            }
            if ((c == 175 || c == 195) && r >= 3 && r <= 12) {
                world_map[r][c].ground = GROUND_STONE;
                world_map[r][c].side = SIDE_STONE;
                world_map[r][c].height = 2;
            }
            if (c >= 180 && c <= 190 && r >= 5 && r <= 10) {
                world_map[r][c].ground = GROUND_STONE;
                world_map[r][c].side = SIDE_STONE;
                world_map[r][c].height = 3;
            }
        }
    }

    // === Small pond near start ===
    for (int c = 5; c <= 10; c++) {
        for (int r = 1; r <= 4; r++) {
            int dc = c - 7, dr = r - 2;
            if (dc * dc + dr * dr <= 4) {
                world_map[r][c].height = 0;
                world_map[r][c].ground = GROUND_WATER;
                world_map[r][c].side = SIDE_DIRT;
            }
        }
    }
}

//=============================================================================
// Palette setup
//=============================================================================
static void setup_palette(void) {
    // Copy metatile palette to BG palette
    for (int i = 0; i < MT_PALETTE_SIZE; i++)
        pal_bg_mem[i] = mt_palette[i];
    // Index 0 = background color (dark blue-black), not transparent magenta
    pal_bg_mem[0] = RGB15(2, 2, 5);

    // Hero sprite palette
    memcpy16(pal_obj_mem, hero_walkPal, hero_walkPalLen / 2);
}

//=============================================================================
// Tile dedup with simple hash for speed
//=============================================================================
#define HASH_SIZE 2048
#define HASH_MASK (HASH_SIZE - 1)
static u16 hash_table[HASH_SIZE];  // tile index + 1, or 0 = empty
static u16 hash_keys[HASH_SIZE];   // hash of tile data

static u32 tile_hash(const u8 *data) {
    u32 h = 0x811C9DC5;
    const u32 *p = (const u32 *)data;
    for (int i = 0; i < 16; i++) {
        h ^= p[i];
        h *= 0x01000193;
    }
    return h;
}

static int find_or_add_tile(const u8 *pixels) {
    u32 h = tile_hash(pixels);
    u32 slot = h & HASH_MASK;

    // Linear probe
    for (int i = 0; i < HASH_SIZE; i++) {
        u32 s = (slot + i) & HASH_MASK;
        if (hash_table[s] == 0) {
            // Empty slot - add new tile
            if (num_tiles >= MAX_PRECOMP_TILES) return 0;
            int id = num_tiles++;
            memcpy(tile_dict[id], pixels, 64);
            hash_table[s] = id + 1;
            hash_keys[s] = (u16)(h >> 16);
            return id;
        }
        int tid = hash_table[s] - 1;
        if (hash_keys[s] == (u16)(h >> 16)) {
            // Possible match - verify
            if (memcmp(tile_dict[tid], pixels, 64) == 0)
                return tid;
        }
    }
    return 0;  // hash table full
}

//=============================================================================
// Metatile stamping: composite a 4x2 metatile at world pixel (px, py)
// Handles transparency (pixel index 0 = don't overwrite)
//=============================================================================
static void stamp_metatile(int mt_idx, int px, int py) {
    const u16 *mt_tiles = mt_metatile_tiles[mt_idx];

    for (int ty = 0; ty < 2; ty++) {
        for (int tx = 0; tx < 4; tx++) {
            int src_tile = mt_tiles[ty * 4 + tx];
            const u8 *src = mt_tile_pixels[src_tile];

            // World tile coords
            int wtc = (px + tx * 8 - WORLD_PX_X0) / 8;
            int wtr = (py + ty * 8 - WORLD_PX_Y0) / 8;

            if (wtc < 0 || wtc >= WORLD_TILE_W || wtr < 0 || wtr >= WORLD_TILE_H)
                continue;

            // Check if source tile is all transparent
            int has_opaque = 0;
            for (int i = 0; i < 64; i++) {
                if (src[i] != 0) { has_opaque = 1; break; }
            }
            if (!has_opaque) continue;

            // Get current tile pixels at this position
            int cur_idx = world_tilemap[wtr * WORLD_TILE_W + wtc];
            u8 composite[64];
            memcpy(composite, tile_dict[cur_idx], 64);

            // Composite: overwrite non-transparent pixels
            for (int i = 0; i < 64; i++) {
                if (src[i] != 0) composite[i] = src[i];
            }

            // Dedup and store
            int new_idx = find_or_add_tile(composite);
            world_tilemap[wtr * WORLD_TILE_W + wtc] = (u16)new_idx;
        }
    }
}

//=============================================================================
// Isometric side face: stamp a parallelogram-shaped face into world tilemap.
// face=0 → left face (slopes down-right following diamond left edge)
// face=1 → right face (slopes down-left following diamond right edge)
//
// Left face parallelogram (world coords, relative to diamond center wx,wy):
//   A=(wx-16, top_y+8)  B=(wx, top_y+16)  C=(wx, top_y+16+fh)  D=(wx-16, top_y+8+fh)
//   where top_y = base_y - h*SIDE_HEIGHT, fh = h*SIDE_HEIGHT
//
// Right face parallelogram:
//   E=(wx, top_y+16)  F=(wx+16, top_y+8)  G=(wx+16, top_y+8+fh)  H=(wx, top_y+16+fh)
//=============================================================================
static void stamp_side_face(int mt_idx, int face, int wx, int top_y, int face_h) {
    // Proper isometric parallelogram side face.
    // LEFT face: top edge from (wx-16, top_y+8) to (wx, top_y+16) — slope 8/16 = 1:2
    //   At local x (0..15), the top of the face is at ly = lx/2
    //   Bottom edge parallel: ly = lx/2 + face_h
    //   Bounding box: (wx-16, top_y+8), 16 x (face_h + 8)
    // RIGHT face: top edge from (wx, top_y+16) to (wx+16, top_y+8) — slope -1:2
    //   At local x (0..15), the top of the face is at ly = 8 - (lx+1)/2
    //   Bottom edge parallel: ly = 8 - (lx+1)/2 + face_h
    //   Bounding box: (wx, top_y+8), 16 x (face_h + 8)
    if (face_h <= 0) return;

    int face_px, face_py, face_w;
    face_w = 16;
    face_py = top_y + 8;
    int total_h = face_h + 8;

    if (face == 0) {
        face_px = wx - 16;
    } else {
        face_px = wx;
    }

    int tc_min = (face_px - WORLD_PX_X0) / 8;
    int tc_max = (face_px + face_w - 1 - WORLD_PX_X0) / 8;
    int tr_min = (face_py - WORLD_PX_Y0) / 8;
    int tr_max = (face_py + total_h - 1 - WORLD_PX_Y0) / 8;

    for (int tr = tr_min; tr <= tr_max; tr++) {
        if (tr < 0 || tr >= WORLD_TILE_H) continue;
        for (int tc = tc_min; tc <= tc_max; tc++) {
            if (tc < 0 || tc >= WORLD_TILE_W) continue;

            int cur_idx = world_tilemap[tr * WORLD_TILE_W + tc];
            u8 composite[64];
            memcpy(composite, tile_dict[cur_idx], 64);
            int changed = 0;

            for (int py = 0; py < 8; py++) {
                int wy = tr * 8 + WORLD_PX_Y0 + py;
                int ly = wy - face_py;
                if (ly < 0 || ly >= total_h) continue;

                for (int px_off = 0; px_off < 8; px_off++) {
                    int wx2 = tc * 8 + WORLD_PX_X0 + px_off;
                    int lx = wx2 - face_px;
                    if (lx < 0 || lx >= face_w) continue;

                    // Check parallelogram bounds
                    int top_edge;
                    if (face == 0) {
                        // Left face: top edge at ly = lx/2
                        top_edge = lx / 2;
                    } else {
                        // Right face: top edge at ly = (16 - lx) / 2
                        // At lx=0: top=8, at lx=15: top=0
                        top_edge = (16 - lx) / 2;
                    }

                    if (ly < top_edge || ly >= top_edge + face_h) continue;

                    // Texture coordinate: ty wraps within 16px for tiling
                    int ty = (ly - top_edge) % 16;
                    int tx = (face == 0) ? lx : (lx + 16);

                    int mt_tx = tx / 8;
                    int mt_ty = ty / 8;
                    int tile_id = mt_metatile_tiles[mt_idx][mt_ty * 4 + mt_tx];
                    int pixel = mt_tile_pixels[tile_id][(ty & 7) * 8 + (tx & 7)];

                    if (pixel != 0) {
                        composite[py * 8 + px_off] = (u8)pixel;
                        changed = 1;
                    }
                }
            }

            if (changed) {
                int new_idx = find_or_add_tile(composite);
                world_tilemap[tr * WORLD_TILE_W + tc] = (u16)new_idx;
            }
        }
    }
}

//=============================================================================
// Boot: build world tilemap using metatile compositing
//=============================================================================
static void precompute_world(void) {
    num_tiles = 0;
    memset(world_tilemap, 0, sizeof(world_tilemap));
    memset(hash_table, 0, sizeof(hash_table));
    memset(tile_dict[0], 0, 64);  // tile 0 = transparent
    num_tiles = 1;

    // Ground metatile indices
    static const int ground_mt[] = {
        MT_GROUND_GRASS, MT_GROUND_STONE, MT_GROUND_DIRT,
        MT_GROUND_WATER, MT_GROUND_ROOF
    };
    // Side metatile indices (used as texture source for parallelogram faces)
    static const int side_mt[] = {
        MT_SIDE_GRASS_EDGE, MT_SIDE_STONE_WALL, MT_SIDE_DIRT_WALL,
        MT_SIDE_BRICK_WALL, MT_SIDE_ROOF_EDGE
    };

    // Render back-to-front: by (col+row) ascending
    for (int diag = 0; diag < MAP_COLS + MAP_ROWS - 1; diag++) {
        int r_min = diag - (MAP_COLS - 1);
        if (r_min < 0) r_min = 0;
        int r_max = diag;
        if (r_max >= MAP_ROWS) r_max = MAP_ROWS - 1;

        for (int r = r_min; r <= r_max; r++) {
            int c = diag - r;
            if (c < 0 || c >= MAP_COLS) continue;

            MapCell *cell = &world_map[r][c];
            int wx = (c - r) * ISO_HALF_W;
            int base_y = (c + r) * ISO_HALF_H;
            int h = cell->height;

            int top_y = base_y - h * SIDE_HEIGHT;

            // Draw parallelogram side faces (left and right)
            if (h > 0) {
                int face_h = h * SIDE_HEIGHT;
                stamp_side_face(side_mt[cell->side], 0, wx, top_y, face_h);  // left
                stamp_side_face(side_mt[cell->side], 1, wx, top_y, face_h);  // right
            }

            // Draw top face diamond
            int px = wx - ISO_HALF_W;
            stamp_metatile(ground_mt[cell->ground], px, top_y);
        }
    }
}

//=============================================================================
// Upload tile dictionary to VRAM as 8bpp tiles
//=============================================================================
static void upload_tiles_to_vram(void) {
    // Each 8bpp tile = 64 bytes = 16 words
    u32 *dst = (u32 *)&tile_mem[TILE_CBB][0];
    for (int t = 0; t < num_tiles; t++) {
        const u8 *src = tile_dict[t];
        u32 *d = &dst[t * 16];
        for (int i = 0; i < 16; i++) {
            const u8 *s = &src[i * 4];
            d[i] = s[0] | (s[1] << 8) | (s[2] << 16) | (s[3] << 24);
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

    if (desired_col < 0) desired_col = 0;
    if (desired_col > WORLD_TILE_W - 64) desired_col = WORLD_TILE_W - 64;
    if (desired_row < 0) desired_row = 0;
    if (desired_row > WORLD_TILE_H - 64) desired_row = WORLD_TILE_H - 64;

    while (loaded_col_min < desired_col) {
        load_hw_col(loaded_col_min + 64);
        loaded_col_min++;
    }
    while (loaded_col_min > desired_col) {
        loaded_col_min--;
        load_hw_col(loaded_col_min);
    }
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

    // === BOOT: build world tilemap via metatile compositing ===
    precompute_world();

    // Upload tile dictionary to VRAM
    upload_tiles_to_vram();

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

    // === MAIN LOOP ===
    while (1) {
        key_poll();
        player_update();
        camera_update();

        cam_wx = FP2INT(camera.x);
        cam_wy = FP2INT(camera.y);

        update_hw_tilemap(cam_wx, cam_wy);

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
