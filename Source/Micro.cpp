#include "Micro.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

size_t TotalCommands = 0;

const int dotRadius = 2;

void Micro::drawAPM(int x, int y)
{
	// Don't divide by zero, even theoretically.
	if (BWAPI::Broodwar->getFrameCount() > 0)
	{
		int bwapiAPM = BWAPI::Broodwar->getAPM();
		int myAPM = 0;
		myAPM = int(TotalCommands / (double(BWAPI::Broodwar->getFrameCount()) / (24 * 60)));
		BWAPI::Broodwar->drawTextScreen(x, y, "%d %d", bwapiAPM, myAPM);
	}
}

void Micro::SmartAttackUnit(BWAPI::Unit attacker, BWAPI::Unit target)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
    {
		UAB_ASSERT(false,"bad arg Micro::SmartAttackUnit");
        return;
    }

    // if we have issued a command to this unit already this frame, ignore this one
	if (attacker->getType() != BWAPI::UnitTypes::Zerg_Lurker &&
		!(attacker->getType().isBuilding())) { //...unless it's a lurker/sunken.
		if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
		{
			return;
		}
	}

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&	currentCommand.getTarget() == target)
    {
        return;
    }

    // if nothing prevents it, attack the target
    attacker->attack(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawLineMap( attacker->getPosition(), target->getPosition(), BWAPI::Colors::Red );
    }
}

void Micro::SmartAttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() || !targetPosition.isValid())
    {
		UAB_ASSERT(false, "bad arg to to Micro::SmartAttackMove");
		return;
    }

    // if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move &&	currentCommand.getTargetPosition() == targetPosition)
	{
		return;
	}

    // if nothing prevents it, attack the target
    attacker->attack(targetPosition);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::Orange);
    }
}

void Micro::SmartMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
	// -- -- TODO temporary extra debugging to solve 2 bugs
	/*
	if (!attacker->exists())
	{
		UAB_ASSERT(false, "SmartMove: nonexistent");
		BWAPI::Broodwar->printf("SM: non-exist %s @ %d, %d", attacker->getType().getName().c_str(), targetPosition.x, targetPosition.y);
		return;
	}
	if (attacker->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "SmartMove: not my unit");
		return;
	}
	if (!targetPosition.isValid())
	{
		UAB_ASSERT(false, "SmartMove: bad position");
		return;
	}
	// -- -- TODO end
	*/

	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() || !targetPosition.isValid())
	{
		//UAB_ASSERT(false, "bad arg");  // TODO restore this after the bugs are solved; can make too many beeps
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
	if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || (attacker->isAttackFrame() &&
		attacker->getType() != BWAPI::UnitTypes::Zerg_Guardian))
    {
        return;
    }

    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move) && (currentCommand.getTargetPosition() == targetPosition) /*&& attacker->isMoving() &&
		!(attacker->isBraking() || attacker->isStuck())*/) //if the unit's braking/stuck, try spamming move again
    {
        return;
    }

    // if nothing prevents it, move the target position
    attacker->move(targetPosition);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::White);
    }
}

void Micro::SmartStop(BWAPI::Unit unit) {
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		//UAB_ASSERT(false, "bad arg");  // TODO restore this after the bugs are solved; can make too many beeps
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if nothing prevents it, move the target position
	unit->stop();
	TotalCommands++;

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Brown, true);
	}
}

//Kites away according to the kite vector, otherwise moves normally. 
void Micro::SmartTravel(BWAPI::Unit unit, const BWAPI::Position & targetPosition, BWAPI::Position kiteVector, bool path) {
	if (!unit || !targetPosition.isValid()) {
		UAB_ASSERT(false, "Invalid arg to Micro::SmartTravel");
		return;
	}

	if (path && kiteVector != BWAPI::Position(0, 0)) {
		BWAPI::Position movePosition = unit->getPosition() + kiteVector;
		if (!checkMovable(movePosition)) movePosition = targetPosition;
		SmartMove(unit, movePosition);
	}
	else if (path) { //in the future, replace this with an appropriate pathfinding algorithm
		if (!unit->isFlying()) {
			//intended to help those drones that get stuck in silly positions

			//sadly does not work since BWTA crashes consistently while trying to find a path

			/*try {
				auto path = BWTA::getShortestPath2(unit->getTilePosition(), BWAPI::TilePosition(targetPosition));

				while (!path.empty() && unit->getDistance(BWAPI::Position(path.front()->getCenter())) < 300){
					path.pop_front();
				}

				if (!path.empty()) {
					SmartMove(unit, path.front()->getCenter());
					return;
				}
			}*/
			SmartMove(unit, targetPosition);
			return;
		}
		else { 
			SmartMove(unit, targetPosition);
			return;
		}

		//otherwise default to moving to the target position
		SmartMove(unit, targetPosition);
	}
	else {
		SmartMove(unit, targetPosition);
	}
}

void Micro::SmartRightClick(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg to Micro::SmartRightClick");
		return;
	}


    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit) && (currentCommand.getTargetPosition() == target->getPosition()))
    {
        return;
    }

    // if nothing prevents it, attack the target
    unit->rightClick(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
    }
}

void Micro::SmartLaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() || !pos.isValid())
	{
		UAB_ASSERT(false, "bad arg to Micro::SmartLaySpiderMine");
		return;
	}

    if (!unit->canUseTech(BWAPI::TechTypes::Spider_Mines, pos))
    {
        return;
    }

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Position) && (currentCommand.getTargetPosition() == pos))
    {
        return;
    }

    unit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, pos);
}

void Micro::SmartRepair(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg to Micro::SmartRepair");
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Repair) && (currentCommand.getTarget() == target))
    {
        return;
    }

    // Nothing prevents it, so attack the target.
    unit->repair(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
    }
}

void Micro::SmartUseTech(BWAPI::Unit unit, BWAPI::TechType tech, const BWAPI::PositionOrUnit & target) {
	//we presume the target is valid for this method; it is allowable for it to be NULL
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		tech == BWAPI::TechTypes::None)
	{
		UAB_ASSERT(false, "bad arg to Micro::SmartUseTech");
		return;
	}

	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	if (!unit->getPlayer()->hasResearched(tech))
	{
		UAB_ASSERT(false, "Player has not yet researched %s in Micro::SmartUseTech", tech.c_str());
		return;
	}

	if (target.isUnit()) {
		if (currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit) {
			auto u = unit->getTarget();

			auto utarget = target.getUnit();
			if (!utarget || !utarget->exists()) {
				UAB_ASSERT(false, "invalid arg for unit target in Micro::SmartUseTech");
				return;
			}

			if (!u || !u->exists()) { //this is fine, could simply mean the unit died or was consumed
				//UAB_ASSERT(false, "prior use tech with invalid target in Micro::SmartUseTech");
				//return;
			}
			else { //if it's still there we compare it
				if (u == utarget) return;
			}
		}
	}
	else if (target.isPosition()) {
		if (currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Position) {
			auto p = unit->getTargetPosition();
			if (!p.isValid()) {
				UAB_ASSERT(false, "prior use tech with invalid position in Micro::SmartUseTech");
				return;
			}
			auto ptarget = target.getPosition();
			if (!ptarget.isValid()) {
				UAB_ASSERT(false, "invalid arg for position target in Micro::SmartUseTech");
				return;
			}

			if (p == ptarget) return;
		}
	}

	// if nothing prevents it, attack the target
	if (unit->canUseTech(tech, target)) {
		unit->useTech(tech, target);
		TotalCommands++;
	}
	else {
		UAB_ASSERT(false, "Unit in Micro::SmartUseTech cannot use tech on specified target");
	}
}

// Perform a comsat scan at the given position if possible.
// If it's not possible (no comsat, not enough energy), do nothing.
// Return whether the scan occurred.
bool Micro::SmartScan(const BWAPI::Position & targetPosition)
{
	// Choose the comsat with the highest energy.
	// If we're not terran, we're unlikely to have any comsats....
	int maxEnergy = 49;      // anything greater is enough energy for a scan
	BWAPI::Unit comsat = nullptr;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Terran_Comsat_Station &&
			unit->getEnergy() > maxEnergy &&
			unit->canUseTech(BWAPI::TechTypes::Scanner_Sweep, targetPosition))
		{
			maxEnergy = unit->getEnergy();
			comsat = unit;
		}
	}

	if (comsat)
	{
		return comsat->useTech(BWAPI::TechTypes::Scanner_Sweep, targetPosition);
	}

	return false;
}

void Micro::SmartReturnCargo(BWAPI::Unit worker)
{
	if (!worker || !worker->exists() || worker->getPlayer() != BWAPI::Broodwar->self() ||
		!worker->getType().isWorker())
	{
		UAB_ASSERT(false, "bad arg to Micro::SmartReturnCargo");
		return;
	}

	// If the worker has no cargo, ignore this command.
	if (!worker->isCarryingMinerals() && !worker->isCarryingGas())
	{
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (worker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || worker->isAttackFrame())
	{
		return;
	}

	// If we've already issued this command, don't issue it again.
	BWAPI::UnitCommand currentCommand(worker->getLastCommand());
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Return_Cargo &&
		!((worker->isIdle() || worker->getOrder() == BWAPI::Orders::PlayerGuard)))
	{
		return;
	}

	// Nothing prevents it, so return cargo.
	worker->returnCargo();
	TotalCommands++;
}

void Micro::SmartHoldPosition(BWAPI::Unit unit) {
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad arg to Micro::SmartHoldPosition");
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we've already told this unit to move to this position, ignore this command
	if (unit->isHoldingPosition())
	{
		return;
	}

	// if nothing prevents it, attack the target
	if (unit->canHoldPosition()) {
		unit->holdPosition();
		TotalCommands++;
	}
	else {
		UAB_ASSERT(false, "Unit in Micro::SmartHoldPosition is incapable of holding position");
	}

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Purple, true);
	}
}

/* 3-frame delay compared to using attack -- 2 frames on top of normal attack order.
 * Not recommended to actually use this... */
void Micro::SmartPatrol(BWAPI::Unit unit, BWAPI::Position targetPosition) {
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad arg to Micro::SmartPatrol");
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we've already told this unit to move to this position, ignore this command
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Patrol) || unit->isPatrolling())
	{
		return;
	}

	// if nothing prevents it, attack the target
	if (unit->canPatrol()) {
		unit->patrol(targetPosition);
		TotalCommands++;
	}
	else {
		UAB_ASSERT(false, "Unit in Micro::SmartPatrol is incapable of holding patrol.");
	}

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Purple, true);
	}
}


void Micro::SmartKiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target, BWAPI::Position kiteVector)
{
	if (!rangedUnit || !rangedUnit->exists() || rangedUnit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	double range(UnitUtil::AttackRange(rangedUnit, target));

	bool kite(true);

	// Kite if configured as a "KiteLonger" unit, or if the enemy's range is shorter than ours.
	// Note: Assumes both units have range upgrades
	bool kiteLonger = Config::Micro::KiteLongerRangedUnits.find(rangedUnit->getType()) != Config::Micro::KiteLongerRangedUnits.end();
	double enemyRange = UnitUtil::AttackRange(target, rangedUnit);
	if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk &&
		target->getType() == BWAPI::UnitTypes::Terran_Marine) { 
		kite = false;

		//kite back to any nearby allied lurker/sunken colony that's not already attacking
		auto allies = rangedUnit->getUnitsInRadius(2 * 32, BWAPI::Filter::IsAlly && !BWAPI::Filter::IsAttacking);
		bool supported = false;
		BWAPI::Unit closestAlly = NULL;
		for (auto ally : allies) {
			if (ally->getType() != BWAPI::UnitTypes::Zerg_Lurker && ally->getType() != BWAPI::UnitTypes::Zerg_Sunken_Colony) continue;
			if (ally->getDistance(rangedUnit) < 32) {
				supported = true;
				break; //we're close enough
			}

			closestAlly = ally;
		}

		if (!supported && closestAlly) {
			Micro::SmartMove(rangedUnit, closestAlly->getPosition());
			return;
		}
	}
	else if (!kiteLonger && (range <= enemyRange))
	{
		kite = false;
	} // if the unit can't attack back don't kite
	else if ((rangedUnit->isFlying() && !UnitUtil::CanAttackAir(target)) || (!rangedUnit->isFlying() && !UnitUtil::CanAttackGround(target)))
	{
		kite = false;
	}


	double dist(rangedUnit->getDistance(target));

	BWAPI::Position projectedPosition = GetProjectedPosition(rangedUnit, target);
	if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk) {
		//don't worry about micro against interceptors or buildings or dragoons...
		if (target->getType() == BWAPI::UnitTypes::Protoss_Interceptor ||
			target->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			target->getType().isBuilding() ||
			target->getType() == BWAPI::UnitTypes::Protoss_Dragoon) {
			kite = false; 
		}
		//keep running from melee units that are getting near
		else if (UnitUtil::AttackRange(target, rangedUnit) <= 64) {
			if (dist <= 32 + 10) {
				//always kite at this range
				BWAPI::Position fleePosition(GetKiteVector(target, rangedUnit) + rangedUnit->getPosition());
				Micro::SuperSmartMove(rangedUnit, fleePosition);
				return;
			}
			else if (!target->isMoving() || target->isBraking()) {
				kite = false;
			}
			else if (rangedUnit->getDistance(projectedPosition) > range) {
				//they're running! chase! [see below]
				kite = false;
			}
			else if (rangedUnit->getDistance(GetChasePosition(rangedUnit, target)) > rangedUnit->getDistance(target->getPosition())) {
				//they're moving AWAY from us
				kite = false;
			}
		}
	}
	//always kite as a guardian
	else if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Guardian) {
		kite = true;
	}
	else if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Devourer) {
		//assume a devourer has a long attack frame and needs time to attack, otherwise kite
		if (rangedUnit->isAttackFrame() || rangedUnit->isStartingAttack()) return;
		kite = true;
	}

	// Kite if we're not ready yet: Wait for the weapon.
	double speed(rangedUnit->getType().topSpeed());
	double timeToEnter = 0.0;                      // time to reach firing range
	if (speed > .00001) timeToEnter = std::max(0.0, dist - range) / speed; // no division by 0
	if (rangedUnit->getGroundWeaponCooldown() <= timeToEnter)
	{
		if (timeToEnter > 2.0) {
			if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Guardian) {
				/* Special case for guardians */
				/* ------------------------------ */
				double realSpeed = sqrt(pow(rangedUnit->getVelocityX(), 2.0) + pow(rangedUnit->getVelocityY(), 2.0));
				if (realSpeed <= 0 && dist > range) {
					Micro::SmartMove(rangedUnit, target->getPosition());
				}
				else {
					//we want to come to an almost full stop RIGHT at the attack range
					//readjust our speed so that the time it takes to decelerate to 0
					//is less than the time we will reach the firing range

					//not certain this is actually doing anything -- dividing by 256, which should give the correct units, 
					//makes the guardians move too slow
					double decel = (BWAPI::UnitTypes::Zerg_Guardian.acceleration()/* / 256*/);
					//presume they're the same. note that decel is given in pixel/frame^2; it matches the velocity's pixel/frame
					if (realSpeed / decel > (dist - range) / realSpeed) {
						Micro::SmartHoldPosition(rangedUnit);
					}
					else {
						Micro::SmartMove(rangedUnit, target->getPosition());
					}
				}
				/* ------------------------------ */
			}
			else if (timeToEnter < 6.0) {
				Micro::SuperSmartMove(rangedUnit, projectedPosition);
			}
			else {
				Micro::SmartMove(rangedUnit, projectedPosition);
			}
		}
		else {
			Micro::SmartAttackUnit(rangedUnit, target);
		}
	}
	else if (kite) {
		BWAPI::Position fleePosition(GetKiteVector(target, rangedUnit) + rangedUnit->getPosition()); //default in case of local minima
		if (kiteVector != BWAPI::Position(0, 0)) { //kite away from the accumulated threats nearby
			fleePosition = rangedUnit->getPosition() + kiteVector;
		}

		if (rangedUnit->isFlying()) {
			Micro::SmartMove(rangedUnit, fleePosition);
		}
		else {
			Micro::SuperSmartMove(rangedUnit, fleePosition);
		}
	}
	else if (rangedUnit->getDistance(projectedPosition) > range - (target->isMoving() ? 16 : 0) || //allows melee unit chases to fall through
		//advance if your target is on higher ground than you
		(BWAPI::Broodwar->getGroundHeight(target->getTilePosition()) > BWAPI::Broodwar->getGroundHeight(rangedUnit->getTilePosition())) ||
		((target->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
		target->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) &&
		rangedUnit->getDistance(target) > 2*32 + 16) ||
		
		(rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk && //... keep getting closer to dragoons.
		(target->getType() == BWAPI::UnitTypes::Protoss_Dragoon ||
		target->getType() == BWAPI::UnitTypes::Protoss_Carrier) && 
		rangedUnit->getDistance(target) > 3*32 &&
		!rangedUnit->isAttackFrame())
		
		) {
		if (timeToEnter < 6.0) {
			Micro::SuperSmartMove(rangedUnit, projectedPosition);
		}
		else {
			Micro::SmartMove(rangedUnit, projectedPosition);
		}
	}
	else {
		Micro::SmartAttackUnit(rangedUnit, target);
	}
}

// Used for both mutalisks and vultures--fast units with short range.
void Micro::MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target, BWAPI::Position kiteVector)
{
	if (!muta || !muta->exists() || muta->getPlayer() != BWAPI::Broodwar->self() ||
		(muta->getType() != BWAPI::UnitTypes::Zerg_Mutalisk && muta->getType() != BWAPI::UnitTypes::Terran_Vulture) ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    const int cooldown                  = muta->getType().groundWeapon().damageCooldown();
    const int latency                   = BWAPI::Broodwar->getLatency();
    const double speed                  = muta->getType().topSpeed();   // known to be non-zero :-)
    const double range                  = muta->getType().groundWeapon().maxRange();
    const double distanceToTarget       = muta->getDistance(GetProjectedPosition(muta, target));
	const double distanceToFiringRange  = std::max(distanceToTarget - range,0.0);
	const double timeToEnterFiringRange = distanceToFiringRange / speed;
	const int framesToAttack            = static_cast<int>(timeToEnterFiringRange) + 2*latency + (UnitUtil::IsFacing(muta, target) ? 0 : 1);


	BWAPI::UnitType targetType = target->getType();
	bool isThreat = UnitUtil::IsThreat(muta, target, false);

	// always retreat condition: is a mutalisk with low HP, fighting a threat, 
	//that is too far to hit right away, but the 
	//muta should stay not too far away from combat. 
	//also run away if being stormed. 
	bool tooCloseToScourge = (targetType == BWAPI::UnitTypes::Zerg_Scourge) &&
		distanceToTarget <= 12 && muta->getHitPoints() >= 30; //at this distance just keep running
		//if we're low on hp we can afford to hit scourge
	bool notInRangeAndDying = (muta->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
		&& (isThreat && (timeToEnterFiringRange > 1.0) && (distanceToTarget < 13*32)
		&& 
		(muta->isUnderStorm() || tooCloseToScourge)
		);

	// How many frames are left before we can attack?
	const int currentCooldown = muta->isStartingAttack() ? cooldown : muta->getGroundWeaponCooldown();
	//const int currentCooldown = muta->getGroundWeaponCooldown();
	// If we can attack by the time we reach our firing range
	BWAPI::Position fleeVector = GetKiteVector(target, muta);
	if (currentCooldown <= framesToAttack && !notInRangeAndDying)
	{
		if (timeToEnterFiringRange >= 6.0) {
			BWAPI::Position chasePosition = GetChasePosition(muta, target);
			if (chasePosition.isValid() && targetType != BWAPI::UnitTypes::Zerg_Scourge) {
				muta->move(chasePosition - fleeVector);
			} else {
				if (distanceToTarget > 16 && !UnitUtil::IsFacing(muta, target, 2)) muta->move(target->getPosition());
				muta->attack(target);
			}
		}
		else {
			if (distanceToTarget > 16 && !UnitUtil::IsFacing(muta, target, 2)) muta->move(target->getPosition());
			muta->attack(target);
		}
	}
	else // Otherwise we cannot attack and should temporarily back off, or continue chasing non-threats
	{
		//presume that if we are damaged that something is protecting the workers. 
		BWAPI::Position moveToPosition(muta->getPosition() + fleeVector); //default movement in case we are stuck in local minima
		if (kiteVector != BWAPI::Position(0, 0)) { //move according to cumulative kite vectors of all threats in area
			moveToPosition = muta->getPosition() + kiteVector;
		}

		bool isSpecial = (targetType.isWorker() && muta->getHitPoints() >= muta->getType().maxHitPoints() - 20) || targetType.isBuilding();

		if ((!isThreat || isSpecial) && kiteVector == BWAPI::Position(0,0)) {
			/*if (target->getType() != BWAPI::UnitTypes::Zerg_Scourge) {
				BWAPI::Position pDiff = target->getPosition() - muta->getPosition();
				double ourAngle = muta->getAngle() < M_PI ? muta->getAngle() : muta->getAngle() - 2 * M_PI;
				double facingAngle = atan2(pDiff.y, pDiff.x);

				if (currentCooldown <= 6 && abs(ourAngle - facingAngle) >= M_PI / 8) {
					muta->attack(target);
				}
			}*/

			BWAPI::Position chasePosition = GetChasePosition(muta, target);
			moveToPosition = BWAPI::Position(chasePosition - fleeVector);
		}


		if (moveToPosition.isValid())
		{
			muta->move(moveToPosition);
		}
	}
}

/* Returns the position the target is predicted to be at by the time unit reaches the initial
 * melee position. */
BWAPI::Position Micro::GetChasePosition(BWAPI::Unit unit, BWAPI::Unit target)
{
	double speed = unit->getType().topSpeed();
	double distance = unit->getDistance(target);

	double approxTimeToPosition = 1.0;
	if (speed > 0) { /* No division by 0! */
		approxTimeToPosition = std::min(14.0, distance / speed);
	}
	BWAPI::Position projectedMovement(int(approxTimeToPosition * target->getVelocityX()), int(approxTimeToPosition * target->getVelocityY()));
	BWAPI::Position chasePosition(target->getPosition() + projectedMovement);
	if (chasePosition.isValid()) return chasePosition;

	return target->getPosition();
}

/* Returns the position the target is predicted to be at by the time unit reaches the initial
 * attack range distance away from the target. */
BWAPI::Position Micro::GetProjectedPosition(BWAPI::Unit unit, BWAPI::Unit target)
{
	double speed = unit->getType().topSpeed();
	double distance = unit->getDistance(target);
	double range = UnitUtil::AttackRange(unit, target);

	double approxTimeToAttackRange = 1.0;
	if (speed > 0) { /* No division by 0! But we at least check where the unit will be the next frame. */
		approxTimeToAttackRange = (distance - range) / speed;
	}
	if (approxTimeToAttackRange < 0.0) approxTimeToAttackRange = 1.0; //already in range, but check next frame position anyway

	BWAPI::Position projectedMovement(int(approxTimeToAttackRange * target->getVelocityX()), int(approxTimeToAttackRange * target->getVelocityY()));
	BWAPI::Position chasePosition(target->getPosition() + projectedMovement);
	if (chasePosition.isValid()) return chasePosition;

	return target->getPosition();
}

BWAPI::Position Micro::GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target)
{
    BWAPI::Position fleeVec(target->getPosition() - unit->getPosition());
    double fleeAngle = atan2(fleeVec.y, fleeVec.x);
    fleeVec = BWAPI::Position(static_cast<int>(64 * cos(fleeAngle)), static_cast<int>(64 * sin(fleeAngle)));
    return fleeVec;
}

BWAPI::Position Micro::GetTangentVector(BWAPI::Unit unit, BWAPI::Unit target, BWAPI::Position heading) {
	BWAPI::Position forwardVector = GetKiteVector(unit, target);
	BWAPI::Position c1(-forwardVector.y, forwardVector.x);
	BWAPI::Position c2(forwardVector.y, -forwardVector.x);

	if (heading.getDistance(unit->getPosition() + c1) < heading.getDistance(unit->getPosition() + c2)) {
		return c1;
	}
	else {
		return c2;
	}
}

// TODO "Common.h" has double2 with rotate and normalize

const double PI = 3.14159265;
void Micro::Rotate(double &x, double &y, double angle)
{
	angle = angle*PI/180.0;
	x = (x * cos(angle)) - (y * sin(angle));
	y = (y * cos(angle)) + (x * sin(angle));
}

void Micro::Normalize(double &x, double &y)
{
	double length = sqrt((x * x) + (y * y));
	if (length != 0)
	{
		x = (x / length);
		y = (y / length);
	}
}

// Avoid allies within specified radius. If radius = 0 is specified, avoids units in radius 4*32.
bool Micro::AvoidAllies(BWAPI::Unit unit, int radius)
{ 
	int r = radius;
	if (radius == 0) { //by default assume this is for irradiate which has radius 2*32
		r = 4 * 32;
	}
	BWAPI::Unit closestAlly = unit->getClosestUnit(BWAPI::Filter::IsAlly, r);

	// nobody to run from!
	if (!closestAlly) {
		return false;
	}

	/* Add a random, small perturbance so it will rush out of the unstable equilibrium point
	where a mutalisk is following an irradiated mutalisk. */
	int perturbanceX = (rand() % 2) - 1;
	int perturbanceY = (rand() % 2) - 1;
	BWAPI::Position fleeVector = GetKiteVector(closestAlly, unit);
	BWAPI::Position allyVelocity(int(closestAlly->getVelocityX()) + perturbanceX, int(closestAlly->getVelocityY()) + perturbanceY);
	BWAPI::Position moveToPosition(unit->getPosition() + fleeVector - allyVelocity);

	if (moveToPosition.isValid())
	{
		if (unit->isBurrowed() && unit->canUnburrow()) { //unburrow if we have to
			unit->unburrow();
			return true;
		}
		else {
			Micro::SmartMove(unit, moveToPosition);
		}
	}
	else { //allow process to continue if position finding failed
		return false; 
	}

	return true; 
}

// Stack with allies! Party!! Yay!!
//mainly for mutalisks, and for devourers stacking with mutalisks - stackType is a mutalisk by default
//strictness indicates how strictly the units are to judge how stacked they are with one another. 
bool Micro::StackUp(BWAPI::Unit unit, int strictness, BWAPI::UnitType stackType)
{
	/*BWAPI::Unit closestAlly = unit->getClosestUnit(BWAPI::Filter::IsAlly, 4*32);
	if (!closestAlly) return false; //nobody to party with!
	if (unit->getType() != closestAlly->getType()) return false; //can't party with them!
	
	if (unit->getDistance(closestAlly) <= strictness * 32 / 8) return false; //already partying!*/

	auto allies = unit->getUnitsInRadius(4 * 32, BWAPI::Filter::IsAlly && BWAPI::Filter::IsFlyer);
	if (allies.empty()) return false; //nobody to party with!
	BWAPI::Position chasePosition(0, 0);
	int n = 0;
	int nStacked = 0;
	for (auto & ally : allies) {
		if (!ally || unit == ally || stackType != ally->getType()) continue;
		if (strictness == 0) {
			if (unit->getDistance(ally) <= 0) nStacked++;
			if (nStacked >= int(allies.size()) - 2) return false; //already partying
		}
		else if (unit->getDistance(ally) <= strictness * 32 / 8)  {
			return false; //already partying!
		}
		n++;
		chasePosition += GetChasePosition(unit, ally); 
	}

	if (n <= 0) return false; //nobody to party with!
	chasePosition /= n; //average chase position

	Micro::SmartMove(unit, chasePosition);

	return true;
}

//Zergling attack micro. Also applied to combat workers. Under testing.
bool Micro::ZerglingAttack(BWAPI::Unit unit, BWAPI::Unit target, BWAPI::Position kiteVector) {
	if (!unit || !unit->exists() || !target || !target->exists()) {
		UAB_ASSERT(false, "invalid arg to Micro::ZerglingAttack");
		return false;
	}

	if (unit->getType() != BWAPI::UnitTypes::Zerg_Zergling && unit->getType() != BWAPI::UnitTypes::Zerg_Drone) {
		return false;
	}

	int distance = unit->getDistance(target);
	if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling) { //preference for continuing an attack that's already begun
		if (unit->isAttacking()) return false; //already attacking! continue;
		if (distance < 40) return false; //we're already too close! attack!
		if (unit->getUnitsInRadius(24, !BWAPI::Filter::IsFlying).size() >= 5) return false; //too many people nearby!
	}

	if (distance > 96) return false; //we're too far! catch up!

	auto type = target->getType();
	if (type.groundWeapon() == BWAPI::WeaponTypes::None) return false;
	if (type.isBuilding()) return false;

	auto range = type.groundWeapon().maxRange();
	if (range >= 32) return false; //it's a ranged unit, just charge!

	if (target->getGroundWeaponCooldown() >= 10) return false; //their weapon is on a long cooldown! attack!

	//their weapon is on a shorter cooldown, but we can survive it! attack!
	if (target->getGroundWeaponCooldown() > 0 && unit->getHitPoints() > type.groundWeapon().damageAmount()) return false;

	if (!UnitUtil::IsFacing(target, unit, 1)) return false; //not facing us! attack!

	//otherwise back off for a bit
	
	if (kiteVector == BWAPI::Position(0, 0)) {
		SuperSmartMove(unit, unit->getPosition() + GetKiteVector(target, unit));
	}
	else {
		SuperSmartMove(unit, unit->getPosition() + kiteVector);
	}


	return false;
}

/* Consider whether to go around the target as a zergling.
 * Specifically, when the enemy is isolated and not many units are attacking it. */
bool Micro::SurroundTarget(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!target || !unit) { //something is wrong!
		UAB_ASSERT(false, "bad arg to Micro::SurroundTarget");
		return false;
	}
	if (unit->getType() != BWAPI::UnitTypes::Zerg_Zergling) return false; 
	if (target->getHitPoints() <= 60 && (!(target->isMoving()) || target->isBraking() || target->isAttackFrame())) return false;

	int searchRadius = 40;
	BWAPI::UnitType targetType = target->getType();
	if (targetType.isBuilding() ||
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran || //surrounding these might be a bad idea
		targetType == BWAPI::UnitTypes::Terran_Firebat ||
		targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
		targetType == BWAPI::UnitTypes::Zerg_Lurker) return false;

	int enemySearchRadius = searchRadius; //default to search radius of 40;
	if (target->getType() == BWAPI::UnitTypes::Protoss_Zealot) enemySearchRadius = 23 + 8;

	BWAPI::Unitset nearbyEnemies = target->getUnitsInRadius(enemySearchRadius, BWAPI::Filter::IsEnemy);
	if (!target->isMoving() && nearbyEnemies.size() > 1) return false; //can't surround someone fighting with friends!
	if (target->isMoving() && nearbyEnemies.size() > 2) return false; //can't surround a running target with too many friends
	
	BWAPI::Unitset alliesAttacking = target->getUnitsInRadius(enemySearchRadius, BWAPI::Filter::IsAlly);
	int numAllies = 0;
	for (auto & ally : alliesAttacking) {
		if (ally->getType() == BWAPI::UnitTypes::Zerg_Ultralisk) return false; //too big!

		numAllies++;
		if (numAllies > 6) return false; //already surrounded!
	}

	BWAPI::Unitset nearbyAllies = unit->getUnitsInRadius(searchRadius, BWAPI::Filter::IsAlly);
	int numZerglings = 1; //1 because, you know, we're a zergling
	BWAPI::Unit farAlly = NULL;
	for (auto & ally : nearbyAllies) {
		if (ally->getType() == BWAPI::UnitTypes::Zerg_Ultralisk) return false; //too big!
		if (ally->getType() != BWAPI::UnitTypes::Zerg_Zergling) continue;
		numZerglings++;
		if (ally->getDistance(target) > unit->getDistance(target) && //ally is farther away from enemy than us
			ally->getDistance(unit) < ally->getDistance(target)) { //ally is not on other end of enemy
			farAlly = ally;
		}
		if (farAlly && numZerglings >= 6) break;
	}
	if (!farAlly || //we're the farthest away! need to catch up!
		numZerglings < 6) return false; //too few zerglings to bother surrounding.

	Micro::SuperSmartMove(unit, target->getPosition() + Micro::GetKiteVector(farAlly, target));

	return true;
}

// returns true if position:
// a) is walkable
// b) doesn't have buildings on it
// c) doesn't have a unit on it
// note: best for small units as it doesn't check multiple tiles
bool Micro::checkMovable(BWAPI::Position pos, int scale)
{
	// walkable tiles exist every 8 pixels, 	// if it's not walkable throw it out
	if (!pos.isValid()) return false;
	bool good = BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos));
	if (!good) return false;

	// check if there is a unit at that exact position
	for (auto & unit : BWAPI::Broodwar->getUnitsInRadius(pos, scale*(32 / 8), !BWAPI::Filter::IsFlying))
	{

		auto type = unit->getType();
		if (type.isBuilding()) {
			auto p = unit->getPosition();
			auto pDiff = pos - p;

			pDiff.x = abs(pDiff.x) - (pDiff.x > 0) ? type.dimensionRight() : type.dimensionLeft();
			pDiff.y = abs(pDiff.y) - (pDiff.y > 0) ? type.dimensionDown() : type.dimensionUp();

			//estimate the space we have
			int tolerableGap = -2 + (scale >= 3) ? 16
				: (scale == 2) ? 12
				: 8;

			if (pDiff.x >= tolerableGap || pDiff.y >= tolerableGap) {
				continue;
			}
		}

		return false; 
	}

	return true;
}

/* Before trying to move, checks the next square in front and then tries different angles otherwise. */
bool Micro::SuperSmartMove(BWAPI::Unit unit, BWAPI::Position pos)
{
	if (!unit) { //something is wrong!
		UAB_ASSERT(false, "bad arg to Micro::SuperSmartMove");
		return false;
	}
	/*if (!pos.isValid()) { //if it's invalid, we may be on the map boundary; this is ok since checkmovable will only find valid positions
		UAB_ASSERT(false, "bad position to Micro::SuperSmartMove from unit marked by red circle");
		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 64, BWAPI::Colors::Red, true);
		return false;
	}*/
	BWAPI::Position nextVector = pos - unit->getPosition();
	double factor = 16*sqrt((nextVector.x * nextVector.x) + (nextVector.y * nextVector.y));
	nextVector = BWAPI::Position(int(nextVector.x * factor), int(nextVector.y * factor));

	//approximate the unit size
	auto size = unit->getType().size();
	int scale = (size == BWAPI::UnitSizeTypes::Large) ? 4 
		: (size == BWAPI::UnitSizeTypes::Medium) ? 2 
		: (size == BWAPI::UnitSizeTypes::Small) ? 1 
		: 2;

	if (Micro::checkMovable(unit->getPosition() + nextVector, scale)) { //no issue moving normally
		Micro::SmartMove(unit, pos);
		return true;
	}
	else { // try angles 22.5, -22.5, 45, -45... etc. in that order
		for (int i = 0; i < 8; i++) {
			double angle = 22.5 * (1+int(i/2)) * pow(-1, i);
			double x = nextVector.x;
			double y = nextVector.y;
			Micro::Rotate(x, y, angle);

			BWAPI::Position nextCandidate = unit->getPosition() + BWAPI::Position(int(x),int(y));
			if (Micro::checkMovable(nextCandidate)) { 
				Micro::SmartMove(unit, nextCandidate);
				return true; 
			}
		}
	}

	//failed to find any good movement areas, just default to normal behavior
	Micro::SmartMove(unit, pos);

	return false;
}

/* Handle special critical cases first */
bool Micro::CheckSpecialCases(BWAPI::Unit unit) {
	if (Micro::Unstick(unit)) return true;
	if (Micro::NukeDodge(unit)) return true;
	if (Micro::StormDodge(unit)) return true;
	if (unit->isIrradiated()) {
		if (Micro::AvoidAllies(unit, 0)) return true;
	}
	if (Micro::ScarabDodge(unit)) return true;

	return false;
}

bool Micro::NukeDodge(BWAPI::Unit unit) {
	for (auto p : BWAPI::Broodwar->getNukeDots()) {
		if (unit->getDistance(p) > 300) continue; //the AOE is about 256,

		auto ghosts = BWAPI::Broodwar->getUnitsInRadius(p, 10 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Ghost && BWAPI::Filter::IsEnemy &&
			BWAPI::Filter::IsDetected && !BWAPI::Filter::IsMoving && !BWAPI::Filter::IsAttacking);
		
		if (!ghosts.empty()) {
			auto g = *(ghosts.begin());
			if (unit->canAttackUnit(g)) {
				//Micro::SmartAttackMove(unit, g->getPosition());
				return false; //leave it to the unit to decide how to fight the ghost
				//return true;
			}
		}

		if (unit->canUnburrow()) {
			unit->unburrow();
			return true;
		}
		else if (unit->canUnsiege()) {
			unit->unsiege();
			return true;
		}

		BWAPI::Position movementVector = unit->getPosition() - p;
		double scale = 64.0 / movementVector.getLength();
		movementVector = BWAPI::Position(int(scale*movementVector.x), int(scale*movementVector.y));
		if (unit->isFlying()) {
			Micro::SmartMove(unit, unit->getPosition() + movementVector);
		}
		else {
			Micro::SuperSmartMove(unit, unit->getPosition() + movementVector);
		}
		return true;
	}

	for (auto nuke : InformationManager::Instance().nukes) {
		if (!nuke || !nuke->exists()) {
			UAB_ASSERT(false, "Invalid nuke recorded");
			continue;
		}
		auto p = nuke->getPosition();
		if (unit->getDistance(p) > 256) continue; //the AOE is about 256, but we might as well play it safe

		if (unit->canUnburrow()) {
			unit->unburrow();
			return true;
		}
		else if (unit->canUnsiege()) {
			unit->unsiege();
			return true;
		}

		BWAPI::Position movementVector = unit->getPosition() - p;
		double scale = 64.0 / movementVector.getLength();
		movementVector = BWAPI::Position(int(scale*movementVector.x), int(scale*movementVector.y));
		if (unit->isFlying()) {
			Micro::SmartMove(unit, unit->getPosition() + movementVector);
		}
		else {
			Micro::SuperSmartMove(unit, unit->getPosition() + movementVector);
		}
		return true;
	}

	return false;
}

bool Micro::Unstick(BWAPI::Unit unit) {
	auto type = unit->getType();
	if (type != BWAPI::UnitTypes::Zerg_Zergling &&
		type != BWAPI::UnitTypes::Zerg_Hydralisk &&
		type != BWAPI::UnitTypes::Zerg_Ultralisk &&
		type != BWAPI::UnitTypes::Protoss_Dragoon) return false;
	
	if (stuckFrames.find(unit) == stuckFrames.end()) { //initialize value
		stuckFrames[unit] = BWAPI::Broodwar->getFrameCount();
		return false;
	}

	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	auto ctype = currentCommand.getType();
	auto cdist = unit->getDistance(currentCommand.getTargetPosition());
	bool stuck = ((ctype == BWAPI::UnitCommandTypes::Move && cdist > 2*32) ||
		(ctype == BWAPI::UnitCommandTypes::Attack_Unit && unit->getGroundWeaponCooldown() == 0)) && //consecutive cd frames
		abs(unit->getVelocityX()) + abs(unit->getVelocityY()) == 0; // <= 0.01;
	if (!stuck) {
		stuckFrames[unit] = BWAPI::Broodwar->getFrameCount();
		return false;
	}

	bool veryStuck = BWAPI::Broodwar->getFrameCount() - stuckFrames[unit] > minStuckTime;

	if (veryStuck)
	{
		unit->stop();
		stuckFrames[unit] = BWAPI::Broodwar->getFrameCount();
		return true;
	}

	return false;
}

/* Find the storm we're in and avoid it! */
bool Micro::StormDodge(BWAPI::Unit unit) {
	if (!unit) {
		UAB_ASSERT(false, "bad arg to Micro::StormDodge");
		return false;
	}

	if (!unit->isUnderStorm() && !unit->isMoving()) { //not relevant if 
		return false;
	}

	/* Need to unburrow to dodge a storm! */
	if (unit->isUnderStorm() && unit->isBurrowed() && unit->canUnburrow()) {
		unit->unburrow();
		return true;
	}

	const double stormAvoidRadius = 3; //note that the AOE is 3x3
	BWAPI::Bullet storm;
	bool stormFound = false;
	for (auto & bullet : InformationManager::Instance().storms) {
		auto center = bullet->getPosition();

		if (bullet->getType() == BWAPI::BulletTypes::Psionic_Storm) {
			BWAPI::Broodwar->drawCircleMap(bullet->getPosition(), 32, BWAPI::Colors::Blue, true);
		}
		if (bullet->getType() == BWAPI::BulletTypes::Psionic_Storm &&
			unit->getDistance(center) <= stormAvoidRadius * 32) {
			storm = bullet;
			stormFound = true;
			break;
		}
	}

	if (!stormFound) {
		if (unit->isUnderStorm()) (false, "Stormed Unit could not find storm it's in!");
		return false;
	}
	else {
		auto center = storm->getPosition();
		//units take a long time to turn (3-6 frames) and decelerate (3-6 frames), 
		//so we need to factor that into the effective position when dodging
		auto pos = unit->getPosition() + BWAPI::Position(int(unit->getVelocityX()), int(unit->getVelocityY()));
		bool isFlying = unit->isFlying();
		bool xFaster = abs(center.x - pos.x) > abs(center.y - pos.y);
		
		BWAPI::Position movePosition;
		if (center.x - pos.x == 0 && center.y - pos.y == 0) { //dead center, move ANYWHERE
			int sign = rand() % 2 ? 1 : -1;
			movePosition = BWAPI::Position(pos.x + sign * 32, pos.y + sign * 32);
		}
		else if (xFaster) { //move in x dir
			int offset = (pos.x - center.x) > 0 ? 64 : -64;
			movePosition = BWAPI::Position(pos.x + offset, pos.y);
		}
		else { //move in y dir
			int offset = (pos.y - center.y) > 0 ? 64 : -64;
			movePosition = BWAPI::Position(pos.x, pos.y + offset);
		}
		if (!movePosition.isValid()) {
			UAB_ASSERT(false, "Stormed Unit could not find valid escape position!");
			return false;
		}

		if (isFlying) {
			SmartMove(unit, movePosition);
		}
		else {
			SuperSmartMove(unit, movePosition);
		}
	}

	return true; //completed storm dodging successfully
}

bool Micro::ScarabDodge(BWAPI::Unit unit) {
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self()) {
		UAB_ASSERT(false, "Invalid input to ScarabDodge in Micro.cpp");
	}
	//hydralisks/zerglings/drones only for now
	if (unit->getType() != BWAPI::UnitTypes::Zerg_Hydralisk && unit->getType() != BWAPI::UnitTypes::Zerg_Zergling &&
		unit->getType() != BWAPI::UnitTypes::Zerg_Drone) { 
		return false;
	}

	BWAPI::Unitset targets = unit->getUnitsInRadius(7 * 32, (BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Scarab) && (BWAPI::Filter::GetPlayer != BWAPI::Broodwar->self()));

	if (targets.empty()) return false;
	for (auto target : targets) {
		if (!target || !(target->exists())) continue;

		auto up = unit->getPosition();
		auto tp = target->getPosition();
		BWAPI::Position ttp(0,0);
		/* all these crash! D:! I suppose it would be cheating anyway, though.
		
		try {
			auto targ = target->getOrderTarget();
			if (targ && targ->exists()) {
				ttp = targ->getPosition();
				BWAPI::Broodwar->sendText("1 worked");
			}
 			//ttp = target->getOrderTarget()->getPosition();
		}
		catch (int e) {
			e = 0;
			BWAPI::Broodwar->printf("1 failed");
		}

		try {
			auto targ = target->getTarget();
			if (targ && targ->exists()) {
				ttp = targ->getPosition();
				BWAPI::Broodwar->sendText("2 worked");
			}
			//ttp = target->getTarget()->getPosition();
			BWAPI::Broodwar->sendText("2 worked");
		}
		catch (int e) {
			e = 0;
			BWAPI::Broodwar->printf("2 failed");
		}

		try {
			ttp = target->getTargetPosition();
			BWAPI::Broodwar->sendText("3 worked");
		}
		catch (int e) {
			e = 0;
			BWAPI::Broodwar->printf("3 failed");
		}

		try {
			ttp = target->getOrderTargetPosition();
			BWAPI::Broodwar->sendText("4 worked");
		}
		catch (int e) {
			e = 0;
			BWAPI::Broodwar->printf("4 failed");
		}*/

		double tTravel = (up.getDistance(tp) / BWAPI::UnitTypes::Protoss_Scarab.topSpeed());
		BWAPI::Position projectedPosition(int(tTravel *  target->getVelocityX()), int(tTravel * target->getVelocityY()));
		ttp = tp + projectedPosition;

		BWAPI::Broodwar->drawCircleMap(tp, 16, BWAPI::Colors::Red, false);
		BWAPI::Broodwar->drawLineMap(tp, ttp, BWAPI::Colors::Red);
		BWAPI::Broodwar->drawCircleMap(ttp, 32, BWAPI::Colors::Red, false);
		
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Drone) {
			if (up.getDistance(ttp) > 5*32 && up.getDistance(tp) > 5*32) return false; //drones better run their butts off
		}
		else {
			if (up.getDistance(ttp) > 70 && up.getDistance(tp) > 70) return false; //aoe is 20/40/60. we don't want to move too many hydras over a scarab
		}

		auto kiteVector = up - tp; //move away from scarab
		kiteVector = BWAPI::Position(int(64 * kiteVector.x / kiteVector.getLength()), int(64 * kiteVector.y / kiteVector.getLength()));

		Micro::SuperSmartMove(unit, up + kiteVector);

		BWAPI::Broodwar->drawLineMap(up, up + kiteVector, BWAPI::Colors::Blue);

		return true; //attempted scarab dodging
	}

	return false; //no scarabs found, no need to dodge
}

bool Micro::AvoidWorkerHazards(BWAPI::Unit unit) {
	if (unit->isAttacking()) return false;

	//note: might not include siege tanks. but we can't see them at 13 range anyway...
	//
	auto hazards = unit->getUnitsInRadius(10*32, BWAPI::Filter::IsEnemy && 
		(BWAPI::Filter::HP >= 40 || BWAPI::Filter::IsFlying) && //exclude marines and zerglings
		(
		(BWAPI::Filter::GroundWeapon != BWAPI::WeaponTypes::None) || 
		BWAPI::Filter::IsTransport ||
		BWAPI::Filter::IsIrradiated ||
		(BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_High_Templar && BWAPI::Filter::Energy >= 70) ||
		BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Reaver) &&
		!BWAPI::Filter::IsWorker &&
		BWAPI::Filter::GetType != BWAPI::UnitTypes::Protoss_Zealot
		);

	auto detectors = unit->getUnitsInRadius(12 * 32, BWAPI::Filter::IsDetector && BWAPI::Filter::IsEnemy);

	BWAPI::Position kiteVector(0, 0);
	for (auto hazard : hazards) {
		auto type = hazard->getType();
		if (UnitUtil::CanAttackGround(hazard)) {
			int range = UnitUtil::AttackRange(hazard, unit);
			if (range <= 32 && !type.isFlyer()) continue; //the drone should probably fight it

			//extra caution around these
			if (type == BWAPI::UnitTypes::Protoss_Interceptor) range = 6*32;
			if (type == BWAPI::UnitTypes::Protoss_Arbiter) range = 8*32;
			if (hazard->isIrradiated() && range < 4*32) range = 4*32;

			if (unit->getDistance(hazard) > range + 64) continue;
		}
		else if (unit->getDistance(hazard) > 6 * 32) {
			continue;
		}

		//run away!
		if (detectors.empty() && unit->canBurrow()) {
			//UAB_ASSERT(false, "Stub function for burrowing drones called in Micro.cpp");
			unit->burrow();
			return true;
		}
		else if (unit->isBurrowed() && !unit->isDetected()) { //something dangerous is nearby, stay burrowed
			return true;
		}

		kiteVector += Micro::GetKiteVector(hazard, unit);
		//Micro::SmartMove(unit, unit->getPosition() + Micro::GetKiteVector(hazard, unit));
	}

	if (unit->isBurrowed() && unit->canUnburrow()) {
		//either it's safe or we need to move
		//UAB_ASSERT(false, "Stub function for burrowing drones called in Micro.cpp");
		unit->unburrow();
		return true;
	}
	else if (kiteVector != BWAPI::Position(0, 0)) {
		Micro::SmartMove(unit, unit->getPosition() + kiteVector);
		return true;
	}
	
	return false;
}

//Computes the potential field influence of the wall based on the unit's position and desired move vector.
/*BWAPI::Position GetWallVector(BWAPI::Unit unit, BWAPI::Position moveVector) {
	BWAPI::Position vector(0, 0);
	if (!unit || !unit->exists()) {
		UAB_ASSERT(false, "Invalid arg to GetWallVector in Micro.cpp");
		return vector;
	}

	int w = BWAPI::Broodwar->mapWidth();
	int h = BWAPI::Broodwar->mapHeight();

	if (std::min(w, h) <= 20 * 32) {
		UAB_ASSERT(false, "Map too small for GetWallVector in Micro.cpp");
		return vector; //map must be insanely small! forget about it!
	}

	int threshold = 5 * 32;

	auto p = unit->getPosition();
	//cancel out or reduce the x movement if it is towards the wall
	if (p.x < threshold) {

	}
	else if (w - p.x < threshold) {

	}

	//cancel out or reduce the y movement if it is towards the wall
	if (p.y < threshold) {
		
	}
	else if (h - p.y < threshold) {

	}

	//if the movevector + wallvector = 0 combined, then add some arbitrary movement perpendicular to wall

	if (unit->isFlying()) {
		//also compute terrain
	}

	return vector; 
}*/