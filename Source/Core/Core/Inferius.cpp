// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// GeckoTunnel Inferius
// Synchronization Enhancement Engine for Dolphin

#include "Core/Inferius.h"
#include "Core/InferiusGames/InferiusGame.h"
#include "Core/InferiusGames/StarFoxAssault.h"

#include <stdio.h>
#include <vector>
#include <unordered_set>

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"

#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"

#include "Core/PowerPC/PowerPC.h"

#include "Common/CommonPaths.h"
#include "Common/Thread.h"

#define _USE_MATH_DEFINES

#include <math.h>
#include "Common/MathUtil.h"

namespace Inferius
{

void Write_F32(const float _Data, const u32 _Address);
std::string GetCurrentGameID();
void Init();

void Write_F32(const float _Data, const u32 _Address)
{
	union
	{
		u32 i;
		float d;
	} cvt;
	cvt.d = _Data;
	Memory::Write_U32(cvt.i, _Address);
}

#if 0
bool enable_inferius = true;
bool enable_head_tracking = false;
bool enable_first_person = false;
bool enable_netplay = true;
#endif

double P1_Xr_smoothed_float = 0.0;
double P1_Yr_smoothed_float = 0.0;
double P1_Zr_smoothed_float = 0.0;

float head_yaw=0.0, head_pitch=0.0, head_roll=0.0;
float body_yaw=0.0, body_pitch=0.0, body_roll=0.0;
float yaw_offset=0.0;
	
bool m_initialized = false;

float m_rotation[3];

sf::UdpSocket m_socket;
std::size_t m_size_received;
sf::IpAddress m_ip_received;
unsigned short m_port_received;
sf::Socket::Status m_status;
//sf::IpAddress m_dest("127.0.0.1");
sf::IpAddress m_nullIP("0.0.0.0");
sf::IpAddress m_dest[31];
unsigned short m_dest_port[31];

#if 0

struct InferiusPlayerDataSonicAdventure2Battle
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
	
	float size_X;
	float size_Y;
	float size_Z;
	
	float cam_X;
	float cam_Y;
	float cam_Z;
	
	float cam_look_at_X;
	float cam_look_at_Y;
	float cam_look_at_Z;
	
	u16 rings;
	u16 prev_rings; // ToDo: populate this field
	
	u8 character;
	
	float progress;
} ;

struct InferiusPlayerDataStarFoxAssault
{
	u8 player_num;
	u8 frame_num;
	
	GCPadStatus pad;
	
	float X;
	float Y;
	float Z;
	
	/*
	// ToDo: populate these
	float prev_X;
	float prev_Y;
	float prev_Z;
	
	float Xr; // radians
	float Yr;
	float Zr;
	
	float size_X;
	float size_Y;
	float size_Z;
	
	float cam_X;
	float cam_Y;
	float cam_Z;
	
	float cam_look_at_X;
	float cam_look_at_Y;
	float cam_look_at_Z;
	
	
	u8 character;
	
	*/
} ;

#endif

std::vector<InferiusPlayerData*> current_player_data;
//std::vector<char*> current_player_data;
std::vector<bool> player_local;
std::vector<bool> player_local_override;
std::vector<bool> player_remote;
std::map<std::string, InferiusPlayerData*> player_mux_data;
//std::map<std::string, char*> player_mux_data;
std::unordered_set<std::string> teammates; // ToDo: handle teammates separately per local user

InferiusPlayerData* temp_player_data;
//char* temp_player_data;

std::map<std::string, int> max_players_for_game_id;

std::string GetCurrentGameID()
{
	u8 *id_bin = Memory::GetPointer(0x80000000);
	
	char id_str[7];
	
	for(int byte=0; byte<6; byte++)
	{
		id_str[byte] = id_bin[byte];
	}
	id_str[6] = 0;
	
	return std::string(id_str);
}

InferiusGame *current_inferius_game = NULL;


/*
typedef int (*GetPlayerNum_Template) (InferiusPlayerData&);
typedef int (*GetFrameNum_Template) (InferiusPlayerData&);

std::map<std::string, GetPlayerNum_Template> GetPlayerNum_Game;
std::map<std::string, GetFrameNum_Template> GetFrameNum_Game;
*/

void Init()
{
	printf("Initializing Inferius...\n");
	
	// Setup netplay peer addresses/ports.
	
	m_dest[0] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP0);
	m_dest[1] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP1);
	m_dest[2] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP2);
	m_dest[3] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP3);
	m_dest[4] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP4);
	m_dest[5] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP5);
	m_dest[6] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP6);
	m_dest[7] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP7);
	m_dest[8] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP8);
	m_dest[9] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP9);
	m_dest[10] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP10);
	m_dest[11] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP11);
	m_dest[12] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP12);
	m_dest[13] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP13);
	m_dest[14] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP14);
	m_dest[15] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP15);
	m_dest[16] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP16);
	m_dest[17] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP17);
	m_dest[18] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP18);
	m_dest[19] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP19);
	m_dest[20] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP20);
	m_dest[21] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP21);
	m_dest[22] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP22);
	m_dest[23] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP23);
	m_dest[24] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP24);
	m_dest[25] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP25);
	m_dest[26] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP26);
	m_dest[27] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP27);
	m_dest[28] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP28);
	m_dest[29] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP29);
	m_dest[30] = sf::IpAddress(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusRemoteIP30);

	// ToDo: Maybe more than 32 players?
	m_dest_port[0] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort0;
	m_dest_port[1] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort1;
	m_dest_port[2] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort2;
	m_dest_port[3] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort3;
	m_dest_port[4] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort4;
	m_dest_port[5] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort5;
	m_dest_port[6] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort6;
	m_dest_port[7] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort7;
	m_dest_port[8] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort8;
	m_dest_port[9] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort9;
	m_dest_port[10] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort10;
	m_dest_port[11] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort11;
	m_dest_port[12] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort12;
	m_dest_port[13] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort13;
	m_dest_port[14] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort14;
	m_dest_port[15] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort15;
	m_dest_port[16] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort16;
	m_dest_port[17] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort17;
	m_dest_port[18] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort18;
	m_dest_port[19] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort19;
	m_dest_port[20] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort20;
	m_dest_port[21] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort21;
	m_dest_port[22] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort22;
	m_dest_port[23] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort23;
	m_dest_port[24] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort24;
	m_dest_port[25] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort25;
	m_dest_port[26] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort26;
	m_dest_port[27] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort27;
	m_dest_port[28] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort28;
	m_dest_port[29] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort29;
	m_dest_port[30] = SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusRemotePort30;
	
	// Setup list of netplay teammates
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate0);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate1);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate2);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate3);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate4);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate5);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate6);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate7);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate8);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate9);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate10);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate11);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate12);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate13);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate14);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate15);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate16);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate17);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate18);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate19);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate20);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate21);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate22);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate23);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate24);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate25);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate26);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate27);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate28);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate29);
	teammates.insert(SConfig::GetInstance().m_LocalCoreStartupParameter.strInferiusTeammate30);
	
	// Use Game ID to figure out max number of players, and initialize the data storage and function pointers accordingly
	
	std::string SA2B_USA_ID = "GSNE8P";
	std::string SFA_USA_ID = "GF7E01";
	
        if(GetCurrentGameID() == SA2B_USA_ID)
        {
            current_inferius_game = new InferiusGameSonicAdventure2Battle();
        }
        else if(GetCurrentGameID() == SFA_USA_ID)
        {
            current_inferius_game = new InferiusGameStarFoxAssault();
        }
	
	int max_players = current_inferius_game->MaxPlayerLimit();
	
	current_player_data.resize(max_players);
	player_local.resize(max_players);
	player_local_override.resize(max_players);
	player_remote.resize(max_players);
	
	for(unsigned int slot = 0; slot < current_player_data.size(); slot++)
	{
		current_player_data[slot] = new InferiusPlayerData;
		
		// ToDo: make this load from config file
		if(slot == 0)
		{
			player_local[slot] = true;
			player_remote[slot] = false;
		}
		else
		{
			player_local[slot] = false;
			player_remote[slot] = true;
		}
		
		player_local_override[slot] = false;
	}
	
	temp_player_data = new InferiusPlayerData;
	
	// Setup the netplay socket
	m_socket.setBlocking(false);
	m_socket.bind(SConfig::GetInstance().m_LocalCoreStartupParameter.iInferiusLocalPort);
	
	// We've initialized, no need to do it again
	m_initialized = true;
}

bool InferiusEnabled()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bInferiusEnabled;
}

bool InferiusHeadTrackingEnabled()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bInferiusHeadTrackingEnabled;
}

bool InferiusFirstPersonEnabled()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bInferiusFirstPersonEnabled;
}

bool InferiusNetplayEnabled()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bInferiusNetplayEnabled;
}

void FreeLookRotate(float &yaw, float &pitch, float &roll)
{
// TODO: This will be refactored later.
#if 0

	if(!InferiusEnabled())
	{
		return;
	}
	
	u8 *gameId = Memory::GetPointer(0x80000000);

	char SA2B_USA_ID[] = "GSNE8P";
	char SFA_USA_ID[] = "GF7E01";		

	if(! memcmp(gameId, SA2B_USA_ID, 6) || ! memcmp(gameId, SFA_USA_ID, 6) )
	{
		// We're in SA2B USA
		
		//printf("Yaw=%f, Pitch=%f\n", yaw, pitch);
		
		head_yaw = yaw;
		head_pitch = pitch;
		//head_roll = 0.0;
		head_roll = roll;
		
		ApplySync();
		
		yaw = 0.0;
		pitch = 0.0;
		roll = head_roll;
	}
#endif
}

// IN PROGRESS: make this function independent of the game being run.
void ProcessPad(u8 _numPAD, GCPadStatus* _pPADStatus)
{
	if(!InferiusEnabled())
	{
		return;
	}
	
	if(!InferiusNetplayEnabled() && !InferiusHeadTrackingEnabled())
	{
		return;
	}
	
	if(!m_initialized)
	{
		Init();
	}
	
	//u8 *gameId = Memory::GetPointer(0x80000000);

	//char SA2B_USA_ID[] = "GSNE8P";
	//char SFA_USA_ID[] = "GF7E01";
	
	//std::string current_game_id = GetCurrentGameID();
	
	
	
	
	// TODO: Removed this until I can reintegrate the netplay properly in the new framework.
#if 0
	
	//if(! memcmp(gameId, SA2B_USA_ID, 6) && InferiusNetplayEnabled())
	if(InferiusNetplayEnabled())
	{
		// We're in SA2B USA

		// Test code
		//_pPADStatus->stickY = 255;
		
		while(1)
		{	
			m_status = m_socket.receive( (char*) &(current_inferius_game->GetSyncStatePointer(temp_player_data)),  current_inferius_game->GetSyncStateSize(temp_player_data) ), m_size_received, m_ip_received, m_port_received);
			//m_status = m_socket.receive( (char*) &(temp_player_data->sync.SonicAdventure2Battle), sizeof(temp_player_data->sync.SonicAdventure2Battle), m_size_received, m_ip_received, m_port_received);
			
			if(m_status == sf::Socket::Done)
			{
				// Calculate username: IP:Port:PlayerNum
				//std::string user = m_ip_received.toString() + std::string(":") + std::to_string( (long long) m_port_received) + std::string(":") + std::to_string( (long long) ( (temp_player_data)->sync.SonicAdventure2Battle.player_num ) );
				//std::string user = m_ip_received.toString() + std::string(":") + std::to_string( (long long) m_port_received) + std::string(":") + std::to_string( (long long) ( GetPlayerNum_Game[current_game_id](*temp_player_data) ) );
				std::string user = m_ip_received.toString() + std::string(":") + std::to_string( (long long) m_port_received) + std::string(":") + std::to_string( (long long) ( current_inferius_game->GetPlayerIndex(temp_player_data) ) );
			
				printf("Received data from player %s\n", user.c_str());	
				
				// Create user if not already present.
				if(! player_mux_data.count(user))
				{
					player_mux_data[user] = new InferiusPlayerData;
				}
				
				// Store data
				* (player_mux_data[user]) = * (temp_player_data);
				
				// Multiplex
				std::map< std::string,InferiusPlayerData* >::iterator it;
				
				u8 player_slot = 0;
				float min_priority = 9999999999999999.0;
				std::unordered_set<std::string> placed_players;
				
				player_local_override[0] = false;
				player_local_override[1] = false;

				// ToDo: try to keep slots persistent within a priority (e.g. human player, CPU player)
				while(player_slot < current_player_data.size())
				{
					if(! player_local[player_slot] && ! player_remote[player_slot])
					{
						player_slot++;
						continue;
					}

					if(player_local[player_slot] && ! player_local_override[player_slot]) 
					{
						std::string best_user = "";
						
						for ( it=player_mux_data.begin() ; it != player_mux_data.end(); it++ )
						{
							
							// If we already placed this player in a slot, disqualify them from subsequent slots
							if(placed_players.count(it->first))
							{
								continue;
							}
							
							unsigned short buttons = (it->second) -> sync.SonicAdventure2Battle.pad.button & (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_BUTTON_UP | PAD_BUTTON_DOWN | PAD_BUTTON_START);

							// Only override local player for a teammate
							//if(teammates.count(it->first) || buttons)
							// removed above line for HGE 2014... will re-enable teams later
							if(0)
							{
								
								// ToDo: check all local users, not just player 0
								
								float progress = (it->second) -> sync.SonicAdventure2Battle.progress;
								
								if( progress > 0.985 || buttons)
								{
									printf("Teammate replaced slot %d\n", player_slot);
									player_local_override[player_slot] = true;
									* (current_player_data[player_slot]) = * (it->second);
									placed_players.insert(it->first);
									// WINNING
								}
							
							}
						}
	
						player_slot++;
						
						continue;
					}
					
					std::string best_user = "";
					
					for ( it=player_mux_data.begin() ; it != player_mux_data.end(); it++ )
					{
						// If we already placed this player in a slot, disqualify them from subsequent slots
						if(placed_players.count(it->first))
						{
							continue;
						}
						
						// ToDo: heuristics other than XYZ proximity
						
						// Teammates won't be shown just because of XYZ proximity
						// ToDo: override local player if teammate is about to win
						if(! teammates.count(it->first))
						{
							float remote_player_priority = 9999999999999999.0;
							
							for (local_player_compare = 0; local_player_compare < current_player_data.size(); local_player_compare++)
							{
								if(player_local[local_player_compare])
								{
									float sub_priority = current_inferius_game->MultiplexRemotePriority(current_player_data[local_player_compare], it->second);
									if(sub_priority < remote_player_priority)
									{
										remote_player_priority = sub_priority;
									}
								}
							}
							
							if(remote_player_priority < min_priority)
							{
								min_priority = remote_player_priority;
								best_user = it->first;
							}
						
						}
					}
					
					if(best_user == "")
					{
						break;
					}
					
					printf("Slot %d is %s\n", player_slot, best_user.c_str());
					* (current_player_data[player_slot]) = * (player_mux_data[best_user]);
					placed_players.insert(best_user);
					
					player_slot++;
				}
				
				// This is for without multiplexing
				#if 0
				if(( (InferiusPlayerDataSonicAdventure2Battle*) (temp_player_data) ) -> player_num == 0) // Temporary compare; in reality we want all remote players to be used
				{
					// Store data for player 1 (remote player)
					* (InferiusPlayerDataSonicAdventure2Battle*) (current_player_data[1]) = * (InferiusPlayerDataSonicAdventure2Battle*) (temp_player_data);
				}
				#endif
			}
			else
			{
				break;
			}
		
		}
		
		if(_numPAD < player_local.size() && player_local[_numPAD] && ! player_local_override[_numPAD])
		{
			// Local player, peek data
		
			(current_player_data[_numPAD])->sync.SonicAdventure2Battle.player_num = _numPAD;
			( (current_player_data[_numPAD])->sync.SonicAdventure2Battle.frame_num) ++;
			(current_player_data[_numPAD])->sync.SonicAdventure2Battle.pad = *_pPADStatus;

			// Send UDP packet
			// ToDo: send to arbitrary number of destinations, check for null IP in case destination isn't specified
			for(u8 dest = 0; dest < 31; dest++)
			{
				if(m_dest[dest] == m_nullIP)
				{
					printf("Null IP for remote player %d\n", dest);
					break;
				}
				m_status = m_socket.send((const char *) &((current_player_data[_numPAD])->sync.SonicAdventure2Battle), sizeof((current_player_data[_numPAD])->sync.SonicAdventure2Battle), m_dest[dest], m_dest_port[dest]);
			}
			
			if(m_status != sf::Socket::Done)
			{
				//printf("Pad Data Send failed: %d\n", m_status);
			}
			else
			{
				//printf("Sent pad data.\n");
			}
		}
		else if(_numPAD < player_remote.size() && (player_remote[_numPAD] || player_local_override[_numPAD]))
		{
			// Remote player, poke data
			
			(current_player_data[_numPAD])->sync.SonicAdventure2Battle.player_num = _numPAD;
			*_pPADStatus = (current_player_data[_numPAD])->sync.SonicAdventure2Battle.pad;
		}
		
		//printf("Pad %d Y = %d\n", (int) _numPAD, (int) _pPADStatus->stickY);
	}
#endif
	
	
	if(_numPAD < current_player_data.size())
	{
		current_inferius_game->PadPostHook(current_player_data[_numPAD], _numPAD, _pPADStatus);
	}
}

// TODO: refactor this.  For HGE 2015, maybe just leave it out completely.
void ApplySync()
{
	if(!InferiusEnabled())
	{
		return;
	}
	
	if(!m_initialized)
	{
		Init();
	}
	
	u8 *gameId = Memory::GetPointer(0x80000000);

	char SA2B_USA_ID[] = "GSNE8P";	
	char SFA_USA_ID[] = "GF7E01";		

	// TODO: Refactor the below SA2B code.
#if 0
	if(! memcmp(gameId, SA2B_USA_ID, 6))
	{
		// We're in SA2B USA
		
		InferiusPlayerData* Players[] = { current_player_data[0], current_player_data[1] };
		
		unsigned int P1_X_addr = 0x80C5D5B4;
		unsigned int P1_Y_addr = 0x80C5D5B8;
		unsigned int P1_Z_addr = 0x80C5D5BC;
		
		unsigned int P1_Xr_addr = 0x80C5D5AA;
		unsigned int P1_Yr_addr = 0x80C5D5AE;
		unsigned int P1_Zr_addr = 0x80C5D5B2;
		
		unsigned int P1_size_X_addr = 0x80C5D5C0;
		unsigned int P1_size_Y_addr = 0x80C5D5C4;
		unsigned int P1_size_Z_addr = 0x80C5D5C8;		
		
		unsigned int P1_cam_X_addr = 0x801ff5b8;
		unsigned int P1_cam_Y_addr = 0x801ff5bc;
		unsigned int P1_cam_Z_addr = 0x801ff5c0;
		
		unsigned int P1_cam_look_at_X_addr = 0x801ff5dc;
		unsigned int P1_cam_look_at_Y_addr = 0x801ff5e0;
		unsigned int P1_cam_look_at_Z_addr = 0x801ff5e4;
		
		unsigned int P1_cam_matrix_addr = 0x801e5bd8;
		
		unsigned int P1_progress_addr = 0x80CD3540;
		
		unsigned int P2_X_addr = 0x80CD3614;
		unsigned int P2_Y_addr = 0x80CD3618;
		unsigned int P2_Z_addr = 0x80CD361C;
		
		unsigned int P2_progress_addr = 0x80CD3544;

		if(player_local[0] && ! player_local_override[0])
		{
	
			Players[0]->sync.SonicAdventure2Battle.X = Memory::Read_F32(P1_X_addr);
			Players[0]->sync.SonicAdventure2Battle.Y = Memory::Read_F32(P1_Y_addr);
			Players[0]->sync.SonicAdventure2Battle.Z = Memory::Read_F32(P1_Z_addr);
	
			// scale period from 0x10000 to 2pi
			Players[0]->sync.SonicAdventure2Battle.Xr = ( (float) Memory::Read_U16(P1_Xr_addr) ) / 0x10000 * (2 * M_PI);
			Players[0]->sync.SonicAdventure2Battle.Yr = ( (float) Memory::Read_U16(P1_Yr_addr) ) / 0x10000 * (2 * M_PI);
			Players[0]->sync.SonicAdventure2Battle.Zr = ( (float) Memory::Read_U16(P1_Zr_addr) ) / 0x10000 * (2 * M_PI);
			
			Players[0]->internal.SonicAdventure2Battle.cam_X = Memory::Read_F32(P1_cam_X_addr);
			Players[0]->internal.SonicAdventure2Battle.cam_Y = Memory::Read_F32(P1_cam_Y_addr);
			Players[0]->internal.SonicAdventure2Battle.cam_Z = Memory::Read_F32(P1_cam_Z_addr);
			
			Players[0]->internal.SonicAdventure2Battle.cam_look_at_X = Memory::Read_F32(P1_cam_look_at_X_addr);
			Players[0]->internal.SonicAdventure2Battle.cam_look_at_Y = Memory::Read_F32(P1_cam_look_at_Y_addr);
			Players[0]->internal.SonicAdventure2Battle.cam_look_at_Z = Memory::Read_F32(P1_cam_look_at_Z_addr);
			
			Players[0]->sync.SonicAdventure2Battle.progress = Memory::Read_F32(P1_progress_addr);
			
			if(InferiusFirstPersonEnabled())
			{
				
				
				// Smooth the rotation since the game engine allows very rapid turning of Sonic
				P1_Xr_smoothed_float = smooth(P1_Xr_smoothed_float, Players[0]->sync.SonicAdventure2Battle.Xr, 2.0);
				P1_Yr_smoothed_float = smooth(P1_Yr_smoothed_float, Players[0]->sync.SonicAdventure2Battle.Yr, 0.5);
				P1_Zr_smoothed_float = smooth(P1_Zr_smoothed_float, Players[0]->sync.SonicAdventure2Battle.Zr, 2.0);
				
				// Write new size data (0.25)
				//Write_F32(0.25, P1_size_X_addr);
				//Write_F32(0.25, P1_size_Y_addr);
				//Write_F32(0.25, P1_size_Z_addr);
				Write_F32(0.1, P1_size_X_addr);
				Write_F32(0.1, P1_size_Y_addr);
				Write_F32(0.1, P1_size_Z_addr);
				
				
				
				// 1 to use only yaw; 0 to use all euler angles
				#if 0
				
				// New Cam XYZ is 10 above Player XYZ
				Players[0]->cam_X = Players[0]->X; 
				Players[0]->cam_Y = Players[0]->Y + 10.0; 
				Players[0]->cam_Z = Players[0]->Z; 
				// 10.0 was inside Sonic at times, fixed with a size modifier

				// Write new camera position data
				Write_F32(Players[0]->cam_X, P1_cam_X_addr);
				Write_F32(Players[0]->cam_Y, P1_cam_Y_addr);
				Write_F32(Players[0]->cam_Z, P1_cam_Z_addr);

				// Calculate yaw/pitch
				float look_at_yaw = P1_Yr_smoothed_float;
				float look_at_pitch = 0.0;
				
				float new_yaw, new_pitch;
				
				if(InferiusHeadTrackingEnabled())
				{
					new_yaw = look_at_yaw - head_yaw;
					new_pitch = look_at_pitch + head_pitch;
				}
				else
				{
					new_yaw = look_at_yaw;
					new_pitch = look_at_pitch;
				}
				
				float head_look_at_vector[] = {(float) ( cos(new_yaw)*cos(new_pitch) ), (float) ( sin(new_pitch) ), (float) ( sin(new_yaw)*cos(new_pitch) )};
				
				#else
				
				//float look_at_yaw = -1.0*P1_Yr_smoothed_float - M_PI_2;
				float look_at_yaw = -1.0*P1_Yr_smoothed_float + M_PI_2;
				// above works
				//float look_at_pitch = Players[0]->Xr;
				//float look_at_pitch = -1.0 * Players[0]->Xr; // at this stage the GHZ loop worked fine
				float look_at_pitch = P1_Xr_smoothed_float; // I think we need to swap yaw/pitch order
				// above works
				//float look_at_roll = Players[0]->Zr; // this had Sonic roll when going through loop in GHZ
				//float look_at_roll = -1.0 * Players[0]->Zr; // but so did this
				//float look_at_roll = P1_Zr_smoothed_float; // this had camera go through wall in Sonic Forest
				//float look_at_roll = -1.0 * P1_Zr_smoothed_float; // this had camera go through wall way more
				float look_at_roll = P1_Zr_smoothed_float; // deferred
				
				Matrix33 mYaw, mPitch, mRoll, mYawPitch, mYawPitchRoll, mHeadYaw, mHeadPitch, mHeadRoll, mHeadYawPitch, mHeadYawPitchRoll, mResult, mFinalRoll, mAdjustedResult;
				
				Matrix33::RotateY(mYaw, look_at_yaw);
				Matrix33::RotateX(mPitch, look_at_pitch);
				Matrix33::RotateZ(mRoll, look_at_roll);
				
				Matrix33::RotateY(mHeadYaw, head_yaw);
				//Matrix33::RotateX(mHeadPitch, head_pitch); // head pitch looked inverted in City Escape
				Matrix33::RotateX(mHeadPitch, -head_pitch);
				//Matrix33::RotateZ(mHeadRoll, head_roll); // This has head roll inverted, but main roll might have been too... need to check later
				Matrix33::RotateZ(mHeadRoll, -head_roll);
				
				Matrix33::Multiply(mPitch, mYaw, mYawPitch);
				Matrix33::Multiply(mYawPitch, mRoll, mYawPitchRoll);
				
				Matrix33::Multiply(mHeadYaw, mHeadPitch, mHeadYawPitch);
				Matrix33::Multiply(mHeadYawPitch, mHeadRoll, mHeadYawPitchRoll);
				
				Matrix33::Multiply(mYawPitchRoll, mHeadYawPitchRoll, mResult);
				
				float up[] = {0.0, 1.0, 0.0};
				float right[] = {1.0, 0.0, 0.0};
				float forward[] = {0.0, 0.0, 1.0};
				
				float up_result[3];
				float right_result[3];
				float forward_result[3];
				
				float player_up[3];

				Matrix33::Multiply(mResult, up, up_result);
				Matrix33::Multiply(mResult, right, right_result);
				Matrix33::Multiply(mResult, forward, forward_result);

				Matrix33::Multiply(mYawPitchRoll, up, player_up);
				
				// Possibilities: 2nd arg could be right[1], -right[1], forward[1], -forward[1]
				// Or, 1st arg could be -up[1] and the last part would be + M_PI_2
				// Not figured out yet
				head_roll = atan2(up_result[1], right_result[1]) - M_PI_2;
				
				Matrix33::RotateZ(mFinalRoll, -head_roll);
				
				Matrix33::Multiply(mResult, mFinalRoll, mAdjustedResult);
				
				
				float head_look_at_vector[3];
				
				Matrix33::Multiply(mAdjustedResult, forward, head_look_at_vector);
				
				// New Cam XYZ is 10 above Player XYZ
				Players[0]->internal.SonicAdventure2Battle.cam_X = Players[0]->sync.SonicAdventure2Battle.X + 10.0 * player_up[0]; 
				Players[0]->internal.SonicAdventure2Battle.cam_Y = Players[0]->sync.SonicAdventure2Battle.Y + 10.0 * player_up[1]; 
				Players[0]->internal.SonicAdventure2Battle.cam_Z = Players[0]->sync.SonicAdventure2Battle.Z + 10.0 * player_up[2]; 
				// 10.0 was inside Sonic at times, fixed with a size modifier

				// Write new camera position data
				Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_X, P1_cam_X_addr);
				Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_Y, P1_cam_Y_addr);
				Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_Z, P1_cam_Z_addr);

				#endif
				
				// Scale to magnitude 100.0
				head_look_at_vector[0] *= 100.0;
				head_look_at_vector[1] *= 100.0;
				head_look_at_vector[2] *= 100.0;
				
				Players[0]->internal.SonicAdventure2Battle.cam_look_at_X = Players[0]->internal.SonicAdventure2Battle.cam_X + head_look_at_vector[0];
				Players[0]->internal.SonicAdventure2Battle.cam_look_at_Y = Players[0]->internal.SonicAdventure2Battle.cam_Y + head_look_at_vector[1];
				Players[0]->internal.SonicAdventure2Battle.cam_look_at_Z = Players[0]->internal.SonicAdventure2Battle.cam_Z + head_look_at_vector[2];
				
				Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_look_at_X, P1_cam_look_at_X_addr);
				Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_look_at_Y, P1_cam_look_at_Y_addr);
				Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_look_at_Z, P1_cam_look_at_Z_addr);
				
				
				
				// Knockout the code that writes the LookAt values (should hook these in future)
				Memory::Write_U32(0x60000000, 0x800c7564); // X
				Memory::Write_U32(0x60000000, 0x800c7568); // Y
				Memory::Write_U32(0x60000000, 0x800c7570); // Z
				
				// Knockout the code that writes the Cam XYZ values (should hook these in future)
				Memory::Write_U32(0x60000000, 0x800C751C); // X
				Memory::Write_U32(0x60000000, 0x800C7520); // Y
				Memory::Write_U32(0x60000000, 0x800C7528); // Z
				
			}	
			else if(InferiusHeadTrackingEnabled())
			{
				float look_at_vector[] = { Players[0]->sync.SonicAdventure2Battle.X - Players[0]->internal.SonicAdventure2Battle.cam_X,
				                           0.0,
							   Players[0]->sync.SonicAdventure2Battle.Z - Players[0]->internal.SonicAdventure2Battle.cam_Z } ;
				
				float magnitude = sqrt(look_at_vector[0] * look_at_vector[0] + look_at_vector[1] * look_at_vector[1] + look_at_vector[2] * look_at_vector[2]);
				
				look_at_vector[0] /= magnitude;
				look_at_vector[1] /= magnitude;
				look_at_vector[2] /= magnitude;
				
				#if 0
				
				Matrix33 mYaw;
				Matrix33 mPitch;
		
				Matrix33 mYawPitch;
		
				Matrix33::RotateY(mYaw, head_yaw);
				Matrix33::RotateX(mPitch, head_pitch);
		
				Matrix33::Multiply(mYaw, mPitch, mYawPitch);
				//Matrix33::Multiply(mPitch, mYaw, mYawPitch);
				
				Matrix33 mHead = mYawPitch;
				
					/*
					printf("YawPitch Matrix:\n");
					printf("%.8f, %.8f, %.8f\n", mHead.data[0], mHead.data[1], mHead.data[2]);
					printf("%.8f, %.8f, %.8f\n", mHead.data[3], mHead.data[4], mHead.data[5]);
					printf("%.8f, %.8f, %.8f\n", mHead.data[6], mHead.data[7], mHead.data[8]);
					printf("\n\n\n");
					*/
				
				/*
				//printf("Writing matrix...\n");
				
				Write_F32(mHead.data[0], P1_cam_matrix_addr+0x0);
				Write_F32(mHead.data[1], P1_cam_matrix_addr+0x4);
				Write_F32(mHead.data[2], P1_cam_matrix_addr+0x8);
				Write_F32(mHead.data[3], P1_cam_matrix_addr+0x10);
				Write_F32(mHead.data[4], P1_cam_matrix_addr+0x14);
				Write_F32(mHead.data[5], P1_cam_matrix_addr+0x18);
				Write_F32(mHead.data[6], P1_cam_matrix_addr+0x20);
				Write_F32(mHead.data[7], P1_cam_matrix_addr+0x24);
				Write_F32(mHead.data[8], P1_cam_matrix_addr+0x28);
				*/
				
				float head_look_at_0[3];
				float head_look_at_1[3];
				float head_look_at_vector[3];
				
				Matrix33 mXtoZ;
				mXtoZ.data[0] = 0.0;
				mXtoZ.data[1] = 0.0;
				mXtoZ.data[2] = 1.0;
				mXtoZ.data[3] = 0.0;
				mXtoZ.data[4] = 1.0;
				mXtoZ.data[5] = 0.0;
				mXtoZ.data[6] = 1.0;
				mXtoZ.data[7] = 0.0;
				mXtoZ.data[8] = 0.0;
				
				Matrix33 mZtoX = mXtoZ;
				
				Matrix33::Multiply(mXtoZ, look_at_vector, head_look_at_0);
				Matrix33::Multiply(mHead, head_look_at_0, head_look_at_1);
				Matrix33::Multiply(mZtoX, head_look_at_1, head_look_at_vector);
				
				#else
				
				float look_at_yaw = atan2(look_at_vector[2], look_at_vector[0]);
				float look_at_pitch = 0.0;
				
				float new_yaw = look_at_yaw - head_yaw;
				float new_pitch = look_at_pitch + head_pitch;
				
				float head_look_at_vector[] = { (float) ( cos(new_yaw)*cos(new_pitch) ), (float) ( sin(new_pitch) ), (float) ( sin(new_yaw)*cos(new_pitch) )}; 
				
				#endif
				
				//printf("NewYaw: %.8f, NewPitch: %.8f\n", new_yaw, new_pitch);
				//printf("V0: %.8f, %.8f, %.8f\n", look_at_vector[0], look_at_vector[1], look_at_vector[2]);
				//printf("V1: %.8f, %.8f, %.8f\n\n\n\n", head_look_at_vector[0], head_look_at_vector[1], head_look_at_vector[2]);
				
				// Scale to magnitude 100.0
				head_look_at_vector[0] *= 100.0;
				head_look_at_vector[1] *= 100.0;
				head_look_at_vector[2] *= 100.0;
				
				Players[0]->internal.SonicAdventure2Battle.cam_look_at_X = Players[0]->internal.SonicAdventure2Battle.cam_X + head_look_at_vector[0];
				Players[0]->internal.SonicAdventure2Battle.cam_look_at_Y = Players[0]->internal.SonicAdventure2Battle.cam_Y + head_look_at_vector[1];
				Players[0]->internal.SonicAdventure2Battle.cam_look_at_Z = Players[0]->internal.SonicAdventure2Battle.cam_Z + head_look_at_vector[2];
				
				Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_look_at_X, P1_cam_look_at_X_addr);
				Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_look_at_Y, P1_cam_look_at_Y_addr);
				Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_look_at_Z, P1_cam_look_at_Z_addr);
				
				
				
				// Knockout the code that writes the LookAt values (should hook these in future)
				Memory::Write_U32(0x60000000, 0x800c7564); // X
				Memory::Write_U32(0x60000000, 0x800c7568); // Y
				Memory::Write_U32(0x60000000, 0x800c7570); // Z
				
				
			}
		
		}
		if(player_local[1] && ! player_local_override[1])
		{
			Players[1]->sync.SonicAdventure2Battle.X = Memory::Read_F32(P2_X_addr);
			Players[1]->sync.SonicAdventure2Battle.Y = Memory::Read_F32(P2_Y_addr);
			Players[1]->sync.SonicAdventure2Battle.Z = Memory::Read_F32(P2_Z_addr);
			
			Players[1]->sync.SonicAdventure2Battle.progress = Memory::Read_F32(P2_progress_addr);
			
			// not implemented yet
			#if 0
			
			// scale period from 0x10000 to 2pi
			Players[1]->Xr = ( (float) Memory::Read_U16(P1_Xr_addr) ) / 0x10000 * (2 * M_PI);
			Players[1]->Yr = ( (float) Memory::Read_U16(P1_Yr_addr) ) / 0x10000 * (2 * M_PI);
			Players[1]->Zr = ( (float) Memory::Read_U16(P1_Zr_addr) ) / 0x10000 * (2 * M_PI);
			
			Players[1]->cam_X = Memory::Read_F32(P1_cam_X_addr);
			Players[1]->cam_Y = Memory::Read_F32(P1_cam_Y_addr);
			Players[1]->cam_Z = Memory::Read_F32(P1_cam_Z_addr);
			
			Players[1]->cam_look_at_X = Memory::Read_F32(P1_cam_look_at_X_addr);
			Players[1]->cam_look_at_Y = Memory::Read_F32(P1_cam_look_at_Y_addr);
			Players[1]->cam_look_at_Z = Memory::Read_F32(P1_cam_look_at_Z_addr);
			#endif
		}
		
		if(InferiusNetplayEnabled())
		{
		
		if(player_remote[0] || player_local_override[0])
		{
			Write_F32(Players[0]->sync.SonicAdventure2Battle.X, P1_X_addr);
			Write_F32(Players[0]->sync.SonicAdventure2Battle.Y, P1_Y_addr);
			Write_F32(Players[0]->sync.SonicAdventure2Battle.Z, P1_Z_addr);
			
			// scale period from 0x10000 to 2pi
			Memory::Write_U16((u16) ( Players[0]->sync.SonicAdventure2Battle.Xr * (float)0x10000 / (2*M_PI) ), P1_Xr_addr);
			Memory::Write_U16((u16) ( Players[0]->sync.SonicAdventure2Battle.Yr * (float)0x10000 / (2*M_PI) ), P1_Yr_addr);
			Memory::Write_U16((u16) ( Players[0]->sync.SonicAdventure2Battle.Zr * (float)0x10000 / (2*M_PI) ), P1_Zr_addr);
			
			// Don't sync the internal variables anymore
			/*
			Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_X, P1_cam_X_addr);
			Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_Y, P1_cam_Y_addr);
			Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_Z, P1_cam_Z_addr);
			
			Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_look_at_X, P1_cam_look_at_X_addr);
			Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_look_at_Y, P1_cam_look_at_Y_addr);
			Write_F32(Players[0]->internal.SonicAdventure2Battle.cam_look_at_Z, P1_cam_look_at_Z_addr);
			*/
		}
		if(player_remote[1] || player_local_override[1])
		{
			Write_F32(Players[1]->sync.SonicAdventure2Battle.X, P2_X_addr);
			Write_F32(Players[1]->sync.SonicAdventure2Battle.Y, P2_Y_addr);
			Write_F32(Players[1]->sync.SonicAdventure2Battle.Z, P2_Z_addr);
			
			// not implemented yet
			#if 0
			// scale period from 0x10000 to 2pi
			Memory::Write_U16((u16) ( Players[0]->Xr * (float)0x10000 / (2*M_PI) ), P1_Xr_addr)
			Memory::Write_U16((u16) ( Players[0]->Yr * (float)0x10000 / (2*M_PI) ), P1_Yr_addr)
			Memory::Write_U16((u16) ( Players[0]->Zr * (float)0x10000 / (2*M_PI) ), P1_Zr_addr)
			
			Write_F32(Players[0]->cam_X, P1_cam_X_addr);
			Write_F32(Players[0]->cam_Y, P1_cam_Y_addr);
			Write_F32(Players[0]->cam_Z, P1_cam_Z_addr);
			
			Write_F32(Players[0]->cam_look_at_X, P1_cam_look_at_X_addr);
			Write_F32(Players[0]->cam_look_at_Y, P1_cam_look_at_Y_addr);
			Write_F32(Players[0]->cam_look_at_Z, P1_cam_look_at_Z_addr);
			#endif
		}
		
		}
		
		//printf("Player 1 XYZ: %f %f %f\n", Players[0]->X, Players[0]->Y, Players[0]->Z);
		
		// Zero P2 XYZ
		//Memory::Write_U32(0, 0x80CD3614);
		//Memory::Write_U32(0, 0x80CD3618);
		//Memory::Write_U32(0, 0x80CD361C);
		
		prev_ticks = CoreTiming::GetTicks();
	}
#endif

	if(! memcmp(gameId, SFA_USA_ID, 6))
	{
		if(InferiusHeadTrackingEnabled())
		{
			
			Memory::Write_U32(0x4E800020, 0x8007cf88);
			
			/*
			short short_yaw = (short) ( (head_yaw+yaw_offset+M_PI) * (float)0x10000 / (2*M_PI) );
			short short_pitch = (short) ( -(head_pitch) * (float)0x10000 / (2*M_PI) ) ;
			
			Memory::Write_U16(short_yaw, 0x80d7e4f6);
			Memory::Write_U16(short_pitch, 0x80d7e586);
			*/
			
		}
	}
	else
	{
		//printf("Unsupported Inferius Game.\n");
	}
	
}

void HLEXYZPositionRead0()
{
	if(!InferiusEnabled())
	{
		return;
	}
	
	if(!m_initialized)
	{
		Init();
	}
	
	u8 *gameId = Memory::GetPointer(0x80000000);

	char SA2B_USA_ID[] = "GSNE8P";	
	char SA2B_PAL_ID[] = "GSNP8P";	

	if(! memcmp(gameId, SA2B_USA_ID, 6) || ! memcmp(gameId, SA2B_PAL_ID, 6))
	{
		// We're in SA2B USA
		
		if(GPR(0) == 4)
		{
			printf("Player 0 XYZ Address = %X, Y value = %f\n", (int) GPR(3), Memory::Read_F32(GPR(3)+0x4));
		}
		else if(GPR(0) == 0)
		{
			printf("Player 1 XYZ Address = %X, Y value = %f\n", (int) GPR(3), Memory::Read_F32(GPR(3)+0x4));
		}
	}
	else
	{
		printf("Unrecognized Inferius game.\n");
	}
}

u32 SFA_P0_Aim_Address = 0x0; // ToDo: Improve this hack

void HLEPlayerAimWrite0()
{
	//printf("               Hooked PlayerAimWrite0\n");

	//printf("                  Player start %ld\n", CoreTiming::GetTicks());

	static float old_yaw, old_pitch;

	if(!InferiusEnabled())
	{
		return;
	}
	
	if(!InferiusHeadTrackingEnabled())
	{
		return;
	}
	
	if(!m_initialized)
	{
		Init();
	}
	
	u8 *gameId = Memory::GetPointer(0x80000000);

	char SA2B_USA_ID[] = "GSNE8P";	
	char SA2B_PAL_ID[] = "GSNP8P";	
	char SFA_USA_ID[] = "GF7E01";	

	if(! memcmp(gameId, SFA_USA_ID, 6) )
	{
		// We're in SFA USA
		
		if(GPR(31) >= 0 && GPR(31) <= 3)
		{
			
			
			u32 AimAddress = (u32) GPR(3);
			SFA_P0_Aim_Address = AimAddress; // ToDo: improve this hack
			
			if(GPR(31) == 0) 
			{
				printf("Before:\n%.4f %.4f %.4f \n%.4f %.4f %.4f \n%.4f %.4f %.4f \n\n\n\n", Memory::Read_F32(AimAddress+0*4), Memory::Read_F32(AimAddress+1*4), Memory::Read_F32(AimAddress+2*4), Memory::Read_F32(AimAddress+3*4), Memory::Read_F32(AimAddress+4*4), Memory::Read_F32(AimAddress+5*4), Memory::Read_F32(AimAddress+6*4), Memory::Read_F32(AimAddress+7*4), Memory::Read_F32(AimAddress+8*4) );
			}
			
			//printf("                        Player %d Aim Address = %X\n", (int) GPR(31), AimAddress );
			
			// Construct a YawPitch rotation matrix...
			
			//printf("           Player yaw: %.4f\n", head_yaw+yaw_offset);
			
			Matrix33 mYaw;
			Matrix33 mPitch;
			
			Matrix33 mYawPitch;
			
			float prev_vec_x = Memory::Read_F32(0x80d7e70c);
			float prev_vec_y = Memory::Read_F32(0x80d7e70c+4);
			float prev_vec_z = Memory::Read_F32(0x80d7e70c+8);
			
			//Matrix33::RotateY(mYaw, -(head_yaw+yaw_offset));
			//Matrix33::RotateY(mYaw, -(head_yaw+yaw_offset + 0.01)); // adding a constant offset to test some bugs
			//Matrix33::RotateY(mYaw, -(head_yaw+yaw_offset - atan2(prev_vec_z, prev_vec_x) )); // adding a constant offset to test some bugs
			
			Matrix33::RotateY(mYaw, -(head_yaw+yaw_offset + (head_yaw - old_yaw) )); // adding a constant offset to test some bugs
			
			//Matrix33::RotateX(mPitch, -head_pitch);
			//Matrix33::RotateX(mPitch, -(head_pitch + 0.01)); // adding a constant offset to test some bugs
			//Matrix33::RotateX(mPitch, -(head_pitch /*-asin(-prev_vec_y)*/ )); // adding a constant offset to test some bugs
			
			Matrix33::RotateX(mPitch, -(head_pitch + (head_pitch - old_pitch) )); // adding a constant offset to test some bugs
			
			Matrix33::Multiply(mPitch, mYaw, mYawPitch);
			
			old_yaw = head_yaw;
			old_pitch = head_pitch;
			
			Write_F32(mYawPitch.data[0], AimAddress+0*4);
			Write_F32(mYawPitch.data[1], AimAddress+1*4);
			Write_F32(mYawPitch.data[2], AimAddress+2*4);
			Write_F32(mYawPitch.data[3], AimAddress+3*4);
			Write_F32(mYawPitch.data[4], AimAddress+4*4);
			Write_F32(mYawPitch.data[5], AimAddress+5*4);
			Write_F32(mYawPitch.data[6], AimAddress+6*4);
			Write_F32(mYawPitch.data[7], AimAddress+7*4);
			Write_F32(mYawPitch.data[8], AimAddress+8*4);
			
			if(GPR(31) == 0) 
			{
				//printf("Player 0 (r4 = %x) aim 9,10,11 = %.4f %.4f %.4f \n\n\n", (int) GPR(4), Memory::Read_F32(AimAddress+9*4), Memory::Read_F32(AimAddress+10*4), Memory::Read_F32(AimAddress+11*4) );
				
				printf("After:\n                                       %.4f %.4f %.4f \n                                       %.4f %.4f %.4f \n                                       %.4f %.4f %.4f\n\n                                       Player vector? = %.4f %.4f %.4f\n\n\n\n", Memory::Read_F32(AimAddress+0*4), Memory::Read_F32(AimAddress+1*4), Memory::Read_F32(AimAddress+2*4), Memory::Read_F32(AimAddress+3*4), Memory::Read_F32(AimAddress+4*4), Memory::Read_F32(AimAddress+5*4), Memory::Read_F32(AimAddress+6*4), Memory::Read_F32(AimAddress+7*4), Memory::Read_F32(AimAddress+8*4), Memory::Read_F32(0x80d7e70c), Memory::Read_F32(0x80d7e70c+4), Memory::Read_F32(0x80d7e70c+8) );
			}
		}
		
		//printf("PlayerAim PC=%x, NPC= %x, LR= %x\n", (int) PC, (int) NPC, (int) LR);
		NPC = LR; // skip the rest of the function
	}
	else
	{
		printf("Unrecognized Inferius game.\n");
	}
	
	//printf("                  Player end %ld\n", CoreTiming::GetTicks());
}

void HLEWeaponAimVectorWrite0()
{
	if(!InferiusEnabled())
	{
		return;
	}
	
	if(!InferiusHeadTrackingEnabled())
	{
		return;
	}
	
	if(!m_initialized)
	{
		Init();
	}
	
	u8 *gameId = Memory::GetPointer(0x80000000);

	char SA2B_USA_ID[] = "GSNE8P";	
	char SA2B_PAL_ID[] = "GSNP8P";	
	char SFA_USA_ID[] = "GF7E01";	

	if(! memcmp(gameId, SFA_USA_ID, 6) )
	{
		// We're in SFA USA
		
		unsigned long ptr = GPR(4);
		
		Matrix33 mYaw;
		Matrix33 mPitch;
		
		Matrix33 mYawPitch;
		
		Matrix33::RotateY(mYaw, head_yaw+yaw_offset);
		Matrix33::RotateX(mPitch, head_pitch);
		
		Matrix33::Multiply(mYaw, mPitch, mYawPitch);
		
		float forward[] = {0.0, 0.0, -1.0};
		
		float forward_result[3];
		
		Matrix33::Multiply(mYawPitch, forward, forward_result);
		
		Write_F32(forward_result[0], ptr+0*4);
		Write_F32(forward_result[1], ptr+1*4);
		Write_F32(forward_result[2], ptr+2*4);
		
		//Write_F32(0.0, ptr+0*4);
		//Write_F32(0.0, ptr+1*4);
		//Write_F32(1.0, ptr+2*4);
		
		//printf("Writing weapon aim (%.2f, %.2f, %.2f) to %x\n", forward_result[0], forward_result[1], forward_result[2], ptr);
		
		NPC = LR;
	}
	else
	{
		printf("Unrecognized Inferius game.\n");
	}
}

void HLEPlayerAimRead0()
{
	static unsigned long prev_ptr;
	
	if(!InferiusEnabled())
	{
		return;
	}
	
	if(!InferiusHeadTrackingEnabled())
	{
		return;
	}
	
	if(!m_initialized)
	{
		Init();
	}
	
	u8 *gameId = Memory::GetPointer(0x80000000);

	char SA2B_USA_ID[] = "GSNE8P";	
	char SA2B_PAL_ID[] = "GSNP8P";	
	char SFA_USA_ID[] = "GF7E01";	

	if(! memcmp(gameId, SFA_USA_ID, 6) )
	{
		// We're in SFA USA
		
		unsigned long ptr = GPR(4);
		
		if(ptr > 0x80000000 /* && ptr != prev_ptr */ )
		{
			
			
			short short_yaw = (short) ( (head_yaw+yaw_offset+M_PI) * (float)0x10000 / (2*M_PI) );
			short short_pitch = (short) ( -(head_pitch) * (float)0x10000 / (2*M_PI) ) ;
			
			Memory::Write_U16(short_yaw, 0x80d7e4f6-0x80d7e4f4+ptr);
			Memory::Write_U16(short_pitch, 0x80d7e586-0x80d7e4f4+ptr);
			
			//printf("Wrote player aim\n");
			
			printf("Player Aim ptr = %x\n", (unsigned int)ptr);
			prev_ptr = ptr;
		}
		
		/*
		unsigned long ptr = GPR(28);
		unsigned long ptr2 = GPR(31);
		
		// 80d7e3a0 is what I observed
		if(ptr > 0x80000000)
		{
			

			
			printf("Player Aim ptr1 = %x\n", ptr);
		}
		if(ptr2 > 0x80000000)
		{
			
			
			printf("                          Player Aim ptr2 = %x\n", ptr2);
		}
		*/
	}
}

void HLECamMatrixWrite0()
{
	//return;
	
	//printf("Hooked CamMatrixWrite0\n");
	//printf("                  Cam start %ld\n", CoreTiming::GetTicks());

	if(!InferiusEnabled())
	{
		return;
	}
	
	if(!InferiusHeadTrackingEnabled())
	{
		return;
	}
	
	if(!m_initialized)
	{
		Init();
	}
	
	u8 *gameId = Memory::GetPointer(0x80000000);

	char SA2B_USA_ID[] = "GSNE8P";	
	char SA2B_PAL_ID[] = "GSNP8P";	
	char SFA_USA_ID[] = "GF7E01";	

	if(! memcmp(gameId, SFA_USA_ID, 6) )
	{
		// We're in SFA USA
		
		if(SFA_P0_Aim_Address)
		{
			//printf("Cam view of Player aim:\n%.4f %.4f %.4f \n%.4f %.4f %.4f \n%.4f %.4f %.4f \n\n\n", Memory::Read_F32(SFA_P0_Aim_Address+0*4), Memory::Read_F32(SFA_P0_Aim_Address+1*4), Memory::Read_F32(SFA_P0_Aim_Address+2*4), Memory::Read_F32(SFA_P0_Aim_Address+3*4), Memory::Read_F32(SFA_P0_Aim_Address+4*4), Memory::Read_F32(SFA_P0_Aim_Address+5*4), Memory::Read_F32(SFA_P0_Aim_Address+6*4), Memory::Read_F32(SFA_P0_Aim_Address+7*4), Memory::Read_F32(SFA_P0_Aim_Address+8*4));
		}
		
		u32 MatAddress = (u32) GPR(3);
		u32 PlayerXYZAddress = Memory::Read_U32(0x802A2B60) + 0x13C; // ToDo: hook this instead of hardcoding it
		
		//printf("                         Camera Mat Address = %X\n", MatAddress );
		
		// Construct a YawPitch rotation matrix...
		
		Matrix33 mYaw;
		Matrix33 mPitch;
		
		Matrix33 mYawPitch;
		
		Matrix44 mTranslate;
		Matrix44 mYawPitch4;
		Matrix44 mTranslateYawPitch;
		
		// Get inverse of player XYZ, use as translation vector
		float PlayerXYZInv[] = {-Memory::Read_F32(PlayerXYZAddress+0), -(Memory::Read_F32(PlayerXYZAddress+4) + 2.0), -Memory::Read_F32(PlayerXYZAddress+8)}; // 2.5 too high
		
		// Do rotation
		Matrix33::RotateY(mYaw, head_yaw + yaw_offset);
		Matrix33::RotateX(mPitch, head_pitch);
		
		Matrix33::Multiply(mYaw, mPitch, mYawPitch);
		
		//printf("Player 0 XYZ = %.4f %.4f %.4f\n", -PlayerXYZInv[0], -PlayerXYZInv[1], -PlayerXYZInv[2]);
		
		// Generate 4x4 matrix
		
		// Translate function is broken, use the below instead
		Matrix44::LoadIdentity(mTranslate);
		mTranslate.data[12] = PlayerXYZInv[0];
		mTranslate.data[13] = PlayerXYZInv[1];
		mTranslate.data[14] = PlayerXYZInv[2];
		
		Matrix44::LoadMatrix33(mYawPitch4, mYawPitch);
		Matrix44::Multiply(mTranslate, mYawPitch4, mTranslateYawPitch);
		
		//printf("mTranslate last row = %.4f %.4f %.4f %.4f\n\n\n", mTranslate.data[12], mTranslate.data[13], mTranslate.data[14], mTranslate.data[15]);
		
		// Write 4x4 matrix to game
		Write_F32(mTranslateYawPitch.data[0], MatAddress+0*4);
		Write_F32(mTranslateYawPitch.data[1], MatAddress+1*4);
		Write_F32(mTranslateYawPitch.data[2], MatAddress+2*4);
		Write_F32(mTranslateYawPitch.data[4], MatAddress+4*4);
		Write_F32(mTranslateYawPitch.data[5], MatAddress+5*4);
		Write_F32(mTranslateYawPitch.data[6], MatAddress+6*4);
		Write_F32(mTranslateYawPitch.data[8], MatAddress+8*4);
		Write_F32(mTranslateYawPitch.data[9], MatAddress+9*4);
		Write_F32(mTranslateYawPitch.data[10], MatAddress+10*4);
		Write_F32(mTranslateYawPitch.data[12], MatAddress+12*4);
		Write_F32(mTranslateYawPitch.data[13], MatAddress+13*4);
		Write_F32(mTranslateYawPitch.data[14], MatAddress+14*4);
		
		//printf("TransInv Vector: %.4f %.4f %.4f\n\n\n", Memory::Read_F32(MatAddress+12*4), Memory::Read_F32(MatAddress+13*4), Memory::Read_F32(MatAddress+14*4));
		
		/*
		
		// now the other matrix
		
		// Get inverse of player XYZ, use as translation vector
		float PlayerXYZ[] = {Memory::Read_F32(PlayerXYZAddress+0), Memory::Read_F32(PlayerXYZAddress+4), Memory::Read_F32(PlayerXYZAddress+8)};
		
		// Do rotation
		Matrix33::RotateY(mYaw, -head_yaw);
		Matrix33::RotateX(mPitch, -head_pitch);
		
		Matrix33::Multiply(mPitch, mYaw, mYawPitch);
		
		// Generate 4x4 matrix
		Matrix44::Translate(mTranslate, PlayerXYZ);
		Matrix44::LoadMatrix33(mYawPitch4, mYawPitch);
		Matrix44::Multiply(mYawPitch4, mTranslate, mTranslateYawPitch);
		
		// Write 4x4 matrix to game
		Write_F32(mTranslateYawPitch.data[0], MatAddress+16*4);
		Write_F32(mTranslateYawPitch.data[1], MatAddress+17*4);
		Write_F32(mTranslateYawPitch.data[2], MatAddress+18*4);
		Write_F32(mTranslateYawPitch.data[4], MatAddress+20*4);
		Write_F32(mTranslateYawPitch.data[5], MatAddress+21*4);
		Write_F32(mTranslateYawPitch.data[6], MatAddress+22*4);
		Write_F32(mTranslateYawPitch.data[8], MatAddress+24*4);
		Write_F32(mTranslateYawPitch.data[9], MatAddress+25*4);
		Write_F32(mTranslateYawPitch.data[10], MatAddress+26*4);
		Write_F32(mTranslateYawPitch.data[12], MatAddress+28*4);
		Write_F32(mTranslateYawPitch.data[13], MatAddress+29*4);
		Write_F32(mTranslateYawPitch.data[14], MatAddress+30*4);
		
		*/
		
		//printf("CameraAim PC=%x, NPC= %x, LR= %x\n", (int) PC, (int) NPC, (int) LR);
		NPC = LR; // skip the rest of the function
	}
	else
	{
		printf("Unrecognized Inferius game.\n");
	}
	
	//printf("                  Cam End %ld\n", CoreTiming::GetTicks());
}

}	// namespace Inferius
