// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_things.h
/// \brief Rendering of moving objects, sprites

#ifndef __R_THINGS__
#define __R_THINGS__

#include "sounds.h"
#include "r_plane.h"

// "Left" and "Right" character symbols for additional rotation functionality
#define ROT_L ('L' - '0')
#define ROT_R ('R' - '0')

// number of sprite lumps for spritewidth,offset,topoffset lookup tables
// Fab: this is a hack : should allocate the lookup tables per sprite
#define MAXVISSPRITES 2048 // added 2-2-98 was 128

#define VISSPRITECHUNKBITS 6	// 2^6 = 64 sprites per chunk
#define VISSPRITESPERCHUNK (1 << VISSPRITECHUNKBITS)
#define VISSPRITEINDEXMASK (VISSPRITESPERCHUNK - 1)

// Constant arrays used for psprite clipping
//  and initializing clipping.
extern INT16 negonearray[MAXVIDWIDTH];
extern INT16 screenheightarray[MAXVIDWIDTH];

// vars for R_DrawMaskedColumn
extern INT16 *mfloorclip;
extern INT16 *mceilingclip;
extern fixed_t spryscale;
extern fixed_t sprtopscreen;
extern fixed_t sprbotscreen;
extern fixed_t windowtop;
extern fixed_t windowbottom;

void R_DrawMaskedColumn(column_t *column);
void R_SortVisSprites(void);

//faB: find sprites in wadfile, replace existing, add new ones
//     (only sprites from namelist are added or replaced)
void R_AddSpriteDefs(UINT16 wadnum);

#ifdef DELFILE
void R_DelSpriteDefs(UINT16 wadnum);
#endif

//SoM: 6/5/2000: Light sprites correctly!
void R_AddSprites(sector_t *sec, INT32 lightlevel);
void R_InitSprites(void);
void R_ClearSprites(void);
void R_ClipSprites(void);
void R_DrawMasked(void);

// -----------
// SKINS STUFF
// -----------
#define SKINNAMESIZE 16
// should be all lowercase!! S_SKIN processing does a strlwr
#define DEFAULTSKIN "sonic"
#define DEFAULTSKIN2 "tails" // secondary player
#define DEFAULTSKIN3 "knuckles" // third player
#define DEFAULTSKIN4 "eggman" // fourth player

typedef struct
{
	char name[SKINNAMESIZE+1]; // INT16 descriptive name of the skin
	spritedef_t spritedef;
	UINT16 wadnum;
	char sprite[4]; // Sprite name, if seperated from S_SKIN.
	skinflags_t flags;

	char realname[SKINNAMESIZE+1]; // Display name for level completion.
	char hudname[SKINNAMESIZE+1]; // HUD name to display (officially exactly 5 characters long)
	char facerank[9], facewant[9], facemmap[9]; // Arbitrarily named patch lumps

	// SRB2kart
	UINT8 kartspeed;
	UINT8 kartweight;
	//

	// Definable color translation table
	UINT8 starttranscolor;
	UINT8 prefcolor;
	fixed_t highresscale; // scale of highres, default is 0.5

	// specific sounds per skin
	sfxenum_t soundsid[NUMSKINSOUNDS]; // sound # in S_sfx table
} skin_t;

extern CV_PossibleValue_t Forceskin_cons_t[];
//
// for followers.
//
// We'll define these here because they're really just a mobj that'll follow some rules behind a player
//
typedef struct follower_s
{
	char skinname[SKINNAMESIZE+1];	// Skin Name. This is what to refer to when asking the commands anything.
	char name[SKINNAMESIZE+1];		// Name. This is used for the menus. We'll just follow the same rules as skins for this.

	fixed_t scale;			// Scale relative to the player's.

	// some position shenanigans:
	INT32 atangle;			// angle the object will be at around the player. The object itself will always face the same direction as the player.
	INT32 dist;				// distance relative to the player. (In a circle)
	INT32 zoffs;			// Z offset relative to the player's height. Cannot be negative.

	// movement options

	INT32 horzlag;			// Lag for X/Y displacement. Default is 2. Must be > 0 because we divide by this number.
	INT32 vertlag;			// not Vert from Neptunia lagging, this is for Z displacement lag Default is 6. Must be > 0 because we divide by this number.
	INT32 bobamp;			// Bob amplitude. Default is 4.
	INT32 bobspeed;			// Arbitrary modifier for bobbing speed, default is TICRATE*2 (70).

	// from there on out, everything is STATES to allow customization
	// these are only set once when the action is performed and are then free to animate however they want.

	INT32 idlestate;		// state when the player is at a standstill
	INT32 followstate;		// state when the player is moving
	INT32 hurtstate;		// state when the player is being hurt
	INT32 winstate;			// state when the player has won
	INT32 losestate;		// state when the player has lost
	INT32 hitconfirmstate;	// state for hit confirm
	INT32 hitconfirmtime;	// time to keep the above playing for
} follower_t;

// -----------
// NOT SKINS STUFF !
// -----------
typedef enum
{
	SC_NONE = 0,
	SC_TOP = 1,
	SC_BOTTOM = 2
} spritecut_e;

// A vissprite_t is a thing that will be drawn during a refresh,
// i.e. a sprite object that is partly visible.
typedef struct vissprite_s
{
	// Doubly linked list.
	struct vissprite_s *prev;
	struct vissprite_s *next;

	mobj_t *mobj; // for easy access

	INT32 x1, x2;

	fixed_t gx, gy; // for line side calculation
	fixed_t gz, gzt; // global bottom/top for silhouette clipping
	fixed_t pz, pzt; // physical bottom/top for sorting with 3D floors

	fixed_t startfrac; // horizontal position of x1
	fixed_t scale, sortscale; // sortscale only differs from scale for flat sprites
	fixed_t scalestep; // only for flat sprites, 0 otherwise
	fixed_t xiscale; // negative if flipped

	fixed_t texturemid;
	lumpnum_t patch;

	lighttable_t *colormap; // for color translation and shadow draw
	                        // maxbright frames as well

	UINT8 *transmap; // for MF2_SHADOW sprites, which translucency table to use

	INT32 mobjflags;

	INT32 heightsec; // height sector for underwater/fake ceiling support

	extracolormap_t *extra_colormap; // global colormaps

	fixed_t xscale;

	// Precalculated top and bottom screen coords for the sprite.
	fixed_t thingheight; // The actual height of the thing (for 3D floors)
	sector_t *sector; // The sector containing the thing.
	INT16 sz, szt;

	spritecut_e cut;

	INT16 clipbot[MAXVIDWIDTH], cliptop[MAXVIDWIDTH];

	boolean precip;
	boolean vflip; // Flip vertically
	boolean isScaled;
	INT32 dispoffset; // copy of info->dispoffset, affects ordering but not drawing
} vissprite_t;

// A drawnode is something that points to a 3D floor, 3D side, or masked
// middle texture. This is used for sorting with sprites.
typedef struct drawnode_s
{
	visplane_t *plane;
	drawseg_t *seg;
	drawseg_t *thickseg;
	ffloor_t *ffloor;
	vissprite_t *sprite;

	struct drawnode_s *next;
	struct drawnode_s *prev;
} drawnode_t;

extern INT32 numskins;
extern skin_t skins[MAXSKINS];
extern INT32 numfollowers;
extern follower_t followers[MAXSKINS];	// again, use the same rules as skins, no reason not to.

boolean SetPlayerSkin(INT32 playernum,const char *skinname);
void SetPlayerSkinByNum(INT32 playernum,INT32 skinnum); // Tails 03-16-2002
INT32 R_SkinAvailable(const char *name);
void R_AddSkins(UINT16 wadnum);

INT32 R_FollowerAvailable(const char *name);
boolean SetPlayerFollower(INT32 playernum,const char *skinname);
void SetFollower(INT32 playernum,INT32 skinnum);

#ifdef DELFILE
void R_DelSkins(UINT16 wadnum);
#endif

void R_InitDrawNodes(void);

char *GetPlayerFacePic(INT32 skinnum);

// Functions to go from sprite character ID to frame number
// for 2.1 compatibility this still uses the old 'A' + frame code
// The use of symbols tends to be painful for wad editors though
// So the future version of this tries to avoid using symbols
// as much as possible while also defining all 64 slots in a sane manner
// 2.1:    [[ ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~   ]]
// Future: [[ ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz!@ ]]
FUNCMATH FUNCINLINE static ATTRINLINE char R_Frame2Char(UINT8 frame)
{
#if 1 // 2.1 compat
	return 'A' + frame;
#else
	if (frame < 26) return 'A' + frame;
	if (frame < 36) return '0' + (frame - 26);
	if (frame < 62) return 'a' + (frame - 36);
	if (frame == 62) return '!';
	if (frame == 63) return '@';
	return '\xFF';
#endif
}

FUNCMATH FUNCINLINE static ATTRINLINE UINT8 R_Char2Frame(char cn)
{
#if 1 // 2.1 compat
	if (cn == '+') return '\\' - 'A'; // PK3 can't use backslash, so use + instead
	return cn - 'A';
#else
	if (cn >= 'A' && cn <= 'Z') return cn - 'A';
	if (cn >= '0' && cn <= '9') return (cn - '0') + 26;
	if (cn >= 'a' && cn <= 'z') return (cn - 'a') + 36;
	if (cn == '!') return 62;
	if (cn == '@') return 63;
	return 255;
#endif
}

FUNCMATH FUNCINLINE static ATTRINLINE boolean R_ValidSpriteAngle(UINT8 rotation)
{
	return ((rotation <= 8) || (rotation == ROT_L) || (rotation == ROT_R));
}

#endif //__R_THINGS__
