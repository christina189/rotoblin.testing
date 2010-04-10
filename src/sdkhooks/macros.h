#define CHECKOFFSET(gamedataname) \
	offset = 0; \
	g_pGameConf->GetOffset(gamedataname, &offset); \
	if (offset > 0)

#define HOOKLOOP \
	for(int i = g_HookList.Count() - 1; i >= 0; i--)
