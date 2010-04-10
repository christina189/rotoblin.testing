#ifndef _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

#define GAME_DLL 1

#include "smsdk_ext.h"
#include <IBinTools.h>
#include "basecombatweapon_shared.h"
#include "takedamageinfo.h"

#ifndef METAMOD_PLAPI_VERSION
#define GetCGlobals pGlobals
#define GetEngineFactory engineFactory
#define GetServerFactory serverFactory
#endif


/**
 * Globals
 */
enum SDKHookType
{
	SDKHook_EndTouch,
	SDKHook_FireBulletsPost,
	SDKHook_OnTakeDamage,
	SDKHook_OnTakeDamagePost,
	SDKHook_PreThink,
	SDKHook_PostThink,
	SDKHook_SetTransmit,
	SDKHook_Spawn,
	SDKHook_StartTouch,
	SDKHook_Think,
	SDKHook_Touch,
	SDKHook_TraceAttack,
	SDKHook_TraceAttackPost,
	SDKHook_WeaponCanSwitchTo,
	SDKHook_WeaponCanUse,
	SDKHook_WeaponDrop,
	SDKHook_WeaponEquip,
	SDKHook_WeaponSwitch,
	SDKHook_ShouldCollide,
	SDKHook_PreThinkPost,
	SDKHook_PostThinkPost,
	SDKHook_ThinkPost,
	SDKHook_MAXHOOKS
};

const char g_szHookNames[SDKHook_MAXHOOKS][18] = {
	"EndTouch",
	"FireBulletsPost)",
	"OnTakeDamage",
	"OnTakeDamagePost",
	"PreThink",
	"PostThink",
	"SetTransmit",
	"Spawn",
	"StartTouch",
	"Think",
	"Touch",
	"TraceAttack",
	"TraceAttackPost",
	"WeaponCanSwitchTo",
	"WeaponCanUse",
	"WeaponDrop",
	"WeaponEquip",
	"WeaponSwitch",
	"ShouldCollide",
	"PreThinkPost",
	"PostThinkPost",
	"ThinkPost"
};

enum HookReturn
{
	HookRet_Successful,
	HookRet_InvalidEntity,
	HookRet_InvalidHookType,
	HookRet_NotSupported
};

/**
 * Classes
 */
class CTakeDamageInfoHack : public CTakeDamageInfo
{
public:
	inline int GetAttacker() { return CTakeDamageInfo::m_hAttacker.GetEntryIndex(); }
	inline int GetInflictor() { return CTakeDamageInfo::m_hInflictor.GetEntryIndex(); }
};

class HookList
{
public:
	int entity;
	SDKHookType type;
	IPluginFunction *callback;
};

class SDKHooks :
	public SDKExtension,
	public IClientListener,
	public IPluginsListener
{
public:
	/**
	 * @brief This is called after the initial loading sequence has been processed.
	 *
	 * @param error		Error message buffer.
	 * @param maxlength	Size of error message buffer.
	 * @param late		Whether or not the module was loaded after map load.
	 * @return			True to succeed loading, false to fail.
	 */
	virtual bool SDK_OnLoad(char *error, size_t maxlength, bool late);
	
	/**
	 * @brief This is called right before the extension is unloaded.
	 */
	virtual void SDK_OnUnload();

	/**
	 * @brief This is called once all known extensions have been loaded.
	 * Note: It is is a good idea to add natives here, if any are provided.
	 */
	virtual void SDK_OnAllLoaded();

	/**
	 * @brief Called when the pause state is changed.
	 */
	//virtual void SDK_OnPauseChange(bool paused);

	/**
	 * @brief this is called when Core wants to know if your extension is working.
	 *
	 * @param error		Error message buffer.
	 * @param maxlength	Size of error message buffer.
	 * @return			True if working, false otherwise.
	 */
	//virtual bool QueryRunning(char *error, size_t maxlength);
public:
#if defined SMEXT_CONF_METAMOD
	/**
	 * @brief Called when Metamod is attached, before the extension version is called.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @param late			Whether or not Metamod considers this a late load.
	 * @return				True to succeed, false to fail.
	 */
	virtual bool SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlength, bool late);

	/**
	 * @brief Called when Metamod is detaching, after the extension version is called.
	 * NOTE: By default this is blocked unless sent from SourceMod.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @return				True to succeed, false to fail.
	 */
	//virtual bool SDK_OnMetamodUnload(char *error, size_t maxlength);

	/**
	 * @brief Called when Metamod's pause state is changing.
	 * NOTE: By default this is blocked unless sent from SourceMod.
	 *
	 * @param paused		Pause state being set.
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @return				True to succeed, false to fail.
	 */
	//virtual bool SDK_OnMetamodPauseChange(bool paused, char *error, size_t maxlength);
#endif
	virtual void OnPluginUnloaded(IPlugin *plugin);
	virtual void OnClientDisconnecting(int client);
public:
	/**
	 * Functions
	 */
	cell_t Call(int entity, SDKHookType type, int other);
	void SetupHooks();

	HookReturn Hook(int entity, SDKHookType type, IPluginFunction *callback);
	void Unhook(int index);

	/**
	 * IEntityFactoryDictionary, IServerGameDLL & IVEngineServer Hook Handlers
	 */
	IServerNetworkable *Hook_Create(const char *pClassName);
	const char *Hook_GetGameDescription();
	const char *Hook_GetMapEntitiesString();
	bool Hook_LevelInit(char const *pMapName, char const *pMapEntities, char const *pOldLevel, char const *pLandmarkName, bool loadGame, bool background);

	/**
	 * CBaseEntity Hook Handlers
	 */
	void Hook_EndTouch(CBaseEntity *pOther);
	void Hook_FireBulletsPost(const FireBulletsInfo_t &info);
	int Hook_OnTakeDamage(CTakeDamageInfoHack &info);
	int Hook_OnTakeDamagePost(CTakeDamageInfoHack &info);
	void Hook_PreThink();
	void Hook_PreThinkPost();
	void Hook_PostThink();
	void Hook_PostThinkPost();
	void Hook_SetTransmit(CCheckTransmitInfo *pInfo, bool bAlways);
	void Hook_Spawn();
	void Hook_StartTouch(CBaseEntity *pOther);
	void Hook_Think();
	void Hook_ThinkPost();
	void Hook_Touch(CBaseEntity *pOther);
	void Hook_TraceAttack(CTakeDamageInfoHack &info, const Vector &vecDir, trace_t *ptr);
	void Hook_TraceAttackPost(CTakeDamageInfoHack &info, const Vector &vecDir, trace_t *ptr);
	void Hook_UpdateOnRemove();
	bool Hook_WeaponCanSwitchTo(CBaseCombatWeapon *pWeapon);
	bool Hook_WeaponCanUse(CBaseCombatWeapon *pWeapon);
	void Hook_WeaponDrop(CBaseCombatWeapon *pWeapon, const Vector *pvecTarget, const Vector *pVelocity);
	void Hook_WeaponEquip(CBaseCombatWeapon *pWeapon);
	bool Hook_WeaponSwitch(CBaseCombatWeapon *pWeapon, int viewmodelindex);
	bool Hook_ShouldCollide(int collisonGroup, int contentsMask);
	
private:
	void RemoveEntityHooks(CBaseEntity *pEnt);
};

extern CGlobalVars *g_pGlobals;
extern CUtlVector<HookList> g_HookList;

#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
