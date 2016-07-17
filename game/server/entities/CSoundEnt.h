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
//=========================================================
// Soundent.h - the entity that spawns when the world 
// spawns, and handles the world's active and free sound
// lists.
//=========================================================

#define	MAX_WORLD_SOUNDS	64 // maximum number of sounds handled by the world at one time.

#define bits_SOUND_NONE		0
#define	bits_SOUND_COMBAT	( 1 << 0 )// gunshots, explosions
#define bits_SOUND_WORLD	( 1 << 1 )// door opening/closing, glass breaking
#define bits_SOUND_PLAYER	( 1 << 2 )// all noises generated by player. walking, shooting, falling, splashing
#define bits_SOUND_CARCASS	( 1 << 3 )// dead body
#define bits_SOUND_MEAT		( 1 << 4 )// gib or pork chop
#define bits_SOUND_DANGER	( 1 << 5 )// pending danger. Grenade that is about to explode, explosive barrel that is damaged, falling crate
#define bits_SOUND_GARBAGE	( 1 << 6 )// trash cans, banana peels, old fast food bags.

#define bits_ALL_SOUNDS 0xFFFFFFFF

#define SOUNDLIST_EMPTY	-1

#define SOUNDLISTTYPE_FREE	1// identifiers passed to functions that can operate on either list, to indicate which list to operate on.
#define SOUNDLISTTYPE_ACTIVE 2

#define	SOUND_NEVER_EXPIRE	-1 // with this set as a sound's ExpireTime, the sound will never expire.

//=========================================================
// CSound - an instance of a sound in the world.
//=========================================================
class CSound
{
public:

	void	Clear();
	void	Reset();

	Vector	m_vecOrigin;	// sound's location in space
	int		m_iType;		// what type of sound this is
	int		m_iVolume;		// how loud the sound is
	float	m_flExpireTime;	// when the sound should be purged from the list
	int		m_iNext;		// index of next sound in this list ( Active or Free )
	int		m_iNextAudible;	// temporary link that monsters use to build a list of audible sounds

	bool	FIsSound() const;
	bool	FIsScent() const;
};

//=========================================================
// CSoundEnt - a single instance of this entity spawns when
// the world spawns. The SoundEnt's job is to update the 
// world's Free and Active sound lists.
//=========================================================
class CSoundEnt : public CBaseEntity 
{
public:
	DECLARE_CLASS( CSoundEnt, CBaseEntity );

	void Precache ( void ) override;
	void Spawn( void ) override;
	void Think( void ) override;
	void Initialize ( void );
	
	static void		InsertSound ( int iType, const Vector &vecOrigin, int iVolume, float flDuration );
	static void		FreeSound ( int iSound, int iPrevious );
	static int		ActiveList( void );// return the head of the active list
	static int		FreeList( void );// return the head of the free list
	static CSound*	SoundPointerForIndex( int iIndex );// return a pointer for this index in the sound list
	static int		ClientSoundIndex( const CBaseEntity* const pClient );

	bool	IsEmpty() const { return m_iActiveSound == SOUNDLIST_EMPTY; }
	int		ISoundsInList ( int iListType );
	int		IAllocSound ( void );
	virtual int		ObjectCaps() const override { return FCAP_DONT_SAVE; }
	
	int		m_iFreeSound;	// index of the first sound in the free sound list
	int		m_iActiveSound; // indes of the first sound in the active sound list
	int		m_cLastActiveSounds; // keeps track of the number of active sounds at the last update. (for diagnostic work)
	bool	m_fShowReport; // if true, dump information about free/active sounds.

private:
	CSound		m_SoundPool[ MAX_WORLD_SOUNDS ];
};
