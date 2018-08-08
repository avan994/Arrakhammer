#include "Squad.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

Squad::Squad()
    : _name("Default")
	, _combatSquad(false)
	, _attackAtMax(false)
    , _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(0)
	, _lastFormedSwitch(-100)
{
    int a = 10;   // only you can prevent linker errors
}

Squad::Squad(const std::string & name, SquadOrder order, size_t priority) 
	: _name(name)
	, _combatSquad(name != "Idle")
	, _attackAtMax(false)
	, _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(priority)
	, _order(order)
{
}

Squad::~Squad()
{
    clear();
}

void Squad::update()
{
	// update all necessary unit information within this squad
	updateUnits();

	if (_units.empty()) //nothing to do
	{
		return;
	}

	//The survey overlord scouts the base and then starts going all over the map
	if (_order.getType() == SquadOrderTypes::Survey && BWAPI::Broodwar->getFrameCount() < 24)
	{
		BWAPI::Unit surveyor = *(_units.begin());
		if (surveyor && surveyor->exists())
		{
			_overlordManager.execute(_order);
		}


		return;
	} //The SpecOps squad is delegated special authority to handle unusual tasks
	else if (_order.getType() == SquadOrderTypes::SpecOps) {
		_staticDefenseManager.execute(_order); //static defense; doesn't really use the order, but
		//is grouped in with the SpecOps squad

		handleSpecOps();
		return;
	}

	SquadOrder order = _order;
	// determine whether or not we should regroup
	bool needToRegroup = needsToRegroup();
    
	// draw some debug info
	if (Config::Debug::DrawSquadInfo && _order.getType() == SquadOrderTypes::Attack) 
	{
		BWAPI::Broodwar->drawTextScreen(200, 350, "%c%s", white, _regroupStatus.c_str());
	}

	//draw how long the status has been on
	auto color = BWAPI::Colors::Green;
	if (needToRegroup) color = BWAPI::Colors::Red;
	int offset = 10;
	if (_name == "Flying") {
		BWAPI::Broodwar->drawBoxScreen(240, 150, 240 + std::min(120, (BWAPI::Broodwar->getFrameCount() - _lastRetreatSwitch) / 24), 150+offset, color, true);
		BWAPI::Broodwar->drawBoxScreen(240, 150, 280, 150+offset, color, false);
	}
	else if (_name == "MainAttack") {
		BWAPI::Broodwar->drawBoxScreen(240, 170, 240 + std::min(120, (BWAPI::Broodwar->getFrameCount() - _lastRetreatSwitch) / 24), 170+offset, color, true);
		BWAPI::Broodwar->drawBoxScreen(240, 170, 280, 170+offset, color, false);
	}

	// if we do need to regroup, do it
	if (needToRegroup)
	{
		BWAPI::Position regroupPosition = calcRegroupPosition();

        if (Config::Debug::DrawCombatSimulationInfo)
        {
		    BWAPI::Broodwar->drawTextScreen(200, 150, "REGROUP");
        }

		if (Config::Debug::DrawSquadInfo)
		{
			BWAPI::Broodwar->drawCircleMap(regroupPosition.x, regroupPosition.y, 30, BWAPI::Colors::Purple, true);
		}

		order = SquadOrder(SquadOrderTypes::Regroup, regroupPosition, 200, "Regroup");
		//regrouping is now an order
		// NOTE Detectors and transports do not regroup.
	}
	else // otherwise, execute micro
	{
		_transportManager.update();
	}


	/* ------------------------------------ */
	//Concave forming processing

	//if any one manager is attacked or attacking, all squad don't form
	if (_name == "MainAttack") {
		if (_rangedManager._fighting ||
			_meleeManager._fighting) {
			_meleeManager._doNotForm = true;
			_rangedManager._doNotForm = true;
			BWAPI::Broodwar->drawTextScreen(200, 320, "%s", "Fighting, stop formation");
		}
		else {
			_meleeManager._doNotForm = false;
			_rangedManager._doNotForm = false;

			//define whether each manager is formed already or is empty
			bool formed = (_meleeManager._formed || _meleeManager.getUnits().size() == 0) && (_rangedManager._formed || _rangedManager.getUnits().size() == 0);
			int switchTime = 24 * 8;
			BWAPI::Broodwar->drawTextScreen(200, 300, "%d %d %d %d", _meleeManager._formed, _meleeManager.getUnits().size() == 0, _rangedManager._formed, _rangedManager.getUnits().size() == 0);
			// we should not form again unless 4 seconds have passed since a finished formation
			if (formed)
			{
				if (BWAPI::Broodwar->getFrameCount() - _lastFormedSwitch > switchTime)
				{
					_lastFormedSwitch = BWAPI::Broodwar->getFrameCount();
				}
				BWAPI::Broodwar->drawTextScreen(200, 340, "%s", "Finished Formation");
				_meleeManager._doNotForm = true;
				_rangedManager._doNotForm = true;
			}
			else
			{
				if (BWAPI::Broodwar->getFrameCount() - _lastFormedSwitch <= switchTime)
				{
					_meleeManager._doNotForm = true;
					_rangedManager._doNotForm = true;
				}
				else {
					BWAPI::Broodwar->drawTextScreen(200, 300, "%s", "Forming");
				}
			}
		}
	}
	/* ----------------------------------- */

	BWAPI::Unit unitClosest(unitClosestToEnemy());
	_detectorManager.setUnitClosestToEnemy(unitClosest);
	_detectorManager.execute(order);

	//regardless of what the order is, engage nearby enemies if necessary
	_meleeManager.execute(order);
	_rangedManager.execute(order);
	_lurkerManager.execute(order);
	_tankManager.execute(order);
	_medicManager.execute(order);

	_overlordManager.setUnitClosestToEnemy(unitClosest);
	_overlordManager.execute(order);

	_casterManager.setUnitClosestToEnemy(unitClosest);
	_casterManager.execute(order);
}

bool Squad::isEmpty() const
{
    return _units.empty();
}

size_t Squad::getPriority() const
{
    return _priority;
}

void Squad::setPriority(const size_t & priority)
{
    _priority = priority;
}

void Squad::updateUnits()
{
	setAllUnits();
	setNearEnemyUnits();
	addUnitsToMicroManagers();
}

// Clean up the _units vector.
// Also notice and remember a few facts about the members of the squad.
// Note: Some units may be loaded in a bunker or transport and cannot accept orders.
//       Check unit->isLoaded() before issuing orders.
void Squad::setAllUnits()
{
	_hasAir = false;
	_hasGround = false;
	_hasAntiAir = false;
	_hasAntiGround = false;

	BWAPI::Unitset goodUnits;
	for (auto & unit : _units)
	{
		if (UnitUtil::IsValidUnit(unit))
		{
			goodUnits.insert(unit);

			if (unit->isFlying()) {
				_hasAir = true;
			}
			else {
				_hasGround = true;
			}

			if (UnitUtil::CanAttackAir(unit)) {
				_hasAntiAir = true;
			}

			if (UnitUtil::CanAttackGround(unit)) {
				_hasAntiGround = true;
			}
		}
	}
	_units = goodUnits;
}

void Squad::setNearEnemyUnits()
{
	_nearEnemy.clear();
	for (auto & unit : _units)
	{
		int x = unit->getPosition().x;
		int y = unit->getPosition().y;

		int left = unit->getType().dimensionLeft();
		int right = unit->getType().dimensionRight();
		int top = unit->getType().dimensionUp();
		int bottom = unit->getType().dimensionDown();

		_nearEnemy[unit] = unitNearEnemy(unit);

		if (Config::Debug::DrawSquadInfo) {
			BWAPI::Broodwar->drawBoxMap(x - left, y - top, x + right, y + bottom,
				(_nearEnemy[unit]) ? Config::Debug::ColorUnitNearEnemy : Config::Debug::ColorUnitNotNearEnemy);
		}
	}
}

void Squad::addUnitsToMicroManagers()
{
	BWAPI::Unitset meleeUnits;
	BWAPI::Unitset rangedUnits;
	BWAPI::Unitset detectorUnits;
	BWAPI::Unitset transportUnits;
	BWAPI::Unitset lurkerUnits;
    BWAPI::Unitset tankUnits;
    BWAPI::Unitset medicUnits;
	BWAPI::Unitset overlordUnits;
	BWAPI::Unitset casterUnits;
	BWAPI::Unitset staticDefenseUnits;

	//Note that the order in which the if statements are checked is very important!
	for (auto & unit : _units)
	{
		UAB_ASSERT(unit, "missing unit");
		if (unit->isCompleted() && unit->getHitPoints() > 0 && unit->exists())
		{
			if (unit->getType().isBuilding() && (unit->getType().groundWeapon() != BWAPI::WeaponTypes::None ||
				unit->getType().airWeapon() != BWAPI::WeaponTypes::None)) {
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
					unit->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony) {
					//BWAPI::Broodwar->printf("Added sunken!");
				}
				else {
					BWAPI::Broodwar->printf("Added unsupported unit type to static defense units!");
				}
				staticDefenseUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) {
				overlordUnits.insert(unit);
			}
            else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic)
            {
                medicUnits.insert(unit);
            }
			else if (unit->getType().isSpellcaster() ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Infested_Terran) {
				casterUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
			{
				lurkerUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
				unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
            {
                tankUnits.insert(unit);
            }   
			else if (unit->getType().isDetector() && !unit->getType().isBuilding())
			{
				detectorUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle || //overlords are separate
				unit->getType() == BWAPI::UnitTypes::Terran_Dropship)
			{
				transportUnits.insert(unit);
			}
			// TODO excludes some units: carriers, reavers, valkyries, corsairs
			else if ((unit->getType().groundWeapon().maxRange() > 64) ||
				(unit->getType().airWeapon().maxRange() > 64) ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
			{
				rangedUnits.insert(unit);
			}
			else if (unit->getType().isWorker())
			{
				// If this is a combat squad, then workers are melee units like any other,
				// but we have to tell WorkerManager about them.
				// If it's not a combat squad, WorkerManager owns them; don't add them to a micromanager.
				if (_combatSquad) {
					WorkerManager::Instance().setCombatWorker(unit);
					meleeUnits.insert(unit);
				}
			}
			else if (unit->getType().groundWeapon().maxRange() <= 32)
			{
				meleeUnits.insert(unit);
			}
			// Note: Some units may fall through and not be assigned.
		}
	}

	_staticDefenseManager.setUnits(staticDefenseUnits);
	_meleeManager.setUnits(meleeUnits);
	_rangedManager.setUnits(rangedUnits);
	_detectorManager.setUnits(detectorUnits);
	_transportManager.setUnits(transportUnits);
	_lurkerManager.setUnits(lurkerUnits);
	_tankManager.setUnits(tankUnits);
    _medicManager.setUnits(medicUnits);
	_overlordManager.setUnits(overlordUnits);
	_casterManager.setUnits(casterUnits);
}

// Calculates whether to regroup, aka retreat. Does combat sim if necessary.
bool Squad::needsToRegroup()
{
	// if we are not attacking, never regroup
	if (_units.empty() || (_order.getType() != SquadOrderTypes::Attack && _order.getType() != SquadOrderTypes::Defend))
	{
		_regroupStatus = std::string("No attackers or defenders available");
		return false;
	}

	// If we're nearly maxed and have good income or cash, don't retreat.
	if ((BWAPI::Broodwar->self()->supplyUsed() >= 350 &&
		(BWAPI::Broodwar->self()->minerals() > 1000 || WorkerManager::Instance().getNumMineralWorkers() > 12)) ||
		(BWAPI::Broodwar->self()->supplyUsed() >= 300 &&
		(InformationManager::Instance().getNumBases(BWAPI::Broodwar->self()) > 7 && //for zerg!
		WorkerManager::Instance().getNumMineralWorkers() > 40 )))
	{
		_attackAtMax = true;

		// Corny animation for attack at max just for zerg.
		if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {
			BWAPI::Broodwar->drawBoxScreen(0, 0, 640, 300, BWAPI::Colors::Red);
			double theta = (BWAPI::Broodwar->getFrameCount() / 50) % 16;
			BWAPI::Broodwar->drawEllipseScreen(550, 64, 2, 14 + int(6 * cos(theta)), BWAPI::Colors::Red, true);
			BWAPI::Broodwar->drawEllipseScreen(550, 64, 18 + int(10 * sin(theta)), 2, BWAPI::Colors::Red, true);

			char red = '\x08';
			BWAPI::Broodwar->drawTextScreen(480, 96, "%cUnleash the swarm's fury.", red);
		}

		_regroupStatus = std::string("Maxed. Banzai!");
		return false;
	}

	//don't regroup if we're close enough to the defense location
	if (_order.getType() == SquadOrderTypes::Defend) {
		//draw out the positions for debugging
		auto p1 = _units.getPosition();
		auto p2 = _order.getPosition();

		BWAPI::Broodwar->drawCircleMap(p2, 8 * 32 + (BWAPI::Broodwar->getFrameCount() % 20), BWAPI::Colors::Blue);
		BWAPI::Broodwar->drawTextMap(p2, "Defense Zone");
	}


	BWAPI::Unit unitClosest = unitClosestToEnemy();

	if (!unitClosest)
	{
		//remaining overlords/queens/whatever should run
		_regroupStatus = std::string("No closest unit");
		return true; 
	}

	if (_order.getType() == SquadOrderTypes::Defend) {
		//we really can't let the mutalisks suicide constantly
		if (unitClosest->getPosition().getDistance(_order.getPosition()) < 7 * 32) {
			return false;
		}
	}

	double score = 0;

	//do the SparCraft Simulation!
	CombatSimulation sim;
	
	int radius = (_name == "Flying") ? ((_units.size() >= 11 || BWAPI::Broodwar->elapsedTime() < 10 * 60) ? 14 * 32 : 17 * 32)
		: Config::Micro::CombatRegroupRadius;
	sim.setCombatUnits(unitClosest->getPosition(), radius, _name == "Flying");
	score = sim.simulateCombat();

    bool retreat = score < 0;
    int switchTime = 50;

	//stay in the fight longer if our melee squad is engaged
	int stayInFightTime = (_name == "MainAttack") ? (_meleeManager._fighting ? 100 : 0) 
		: (_name == "Flying") ? 100
		: 0; //100 will apply to the flying squad - mutalisks may dive in and out 

    bool waiting = false;

    // we should not attack or run too early after a previously decision
    if (retreat != _lastRetreatSwitchVal)
    {
        if (!retreat && (BWAPI::Broodwar->getFrameCount() - _lastRetreatSwitch < switchTime))
        {
            waiting = true;
            retreat = _lastRetreatSwitchVal;
        }
		else if (retreat && (BWAPI::Broodwar->getFrameCount() - _lastRetreatSwitch < stayInFightTime))
        {
			waiting = true;
			retreat = _lastRetreatSwitchVal;
		}
		else {
			waiting = false;
			_lastRetreatSwitch = BWAPI::Broodwar->getFrameCount();
			_lastRetreatSwitchVal = retreat;
		}
    }
	
	if (retreat)
	{
		_regroupStatus = std::string("Retreat");
	}
	else
	{
		_regroupStatus = std::string("Attack");
	}

	return retreat;
}

void Squad::setSquadOrder(const SquadOrder & so)
{
	_order = so;
}

bool Squad::containsUnit(BWAPI::Unit u) const
{
    return _units.contains(u);
}

void Squad::clear()
{
    for (auto & unit : getUnits())
    {
        if (unit->getType().isWorker())
        {
            WorkerManager::Instance().finishedWithWorker(unit);
        }
    }

    _units.clear();
}

bool Squad::unitNearEnemy(BWAPI::Unit unit)
{
	UAB_ASSERT(unit, "missing unit");

	BWAPI::Unitset enemyNear;

	MapGrid::Instance().GetUnits(enemyNear, unit->getPosition(), 400, false, true);

	return enemyNear.size() > 0;
}

BWAPI::Position Squad::calcCenter()
{
    if (_units.empty())
    {
        if (Config::Debug::DrawSquadInfo)
        {
            BWAPI::Broodwar->printf("Squad::calcCenter() called on empty squad");
        }
        return BWAPI::Position(0,0);
    }

	BWAPI::Position accum(0,0);
	for (auto & unit : _units)
	{
		accum += unit->getPosition();
	}
	return BWAPI::Position(accum.x / _units.size(), accum.y / _units.size());
}

BWAPI::Position Squad::calcRegroupPosition()
{
	BWAPI::Position regroup(0, 0);

	int minDist = 100000;

	// Retreat to the location of the squad unit not near the enemy which is
	// closest to the order position.
	// NOTE May retreat somewhere silly if the chosen unit was newly produced.
	//      Zerg sometimes retreats back and forth through the enemy when new
	//      zerg units are produced in bases on opposite sides.
	for (const auto unit : _units)
	{
		// Count combat units only. Bug fix originally thanks to AIL, it's been rewritten a bit since then.
		if (!_nearEnemy[unit] &&
			!unit->getType().isDetector() &&
			unit->getType() != BWAPI::UnitTypes::Terran_Medic &&
			unit->getPosition().isValid())    // excludes loaded units
		{
			int dist = unit->getDistance(_order.getPosition());
			if (dist < minDist)
			{
				// If the squad has any ground units, don't try to retreat to the position of an air unit
				// which is flying in a place that a ground unit cannot reach.
				if (!_hasGround || -1 != MapTools::Instance().getGroundTileDistance(unit->getPosition(), _order.getPosition()))
				{
					minDist = dist;
					regroup = unit->getPosition();
				}
			}
		}
	}

	// Failing that, retreat to a base we own.
	if (regroup == BWAPI::Position(0, 0))
	{
		// Retreat to the main base (guaranteed not null, even if the buildings were destroyed).
		BWTA::BaseLocation * base = InformationManager::Instance().getMyMainBaseLocation();

		// If the natural has been taken, retreat there instead.
		BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
		if (natural && InformationManager::Instance().getBaseOwner(natural) == BWAPI::Broodwar->self())
		{
			base = natural;
		}
		return BWTA::getRegion(base->getTilePosition())->getCenter();
	}
	return regroup;
}

BWAPI::Unit Squad::unitClosestToEnemy()
{
	BWAPI::Unit closest = nullptr;
	int closestDist = 100000;

	UAB_ASSERT(_order.getPosition().isValid(), "bad order position");

	for (auto & unit : _units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Observer ||
			unit->getType().isSpellcaster() || //ignore queens and defilers
			unit->getType() == BWAPI::UnitTypes::Zerg_Infested_Terran ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Broodling) 
		{
			continue;
		}

		// the distance to the order position
		int dist = 0;
		if (_hasGround)
		{
			// A ground or air-ground squad. Use ground distance.
			// It is -1 if no ground path exists.
			dist = MapTools::Instance().getGroundTileDistance(unit->getPosition(), _order.getPosition());
		}
		else
		{
			// An all-air squad. Use air distance (which is what unit->getDistance() gives).
			dist = unit->getDistance(_order.getPosition());
		}

		if (dist != -1 && dist < closestDist)
		{
			closest = unit;
			closestDist = dist;
		}
	}

	return closest;
}

int Squad::squadUnitsNear(BWAPI::Position p)
{
	int numUnits = 0;

	for (auto & unit : _units)
	{
		if (unit->getDistance(p) < 600)
		{
			numUnits++;
		}
	}

	return numUnits;
}

const BWAPI::Unitset & Squad::getUnits() const	
{ 
	return _units; 
} 

const SquadOrder & Squad::getSquadOrder()	const			
{ 
	return _order; 
}

void Squad::addUnit(BWAPI::Unit u)
{
	_units.insert(u);
}

void Squad::removeUnit(BWAPI::Unit u)
{
	//release it if it's a worker
	if (_combatSquad && u->getType().isWorker())
	{
		WorkerManager::Instance().finishedWithWorker(u);
	}

    _units.erase(u);
}

// Remove all workers from the squad, releasing them back to WorkerManager.
void Squad::releaseWorkers()
{
	UAB_ASSERT(_combatSquad, "Idle squad should not release workers");

	std::vector<BWAPI::Unit> workers;
	for (const auto unit : _units)
	{
		if (!unit || !unit->exists()) continue;

		if (unit->getType().isWorker())
		{
			workers.push_back(unit);
		}
	}

	for (auto unit : workers) {
		removeUnit(unit);
	}
}

const std::string & Squad::getName() const
{
    return _name;
}


void Squad::handleSpecOps() {
	SquadOrder order = _order;

	/* Determine what the order is for the SpecOps squad */

	/*if (busy) {
		maintain same order
	}
	else {
		if (expanding) {
			escortWorkers;
		}
		else if (gametime < 4:00) {
			harassWithLings;
		}
		else if (harass) {
			mutas
			lurkerdrop
			ling queen cc steal
		}
	}*/

	BWAPI::Unit unitClosest(unitClosestToEnemy());
	_detectorManager.setUnitClosestToEnemy(unitClosest);
	_detectorManager.execute(order);

	_meleeManager.execute(order);
	_rangedManager.execute(order);
	_lurkerManager.execute(order);
	_tankManager.execute(order);
	_medicManager.execute(order);

	_overlordManager.setUnitClosestToEnemy(unitClosest);
	_overlordManager.execute(order);
}