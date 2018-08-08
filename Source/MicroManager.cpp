#include "MicroManager.h"
#include "MapTools.h"

using namespace UAlbertaBot;

//special attack priorities
std::map<BWAPI::Unit, int> MicroManager::specialPriorities;
int MicroManager::lastFrameComputedSpecialPriorities;

MicroManager::MicroManager() 
	: _doNotForm(true)
	, _fighting(false)
	, _formed(false)
	//, specialPriorities(InformationManager::Instance().specialPriorities)
	//, lastFrameComputedSpecialPriorities(InformationManager::Instance().lastFrameComputedSpecialPriorities)
{
}

void MicroManager::setUnits(const BWAPI::Unitset & u) 
{ 
	_units = u; 
}

BWAPI::Position MicroManager::calcCenter() const
{
    if (_units.empty())
    {
        if (Config::Debug::DrawSquadInfo)
        {
            BWAPI::Broodwar->printf("calcCenter() called on empty squad");
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

void MicroManager::execute(const SquadOrder & inputOrder)
{
	// Nothing to do if we have no units
	if (_units.empty() /*|| !(inputOrder.getType() == SquadOrderTypes::Attack || inputOrder.getType() == SquadOrderTypes::Defend)*/)
	{
		return;
	}

	order = inputOrder;
	drawOrderText();

	// Discover enemies within region of interest
	BWAPI::Unitset nearbyEnemies;

	MapGrid::Instance().GetUnits(nearbyEnemies, order.getPosition(), order.getRadius(), false, true);
	for (auto & unit : _units)
	{
		MapGrid::Instance().GetUnits(nearbyEnemies, unit->getPosition(), std::min(8*32, order.getRadius()), false, true);
	}

	computeSpecialPriorities(nearbyEnemies);
	executeMicro(nearbyEnemies);
}

const BWAPI::Unitset & MicroManager::getUnits() const 
{ 
    return _units; 
}

bool MicroManager::unitNearEnemy(BWAPI::Unit unit)
{
	assert(unit);

	BWAPI::Unitset enemyNear;

	MapGrid::Instance().GetUnits(enemyNear, unit->getPosition(), 800, false, true);

	return enemyNear.size() > 0;
}

// returns true if position:
// a) is walkable
// b) doesn't have buildings on it
// c) doesn't have a unit on it that can attack ground
bool MicroManager::checkPositionWalkable(BWAPI::Position pos) 
{
	// get x and y from the position
	int x(pos.x), y(pos.y);

	// walkable tiles exist every 8 pixels
	bool good = BWAPI::Broodwar->isWalkable(x/8, y/8);
	
	// if it's not walkable throw it out
	if (!good) return false;
	
	// for each of those units, if it's a building or an attacking enemy unit we don't want to go there
	for (auto & unit : BWAPI::Broodwar->getUnitsOnTile(x/32, y/32)) 
	{
		if	(unit->getType().isBuilding() || unit->getType().isResourceContainer() || 
			(unit->getPlayer() != BWAPI::Broodwar->self() && unit->getType().groundWeapon() != BWAPI::WeaponTypes::None)) 
		{		
				return false;
		}
	}

	// otherwise it's okay
	return true;
}

void MicroManager::trainSubUnits(BWAPI::Unit unit) const
{
	if (unit->getType() == BWAPI::UnitTypes::Protoss_Reaver)
	{
		unit->train(BWAPI::UnitTypes::Protoss_Scarab);
	}
	else if (unit->getType() == BWAPI::UnitTypes::Protoss_Carrier)
	{
		unit->train(BWAPI::UnitTypes::Protoss_Interceptor);
	}
}

bool MicroManager::unitNearChokepoint(BWAPI::Unit unit) const
{
	for (BWTA::Chokepoint * choke : BWTA::getChokepoints())
	{
		if (unit->getDistance(choke->getCenter()) < 4*32)
		{
			return true;
		}
	}

	return false;
}

void MicroManager::drawOrderText() 
{
	if (Config::Debug::DrawUnitTargetInfo)
    {
		for (auto & unit : _units)
		{
			BWAPI::Broodwar->drawTextMap(unit->getPosition().x, unit->getPosition().y, "%s", order.getStatus().c_str());
		}
	}
}



/* Flash's concave code. */
//dist is distance between middle unit and target gravity center, interval is length of arc between two units
bool MicroManager::formSquad(const BWAPI::Unitset & targets, int dist, int radius, double angle, int interval)
{
	const BWAPI::Unitset & units = getUnits();
	BWAPI::Position tpos = targets.getPosition();
	BWAPI::Position mpos = units.getPosition();
	int valid_number = 0;

	//do not form squad when fighting or close to enemy
	_fighting = false;
	for (auto & unit : units) {
		if (unit->isUnderAttack() || unit->isAttackFrame() || unit->getDistance(tpos) < 80) {
			_fighting = true;
			return false;
		}
		BWAPI::UnitType type = unit->getType();
		if (unit->getDistance(tpos) < 400 && (type == BWAPI::UnitTypes::Zerg_Hydralisk ||
			type == BWAPI::UnitTypes::Zerg_Zergling) ){
			valid_number++;
		}
	}

	//if there is no target, don't form
	if (targets.size() == 0) {
		return false;
	}

	//if there are less than 4 hydralisks/zerglings, no need to form
	if (valid_number < 4) {
		return true;
	}

	/* It seems zerglings act dumb around static defenses without this, and do not attack. */
	//if there is a building near the unit, do not form
	for (auto & target : targets){
		auto type = target->getType();
		if (type.isBuilding()) {
			return false;
		}
	}

	//Formation is set false by Squad for 5 seconds after formation finished once or whichever manager is attacked
	if (_doNotForm) {
		return false;
	}

	//BWAPI::Broodwar->drawTextScreen(200, 340, "%s", "Forming");

	const double PI = 3.14159265;
	double ang = angle / 180 * PI;

	//the angle of mid_point on arc
	double m_ang = atan2(mpos.y - tpos.y, mpos.x - tpos.x);
	//circle center
	int cx = (int)(tpos.x - (radius - dist) * cos(m_ang));
	int cy = (int)(tpos.y - (radius - dist) * sin(m_ang));
	BWAPI::Position c(cx, cy);
	//mid_point on arc
	BWAPI::Position m((int)(cx + radius*cos(m_ang)), (int)(cy + radius*sin(m_ang)));
	BWAPI::Broodwar->drawLineMap(c, m, BWAPI::Colors::Yellow);

	BWAPI::Unitset unassigned;
	for (auto & unit : units){
		BWAPI::UnitType type = unit->getType();

		if (type == BWAPI::UnitTypes::Zerg_Hydralisk ||
			type == BWAPI::UnitTypes::Zerg_Zergling) {
			unassigned.insert(unit);
		}
	}

	//move every positions on the arc to the closest unit
	BWAPI::Position tmp;
	int try_time = 0;
	int r = radius;
	int total_dest_dist = 0;
	int num_assigned = 0;

	while (unassigned.size() > 0 && try_time < 5){
		double ang_interval = interval * 1.0 / r;
		double final_ang;
		int num_to_assign;
		int max_units = (int)(ang / ang_interval) + 1;
		if (unassigned.size() < (unsigned)max_units){
			num_to_assign = unassigned.size();
			final_ang = ang_interval * num_to_assign;
		}
		else {
			num_to_assign = max_units;
			final_ang = ang;
		}
		for (int i = 0; i < num_to_assign; i++) {
			//assign from two ends to middle
			double a = m_ang + pow(-1, i % 2)*(final_ang / 2 - (i / 2)*ang_interval);
			int min_dist = MAXINT;
			BWAPI::Unit closest_unit = nullptr;
			tmp.x = (int)(cx + r * cos(a));
			tmp.y = (int)(cy + r * sin(a));
			for (auto & unit : unassigned){
				int d = unit->getDistance(tmp);
				if (d < min_dist){
					min_dist = d;
					closest_unit = unit;
				}
			}
			//if it's a unit far away from fight, do not assign it to a position
			if (closest_unit && min_dist > 32 * 15){
				unassigned.erase(closest_unit);
				continue;
			}
			BWAPI::WalkPosition WP(tmp);
			//the valid form position should be walkable and in the same region as the arc
			if (tmp.isValid() && Micro::checkMovable(tmp) && closest_unit && BWTA::getRegion(tmp) == BWTA::getRegion(m)){
				BWAPI::Broodwar->drawLineMap(closest_unit->getPosition(), tmp, BWAPI::Colors::Brown);
				//if this unit is already at the point, attackmove, otherwise move
				if (closest_unit->getDistance(tmp) > 32)
					Micro::SmartMove(closest_unit, tmp);
				else
					Micro::SmartAttackMove(closest_unit, tmp);
				unassigned.erase(closest_unit);
				//find the total distance between unit and destination
				total_dest_dist += min_dist;
				num_assigned++;
			}
		}
		r += interval;
		try_time++;
	}

	//if not assigned, move it to base
	for (auto & unit : unassigned) {
		Micro::SmartMove(unit, tpos);
	}

	//if max destination distance less than 32, means forming has been finished
	if (num_assigned > 0 && total_dest_dist / num_assigned <= 32){
		return true;
	}
	else {
		return false;
	}
}

void MicroManager::initializeVectors() {
	kiteVector = BWAPI::Position(0, 0);
	tangentVector = BWAPI::Position(0, 0);
	accumulatedTangents = 0;
}

void MicroManager::updateVectors(BWAPI::Unit unit, BWAPI::Unit target, int inclusionScale, BWAPI::Position heading) {
	int distance = unit->getDistance(target);
	if (UnitUtil::IsThreat(unit, target, false)) {
		int range = UnitUtil::AttackRange(target, unit);
		if (distance < range + inclusionScale * (32 + 3 * (target->getType().topSpeed()))) {
			kiteVector += Micro::GetKiteVector(target, unit);
		}
		//difficult to say what to do with it right now until other things have been set up
		else if (heading.isValid() && distance < range + inclusionScale*(2*32) && target->getDistance(heading) < unit->getDistance(heading)) {
			accumulatedTangents++;
			//int scale = (target->getDistance(unit))/64; //let's just do short range for now...
			tangentVector += Micro::GetTangentVector(target, unit, heading);
		}
		
	}
}

BWAPI::Position	MicroManager::computeAttractionVector(BWAPI::Unit unit) {
	auto allies = BWAPI::Broodwar->getUnitsInRadius(unit->getPosition(), 48, BWAPI::Filter::GetType == unit->getType());
	return (allies.getPosition() - unit->getPosition());
}

void MicroManager::normalizeVectors() {
	if (kiteVector.getLength() >= 64) {  //normalize
		kiteVector = BWAPI::Position(int(64 * kiteVector.x / kiteVector.getLength()), int(64 * kiteVector.y / kiteVector.getLength()));
	}
	if (accumulatedTangents > 0) {
		tangentVector /= accumulatedTangents;
	}
}

//Gets the closest friend from the set of friends. 
//The caller is responsible for choosing their friends wisely. 
const BWAPI::Unit MicroManager::getClosestFriend(const BWAPI::Unit unit, const BWAPI::Unitset friends) const {
	if (friends.empty()) return NULL; //awwww 
	int closestDistance = INT32_MAX; //INTMAX_MAX;
	BWAPI::Unit best = NULL;

	for (auto f : friends) {
		if (!f || !f->exists()) continue;
		int dist = f->getDistance(unit);
		if (dist < closestDistance) {
			best = f;
			closestDistance = dist;
		}
	}

	return best;
}

//Computes special priorities. All buildings are handled as special priorities.
//1. Artosis pylons
//2. Possibly in the future: walls
//Should probably move this computation to InformationManager to ensure it's only done once
//and is shared across all types
void MicroManager::computeSpecialPriorities(const BWAPI::Unitset & enemies) {
	/*for (auto enemy : specialPriorities) {
		if (!enemy.first || !enemy.first->exists()) continue;
		BWAPI::Broodwar->drawTextMap(enemy.first->getPosition(), "%d", enemy.second);

		if (enemy.first->getType() == BWAPI::UnitTypes::Protoss_Pylon) {
			BWAPI::Position pOffset(6 * 32, 3 * 32);
			BWAPI::Broodwar->drawBoxMap(enemy.first->getPosition() - pOffset, enemy.first->getPosition() + pOffset, BWAPI::Colors::Blue);
			if (enemy.second >= 12) BWAPI::Broodwar->drawCircleMap(enemy.first->getPosition(), 64, BWAPI::Colors::Blue);
		}
	}*/

	int currentFrame = BWAPI::Broodwar->getFrameCount();
	if (currentFrame % (24 * 60 * 5)) { //recompute every so often; give time for queens to infest CC's, non-wasteful for computing
		specialPriorities.clear();
	}
	else if (currentFrame % (24 * 5) != 0) {
		return;
	}


	BWAPI::Position powerOffset(6*32, 3*32);
	BWAPI::Unitset stuff;

	std::map<BWAPI::Unit, int> totalPylonsHP;
	BWAPI::Unitset pylons; //store pylons for second pass; more efficient
	//construct map of total pylon HPs in the first pass
	for (auto enemy : enemies) {
		if (!enemy || !enemy->exists()) continue;

		if (specialPriorities.count(enemy) > 0) continue; //already computed

		if (enemy->getType() == BWAPI::UnitTypes::Protoss_Pylon) {
			pylons.insert(enemy);

			auto eTile = BWAPI::Position(enemy->getTilePosition());
			stuff = BWAPI::Broodwar->getUnitsInRectangle(eTile - powerOffset, eTile+powerOffset, 
				BWAPI::Filter::IsEnemy && BWAPI::Filter::IsBuilding && BWAPI::Filter::IsPowered);

			int pylonHP = enemy->getHitPoints() + enemy->getShields();

			for (auto thing : stuff) {
				auto type = thing->getType();
				if (type == BWAPI::UnitTypes::Protoss_Pylon ||
					type == BWAPI::UnitTypes::Protoss_Nexus ||
					type == BWAPI::UnitTypes::Protoss_Assimilator) continue;
				if (type.getRace() != BWAPI::Races::Protoss) continue;

				totalPylonsHP[thing] += pylonHP;
			}
		}
		else {
			//auto targetType = enemy->getType();
		}
	}

	for (auto enemy : pylons) {
		/*if (enemy->getType() != BWAPI::UnitTypes::Protoss_Pylon) {
			UAB_ASSERT(false, "Invalid pylon that's not actually a pylon in artosis pylon computation");
			continue;
		}*/


		auto eTile = BWAPI::Position(enemy->getTilePosition());
		stuff = BWAPI::Broodwar->getUnitsInRectangle(eTile - powerOffset, eTile + powerOffset,
			BWAPI::Filter::IsEnemy && BWAPI::Filter::IsBuilding && BWAPI::Filter::IsPowered);

		int pylonHP = enemy->getHitPoints() + enemy->getShields();

		int totalSupportedHP = 0;
		int totalSupportedCannonHP = 0;
		for (auto thing : stuff) {
			auto type = thing->getType();
			if (type == BWAPI::UnitTypes::Protoss_Pylon ||
				type == BWAPI::UnitTypes::Protoss_Nexus ||
				type == BWAPI::UnitTypes::Protoss_Assimilator) continue;
			if (type.getRace() != BWAPI::Races::Protoss) continue;

			int thingHP = thing->getHitPoints() + thing->getShields();

			if (type == BWAPI::UnitTypes::Protoss_Photon_Cannon) {
				totalSupportedCannonHP += thingHP * (pylonHP / totalPylonsHP[thing]);
			}
			totalSupportedHP += thingHP * (pylonHP / totalPylonsHP[thing]);
		}

		if (totalSupportedCannonHP >= pylonHP+100) {
			specialPriorities[enemy] = 13; //as dangerous as a threat!
		}
		else if (totalSupportedCannonHP > 0) {
			specialPriorities[enemy] = 12; //if we're ranged, maybe shoot it
		}
		else if (totalSupportedHP >= 4400) {
			specialPriorities[enemy] = 13; //this is an unreal artosis pylon
		}
		else if (totalSupportedHP >= 2200) {
			specialPriorities[enemy] = 12; //this is a typical artosis pylon
		}
		else if (totalSupportedHP >= pylonHP) {
			specialPriorities[enemy] = 8; //less important than a typical worker
		}
		else if (totalSupportedHP > 100) {
			specialPriorities[enemy] = 5; //typical pylon
		}
		//else we treat the pylon like any other building
	}
}

//computes the special priority of target
//for now, simply returns the special priority itself
int	MicroManager::getSpecialPriority(BWAPI::Unit unit, BWAPI::Unit target) {
	if (!unit || !target || !unit->exists() || !target->exists()) {
		UAB_ASSERT(false, "Invalid arg to MicroManager::getSpecialPriority");
		return -1;
	}

	if (specialPriorities.count(target) <= 0) return -1; //none found

	//for non-critical cannon priorities, check if the unit is long-ranged enough to maybe hit a pylon
	//without getting hit by a cannon behind it --> return priority 13 for pylon if so
	//if (!unit->isUnderAttack() && specialPriorities[target] == 12) return 13;

	if (target->getType() == BWAPI::UnitTypes::Terran_Command_Center) {
		BWAPI::Broodwar->printf("Unit type %s sees command center with priority %d", unit->getType().c_str(), specialPriorities[target]);
	}

	return specialPriorities[target];
}

int MicroManager::getCommonPriority(BWAPI::Unit unit, BWAPI::Unit target) {
	if (!UnitUtil::CanAttack(unit, target)) return 0;

	auto targetType = target->getType();

	int specialPriority = getSpecialPriority(unit, target);
	if (specialPriority >= 0) return specialPriority;

	//ground units may have some trouble focusing them down - such units should 
	//handle vulnerable carriers before reaching this case
	if (targetType == BWAPI::UnitTypes::Protoss_Carrier && !unit->isFlying()) return 7;

	if (targetType == BWAPI::UnitTypes::Zerg_Larva || targetType == BWAPI::UnitTypes::Zerg_Egg ||
		targetType.isAddon()) return 0;

	if (UnitUtil::IsThreat(unit, target)) return 13;

	if (targetType.isWorker())
	{
		if (target->isRepairing() || target->isConstructing() || unitNearChokepoint(target) ||
			target->isBraking() || !target->isMoving() || target->isGatheringMinerals() || target->isGatheringGas())
		{
			return 12;
		}

		return 9;
	}

	if (targetType.isSpellcaster() ||
		targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
		targetType.airWeapon() != BWAPI::WeaponTypes::None)
	{
		return 7;
	}

	// important buildings
	if (targetType == BWAPI::UnitTypes::Protoss_Templar_Archives ||
		targetType == BWAPI::UnitTypes::Zerg_Spire ||
		targetType.isResourceDepot() ||
		targetType == BWAPI::UnitTypes::Terran_Armory)
	{
		return 7;
	}

	// Downgrade unfinished/unpowered/on-fire buildings
	bool onFire = targetType.getRace() == BWAPI::Races::Terran && target->getHitPoints() < targetType.maxHitPoints() / 3;
	if (targetType.isBuilding() && (!target->isCompleted() || !target->isPowered() || onFire)) {
		return 2;
	}

	if (targetType.gasPrice() + targetType.mineralPrice() > 0) return 3;

	// Everything else
	return 1;
}