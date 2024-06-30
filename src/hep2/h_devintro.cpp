// Haya's Expansion Pack II for Dr. Robotnik's Ring Racers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by hayaUnderscore/haya

// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hep2/h_devintro.cpp
/// \brief DEVMODE password intro screen :-)

#include <algorithm>
#include <array>
#include <vector>
#include <deque>

#include <string>

#include "h_cvars.h"
#include "../d_main.h"
#include "../d_netcmd.h"
#include "../v_video.h"
#include "../noire/n_menu.h"
#include "../m_cond.h"
#include "../m_misc.h"
#include "../k_menu.h"
#include "../s_sound.h"
#include "../m_random.h"
#include "../music.h"
#include "../r_skins.h"
#include "../v_video.h"
#include "../v_draw.hpp"
#include "../z_zone.h"
#include "../doomdef.h"
#include "../g_game.h"
#include "../r_fps.h"
#include "../r_main.h"

struct skidrow_s skidrow;

static menuitem_t MISC_SkidrowMenu[] =
{
	{IT_NOTHING, NULL, NULL, NULL, {NULL}, 0, 0},
};

void f_devmode()
{
	INT32 i;

	if (modifiedgame)
		return;

	// Just unlock all the things.
	for (i = 0; i < MAXUNLOCKABLES; i++)
	{
		if (!unlockables[i].conditionset)
			continue;
		gamedata->unlocked[i] = true;
	}

	// Unlock all hidden levels.
	for (i = 0; i < nummapheaders; i++)
	{
		mapheaderinfo[i]->records.mapvisited = MV_MAX;
	}

	gamedata->gonerlevel = GDGONER_DONE;
	gamedata->sealedswapalerted = true;

	// You have been warned!
	G_SaveGameData();
}

static int bottomtext_x = 320*FRACUNIT;
static int skidrow_tick_frac = 0;
patch_t* checkerpatches[2];

static void M_SkidrowAccept(INT32 choice)
{
	if (choice != MA_YES)
		return;

	skidrow.ticker = 0;
	skidrow.curline = 0;
	skidrow.linetime = 0;
	skidrow.waitline = 0;
	skidrow.bottomtext_line = 0;
	bottomtext_x = 320*FRACUNIT;
	skidrow_tick_frac = 0;
	checkerpatches[0] = static_cast<patch_t *>(W_CachePatchName("~027", PU_CACHE));
	checkerpatches[1] = static_cast<patch_t *>(W_CachePatchName("~020", PU_CACHE));

	M_SetupNextMenu(&MISC_SkidrowDef, false);

	// Done here to avoid immediate music credit
	Music_Remap("menu_nocred", "4MAT");
	Music_Play("menu_nocred");
	
	f_devmode();
}

void M_Skidrow(INT32 choice)
{
	M_StartMessage("AAAAACK!", M_GetText("This option will unlock\x85 EVERYTHING.\n\nAll unlockables are permanently saved to your save file. If you still want to unlock stuff, PLEASE do not proceed.\n\nYou have been warned."), M_SkidrowAccept, MM_YESNO, "Start cracktro", "Nevermind");
}

// :-)
static std::string SkidrowText[] =
{
	"* KART ROW *",
	"PRESENTS",
	"ANOTHER LIGHTSPEED RELEASE",
	"",
	"RING RACERS DEV MODE FINAL",
	"",
	"DONE BY KART ROW PRODUCTIONS IN 2024",
	"CALL OUR BOARDS WORLDWIDE TO GET THE",
	"NEWEST STUFF AND OUR LATEST PRODUCTIONS.",
	"SEE YOU LATER IN ANOTHER INTRO !!!",
};
static int SkidrowTextColors[] = 
{
	V_REDMAP,
	V_ORANGEMAP,
	V_YELLOWMAP,
	0,
	V_GREENMAP,
	0,
	V_SKYMAP,
	V_BLUEMAP,
	V_LAVENDERMAP,
	V_MAGENTAMAP,
};
static int skidrow_length = 9;
// rainbow colors for drawfill
static UINT8 skidrow_rainbow_fill[] = {
	37, 55, 53, 73, 188, 112, 96, 121, 255, 134, 152, 166, 164, 183, 182, 215
};
// wait line durations
static int Skidrow_WaitLine[] =
{
	TICRATE,
	TICRATE,
	TICRATE,
	2*TICRATE,
	TICRATE,
	2*TICRATE,
	TICRATE/4,
	TICRATE/4,
	TICRATE/3,
	0,
};
static std::string SkidrowBottomText[] =
{
	"~ KART ROW ~",
	"- THE LEADING FORCE -",
	"IS BACK ON YOUR SCREEN WITH:",
	"RING RACERS FROM KARTKREW",
	"CRACKED BY KART ROW IN THE YEAR OF 2024 !!!",
	"IF YOU WANT TO BUY THE LATEST STUFF FOR PC WRITE TO PLK: HELLSCAPE 4001 !!!",
	"HELLO TO ALL OUR BENEFACTORS AND SUPPORTERS",
	"1 BILLION YEARS !!!",
	"SORRY FOR THE DISAPPOINTING WAVY TEXT AND BACKGROUND.....",
	"ITS VERY HARD TO MAKE ADVANCED EFFECTS WITH WHAT I HAVE.....",
	"THIS MENU SCREEN TOOK A WHILE AND WAS PROBABLY A WASTE OF TIME TO PROGRAM !! WOOHOO !!",
	"IF YOU USE THIS PIECE OF SOFTWARE, PLEASE CONSIDER SUPPORTING THE DEVS.",
	"      ... SCROLL RESTARTS ...      ",
};

// Space background
static void M_DrawSkidrowBack(void)
{
	INT32 x = 0, y = 0, i = 0, k = 0;
	
	// black bg ofc
	V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 31);
	
	V_SetClipRect(
		4 * FRACUNIT,
		5 * FRACUNIT,
		(BASEVIDWIDTH-8) * FRACUNIT,
		(BASEVIDHEIGHT-20) * FRACUNIT,
		0
	);
	
	// HAYA: Reworked this from it's initial impl to allow the damn thing to interpolate
	// Oh and disregard the 320x200 grid
	skidrow_tick_frac += R_UsingFrameInterpolation() ? renderdeltatics : FRACUNIT;
	int lowerbackxscroll = (skidrow_tick_frac * 2) % (64 << FRACBITS);
	int higherbackxscroll = skidrow_tick_frac % (64 << FRACBITS);
	
	INT32 backoffs = FSIN((ANG10/FRACUNIT)*(skidrow_tick_frac/2))*16;
	
#define VIDWIDTH (BASEVIDWIDTH << FRACBITS)
#define VIDHEIGHT (BASEVIDHEIGHT << FRACBITS)
#define DrawThirtySquare(x, y, patch) V_DrawStretchyFixedPatch(x, y, FRACUNIT/2, FRACUNIT/2, 0, patch, NULL)
	
	// checkerboards
	for (x = -48*FRACUNIT; x < VIDWIDTH+lowerbackxscroll; x += 64*FRACUNIT)
		for (y = -80*FRACUNIT; y < VIDHEIGHT-backoffs; y += 64*FRACUNIT)
			DrawThirtySquare(x-lowerbackxscroll, y+backoffs, checkerpatches[0]); // V_DrawFill doesn't support fracunit coords :(
	
	// checkerboards 2
	for (x = -16*FRACUNIT; x < VIDWIDTH+lowerbackxscroll; x += 64*FRACUNIT)
		for (y = -112*FRACUNIT; y < VIDHEIGHT-backoffs; y += 64*FRACUNIT)
			DrawThirtySquare(x-lowerbackxscroll, y+backoffs, checkerpatches[0]);
		
	backoffs = FSIN((ANG10/FRACUNIT)*(skidrow_tick_frac/8))*16;
	
	// checkerboards
	for (x = 0; x < VIDWIDTH+higherbackxscroll; x += 64*FRACUNIT)
		for (y = -64*FRACUNIT; y < VIDHEIGHT-backoffs; y += 64*FRACUNIT)
			DrawThirtySquare(x-higherbackxscroll, y+backoffs, checkerpatches[1]);
	
	// checkerboards 2
	for (x = 32*FRACUNIT; x < VIDWIDTH+higherbackxscroll; x += 64*FRACUNIT)
		for (y = -32*FRACUNIT; y < VIDHEIGHT-backoffs; y += 64*FRACUNIT)
			DrawThirtySquare(x-higherbackxscroll, y+backoffs, checkerpatches[1]);
		
#undef VIDWIDTH
#undef VIDHEIGHT
#undef DrawThirtySquare
	
	V_ClearClipRect();
	
	x = 0; y = 2; i = 0;
	
	// top
	while (x < BASEVIDWIDTH)
	{
		V_DrawFill(x, y, 10, 1, skidrow_rainbow_fill[(i+(skidrow.ticker))%16]);
		x += 10; i++;
	}
	
	x = BASEVIDWIDTH; i = 0; y = BASEVIDHEIGHT-13;
	
	// bottom
	while (x > 0)
	{
		V_DrawFill(x-10, y, 10, 1, skidrow_rainbow_fill[(i+(skidrow.ticker))%16]);
		x -= 10; i++;
	}
	
	// bottom text
	V_DrawThinString(4, BASEVIDHEIGHT-10, V_GRAYMAP, "Press \xBB \xBD to exit");
}

// Tick text and the like
static void M_SkidrowTick(void)
{
	skidrow.ticker++;
	
	if (skidrow.ticker < TICRATE/2)
		return;
	
	if (skidrow.linetime < SkidrowText[skidrow.curline].length())
		skidrow.linetime++;
	else
	{
		skidrow.waitline++;
		if (skidrow.waitline > Skidrow_WaitLine[skidrow.curline])
		{
			if (skidrow.curline >= skidrow_length) return;
			skidrow.curline++;
			skidrow.linetime = 0;
			skidrow.waitline = 0;
		}
	}
}

// Text
static void M_DrawSkidrow(void)
{
	int i;
	int yshift = 0;
	
	// Moved here for interp
	fixed_t frac = R_UsingFrameInterpolation() ? renderdeltatics : FRACUNIT;
	
	if (bottomtext_x < -(V_LSTitleLowStringWidth(SkidrowBottomText[skidrow.bottomtext_line].c_str(), V_MONOSPACE) << FRACBITS))
	{
		bottomtext_x = BASEVIDWIDTH << FRACBITS;
		// wrap
		skidrow.bottomtext_line++;
		INT32 arrsize = (sizeof (SkidrowBottomText))/(sizeof (SkidrowBottomText[0]));
		if (skidrow.bottomtext_line >= arrsize)
			skidrow.bottomtext_line = 0;
	} 
	else
	{
		bottomtext_x -= 6 * frac; // fast
	}

	V_DrawLSTitleLowString(bottomtext_x >> FRACBITS, BASEVIDHEIGHT/2 - 15, V_SINESCROLL|V_MONOSPACE, SkidrowBottomText[skidrow.bottomtext_line].c_str());
	
	// center
	int centerx = (BASEVIDHEIGHT/2)-(skidrow_length*10)/2;
	// draw all original text
	for (i = 0; i < skidrow.curline; i++)
	{
		using srb2::Draw;

		Draw::TextElement text(SkidrowText[i]);
		text.font(Draw::Font::kSkidrow);
		
		Draw(160, centerx+yshift).flags(V_MONOSPACE|SkidrowTextColors[i]).align(Draw::Align::kCenter).text(text);
		
		yshift += 6;
	}
	
	// draw current text
	using srb2::Draw;
	
	std::string msg = SkidrowText[skidrow.curline];
	UINT8 sublen = skidrow.linetime;
	std::string submsg = msg.substr(0, sublen);
	
	Draw::TextElement text(submsg);
	text.font(Draw::Font::kSkidrow);
	
	// We don't exactly want auto alignment here. Do alignment ourselves!
#define V_MediumStringWidth( string,option ) \
	V__IntegerStringWidth ( FRACUNIT,option,MED_FONT,string )
	
	Draw(160-(V_MediumStringWidth(msg.c_str(), V_MONOSPACE)/2)-0.5, centerx+yshift).flags(V_MONOSPACE|SkidrowTextColors[skidrow.curline]).text(text);
	
#undef V_MediumStringWidth
}

menu_t MISC_SkidrowDef = {
	sizeof (MISC_SkidrowMenu)/sizeof (menuitem_t),
	NULL,
	0,
	MISC_SkidrowMenu,
	0, 0,
	0, 0,
	MBF_SOUNDLESS|MBF_DRAWBGWHILEPLAYING,
	".",
	98, 0,
	M_DrawSkidrow,
	M_DrawSkidrowBack,
	M_SkidrowTick,
	NULL,
	NULL,
	NULL,
};
