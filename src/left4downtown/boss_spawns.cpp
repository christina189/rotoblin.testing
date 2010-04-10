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
#include "boss_spawns.h"
#include "asm/asm.h"
#include "CDetour/detourhelpers.h"

#define MFP_CODE_ADDRESS(mfp) (*(void **)(&(mfp)))

IGameConfig *BossSpawns::_gameconf = NULL;
ISourcePawnEngine *BossSpawns::_spengine = NULL;

BossSpawns::Detours::FunctionType BossSpawns::Detours::SpawnTankTrampoline;
BossSpawns::Detours::FunctionType BossSpawns::Detours::SpawnWitchTrampoline;

//insert a specific JMP instruction at the given location, save it to the buffer
void inject_jmp(void *buffer, void* src, void* dest) {
	*(unsigned char*)buffer = OP_JMP;
	*(long*)((unsigned char*)buffer+1) = (long)((unsigned char*)dest - ((unsigned char*)src + OP_JMP_SIZE));
}


void BossSpawns::Init(ISourcePawnEngine *spengine, IGameConfig *gameconf)
{
	_spengine = spengine;
	_gameconf = gameconf;
}

void *BossSpawns::Detours::SpawnTank(void *vector, void *qangle)
{
	//does not get called if you do z_spawn tank and the player is in ghost mode..
	//because it simply calls PlayerTransitionState or something like that
	//anyway not a problem since director spawns are always using ZombieManager afaik
	L4D_DEBUG_LOG("BossSpawns - SpawnTank has been called");

	return (this->*SpawnTankTrampoline)(vector, qangle);
}

BossSpawns::BossSpawns() 
{
	_isPatched = false;
}

BossSpawns::~BossSpawns()
{
	Unpatch();
}

void BossSpawns::Patch()
{
	if(_isPatched)
	{
		return;
	}

	void *(Detours::*SpawnTankDetour)(void *terrorNavArea, void *qangle);
	SpawnTankDetour = &Detours::SpawnTank;

	L4D_DEBUG_LOG("Spawn tank detour address = %p", SpawnTankDetour);
	L4D_DEBUG_LOG("Spawn tank detour converted = %p", MFP_CODE_ADDRESS(SpawnTankDetour));

	//PatchFromSignature("SpawnWitch", MFP_CODE_ADDRESS(Detours::SpawnWitch), MFP_CODE_ADDRESS(SpawnWitchTrampoline), SpawnWitchSignature);
	unsigned char *SpawnTankSignature;
	PatchFromSignature("SpawnTank",  MFP_CODE_ADDRESS(SpawnTankDetour), MFP_CODE_ADDRESS(_detours.SpawnTankTrampoline), SpawnTankSignature);
	
	_isPatched = true;
}

//TODO: remove this by putting things in its own class
static patch_t detourRestore;
static void *detourSignature;
static unsigned char *trampoline;

void BossSpawns::PatchFromSignature(const char *signatureName, void *targetFunction, void *&originalFunction, unsigned char *&signature)
{
	if (!_gameconf->GetMemSig(signatureName, (void**)&signature) || !signature) 
	{ 
		g_pSM->LogError(myself, "BossSpawns -- Could not find '%s' signature", signatureName);
		return;
	} 
	detourSignature = signature;

	// create the jmp to our detour function
	patch_t detourJmpPatch;
	detourJmpPatch.bytes = OP_JMP_SIZE;
	inject_jmp(detourJmpPatch.patch, signature, targetFunction);

	//copy the original func's first few bytes into the trampoline
	int copiedBytes = copy_bytes(/*src*/signature, /*dest*/NULL, OP_JMP_SIZE);
	L4D_DEBUG_LOG("BossSpawns -- Will be copying %d bytes to trampoline", copiedBytes);

	trampoline = (unsigned char*) _spengine->AllocatePageMemory(copiedBytes + OP_JMP_SIZE);
	memcpy(/*dst*/trampoline, /*src*/signature, copiedBytes);

	//at end of trampoline, place jmp back to resume spot of the original func
	inject_jmp(/*src*/trampoline + copiedBytes, /*dest*/signature + copiedBytes);

	//NOTE: above will 'break' if there is any JMP that goes into the first copiedBytes anywhere else :(
	ApplyPatch(signature, /*offset*/0, &detourJmpPatch, &detourRestore);

	originalFunction = trampoline;

	L4D_DEBUG_LOG("BossSpawns has been patched for signature %s", signatureName);
}

void BossSpawns::Unpatch()
{
	if(!_isPatched)
	{
		return;
	}

	ApplyPatch(detourSignature, /*offset*/0, &detourRestore, /*restore*/NULL);
	_spengine->FreePageMemory(trampoline);

	_isPatched = false;

	L4D_DEBUG_LOG("BossSpawns has been unpatched");
}
