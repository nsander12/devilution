/**
 * @file gamestate.h
 *
 * Lightweight facade over core gameplay globals.
 */
#ifndef __GAMESTATE_H__
#define __GAMESTATE_H__

typedef struct GameState {
	PlayerStruct *players;
	MonsterStruct *monsters;
	MissileStruct *missiles;
	ItemStruct *items;
	ObjectStruct *objects;
	int *missileactive;
	int *nummissiles;
	int *nummonsters;
	int *numitems;
	BOOL *missilePreFlag;
	int *force_redraw;
	BYTE *currlevel;
	int *ViewX;
	int *ViewY;
	char (*dFlags)[MAXDUNY];
	char (*dMissile)[MAXDUNY];
} GameState;

void InitGameState();
const GameState *GetGameState();

#endif /* __GAMESTATE_H__ */
