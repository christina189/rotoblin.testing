/*
 * ============================================================================
 *
 *  File:			limithuntingrifle
 *  Type:			Module
 *  Description:	Adds a limit to hunting rifles for the survivors.
 *
 *  Copyright (C) 2010  Mr. Zero <mrzerodk@gmail.com
 *  This file is part of Rotoblin.
 *
 *  Rotoblin is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Rotoblin is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Rotoblin.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ============================================================================
 */

/*
 * ==================================================
 *                     Variables
 * ==================================================
 */

/*
 * --------------------
 *       Private
 * --------------------
 */

static	const	String:	WEAPON_HUNTING_RIFLE[]			= "weapon_hunting_rifle";

static			Handle:	g_hLimitHuntingRifle_Cvar 		= INVALID_HANDLE;
static					g_iLimitHuntingRifle			= 1;

static	const	Float:	TIP_TIMEOUT						= 8.0;
static			bool:	g_bHaveTipped[MAXPLAYERS + 1] 	= {false};

/*
 * ==================================================
 *                     Forwards
 * ==================================================
 */

/**
 * Called on plugin start.
 *
 * @noreturn
 */
public _LimitHuntingRifl_OnPluginStart()
{
	g_hLimitHuntingRifle_Cvar = CreateConVarEx("limit_huntingrifle", "1", "Maximum of hunting rifles the survivors can pick up.", FCVAR_NOTIFY | FCVAR_PLUGIN);
	if (g_hLimitHuntingRifle_Cvar == INVALID_HANDLE) ThrowError("Unable to create limit hunting rifle cvar!");
	AddConVarToReport(g_hLimitHuntingRifle_Cvar); // Add to report status module

	HookPublicEvent(EVENT_ONPLUGINENABLE, _LHR_OnPluginEnabled);
	HookPublicEvent(EVENT_ONPLUGINDISABLE, _LHR_OnPluginDisabled);
}

/**
 * Called on plugin enabled.
 *
 * @noreturn
 */
public _LHR_OnPluginEnabled()
{
	g_iLimitHuntingRifle = GetConVarInt(g_hLimitHuntingRifle_Cvar);
	HookConVarChange(g_hLimitHuntingRifle_Cvar, _LHR_Limit_CvarChange);

	HookPublicEvent(EVENT_ONCLIENTPUTINSERVER, _LHR_OnClientPutInServer);
	HookPublicEvent(EVENT_ONCLIENTDISCONNECT_POST, _LHR_OnClientDisconnect);

	for (new client = 1; client <= MaxClients; client++)
	{
		if (!IsClientInGame(client)) continue;
		SDKHook(client, SDKHook_WeaponCanUse, _LHR_OnWeaponCanUse);
	}
}

/**
 * Called on plugin disabled.
 *
 * @noreturn
 */
public _LHR_OnPluginDisabled()
{
	UnhookConVarChange(g_hLimitHuntingRifle_Cvar, _LHR_Limit_CvarChange);
	
	UnhookPublicEvent(EVENT_ONCLIENTPUTINSERVER, _PS_OnClientPutInServer);
	UnhookPublicEvent(EVENT_ONCLIENTDISCONNECT_POST, _LHR_OnClientDisconnect);

	for (new client = 1; client <= MaxClients; client++)
	{
		if (!IsClientInGame(client)) continue;
		SDKUnhook(client, SDKHook_WeaponCanUse, _LHR_OnWeaponCanUse);
	}
}

/**
 * Called on limit hunting rifle cvar changed.
 *
 * @param convar		Handle to the convar that was changed.
 * @param oldValue		String containing the value of the convar before it was changed.
 * @param newValue		String containing the new value of the convar.
 * @noreturn
 */
public _LHR_Limit_CvarChange(Handle:convar, const String:oldValue[], const String:newValue[])
{
	g_iLimitHuntingRifle = StringToInt(newValue);
}

/**
 * Called on client put in server.
 *
 * @param client		Client index.
 * @noreturn
 */
public _LHR_OnClientPutInServer(client)
{
	SDKHook(client, SDKHook_WeaponCanUse, _LHR_OnWeaponCanUse);
}

/**
 * Called on client disconnect.
 *
 * @param client		Client index.
 * @noreturn
 */
public _LHR_OnClientDisconnect(client)
{
	SDKUnhook(client, SDKHook_WeaponCanUse, _LHR_OnWeaponCanUse);
}

/**
 * Called on weapon can use.
 *
 * @param client		Client index.
 * @param weapon		Weapon entity index.
 * @return				Plugin_Continue to allow weapon usage, Plugin_Handled
 *						to disallow weapon usage.
 */
public Action:_LHR_OnWeaponCanUse(client, weapon)
{
	if (GetClientTeam(client) != TEAM_SURVIVOR) return Plugin_Continue;

	decl String:classname[128];
	GetEdictClassname(weapon, classname, sizeof(classname));
	if (!StrEqual(classname, WEAPON_HUNTING_RIFLE)) return Plugin_Continue;

	new curWeapon = GetPlayerWeaponSlot(client, 0); // Get current primary weapon
	if (curWeapon != -1 && IsValidEntity(curWeapon))
	{
		GetEdictClassname(curWeapon, classname, sizeof(classname));
		if (StrEqual(classname, WEAPON_HUNTING_RIFLE))
		{
			return Plugin_Continue; // Survivor already got a hunting rifle and trying to pick up a ammo refill, allow it
		}
	}

	if (GetActiveHuntingRifles() >= g_iLimitHuntingRifle) // If ammount of active hunting rifles are at the limit
	{
		if (!IsFakeClient(client) && !g_bHaveTipped[client])
		{
			g_bHaveTipped[client] = true;
			if (g_iLimitHuntingRifle > 0)
			{
				PrintToChat(client, "[%s] Your team has already picked up the maximum amount of hunting rifles.", PLUGIN_TAG);
			}
			else
			{
				PrintToChat(client, "[%s] Hunting rifle is not allowed.", PLUGIN_TAG);
			}
			CreateTimer(TIP_TIMEOUT, _LHR_Tip_Timer, client);
		}
		return Plugin_Handled; // Dont allow survivor picking up the hunting rifle
	}

	return Plugin_Continue;
}

public Action:_LHR_Tip_Timer(Handle:timer, any:client)
{
	g_bHaveTipped[client] = false;
	return Plugin_Stop;
}

/*
 * ==================================================
 *                    Private API
 * ==================================================
 */

static GetActiveHuntingRifles()
{
	new weapon;
	decl String:classname[128];
	new count;
	for (new client = FIRST_CLIENT; client <= MaxClients; client++)
	{
		if (!IsClientInGame(client) || GetClientTeam(client) != TEAM_SURVIVOR || !IsPlayerAlive(client)) continue;
		weapon = GetPlayerWeaponSlot(client, 0); // Get primary weapon
		if (weapon == -1 || !IsValidEntity(weapon)) continue;

		GetEdictClassname(weapon, classname, sizeof(classname));
		if (!StrEqual(classname, WEAPON_HUNTING_RIFLE)) continue;
		count++;
	}
	return count;
}