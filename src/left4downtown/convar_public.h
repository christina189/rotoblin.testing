//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $NoKeywords: $
//===========================================================================//

#ifndef CONVAR_PUBLIC_H
#define CONVAR_PUBLIC_H
//-----------------------------------------------------------------------------
// Purpose: A console variable, with all its class fields public
//-----------------------------------------------------------------------------
class ConVarPublic : public ConCommandBase, public IConVar
{
friend class CCvar;
friend class ConVarRef;

public:
	typedef ConCommandBase BaseClass;



private:


public:

	// This either points to "this" or it points to the original declaration of a ConVar.
	// This allows ConVars to exist in separate modules, and they all use the first one to be declared.
	// m_pParent->m_pParent must equal m_pParent (ie: m_pParent must be the root, or original, ConVar).
	ConVarPublic						*m_pParent;

	// Static data
	const char					*m_pszDefaultValue;
	
	// Value
	// Dynamically allocated
	char						*m_pszString;
	int							m_StringLength;

	// Values
	float						m_fValue;
	int							m_nValue;

	// Min/Max values
	bool						m_bHasMin;
	float						m_fMinVal;
	bool						m_bHasMax;
	float						m_fMaxVal;
	
	// Call this function when ConVar changes
	FnChangeCallback_t			m_fnChangeCallback;
};

#endif
