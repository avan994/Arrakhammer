#pragma once

#include "Common.h"
#include "DistanceMap.hpp"
#include "StrategyManager.h"
#include "CombatSimulation.h"
#include "SquadOrder.h"

#include "MeleeManager.h"
#include "RangedManager.h"
#include "DetectorManager.h"
#include "LurkerManager.h"
#include "TankManager.h"
#include "TransportManager.h"
#include "MedicManager.h"
#include "OverlordManager.h"
#include "CasterManager.h"
#include "StaticDefenseManager.h"

namespace UAlbertaBot
{
    
class Squad
{
    std::string         _name;
	BWAPI::Unitset      _units;
	std::string         _regroupStatus;
	bool				_combatSquad;
	bool				_hasAir;
	bool				_hasGround;
	bool				_hasAntiAir;
	bool				_hasAntiGround;
	bool				_attackAtMax;
    int                 _lastRetreatSwitch;
    bool                _lastRetreatSwitchVal;
    size_t              _priority;
	
	int _lastFormedSwitch;
	
	SquadOrder          _order;
	MeleeManager        _meleeManager;
	RangedManager       _rangedManager;
	DetectorManager     _detectorManager;
	LurkerManager       _lurkerManager;
	TankManager         _tankManager;
	TransportManager    _transportManager;
    MedicManager        _medicManager;
	OverlordManager		_overlordManager;
	CasterManager		_casterManager;
	StaticDefenseManager _staticDefenseManager;

	std::map<BWAPI::Unit, bool>	_nearEnemy;

    
	BWAPI::Unit		getRegroupUnit();
	BWAPI::Unit		unitClosestToEnemy();
    
	void			updateUnits();
	void			addUnitsToMicroManagers();
	void			setNearEnemyUnits();
	void			setAllUnits();
	
	bool			unitNearEnemy(BWAPI::Unit unit);
	bool			needsToRegroup();
	int				squadUnitsNear(BWAPI::Position p);

	void			handleSpecOps();

public:

	Squad(const std::string & name, SquadOrder order, size_t priority);
	Squad();
    ~Squad();

	void                update();
	void                setSquadOrder(const SquadOrder & so);
	void                addUnit(BWAPI::Unit u);
	void                removeUnit(BWAPI::Unit u);
	void				releaseWorkers();
    bool                containsUnit(BWAPI::Unit u) const;
    bool                isEmpty() const;
    void                clear();
    size_t              getPriority() const;
    void                setPriority(const size_t & priority);
    const std::string & getName() const;
    
	BWAPI::Position     calcCenter();
	BWAPI::Position     calcRegroupPosition();

	const BWAPI::Unitset &  getUnits() const;
	const SquadOrder &  getSquadOrder()	const;
};
}