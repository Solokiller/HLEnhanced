/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef GAME_SERVER_SAVERESTORE_SAVERESTOREDEFS_H
#define GAME_SERVER_SAVERESTORE_SAVERESTOREDEFS_H

enum FIELDTYPE
{
	FIELD_FLOAT = 0,		// Any floating point value
	FIELD_STRING,			// A string ID (return from ALLOC_STRING)
	FIELD_ENTITY,			// An entity offset (EOFFSET)
	FIELD_CLASSPTR,			// CBaseEntity *
	FIELD_EHANDLE,			// Entity handle
	FIELD_EVARS,			// EVARS *
	FIELD_EDICT,			// edict_t *, or edict_t *  (same thing)
	FIELD_VECTOR,			// Any vector
	FIELD_POSITION_VECTOR,	// A world coordinate (these are fixed up across level transitions automagically)
	FIELD_INTEGER,			// Any integer or enum
	FIELD_FUNCTION,			// A class function pointer (Think, Use, etc)
	FIELD_BOOLEAN,			// boolean, I may use this as a hint for compression
	FIELD_SHORT,			// 2 byte integer
	FIELD_CHARACTER,		// a byte
	FIELD_TIME,				// a floating point time (these are fixed up automatically too!)
	FIELD_MODELNAME,		// Engine string that is a model name (needs precache)
	FIELD_SOUNDNAME,		// Engine string that is a sound name (needs precache)

	FIELD_TYPECOUNT,		// MUST BE LAST
};

namespace TypeDescFlag
{
enum TypeDescFlag
{
	/**
	*	This field is masked for global entity save/restore
	*/
	GLOBAL	= 0x0001,

	/**
	*	This field can be automatically initialized by DispatchKeyValue. - Solokiller
	*/
	KEY		= 0x0002,
};
}

struct TYPEDESCRIPTION
{
	FIELDTYPE		fieldType;
	const char*		fieldName;
	/**
	*	The name used to refer to this field if it's a key. - Solokiller
	*/
	const char*		pszPublicName;
	int				fieldOffset;
	short			fieldSize;
	short			flags;
};

#define _FIELD( type, name, fieldtype, count, flags )			{ fieldtype, #name, nullptr, static_cast<int>( OFFSETOF( type, name ) ), count, flags }
#define _BASEENT_FIELD( name, fieldtype, count, flags )			_FIELD( ThisClass, name, fieldtype, count, flags )
#define DEFINE_FIELD( name, fieldtype )							_BASEENT_FIELD( name, fieldtype, 1, 0 )
#define DEFINE_ARRAY( name, fieldtype, count )					_BASEENT_FIELD( name, fieldtype, count, 0 )
#define DEFINE_ENTITY_FIELD( name, fieldtype )					_FIELD( entvars_t, name, fieldtype, 1, 0 )
#define DEFINE_ENTITY_GLOBAL_FIELD( name, fieldtype )			_FIELD( entvars_t, name, fieldtype, 1, TypeDescFlag::GLOBAL )
#define DEFINE_GLOBAL_FIELD( name, fieldtype )					_BASEENT_FIELD( name, fieldtype, 1, TypeDescFlag::GLOBAL )

/**
*	Defines a field that can be automatically initialized by DispatchKeyValue. - Solokiller
*/
#define DEFINE_KEYFIELD( name, fieldtype, szKVName )			{ fieldtype, #name, szKVName, static_cast<int>( OFFSETOF( ThisClass, name ) ), 1, TypeDescFlag::KEY }

#endif //GAME_SERVER_SAVERESTORE_SAVERESTOREDEFS_H
