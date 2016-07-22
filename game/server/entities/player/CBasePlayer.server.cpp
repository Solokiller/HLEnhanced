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
/*

===== player.cpp ========================================================

  functions dealing with the player

*/

#include "extdll.h"
#include "util.h"

#include "cbase.h"
#include "CBasePlayer.h"
#include "Weapons.h"
#include "entities/CCorpse.h"
#include "entities/CSoundEnt.h"
#include "entities/NPCs/Monsters.h"
#include "entities/spawnpoints/CBaseSpawnPoint.h"

#include "gamerules/GameRules.h"
#include "nodes/Nodes.h"
#include "hltv.h"

extern DLL_GLOBAL unsigned int	g_ulModelIndexPlayer;
extern DLL_GLOBAL bool			g_fGameOver;
extern DLL_GLOBAL bool			gDisplayTitle;

bool gInitHUD = true;

// This is a glorious hack to find free space when you've crouched into some solid space
// Our crouching collisions do not work correctly for some reason and this is easier
// than fixing the problem :(
void FixPlayerCrouchStuck( edict_t *pPlayer )
{
	TraceResult trace;

	// Move up as many as 18 pixels if the player is stuck.
	for( int i = 0; i < 18; i++ )
	{
		UTIL_TraceHull( pPlayer->v.origin, pPlayer->v.origin, dont_ignore_monsters, Hull::HEAD, pPlayer, &trace );
		if( trace.fStartSolid )
			pPlayer->v.origin.z++;
		else
			break;
	}
}

int TrainSpeed( int iSpeed, int iMax )
{
	const float fMax = ( float ) iMax;
	const float fSpeed = ( ( float ) iSpeed ) / fMax;

	int iRet = 0;

	if( iSpeed < 0 )
		iRet = TRAIN_BACK;
	else if( iSpeed == 0 )
		iRet = TRAIN_NEUTRAL;
	else if( fSpeed < 0.33 )
		iRet = TRAIN_SLOW;
	else if( fSpeed < 0.66 )
		iRet = TRAIN_MEDIUM;
	else
		iRet = TRAIN_FAST;

	return iRet;
}

LINK_ENTITY_TO_CLASS( player, CBasePlayer );

//
// ID's player as such.
//
int CBasePlayer::Classify()
{
	return CLASS_PLAYER;
}

void CBasePlayer::Precache()
{
	// SOUNDS / MODELS ARE PRECACHED in ClientPrecache() (game specific)
	// because they need to precache before any clients have connected

	// init geiger counter vars during spawn and each time
	// we cross a level transition

	m_flgeigerRange = 1000;
	m_igeigerRangePrev = 1000;

	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;

	m_iClientBattery = -1;

	m_iTrain = TRAIN_NEW;

	m_iUpdateTime = 5;  // won't update for 1/2 a second

	if( gInitHUD )
		m_fInitHUD = true;
}

void CBasePlayer::InitialSpawn()
{
	SetCustomDecalFrames( -1 ); // Assume none;

	// Reset interpolation during first frame
	pev->effects |= EF_NOINTERP;

	pev->iuser1 = 0;	// disable any spec modes
	pev->iuser2 = 0;

	Spawn();
}

void CBasePlayer::Spawn()
{
	pev->classname		= MAKE_STRING( "player" );
	pev->health			= 100;
	pev->armorvalue		= 0;
	pev->takedamage		= DAMAGE_AIM;
	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_WALK;
	pev->max_health		= pev->health;
	pev->flags			&= FL_PROXY;	// keep proxy flag sey by engine
	pev->flags			|= FL_CLIENT;
	pev->air_finished	= gpGlobals->time + 12;
	pev->dmg			= 2;				// initial water damage
	pev->effects		= 0;
	pev->deadflag		= DEAD_NO;
	pev->dmg_take		= 0;
	pev->dmg_save		= 0;
	pev->friction		= 1.0;
	pev->gravity		= 1.0;
	m_bitsHUDDamage		= -1;
	m_bitsDamageType	= 0;
	m_afPhysicsFlags	= 0;
	m_fLongJump			= false;// no longjump module. 

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );

	pev->fov = m_iFOV = 0;// init field of view.
	m_iClientFOV = -1; // make sure fov reset is sent

	m_flNextDecalTime = 0;// let this player decal as soon as he spawns.

	m_flgeigerDelay = gpGlobals->time + 2.0;	// wait a few seconds until user-defined message registrations
												// are recieved by all clients

	m_flTimeStepSound = 0;
	m_iStepLeft = 0;
	m_flFieldOfView = 0.5;// some monsters use this to determine whether or not the player is looking at them.

	m_bloodColor = BLOOD_COLOR_RED;
	m_flNextAttack = UTIL_WeaponTimeBase();

	m_iFlashBattery = 99;
	m_flFlashLightTime = 1; // force first message

	// dont let uninitialized value here hurt the player
	m_flFallVelocity = 0;

	g_pGameRules->SetDefaultPlayerTeam( this );
	g_pGameRules->GetPlayerSpawnSpot( this );

	SetModel( "models/player.mdl" );
	g_ulModelIndexPlayer = pev->modelindex;
	pev->sequence = LookupActivity( ACT_IDLE );

	if( FBitSet( pev->flags, FL_DUCKING ) )
		SetSize( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		SetSize( VEC_HULL_MIN, VEC_HULL_MAX );

	pev->view_ofs = VEC_VIEW;
	Precache();
	m_HackedGunPos = Vector( 0, 32, 0 );

	if( m_iPlayerSound == SOUNDLIST_EMPTY )
	{
		ALERT( at_console, "Couldn't alloc player sound slot!\n" );
	}

	m_fNoPlayerSound = false;// normal sound behavior.

	m_pLastItem = NULL;
	m_fInitHUD = true;
	m_iClientHideHUD = -1;  // force this to be recalculated
	m_fWeapon = false;
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;

	// reset all ammo values to 0
	for( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_rgAmmo[ i ] = 0;
		m_rgAmmoLast[ i ] = 0;  // client ammo values also have to be reset  (the death hud clear messages does on the client side)
	}

	m_lastx = m_lasty = 0;

	m_flNextChatTime = gpGlobals->time;

	g_pGameRules->PlayerSpawn( this );
}

//Called by PlayerDeadThink
void CBasePlayer::PlayerRespawn( const bool bCopyCorpse )
{
	if( gpGlobals->coop || gpGlobals->deathmatch )
	{
		if( bCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue( this );
		}

		// respawn player
		Spawn();
	}
	else
	{       // restart the entire server
		SERVER_COMMAND( "reload\n" );
	}
}

Vector CBasePlayer::GetGunPosition()
{
	//m_HackedGunPos = pev->view_ofs;
	return pev->origin + pev->view_ofs;
}

// override GiveHealth
// bitsDamageType indicates type of damage healed. 

float CBasePlayer::GiveHealth( float flHealth, int bitsDamageType )
{
	return CBaseMonster::GiveHealth( flHealth, bitsDamageType );
}

//=========================================================
// TraceAttack
//=========================================================
void CBasePlayer::TraceAttack( const CTakeDamageInfo& info, Vector vecDir, TraceResult *ptr )
{
	CTakeDamageInfo newInfo = info;

	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_GENERIC:
			break;
		case HITGROUP_HEAD:
			newInfo.GetMutableDamage() *= gSkillData.plrHead;
			break;
		case HITGROUP_CHEST:
			newInfo.GetMutableDamage() *= gSkillData.plrChest;
			break;
		case HITGROUP_STOMACH:
			newInfo.GetMutableDamage() *= gSkillData.plrStomach;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			newInfo.GetMutableDamage() *= gSkillData.plrArm;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			newInfo.GetMutableDamage() *= gSkillData.plrLeg;
			break;
		default:
			break;
		}

		SpawnBlood(ptr->vecEndPos, BloodColor(), newInfo .GetDamage());// a little surface blood.
		TraceBleed( newInfo, vecDir, ptr );
		g_MultiDamage.AddMultiDamage( newInfo, this );
	}
}

/*
*	Take some damage.  
*	NOTE: each call to OnTakeDamage with bitsDamageType set to a time-based damage
*	type will cause the damage time countdown to be reset.  Thus the ongoing effects of poison, radiation
*	etc are implemented with subsequent calls to OnTakeDamage using DMG_GENERIC.
*/
void CBasePlayer::OnTakeDamage( const CTakeDamageInfo& info )
{
	//The inflictor must be valid. - Solokiller
	ASSERT( info.GetInflictor() );

	CTakeDamageInfo newInfo = info;

	// have suit diagnose the problem - ie: report damage type
	int bitsDamage = newInfo.GetDamageTypes();
	int fmajor;
	int fcritical;
	int ftrivial;
	float flRatio;
	float flBonus;
	float flHealthPrev = pev->health;

	flBonus = PLAYER_ARMOR_BONUS;
	flRatio = PLAYER_ARMOR_RATIO;

	if ( ( newInfo.GetDamageTypes() & DMG_BLAST ) && g_pGameRules->IsMultiplayer() )
	{
		// blasts damage armor more.
		flBonus *= 2;
	}

	// Already dead
	if ( !IsAlive() )
		return;
	// go take the damage first

	if ( !g_pGameRules->FPlayerCanTakeDamage( this, newInfo ) )
	{
		// Refuse the damage
		return;
	}

	// keep track of amount of damage last sustained
	m_lastDamageAmount = newInfo.GetDamage();

	// Armor. 
	if (pev->armorvalue && !( newInfo.GetDamageTypes() & (DMG_FALL | DMG_DROWN)) )// armor doesn't protect against fall or drown damage!
	{
		float flNew = newInfo.GetDamage() * flRatio;

		float flArmor;

		flArmor = ( newInfo.GetDamage() - flNew) * flBonus;

		// Does this use more armor than we have?
		if (flArmor > pev->armorvalue)
		{
			flArmor = pev->armorvalue;
			flArmor *= (1/flBonus);
			flNew = newInfo.GetDamage() - flArmor;
			pev->armorvalue = 0;
		}
		else
			pev->armorvalue -= flArmor;
		
		newInfo.GetMutableDamage() = flNew;
	}

	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)
	newInfo.GetMutableDamage() = int( newInfo.GetDamage() );
	const float flOldHealth = pev->health;
	CBaseMonster::OnTakeDamage( newInfo );

	const bool bTookDamage = flOldHealth != pev->health;

	// reset damage time countdown for each type of time based damage player just sustained

	{
		for (int i = 0; i < CDMG_TIMEBASED; i++)
			if ( newInfo.GetDamageTypes() & (DMG_PARALYZE << i))
				m_rgbTimeBasedDamage[i] = 0;
	}

	// tell director about it
	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE ( 9 );	// command length in bytes
		WRITE_BYTE ( DRC_CMD_EVENT );	// take damage event
		WRITE_SHORT( ENTINDEX(this->edict()) );	// index number of primary entity
		WRITE_SHORT( newInfo.GetInflictor()->entindex() );	// index number of secondary entity
		WRITE_LONG( 5 );   // eventflags (priority and flags)
	MESSAGE_END();


	// how bad is it, doc?

	ftrivial = (pev->health > 75 || m_lastDamageAmount < 5);
	fmajor = (m_lastDamageAmount > 25);
	fcritical = (pev->health < 30);

	// handle all bits set in this damage message,
	// let the suit give player the diagnosis

	// UNDONE: add sounds for types of damage sustained (ie: burn, shock, slash )

	// UNDONE: still need to record damage and heal messages for the following types

		// DMG_BURN	
		// DMG_FREEZE
		// DMG_BLAST
		// DMG_SHOCK

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;  // make sure the damage bits get resent

	bool bFound = true;

	while ( bTookDamage && (!ftrivial || (bitsDamage & DMG_TIMEBASED)) && bFound && bitsDamage)
	{
		bFound = false;

		if (bitsDamage & DMG_CLUB)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG4", SUIT_SENTENCE, SUIT_NEXT_IN_30SEC);	// minor fracture
			bitsDamage &= ~DMG_CLUB;
			bFound = true;
		}
		if (bitsDamage & (DMG_FALL | DMG_CRUSH))
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG5", SUIT_SENTENCE, SUIT_NEXT_IN_30SEC);	// major fracture
			else
				SetSuitUpdate("!HEV_DMG4", SUIT_SENTENCE, SUIT_NEXT_IN_30SEC);	// minor fracture
	
			bitsDamage &= ~(DMG_FALL | DMG_CRUSH);
			bFound = true;
		}
		
		if (bitsDamage & DMG_BULLET)
		{
			if (m_lastDamageAmount > 5)
				SetSuitUpdate("!HEV_DMG6", SUIT_SENTENCE, SUIT_NEXT_IN_30SEC);	// blood loss detected
			//else
			//	SetSuitUpdate("!HEV_DMG0", SUIT_SENTENCE, SUIT_NEXT_IN_30SEC);	// minor laceration
			
			bitsDamage &= ~DMG_BULLET;
			bFound = true;
		}

		if (bitsDamage & DMG_SLASH)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG1", SUIT_SENTENCE, SUIT_NEXT_IN_30SEC);	// major laceration
			else
				SetSuitUpdate("!HEV_DMG0", SUIT_SENTENCE, SUIT_NEXT_IN_30SEC);	// minor laceration

			bitsDamage &= ~DMG_SLASH;
			bFound = true;
		}
		
		if (bitsDamage & DMG_SONIC)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG2", SUIT_SENTENCE, SUIT_NEXT_IN_1MIN);	// internal bleeding
			bitsDamage &= ~DMG_SONIC;
			bFound = true;
		}

		if (bitsDamage & (DMG_POISON | DMG_PARALYZE))
		{
			SetSuitUpdate("!HEV_DMG3", SUIT_SENTENCE, SUIT_NEXT_IN_1MIN);	// blood toxins detected
			bitsDamage &= ~(DMG_POISON | DMG_PARALYZE);
			bFound = true;
		}

		if (bitsDamage & DMG_ACID)
		{
			SetSuitUpdate("!HEV_DET1", SUIT_SENTENCE, SUIT_NEXT_IN_1MIN);	// hazardous chemicals detected
			bitsDamage &= ~DMG_ACID;
			bFound = true;
		}

		if (bitsDamage & DMG_NERVEGAS)
		{
			SetSuitUpdate("!HEV_DET0", SUIT_SENTENCE, SUIT_NEXT_IN_1MIN);	// biohazard detected
			bitsDamage &= ~DMG_NERVEGAS;
			bFound = true;
		}

		if (bitsDamage & DMG_RADIATION)
		{
			SetSuitUpdate("!HEV_DET2", SUIT_SENTENCE, SUIT_NEXT_IN_1MIN);	// radiation detected
			bitsDamage &= ~DMG_RADIATION;
			bFound = true;
		}
		if (bitsDamage & DMG_SHOCK)
		{
			bitsDamage &= ~DMG_SHOCK;
			bFound = true;
		}
	}

	pev->punchangle.x = -2;

	if ( bTookDamage && !ftrivial && fmajor && flHealthPrev >= 75)
	{
		// first time we take major damage...
		// turn automedic on if not on
		SetSuitUpdate("!HEV_MED1", SUIT_SENTENCE, SUIT_NEXT_IN_30MIN);	// automedic on

		// give morphine shot if not given recently
		SetSuitUpdate("!HEV_HEAL7", SUIT_SENTENCE, SUIT_NEXT_IN_30MIN);	// morphine shot
	}
	
	if ( bTookDamage && !ftrivial && fcritical && flHealthPrev < 75)
	{

		// already took major damage, now it's critical...
		if (pev->health < 6)
			SetSuitUpdate("!HEV_HLTH3", SUIT_SENTENCE, SUIT_NEXT_IN_10MIN);	// near death
		else if (pev->health < 20)
			SetSuitUpdate("!HEV_HLTH2", SUIT_SENTENCE, SUIT_NEXT_IN_10MIN);	// health critical
	
		// give critical health warnings
		if (!RANDOM_LONG(0,3) && flHealthPrev < 50)
			SetSuitUpdate("!HEV_DMG7", SUIT_SENTENCE, SUIT_NEXT_IN_5MIN); //seek medical attention
	}

	// if we're taking time based damage, warn about its continuing effects
	if ( bTookDamage && ( newInfo.GetDamageTypes() & DMG_TIMEBASED) && flHealthPrev < 75)
	{
		if (flHealthPrev < 50)
		{
			if (!RANDOM_LONG(0,3))
				SetSuitUpdate("!HEV_DMG7", SUIT_SENTENCE, SUIT_NEXT_IN_5MIN); //seek medical attention
		}
		else
			SetSuitUpdate("!HEV_HLTH1", SUIT_SENTENCE, SUIT_NEXT_IN_10MIN);	// health dropping
	}
}

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 */
void CBasePlayer::Killed( const CTakeDamageInfo& info, GibAction gibAction )
{
	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		m_pActiveItem->Holster();

	g_pGameRules->PlayerKilled( this, info );

	if ( m_pTank != NULL )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}

	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	if ( CSound* pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( this ) ) )
	{
		pSound->Reset();
	}

	SetAnimation( PLAYER_DIE );
	
	m_iRespawnFrames = 0;

	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes

	pev->deadflag		= DEAD_DYING;
	pev->movetype		= MOVETYPE_TOSS;
	ClearBits( pev->flags, FL_ONGROUND );
	if (pev->velocity.z < 10)
		pev->velocity.z += RANDOM_FLOAT(0,300);

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, SUIT_SENTENCE, 0);

	// send "health" update message to zero
	m_iClientHealth = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
		WRITE_BYTE( m_iClientHealth );
	MESSAGE_END();

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0XFF);
		WRITE_BYTE(0xFF);
	MESSAGE_END();

	// reset FOV
	pev->fov = m_iFOV = m_iClientFOV = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
		WRITE_BYTE(0);
	MESSAGE_END();


	// UNDONE: Put this in, but add FFADE_PERMANENT and make fade time 8.8 instead of 4.12
	// UTIL_ScreenFade( edict(), Vector(128,0,0), 6, 15, 255, FFADE_OUT | FFADE_MODULATE );

	if ( ( pev->health < -40 && gibAction != GIB_NEVER ) || gibAction == GIB_ALWAYS )
	{
		pev->solid			= SOLID_NOT;
		GibMonster();	// This clears pev->model
		pev->effects |= EF_NODRAW;
		return;
	}

	DeathSound();
	
	pev->angles.x = 0;
	pev->angles.z = 0;

	SetThink(&CBasePlayer::PlayerDeathThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

const char *CBasePlayer::TeamID() const
{
	if( pev == NULL )		// Not fully connected yet
		return "";

	// return their team name
	return m_szTeamName;
}

bool CBasePlayer::Restore( CRestore &restore )
{
	if( !BaseClass::Restore( restore ) )
		return false;

	SAVERESTOREDATA *pSaveData = ( SAVERESTOREDATA * ) gpGlobals->pSaveData;
	// landmark isn't present.
	if( !pSaveData->fUseLandmark )
	{
		ALERT( at_console, "No Landmark:%s\n", pSaveData->szLandmarkName );

		// default to normal spawn
		CBaseEntity* pSpawnSpot = EntSelectSpawnPoint( this );
		pev->origin = pSpawnSpot->pev->origin + Vector( 0, 0, 1 );
		pev->angles = pSpawnSpot->pev->angles;
	}
	pev->v_angle.z = 0;	// Clear out roll
	pev->angles = pev->v_angle;

	pev->fixangle = FIXANGLE_SET;           // turn this way immediately

											// Copied from spawn() for now
	m_bloodColor = BLOOD_COLOR_RED;

	g_ulModelIndexPlayer = pev->modelindex;

	if( FBitSet( pev->flags, FL_DUCKING ) )
	{
		// Use the crouch HACK
		//FixPlayerCrouchStuck( edict() );
		// Don't need to do this with new player prediction code.
		SetSize( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	}
	else
	{
		SetSize( VEC_HULL_MIN, VEC_HULL_MAX );
	}

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );

	if( m_fLongJump )
	{
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "1" );
	}
	else
	{
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );
	}

	RenewItems();

	//Resync ammo data so you can reload - Solokiller
	TabulateAmmo();

#if defined( CLIENT_WEAPONS )
	// HACK:	This variable is saved/restored in CBaseMonster as a time variable, but we're using it
	//			as just a counter.  Ideally, this needs its own variable that's saved as a plain float.
	//			Barring that, we clear it out here instead of using the incorrect restored time value.
	m_flNextAttack = UTIL_WeaponTimeBase();
#endif

	return true;
}

//=========================================================
// BarnacleVictimGrabbed - Overridden for the player to set the proper
// physics flags when a barnacle grabs player.
//=========================================================
bool CBasePlayer::BarnacleVictimGrabbed( CBaseMonster* pBarnacle )
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return true;
}

//=========================================================
// BarnacleVictimBitten - bad name for a function that is called
// by Barnacle victims when the barnacle pulls their head
// into its mouth. For the player, just die.
//=========================================================
void CBasePlayer::BarnacleVictimBitten( CBaseEntity* pBarnacle )
{
	TakeDamage( pBarnacle, pBarnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB );
}

//=========================================================
// BarnacleVictimReleased - overridden for player who has
// physics flags concerns. 
//=========================================================
void CBasePlayer::BarnacleVictimReleased()
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}

//=========================================================
// Illumination 
// return player light level plus virtual muzzle flash
//=========================================================
int CBasePlayer::Illumination() const
{
	int iIllum = CBaseEntity::Illumination();

	iIllum += m_iWeaponFlash;
	if( iIllum > 255 )
		return 255;
	return iIllum;
}

/*
=============
SetCustomDecalFrames

UNDONE:  Determine real frame limit, 8 is a placeholder.
Note:  -1 means no custom frames present.
=============
*/
void CBasePlayer::SetCustomDecalFrames( int nFrames )
{
	if( nFrames > 0 &&
		nFrames < 8 )
		m_nCustomSprayFrames = nFrames;
	else
		m_nCustomSprayFrames = -1;
}

/*
=============
GetCustomDecalFrames

Returns the # of custom frames this player's custom clan logo contains.
=============
*/
int CBasePlayer::GetCustomDecalFrames()
{
	return m_nCustomSprayFrames;
}

//Player ID
void CBasePlayer::InitStatusBar()
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString1[0] = m_SbarString0[0] = 0; 
}

void CBasePlayer::UpdateStatusBar()
{
	int newSBarState[ SBAR_END ];
	char sbuf0[ SBAR_STRING_SIZE ];
	char sbuf1[ SBAR_STRING_SIZE ];

	memset( newSBarState, 0, sizeof(newSBarState) );
	strcpy( sbuf0, m_SbarString0 );
	strcpy( sbuf1, m_SbarString1 );

	// Find an ID Target
	TraceResult tr;
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );
	Vector vecSrc = EyePosition();
	Vector vecEnd = vecSrc + (gpGlobals->v_forward * MAX_ID_RANGE);
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr);

	if (tr.flFraction != 1.0)
	{
		if ( !FNullEnt( tr.pHit ) )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

			if (pEntity->Classify() == CLASS_PLAYER )
			{
				newSBarState[ SBAR_ID_TARGETNAME ] = ENTINDEX( pEntity->edict() );
				strcpy( sbuf1, "1 %p1\n2 Health: %i2%%\n3 Armor: %i3%%" );

				// allies and medics get to see the targets health
				if ( g_pGameRules->PlayerRelationship( this, pEntity ) == GR_TEAMMATE )
				{
					newSBarState[ SBAR_ID_TARGETHEALTH ] = 100 * (pEntity->pev->health / pEntity->pev->max_health);
					newSBarState[ SBAR_ID_TARGETARMOR ] = pEntity->pev->armorvalue; //No need to get it % based since 100 it's the max.
				}

				m_flStatusBarDisappearDelay = gpGlobals->time + 1.0;
			}
		}
		else if ( m_flStatusBarDisappearDelay > gpGlobals->time )
		{
			// hold the values for a short amount of time after viewing the object
			newSBarState[ SBAR_ID_TARGETNAME ] = m_izSBarState[ SBAR_ID_TARGETNAME ];
			newSBarState[ SBAR_ID_TARGETHEALTH ] = m_izSBarState[ SBAR_ID_TARGETHEALTH ];
			newSBarState[ SBAR_ID_TARGETARMOR ] = m_izSBarState[ SBAR_ID_TARGETARMOR ];
		}
	}

	bool bForceResend = false;

	if ( strcmp( sbuf0, m_SbarString0 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( sbuf0 );
		MESSAGE_END();

		strcpy( m_SbarString0, sbuf0 );

		// make sure everything's resent
		bForceResend = true;
	}

	if ( strcmp( sbuf1, m_SbarString1 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( sbuf1 );
		MESSAGE_END();

		strcpy( m_SbarString1, sbuf1 );

		// make sure everything's resent
		bForceResend = true;
	}

	// Check values and send if they don't match
	for (int i = 1; i < SBAR_END; i++)
	{
		if ( newSBarState[i] != m_izSBarState[i] || bForceResend )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusValue, NULL, pev );
				WRITE_BYTE( i );
				WRITE_SHORT( newSBarState[i] );
			MESSAGE_END();

			m_izSBarState[i] = newSBarState[i];
		}
	}
}