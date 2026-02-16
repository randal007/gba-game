
//{{BLOCK(floor_iso)

//======================================================================
//
//	floor_iso — isometric cube tiles (top face + left/right side faces)
//	17 tiles (4bpp, 8×8 each):
//	  0     = empty/transparent
//	  1,2   = grass  top (TL, TR)
//	  3,4   = stone  top (TL, TR)
//	  5,6   = dirt   top (TL, TR)
//	  7,8   = water  top (TL, TR)
//	  9,10  = grass  sides (BL left-face, BR right-face)
//	  11,12 = stone  sides
//	  13,14 = dirt   sides
//	  15,16 = water  sides
//
//	Palette indices:
//	  0  = transparent
//	  1  = border (dark brown)
//	  2  = water TR fill (teal)
//	  3  = grass TR fill
//	  4  = stone TR fill
//	  5  = dirt  TR fill
//	  6  = water TL fill (dark teal)
//	  7  = grass TL fill (green)
//	  8  = stone TL fill (olive)
//	  9  = dirt  TL fill (warm brown)
//	  10 = grass dark (left side)
//	  11 = stone dark
//	  12 = dirt  dark
//	  13 = water dark
//
//======================================================================

const unsigned int floor_isoTiles[136] __attribute__((aligned(4))) __attribute__((visibility("hidden")))=
{
	// Tile 0: empty
	0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,

	// Tile 1: grass TL (top-left of diamond, fill=7, border=1)
	// Row pattern: diamond left half expanding then contracting
	0x10000000,0x33100000,0x77771000,0x77777710,0x77777100,0x77710000,0x71000000,0x00000000,

	// Tile 2: grass TR (top-right of diamond, fill=3, border=1)
	0x00000001,0x00000133,0x00013333,0x01333333,0x00133333,0x00001333,0x00000013,0x00000000,

	// Tile 3: stone TL (fill=8)
	0x10000000,0x44100000,0x88881000,0x88888810,0x88888100,0x88810000,0x81000000,0x00000000,

	// Tile 4: stone TR (fill=4)
	0x00000001,0x00000144,0x00014444,0x01444444,0x00144444,0x00001444,0x00000014,0x00000000,

	// Tile 5: dirt TL (fill=9)
	0x10000000,0x55100000,0x99991000,0x99999910,0x99999100,0x99910000,0x91000000,0x00000000,

	// Tile 6: dirt TR (fill=5)
	0x00000001,0x00000155,0x00015555,0x01555555,0x00155555,0x00001555,0x00000015,0x00000000,

	// Tile 7: water TL (fill=6)
	0x10000000,0x22100000,0x66661000,0x66666610,0x66666100,0x66610000,0x61000000,0x00000000,

	// Tile 8: water TR (fill=2)
	0x00000001,0x00000122,0x00012222,0x01222222,0x00122222,0x00001222,0x00000012,0x00000000,

	// --- SIDE FACE TILES (new) ---
	// Left face (BL): parallelogram hanging from diamond's bottom-left edge
	// Shape: full rows 0-4, then diagonal cutoff rows 5-7
	// Border=1 on left edge (x=0) and bottom diagonal

	// Tile 9: grass BL (left side, dark=0xA=10)
	0xAAAAAAA1,0xAAAAAAA1,0xAAAAAAA1,0xAAAAAAA1,0xAAAAAAA1,0xAAAAA100,0xAAA10000,0xA1000000,

	// Tile 10: grass BR (right side, fill=3, border=1)
	// Shape: full rows 0-4, diagonal cutoff on right side rows 5-7
	0x33333331,0x33333331,0x33333331,0x33333331,0x33333331,0x00133331,0x00001331,0x00000011,

	// Tile 11: stone BL (dark=0xB=11)
	0xBBBBBBB1,0xBBBBBBB1,0xBBBBBBB1,0xBBBBBBB1,0xBBBBBBB1,0xBBBBB100,0xBBB10000,0xB1000000,

	// Tile 12: stone BR (fill=4)
	0x44444441,0x44444441,0x44444441,0x44444441,0x44444441,0x00144441,0x00001441,0x00000011,

	// Tile 13: dirt BL (dark=0xC=12)
	0xCCCCCCC1,0xCCCCCCC1,0xCCCCCCC1,0xCCCCCCC1,0xCCCCCCC1,0xCCCCC100,0xCCC10000,0xC1000000,

	// Tile 14: dirt BR (fill=5)
	0x55555551,0x55555551,0x55555551,0x55555551,0x55555551,0x00155551,0x00001551,0x00000011,

	// Tile 15: water BL (dark=0xD=13)
	0xDDDDDDD1,0xDDDDDDD1,0xDDDDDDD1,0xDDDDDDD1,0xDDDDDDD1,0xDDDDD100,0xDDD10000,0xD1000000,

	// Tile 16: water BR (fill=2)
	0x22222221,0x22222221,0x22222221,0x22222221,0x22222221,0x00122221,0x00001221,0x00000011,
};

const unsigned short floor_isoPal[16] __attribute__((aligned(4))) __attribute__((visibility("hidden")))=
{
	0x0000,  // 0:  transparent
	0x0023,  // 1:  border (dark brown)
	0x65E7,  // 2:  water TR fill (teal)
	0x2ACC,  // 3:  grass TR fill
	0x4A74,  // 4:  stone TR fill
	0x2E36,  // 5:  dirt TR fill
	0x5565,  // 6:  water TL fill (dark teal)
	0x1E6A,  // 7:  grass TL fill (bright green)
	0x3DF0,  // 8:  stone TL fill (olive)
	0x21D2,  // 9:  dirt TL fill (warm brown)
	0x0545,  // 10: grass dark (left side)
	0x0D08,  // 11: stone dark
	0x08E9,  // 12: dirt dark
	0x14C2,  // 13: water dark
	0x0000,  // 14: unused
	0x0000,  // 15: unused
};

//}}BLOCK(floor_iso)
