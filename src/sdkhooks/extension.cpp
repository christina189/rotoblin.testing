#include "extension.h"
#include "compat_wrappers.h"
#include "macros.h"
#include "natives.h"

//#define SDKHOOKSDEBUG


/**
 * Globals
 */
SDKHooks g_Interface;
SMEXT_LINK(&g_Interface);

CGlobalVars *g_pGlobals;
CUtlVector<HookList> g_HookList;

IBinTools *g_pBinTools = NULL;
void *g_pEntityFactoryDictAddr = NULL;
IEntityFactoryDictionary *g_pEntityFactoryDict = NULL;
IForward *g_pOnEntityCreated = NULL;
IForward *g_pOnEntityDestroyed = NULL;
IForward *g_pOnGetGameNameDescription = NULL;
IForward *g_pOnLevelInit = NULL;
IGameConfig *g_pGameConf = NULL;
IServerGameEnts *gameents = NULL;

char g_szMapEntities[2097152];

bool g_bHookSupported[SDKHook_MAXHOOKS] = {false};


/**
 * IEntityFactoryDictionary, IServerGameDLL & IVEngineServer Hooks
 */
SH_DECL_HOOK1(IEntityFactoryDictionary, Create, SH_NOATTRIB, 0, IServerNetworkable *, const char *);
SH_DECL_HOOK6(IServerGameDLL, LevelInit, SH_NOATTRIB, 0, bool, const char *, const char *, const char *, const char *, bool, bool);
SH_DECL_HOOK0(IServerGameDLL, GetGameDescription, SH_NOATTRIB, 0, const char *);
SH_DECL_HOOK0(IVEngineServer, GetMapEntitiesString, SH_NOATTRIB, 0, const char *);


/**
 * CBaseEntity Hooks
 */
SH_DECL_MANUALHOOK1_void(EndTouch, 0, 0, 0, CBaseEntity *);
SH_DECL_MANUALHOOK1_void(FireBullets, 0, 0, 0, FireBulletsInfo_t const&);
SH_DECL_MANUALHOOK1(OnTakeDamage, 0, 0, 0, int, CTakeDamageInfoHack &);
SH_DECL_MANUALHOOK0_void(PreThink, 0, 0, 0);
SH_DECL_MANUALHOOK0_void(PostThink, 0, 0, 0);
SH_DECL_MANUALHOOK2_void(SetTransmit, 0, 0, 0, CCheckTransmitInfo *, bool);
SH_DECL_MANUALHOOK0_void(Spawn, 0, 0, 0);
SH_DECL_MANUALHOOK1_void(StartTouch, 0, 0, 0, CBaseEntity *);
SH_DECL_MANUALHOOK0_void(Think, 0, 0, 0);
SH_DECL_MANUALHOOK1_void(Touch, 0, 0, 0, CBaseEntity *);
SH_DECL_MANUALHOOK3_void(TraceAttack, 0, 0, 0, CTakeDamageInfoHack &, const Vector &, CGameTrace *);
SH_DECL_MANUALHOOK0_void(UpdateOnRemove, 0, 0, 0);
SH_DECL_MANUALHOOK1(Weapon_CanSwitchTo, 0, 0, 0, bool, CBaseCombatWeapon *);
SH_DECL_MANUALHOOK1(Weapon_CanUse, 0, 0, 0, bool, CBaseCombatWeapon *);
SH_DECL_MANUALHOOK3_void(Weapon_Drop, 0, 0, 0, CBaseCombatWeapon *, const Vector *, const Vector *);
SH_DECL_MANUALHOOK1_void(Weapon_Equip, 0, 0, 0, CBaseCombatWeapon *);
SH_DECL_MANUALHOOK2(Weapon_Switch, 0, 0, 0, bool, CBaseCombatWeapon *, int);
SH_DECL_MANUALHOOK2(ShouldCollide, 0, 0, 0, bool, int, int);


class BaseAccessor : public IConCommandBaseAccessor
{
public:
	bool RegisterConCommandBase(ConCommandBase *pCommandBase)
	{
		return META_REGCVAR(pCommandBase);
	}
} s_BaseAccessor;


/**
 * Forwards
 */
bool SDKHooks::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	sharesys->AddDependency(myself, "bintools.ext", true, true);
	sharesys->AddNatives(myself, g_Natives);
	
	playerhelpers->AddClientListener(&g_Interface);
	plsys->AddPluginsListener(&g_Interface);

	g_pOnEntityCreated = forwards->CreateForward("OnEntityCreated", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	g_pOnEntityDestroyed = forwards->CreateForward("OnEntityDestroyed", ET_Ignore, 1, NULL, Param_Cell);
	g_pOnGetGameNameDescription = forwards->CreateForward("OnGetGameDescription", ET_Hook, 2, NULL, Param_String);
	g_pOnLevelInit = forwards->CreateForward("OnLevelInit", ET_Hook, 2, NULL, Param_String, Param_String);

#if SOURCE_ENGINE >= SE_ORANGEBOX
	g_pCVar = icvar;
	ConVar_Register(0, &s_BaseAccessor);
#else
	ConCommandBaseMgr::OneTimeInit(&s_BaseAccessor);
#endif

	char conf_error[255] = "";
	if(!gameconfs->LoadGameConfigFile("sdkhooks.games", &g_pGameConf, conf_error, sizeof(conf_error)))
	{
		if(conf_error[0])
			snprintf(error, maxlength, "Could not read sdkhooks.games.txt: %s", conf_error);
		
		return false;
	}
	if(!g_pGameConf->GetMemSig("IEntityFactoryDictionary", &g_pEntityFactoryDictAddr))
	{
		snprintf(error, maxlength, "Failed to locate IEntityFactoryDictionary sig!");
		return false;
	}
	int offset;
	CHECKOFFSET("UpdateOnRemove")
	{
		SH_MANUALHOOK_RECONFIGURE(UpdateOnRemove, offset, 0, 0);
	}
	else
	{
		snprintf(error, maxlength, "Offset for UpdateOnRemove not found for this mod");
		return false;
	}

	SetupHooks();

	return true;
}

void SDKHooks::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, g_pBinTools);

	if(!g_pBinTools)
		return;

	PassInfo retData;
	retData.flags = PASSFLAG_BYVAL;
	retData.size = sizeof(void *);
	retData.type = PassType_Basic;

	ICallWrapper *pWrapper = g_pBinTools->CreateCall(g_pEntityFactoryDictAddr, CallConv_Cdecl, &retData, NULL, 0);
	void *returnData = NULL;
	pWrapper->Execute(NULL, &returnData);
	pWrapper->Destroy();
	if(!returnData)
	{
		g_pSM->LogError(myself, "Sig was loaded but NULL was returned...");
		return;
	}

	g_pEntityFactoryDict = (IEntityFactoryDictionary *)returnData;
	if(!g_pEntityFactoryDict)
	{
		g_pSM->LogError(myself, "couldn't recast IEntityFactoryDictionary...");
		return;
	}

	SH_ADD_HOOK_MEMFUNC(IEntityFactoryDictionary, Create, g_pEntityFactoryDict, &g_Interface, &SDKHooks::Hook_Create, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, LevelInit, gamedll, &g_Interface, &SDKHooks::Hook_LevelInit, false);
	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GetGameDescription, gamedll, &g_Interface, &SDKHooks::Hook_GetGameDescription, false);
	SH_ADD_HOOK_MEMFUNC(IVEngineServer, GetMapEntitiesString, engine, &g_Interface, &SDKHooks::Hook_GetMapEntitiesString, false);
}

void SDKHooks::SDK_OnUnload()
{
	// Remove left over hooks
	HOOKLOOP
		Unhook(i);

	SH_REMOVE_HOOK_MEMFUNC(IEntityFactoryDictionary, Create, g_pEntityFactoryDict, &g_Interface, &SDKHooks::Hook_Create, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, LevelInit, gamedll, &g_Interface, &SDKHooks::Hook_LevelInit, false);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, GetGameDescription, gamedll, &g_Interface, &SDKHooks::Hook_GetGameDescription, false);
	SH_REMOVE_HOOK_MEMFUNC(IVEngineServer, GetMapEntitiesString, engine, &g_Interface, &SDKHooks::Hook_GetMapEntitiesString, false);

	forwards->ReleaseForward(g_pOnEntityCreated);
	forwards->ReleaseForward(g_pOnEntityDestroyed);
	forwards->ReleaseForward(g_pOnGetGameNameDescription);
	forwards->ReleaseForward(g_pOnLevelInit);

	gameconfs->CloseGameConfigFile(g_pGameConf);

	playerhelpers->RemoveClientListener(&g_Interface);
	plsys->RemovePluginsListener(&g_Interface);
}

bool SDKHooks::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetServerFactory, gameents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);

	g_pGlobals = ismm->GetCGlobals();

	return true;
}

void SDKHooks::OnPluginUnloaded(IPlugin *plugin)
{
	IPluginContext *plugincontext = plugin->GetBaseContext();
	HOOKLOOP
	{
		if(g_HookList[i].callback->GetParentContext() == plugincontext)
			Unhook(i);
	}
}

void SDKHooks::OnClientDisconnecting(int client)
{
	CBaseEntity *pEnt = gameents->EdictToBaseEntity(PEntityOfEntIndex(client));
	SDKHooks::RemoveEntityHooks(pEnt);
}

/**
 * Functions
 */
cell_t SDKHooks::Call(int entity, SDKHookType type, int other = -1)
{
	IPluginFunction *callback = NULL;
	cell_t res, ret = Pl_Continue;

	HOOKLOOP
	{
		if(g_HookList[i].entity != entity || g_HookList[i].type != type)
			continue;

		callback = g_HookList[i].callback;
		callback->PushCell(entity);
		if(other > -1)
			callback->PushCell(other);

		callback->Execute(&res);
		if(res > ret)
			ret = res;
	}

	return ret;
}

void SDKHooks::SetupHooks()
{
	int offset;

	CHECKOFFSET("EndTouch")
	{
		SH_MANUALHOOK_RECONFIGURE(EndTouch, offset, 0, 0);
		g_bHookSupported[SDKHook_EndTouch] = true;
	}
	CHECKOFFSET("FireBullets")
	{
		SH_MANUALHOOK_RECONFIGURE(FireBullets, offset, 0, 0);
		g_bHookSupported[SDKHook_FireBulletsPost] = true;
	}
	CHECKOFFSET("OnTakeDamage")
	{
		SH_MANUALHOOK_RECONFIGURE(OnTakeDamage, offset, 0, 0);
		g_bHookSupported[SDKHook_OnTakeDamage] = true;
		g_bHookSupported[SDKHook_OnTakeDamagePost] = true;
	}
	CHECKOFFSET("PreThink")
	{
		SH_MANUALHOOK_RECONFIGURE(PreThink, offset, 0, 0);
		g_bHookSupported[SDKHook_PreThink] = true;
		g_bHookSupported[SDKHook_PreThinkPost] = true;
	}
	CHECKOFFSET("PostThink")
	{
		SH_MANUALHOOK_RECONFIGURE(PostThink, offset, 0, 0);
		g_bHookSupported[SDKHook_PostThink] = true;
		g_bHookSupported[SDKHook_PostThinkPost] = true;
	}
	CHECKOFFSET("SetTransmit")
	{
		SH_MANUALHOOK_RECONFIGURE(SetTransmit, offset, 0, 0);
		g_bHookSupported[SDKHook_SetTransmit] = true;
	}
	CHECKOFFSET("ShouldCollide")
	{
		SH_MANUALHOOK_RECONFIGURE(ShouldCollide, offset, 0, 0);
		g_bHookSupported[SDKHook_ShouldCollide] = true;
	}
	CHECKOFFSET("Spawn")
	{
		SH_MANUALHOOK_RECONFIGURE(Spawn, offset, 0, 0);
		g_bHookSupported[SDKHook_Spawn] = true;
	}
	CHECKOFFSET("StartTouch")
	{
		SH_MANUALHOOK_RECONFIGURE(StartTouch, offset, 0, 0);
		g_bHookSupported[SDKHook_StartTouch] = true;
	}
	CHECKOFFSET("Think")
	{
		SH_MANUALHOOK_RECONFIGURE(Think, offset, 0, 0);
		g_bHookSupported[SDKHook_Think] = true;
		g_bHookSupported[SDKHook_ThinkPost] = true;
	}
	CHECKOFFSET("Touch")
	{
		SH_MANUALHOOK_RECONFIGURE(Touch, offset, 0, 0);
		g_bHookSupported[SDKHook_Touch] = true;
	}
	CHECKOFFSET("TraceAttack")
	{
		SH_MANUALHOOK_RECONFIGURE(TraceAttack, offset, 0, 0);
		g_bHookSupported[SDKHook_TraceAttack] = true;
		g_bHookSupported[SDKHook_TraceAttackPost] = true;
	}
	CHECKOFFSET("Weapon_CanSwitchTo")
	{
		SH_MANUALHOOK_RECONFIGURE(Weapon_CanSwitchTo, offset, 0, 0);
		g_bHookSupported[SDKHook_WeaponCanSwitchTo] = true;
	}
	CHECKOFFSET("Weapon_CanUse")
	{
		SH_MANUALHOOK_RECONFIGURE(Weapon_CanUse, offset, 0, 0);
		g_bHookSupported[SDKHook_WeaponCanUse] = true;
	}
	CHECKOFFSET("Weapon_Drop")
	{
		SH_MANUALHOOK_RECONFIGURE(Weapon_Drop, offset, 0, 0);
		g_bHookSupported[SDKHook_WeaponDrop] = true;
	}
	CHECKOFFSET("Weapon_Equip")
	{
		SH_MANUALHOOK_RECONFIGURE(Weapon_Equip, offset, 0, 0);
		g_bHookSupported[SDKHook_WeaponEquip] = true;
	}
	CHECKOFFSET("Weapon_Switch")
	{
		SH_MANUALHOOK_RECONFIGURE(Weapon_Switch, offset, 0, 0);
		g_bHookSupported[SDKHook_WeaponSwitch] = true;
	}
}

HookReturn SDKHooks::Hook(int entity, SDKHookType type, IPluginFunction *callback)
{
	if(!g_bHookSupported[type])
		return HookRet_NotSupported;
	CBaseEntity *pEnt = gameents->EdictToBaseEntity(PEntityOfEntIndex(entity));
	if(!pEnt)
		return HookRet_InvalidEntity;
	if(type < 0 || type >= SDKHook_MAXHOOKS)
		return HookRet_InvalidHookType;

	bool bHooked = false;
	HOOKLOOP
	{
		if (g_HookList[i].entity == entity && g_HookList[i].type == type)
		{
			bHooked = true;
			break;
		}
	}
	if (!bHooked)
	{
		switch(type)
		{
			case SDKHook_EndTouch:
				SH_ADD_MANUALHOOK_MEMFUNC(EndTouch, pEnt, &g_Interface, &SDKHooks::Hook_EndTouch, false);
				break;
			case SDKHook_FireBulletsPost:
				SH_ADD_MANUALHOOK_MEMFUNC(FireBullets, pEnt, &g_Interface, &SDKHooks::Hook_FireBulletsPost, true);
				break;
			case SDKHook_OnTakeDamage:
				SH_ADD_MANUALHOOK_MEMFUNC(OnTakeDamage, pEnt, &g_Interface, &SDKHooks::Hook_OnTakeDamage, false);
				break;
			case SDKHook_OnTakeDamagePost:
				SH_ADD_MANUALHOOK_MEMFUNC(OnTakeDamage, pEnt, &g_Interface, &SDKHooks::Hook_OnTakeDamagePost, true);
				break;
			case SDKHook_PreThink:
				SH_ADD_MANUALHOOK_MEMFUNC(PreThink, pEnt, &g_Interface, &SDKHooks::Hook_PreThink, false);
				break;
			case SDKHook_PreThinkPost:
				SH_ADD_MANUALHOOK_MEMFUNC(PreThink, pEnt, &g_Interface, &SDKHooks::Hook_PreThinkPost, true);
				break;
			case SDKHook_PostThink:
				SH_ADD_MANUALHOOK_MEMFUNC(PostThink, pEnt, &g_Interface, &SDKHooks::Hook_PostThink, false);
				break;
			case SDKHook_PostThinkPost:
				SH_ADD_MANUALHOOK_MEMFUNC(PostThink, pEnt, &g_Interface, &SDKHooks::Hook_PostThinkPost, true);
				break;
			case SDKHook_SetTransmit:
				SH_ADD_MANUALHOOK_MEMFUNC(SetTransmit, pEnt, &g_Interface, &SDKHooks::Hook_SetTransmit, false);
				break;
			case SDKHook_Spawn:
				SH_ADD_MANUALHOOK_MEMFUNC(Spawn, pEnt, &g_Interface, &SDKHooks::Hook_Spawn, false);
				break;
			case SDKHook_StartTouch:
				SH_ADD_MANUALHOOK_MEMFUNC(StartTouch, pEnt, &g_Interface, &SDKHooks::Hook_StartTouch, false);
				break;
			case SDKHook_Think:
				SH_ADD_MANUALHOOK_MEMFUNC(Think, pEnt, &g_Interface, &SDKHooks::Hook_Think, false);
				break;
			case SDKHook_ThinkPost:
				SH_ADD_MANUALHOOK_MEMFUNC(Think, pEnt, &g_Interface, &SDKHooks::Hook_ThinkPost, true);
				break;
			case SDKHook_Touch:
				SH_ADD_MANUALHOOK_MEMFUNC(Touch, pEnt, &g_Interface, &SDKHooks::Hook_Touch, false);
				break;
			case SDKHook_TraceAttack:
				SH_ADD_MANUALHOOK_MEMFUNC(TraceAttack, pEnt, &g_Interface, &SDKHooks::Hook_TraceAttack, false);
				break;
			case SDKHook_TraceAttackPost:
				SH_ADD_MANUALHOOK_MEMFUNC(TraceAttack, pEnt, &g_Interface, &SDKHooks::Hook_TraceAttackPost, true);
				break;
			case SDKHook_WeaponCanSwitchTo:
				SH_ADD_MANUALHOOK_MEMFUNC(Weapon_CanSwitchTo, pEnt, &g_Interface, &SDKHooks::Hook_WeaponCanSwitchTo, false);
				break;
			case SDKHook_WeaponCanUse:
				SH_ADD_MANUALHOOK_MEMFUNC(Weapon_CanUse, pEnt, &g_Interface, &SDKHooks::Hook_WeaponCanUse, false);
				break;
			case SDKHook_WeaponDrop:
				SH_ADD_MANUALHOOK_MEMFUNC(Weapon_Drop, pEnt, &g_Interface, &SDKHooks::Hook_WeaponDrop, false);
				break;
			case SDKHook_WeaponEquip:
				SH_ADD_MANUALHOOK_MEMFUNC(Weapon_Equip, pEnt, &g_Interface, &SDKHooks::Hook_WeaponEquip, false);
				break;
			case SDKHook_WeaponSwitch:
				SH_ADD_MANUALHOOK_MEMFUNC(Weapon_Switch, pEnt, &g_Interface, &SDKHooks::Hook_WeaponSwitch, false);
				break;
			case SDKHook_ShouldCollide:
				SH_ADD_MANUALHOOK_MEMFUNC(ShouldCollide, pEnt, &g_Interface, &SDKHooks::Hook_ShouldCollide, false);
				break;
		}
	}

	// Add hook to hook list
	HookList hook;
	hook.entity = entity;
	hook.type = type;
	hook.callback = callback;
	g_HookList.AddToTail(hook);
#ifdef SDKHOOKSDEBUG
	META_CONPRINTF("DEBUG: Adding to hooklist (ent%d, type%s, cb%d). Total hook count %d\n", entity, g_szHookNames[type], callback, g_HookList.Count());
#endif
	return HookRet_Successful;
}

void SDKHooks::Unhook(int index)
{
	CBaseEntity *pEnt = gameents->EdictToBaseEntity(PEntityOfEntIndex(g_HookList[index].entity));
	if(!pEnt)
		return;

	int iHooks = 0;
	HOOKLOOP
	{
		if (g_HookList[i].entity == g_HookList[index].entity && g_HookList[i].type == g_HookList[index].type)
		{
			iHooks++;
#ifdef SDKHOOKSDEBUG
			META_CONPRINTF("DEBUG: Found hook %d on entity %d\n", i, g_HookList[index].entity);
#endif
		}
	}
	if (iHooks == 1)
	{
#ifdef SDKHOOKSDEBUG
		META_CONPRINTF("DEBUG: Removing hook for hooktype %d\n", g_HookList[index].type);
#endif
		switch(g_HookList[index].type)
		{
			case SDKHook_EndTouch:
				SH_REMOVE_MANUALHOOK_MEMFUNC(EndTouch, pEnt, &g_Interface, &SDKHooks::Hook_EndTouch, false);
				break;
			case SDKHook_FireBulletsPost:
				SH_REMOVE_MANUALHOOK_MEMFUNC(FireBullets, pEnt, &g_Interface, &SDKHooks::Hook_FireBulletsPost, true);
				break;
			case SDKHook_OnTakeDamage:
				SH_REMOVE_MANUALHOOK_MEMFUNC(OnTakeDamage, pEnt, &g_Interface, &SDKHooks::Hook_OnTakeDamage, false);
				break;
			case SDKHook_OnTakeDamagePost:
				SH_REMOVE_MANUALHOOK_MEMFUNC(OnTakeDamage, pEnt, &g_Interface, &SDKHooks::Hook_OnTakeDamagePost, true);
				break;
			case SDKHook_PreThink:
				SH_REMOVE_MANUALHOOK_MEMFUNC(PreThink, pEnt, &g_Interface, &SDKHooks::Hook_PreThink, false);
				break;
			case SDKHook_PreThinkPost:
				SH_REMOVE_MANUALHOOK_MEMFUNC(PreThink, pEnt, &g_Interface, &SDKHooks::Hook_PreThinkPost, true);
				break;
			case SDKHook_PostThink:
				SH_REMOVE_MANUALHOOK_MEMFUNC(PostThink, pEnt, &g_Interface, &SDKHooks::Hook_PostThink, false);
				break;
			case SDKHook_PostThinkPost:
				SH_REMOVE_MANUALHOOK_MEMFUNC(PostThink, pEnt, &g_Interface, &SDKHooks::Hook_PostThinkPost, true);
				break;
			case SDKHook_SetTransmit:
				SH_REMOVE_MANUALHOOK_MEMFUNC(SetTransmit, pEnt, &g_Interface, &SDKHooks::Hook_SetTransmit, false);
				break;
			case SDKHook_Spawn:
				SH_REMOVE_MANUALHOOK_MEMFUNC(Spawn, pEnt, &g_Interface, &SDKHooks::Hook_Spawn, false);
				break;
			case SDKHook_StartTouch:
				SH_REMOVE_MANUALHOOK_MEMFUNC(StartTouch, pEnt, &g_Interface, &SDKHooks::Hook_StartTouch, false);
				break;
			case SDKHook_Think:
				SH_REMOVE_MANUALHOOK_MEMFUNC(Think, pEnt, &g_Interface, &SDKHooks::Hook_Think, false);
				break;
			case SDKHook_ThinkPost:
				SH_REMOVE_MANUALHOOK_MEMFUNC(Think, pEnt, &g_Interface, &SDKHooks::Hook_ThinkPost, true);
				break;
			case SDKHook_Touch:
				SH_REMOVE_MANUALHOOK_MEMFUNC(Touch, pEnt, &g_Interface, &SDKHooks::Hook_Touch, false);
				break;
			case SDKHook_TraceAttack:
				SH_REMOVE_MANUALHOOK_MEMFUNC(TraceAttack, pEnt, &g_Interface, &SDKHooks::Hook_TraceAttack, false);
				break;
			case SDKHook_TraceAttackPost:
				SH_REMOVE_MANUALHOOK_MEMFUNC(TraceAttack, pEnt, &g_Interface, &SDKHooks::Hook_TraceAttackPost, true);
				break;
			case SDKHook_WeaponCanSwitchTo:
				SH_REMOVE_MANUALHOOK_MEMFUNC(Weapon_CanSwitchTo, pEnt, &g_Interface, &SDKHooks::Hook_WeaponCanSwitchTo, false);
				break;
			case SDKHook_WeaponCanUse:
				SH_REMOVE_MANUALHOOK_MEMFUNC(Weapon_CanUse, pEnt, &g_Interface, &SDKHooks::Hook_WeaponCanUse, false);
				break;
			case SDKHook_WeaponDrop:
				SH_REMOVE_MANUALHOOK_MEMFUNC(Weapon_Drop, pEnt, &g_Interface, &SDKHooks::Hook_WeaponDrop, false);
				break;
			case SDKHook_WeaponEquip:
				SH_REMOVE_MANUALHOOK_MEMFUNC(Weapon_Equip, pEnt, &g_Interface, &SDKHooks::Hook_WeaponEquip, false);
				break;
			case SDKHook_WeaponSwitch:
				SH_REMOVE_MANUALHOOK_MEMFUNC(Weapon_Switch, pEnt, &g_Interface, &SDKHooks::Hook_WeaponSwitch, false);
				break;
			case SDKHook_ShouldCollide:
				SH_REMOVE_MANUALHOOK_MEMFUNC(ShouldCollide, pEnt, &g_Interface, &SDKHooks::Hook_ShouldCollide, false);
				break;
			default:
				return;
		}
	}
	g_HookList.Remove(index);
}


/**
 * IEntityFactoryDictionary, IServerGameDLL & IVEngineServer Hook Handlers
 */
IServerNetworkable *SDKHooks::Hook_Create(const char *pClassName)
{
	IServerNetworkable *pNet = META_RESULT_ORIG_RET(IServerNetworkable *);
	if(!pNet)
		RETURN_META_VALUE(MRES_IGNORED, NULL);

	// Get entity edict and CBaseEntity
	edict_t *pEdict = pNet->GetEdict();
	CBaseEntity *pEnt = gameents->EdictToBaseEntity(pEdict);

	// Add UpdateOnRemove hook
	if(pEnt)
		SH_ADD_MANUALHOOK_MEMFUNC(UpdateOnRemove, pEnt, &g_Interface, &SDKHooks::Hook_UpdateOnRemove, false);

	// Call OnEntityCreated forward
	g_pOnEntityCreated->PushCell(IndexOfEdict(pEdict));
	g_pOnEntityCreated->PushString(pClassName);
	g_pOnEntityCreated->Execute(NULL);

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}

const char *SDKHooks::Hook_GetGameDescription()
{
	static char szGameDesc[64];
	cell_t result = Pl_Continue;

	// Call OnGetGameDescription forward
	g_pOnGetGameNameDescription->PushStringEx(szGameDesc, sizeof(szGameDesc), SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	g_pOnGetGameNameDescription->Execute(&result);

	if(result == Pl_Changed)
		RETURN_META_VALUE(MRES_SUPERCEDE, szGameDesc);

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}

const char *SDKHooks::Hook_GetMapEntitiesString()
{
	if(g_szMapEntities[0])
		RETURN_META_VALUE(MRES_SUPERCEDE, g_szMapEntities);

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}

bool SDKHooks::Hook_LevelInit(char const *pMapName, char const *pMapEntities, char const *pOldLevel, char const *pLandmarkName, bool loadGame, bool background)
{
	strcpy(g_szMapEntities, pMapEntities);
	cell_t result = Pl_Continue;

	// Call OnLevelInit forward
	g_pOnLevelInit->PushString(pMapName);
	g_pOnLevelInit->PushStringEx(g_szMapEntities, sizeof(g_szMapEntities), SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	g_pOnLevelInit->Execute(&result);

	if(result >= Pl_Handled)
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	if(result == Pl_Changed)
		RETURN_META_VALUE_NEWPARAMS(MRES_HANDLED, true, &IServerGameDLL::LevelInit, (pMapName, g_szMapEntities, pOldLevel, pLandmarkName, loadGame, background));

	RETURN_META_VALUE(MRES_IGNORED, true);
}


/**
 * CBaseEntity Hook Handlers
 */
void SDKHooks::Hook_EndTouch(CBaseEntity *pOther)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int other = IndexOfEdict(gameents->BaseEntityToEdict(pOther));

	Call(entity, SDKHook_EndTouch, other);
}

void SDKHooks::Hook_FireBulletsPost(const FireBulletsInfo_t &info)
{
	edict_t *pEdict = gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity));
	int entity = IndexOfEdict(pEdict);

	IGamePlayer *pPlayer = playerhelpers->GetGamePlayer(pEdict);
	if(!pPlayer)
		RETURN_META(MRES_IGNORED);

	IPlayerInfo *pInfo = pPlayer->GetPlayerInfo();
	if(!pInfo)
		RETURN_META(MRES_IGNORED);

	const char *weapon = pInfo->GetWeaponName();
	IPluginFunction *callback = NULL;

	HOOKLOOP
	{
		if(g_HookList[i].entity != entity || g_HookList[i].type != SDKHook_FireBulletsPost)
			continue;

		callback = g_HookList[i].callback;
		callback->PushCell(entity);
		callback->PushCell(info.m_iShots);
		callback->PushString(weapon?weapon:"");
		callback->Execute(NULL);
	}

	RETURN_META(MRES_IGNORED);
}

int SDKHooks::Hook_OnTakeDamage(CTakeDamageInfoHack &info)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int attacker = info.GetAttacker();
	int inflictor = info.GetInflictor();
	float damage = info.GetDamage();
	int damagetype = info.GetDamageType();
	IPluginFunction *callback = NULL;
	cell_t res, ret = Pl_Continue;

	HOOKLOOP
	{
		if(g_HookList[i].entity != entity || g_HookList[i].type != SDKHook_OnTakeDamage)
			continue;

		callback = g_HookList[i].callback;
		callback->PushCell(entity);
		callback->PushCellByRef(&attacker);
		callback->PushCellByRef(&inflictor);
		callback->PushFloatByRef(&damage);
		callback->PushCellByRef(&damagetype);
		callback->Execute(&res);

		if(res > ret)
			ret = res;
	}

	if(ret >= Pl_Handled)
		RETURN_META_VALUE(MRES_SUPERCEDE, 1);

	if(ret == Pl_Changed)
	{
		CBaseEntity *pEntAttacker = gameents->EdictToBaseEntity(PEntityOfEntIndex(attacker));
		if(!pEntAttacker)
		{
			callback->GetParentContext()->ThrowNativeError("Entity %d for attacker is invalid", attacker);
			RETURN_META_VALUE(MRES_IGNORED, 0);
		}
		CBaseEntity *pEntInflictor = gameents->EdictToBaseEntity(PEntityOfEntIndex(inflictor));
		if(!pEntInflictor)
		{
			callback->GetParentContext()->ThrowNativeError("Entity %d for inflictor is invalid", inflictor);
			RETURN_META_VALUE(MRES_IGNORED, 0);
		}

		info.SetAttacker(pEntAttacker);
		info.SetInflictor(pEntInflictor);
		info.SetDamage(damage);
		info.SetDamageType(damagetype);

		RETURN_META_VALUE(MRES_HANDLED, 1); 
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

int SDKHooks::Hook_OnTakeDamagePost(CTakeDamageInfoHack &info)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	IPluginFunction *callback = NULL;

	HOOKLOOP
	{
		if(g_HookList[i].entity != entity || g_HookList[i].type != SDKHook_OnTakeDamagePost)
			continue;

		callback = g_HookList[i].callback;
		callback->PushCell(entity);
		callback->PushCell(info.GetAttacker());
		callback->PushCell(info.GetInflictor());
		callback->PushFloat(info.GetDamage());
		callback->PushCell(info.GetDamageType());
		callback->Execute(NULL);
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void SDKHooks::Hook_PreThink()
{
	int client = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));

	Call(client, SDKHook_PreThink);
}

void SDKHooks::Hook_PreThinkPost()
{
	int client = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));

	Call(client, SDKHook_PreThinkPost);
}

void SDKHooks::Hook_PostThink()
{
	int client = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));

	Call(client, SDKHook_PostThink);
}

void SDKHooks::Hook_PostThinkPost()
{
	int client = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));

	Call(client, SDKHook_PostThinkPost);
}

void SDKHooks::Hook_SetTransmit(CCheckTransmitInfo *pInfo, bool bAlways)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int client = IndexOfEdict(pInfo->m_pClientEnt);
	cell_t result = Call(entity, SDKHook_SetTransmit, client);

	if(result >= Pl_Handled)
		RETURN_META(MRES_SUPERCEDE);

	RETURN_META(MRES_IGNORED);
}

bool SDKHooks::Hook_ShouldCollide(int collisionGroup, int contentsMask)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	IPluginFunction *callback = NULL;
	cell_t res;
	int returnValue = 0;

	HOOKLOOP
	{
		if (g_HookList[i].entity != entity || g_HookList[i].type != SDKHook_ShouldCollide)
			continue;

		callback = g_HookList[i].callback;
		callback->PushCell(entity);
		callback->PushCellByRef(&collisionGroup);
		callback->PushCellByRef(&contentsMask);
		callback->PushCellByRef(&returnValue);
		callback->Execute(&res);
	}

	if (res >= Pl_Changed)
		RETURN_META_VALUE(MRES_SUPERCEDE, (bool)returnValue);

	RETURN_META_VALUE(MRES_IGNORED, true);
}

void SDKHooks::Hook_Spawn()
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));

	Call(entity, SDKHook_Spawn);
}

void SDKHooks::Hook_StartTouch(CBaseEntity *pOther)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int other = IndexOfEdict(gameents->BaseEntityToEdict(pOther));

	Call(entity, SDKHook_StartTouch, other);
}

void SDKHooks::Hook_Think()
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));

	Call(entity, SDKHook_Think);
}

void SDKHooks::Hook_ThinkPost()
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));

	Call(entity, SDKHook_ThinkPost);
}

void SDKHooks::Hook_Touch(CBaseEntity *pOther)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int other = IndexOfEdict(gameents->BaseEntityToEdict(pOther));

	Call(entity, SDKHook_Touch, other);
}

void SDKHooks::Hook_TraceAttack(CTakeDamageInfoHack &info, const Vector &vecDir, trace_t *ptr)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int attacker = info.GetAttacker();
	int inflictor = info.GetInflictor();
	float damage = info.GetDamage();
	int damagetype = info.GetDamageType();
	int ammotype = info.GetAmmoType();
	IPluginFunction *callback = NULL;
	cell_t res, ret = Pl_Continue;

	HOOKLOOP
	{
		if(g_HookList[i].entity != entity || g_HookList[i].type != SDKHook_TraceAttack)
			continue;

		callback = g_HookList[i].callback;
		callback->PushCell(entity);
		callback->PushCellByRef(&attacker);
		callback->PushCellByRef(&inflictor);
		callback->PushFloatByRef(&damage);
		callback->PushCellByRef(&damagetype);
		callback->PushCellByRef(&ammotype);
		callback->PushCell(ptr->hitbox);
		callback->PushCell(ptr->hitgroup);
		callback->Execute(&res);

		if(res > ret)
			ret = res;
	}

	if(ret >= Pl_Handled)
		RETURN_META(MRES_SUPERCEDE);

	if(ret == Pl_Changed)
	{
		CBaseEntity *pEntAttacker = gameents->EdictToBaseEntity(PEntityOfEntIndex(attacker));
		if(!pEntAttacker)
		{
			callback->GetParentContext()->ThrowNativeError("Entity %d for attacker is invalid", attacker);
			RETURN_META(MRES_IGNORED);
		}
		CBaseEntity *pEntInflictor = gameents->EdictToBaseEntity(PEntityOfEntIndex(inflictor));
		if(!pEntInflictor)
		{
			callback->GetParentContext()->ThrowNativeError("Entity %d for inflictor is invalid", inflictor);
			RETURN_META(MRES_IGNORED);
		}
		
		info.SetAttacker(gameents->EdictToBaseEntity(PEntityOfEntIndex(attacker)));
		info.SetInflictor(gameents->EdictToBaseEntity(PEntityOfEntIndex(inflictor)));
		info.SetDamage(damage);
		info.SetDamageType(damagetype);
		info.SetAmmoType(ammotype);

		RETURN_META(MRES_HANDLED); 
	}

	RETURN_META(MRES_IGNORED);
}

void SDKHooks::Hook_TraceAttackPost(CTakeDamageInfoHack &info, const Vector &vecDir, trace_t *ptr)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	IPluginFunction *callback = NULL;

	HOOKLOOP
	{
		if(g_HookList[i].entity != entity || g_HookList[i].type != SDKHook_TraceAttackPost)
			continue;

		callback = g_HookList[i].callback;
		callback->PushCell(entity);
		callback->PushCell(info.GetAttacker());
		callback->PushCell(info.GetInflictor());
		callback->PushFloat(info.GetDamage());
		callback->PushCell(info.GetDamageType());
		callback->PushCell(info.GetAmmoType());
		callback->PushCell(ptr->hitbox);
		callback->PushCell(ptr->hitgroup);
		callback->Execute(NULL);
	}

	RETURN_META(MRES_IGNORED);
}

void SDKHooks::Hook_UpdateOnRemove()
{
	// Get entity CBaseEntity and index
	CBaseEntity *pEnt = META_IFACEPTR(CBaseEntity);
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(pEnt));

	// Call OnEntityDestroyed forward
	g_pOnEntityDestroyed->PushCell(entity);
	g_pOnEntityDestroyed->Execute(NULL);
	
	SDKHooks::RemoveEntityHooks(pEnt);

	RETURN_META(MRES_IGNORED);
}

bool SDKHooks::Hook_WeaponCanSwitchTo(CBaseCombatWeapon *pWeapon)
{
	int client = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int weapon = IndexOfEdict(gameents->BaseEntityToEdict(pWeapon));
	cell_t result = Call(client, SDKHook_WeaponCanSwitchTo, weapon);

	if(result >= Pl_Handled)
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	RETURN_META_VALUE(MRES_IGNORED, true);
}

bool SDKHooks::Hook_WeaponCanUse(CBaseCombatWeapon *pWeapon)
{
	int client = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int weapon = IndexOfEdict(gameents->BaseEntityToEdict(pWeapon));
	cell_t result = Call(client, SDKHook_WeaponCanUse, weapon);

	if(result >= Pl_Handled)
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	RETURN_META_VALUE(MRES_IGNORED, true);
}

void SDKHooks::Hook_WeaponDrop(CBaseCombatWeapon *pWeapon, const Vector *pvecTarget, const Vector *pVelocity)
{
	int client = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int weapon = IndexOfEdict(gameents->BaseEntityToEdict(pWeapon));
	cell_t result = Call(client, SDKHook_WeaponDrop, weapon);

	if(result >= Pl_Handled)
		RETURN_META(MRES_SUPERCEDE);

	RETURN_META(MRES_IGNORED);
}

void SDKHooks::Hook_WeaponEquip(CBaseCombatWeapon *pWeapon)
{
	int client = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int weapon = IndexOfEdict(gameents->BaseEntityToEdict(pWeapon));
	cell_t result = Call(client, SDKHook_WeaponEquip, weapon);

	if(result >= Pl_Handled)
		RETURN_META(MRES_SUPERCEDE);

	RETURN_META(MRES_IGNORED);
}

bool SDKHooks::Hook_WeaponSwitch(CBaseCombatWeapon *pWeapon, int viewmodelindex)
{
	int client = IndexOfEdict(gameents->BaseEntityToEdict(META_IFACEPTR(CBaseEntity)));
	int weapon = IndexOfEdict(gameents->BaseEntityToEdict(pWeapon));
	cell_t result = Call(client, SDKHook_WeaponSwitch, weapon);

	if(result >= Pl_Handled)
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	RETURN_META_VALUE(MRES_IGNORED, true);
}


void SDKHooks::RemoveEntityHooks(CBaseEntity *pEnt)
{
	int entity = IndexOfEdict(gameents->BaseEntityToEdict(pEnt));
	
	// Remove hooks
	HOOKLOOP
	{
		if(g_HookList[i].entity == entity)
		{
#ifdef SDKHOOKSDEBUG
			META_CONPRINTF("DEBUG: Removing hook #%d on entity %d (UpdateOnRemove or clientdisconnect)\n", i, entity);
#endif
			Unhook(i);
		}
	}

	// Remove UpdateOnRemove hook
	SH_REMOVE_MANUALHOOK_MEMFUNC(UpdateOnRemove, pEnt, &g_Interface, &SDKHooks::Hook_UpdateOnRemove, false);	
}

CON_COMMAND(sdkhooks_listhooks, "Lists all current hooks")
{
	int iHookCount = g_HookList.Count();
	META_CONPRINT("SDK Hooks\n");
	META_CONPRINT("---------------------\n");
	if (iHookCount == 0)
	{
		META_CONPRINT("(No active hooks)\n");
		return;
	}
	IPlugin *pPlugin;
	for (int i = 0; i < iHookCount; i++)
	{
		g_HookList[i].callback->GetParentRuntime()->GetDefaultContext()->GetKey(2, (void **)&pPlugin);
		META_CONPRINTF("[%d] %s\tType: %s\tEntity: %d\n", i+1, pPlugin->GetFilename(), g_szHookNames[g_HookList[i].type], g_HookList[i].entity);
	}
}
