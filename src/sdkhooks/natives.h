#ifndef _INCLUDE_SOURCEMOD_NATIVES_PROPER_H_
#define _INCLUDE_SOURCEMOD_NATIVES_PROPER_H_

cell_t Native_Hook(IPluginContext *pContext, const cell_t *params);
cell_t Native_HookEx(IPluginContext *pContext, const cell_t *params);
cell_t Native_Unhook(IPluginContext *pContext, const cell_t *params);

const sp_nativeinfo_t g_Natives[] = 
{
	{"SDKHook",				Native_Hook},
	{"SDKHookEx",			Native_HookEx},
	{"SDKUnhook",			Native_Unhook},
	{NULL,						NULL},
};

extern SDKHooks g_Interface;
extern IServerGameEnts *gameents;

#endif // _INCLUDE_SOURCEMOD_NATIVES_PROPER_H_
