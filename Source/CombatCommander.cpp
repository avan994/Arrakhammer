#include "CombatCommander.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

const size_t IdlePriority = 0;
const size_t AttackPriority = 2;
const size_t BaseDefensePriority = 3;
const size_t ScoutDefensePriority = 4;
const size_t DropPriority = 5;
const size_t SpecOpsPriority = 9;    //stealing from SpecOps could ruin special operations
const size_t SurveyPriority = 10;    // consists of only 1 overlord, no need to steal from it

CombatCommander::CombatCommander() 
    : _initialized(false)
	, _goAggressive(true)
	, _allExplored(false)
{
}

void CombatCommander::initializeSquads()
{
	// The idle squad includes workers at work (not idle at all) and unassigned overlords.
    SquadOrder idleOrder(SquadOrderTypes::Idle, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), 100, "Chill Out");
	_squadData.addSquad("Idle", Squad("Idle", idleOrder, IdlePriority));

    // the main attack squad will pressure the enemy's closest base location
    SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
	_squadData.addSquad("MainAttack", Squad("MainAttack", mainAttackOrder, AttackPriority));

	// The flying squad separates air units so they can act independently.
	// It gets the same order as the attack squad.
	_squadData.addSquad("Flying", Squad("Flying", mainAttackOrder, AttackPriority));

	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

    // the scout defense squad will handle chasing the enemy worker scout
	if (Config::Micro::ScoutDefenseRadius > 0)
	{
		SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, ourBasePosition, Config::Micro::ScoutDefenseRadius, "Get the scout");
		_squadData.addSquad("ScoutDefense", Squad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority));
	}

	// add a drop squad if we are using a drop strategy
    if (StrategyManager::Instance().getOpeningGroup() == "drop")
    {
        SquadOrder zealotDrop(SquadOrderTypes::Drop, ourBasePosition, 900, "Wait for transport");
        _squadData.addSquad("Drop", Squad("Drop", zealotDrop, DropPriority));
    }

	// Zerg can put overlords into a survey squad.
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
	{
		SquadOrder surveyMap = SquadOrder(SquadOrderTypes::Survey, BWAPI::Position(0,0), 100, "Get the surveyors");

		_squadData.addSquad("Survey", Squad("Survey", surveyMap, SurveyPriority));
	}

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) //only intended for zerg right now.
	{
		SquadOrder specOps = SquadOrder(SquadOrderTypes::SpecOps, ourBasePosition, 100, "Special operations squad");

		_squadData.addSquad("SpecOps", Squad("SpecOps", specOps, SpecOpsPriority));
	}

    _initialized = true;
}

void CombatCommander::update(const BWAPI::Unitset & combatUnits)
{
    if (!_initialized)
    {
        initializeSquads();
    }

    _combatUnits = combatUnits;

	int frame8 = BWAPI::Broodwar->getFrameCount() % 8;

	if      (frame8 == 0) { updateIdleSquad(); }
	else if (frame8 == 1) { updateDropSquads(); }
	else if (frame8 == 2) { updateScoutDefenseSquad(); }
	else if (frame8 == 3) { updateBaseDefenseSquads(); }
	else if (frame8 == 4) { updateAttackSquads(); }
	else if (frame8 == 5) { updateSurveySquad(); }
	else if (frame8 == 6)  { updateSpecOpsSquad(); }

	loadOrUnloadBunkers();

	if (frame8 % 4 == 2)
	{
		doComsatScan();
	}
	
	_squadData.update();          // update() all the squads

	cancelDyingBuildings();
}

void CombatCommander::updateIdleSquad()
{
    Squad & idleSquad = _squadData.getSquad("Idle");
    for (auto & unit : _combatUnits)
    {
        // if it hasn't been assigned to a squad yet, put it in the low priority idle squad
        if (_squadData.canAssignUnitToSquad(unit, idleSquad))
        {
            idleSquad.addUnit(unit);
        }
    }
}

void CombatCommander::updateAttackSquads()
{
    Squad & mainAttackSquad = _squadData.getSquad("MainAttack");
	Squad & flyingSquad = _squadData.getSquad("Flying");

	// Include exactly 1 overlord in each squad, for detection.
	bool mainOverlord = false;
	bool mainSquadExists = false;
	for (auto & unit : mainAttackSquad.getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			mainOverlord = true;
		}
		else
		{
			mainSquadExists = true;
		}
	}

	bool flyingOverlord = false;
	bool flyingSquadExists = false;	    // scourge goes to flying squad if any, otherwise main squad
	for (auto & unit : flyingSquad.getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			flyingOverlord = true;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Devourer ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Guardian)
		{
			flyingSquadExists = true;
		}
	}

	for (auto & unit : _combatUnits)
    {
        // get every unit of a lower priority and put it into an attack squad
		bool isOverlord = unit->getType() == BWAPI::UnitTypes::Zerg_Overlord;
		//no need to send our overlords out until they have cloaked
		if (isOverlord && !InformationManager::Instance().enemyHasMobileCloakTech()) continue;

		if ((unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Devourer ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Guardian ||
			(unit->getType() == BWAPI::UnitTypes::Zerg_Scourge && flyingSquadExists) ||
			(isOverlord && !flyingOverlord && flyingSquadExists)) &&
			_squadData.canAssignUnitToSquad(unit, flyingSquad))
		{
			_squadData.assignUnitToSquad(unit, flyingSquad);
			if (isOverlord)
			{
				flyingOverlord = true;
			}
		}
        else if (!unit->getType().isWorker() &&
			(!isOverlord || isOverlord && !mainOverlord && mainSquadExists) &&
			_squadData.canAssignUnitToSquad(unit, mainAttackSquad))
        {
			_squadData.assignUnitToSquad(unit, mainAttackSquad);
			if (isOverlord)
			{
				mainOverlord = true;
			}
        }
    }

    SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
    mainAttackSquad.setSquadOrder(mainAttackOrder);
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {
		SquadOrder huntingOrder(SquadOrderTypes::Attack, getHuntLocation(), 800, "Hunt Them Down");
		flyingSquad.setSquadOrder(huntingOrder);
	}
	else {
		flyingSquad.setSquadOrder(mainAttackOrder);
	}
}

void CombatCommander::updateDropSquads()
{
	if (StrategyManager::Instance().getOpeningGroup() != "drop")
    {
        return;
    }

    Squad & dropSquad = _squadData.getSquad("Drop");

    // figure out how many units the drop squad needs
    bool dropSquadHasTransport = false;
    int transportSpotsRemaining = 8;
    auto & dropUnits = dropSquad.getUnits();

    for (auto & unit : dropUnits)
    {
        if (unit->isFlying() && unit->getType().spaceProvided() > 0)
        {
            dropSquadHasTransport = true;
        }
        else
        {
            transportSpotsRemaining -= unit->getType().spaceRequired();
        }
    }

    // if there are still units to be added to the drop squad, do it
    if (transportSpotsRemaining > 0 || !dropSquadHasTransport)
    {
        // take our first amount of combat units that fill up a transport and add them to the drop squad
        for (auto & unit : _combatUnits)
        {
            // if this is a transport unit and we don't have one in the squad yet, add it
            if (!dropSquadHasTransport && (unit->getType().spaceProvided() > 0 && unit->isFlying()))
            {
                _squadData.assignUnitToSquad(unit, dropSquad);
                dropSquadHasTransport = true;
                continue;
            }

            if (unit->getType().spaceRequired() > transportSpotsRemaining)
            {
                continue;
            }

            // get every unit of a lower priority and put it into the attack squad
            if (!unit->getType().isWorker() && _squadData.canAssignUnitToSquad(unit, dropSquad))
            {
                _squadData.assignUnitToSquad(unit, dropSquad);
                transportSpotsRemaining -= unit->getType().spaceRequired();
            }
        }
    }
    // otherwise the drop squad is full, so execute the order
    else
    {
        SquadOrder dropOrder(SquadOrderTypes::Drop, getMainAttackLocation(), 800, "Attack Enemy Base");
        dropSquad.setSquadOrder(dropOrder);
    }
}

void CombatCommander::updateScoutDefenseSquad() 
{
	if (Config::Micro::ScoutDefenseRadius == 0 || _combatUnits.empty())
    { 
        return; 
    }

    // if the current squad has units in it then we can ignore this
    Squad & scoutDefenseSquad = _squadData.getSquad("ScoutDefense");
  
    // get the region that our base is located in
    BWTA::Region * myRegion = BWTA::getRegion(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition());
    if (!myRegion || !myRegion->getCenter().isValid())
    {
        return;
    }

    // get all of the enemy units in this region
	BWAPI::Unitset enemyUnitsInRegion;
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
        {
            enemyUnitsInRegion.insert(unit);
        }
    }

    // if there's an enemy worker in our region then assign someone to chase him
    bool assignScoutDefender = enemyUnitsInRegion.size() == 1 && (*enemyUnitsInRegion.begin())->getType().isWorker();

    // if our current squad is empty and we should assign a worker, do it
    if (scoutDefenseSquad.isEmpty() && assignScoutDefender)
    {
        // the enemy worker that is attacking us
        BWAPI::Unit enemyWorker = *enemyUnitsInRegion.begin();

        // get our worker unit that is mining that is closest to it
        BWAPI::Unit workerDefender = findClosestWorkerToTarget(_combatUnits, enemyWorker);

		if (enemyWorker && workerDefender)
		{
			// grab it from the worker manager and put it in the squad
            if (_squadData.canAssignUnitToSquad(workerDefender, scoutDefenseSquad))
            {
			    WorkerManager::Instance().setCombatWorker(workerDefender);
                _squadData.assignUnitToSquad(workerDefender, scoutDefenseSquad);
            }
		}
    }
    // if our squad is not empty and we shouldn't have a worker chasing then take it out of the squad
    else if (!scoutDefenseSquad.isEmpty() && !assignScoutDefender)
    {
        scoutDefenseSquad.clear();     // also releases the worker
    }
}

void CombatCommander::updateBaseDefenseSquads() 
{
	if (_combatUnits.empty()) 
    { 
        return; 
    }
    
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
    BWTA::Region * enemyRegion = nullptr;
    if (enemyBaseLocation)
    {
        enemyRegion = BWTA::getRegion(enemyBaseLocation->getPosition());
    }

	BWTA::BaseLocation * mainBaseLocation = InformationManager::Instance().getMyMainBaseLocation();
	BWTA::BaseLocation * naturalLocation = InformationManager::Instance().getMyNaturalLocation();
	BWTA::Region * mainRegion = NULL;
	BWTA::Region * naturalRegion = NULL;
	bool naturalTaken = false;
	if (mainBaseLocation) {
		mainRegion = mainBaseLocation->getRegion();
	}
	if (naturalLocation) {
		naturalRegion = naturalLocation->getRegion();
		naturalTaken = InformationManager::Instance().getBaseOwner(naturalLocation) == BWAPI::Broodwar->self();
	}

	// for each of our occupied regions
	for (BWTA::Region * myRegion : InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->self()))
	{
        // don't defend inside the enemy region, this will end badly when we are stealing gas
        if (myRegion == enemyRegion) continue;
		
		BWAPI::Position regionCenter = myRegion->getCenter();
		if (!regionCenter.isValid()) continue;

		// as zerg we need to outnumber the enemy when defending
		int numDefendersPerEnemyUnit = 3;

		//keep some permanent air defenders around our bases, unless we have more than 5 bases
		bool needAirDefense = InformationManager::Instance().needAirDefense();

		int flyingDefendersNeeded = 0;
		if (flyingDefendersNeeded == 0 && needAirDefense) flyingDefendersNeeded += 2;
		int groundDefendersNeeded = 0;

		// all of the enemy units in this region
		//BWAPI::Unitset enemyUnitsInRegion;
        for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            // If it's a harmless air unit, don't worry about it for base defense.
			// TODO something more sensible
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Observer ||
				unit->isLifted()) // floating terran building
            {
                continue;
            }

			//unpowered protoss buildings are not a major threat
			if (unit->getType().isBuilding() && unit->getType().getRace() == BWAPI::Races::Protoss &&
				!unit->isPowered()) continue;
			
			auto occupiedRegion = BWTA::getRegion(BWAPI::TilePosition(unit->getPosition()));
			if (occupiedRegion == myRegion ||
				(unit->getDistance(regionCenter) < 600) ||
				(unit->getType().isBuilding() && //proxy detection in natural
				myRegion == mainRegion && occupiedRegion == naturalRegion && !naturalTaken)) 
			{
				if (unit->isFlying()) {
					flyingDefendersNeeded += numDefendersPerEnemyUnit;
				}
				else if (unit->getType().isWorker()) {
					groundDefendersNeeded += 1;
				}
				else {
					groundDefendersNeeded += numDefendersPerEnemyUnit;
				}
			}
        }

        std::stringstream squadName;
        squadName << "Base Defense " << regionCenter.x << " " << regionCenter.y; 

        // if there's nothing in this region to worry about
        //if (enemyUnitsInRegion.empty() && !needAirDefense)
		if (flyingDefendersNeeded + groundDefendersNeeded == 0)
        {
            // if a defense squad for this region exists, empty it
            if (_squadData.squadExists(squadName.str()))
            {
				_squadData.getSquad(squadName.str()).clear();
			}
            
            // and return, nothing to defend here
            continue;
        }
        else 
        {
            // if we don't have a squad assigned to this region already, create one
            if (!_squadData.squadExists(squadName.str()))
            {
                SquadOrder defendRegion(SquadOrderTypes::Defend, regionCenter, 32 * 30, "Defend Region!");
                _squadData.addSquad(squadName.str(), Squad(squadName.str(), defendRegion, BaseDefensePriority));
			}
        }

		/*int numEnemyFlyingInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return u->isFlying(); });
		int numEnemyGroundInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return !u->isFlying(); });*/

		// assign units to the squad
		UAB_ASSERT(_squadData.squadExists(squadName.str()), "Squad should exist: %s", squadName.str().c_str());
        Squad & defenseSquad = _squadData.getSquad(squadName.str());

		// Count static defense as air defenders.
		for (auto & unit : BWAPI::Broodwar->self()->getUnits()) {
			if ((unit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony) &&
				unit->isCompleted() && unit->isPowered() &&
				BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
			{
				flyingDefendersNeeded -= 1; //they are helpful, but might not be good enough for harassers
			}
		}
		flyingDefendersNeeded = std::max(flyingDefendersNeeded, 0);

		// Count static defense as ground defenders.
		// Cannons get counted as air and ground, which can be a mistake.
		for (auto & unit : BWAPI::Broodwar->self()->getUnits()) {
			if ((unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony) &&
				unit->isCompleted() && unit->isPowered() &&
				BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
			{
				groundDefendersNeeded -= 2; //better safe than sorry
			}
		}
		groundDefendersNeeded = std::max(groundDefendersNeeded, 0);

		updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefendersNeeded);
    }
	
    // for each of our defense squads, if there aren't any enemy units near the position, remove the squad
	// TODO partially overlaps with "is enemy in region check" above
	for (const auto & kv : _squadData.getSquads())
	{
		const Squad & squad = kv.second;
		const SquadOrder & order = squad.getSquadOrder();

		if (order.getType() != SquadOrderTypes::Defend || squad.isEmpty())
		{
			continue;
		}

		bool enemyUnitInRange = false;
		for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (unit->getPosition().getDistance(order.getPosition()) < order.getRadius())
			{
				enemyUnitInRange = true;
				break;
			}
		}

		if (!enemyUnitInRange)
		{
			_squadData.getSquad(squad.getName()).clear();
		}
	}
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded)
{
	// if there's nothing left to defend, clear the squad
	if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
	{
		defenseSquad.clear();
		return;
	}

	bool pullWorkers = shouldPullWorkers(defenseSquad.getSquadOrder().getPosition());
	if (!pullWorkers) {
		defenseSquad.releaseWorkers(); //release workers if it's not a case where we should pull them
	}
	
	const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();
	size_t flyingDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackAir);
	size_t groundDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackGround);

	bool pullSpecialUnits = flyingDefendersNeeded + groundDefendersNeeded >= 12; //pulls defilers
	//note, that if we wanted to do protoss, high templars should also be pulled. see findClosestDefender for that
	//we presume such units are not produced en-masse, i.e., there are only a few defilers on the map.
	//if there were many, many defilers, it is possible that all of them could be pulled to one location
	//and count as flying defenders

	// add flying defenders if we still need them
	size_t flyingDefendersAdded = 0;
	while (flyingDefendersNeeded > flyingDefendersInSquad + flyingDefendersAdded)
	{
		BWAPI::Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), true, false, pullSpecialUnits);

		// if we find a valid flying defender, add it to the squad
		if (defenderToAdd)
		{
			_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			++flyingDefendersAdded;
		}
		// otherwise we'll never find another one so break out of this loop
		else
		{
			break;
		}
	}

	// add ground defenders if we still need them
	size_t groundDefendersAdded = 0;
	while (groundDefendersNeeded > groundDefendersInSquad + groundDefendersAdded)
	{
		BWAPI::Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), false, pullWorkers, pullSpecialUnits);
		if (WorkerManager::Instance().getNumMineralWorkers() <= 3) {
			pullWorkers = false; //always keep at least 3 drones on minerals
		}

		// If we find a valid ground defender, add it.
		if (defenderToAdd)
		{
			if (defenderToAdd->getType().isWorker())
			{
				WorkerManager::Instance().setCombatWorker(defenderToAdd);
			}
			_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			++groundDefendersAdded;
		}
		// otherwise we'll never find another one so break out of this loop
		else
		{
			break;
		}
	}

	/* Assign an overlord to the squad if we need one */
	if (InformationManager::Instance().enemyHasCloakTech() && BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {

		bool squadHasUnits = false;
		for (auto & unit : defenseSquad.getUnits()) {
			if (unit && unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) {
				return;
			}
			squadHasUnits = true;
		}

		if (!squadHasUnits) return; //no point assigning an overlord to an empty squad

		//try to find an overlord
		BWAPI::Unit overlord = findClosestOverlord(defenseSquad, defenseSquad.getSquadOrder().getPosition());
		if (overlord) {
			_squadData.assignUnitToSquad(overlord, defenseSquad);
		}
	}
}

bool CombatCommander::shouldPullWorkers(BWAPI::Position center) {
	if (!Config::Micro::WorkersDefendRush) return false;
	const int concernRadius = 9*32;
	int proxyConcernRadius = 600;
	auto mainBase = InformationManager::Instance().getMyMainBaseLocation();
	if (mainBase) {
		if (BWTA::getRegion(center) == BWTA::getRegion(mainBase->getPosition())) {
			proxyConcernRadius = 1200;
		}
	}
	int enemyUnits = 0;
	bool concerning = false;
	for (auto unit : BWAPI::Broodwar->getUnitsInRadius(center, proxyConcernRadius, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsFlying))
	{
		auto type = unit->getType();
		if (unit->getDistance(center) < concernRadius) {
			if ((type == BWAPI::UnitTypes::Zerg_Zergling ||
				type == BWAPI::UnitTypes::Terran_Marine))
			{
				++enemyUnits;
			}
			//no good until drones can micro better against zealots
			else if (type == BWAPI::UnitTypes::Protoss_Zealot) {
				enemyUnits += 2;
			}
			else if (type.isWorker() && unit->isAttacking()) {
				++enemyUnits;
			}
		} 

		//no point pulling workers once the cannon is finished...
		if (unit->getDistance(center) < proxyConcernRadius) {
			if (type == BWAPI::UnitTypes::Protoss_Photon_Cannon && unit->isCompleted()) return false;

			if ((type == BWAPI::UnitTypes::Protoss_Pylon ||
				type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				type == BWAPI::UnitTypes::Terran_Bunker ||
				(type == BWAPI::UnitTypes::Terran_Barracks && !unit->isFlying()) ||
				(type == BWAPI::UnitTypes::Protoss_Gateway && unit->isPowered())) && unit->getDistance(center) < proxyConcernRadius)
			{
				concerning = true;
			}
		}
	}
	if (concerning) return true;

	int ourUnits = 0;
	int staticDefense = 0;
	for (auto unit : BWAPI::Broodwar->getUnitsInRadius(center, 10 * 32, BWAPI::Filter::IsAlly)) {
		auto type = unit->getType();
		if (!type.isWorker() && UnitUtil::CanAttackGround(unit))
		{
			if (type.isBuilding()) {
				++staticDefense;
			}
			else {
				++ourUnits;
			}
		}
	}

	//if there's a lot of enemy units, workers are done for - harvest till the end
	if (enemyUnits <= 8 && enemyUnits > 3 * staticDefense + ourUnits) return true;

	return false;
}

void CombatCommander::updateSpecOpsSquad()
{
	if (_combatUnits.empty())
	{
		return;
	}

	if (!_squadData.squadExists("SpecOps"))
	{
		return;
	}

	Squad & specOpsSquad = _squadData.getSquad("SpecOps");
	const BWAPI::Unitset & squadUnits = specOpsSquad.getUnits();

	////////////
	for (auto & unit : _combatUnits)
	{
		// get every unit of a lower priority and put it into an attack squad
		bool isOverlord = unit->getType() == BWAPI::UnitTypes::Zerg_Overlord;

		//sunken colonies and defense buildings are assigned to the specOps squad
		if (unit->getType().isBuilding() &&
			_squadData.canAssignUnitToSquad(unit, specOpsSquad))
		{
			_squadData.assignUnitToSquad(unit, specOpsSquad);
		}
		/*else if (unit matches desired unit composition &&
			_squadData.canAssignUnitToSquad(unit, specOpsSquad)) {
			_squadData.assignUnitToSquad(unit, specOpsSquad);
			
		}*/
	}
	////////////

	/* PSEUDOCODE
	//verify what the spec ops squad's mission is
	//assign units appropriately
	if (harass) {
		getHarassComposition(); 
		assignUnits
		set order position to harass target
	} else if (escort) { //detect if we're expanding
		try to find either 1 muta, 2 hydras, or 6 lings... and an overlord for detection, to escort a worker to a base. 
		note: ground units should be loaded inside the overlord if possible
		set order position to expansion target
	}
	*/




	//specOpsSquad.clear();

	//_squadData.assignUnitToSquad(unit, specOpsSquad);

}

// Choose a defender to join the base defense squad.
BWAPI::Unit CombatCommander::findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender, bool pullWorkers, bool pullSpecialUnits)
{
	BWAPI::Unit closestDefender = nullptr;
	int minDistance = 99999;

	for (auto & unit : _combatUnits) 
	{
		if (pullSpecialUnits && unit->getType() == BWAPI::UnitTypes::Zerg_Defiler) {
			//don't continue
		} 
		else if ((flyingDefender && !UnitUtil::CanAttackAir(unit)) ||
			(!flyingDefender && !UnitUtil::CanAttackGround(unit)))
        {
            continue;
        }

        if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
        {
            continue;
        }

		int dist = unit->getDistance(pos);

		// Pull workers only if requested, and not from distant bases.
		if (unit->getType().isWorker())
        {
			if (!pullWorkers || (dist > 1200)) continue;
			if (!WorkerManager::Instance().shouldPullWorker(unit)) continue;
		}

		if (dist < minDistance)
        {
            closestDefender = unit;
            minDistance = dist;
        }
	}

	return closestDefender;
}

// Choose a defender to join the base defense squad.
BWAPI::Unit CombatCommander::findClosestOverlord(const Squad & defenseSquad, BWAPI::Position pos)
{
	BWAPI::Unit closestDefender = nullptr;
	int minDistance = 99999;
	Squad & idleSquad = _squadData.getSquad("Idle");
	for (auto & unit : idleSquad.getUnits())
	{
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Overlord)
		{
			continue;
		}

		if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
		{
			continue;
		}

		int dist = unit->getDistance(pos);

		if (dist < minDistance)
		{
			closestDefender = unit;
			minDistance = dist;
		}
	}

	return closestDefender;
}

// If we should, add 1 overlord to the survey squad, at most one per 5 minutes
void CombatCommander::updateSurveySquad()
{
	if (!_squadData.squadExists("Survey"))
	{
		return;
	}

	Squad & surveySquad = _squadData.getSquad("Survey");

	bool eTerran = BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran;
	bool baseFound = (InformationManager::Instance().getEnemyMainBaseLocation() != NULL);

	int sSquadSize = surveySquad.getUnits().size();
	if ((BWAPI::Broodwar->getFrameCount() % 5 * 60 * 20 <= 10
		)
		&& ((sSquadSize < 2 && !eTerran && !baseFound) || (eTerran && surveySquad.isEmpty()))
		&& _squadData.squadExists("Idle")) {
		Squad & idleSquad = _squadData.getSquad("Idle");
		const BWAPI::Unit * myOverlord = nullptr;
		for (auto & unit : idleSquad.getUnits())
		{
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord &&
				_squadData.canAssignUnitToSquad(unit, surveySquad))
			{
				myOverlord = &unit;
				break;
			}
		}
		if (myOverlord)
		{
			_squadData.assignUnitToSquad(*myOverlord, surveySquad);
		}
	}

	if (baseFound && sSquadSize > 1) { //remove an overlord if we've already found the enemy base
		for (auto unit : surveySquad.getUnits()) {
			surveySquad.removeUnit(unit);
			break;
		}
	}


	int time = BWAPI::Broodwar->elapsedTime();
	if (BWAPI::Broodwar->elapsedTime() > 6 * 60) {
		surveySquad.setSquadOrder(SquadOrder(SquadOrderTypes::Explore, MapGrid::Instance().getLeastExplored(), 100, "Scout the Map"));
	}
	else {
		surveySquad.setSquadOrder(SquadOrder(SquadOrderTypes::Survey, BWAPI::Position(0,0), 100, "Survey the World"));
	}
}

// NOTE This implementation is kind of cheesy. Orders ought to be delegated to a squad.
void CombatCommander::loadOrUnloadBunkers()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	for (auto & bunker : BWAPI::Broodwar->self()->getUnits())
	{
		if (bunker->getType() == BWAPI::UnitTypes::Terran_Bunker)
		{
			// Are there enemies close to the bunker?
			bool enemyIsNear = false;

			// 1. Is any enemy unit within a small radius?
			BWAPI::Unitset enemiesNear = BWAPI::Broodwar->getUnitsInRadius(bunker->getPosition(), 12 * 32,
				BWAPI::Filter::IsEnemy);
			if (enemiesNear.empty())
			{
				// 2. Is a fast enemy unit within a wider radius?
				enemiesNear = BWAPI::Broodwar->getUnitsInRadius(bunker->getPosition(), 18 * 32,
					BWAPI::Filter::IsEnemy &&
						(BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture ||
						 BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Mutalisk)
					);
				enemyIsNear = !enemiesNear.empty();
			}
			else
			{
				enemyIsNear = true;
			}

			if (enemyIsNear)
			{
				// Load one marine at a time if there is free space.
				if (bunker->getSpaceRemaining() > 0)
				{
					BWAPI::Unit marine = BWAPI::Broodwar->getClosestUnit(
						bunker->getPosition(),
						BWAPI::Filter::IsOwned && BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Marine,
						12 * 32);
					if (marine)
					{
						bunker->load(marine);
					}
				}
			}
			else
			{
				bunker->unloadAll();
			}
		}
	}
}

// Scan enemy cloaked units.
void CombatCommander::doComsatScan()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Comsat_Station) == 0)
	{
		return;
	}

	// Does the enemy have undetected cloaked units that we may be able to engage?
	for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->isVisible() &&
			(!unit->isDetected() || unit->getOrder() == BWAPI::Orders::Burrowing) &&
			unit->getPosition().isValid())
		{
			// At most one scan per call. We don't check whether it succeeds.
			(void) Micro::SmartScan(unit->getPosition());
			break;
		}
	}
}

// Get our money back for stuff that is about to be destroyed.
// TODO does this work for protoss buildings?
void CombatCommander::cancelDyingBuildings()
{
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType().isBuilding() && !unit->isCompleted() && unit->isUnderAttack() && unit->getHitPoints() < 30)
		{
			if (unit->isMorphing() && unit->canCancelMorph()) {
				unit->cancelMorph();
			}
			else if (unit->isBeingConstructed() && unit->canCancelConstruction()) {
				unit->cancelConstruction();
			}
		}
	}
}

BWAPI::Position CombatCommander::getDefendLocation()
{
	return BWTA::getRegion(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition())->getCenter();
}

// Release workers from the attack squad.
void CombatCommander::releaseWorkers()
{
	Squad & groundSquad = _squadData.getSquad("MainAttack");
	groundSquad.releaseWorkers();
}

void CombatCommander::drawSquadInformation(int x, int y)
{
	_squadData.drawSquadInformation(x, y);
}

BWAPI::Position CombatCommander::getHuntLocation() {
	//support the main attack squad
	BWAPI::Unitset enemyUnitsInArea;
	MapGrid::Instance().GetUnits(enemyUnitsInArea, _squadData.getSquad("Flying").calcCenter(), 800, false, true);

	for (auto & unit : enemyUnitsInArea)
	{
		if (!unit || !unit->exists() || unit->getType() == BWAPI::UnitTypes::Zerg_Larva) continue;
		//hunt down all nearby units until we get a few more comrades to join
		if (_squadData.getSquad("Flying").getUnits().size() < 7) {
			return unit->getPosition();
		}
		//hunt down units that will give ground forces/bases trouble
		else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
			unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Reaver ||
			unit->getType() == BWAPI::UnitTypes::Terran_Wraith ||
			unit->getType() == BWAPI::UnitTypes::Terran_Vulture ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Corsair) {
			return unit->getPosition();
		}
	}

	//found nothing of interest, resume normal function
	return getMainAttackLocation();
}

BWAPI::Position CombatCommander::getMainAttackLocation()
{
	// If we're defensive, try to find a front line to hold.
	if (!_goAggressive)
	{
		BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
		if (natural && BWAPI::Broodwar->self() == InformationManager::Instance().getBaseOwner(natural))
		{
			return natural->getPosition();
		}

		BWTA::BaseLocation * main = InformationManager::Instance().getMyMainBaseLocation();
		if (main)
		{
			return main->getPosition();
		}

		// we should always have a main base (even if not yet occupied), but if not,
		// there's no harm in falling through and going aggressive.
	}

	// Otherwise we are aggressive. Try to find a spot to attack.

	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());

	// for each start location in the level
	if (!_allExplored && !enemyBaseLocation)
	{
		for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
		{
			// if we haven't explored it yet
			if (!BWAPI::Broodwar->isExplored(startLocation->getTilePosition()))
			{
				// assign a unit to go scout it
				return BWAPI::Position(startLocation->getTilePosition());
			}
		}
		_allExplored = true;
	}

	/* Search for farthest enemy base first. Least likely to be defended. */
	BWAPI::Position farthestEnemyBasePosition; 
	double distance = -1; 
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (InformationManager::Instance().getBaseOwner(base) != BWAPI::Broodwar->enemy() ||
			base == enemyBaseLocation)
		{
			continue;
		}

		BWAPI::Position enemyBasePosition = base->getPosition();
		if (enemyBasePosition.getDistance(enemyBaseLocation->getPosition()) < distance) {
			continue;
		}

		double currentDistance = enemyBasePosition.getDistance(enemyBaseLocation->getPosition());

		if (currentDistance < 25 * 32 && InformationManager::Instance().getNumBases(BWAPI::Broodwar->enemy()) < 3) { 
			//probably the heavily defended natural; we don't care about the nat (for now)
			continue; 
		}

		// get all known enemy units in the area
		BWAPI::TilePosition enemyBaseTilePosition(enemyBasePosition);
		if (BWAPI::Broodwar->isVisible(enemyBaseTilePosition)) {
			BWAPI::Unitset enemyUnitsInArea;
			MapGrid::Instance().GetUnits(enemyUnitsInArea, enemyBasePosition, 200, false, true);

			bool foundDepot = false;
			for (auto & unit : enemyUnitsInArea)
			{
				if (unit->getType().isResourceDepot() || unit->getType() == BWAPI::UnitTypes::Zerg_Lair ||
					unit->getType() == BWAPI::UnitTypes::Zerg_Hive)
				{
					foundDepot = true;
					break;
				}
			}

			if (!foundDepot) {
				BWAPI::Broodwar->printf("On site, no enemy depot. Inferring dead base...");
				InformationManager::Instance().baseLost(enemyBaseTilePosition);
			}
		}

		if (distance < currentDistance) {
			farthestEnemyBasePosition = enemyBasePosition;
			distance = currentDistance;
		}
	}
	if (distance > -1) return farthestEnemyBasePosition; 

	// First choice: Attack the enemy main unless we think it's empty.
	if (enemyBaseLocation)
	{
		BWAPI::Position enemyBasePosition = enemyBaseLocation->getPosition();

		// If the enemy base hasn't been seen yet, go there.
		if (!BWAPI::Broodwar->isExplored(BWAPI::TilePosition(enemyBasePosition)))
		{
			return enemyBasePosition;
		}

		// get all known enemy units in the area
		BWAPI::Unitset enemyUnitsInArea;
		MapGrid::Instance().GetUnits(enemyUnitsInArea, enemyBasePosition, 800, false, true);

		for (auto & unit : enemyUnitsInArea)
		{
			if (unit->getType() != BWAPI::UnitTypes::Zerg_Overlord &&
				unit->getType() != BWAPI::UnitTypes::Zerg_Larva)
			{
				// Enemy base is not empty: Nothing interesting is in the enemy base area.
				return enemyBasePosition;
			}
		}
	}

	// Second choice: Attack known enemy buildings
	for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
	{
		const UnitInfo & ui = kv.second;

		if (ui.type.isBuilding() && ui.lastPosition != BWAPI::Positions::None)
		{
			return ui.lastPosition;
		}
	}

	// Third choice: Attack visible enemy units that aren't overlords
	// TODO should distinguish depending on the squad's air and ground weapons
	//       don't send zerglings after air units, etc.
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Larva)
		{
			continue;
		}

		if (UnitUtil::IsValidUnit(unit) && unit->isVisible())
		{
			return unit->getPosition();
		}
	}

	// Fourth choice: We can't see anything so explore the map attacking along the way
	return MapGrid::Instance().getLeastExplored();
}

BWAPI::Position CombatCommander::getSurveyLocation()
{
	BWTA::BaseLocation * ourBaseLocation = InformationManager::Instance().getMyMainBaseLocation();
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	// If it's a 2-player map, or we miraculously know the enemy base location, that's it.
	if (enemyBaseLocation)
	{
		//send the overlord home if we've found the enemy base already
		if (ourBaseLocation && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran) {
			return ourBaseLocation->getPosition();
		}

		return enemyBaseLocation->getPosition();
	}

	// Otherwise just pick the closest base that's not ours
	double distance = INT32_MAX;
	BWAPI::Position position(-1, -1);
	for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
	{
		if (startLocation && startLocation != ourBaseLocation && startLocation->getPosition().isValid())
		{
			//if we haven't explored it yet
			if (!BWAPI::Broodwar->isExplored(startLocation->getTilePosition())) {
				auto pos = startLocation->getPosition();
				auto dist = pos.getDistance(ourBaseLocation->getPosition());
				if (dist < distance) {
					distance = dist;
					position = pos;
				}
			}
		}
	}
	if (distance < INT32_MAX) {
		return position;
	}

	if (ourBaseLocation && ourBaseLocation->getPosition().isValid()) {
		UAB_ASSERT(false, "map seems to have only 1 base");
		return ourBaseLocation->getPosition();
	}
	else {
		UAB_ASSERT(false, "map seems to have no bases");
		return BWAPI::Position(0, 0);
	}
}

// Choose one worker to pull for scout defense.
BWAPI::Unit CombatCommander::findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target)
{
    UAB_ASSERT(target != nullptr, "target was null");

    if (!target)
    {
        return nullptr;
    }

    BWAPI::Unit closestMineralWorker = nullptr;
	int closestDist = Config::Micro::ScoutDefenseRadius + 128;    // more distant workers do not get pulled
    
	for (auto & unit : unitsToAssign)
	{
		if (unit->getType().isWorker() && WorkerManager::Instance().isFree(unit))
		{
			int dist = unit->getDistance(target);
			if (unit->isCarryingMinerals())
			{
				dist += 96;
			}

            if (dist < closestDist)
            {
                closestMineralWorker = unit;
                dist = closestDist;
            }
		}
	}

    return closestMineralWorker;
}

CombatCommander & CombatCommander::Instance()
{
	static CombatCommander instance;
	return instance;
}
