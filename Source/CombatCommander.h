#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "InformationManager.h"
#include "StrategyManager.h"

namespace UAlbertaBot
{
class CombatCommander
{
	SquadData       _squadData;
    BWAPI::Unitset  _combatUnits;
    bool            _initialized;

	bool			_goAggressive;

    void            updateScoutDefenseSquad();
	void            updateBaseDefenseSquads();
	void            updateAttackSquads();
    void            updateDropSquads();
	void            updateIdleSquad();
	void			updateSurveySquad();
	void			updateSpecOpsSquad();

	void			loadOrUnloadBunkers();
	void			doComsatScan();

	void			cancelDyingBuildings();

	int             getNumType(BWAPI::Unitset & units, BWAPI::UnitType type);

	BWAPI::Unit     findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender, bool pullWorkers = false, bool pullSpecialUnits = false);
	BWAPI::Unit     findClosestOverlord(const Squad &, BWAPI::Position);
    BWAPI::Unit     findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target);

	bool _allExplored;
	BWAPI::Position getDefendLocation();
    BWAPI::Position getMainAttackLocation();
	BWAPI::Position getSurveyLocation();
	BWAPI::Position getHuntLocation();

    void            initializeSquads();
    void            verifySquadUniqueMembership();
    void            assignFlyingDefender(Squad & squad);
    void            emptySquad(Squad & squad, BWAPI::Unitset & unitsToAssign);
    int             getNumGroundDefendersInSquad(Squad & squad);
    int             getNumAirDefendersInSquad(Squad & squad);

    void            updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded);
	bool			shouldPullWorkers(BWAPI::Position);

public:

	CombatCommander();

	void update(const BWAPI::Unitset & combatUnits);

	void setAggression(bool aggressive) { _goAggressive = aggressive;  }
	bool getAggression() const { return _goAggressive; };
    
	void releaseWorkers();

	void drawSquadInformation(int x, int y);

	static CombatCommander & Instance();
};
}
