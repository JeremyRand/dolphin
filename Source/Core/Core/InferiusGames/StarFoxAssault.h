#ifndef INFERIUS_STAR_FOX_ASSAULT_H
#define INFERIUS_STAR_FOX_ASSAULT_H

#include "Core/InferiusGames/InferiusGame.h"

class InferiusGameStarFoxAssault : public InferiusGame
{
public:
	char* GetSyncStatePointer(Inferius::InferiusPlayerData *data);
	size_t GetSyncStateSize(Inferius::InferiusPlayerData *data);
	int MaxPlayerLimit();
	int GetPlayerIndex(Inferius::InferiusPlayerData *data);
	int GetFrameIndex(Inferius::InferiusPlayerData *data);
	int MultiplexRemotePriority(Inferius::InferiusPlayerData *local, Inferius::InferiusPlayerData *remote);
	void PadPostHook(Inferius::InferiusPlayerData *data, u8 _numPAD, GCPadStatus* _pPADStatus);
};

#endif