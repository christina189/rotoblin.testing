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

#include "extension.h"
#include "vglobals.h"
#include "util.h"

// DEPRECATED ON ALL PLATFORMS/TARGETS
// native L4D_GetTeamScore(logical_team, campaign_score=false)
cell_t L4D_GetTeamScore(IPluginContext *pContext, const cell_t *params)
{
	static ICallWrapper *pWrapper = NULL;

	// CTerrorGameRules::GetTeamScore(int,bool)
	if (!pWrapper)
	{
		PassInfo ret;
		ret.flags = PASSFLAG_BYVAL;
		ret.size = sizeof(int);
		ret.type = PassType_Basic;

		REGISTER_NATIVE_ADDR("GetTeamScore", 
			PassInfo pass[2]; \
			pass[0].flags = PASSFLAG_BYVAL; \
			pass[0].size = sizeof(int); \
			pass[0].type = PassType_Basic; \
			pass[1].flags = PASSFLAG_BYVAL; \
			pass[1].size = sizeof(bool); \
			pass[1].type = PassType_Basic; \
			pWrapper = g_pBinTools->CreateCall(addr, CallConv_ThisCall, &ret, pass, /*numparams*/2));
	}

#define SCORE_TEAM_A 1
#define SCORE_TEAM_B 2
#define SCORE_TYPE_ROUND 0
#define SCORE_TYPE_CAMPAIGN 1

	//sanity check that the team index is valid
	if(params[1] != SCORE_TEAM_A && params[1] != SCORE_TEAM_B)
	{
		return pContext->ThrowNativeError("Logical team %d is invalid", params[1]);
	}
	//campaign_score is a boolean so should be 0 (use round score) or 1 only 
	if(params[2] != SCORE_TYPE_ROUND && params[2] != SCORE_TYPE_CAMPAIGN)
	{
		return pContext->ThrowNativeError("campaign_score %d is invalid, needs to be 0 or 1", params[2]);
	}

	/* Get the CTerrorGameRules pointer */
	if (g_pGameRules == NULL)
	{
		return pContext->ThrowNativeError("GameRules unsupported or not available; file a bug report");
	}

	void *gamerules = *g_pGameRules;

	if (gamerules == NULL)
	{
		return pContext->ThrowNativeError("GameRules not available before map is loaded");
	}

	/* Build the vcall argument stack */
	unsigned char vstk[sizeof(void *) + sizeof(int) + sizeof(bool)];
	unsigned char *vptr = vstk;

	*(void **)vptr = gamerules;
	vptr += sizeof(void *);
	*(int *)vptr = params[1];
	vptr += sizeof(int);
	*(bool *)vptr = params[2] ? true : false;

	cell_t retbuffer;
	pWrapper->Execute(vstk, &retbuffer);

	g_pSM->LogMessage(myself, "L4D_GetTeamScore is deprecated. You should be using SDKTools to invoke it instead.");

	return retbuffer;
}

// native L4D_RestartScenarioFromVote(const String:map[])
cell_t L4D_RestartScenarioFromVote(IPluginContext *pContext, const cell_t *params)
{
	static ICallWrapper *pWrapper = NULL;

	// Director::RestartScenario()
	if (!pWrapper)
	{
		REGISTER_NATIVE_ADDR("RestartScenarioFromVote", 
			PassInfo pass[1]; \
			pass[0].flags = PASSFLAG_BYVAL; \
			pass[0].size = sizeof(char *); \
			pass[0].type = PassType_Basic; \
			pWrapper = g_pBinTools->CreateCall(addr, CallConv_ThisCall, /*returnInfo*/NULL, pass, /*numparams*/1));
	}

#ifdef PLATFORM_WINDOWS
	void *addr;
	if (!g_pGameConf->GetMemSig("RestartScenarioFromVote", (void **)&addr) || !addr)
	{
		return pContext->ThrowNativeError( "Could not read RestartScenarioFromVote from GameConf");
	}

	int offset;
	if (!g_pGameConf->GetOffset("TheDirector", &offset) || !offset)
	{
#if !defined THEDIRECTOR_SETNEXTMISSION_OFFSET
		return pContext->ThrowNativeError("Could not read 'TheDirector' offset from GameConf");
#endif
	}
#endif

	/* Get the Director pointer */
	if (g_pDirector == NULL)
	{
		return pContext->ThrowNativeError("Director unsupported or not available; file a bug report");
	}

	void *director = *g_pDirector;

	if (director == NULL)
	{
		return pContext->ThrowNativeError("Director not available before map is loaded");
	}

	char *map = NULL;
	pContext->LocalToString(params[1], &map);

	/* Build the vcall argument stack */
	unsigned char vstk[sizeof(void *) + sizeof(char *)];
	unsigned char *vptr = vstk;

	*(void **)vptr = director;
	vptr += sizeof(void *);
	*(char **)vptr = map;
	pWrapper->Execute(vstk, /*retbuffer*/NULL);

	return 1;
}

//DEPRECATED ON L4D2
// native L4D_GetCampaignScores(&scoreA, &scoreB)
cell_t L4D_GetCampaignScores(IPluginContext *pContext, const cell_t *params)
{
#if TARGET_L4D
	const char *baseSignatureName;

#ifdef PLATFORM_WINDOWS
	baseSignatureName = "OnServerShutdown";
#else
	baseSignatureName = "ClearTeamScores_Director";
#endif
	if (!g_pGameConf->GetMemSig(baseSignatureName, (void **)&baseSignatureName) || !baseSignatureName)
	{
		return pContext->ThrowNativeError( "Could not read signature %s from GameConf", baseSignatureName);
	}

	/* Get the code offsets to calculate offsets into Director */
	int offsetA;
	if (!g_pGameConf->GetOffset("ClearTeamScore_A", &offsetA) || !offsetA)
	{
		return pContext->ThrowNativeError("Could not get offset for ClearTeamScore_A");
	}

	int offsetB;
	if (!g_pGameConf->GetOffset("ClearTeamScore_B", &offsetB) || !offsetB)
	{
		return pContext->ThrowNativeError("Could not get offset for ClearTeamScore_B");
	}
	/* Turn the code offsets into direct offsets */
	int campaignScoreA = *(int *)(baseSignatureName + offsetA);
	L4D_DEBUG_LOG("GetCampaignScores offset for score A is %x", campaignScoreA);
	int campaignScoreB = *(int *)(baseSignatureName + offsetB);
	L4D_DEBUG_LOG("GetCampaignScores offset for score B is %x", campaignScoreB);

	/* Get the Director pointer */
	if (g_pDirector == NULL)
	{
		return pContext->ThrowNativeError("Director unsupported or not available; file a bug report");
	}

	unsigned char *director = (unsigned char*) *g_pDirector;

	if (director == NULL)
	{
		return pContext->ThrowNativeError("Director not available before map is loaded");
	}

	/* Read in the campaign scores from the director */
	int scoreA = *(int*)(director + campaignScoreA);
	int scoreB = *(int*)(director + campaignScoreB);

	cell_t *addr;
	pContext->LocalToPhysAddr(params[1], &addr);
	*addr = static_cast<cell_t>(scoreA);

	pContext->LocalToPhysAddr(params[2], &addr);
	*addr = static_cast<cell_t>(scoreB);

	L4D_DEBUG_LOG("L4D_GetCampaignScores(A=%d, B=%d) returned", scoreA, scoreB);
#else
	/*
	 Support for this going forward is deprecated in L4D2
	Users can use OnClearTeamScores detour and check for 'true' when campaign scores are reset
	And keep their own campaign tally using GetTeamScore
	*/
	cell_t *addr;
	pContext->LocalToPhysAddr(params[1], &addr);
	*addr = static_cast<cell_t>(-1);

	pContext->LocalToPhysAddr(params[2], &addr);
	*addr = static_cast<cell_t>(-1);

	g_pSM->LogError(myself, "L4D_OnGetCampaignScores(a,b) has been called. It is deprecated in L4D2, consider updating the plugin using this native.");

#endif

	return 1;
}


// native L4D_LobbyUnreserve()
cell_t L4D_LobbyUnreserve(IPluginContext *pContext, const cell_t *params)
{
	const int64_t cookieUnreserved = 0;

	if(g_pServer == NULL)
	{
		return pContext->ThrowNativeError("CBaseServer not available");
	}

	static ICallWrapper *pWrapper = NULL;

	/* CBaseServer::SetReservationCookie(
			uint64_t reservationCookie,
			const char* formatString,
			va_list ap)

		__cdecl on windows, __thiscall on linux
	*/
	if (!pWrapper)
	{
#ifdef PLATFORM_WINDOWS
		REGISTER_NATIVE_ADDR("SetReservationCookie", 
		PassInfo pass[4]; \
			pass[0].flags = PASSFLAG_BYVAL; \
			pass[0].size = sizeof(IServer *); \
			pass[0].type = PassType_Basic; \
			pass[1].flags = PASSFLAG_BYVAL; \
			pass[1].size = sizeof(uint64_t); \
			pass[1].type = PassType_Basic; \
			pass[2].flags = PASSFLAG_BYVAL; \
			pass[2].size = sizeof(char const*); \
			pass[2].type = PassType_Basic; \
			pass[3].flags = PASSFLAG_BYVAL; \
			pass[3].size = sizeof(void *); \
			pass[3].type = PassType_Basic; \
			pWrapper = g_pBinTools->CreateCall(addr, CallConv_Cdecl, \
							/*retInfo*/NULL, /*paramInfo*/pass, /*numparams*/4));
#else
		REGISTER_NATIVE_ADDR("SetReservationCookie", 
		PassInfo pass[3]; \
			pass[0].flags = PASSFLAG_BYVAL; \
			pass[0].size = sizeof(uint64_t); \
			pass[0].type = PassType_Basic; \
			pass[1].flags = PASSFLAG_BYVAL; \
			pass[1].size = sizeof(char const*); \
			pass[1].type = PassType_Basic; \
			pass[2].flags = PASSFLAG_BYVAL; \
			pass[2].size = sizeof(void *); \
			pass[2].type = PassType_Basic; \
			pWrapper = g_pBinTools->CreateCall(addr, CallConv_ThisCall, \
							/*retInfo*/NULL, /*paramInfo*/pass, /*numparams*/3));
#endif
	}

	/* Build the vcall argument stack */
	unsigned char vstk[sizeof(IServer *) + sizeof(uint64_t) + sizeof(char const *) + sizeof(void *)];
	unsigned char *vptr = vstk;

	*(void **)vptr = g_pServer;
	vptr += sizeof(IServer *);
	*(uint64_t *)vptr = cookieUnreserved;
	vptr += sizeof(uint64_t);
	*(const char **)vptr = "Manually unreserved by Left 4 Downtown Extension";
	vptr += sizeof(char *);
	*(void **)vptr = NULL; // don't bother using a va_list ap :)

	pWrapper->Execute(vstk, /*retbuffer*/NULL);

	L4D_DEBUG_LOG("Invoked CBaseServer::SetReservationCookie(0,fmt,...)");

	return 1;
}

//DEPRECATED ON L4D2 and L4D1 Linux
// native bool:L4D_LobbyIsReserved()
cell_t L4D_LobbyIsReserved(IPluginContext *pContext, const cell_t *params)
{
#if TARGET_L4D2
	g_pSM->LogError(myself, "L4D_LobbyIsReserved() has been called. It is deprecated in L4D2, consider updating the plugin using this native.");

	return 0;
#elif !defined PLATFORM_WINDOWS
	g_pSM->LogError(myself, "L4D_LobbyIsReserved() has been called. It is deprecated in Linux, consider updating the plugin using this native.");

	return 0;
#endif

	void *thisptr;
	char *thisClassName;

	/*
	on Windows we use CBaseServer::IsReserved()
		that function doesn't exist on Linux as it's inlined,
		but it does the same thing.. so yay

	on Linux we use IVEngineServer::IsReserved() 
		which is actually virtual but calling statically is safer :)
	*/
#if defined PLATFORM_WINDOWS
	if(g_pServer == NULL)
	{
		return pContext->ThrowNativeError("CBaseServer not available");
	}

	thisptr = g_pServer;
	thisClassName = "CBaseServer";
#else
	return pContext->ThrowNativeError("LobbyIsReserved not available on Linux");

	if(g_pEngine == NULL)
	{
		return pContext->ThrowNativeError("IVEngineServer not available");
	}

	thisptr = g_pEngine;
	thisClassName = (char *)"IVEngineServer";
#endif

	L4D_DEBUG_LOG("In native L4D_LobbyIsReserved, server=%p, engine=%p/%p", g_pServer, g_pEngine, engine);

	static ICallWrapper *pWrapper = NULL;

	/* bool IVEngineServer::IsReserved()
			virtual call
	*/
	if (!pWrapper)
	{
		PassInfo retInfo; 
		retInfo.flags = PASSFLAG_BYVAL; 
#if defined PLATFORM_WINDOWS
		retInfo.size = sizeof(int);  //ret value in al on linux, eax on windows
#else
		retInfo.size = sizeof(bool); //ret value in al on linux, eax on windows
#endif
		retInfo.type = PassType_Basic; 

		//vcall way too brittle, offset is too high and breaks too often IMHO
#if 0
		int vtableIndex;
		if(!g_pGameConf->GetOffset("IsReserved", &vtableIndex))
		{
			return pContext->ThrowNativeError("Failed to locate offset %s", "IsReserved"); 
		}

		pWrapper = g_pBinTools->CreateVCall(vtableIndex, /*vtableOffset*/0, /*thisOffset*/0,
							/*retInfo*/&retInfo, /*paramInfo*/NULL, /*numparams*/0);
#endif
		REGISTER_NATIVE_ADDR("IsReserved", 
			pWrapper = g_pBinTools->CreateCall(addr, CallConv_ThisCall, \
							/*retInfo*/&retInfo, /*paramInfo*/NULL, /*numparams*/0));

		L4D_DEBUG_LOG("Built call wrapper for %s::IsReserved", thisClassName);
	}

	/* Build the vcall argument stack */
	unsigned char vstk[sizeof(void *)];
	unsigned char *vptr = vstk;

	int retbuffer = 0;

	*(void **)vptr = thisptr;

	L4D_DEBUG_LOG("Will execute %s::IsReserved", thisClassName);
	pWrapper->Execute(vstk, &retbuffer);

	L4D_DEBUG_LOG("Invoked %s::IsReserved, got back = %d", thisClassName, retbuffer);

	return retbuffer;
}

// native L4D_ScavengeBeginRoundSetupTime()
cell_t L4D_ScavengeBeginRoundSetupTime(IPluginContext *pContext, const cell_t *params)
{
#if TARGET_L4D
	return pContext->ThrowNativeError("L4D_ScavengeBeginRoundSetupTime() does not exist in L4D");
#endif

	static ICallWrapper *pWrapper = NULL;

	// Director::RestartScenario()
	if (!pWrapper)
	{
		REGISTER_NATIVE_ADDR("CDirectorScavengeMode_OnBeginRoundSetupTime", 
			pWrapper = g_pBinTools->CreateCall(addr, CallConv_ThisCall, /*returnInfo*/NULL, /*Pass*/NULL, /*numparams*/0));
	}

	void *addr;
	if (!g_pGameConf->GetMemSig("CDirectorScavengeMode_OnBeginRoundSetupTime", (void **)&addr) || !addr)
	{
		return pContext->ThrowNativeError( "Could not read CDirectorScavengeMode_OnBeginRoundSetupTime from GameConf");
	}

	/* Get the Director pointer */
	if (g_pDirector == NULL)
	{
		return pContext->ThrowNativeError("Director unsupported or not available; file a bug report");
	}

	void *director = *g_pDirector;

	if (director == NULL)
	{
		return pContext->ThrowNativeError("Director not available before map is loaded");
	}

	/* Get the DirectorScavengeMode pointer */
	if (!g_pGameConf->GetMemSig("CDirector_AreWanderersAllowed", (void **)&addr) || !addr)
	{
		return pContext->ThrowNativeError( "Could not read CDirector_AreWanderersAllowed from GameConf");
	}

	//read offset into CDirector_AreWanderersAllowed
	int offset;
	if (!g_pGameConf->GetOffset("CDirectorScavengeMode", &offset) || !offset)
	{
#if !defined THEDIRECTOR_SETNEXTMISSION_OFFSET
		return pContext->ThrowNativeError("Could not read 'CDirectorScavengeMode' offset from GameConf");
#endif
	}

	/* read the offset into director */
	offset = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(addr) + offset);
	L4D_DEBUG_LOG("CDirectorScavengeMode director offset calculated to %d", offset);

	void *directorScavengeMode = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(director) + offset);
	L4D_DEBUG_LOG("CDirectorScavengeMode pointer is %x", directorScavengeMode);

	if(directorScavengeMode == NULL)
	{
		return pContext->ThrowNativeError("CDirectorScavengeMode unsupported or not available; file a bug report");
	}

	/* Build the vcall argument stack */
	unsigned char vstk[sizeof(void *)];
	unsigned char *vptr = vstk;

	*(void **)vptr = directorScavengeMode;
	pWrapper->Execute(vstk, /*retbuffer*/NULL);

	return 1;
}

sp_nativeinfo_t g_L4DoNatives[] = 
{
	{"L4D_GetTeamScore",			L4D_GetTeamScore},
	{"L4D_GetCampaignScores",		L4D_GetCampaignScores},
	{"L4D_RestartScenarioFromVote",	L4D_RestartScenarioFromVote},
	{"L4D_LobbyUnreserve",			L4D_LobbyUnreserve},
	{"L4D_LobbyIsReserved",			L4D_LobbyIsReserved},
	{"L4D_ScavengeBeginRoundSetupTime", L4D_ScavengeBeginRoundSetupTime},
	{NULL,							NULL}
};
