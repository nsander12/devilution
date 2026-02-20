/**
 * @file gamestate.cpp
 *
 * Game state facade implementation.
 */
#include "all.h"

static GameState sgGameState;

void InitGameState()
{
	sgGameState.players = plr;
	sgGameState.monsters = monster;
	sgGameState.missiles = missile;
	sgGameState.items = item;
	sgGameState.objects = object;
	sgGameState.missileactive = missileactive;
	sgGameState.nummissiles = &nummissiles;
	sgGameState.nummonsters = &nummonsters;
	sgGameState.numitems = &numitems;
	sgGameState.missilePreFlag = &MissilePreFlag;
	sgGameState.force_redraw = &force_redraw;
	sgGameState.currlevel = &currlevel;
	sgGameState.ViewX = &ViewX;
	sgGameState.ViewY = &ViewY;
	sgGameState.dFlags = dFlags;
	sgGameState.dMissile = dMissile;
}

const GameState *GetGameState()
{
	return &sgGameState;
}
