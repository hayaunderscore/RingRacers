// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) by Sonic Team Junior
// Copyright (C) by Kart Krew
// Copyright (C) by Sally "TehRealSalt" Cochenour
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_dialogue.cpp
/// \brief Basic text prompts

#include "k_dialogue.hpp"
#include "k_dialogue.h"

#include <string>
#include <algorithm>

#include "info.h"
#include "sounds.h"
#include "g_game.h"
#include "v_video.h"
#include "r_draw.h"
#include "m_easing.h"
#include "r_skins.h"
#include "s_sound.h"
#include "z_zone.h"

#include "v_draw.hpp"

#include "acs/interface.h"

using srb2::Dialogue;

void Dialogue::Init(void)
{
	active = true;
	dismissable = false;
	syllable = true;
}

void Dialogue::SetSpeaker(void)
{
	// Unset speaker
	speaker.clear();

	portrait = nullptr;
	portraitColormap = nullptr;

	voiceSfx = sfx_None;
}

void Dialogue::SetSpeaker(std::string skinName, int portraitID)
{
	Init();

	// Set speaker based on a skin
	int skinID = -1;
	if (!skinName.empty())
	{
		skinID = R_SkinAvailable(skinName.c_str());
	}

	if (skinID >= 0 && skinID < numskins)
	{
		const skin_t *skin = &skins[skinID];
		const spritedef_t *sprdef = &skin->sprites[SPR2_TALK];

		if (sprdef->numframes > 0)
		{
			portraitID %= sprdef->numframes;
		}

		const spriteframe_t *sprframe = &sprdef->spriteframes[portraitID];

		speaker = skin->realname;

		portrait = static_cast<patch_t *>( W_CachePatchNum(sprframe->lumppat[0], PU_CACHE) );
		portraitColormap = R_GetTranslationColormap(skinID, static_cast<skincolornum_t>(skin->prefcolor), GTC_CACHE);

		voiceSfx = skin->soundsid[ S_sfx[sfx_ktalk].skinsound ];
	}
	else
	{
		SetSpeaker();
	}
}

void Dialogue::SetSpeaker(std::string name, patch_t *patch, UINT8 *colormap, sfxenum_t voice)
{
	Init();

	// Set custom speaker
	speaker = name;

	if (speaker.empty())
	{
		portrait = nullptr;
		portraitColormap = nullptr;
		voiceSfx = sfx_None;
		return;
	}

	portrait = patch;
	portraitColormap = colormap;

	voiceSfx = voice;
}

void Dialogue::NewText(std::string newText)
{
	Init();

	newText = V_ScaledWordWrap(
		290 << FRACBITS,
		FRACUNIT, FRACUNIT, FRACUNIT,
		0, HU_FONT,
		newText.c_str()
	);

	text.clear();

	textDest = newText;
	std::reverse(textDest.begin(), textDest.end());

	textTimer = kTextPunctPause;
	textSpeed = kTextSpeedDefault;
	textDone = false;
}

bool Dialogue::Active(void)
{
	return active;
}

bool Dialogue::TextDone(void)
{
	return textDone;
}

bool Dialogue::Dismissable(void)
{
	return dismissable;
}

void Dialogue::SetDismissable(bool value)
{
	dismissable = value;
}

void Dialogue::WriteText(void)
{
	bool voicePlayed = false;

	textTimer -= textSpeed;

	while (textTimer <= 0 && !textDest.empty())
	{
		char c = textDest.back();
		text.push_back(c);

		if (voicePlayed == false
			&& std::isprint(c)
			&& c != ' ')
		{
			if (syllable)
			{
				S_StopSoundByNum(voiceSfx);
				S_StartSound(nullptr, voiceSfx);
			}

			syllable = !syllable;
			voicePlayed = true;
		}

		if (c == '.' || c == ',' || c == ';' || c == '!' || c == '?')
		{
			// slow down for punctuation
			textTimer += kTextPunctPause;
		}
		else
		{
			textTimer += FRACUNIT;
		}

		textDest.pop_back();
	}

	textDone = (textTimer <= 0 && textDest.empty());
}

void Dialogue::CompleteText(void)
{
	while (!textDest.empty())
	{
		text.push_back( textDest.back() );
		textDest.pop_back();
	}

	textTimer = 0;
	textDone = true;
}

void Dialogue::Tick(void)
{
	if (Active())
	{
		if (slide < FRACUNIT)
		{
			slide += kSlideSpeed;
		}
	}
	else
	{
		if (slide > 0)
		{
			slide -= kSlideSpeed;

			if (slide <= 0)
			{
				Unset();
			}
		}
	}

	slide = std::clamp(slide, 0, FRACUNIT);

	if (slide != FRACUNIT)
	{
		return;
	}

	WriteText();

	if (Dismissable() == true)
	{
		bool pressed = (
			((players[serverplayer].cmd.buttons & BT_VOTE) == BT_VOTE) &&
			((players[serverplayer].oldcmd.buttons & BT_VOTE) == 0)
		);

		if (pressed == true)
		{
			if (TextDone())
			{
				Dismiss();
			}
			else
			{
				CompleteText();
			}
		}
	}
}

void Dialogue::Draw(void)
{
	if (slide == 0)
	{
		return;
	}

	srb2::Draw drawer = 
		srb2::Draw(
			0, FixedToFloat(Easing_OutCubic(slide, -78 * FRACUNIT, 0))
		).flags(V_SNAPTOTOP);

	drawer.patch("TUTDIAG1");

	if (portrait != nullptr)
	{
		drawer
			.xy(10, 41)
			.colormap(portraitColormap)
			.patch(portrait);
	}

	drawer
		.xy(45, 39)
		.font(srb2::Draw::Font::kConsole)
		.text( speaker.c_str() );

	drawer
		.xy(10, 3)
		.font(srb2::Draw::Font::kConsole)
		.text( text.c_str() );

	if (Dismissable() && TextDone() && Active())
	{
		drawer
			.xy(304, 7)
			.patch("TUTDIAG2");
	}
}

void Dialogue::Dismiss(void)
{
	active = false;
	dismissable = false;

	text.clear();
	textDest.clear();
}

void Dialogue::Unset(void)
{
	Dismiss();
	SetSpeaker();
}

/*
	Ideally, the Dialogue class would be on player_t instead of in global space
	for full multiplayer compatibility, but right now it's only being used for
	the tutorial, and I don't feel like writing network code. If you feel like
	doing that, then you can remove g_dialogue entirely.
*/

Dialogue g_dialogue;

void K_UnsetDialogue(void)
{
	g_dialogue.Unset();
}

void K_DrawDialogue(void)
{
	g_dialogue.Draw();
}

void K_TickDialogue(void)
{
	g_dialogue.Tick();
}
