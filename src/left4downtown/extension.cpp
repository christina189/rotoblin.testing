/**
 * vim: set ts=4 :
 * =============================================================================
 * Left 4 Downtown SourceMod Extension
 * Copyright (C) 2009 Igor "Downtown1" Smirnov.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include <string.h>

#include "extension.h"
#include "vglobals.h"
#include "util.h"
#include "player_slots.h"
#include "boss_spawns.h"
#include "convar_public.h"

#include <ISDKTools.h>

#include "codepatch/patchmanager.h"
#include "codepatch/autopatch.h"

#include "detours/spawn_witch.h"
#include "detours/spawn_tank.h"
#include "detours/clear_team_scores.h"
#include "detours/set_campaign_scores.h"
#include "detours/server_player_counts.h"
#include "detours/first_survivor_left_safe_area.h"

#if TARGET_L4D
#define GAMECONFIG_FILE "left4downtown.l4d"
#else
#define GAMECONFIG_FILE "left4downtown.l4d2"
#endif

Left4Downtown g_Left4DowntownTools;		/**< Global singleton for extension's main interface */
IGameConfig *g_pGameConf = NULL;
IGameConfig *g_pGameConfSDKTools = NULL;
IBinTools *g_pBinTools = NULL;
IServer *g_pServer = NULL; //ptr to CBaseServer
ISDKTools *g_pSDKTools = NULL;
IServerGameEnts *gameents = NULL;
CGlobalVars *gpGlobals;

IForward *g_pFwdOnSpawnTank = NULL;
IForward *g_pFwdOnSpawnWitch = NULL;
IForward *g_pFwdOnClearTeamScores = NULL;
IForward *g_pFwdOnSetCampaignScores = NULL;
IForward *g_pFwdOnFirstSurvivorLeftSafeArea = NULL;

ICvar *icvar = NULL;
SMEXT_LINK(&g_Left4DowntownTools);

extern sp_nativeinfo_t g_L4DoNatives[];

ConVar g_Version("left4downtown_version", SMEXT_CONF_VERSION, FCVAR_SPONLY|FCVAR_NOTIFY, "Left 4 Downtown Extension Version");
ConVar g_MaxPlayers("l4d_maxplayers", "-1", FCVAR_SPONLY|FCVAR_NOTIFY, "Overrides maxplayers with this value");
PatchManager g_PatchManager;

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

bool Left4Downtown::SDK_OnLoad(char *error, size_t maxlength, bool late)
{

#if TARGET_L4D
	//only load extension for l4d
	if (strcmp(g_pSM->GetGameFolderName(), "left4dead") != 0)
	{
		UTIL_Format(error, maxlength, "Cannot Load Left 4 Downtown Ext on mods other than L4D1");
		return false;
	}
#elif TARGET_L4D2
	//only load extension for l4d2
	if (strcmp(g_pSM->GetGameFolderName(), "left4dead2") != 0)
	{
		UTIL_Format(error, maxlength, "Cannot Load Left 4 Downtown Ext on mods other than L4D2");
		return false;
	}
#endif


	//load sigscans and offsets, etc from our gamedata file
	char conf_error[255] = "";
	if (!gameconfs->LoadGameConfigFile(GAMECONFIG_FILE, &g_pGameConf, conf_error, sizeof(conf_error)))
	{
		if (conf_error[0])
		{
			UTIL_Format(error, maxlength, "Could not read " GAMECONFIG_FILE ".txt: %s", conf_error);
		}
		return false;
	}

	//load sigscans and offsets from the sdktools gamedata file
	if (!gameconfs->LoadGameConfigFile("sdktools.games", &g_pGameConfSDKTools, conf_error, sizeof(conf_error)))
	{
		return false;
	}
	sharesys->AddDependency(myself, "bintools.ext", true, true);
	sharesys->AddNatives(myself, g_L4DoNatives);

	g_pFwdOnSpawnTank = forwards->CreateForward("L4D_OnSpawnTank", ET_Event, 2, /*types*/NULL, Param_Array, Param_Array);
	g_pFwdOnSpawnWitch = forwards->CreateForward("L4D_OnSpawnWitch", ET_Event, 2, /*types*/NULL, Param_Array, Param_Array);
	g_pFwdOnClearTeamScores = forwards->CreateForward("L4D_OnClearTeamScores", ET_Event, 1, /*types*/NULL, Param_Cell);
	g_pFwdOnSetCampaignScores = forwards->CreateForward("L4D_OnSetCampaignScores", ET_Event, 2, /*types*/NULL, Param_CellByRef, Param_CellByRef);
	g_pFwdOnFirstSurvivorLeftSafeArea = forwards->CreateForward("L4D_OnFirstSurvivorLeftSafeArea", ET_Event, 1, /*types*/NULL, Param_Cell);

	playerhelpers->AddClientListener(&g_Left4DowntownTools);

	Detour::Init(g_pSM->GetScriptingEngine(), g_pGameConf);

	return true;
}

void Left4Downtown::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, g_pBinTools);
	SM_GET_LATE_IFACE(SDKTOOLS, g_pSDKTools);

	if (!g_pBinTools || !g_pSDKTools)
	{
		L4D_DEBUG_LOG("Failed to loan bintools or failed to load sdktools");
		return;
	}

	IServer *server = g_pSDKTools->GetIServer();
	L4D_DEBUG_LOG("Address of IServer is %p", server);
	//reading out server->GetName() we consistently seem to get (the same?) 
	//garbage characters. this is possibly causing a crash on windows servers
	//when a player connects. so lets not read the server name :(
	//L4D_DEBUG_LOG("Server name is %s", server->GetName());
	g_pServer = server;

	InitializeValveGlobals();

	/*
		allow more than 8 players in l4d if l4d_maxplayers is not -1
		also if +maxplayers or -maxplayers is not the default value (8)
	*/
	g_MaxPlayers.InstallChangeCallback(::OnMaxPlayersChanged);

	/*
	read the +-maxplayers from command line
	*/

	/*commenting out the code below
	seems to not make it crash during connection to my NFO
	
	START HERE
	*/
	ICommandLine *cmdLine = CommandLine_Tier0();
	int maxplayers = -1;
	L4D_DEBUG_LOG("Command line is: %s", cmdLine->GetCmdLine());

	maxplayers = cmdLine->ParmValue("+maxplayers", -1);
	L4D_DEBUG_LOG("Command line +maxplayers is: %d", maxplayers);

	if(maxplayers == -1) 
	{
		maxplayers = cmdLine->ParmValue("-maxplayers", -1);
	}
	/*
	end here
	*/
	PlayerSlots::MaxPlayers = maxplayers;

	/*
	detour the witch/spawn spawns
	*/
	//automatically will unregister/cleanup themselves when the ext is unloaded

	g_PatchManager.Register(new AutoPatch<Detours::SpawnTank>());
	g_PatchManager.Register(new AutoPatch<Detours::SpawnWitch>());
	g_PatchManager.Register(new AutoPatch<Detours::ClearTeamScores>());
	g_PatchManager.Register(new AutoPatch<Detours::SetCampaignScores>());

#if TARGET_L4D2
	g_PatchManager.Register(new AutoPatch<Detours::FirstSurvivorLeftSafeArea>());
#endif

	g_PatchManager.Register(new AutoPatch<Detours::ServerPlayerCounts>());
}

void Left4Downtown::SDK_OnUnload()
{
	gameconfs->CloseGameConfigFile(g_pGameConf);
	gameconfs->CloseGameConfigFile(g_pGameConfSDKTools);

	playerhelpers->RemoveClientListener(&g_Left4DowntownTools);

	//go back to normal old asm 
	PlayerSlots::Unpatch();

	g_PatchManager.UnregisterAll();

	forwards->ReleaseForward(g_pFwdOnSpawnTank);
	forwards->ReleaseForward(g_pFwdOnSpawnWitch);
	forwards->ReleaseForward(g_pFwdOnClearTeamScores);
	forwards->ReleaseForward(g_pFwdOnSetCampaignScores);
	forwards->ReleaseForward(g_pFwdOnFirstSurvivorLeftSafeArea);
}

class BaseAccessor : public IConCommandBaseAccessor
{
public:
	bool RegisterConCommandBase(ConCommandBase *pCommandBase)
	{
		/* Always call META_REGCVAR instead of going through the engine. */
		return META_REGCVAR(pCommandBase);
	}
} s_BaseAccessor;


bool Left4Downtown::SDK_OnMetamodLoad(SourceMM::ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);

	g_pCVar = icvar;
	ConVar_Register(0, &s_BaseAccessor);

	GET_V_IFACE_ANY(GetServerFactory, gameents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);
	gpGlobals = ismm->GetCGlobals();

	return true;
}
	/**
	 * @brief Called when the server is activated.
	 */
void Left4Downtown::OnServerActivated(int max_clients)
{
	L4D_DEBUG_LOG("Server activated with %d maxclients", max_clients);

	static bool firstTime = true;
	if(!firstTime)
	{
		L4D_DEBUG_LOG("Server activated with %d maxclients (doing nothing)", max_clients);
		return;
	}
	firstTime = false;

	PlayerSlots::MaxClients = max_clients;
	
	ConVarPublic *maxPlayersCvar = (ConVarPublic*)&g_MaxPlayers;
	maxPlayersCvar->m_bHasMax = true;
	maxPlayersCvar->m_fMaxVal = (float) max_clients;
	maxPlayersCvar->m_bHasMin  = true;
	maxPlayersCvar->m_fMinVal = (float) -1;

#if RESTRICT_MAX_PLAYERS_BY_COMMAND_LINE
	int max_players = PlayerSlots::MaxPlayers;

	if(max_players >= 0)
	{
		//dont allow it to be larger than max_clients
		max_players = max_players <= max_clients ? max_players : max_clients;

		maxPlayersCvar->m_fMaxVal = (float) max_players; 

		//if GSPs set maxplayers to the non-default value
		//we will patch the code then by default even if l4d_maxplayers is never set
		//otherwise default is -1 disabled
		if(DEFAULT_MAX_PLAYERS != max_players)
		{
			//by putting only +-maxplayers and never l4d_maxplayers, we set maxplayers +-maxplayers value
			//by specifiying l4d_maxplayers, we override the +-maxplayers value as long as its <= +-maxplayers
			g_MaxPlayers.SetValue(max_players);

			static char defaultPlayers[5];
			snprintf(defaultPlayers, sizeof(defaultPlayers), "%d", max_players);
			maxPlayersCvar->m_pszDefaultValue = defaultPlayers;
		}
	}
#endif
}
