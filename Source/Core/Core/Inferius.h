// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __INFERIUS_h__
#define __INFERIUS_h__

#include "Common/Common.h"

#include "InputCommon/GCPadStatus.h"

namespace Inferius
{
	struct InferiusPlayerData
	{
		union
		{
			struct
			{
				u8 player_num;
				u8 frame_num;
				
				GCPadStatus pad;
				
				float X;
				float Y;
				float Z;
				
				// ToDo: populate these
				float prev_X;
				float prev_Y;
				float prev_Z;
				
				float Xr; // radians
				float Yr;
				float Zr;
				
				u16 rings;
				
				u8 character;
				
				float progress;
			} SonicAdventure2Battle;
			
			struct
			{
				u8 player_num;
				u8 frame_num;
				
				GCPadStatus pad;
				
				float X;
				float Y;
				float Z;
				
				float yaw;
				float pitch;
			} StarFoxAssault;
		} sync;
		
		union
		{
			struct
			{
				u8 player_num;
				u8 frame_num;
				
				float size_X;
				float size_Y;
				float size_Z;
				
				float cam_X;
				float cam_Y;
				float cam_Z;
				
				float cam_look_at_X;
				float cam_look_at_Y;
				float cam_look_at_Z;
				
				u16 prev_rings; // ToDo: populate this field
				
			} SonicAdventure2Battle;
		} internal;
	};
	
	bool InferiusEnabled();
	bool InferiusHeadTrackingEnabled();
	bool InferiusFirstPersonEnabled();
	bool InferiusNetplayEnabled();
	
	void FreeLookRotate(float &yaw, float &pitch, float &roll);
	void ProcessPad(u8 _numPAD, GCPadStatus* _pPADStatus);
	void ApplySync();

	void HLEXYZPositionRead0();
	void HLEPlayerAimWrite0();
	void HLECamMatrixWrite0();
	void HLEWeaponAimVectorWrite0();
	void HLEPlayerAimRead0();

}	// namespace Inferius

#endif
