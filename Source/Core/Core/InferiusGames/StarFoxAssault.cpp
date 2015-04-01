#include "Core/InferiusGames/StarFoxAssault.h"

char* InferiusGameStarFoxAssault::GetSyncStatePointer(Inferius::InferiusPlayerData *data)
{
	return (char*) & (data->sync.StarFoxAssault);
}

size_t InferiusGameStarFoxAssault::GetSyncStateSize(Inferius::InferiusPlayerData *data)
{
	return sizeof(data->sync.StarFoxAssault);
}

int InferiusGameStarFoxAssault::MaxPlayerLimit()
{
	return 4;
}

int InferiusGameStarFoxAssault::GetPlayerIndex(Inferius::InferiusPlayerData *data)
{
	return data->sync.StarFoxAssault.player_num;
}

int InferiusGameStarFoxAssault::GetFrameIndex(Inferius::InferiusPlayerData *data)
{
	return data->sync.StarFoxAssault.frame_num;
}

int InferiusGameStarFoxAssault::MultiplexRemotePriority(Inferius::InferiusPlayerData *local, Inferius::InferiusPlayerData *remote)
{
	// TODO: handle player angle/FOV; players who are invisible to each other shouldn't be prioritized
	
	float dx, dy, dz, d2;
	
	dx = remote->sync.StarFoxAssault.X - local->sync.StarFoxAssault.X;
	dy = remote->sync.StarFoxAssault.Y - local->sync.StarFoxAssault.Y;
	dz = remote->sync.StarFoxAssault.Z - local->sync.StarFoxAssault.Z;
	
	d2 = dx*dx + dy*dy + dz*dz;
	
	return d2;
}

void InferiusGameStarFoxAssault::PadPostHook(Inferius::InferiusPlayerData *data, u8 _numPAD, GCPadStatus* _pPADStatus)
{
	if(Inferius::InferiusHeadTrackingEnabled())
	{
		if(abs(_pPADStatus->substickX-127) > 5)
		{
			//data->sync.StarFoxAssault.yaw -= ( ( (float)(_pPADStatus->substickX) / 127.5 - 1.0 ) ) * (2.0 * M_PI) / (5.0 * 60.0);
			data->sync.StarFoxAssault.yaw -= ( ( (float)(_pPADStatus->substickX) / 127.5 - 1.0 ) ) * (2.0 * M_PI) / (5.0 * 150.0); // 150 VPS rather than 60 VPS
			
			printf("                   Player Yaw = %.4f\n", data->sync.StarFoxAssault.yaw);
		}
	}
}
