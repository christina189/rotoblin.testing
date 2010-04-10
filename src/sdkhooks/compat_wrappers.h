#ifndef _INCLUDE_SOURCEMOD_COMPAT_WRAPPERS_H_
#define _INCLUDE_SOURCEMOD_COMPAT_WRAPPERS_H_

#if SOURCE_ENGINE >= SE_LEFT4DEAD
inline int IndexOfEdict(const edict_t *pEdict)
{
	return (int)(pEdict - g_pGlobals->baseEdict);
}
inline edict_t *PEntityOfEntIndex(int iEntIndex)
{
	if(iEntIndex >= 0 && iEntIndex < g_pGlobals->maxEntities)
		return (edict_t *)(g_pGlobals->baseEdict + iEntIndex);

	return NULL;
}
#else
inline int IndexOfEdict(const edict_t *pEdict)
{
	return engine->IndexOfEdict(pEdict);
}
inline edict_t *PEntityOfEntIndex(int iEntIndex)
{
	return engine->PEntityOfEntIndex(iEntIndex);
}
#endif

#endif //_INCLUDE_SOURCEMOD_COMPAT_WRAPPERS_H_
