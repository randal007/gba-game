
//{{BLOCK(floor_iso)

//======================================================================
//
//	floor_iso, 64x8@4, 
//	+ palette 16 entries, not compressed
//	+ 9 tiles (t|f|p reduced) not compressed
//	+ regular map (flat), not compressed, 4x1 
//	Metatiled by 2x1
//	Total size: 32 + 288 + 20 + 8 = 348
//
//	Time-stamp: 2026-02-15, 15:33:36
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_FLOOR_ISO_H
#define GRIT_FLOOR_ISO_H

#define floor_isoTilesLen 288
extern const unsigned int floor_isoTiles[72];

#define floor_isoMetaTilesLen 20
extern const unsigned short floor_isoMetaTiles[10];

#define floor_isoMetaMapLen 8
extern const unsigned short floor_isoMetaMap[4];

#define floor_isoPalLen 32
extern const unsigned short floor_isoPal[16];

#endif // GRIT_FLOOR_ISO_H

//}}BLOCK(floor_iso)
