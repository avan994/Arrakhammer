#include "OverlordManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

OverlordManager::OverlordManager() : unitClosestToEnemy(nullptr) { }

void OverlordManager::executeMicro(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & overlords = getUnits();

	if (overlords.empty())
	{
		return;
	}

	//Verify overlord's movement speed
	if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace)) {
		overlordSpeed = 4 * BWAPI::UnitTypes::Zerg_Overlord.topSpeed();

		if (BWAPI::Broodwar->getFrameCount() % 60 * 24) { //reset this every so often if we have speed
			overlordThreatDetected = false;
		}
	}
	else {
		overlordSpeed = BWAPI::UnitTypes::Zerg_Overlord.topSpeed();
	}

	mainBasePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();

	searchRadius = 13 * 32; //default value
	if (order.getType() == SquadOrderTypes::Survey || order.getType() == SquadOrderTypes::Explore) {
		searchRadius = 15 * 32; //need survey overlords to be very aware
		/*int time = BWAPI::Broodwar->elapsedTime();
		int frames = BWAPI::Broodwar->getFrameCount();
		BWAPI::Broodwar->drawTextScreen(240, 20, "Overlord Time: %d:%d", time / 60, time % 60);
		BWAPI::Broodwar->drawTextScreen(240, 40, "Estimated Time From Frames: %d:%d", frames / 60 / 20, (frames % (60 * 20)) / 20);*/

		BWAPI::Position movePosition = order.getPosition();
		for (auto & overlord : overlords) {
			if (order.getType() == SquadOrderTypes::Survey) movePosition = getSurveyLocation(overlord);

			if (overlordThreatDetected && mainBasePosition) {
				cautiouslyTravel(overlord, mainBasePosition);
			}
			else {
				cautiouslyTravel(overlord, movePosition);
			}
		}
	}
	else if (order.getType() == SquadOrderTypes::Regroup || order.getType() == SquadOrderTypes::Attack ||
		order.getType() == SquadOrderTypes::Defend) { //overlord is detection support
		for (auto & overlord : overlords) {
			if (order.getType() == SquadOrderTypes::Defend) {
				BWAPI::Broodwar->drawCircleMap(overlord->getPosition(), 32, BWAPI::Colors::Orange);
			}
		}
		detect(overlords);
	}
	else if (order.getType() == SquadOrderTypes::SpecOps) { //for now, overlord's primary job is detection in SpecOps
		detect(overlords);
	}
	else if (order.getType() == SquadOrderTypes::Idle) {
		searchRadius = 10 * 32;
		idle(overlords);
	}
}

void OverlordManager::detect(const BWAPI::Unitset & overlords) {
	//detecting code:
	cloakedUnitMap.clear();
	BWAPI::Unitset cloakedUnits;

	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits()) //figure out targets
	{
		// conditions for targeting
		// TODO is this good enough for arbiters?
		if (!unit || !(unit->exists())) {
			continue;
		}
		if (unit->getType().hasPermanentCloak() ||     // dark templar, observer
			unit->getType().isCloakable() ||           // wraith, ghost
			unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			unit->isBurrowed() ||
			(unit->isVisible() && !unit->isDetected()))
		{
			cloakedUnits.insert(unit);
			cloakedUnitMap[unit] = false;
		}
	}

	for (auto & unit : cloakedUnits) {
		if (!unit || !(unit->exists())) {
			continue;
		}
		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 32, BWAPI::Colors::Brown);
	}

	for (auto & overlord : overlords)
	{
		if (!overlord) continue;
		// if we need to regroup, move the detectorUnit to that location
		if (unitClosestToEnemy && unitClosestToEnemy->getPosition().isValid())
		{
			cautiouslyTravel(overlord, unitClosestToEnemy->getPosition());
		}
		else {
			if (order.getType() == SquadOrderTypes::Defend) {
				auto u = closestCloakedUnit(cloakedUnits, overlord);
				if (u->getDistance(order.getPosition()) <= 20*32) cautiouslyTravel(overlord, u->getPosition());
			}
		}
	}
}

BWAPI::Unit OverlordManager::closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit)
{
	BWAPI::Unit closestCloaked = nullptr;
	double closestDist = 100000;

	for (auto & unit : cloakedUnits)
	{
		// if we haven't already assigned an detectorUnit to this cloaked unit
		if (!cloakedUnitMap[unit])
		{
			double dist = unit->getDistance(detectorUnit);

			if (!closestCloaked || (dist < closestDist))
			{
				closestCloaked = unit;
				closestDist = dist;
			}
		}
	}

	return closestCloaked;
}


/* Orders the overlord to move to the target destination cautiously.
 * TEMPORARY: Just a straight line right now!
 * FUTURE: Avoids enemy units that can harm it. Aborts mission and flees to nearest base/ranged unit
 * if it detects */ 
void OverlordManager::cautiouslyTravel(BWAPI::Unit overlord, BWAPI::Position destination) {
	//Micro::SmartMove(overlord, destination);
	//return;

	if (Micro::CheckSpecialCases(overlord)) return;

	if (!(destination.isValid())) {
		//UAB_ASSERT(false, "Destination invalid in cautiouslyTravel overlordmanager");
		return;
	}
	
	/* We're very close to the drop position! Rush there! */
	if (overlord->getDistance(destination) < 8 * 32 && order.getType() == SquadOrderTypes::Drop) {
		Micro::SmartMove(overlord, destination);
		return;
	}
	
	BWAPI::Unitset nearbyEnemies = overlord->getUnitsInRadius(searchRadius, BWAPI::Filter::IsEnemy &&
		BWAPI::Filter::Exists);
	
	//initializeVectors();
	for (auto & enemy : nearbyEnemies) {
		//updateVectors(overlord, enemy);

		BWAPI::UnitType type = enemy->getType();
		if (!UnitUtil::IsThreat(overlord, enemy, false)) continue;
		if (overlord->getDistance(enemy) < searchRadius - 2*32) {
			if (type.topSpeed() > overlordSpeed && enemy->isFlying()) { //emergency!
				overlordThreatDetected = true; //notify allies of threat!
				if (runToAlly(overlord, enemy)) return; //run to nearest ally!
				if (runToBase(overlord)) return;
			}

			//failing that, just run! 
			BWAPI::Position fleePosition(overlord->getPosition() + Micro::GetKiteVector(enemy, overlord));
			if (fleePosition.isValid()) {
				Micro::SmartMove(overlord, fleePosition);
			}
			else {
				if (runToBase(overlord)) return;
			}
		}
		else if (enemy->getDistance(destination) < overlord->getDistance(destination)) { //circle around it!
			Micro::SmartMove(overlord, overlord->getPosition() + Micro::GetTangentVector(overlord, enemy, destination));
			return;
		}
		break; //currently, only consider the first unit we find
	}
	//normalizeVectors();

	Micro::SmartMove(overlord, destination); 
}

/* Run to any nearby ally that can fight the attacker. */
bool OverlordManager::runToAlly(BWAPI::Unit overlord, BWAPI::Unit attacker) {
	if (!overlord || !attacker) {
		UAB_ASSERT(false, "no overlord/attacker in runToAlly funct");
		return false; //huh?!
	}

	BWAPI::Unitset nearbyAllies = overlord->getUnitsInRadius(8 * 32, BWAPI::Filter::IsAlly);
	if (nearbyAllies.empty()) return false;
	bool air = attacker->getType().isFlyer();

	for (auto & ally : nearbyAllies) {
		if ((!air && ally->getType().groundWeapon() != BWAPI::WeaponTypes::None) || 
			(air && ally->getType().airWeapon() != BWAPI::WeaponTypes::None)) {
			Micro::SmartMove(overlord, ally->getPosition() + Micro::GetKiteVector(attacker, overlord));
			return true;
		}
	}

	return false;
}

/* Run to the main base.
 * FUTURE: Run to the nearest allied base with defenses? */
bool OverlordManager::runToBase(BWAPI::Unit overlord) {
	if (!mainBasePosition) return false;
	if (overlordSpeed <= BWAPI::UnitTypes::Zerg_Overlord.topSpeed()) return false; //too slow to run to base!
	if (overlord->getDistance(mainBasePosition) < 10 * 32) return false; //already there!
	Micro::SmartMove(overlord, mainBasePosition);
	return true; //hopefully we always have a main base
}
/* ------------------ */
BWAPI::Position OverlordManager::getSurveyLocation(BWAPI::Unit unit)
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

	// if we've assigned the overlord to a base and it hasn't yet been explored
	// go there
	auto assignedBase = overlordToBase[unit];
	if (assignedBase && !BWAPI::Broodwar->isExplored(assignedBase->getTilePosition())) {
		return assignedBase->getPosition();
	}

	// Otherwise just pick the closest base to the overlord that's not ours
	double distance = INT32_MAX;
	BWAPI::Position position(-1, -1);
	BWTA::BaseLocation * selectedBase = NULL;
	for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
	{
		if (!(startLocation && startLocation->getPosition().isValid())) break; //critical error!
		if (startLocation == ourBaseLocation) continue;
		if (BWAPI::Broodwar->isExplored(startLocation->getTilePosition())) continue; //if we haven't explored it yet

		if (baseToOverlord[startLocation]) continue; //someone is exploring or already explored it - could have been us

		auto pos = startLocation->getPosition();
		auto dist = pos.getDistance(unit->getPosition());
		if (dist < distance) {
			distance = dist;
			position = pos;
			selectedBase = startLocation;
		}
	}
	if (distance < INT32_MAX) {
		baseToOverlord[selectedBase] = unit;
		overlordToBase[unit] = selectedBase;
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


/* Transport Manager Code */
/* -------------------------------------------- */
/* Orders the overlord to transport to the specified location within a range of 1. 
 * The calling function is responsible for ensuring the destination is a reasonable location. */
void OverlordManager::unloadAt(BWAPI::Unit overlord, BWAPI::Position destination)
{
	if (overlord->getType() != BWAPI::UnitTypes::Zerg_Overlord) return;

	// If I didn't finish unloading the troops, wait
	BWAPI::UnitCommand currentCommand(overlord->getLastCommand());
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All
		|| currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All_Position)
		&& overlord->getLoadedUnits().size() > 0)
	{
		return;
	}

	bool arrived = overlord->getDistance(destination) < 32;
	if ( (arrived || overlord->getHitPoints() < 100)
		&& overlord->canUnloadAtPosition(overlord->getPosition()))
	{
		BWAPI::UnitCommand currentCommand(overlord->getLastCommand());

		if (currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All || currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All_Position)
		{
			return;
		}

		overlord->unloadAll(overlord->getPosition());
	}
	else if (!arrived) {
		cautiouslyTravel(overlord, destination);
	}
}

void OverlordManager::idle(const BWAPI::Unitset & overlords) {
	/*for (auto & k : overlordToBase) {
		if (!k.first || !k.second) continue;
		BWAPI::Broodwar->drawLineMap(k.first->getPosition(), k.second->getPosition(), BWAPI::Colors::Blue);
	}
	for (auto & k : baseToOverlord) {
		if (!k.first || !k.second) continue;
		BWAPI::Broodwar->drawCircleMap(k.first->getPosition(), 24, BWAPI::Colors::Red);
		BWAPI::Broodwar->drawCircleMap(k.second->getPosition(), 24, BWAPI::Colors::Red);
		//BWAPI::Broodwar->drawLineMap(k.first->getPosition(), k.second->getPosition(), BWAPI::Colors::Red);
	}*/

	//idle overlords will only randomly recompute every few seconds or so
	if (BWAPI::Broodwar->getFrameCount() % 100 >= 10) return;

	bool hasSpeed = overlordSpeed > BWAPI::UnitTypes::Zerg_Overlord.topSpeed();

	bool enemyBaseKnown = InformationManager::Instance().getEnemyMainBaseLocation() != NULL;
	BWAPI::Broodwar->getStartLocations();
	//clean up the vectors
	for (auto base : BWTA::getBaseLocations()) {
		if (!base) continue;
		auto assignedOverlord = baseToOverlord[base];
		auto owner = InformationManager::Instance().getBaseOwner(base);

		//erase the mapping if the overlord is dead, or the base belongs to an enemy
		if (assignedOverlord && (!overlords.contains(assignedOverlord) || //must have been stolen by a higher priority squad
			owner == BWAPI::Broodwar->enemy() || InformationManager::Instance().isEnemyBuildingInRegion(base->getRegion()) ||
			!assignedOverlord->exists() || assignedOverlord->getHitPoints() <= 0)) {
			overlordToBase.erase(assignedOverlord);
			baseToOverlord.erase(base);
		}
	}

	BWAPI::Position randomOffset(32 * (rand() % 8), 32 * (rand() % 8));
	for (auto & overlord : overlords) {
		if (!overlord) continue;
		BWAPI::Position movePosition(mainBasePosition);

		if (overlordToBase[overlord]) {
			auto base = overlordToBase[overlord];
			if (InformationManager::Instance().getBaseOwner(base) != BWAPI::Broodwar->enemy()) {
				movePosition = base->getPosition();
			}
			else {
				overlordToBase.erase(overlord);
				baseToOverlord.erase(base);
			}
		}
		else {
			for (auto & base : BWTA::getBaseLocations()) {
				auto assignedOverlord = baseToOverlord[base];
				auto owner = InformationManager::Instance().getBaseOwner(base);
				
				/*if (assignedOverlord)
				//erase the mapping if the overlord is dead, or the base belongs to an enemy
				if (assignedOverlord && (!overlords.contains(assignedOverlord) || //must have been stolen by a higher priority squad
					owner == BWAPI::Broodwar->enemy() || InformationManager::Instance().isEnemyBuildingInRegion(base->getRegion()) ||
					!assignedOverlord->exists() || assignedOverlord->getHitPoints() <= 0)) {
					overlordToBase.erase(overlord);
					baseToOverlord.erase(base);
				}
				*/
				//check again

				if (assignedOverlord) continue;
				if (owner == BWAPI::Broodwar->enemy()) continue;
				
				if (owner == BWAPI::Broodwar->self() || //1 overlord per base
					(hasSpeed && overlords.size() > 12) || //send overlord to scout all expos if we're doing ok
					InformationManager::Instance().isBaseReserved(base)  //send overlord to intended expansions
					) { //send overlord to scout all expos if we're doing ok
					overlordToBase[overlord] = base;
					baseToOverlord[base] = overlord;
					movePosition = base->getPosition();
					break;
				}
			}
		}

		//now move the overlord
		int leashDistance = overlordThreatDetected ? 32 : 5 * 32;
		if (overlord->getDistance(movePosition) > leashDistance) {
			cautiouslyTravel(overlord, movePosition);
		}
		else if (rand() % 2) {
			BWAPI::Position randomPosition(32 * (rand() % 8 - 4), 32 * (rand() % 8 - 4));

			cautiouslyTravel(overlord, overlord->getPosition() + randomOffset);
		}
	}
}
//mostly appears not useful for what I want
/*

void OverlordManager::calculateMapEdgeVertices()
{
BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

if (!enemyBaseLocation)
{
return;
}

const BWAPI::Position basePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
const std::vector<BWAPI::TilePosition> & closestTobase = MapTools::Instance().getClosestTilesTo(basePosition);

std::set<BWAPI::Position> unsortedVertices;

int minX = std::numeric_limits<int>::max(); int minY = minX;
int maxX = std::numeric_limits<int>::min(); int maxY = maxX;

//compute mins and maxs
for (auto &tile : closestTobase)
{
if (tile.x > maxX) maxX = tile.x;
else if (tile.x < minX) minX = tile.x;

if (tile.y > maxY) maxY = tile.y;
else if (tile.y < minY) minY = tile.y;
}

_minCorner = BWAPI::Position(minX, minY) * 32 + BWAPI::Position(16, 16);
_maxCorner = BWAPI::Position(maxX, maxY) * 32 + BWAPI::Position(16, 16);

//add all(some) edge tiles!
for (int _x = minX; _x <= maxX; _x += 5)
{
unsortedVertices.insert(BWAPI::Position(_x, minY) * 32 + BWAPI::Position(16, 16));
unsortedVertices.insert(BWAPI::Position(_x, maxY) * 32 + BWAPI::Position(16, 16));
}

for (int _y = minY; _y <= maxY; _y += 5)
{
unsortedVertices.insert(BWAPI::Position(minX, _y) * 32 + BWAPI::Position(16, 16));
unsortedVertices.insert(BWAPI::Position(maxX, _y) * 32 + BWAPI::Position(16, 16));
}

std::vector<BWAPI::Position> sortedVertices;
BWAPI::Position current = *unsortedVertices.begin();

_mapEdgeVertices.push_back(current);
unsortedVertices.erase(current);

// while we still have unsorted vertices left, find the closest one remaining to current
while (!unsortedVertices.empty())
{
double bestDist = 1000000;
BWAPI::Position bestPos;

for (const BWAPI::Position & pos : unsortedVertices)
{
double dist = pos.getDistance(current);

if (dist < bestDist)
{
bestDist = dist;
bestPos = pos;
}
}

current = bestPos;
sortedVertices.push_back(bestPos);
unsortedVertices.erase(bestPos);
}

_mapEdgeVertices = sortedVertices;
}


void OverlordManager::followPerimeter(int clockwise)
{
	BWAPI::Position goTo = getFleePosition(clockwise);

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Broodwar->drawCircleMap(goTo, 5, BWAPI::Colors::Red, true);
	}

	Micro::SmartMove(_transportShip, goTo);
}

void OverlordManager::followPerimeter(BWAPI::Position to, BWAPI::Position from)
{
	static int following = 0;
	if (following)
	{
		followPerimeter(following);
		return;
	}

	//assume we're near FROM! 
	if (_transportShip->getDistance(from) < 50 && _waypoints.empty())
	{
		//compute waypoints
		std::pair<int, int> wpIDX = findSafePath(to, from);
		bool valid = (wpIDX.first > -1 && wpIDX.second > -1);
		UAB_ASSERT(valid, "waypoints not valid");
		_waypoints.push_back(_mapEdgeVertices[wpIDX.first]);
		_waypoints.push_back(_mapEdgeVertices[wpIDX.second]);

		// BWAPI::Broodwar->printf("WAYPOINTS: [%d] - [%d]", wpIDX.first, wpIDX.second);

		Micro::SmartMove(_transportShip, _waypoints[0]);
	}
	else if (_waypoints.size() > 1 && _transportShip->getDistance(_waypoints[0]) < 100)
	{
		// BWAPI::Broodwar->printf("FOLLOW PERIMETER TO SECOND WAYPOINT!");
		//follow perimeter to second waypoint! 
		//clockwise or counterclockwise? 
		int closestPolygonIndex = getClosestVertexIndex(_transportShip);
		UAB_ASSERT(closestPolygonIndex != -1, "Couldn't find a closest vertex");  // ensures map edge exists

		if (_mapEdgeVertices[(closestPolygonIndex + 1) % _mapEdgeVertices.size()].getApproxDistance(_waypoints[1]) <
			_mapEdgeVertices[(closestPolygonIndex - 1) % _mapEdgeVertices.size()].getApproxDistance(_waypoints[1]))
		{
			// BWAPI::Broodwar->printf("FOLLOW clockwise");
			following = 1;
			followPerimeter(following);
		}
		else
		{
			// BWAPI::Broodwar->printf("FOLLOW counter clockwise");
			following = -1;
			followPerimeter(following);
		}

	}
	else if (_waypoints.size() > 1 && _transportShip->getDistance(_waypoints[1]) < 50)
	{
		//if close to second waypoint, go to destination!
		following = 0;
		Micro::SmartMove(_transportShip, to);
	}
}

int OverlordManager::getClosestVertexIndex(BWAPI::UnitInterface * unit)
{
	int closestIndex = -1;
	double closestDistance = 10000000;

	for (size_t i(0); i < _mapEdgeVertices.size(); ++i)
	{
		double dist = unit->getDistance(_mapEdgeVertices[i]);
		if (dist < closestDistance)
		{
			closestDistance = dist;
			closestIndex = i;
		}
	}

	return closestIndex;
}

int OverlordManager::getClosestVertexIndex(BWAPI::Position p)
{
	int closestIndex = -1;
	double closestDistance = 10000000;

	for (size_t i(0); i < _mapEdgeVertices.size(); ++i)
	{

		double dist = p.getApproxDistance(_mapEdgeVertices[i]);
		if (dist < closestDistance)
		{
			closestDistance = dist;
			closestIndex = i;
		}
	}

	return closestIndex;
}

std::pair<int, int> OverlordManager::findSafePath(BWAPI::Position to, BWAPI::Position from)
{
	// BWAPI::Broodwar->printf("FROM: [%d,%d]",from.x, from.y);
	// BWAPI::Broodwar->printf("TO: [%d,%d]", to.x, to.y);


	//closest map edge point to destination
	int endPolygonIndex = getClosestVertexIndex(to);
	//BWAPI::Broodwar->printf("end indx: [%d]", endPolygonIndex);

	UAB_ASSERT_WARNING(endPolygonIndex != -1, "Couldn't find a closest vertex");
	BWAPI::Position enemyEdge = _mapEdgeVertices[endPolygonIndex];

	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
	BWAPI::Position enemyPosition = enemyBaseLocation->getPosition();

	//find the projections on the 4 edges
	UAB_ASSERT_WARNING((_minCorner.isValid() && _maxCorner.isValid()), "Map corners should have been set! (transport mgr)");
	std::vector<BWAPI::Position> p;
	p.push_back(BWAPI::Position(from.x, _minCorner.y));
	p.push_back(BWAPI::Position(from.x, _maxCorner.y));
	p.push_back(BWAPI::Position(_minCorner.x, from.y));
	p.push_back(BWAPI::Position(_maxCorner.x, from.y));

	//for (auto _p : p)
	//BWAPI::Broodwar->printf("p: [%d,%d]", _p.x, _p.y);

	int d1 = p[0].getApproxDistance(enemyPosition);
	int d2 = p[1].getApproxDistance(enemyPosition);
	int d3 = p[2].getApproxDistance(enemyPosition);
	int d4 = p[3].getApproxDistance(enemyPosition);

	//have to choose the two points that are not max or min (the sides!)
	int maxIndex = (d1 > d2 ? d1 : d2) > (d3 > d4 ? d3 : d4) ?
		(d1 > d2 ? 0 : 1) : (d3 > d4 ? 2 : 3);



	int minIndex = (d1 < d2 ? d1 : d2) < (d3 < d4 ? d3 : d4) ?
		(d1 < d2 ? 0 : 1) : (d3 < d4 ? 2 : 3);


	if (maxIndex > minIndex)
	{
		p.erase(p.begin() + maxIndex);
		p.erase(p.begin() + minIndex);
	}
	else
	{
		p.erase(p.begin() + minIndex);
		p.erase(p.begin() + maxIndex);
	}

	//BWAPI::Broodwar->printf("new p: [%d,%d] [%d,%d]", p[0].x, p[0].y, p[1].x, p[1].y);

	//get the one that works best from the two.
	BWAPI::Position waypoint = (enemyEdge.getApproxDistance(p[0]) < enemyEdge.getApproxDistance(p[1])) ? p[0] : p[1];

	int startPolygonIndex = getClosestVertexIndex(waypoint);

	return std::pair<int, int>(startPolygonIndex, endPolygonIndex);

}

BWAPI::Position OverlordManager::getFleePosition(int clockwise)
{
	UAB_ASSERT_WARNING(!_mapEdgeVertices.empty(), "We should have a transport route!");

	//BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	// if this is the first flee, we will not have a previous perimeter index
	if (_currentRegionVertexIndex == -1)
	{
		// so return the closest position in the polygon
		int closestPolygonIndex = getClosestVertexIndex(_transportShip);

		UAB_ASSERT_WARNING(closestPolygonIndex != -1, "Couldn't find a closest vertex");

		if (closestPolygonIndex == -1)
		{
			return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
		}
		else
		{
			// set the current index so we know how to iterate if we are still fleeing later
			_currentRegionVertexIndex = closestPolygonIndex;
			return _mapEdgeVertices[closestPolygonIndex];
		}
	}
	// if we are still fleeing from the previous frame, get the next location if we are close enough
	else
	{
		double distanceFromCurrentVertex = _mapEdgeVertices[_currentRegionVertexIndex].getDistance(_transportShip->getPosition());

		// keep going to the next vertex in the perimeter until we get to one we're far enough from to issue another move command
		while (distanceFromCurrentVertex < 128 * 2)
		{
			_currentRegionVertexIndex = (_currentRegionVertexIndex + clockwise * 1) % _mapEdgeVertices.size();

			distanceFromCurrentVertex = _mapEdgeVertices[_currentRegionVertexIndex].getDistance(_transportShip->getPosition());
		}

		return _mapEdgeVertices[_currentRegionVertexIndex];
	}

}
*/