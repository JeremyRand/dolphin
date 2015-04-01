#ifndef INFERIUS_GAME_H
#define INFERIUS_GAME_H

#include "Core/Inferius.h"

class InferiusGame
{
    
public:
    
    InferiusGame() {};
    
    virtual char* GetSyncStatePointer(Inferius::InferiusPlayerData *data) = 0;
    
    virtual size_t GetSyncStateSize(Inferius::InferiusPlayerData *data) = 0;
    
    virtual int MaxPlayerLimit() = 0;
    
    virtual int GetPlayerIndex(Inferius::InferiusPlayerData *data) = 0;
    
    virtual int GetFrameIndex(Inferius::InferiusPlayerData *data) = 0;
    
    virtual int MultiplexRemotePriority(Inferius::InferiusPlayerData *local, Inferius::InferiusPlayerData *remote) = 0;
    
    virtual void PadPostHook(Inferius::InferiusPlayerData *data, u8 _numPAD, GCPadStatus* _pPADStatus) = 0;
};


#endif
