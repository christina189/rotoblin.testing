#include "extension.h"
#include "natives.h"

cell_t Native_Hook(IPluginContext *pContext, const cell_t *params)
{
	int entity = (int)params[1];
	SDKHookType type = (SDKHookType)params[2];
	IPluginFunction *callback = pContext->GetFunctionById(params[3]);
	HookReturn ret = g_Interface.Hook(entity, type, callback);
	switch (ret)
	{
		case HookRet_InvalidEntity:
			pContext->ThrowNativeError("Entity %d is invalid", entity);
			break;
		case HookRet_InvalidHookType:
			pContext->ThrowNativeError("Invalid hook type specified");
			break;
		case HookRet_NotSupported:
			pContext->ThrowNativeError("Hook type not supported on this game");
			break;
	}

	return true;
}

cell_t Native_HookEx(IPluginContext *pContext, const cell_t *params)
{
	int entity = (int)params[1];
	SDKHookType type = (SDKHookType)params[2];
	IPluginFunction *callback = pContext->GetFunctionById(params[3]);
	HookReturn ret = g_Interface.Hook(entity, type, callback);
	if (ret == HookRet_Successful)
		return true;

	return false;
}

cell_t Native_Unhook(IPluginContext *pContext, const cell_t *params)
{
	int entity = (int)params[1];
	SDKHookType type = (SDKHookType)params[2];
	IPluginFunction *callback = pContext->GetFunctionById(params[3]);

	for(int i = g_HookList.Count() - 1; i >= 0; i--)
	{
		if(g_HookList[i].entity == entity && g_HookList[i].type == type && g_HookList[i].callback == callback)
			g_Interface.Unhook(i);
	}

	return true;
}
