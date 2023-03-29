// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2018-2022 by Sally Cochenour
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
// \brief Power Level system

#include "k_pwrlv.h"
#include "d_netcmd.h"
#include "g_game.h"
#include "s_sound.h"
#include "m_random.h"
#include "m_cond.h" // M_UpdateUnlockablesAndExtraEmblems
#include "p_tick.h" // leveltime
#include "k_grandprix.h"
#include "k_profiles.h"

// Client-sided calculations done for Power Levels.
// This is done so that clients will never be able to hack someone else's score over the server.
UINT16 clientpowerlevels[MAXPLAYERS][PWRLV_NUMTYPES];

// Total calculated power add during the match,
// totalled at the end of the round.
INT16 clientPowerAdd[MAXPLAYERS];

// Players who spectated mid-race
UINT8 spectateGriefed = 0;

// Game setting scrambles based on server Power Level
SINT8 speedscramble = -1;
SINT8 encorescramble = -1;

consvar_t cv_debugencorevote = CVAR_INIT ("debugencorevote", "Off", CV_CHEAT|CV_NETVAR, CV_OnOff, NULL);

SINT8 K_UsingPowerLevels(void)
{
	SINT8 pt = PWRLV_DISABLED;

	if (!cv_kartusepwrlv.value || !(netgame || (demo.playback && demo.netgame)) || grandprixinfo.gp == true)
	{
		return PWRLV_DISABLED;
	}

	if (gametype == GT_RACE)
	{
		pt = PWRLV_RACE;
	}
	else if (gametype == GT_BATTLE)
	{
		pt = PWRLV_BATTLE;
	}

	return pt;
}

void K_ClearClientPowerLevels(void)
{
	memset(clientpowerlevels, 0, sizeof clientpowerlevels);
	memset(clientPowerAdd, 0, sizeof clientPowerAdd);
}

// Adapted from this: http://wiki.tockdom.com/wiki/Player_Rating
INT16 K_CalculatePowerLevelInc(INT16 diff)
{
	INT16 control[10] = {0,0,0,1,8,50,125,125,125,125};
	fixed_t increment = 0;
	fixed_t x;
	UINT8 j;

#define MAXDIFF (PWRLVRECORD_MAX - 1)
	if (diff > MAXDIFF)
		diff = MAXDIFF;
	if (diff < -MAXDIFF)
		diff = -MAXDIFF;
#undef MAXDIFF

	x = ((diff-2)<<FRACBITS) / PWRLVRECORD_MEDIAN;

	for (j = 3; j < 10; j++) // Just skipping to 3 since 0 thru 2 will always just add 0...
	{
		fixed_t f = abs(x - ((j-4)<<FRACBITS));
		fixed_t add;

		if (f >= (2<<FRACBITS))
		{
			continue; //add = 0;
		}
		else if (f >= (1<<FRACBITS))
		{
			fixed_t f2 = (2<<FRACBITS) - f;
			add = FixedMul(FixedMul(f2, f2), f2) / 6;
		}
		else
		{
			add = ((3*FixedMul(FixedMul(f, f), f)) - (6*FixedMul(f, f)) + (4<<FRACBITS)) / 6;
		}

		increment += (add * control[j]);
	}

	return (INT16)(increment >> FRACBITS);
}

INT16 K_PowerLevelPlacementScore(player_t *player)
{
	if ((player->pflags & PF_NOCONTEST) || (player->spectator))
	{
		return 0;
	}

	if (gametyperules & GTR_CIRCUIT)
	{
		return MAXPLAYERS - player->position;
	}
	else
	{
		return player->score;
	}
}

INT16 K_CalculatePowerLevelAvg(void)
{
	fixed_t avg = 0;
	UINT8 div = 0;
	SINT8 t = PWRLV_DISABLED;
	UINT8 i;

	if (!netgame || !cv_kartusepwrlv.value)
	{
		CONS_Debug(DBG_GAMELOGIC, "Not in a netgame, or not using power levels -- no average.\n");
		return 0; // No average.
	}

	if ((gametyperules & GTR_CIRCUIT))
		t = PWRLV_RACE;
	else if ((gametyperules & GTR_BUMPERS))
		t = PWRLV_BATTLE;

	if (t == PWRLV_DISABLED)
	{
		CONS_Debug(DBG_GAMELOGIC, "Could not set a power level type -- no average.\n");
		return 0; // Hmm?!
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator
			|| clientpowerlevels[i][t] == 0) // splitscreen player
			continue;

		avg += (clientpowerlevels[i][t] << FRACBITS);
		div++;
	}

	if (!div)
	{
		CONS_Debug(DBG_GAMELOGIC, "Found no players -- no average.\n");
		return 0; // No average.
	}

	avg /= div;

	return (INT16)(avg >> FRACBITS);
}

void K_UpdatePowerLevels(player_t *player, UINT8 lap, boolean forfeit)
{
	const UINT8 playerNum = player - players;
	const boolean exitBonus = ((lap > numlaps) || (player->pflags & PF_NOCONTEST));

	SINT8 powerType = K_UsingPowerLevels();

	INT16 yourScore = 0;
	UINT16 yourPower = 0;

	UINT8 i;

	// Compare every single player against each other for power level increases.
	// Every player you won against gives you more points, and vice versa.
	// The amount of points won per match-up depends on the difference between the loser's power and the winner's power.
	// See K_CalculatePowerLevelInc for more info.

	if (powerType == PWRLV_DISABLED)
	{
		return;
	}

	if (!playeringame[playerNum] || player->spectator)
	{
		return;
	}

	//CONS_Printf("\n========\n");
	//CONS_Printf("* Power Level change for player %s (LAP %d) *\n", player_names[playerNum], lap);
	//CONS_Printf("========\n");

	yourPower = clientpowerlevels[playerNum][powerType];
	if (yourPower == 0)
	{
		// Guests don't record power level changes.
		return;
	}

	//CONS_Printf("%s's PWR.LV: %d\n", player_names[playerNum], yourPower);

	yourScore = K_PowerLevelPlacementScore(player);
	//CONS_Printf("%s's gametype score: %d\n", player_names[playerNum], yourScore);

	//CONS_Printf("========\n");
	for (i = 0; i < MAXPLAYERS; i++)
	{
		UINT16 theirScore = 0;
		INT16 theirPower = 0;

		INT16 diff = 0; // Loser PWR.LV - Winner PWR.LV
		INT16 inc = 0; // Total pt increment

		boolean won = false;

		if (i == playerNum) // Same person
		{
			continue;
		}

		if (!playeringame[i] || players[i].spectator)
		{
			continue;
		}

		//CONS_Printf("%s VS %s:\n", player_names[playerNum], player_names[i]);

		theirPower = clientpowerlevels[i][powerType];
		if (theirPower == 0)
		{
			// No power level (splitscreen guests, bots)
			continue;
		}

		//CONS_Printf("%s's PWR.LV: %d\n", player_names[i], theirPower);

		theirScore = K_PowerLevelPlacementScore(&players[i]);
		//CONS_Printf("%s's gametype score: %d\n", player_names[i], theirScore);

		if (yourScore == theirScore && forfeit == false) // Tie -- neither get any points for this match up.
		{
			//CONS_Printf("TIE, no change.\n");
			continue;
		}

		won = (yourScore > theirScore);

		if (won == true && forfeit == false) // This player won!
		{
			diff = theirPower - yourPower;
			inc += K_CalculatePowerLevelInc(diff);
			//CONS_Printf("WON! Diff is %d, increment is %d\n", diff, inc);
		}
		else // This player lost...
		{
			diff = yourPower - theirPower;
			inc -= K_CalculatePowerLevelInc(diff);
			//CONS_Printf("LOST... Diff is %d, increment is %d\n", diff, inc);
		}

		if (exitBonus == false)
		{
			INT16 prevInc = inc;

			inc /= max(numlaps-1, 1);

			if (inc == 0)
			{
				if (prevInc > 0)
				{
					inc = 1;
				}
				else if (prevInc < 0)
				{
					inc = -1;
				}
			}

			//CONS_Printf("Reduced (%d / %d = %d) because it's not the end of the race\n", prevInc, numlaps, inc);
		}

		//CONS_Printf("========\n");

		if (inc == 0)
		{
			CONS_Printf("Total Result: No increment, no change.\n");
			continue;
		}

		//CONS_Printf("Total Result:\n");
		//CONS_Printf("Increment: %d\n", inc);

		//CONS_Printf("%s current: %d\n", player_names[playerNum], clientPowerAdd[playerNum]);
		clientPowerAdd[playerNum] += inc;
		//CONS_Printf("%s final: %d\n", player_names[playerNum], clientPowerAdd[playerNum]);

		//CONS_Printf("%s current: %d\n", player_names[i], clientPowerAdd[i]);
		clientPowerAdd[i] -= inc;
		//CONS_Printf("%s final: %d\n", player_names[i], clientPowerAdd[i]);

		//CONS_Printf("========\n");
	}
}

void K_UpdatePowerLevelsOnFailure(player_t *player)
{
	// Update upon spectate / quit / NO CONTEST
	INT16 lapsLeft = 0;
	UINT8 i;

	lapsLeft = (numlaps - player->latestlap) + 1;

	if (lapsLeft <= 0)
	{
		return;
	}

	for (i = 0; i < lapsLeft; i++)
	{
		K_UpdatePowerLevels(player, player->latestlap + (i + 1), true);
	}

	player->latestlap = numlaps+1;
}

INT16 K_FinalPowerIncrement(player_t *player, INT16 yourPower, INT16 baseInc)
{
	INT16 inc = baseInc;
	UINT8 numPlayers = 0;
	UINT8 i;

	if (yourPower == 0)
	{
		// Guests don't record power level changes.
		return 0;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
		{
			continue;
		}

		numPlayers++;
	}

	if (inc <= 0)
	{
		if (player->position == 1)
		{
			// Won the whole match?
			// Get at least one point.
			inc = 1;
		}
#if 0
		else
		{
			// You trade points in 1v1s,
			// but is more lenient in bigger lobbies.
			inc /= max(1, numPlayers-1);

			if (inc == 0)
			{
				if (baseInc > 0)
				{
					inc = 1;
				}
				else if (baseInc < 0)
				{
					inc = -1;
				}
			}
		}
#endif
	}

	if (yourPower + inc > PWRLVRECORD_MAX)
	{
		inc -= ((yourPower + inc) - PWRLVRECORD_MAX);
	}

	if (yourPower + inc < PWRLVRECORD_MIN)
	{
		inc -= ((yourPower + inc) - PWRLVRECORD_MIN);
	}

	return inc;
}

void K_CashInPowerLevels(void)
{
	SINT8 powerType = K_UsingPowerLevels();
	UINT8 i;
	boolean gamedataupdate;

	//CONS_Printf("\n========\n");
	//CONS_Printf("Cashing in power level changes...\n");
	//CONS_Printf("========\n");

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] == true && powerType != PWRLV_DISABLED)
		{
			profile_t *pr = PR_GetPlayerProfile(&players[i]);
			INT16 inc = K_FinalPowerIncrement(&players[i], clientpowerlevels[i][powerType], clientPowerAdd[i]);

			clientpowerlevels[i][powerType] += inc;

			//CONS_Printf("%s: %d -> %d (%d)\n", player_names[i], clientpowerlevels[i][powerType] - inc, clientpowerlevels[i][powerType], inc);

			if (pr != NULL && inc != 0)
			{
				pr->powerlevels[powerType] = clientpowerlevels[i][powerType];

				gamedataupdate = true;
			}
		}

		clientPowerAdd[i] = 0;
	}

	if (gamedataupdate)
	{
		M_UpdateUnlockablesAndExtraEmblems(true, true);
		G_SaveGameData();
	}

	//CONS_Printf("========\n");
}

void K_SetPowerLevelScrambles(SINT8 powertype)
{
	switch (powertype)
	{
		case PWRLV_RACE:
			if (cv_kartspeed.value == -1 || cv_kartencore.value == -1)
			{
				UINT8 speed = KARTSPEED_NORMAL;
				boolean encore = false;
				INT16 avg = 0, min = 0;
				UINT8 i, t = 1;

				avg = K_CalculatePowerLevelAvg();

				for (i = 0; i < MAXPLAYERS; i++)
				{
					if (!playeringame[i] || players[i].spectator
						|| clientpowerlevels[i][t] == 0) // splitscreen player
						continue;

					if (min == 0 || clientpowerlevels[i][0] < min)
						min = clientpowerlevels[i][0];
				}

				CONS_Debug(DBG_GAMELOGIC, "Min: %d, Avg: %d\n", min, avg);

				if (avg == 0 || min == 0)
				{
					CONS_Debug(DBG_GAMELOGIC, "No average/minimum, no scramblin'.\n");
					speedscramble = encorescramble = -1;
					return;
				}

				if (min >= 7800)
				{
					if (avg >= 8200)
						t = 5;
					else
						t = 4;
				}
				else if (min >= 6800)
				{
					if (avg >= 7200)
						t = 4;
					else
						t = 3;
				}
				else if (min >= 5800)
				{
					if (avg >= 6200)
						t = 3;
					else
						t = 2;
				}
				else if (min >= 3800)
				{
					if (avg >= 4200)
						t = 2;
					else
						t = 1;
				}
#if 1
				else
					t = 1;
#else
				else if (min >= 1800)
				{
					if (avg >= 2200)
						t = 1;
					else
						t = 0;
				}
				else
					t = 0;
#endif

				CONS_Debug(DBG_GAMELOGIC, "Table position: %d\n", t);

				switch (t)
				{
					case 5:
						speed = KARTSPEED_HARD;
						encore = P_RandomChance(PR_RULESCRAMBLE, FRACUNIT>>1);
						break;
					case 4:
						speed = P_RandomChance(PR_RULESCRAMBLE, (7<<FRACBITS)/10) ? KARTSPEED_HARD : KARTSPEED_NORMAL;
						encore = P_RandomChance(PR_RULESCRAMBLE, FRACUNIT>>1);
						break;
					case 3:
						speed = P_RandomChance(PR_RULESCRAMBLE, (3<<FRACBITS)/10) ? KARTSPEED_HARD : KARTSPEED_NORMAL;
						encore = P_RandomChance(PR_RULESCRAMBLE, FRACUNIT>>2);
						break;
					case 2:
						speed = KARTSPEED_NORMAL;
						encore = P_RandomChance(PR_RULESCRAMBLE, FRACUNIT>>3);
						break;
					case 1: default:
						speed = KARTSPEED_NORMAL;
						encore = false;
						break;
					case 0:
						speed = P_RandomChance(PR_RULESCRAMBLE, (3<<FRACBITS)/10) ? KARTSPEED_EASY : KARTSPEED_NORMAL;
						encore = false;
						break;
				}

				CONS_Debug(DBG_GAMELOGIC, "Rolled speed: %d\n", speed);
				CONS_Debug(DBG_GAMELOGIC, "Rolled encore: %s\n", (encore ? "true" : "false"));

				if (cv_kartspeed.value == KARTSPEED_AUTO)
					speedscramble = speed;
				else
					speedscramble = -1;

				if (cv_debugencorevote.value)
					encorescramble = 1;
				else if (cv_kartencore.value == -1)
					encorescramble = (encore ? 1 : 0);
				else
					encorescramble = -1;
			}
			break;
		default:
			break;
	}
}

void K_PlayerForfeit(UINT8 playerNum, boolean pointLoss)
{
	profile_t *pr;
	UINT8 p = 0;

	SINT8 powerType = PWRLV_DISABLED;
	UINT16 yourPower = 0;
	INT16 inc = 0;

	UINT8 i;

	// power level & spectating is netgames only
	if (!netgame)
	{
		return;
	}

	// Hey, I just got here!
	if (players[playerNum].jointime <= 1)
	{
		return;
	}

	// 20 sec into a match counts as a forfeit -- automatic loss against every other player in the match.
	if (gamestate != GS_LEVEL || leveltime <= starttime+(20*TICRATE))
	{
		return;
	}

	spectateGriefed++;

	// This server isn't using power levels, so don't mess with them.
	if (!cv_kartusepwrlv.value)
	{
		return;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if ((playeringame[i] && !players[i].spectator)
			|| (i == playerNum))
		{
			p++;
		}
	}

	if (p < 2) // no players
	{
		return;
	}

	powerType = K_UsingPowerLevels();

	if (powerType == PWRLV_DISABLED) // No power type?!
	{
		return;
	}

	yourPower = clientpowerlevels[playerNum][powerType];
	if (yourPower == 0) // splitscreen guests don't record power level changes
	{
		return;
	}

	K_UpdatePowerLevelsOnFailure(&players[playerNum]);
	inc = K_FinalPowerIncrement(&players[playerNum], yourPower, clientPowerAdd[playerNum]);

	if (inc == 0)
	{
		// No change
		return;
	}

	if (inc < 0 && pointLoss == false)
	{
		// Don't record point losses for sync-out / crashes.
		return;
	}

	pr = PR_GetPlayerProfile(&players[playerNum]);
	if (pr != NULL)
	{
		pr->powerlevels[powerType] = yourPower + inc;

		M_UpdateUnlockablesAndExtraEmblems(true, true);
		G_SaveGameData();
	}
}
