#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "entities/effects/CEnvExplosion.h"

#include "CFuncTankMortar.h"

LINK_ENTITY_TO_CLASS( func_tankmortar, CFuncTankMortar );

void CFuncTankMortar::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "iMagnitude" ) )
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = true;
	}
	else
		CFuncTank::KeyValue( pkvd );
}

void CFuncTankMortar::Fire( const Vector &barrelEnd, const Vector &forward, CBaseEntity* pAttacker )
{
	//TODO is the attacker being passed in correctly here? - Solokiller

	if( m_fireLast != 0 )
	{
		int bulletCount = ( gpGlobals->time - m_fireLast ) * m_fireRate;
		// Only create 1 explosion
		if( bulletCount > 0 )
		{
			TraceResult tr;

			// TankTrace needs gpGlobals->v_up, etc.
			UTIL_MakeAimVectors( pev->angles );

			TankTrace( barrelEnd, forward, gTankSpread[ m_spread ], tr );

			ExplosionCreate( tr.vecEndPos, pev->angles, this, pev->impulse, true );

			CFuncTank::Fire( barrelEnd, forward, this );
		}
	}
	else
		CFuncTank::Fire( barrelEnd, forward, this );
}